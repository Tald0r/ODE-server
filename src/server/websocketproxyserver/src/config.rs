//! The gateway's JSON configuration.
//!
//! The schema is the one the Node.js gateway read, so existing `config.json`
//! files keep working:
//!
//! ```json
//! {
//!   "bind": "127.0.0.1",
//!   "port": 8080,
//!   "origins": ["https://game.example"],
//!   "allowNative": false,
//!   "trustedProxies": [],
//!   "maxConnections": 2000,
//!   "routes": { "login.example:9999": 19099 }
//! }
//! ```
//!
//! `origins` and `routes` are required; every other key has the default shown
//! above. A JSON `null` is treated like an absent key.

use std::collections::{HashMap, HashSet};
use std::fmt;
use std::net::IpAddr;

use serde_json::{Map, Value};

/// Address the gateway listens on when `bind` is absent.
pub const DEFAULT_BIND: &str = "127.0.0.1";
/// Port the gateway listens on when `port` is absent.
pub const DEFAULT_PORT: u16 = 8080;
/// Connection cap when `maxConnections` is absent.
pub const DEFAULT_MAX_CONNECTIONS: usize = 2000;
/// Largest accepted `maxConnections`.
pub const MAX_CONNECTIONS_LIMIT: usize = 100_000;

const KNOWN_KEYS: [&str; 7] = [
    "bind",
    "port",
    "origins",
    "allowNative",
    "trustedProxies",
    "maxConnections",
    "routes",
];

/// A validated gateway configuration.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Config {
    bind: String,
    port: u16,
    origins: HashSet<String>,
    allow_native: bool,
    trusted_proxies: HashSet<IpAddr>,
    max_connections: usize,
    routes: HashMap<String, u16>,
}

/// Why a configuration was rejected. The message names the offending key.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct ConfigError {
    key: String,
    message: String,
}

impl ConfigError {
    fn new(key: impl Into<String>, message: impl Into<String>) -> Self {
        Self {
            key: key.into(),
            message: message.into(),
        }
    }

    /// The configuration key at fault, or an empty string for the document
    /// as a whole.
    pub fn key(&self) -> &str {
        &self.key
    }
}

impl fmt::Display for ConfigError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        if self.key.is_empty() {
            write!(f, "invalid gateway configuration: {}", self.message)
        } else {
            write!(
                f,
                "invalid gateway configuration: `{}` {}",
                self.key, self.message
            )
        }
    }
}

impl std::error::Error for ConfigError {}

impl Config {
    /// Parses and validates a JSON configuration document.
    pub fn from_json(text: &str) -> Result<Self, ConfigError> {
        let value: Value = serde_json::from_str(text)
            .map_err(|error| ConfigError::new("", format!("is not valid JSON: {error}")))?;
        Self::from_value(&value)
    }

    /// Validates an already parsed JSON configuration document.
    pub fn from_value(value: &Value) -> Result<Self, ConfigError> {
        let object = value
            .as_object()
            .ok_or_else(|| ConfigError::new("", "must be a JSON object"))?;
        for key in object.keys() {
            if !KNOWN_KEYS.contains(&key.as_str()) {
                tracing::warn!("ignoring unknown configuration key `{key}`");
            }
        }

        let bind = match field(object, "bind") {
            None => DEFAULT_BIND.to_owned(),
            Some(Value::String(bind)) if !bind.is_empty() => bind.clone(),
            Some(_) => {
                return Err(ConfigError::new(
                    "bind",
                    "must be a non-empty string naming the listen address",
                ))
            }
        };

        let port = match field(object, "port") {
            None => DEFAULT_PORT,
            Some(value) => integer(value)
                .and_then(|port| u16::try_from(port).ok())
                .ok_or_else(|| ConfigError::new("port", "must be an integer from 0 to 65535"))?,
        };

        let origins = parse_origins(field(object, "origins"))?;

        let allow_native = match field(object, "allowNative") {
            None => false,
            Some(Value::Bool(allow)) => *allow,
            Some(_) => return Err(ConfigError::new("allowNative", "must be true or false")),
        };

        let trusted_proxies = parse_trusted_proxies(field(object, "trustedProxies"))?;

        let max_connections = match field(object, "maxConnections") {
            None => DEFAULT_MAX_CONNECTIONS,
            Some(value) => integer(value)
                .and_then(|limit| usize::try_from(limit).ok())
                .filter(|limit| (1..=MAX_CONNECTIONS_LIMIT).contains(limit))
                .ok_or_else(|| {
                    ConfigError::new(
                        "maxConnections",
                        format!("must be an integer from 1 to {MAX_CONNECTIONS_LIMIT}"),
                    )
                })?,
        };

        let routes = parse_routes(field(object, "routes"))?;

        Ok(Self {
            bind,
            port,
            origins,
            allow_native,
            trusted_proxies,
            max_connections,
            routes,
        })
    }

    /// The address to listen on: an IP address or a host name.
    pub fn bind(&self) -> &str {
        &self.bind
    }

    /// The port to listen on; 0 picks a free port.
    pub fn port(&self) -> u16 {
        self.port
    }

    /// Whether a browser page served from `origin` may connect.
    pub fn origin_allowed(&self, origin: &str) -> bool {
        self.origins.contains(origin)
    }

    /// Whether clients that send no (or an empty) Origin header may connect.
    pub fn allow_native(&self) -> bool {
        self.allow_native
    }

