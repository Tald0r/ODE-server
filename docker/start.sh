#!/bin/bash
#
# Entrypoint for the containerized DarkEden server.
#
# Waits for the database to accept connections, then starts loginserver,
# sharedserver and gameserver in that order and keeps streaming their logs
# to stdout. The container exits as soon as one of the servers dies.
#
# Environment:
#   CONF_DIR         configuration directory   (default /home/darkeden/vs/conf)
#   DB_WAIT_RETRIES  database wait attempts, 2s apart          (default 150)

set -u

VS_HOME=${VS_HOME:-/home/darkeden/vs}
BIN_DIR="$VS_HOME/bin"
CONF_DIR="${CONF_DIR:-$VS_HOME/conf}"
DB_WAIT_RETRIES="${DB_WAIT_RETRIES:-150}"

cd "$BIN_DIR" || exit 1

log() { echo "[start.sh] $*"; }

# Read a "Key : Value" entry out of a server configuration file.
read_conf() {
    awk -F: -v key="$2" '
        $1 ~ "^[[:space:]]*"key"[[:space:]]*$" {
            gsub(/^[[:space:]]+|[[:space:]]+$/, "", $2)
            print $2
            exit
        }' "$1"
}

wait_for_db() {
    local host port i
    host=$(read_conf "$CONF_DIR/loginserver.conf" DB_HOST)
    port=$(read_conf "$CONF_DIR/loginserver.conf" DB_PORT)
    host="${host:-odk-mysql}"
    port="${port:-3306}"

    log "waiting for the database at $host:$port ..."
    for ((i = 1; i <= DB_WAIT_RETRIES; i++)); do
        if (exec 3<>"/dev/tcp/$host/$port") 2>/dev/null; then
            exec 3<&-
            log "database is up"
            return 0
        fi
        sleep 2
    done

    log "ERROR: database at $host:$port is not reachable"
    return 1
}

names=()
pids=()
forwarders=()

start_server() {
    local name="$1"
    local conf="$CONF_DIR/$2"
    local logfd

    if [ ! -f "$conf" ]; then
        log "ERROR: missing configuration file $conf"
        return 1
    fi

    log "starting $name"
    # The pipe carries prefixed output directly to the container log driver.
    # Keep the server's real PID for shutdown and close the supervisor's writer
    # immediately, so the forwarder sees EOF when that server exits.
    exec {logfd}> >(exec sed -u "s/^/[$name] /")
    forwarders+=($!)
    stdbuf -oL -eL ./"$name" -f "$conf" >&"$logfd" 2>&1 &
    pids+=($!)
    names+=("$name")
    exec {logfd}>&-

}

is_alive() {
    local state
    state=$(sed -n 's/.*) \([A-Za-z]\).*/\1/p' "/proc/$1/stat" 2>/dev/null)
    [ -n "$state" ] && [ "$state" != "Z" ]
}

shutdown_all() {
    trap '' INT TERM
    log "stopping servers ..."
    local pid i deadline result=0
    # Keep login/shared services alive while the gameserver drains. Its own
    # 30s deadline handles stuck I/O; this 35s supervisor bound is a backstop.
    for i in "${!pids[@]}"; do
        if [ "${names[$i]}" = gameserver ]; then
            pid=${pids[$i]}
            kill -TERM "$pid" 2>/dev/null || true
            deadline=$((SECONDS + 35))
            while is_alive "$pid" && (( SECONDS < deadline )); do sleep 0.1; done
            if is_alive "$pid"; then
                log "gameserver did not drain; forcing termination"
                kill -KILL "$pid" 2>/dev/null || true
            fi
            wait "$pid" 2>/dev/null || result=$?
        fi
    done
    # login/shared drain cooperatively too now, so they no longer die the
    # instant SIGTERM lands. Bound them here as well: Compose allows 45s in
    # total, and 35s for the gameserver plus 8s here leaves headroom before
    # docker escalates to SIGKILL. Each server still carries its own 30s
    # failed-exit deadline for a run outside this supervisor.
    for i in "${!pids[@]}"; do
        [ "${names[$i]}" = gameserver ] || kill -TERM "${pids[$i]}" 2>/dev/null || true
    done
    deadline=$((SECONDS + 8))
    for i in "${!pids[@]}"; do
        [ "${names[$i]}" != gameserver ] || continue
        pid=${pids[$i]}
        while is_alive "$pid" && (( SECONDS < deadline )); do sleep 0.1; done
        if is_alive "$pid"; then
            log "${names[$i]} did not drain; forcing termination"
            kill -KILL "$pid" 2>/dev/null || true
        fi
    done
    wait "${pids[@]}" 2>/dev/null
    # Bound waiting for forwarders after servers stop. Other writes can still
    # block on a stalled container sink under Docker's blocking delivery mode.
    deadline=$((SECONDS + 1))
    for pid in "${forwarders[@]}"; do
        while is_alive "$pid" && (( SECONDS < deadline )); do sleep 0.01; done
        if is_alive "$pid"; then kill -TERM "$pid" 2>/dev/null || true; fi
        wait "$pid" 2>/dev/null || true
    done
    return "$result"
}

trap 'shutdown_all; exit $?' INT TERM

wait_for_db || exit 1

start_server loginserver loginserver.conf || exit 1
sleep 3
start_server sharedserver sharedserver.conf || { shutdown_all; exit 1; }
sleep 3
start_server gameserver gameserver.conf || { shutdown_all; exit 1; }

log "all servers started"

# A child that already exited stays around as a zombie until it is waited for,
# so `kill -0` is not enough - look at the process state instead.
# Supervise: if any server exits, tear the rest down so docker notices.
while true; do
    for i in "${!pids[@]}"; do
        if ! is_alive "${pids[$i]}"; then
            wait "${pids[$i]}" 2>/dev/null
            log "${names[$i]} exited with status $? - shutting down"
            shutdown_all
            exit 1
        fi
    done
    # Each pipe is now the only copy of that server's console output. Losing
    # its reader must end the stack even if the server ignores SIGPIPE.
    for i in "${!forwarders[@]}"; do
        if ! is_alive "${forwarders[$i]}"; then
            wait "${forwarders[$i]}" 2>/dev/null
            log "${names[$i]} log forwarder exited with status $? - shutting down"
            shutdown_all
            exit 1
        fi
    done
    sleep 5
done
