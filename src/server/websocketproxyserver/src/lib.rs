//! DarkEden WebSocket gateway.
//!
//! Browser and native clients open a WebSocket to `/game?host=H&port=P`,
//! where `H:P` is a login or game server address the game protocol
//! advertised. The gateway maps that address to a configured local
//! `GatewayProxyPort`, connects to `127.0.0.1:<GatewayProxyPort>`, sends a
//! PROXY protocol v1 line naming the client's IPv4 address, and then relays
//! the game's byte stream verbatim: binary WebSocket messages to TCP, TCP
//! bytes back as binary WebSocket messages.
//!
//! The backend is connected (and sent the PROXY line) before the `101`
//! response is written, so a dead backend fails the handshake with `502`.

mod config;
mod handshake;
mod relay;
mod repeated_error;

use std::future::Future;
use std::io;
use std::net::{Ipv4Addr, SocketAddr};
use std::sync::Arc;
use std::time::{Duration, Instant};

use tokio::io::AsyncWriteExt;
use tokio::net::{TcpListener, TcpStream};
use tokio::sync::{watch, OwnedSemaphorePermit, Semaphore};
use tokio::task::{JoinError, JoinSet};
use tokio::time::timeout;
use tokio_tungstenite::tungstenite::protocol::{Role, WebSocketConfig};
use tokio_tungstenite::WebSocketStream;

pub use config::{
    Config, ConfigError, DEFAULT_BIND, DEFAULT_MAX_CONNECTIONS, DEFAULT_PORT, MAX_CONNECTIONS_LIMIT,
};
pub use handshake::parse_query;

use handshake::{Decision, HeadError, Rejection, Upgrade};
use repeated_error::{BackendReports, RepeatedError};

/// Largest WebSocket message and frame a client may send; larger ones close
/// the connection with 1009.
pub const MAX_MESSAGE_BYTES: usize = 1024 * 1024;

/// The gateway's time limits.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct Timeouts {
    /// From accepting a connection to writing the `101` response, including
    /// the backend connection.
    pub handshake: Duration,
    /// For connecting to the backend.
    pub backend_connect: Duration,
    /// Between WebSocket pings; a client that has not answered the previous
    /// ping when the next one is due is disconnected.
    pub ping_interval: Duration,
    /// For finishing a WebSocket close handshake, and for connections to
    /// wind down after shutdown is requested.
    pub close_grace: Duration,
}

impl Default for Timeouts {
    fn default() -> Self {
        Self {
            handshake: Duration::from_secs(10),
            backend_connect: Duration::from_secs(5),
            ping_interval: Duration::from_secs(30),
            close_grace: Duration::from_secs(5),
        }
    }
}

/// A bound, not yet serving, gateway.
#[derive(Debug)]
pub struct Gateway {
    listener: TcpListener,
    shared: Shared,
}

#[derive(Debug)]
struct Shared {
    config: Config,
    timeouts: Timeouts,
    backend_reports: BackendReports,
}

impl Gateway {
    /// Binds the configured `bind:port`.
    pub async fn bind(config: Config) -> io::Result<Self> {
        let listener = TcpListener::bind((config.bind(), config.port()))
            .await
            .map_err(|error| {
                io::Error::new(
                    error.kind(),
                    format!(
                        "cannot listen on {}:{}: {error}",
                        config.bind(),
                        config.port()
                    ),
                )
            })?;
        Ok(Self {
            listener,
            shared: Shared {
                backend_reports: BackendReports::new(config.backend_ports()),
                config,
                timeouts: Timeouts::default(),
            },
        })
    }

    /// Replaces the default time limits.
    #[must_use]
    pub fn with_timeouts(mut self, timeouts: Timeouts) -> Self {
        self.shared.timeouts = timeouts;
        self
    }

    /// The address actually bound, useful with port 0.
    pub fn local_addr(&self) -> io::Result<SocketAddr> {
        self.listener.local_addr()
    }