    /// Whether `peer` is a reverse proxy whose `X-Forwarded-For` is trusted.
    pub fn is_trusted_proxy(&self, peer: IpAddr) -> bool {
        self.trusted_proxies.contains(&peer.to_canonical())
    }

    /// The most TCP connections open at once, handshakes included.
    pub fn max_connections(&self) -> usize {
        self.max_connections
    }

    /// Backend ports from configuration; no request-supplied keys are retained.
    pub(crate) fn backend_ports(&self) -> impl Iterator<Item = u16> + '_ {
        self.routes.values().copied()
    }

    /// The local GatewayProxyPort for an advertised `host:port` route.
    pub fn route(&self, advertised: &str) -> Option<u16> {
        self.routes.get(advertised).copied()
    }
}

/// A key's value, treating JSON `null` as absent.
fn field<'a>(object: &'a Map<String, Value>, key: &str) -> Option<&'a Value> {
    object.get(key).filter(|value| !value.is_null())
}

/// An integral JSON number; `2000` and `2000.0` are the same number in JSON.
fn integer(value: &Value) -> Option<i64> {
    if let Some(integer) = value.as_i64() {
        return Some(integer);
    }
    let float = value.as_f64()?;
    // The range check keeps the cast exact; i64::MAX is not representable.
    if float.fract() == 0.0 && float.abs() < 9.0e15 {
        Some(float as i64)
    } else {
        None
    }
}

fn parse_origins(value: Option<&Value>) -> Result<HashSet<String>, ConfigError> {
    let entries = value
        .and_then(Value::as_array)
        .filter(|entries| !entries.is_empty())
        .ok_or_else(|| {
            ConfigError::new(
                "origins",
                "must be a non-empty array of allowed browser origins, e.g. [\"https://game.example\"]",
            )
        })?;
    entries
        .iter()
        .enumerate()
        .map(|(index, entry)| match entry {
            Value::String(origin) if !origin.is_empty() => Ok(origin.clone()),
            _ => Err(ConfigError::new(
                format!("origins[{index}]"),
                "must be a non-empty origin string; native clients are admitted by `allowNative`",
            )),
        })
        .collect()
}

fn parse_trusted_proxies(value: Option<&Value>) -> Result<HashSet<IpAddr>, ConfigError> {
    let Some(value) = value else {
        return Ok(HashSet::new());
    };
    let entries = value.as_array().ok_or_else(|| {
        ConfigError::new(
            "trustedProxies",
            "must be an array of reverse-proxy IP addresses",
        )
    })?;
    entries
        .iter()
        .enumerate()
        .map(|(index, entry)| {
            entry
                .as_str()
                .and_then(|text| text.parse::<IpAddr>().ok())
                .map(|address| address.to_canonical())
                .ok_or_else(|| {
                    ConfigError::new(
                        format!("trustedProxies[{index}]"),
                        "must be an IP address string such as \"127.0.0.1\"",
                    )
                })
        })
        .collect()
}

fn parse_routes(value: Option<&Value>) -> Result<HashMap<String, u16>, ConfigError> {
    let entries = value
        .and_then(Value::as_object)
        .filter(|entries| !entries.is_empty())
        .ok_or_else(|| {
            ConfigError::new(
                "routes",
                "must be a non-empty object mapping each advertised host:port to a local GatewayProxyPort",
            )
        })?;
    entries
        .iter()
        .map(|(route, port)| {
            if !valid_route(route) {
                return Err(ConfigError::new(
                    format!("routes[{route:?}]"),
                    "must be keyed by an advertised host:port: letters, digits, '.' or '-', then ':' and a port from 1 to 65535",
                ));
            }
            let port = integer(port)
                .and_then(|port| u16::try_from(port).ok())
                .filter(|port| *port != 0)
                .ok_or_else(|| {
                    ConfigError::new(
                        format!("routes[{route:?}]"),
                        "must be a local GatewayProxyPort from 1 to 65535",
                    )
                })?;
            Ok((route.clone(), port))
        })
        .collect()
}

/// Matches `^[a-zA-Z0-9.-]+:[1-9][0-9]{0,4}$` with a port of at most 65535.
fn valid_route(route: &str) -> bool {
    let Some((host, port)) = route.split_once(':') else {
        return false;
    };
    !host.is_empty()
        && host
            .bytes()
            .all(|byte| byte.is_ascii_alphanumeric() || byte == b'.' || byte == b'-')
        && (1..=5).contains(&port.len())
        && port.bytes().all(|byte| byte.is_ascii_digit())
        && !port.starts_with('0')
        && port.parse::<u32>().is_ok_and(|port| port <= 65535)
}

#[cfg(test)]
mod tests {
    use super::valid_route;

    #[test]
    fn route_keys_follow_the_advertised_host_port_pattern() {
        for good in ["192.0.2.2:9999", "login.example:1", "a-b.c:65535"] {
            assert!(valid_route(good), "{good}");
        }
        for bad in [
            "",
            ":9999",
            "host",
            "host:",
            "host:0",
            "host:09999",
            "host:65536",
            "host:123456",
            "host:99:1",
            "under_score:1",
            "[::1]:1",
            "host:1\n",
            "host:+1",
        ] {
            assert!(!valid_route(bad), "{bad:?}");
        }
    }
}