    /// Serves connections until `shutdown` completes, then stops accepting,
    /// closes every connection (WebSocket clients get close code 1001) and
    /// returns once they are gone or the close grace period has passed.
    pub async fn serve<F>(self, shutdown: F)
    where
        F: Future<Output = ()>,
    {
        let Self { listener, shared } = self;
        let shared = Arc::new(shared);
        // Every accepted socket holds a permit, handshakes included.
        let permits = Arc::new(Semaphore::new(shared.config.max_connections()));
        let (stop_tx, stop_rx) = watch::channel(false);
        let mut connections = JoinSet::new();
        let mut shutdown = std::pin::pin!(shutdown);
        let mut accept_failures = RepeatedError::default();

        loop {
            tokio::select! {
                biased;
                () = &mut shutdown => break,
                Some(finished) = connections.join_next() => report(finished),
                accepted = listener.accept() => {
                    let (stream, peer) = match accepted {
                        Ok(accepted) => {
                            if let Some(suppressed_failures) = accept_failures.recovery() {
                                tracing::info!(suppressed_failures, "accept recovered");
                            }
                            accepted
                        },
                        Err(error) => {
                            // Usually descriptor exhaustion; do not spin on it.
                            if let Some(suppressed_failures) = accept_failures.failure(Instant::now()) {
                                tracing::warn!(suppressed_failures, %error, "accept failed");
                            }
                            tokio::time::sleep(Duration::from_millis(100)).await;
                            continue;
                        }
                    };
                    let Ok(permit) = Arc::clone(&permits).try_acquire_owned() else {
                        tracing::debug!(%peer, "connection limit reached; dropping connection");
                        continue;
                    };
                    connections.spawn(connection(
                        stream,
                        peer,
                        Arc::clone(&shared),
                        stop_rx.clone(),
                        permit,
                    ));
                }
            }
        }

        drop(listener);
        stop_tx.send_replace(true);
        let drained = timeout(
            shared.timeouts.close_grace + Duration::from_secs(1),
            async {
                while let Some(finished) = connections.join_next().await {
                    report(finished);
                }
            },
        )
        .await;
        if drained.is_err() {
            connections.shutdown().await;
        }
        shared.backend_reports.finish();
        let suppressed_failures = accept_failures.take_suppressed();
        if suppressed_failures != 0 {
            tracing::warn!(
                suppressed_failures,
                "accept failures suppressed before shutdown"
            );
        }
    }
}

fn report(finished: Result<(), JoinError>) {
    if let Err(error) = finished {
        if error.is_panic() {
            tracing::error!("connection task panicked: {error}");
        }
    }
}

/// Completes when shutdown is requested (or the gateway is gone).
pub(crate) async fn stopped(stop: &mut watch::Receiver<bool>) {
    // An error means the sender is gone, which also means stop.
    let _ = stop.wait_for(|stop| *stop).await;
}

/// Completes on ctrl-c, or on SIGTERM where signals exist.
pub async fn shutdown_signal() {
    let interrupt = async {
        if let Err(error) = tokio::signal::ctrl_c().await {
            tracing::error!("cannot listen for ctrl-c: {error}");
            std::future::pending::<()>().await;
        }
    };
    #[cfg(unix)]
    let terminate = async {
        use tokio::signal::unix::{signal, SignalKind};
        match signal(SignalKind::terminate()) {
            Ok(mut terminate) => {
                terminate.recv().await;
            }
            Err(error) => {
                tracing::error!("cannot listen for SIGTERM: {error}");
                std::future::pending::<()>().await;
            }
        }
    };
    #[cfg(not(unix))]
    let terminate = std::future::pending::<()>();
    tokio::select! {
        () = interrupt => {}
        () = terminate => {}
    }
}

/// The WebSocket limits: 1 MiB messages and frames, no extensions (tungstenite
/// negotiates none, so permessage-deflate is never enabled). The read buffer
/// is kept small because it is allocated per connection and mostly idle; a
/// large frame grows it on demand.
fn websocket_config() -> WebSocketConfig {
    WebSocketConfig::default()
        .read_buffer_size(16 * 1024)
        .max_message_size(Some(MAX_MESSAGE_BYTES))
        .max_frame_size(Some(MAX_MESSAGE_BYTES))
}

/// One client connection, from accept to close.
async fn connection(
    mut stream: TcpStream,
    peer: SocketAddr,
    shared: Arc<Shared>,
    mut stop: watch::Receiver<bool>,
    _permit: OwnedSemaphorePermit,
) {
    if let Err(error) = stream.set_nodelay(true) {
        tracing::debug!(%peer, "cannot set TCP_NODELAY: {error}");
    }
    let admitted = tokio::select! {
        admitted = timeout(shared.timeouts.handshake, negotiate(&mut stream, peer, &shared)) => {
            match admitted {
                Ok(admitted) => admitted,
                Err(_) => {
                    tracing::debug!(%peer, "handshake timed out");
                    None
                }
            }
        }
        () = stopped(&mut stop) => None,
    };
    let Some((backend, leftover)) = admitted else {
        return;
    };
    tracing::debug!(%peer, "WebSocket open");
    let socket = WebSocketStream::from_partially_read(
        stream,
        leftover,
        Role::Server,
        Some(websocket_config()),
    )
    .await;
    relay::run(socket, backend, &shared.timeouts, &mut stop).await;
    tracing::debug!(%peer, "WebSocket closed");
}

/// Reads the request and answers it. For an admitted upgrade, returns the
/// connected backend and any bytes the client sent after the request head.
async fn negotiate(
    stream: &mut TcpStream,
    peer: SocketAddr,
    shared: &Shared,
) -> Option<(TcpStream, Vec<u8>)> {
    let (head, leftover) = match handshake::read_head(stream).await {
        Ok(parsed) => parsed,
        Err(HeadError::TooLarge) => {
            refuse(stream, peer, Rejection::new(431, "request head too large")).await;
            return None;
        }
        Err(HeadError::Malformed) => {
            refuse(stream, peer, Rejection::new(400, "malformed HTTP request")).await;
            return None;
        }
        Err(HeadError::Io(error)) => {
            tracing::debug!(%peer, "cannot read the request: {error}");
            return None;
        }
        Err(HeadError::Closed) => return None,
    };
    let upgrade = match handshake::decide(&head, peer, &shared.config) {
        Decision::Banner { head_only } => {
            respond(stream, &handshake::banner_response(head_only)).await;
            return None;
        }
        Decision::Reject(rejection) => {
            refuse(stream, peer, rejection).await;
            return None;
        }
        Decision::Upgrade(upgrade) => upgrade,
    };
    let backend = match connect_backend(&upgrade, peer, shared.timeouts.backend_connect).await {
        Ok(backend) => {
            if let Some(suppressed_failures) = shared.backend_reports.recovery(upgrade.backend_port)
            {
                tracing::info!(
                    backend_port = upgrade.backend_port,
                    suppressed_failures,
                    "backend recovered"
                );
            }
            backend
        }
        Err(error) => {
            if let Some(suppressed_failures) = shared
                .backend_reports
                .failure(upgrade.backend_port, Instant::now())
            {
                tracing::warn!(backend_port = upgrade.backend_port, suppressed_failures, %error, "backend unavailable");
            }
            refuse(stream, peer, Rejection::new(502, "backend unavailable")).await;
            return None;
        }
    };
    let accepted = handshake::switching_protocols(&upgrade.accept_key);
    if let Err(error) = stream.write_all(accepted.as_bytes()).await {
        tracing::debug!(%peer, "cannot complete handshake: {error}");
        return None;
    }
    Some((backend, leftover))
}

/// Connects to the route's GatewayProxyPort and sends the PROXY line.
async fn connect_backend(
    upgrade: &Upgrade,
    peer: SocketAddr,
    limit: Duration,
) -> io::Result<TcpStream> {
    let connect = TcpStream::connect((Ipv4Addr::LOCALHOST, upgrade.backend_port));
    let mut backend = timeout(limit, connect)
        .await
        .map_err(|_| io::Error::new(io::ErrorKind::TimedOut, "connect timed out"))??;
    backend.set_nodelay(true)?;
    let header = proxy_header(upgrade.client_ip, peer.port(), upgrade.backend_port);
    backend.write_all(header.as_bytes()).await?;
    Ok(backend)
}

/// The PROXY protocol v1 line `ProxyAcceptor` expects: the client's address
/// and the TCP source port the gateway saw, to loopback on the backend port.
fn proxy_header(client: Ipv4Addr, source_port: u16, backend_port: u16) -> String {
    format!("PROXY TCP4 {client} 127.0.0.1 {source_port} {backend_port}\r\n")
}

async fn refuse(stream: &mut TcpStream, peer: SocketAddr, rejection: Rejection) {
    tracing::debug!(
        %peer,
        status = rejection.status,
        "request refused: {}",
        rejection.reason
    );
    respond(stream, &rejection.response()).await;
}

async fn respond(stream: &mut TcpStream, response: &str) {
    if stream.write_all(response.as_bytes()).await.is_ok() {
        let _ = stream.shutdown().await;
    }
}

#[cfg(test)]
mod tests {
    use super::proxy_header;
    use std::net::Ipv4Addr;

    #[test]
    fn proxy_header_matches_the_cpp_listener_format() {
        assert_eq!(
            proxy_header(Ipv4Addr::new(203, 0, 113, 44), 51234, 19099),
            "PROXY TCP4 203.0.113.44 127.0.0.1 51234 19099\r\n"
        );
    }
}
