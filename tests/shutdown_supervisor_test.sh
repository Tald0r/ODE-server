#!/usr/bin/env bash
set -euo pipefail
probe=$(realpath "$1")
supervisor=$(realpath "$2")
scratch=
supervisor_pid=
db_pid=
cleanup() {
    if [ -n "$supervisor_pid" ]; then
        kill -TERM "$supervisor_pid" 2>/dev/null || true
        wait "$supervisor_pid" 2>/dev/null || true
    fi
    if [ -n "$db_pid" ]; then kill "$db_pid" 2>/dev/null || true; fi
    # Exact test-owned mktemp directory, never a checkout or cache volume.
    if [ -n "$scratch" ]; then rm -rf -- "$scratch"; fi
}
trap cleanup EXIT

# Bring up the real supervisor over stand-in servers. $1 names the stand-ins
# that must ignore SIGTERM (empty for the cooperative case).
start_stack() {
    scratch=$(mktemp -d)
    mkdir "$scratch/bin" "$scratch/conf"
    cd "$scratch/bin"
    "$probe" db &
    db_pid=$!
    for ((i=0; i<100; i++)); do
        [ ! -f db.port ] || break
        sleep 0.01
    done
    test -f db.port
    port=$(<db.port)
    for name in loginserver sharedserver gameserver; do
        ln -s "$probe" "$name"
        printf 'DB_HOST : 127.0.0.1\nDB_PORT : %s\n' "$port" > "$scratch/conf/$name.conf"
    done
    VS_HOME="$scratch" SHUTDOWN_PROBE_HANG="$1" bash "$supervisor" > supervisor.log 2>&1 &
    supervisor_pid=$!
    for ((i=0; i<200; i++)); do
        [ ! -f gameserver.ready ] || break
        sleep 0.05
    done
    test -f gameserver.ready
}

stop_stack() {
    kill -TERM "$supervisor_pid"
    wait "$supervisor_pid"
    supervisor_pid=
    kill "$db_pid" 2>/dev/null || true
    db_pid=
}

drop_stack() {
    rm -rf -- "$scratch"
    scratch=
}

# 1. Every server drains cooperatively, and the gameserver goes first: the
#    probe exits 3 if login/shared stopped before it did.
start_stack ""
stop_stack
for name in gameserver loginserver sharedserver; do
    test -f "$scratch/bin/$name.stopped"
    grep -Fq "[$name] probe startup" "$scratch/bin/supervisor.log"
    grep -Fq "[$name] probe stderr" "$scratch/bin/supervisor.log"
    grep -Fq "[$name] probe final line" "$scratch/bin/supervisor.log"
done
test -z "$(find "$scratch/bin" -name '*.out' -print -quit)"
drop_stack
echo 'SIGTERM drained gameserver before stopping login/shared services'

# 2. loginserver and sharedserver now drain cooperatively too, so the
#    supervisor can no longer assume they die the moment SIGTERM lands. One
#    that never drains must be force-terminated inside the supervisor's own
#    bound, leaving headroom under Compose's 45s stop grace period.
start_stack loginserver
begin=$SECONDS
stop_stack
elapsed=$((SECONDS - begin))
test -f "$scratch/bin/gameserver.stopped"
test -f "$scratch/bin/sharedserver.stopped"
if [ -f "$scratch/bin/loginserver.stopped" ]; then
    echo "loginserver was supposed to ignore SIGTERM but drained anyway" >&2
    exit 1
fi
if [ "$elapsed" -ge 25 ]; then
    echo "supervisor took ${elapsed}s to give up on a stuck loginserver" >&2
    exit 1
fi
grep -q 'loginserver did not drain; forcing termination' "$scratch/bin/supervisor.log"
drop_stack
echo "supervisor force-terminated a stuck login service after ${elapsed}s"

# 3. An independently failed child ends the stack; the supervisor still owns
# the actual server PID and drains the surviving servers' final output.
start_stack ""
kill -KILL "$(cat "$scratch/bin/gameserver.ready")"
if wait "$supervisor_pid"; then
    echo "supervisor ignored a failed child" >&2
    exit 1
fi
supervisor_pid=
for name in loginserver sharedserver; do
    test -f "$scratch/bin/$name.stopped"
    grep -Fq "[$name] probe final line" "$scratch/bin/supervisor.log"
done
# The child's death also closes its pipe. Either exit can be observed first
# when it occurs between the supervisor's server and forwarder checks.
grep -Eq 'gameserver (log forwarder )?exited with status' "$scratch/bin/supervisor.log"
test -z "$(find "$scratch/bin" -name '*.out' -print -quit)"
kill "$db_pid" 2>/dev/null || true
db_pid=
drop_stack
echo 'A failed child stopped its siblings without creating duplicate log files'

# 4. A failed log reader must also end the stack, even while its server is
# alive and quiet. The remaining forwarders still drain normal final lines.
start_stack ""
forwarder_pid=
for child in $(cat "/proc/$supervisor_pid/task/$supervisor_pid/children"); do
    if tr '\0' '\n' < "/proc/$child/cmdline" 2>/dev/null | grep -Fxq 's/^/[loginserver] /'; then
        forwarder_pid=$child
        break
    fi
done
test -n "$forwarder_pid"
kill -KILL "$forwarder_pid"
if wait "$supervisor_pid"; then
    echo "supervisor ignored a failed log forwarder" >&2
    exit 1
fi
supervisor_pid=
for name in gameserver sharedserver; do
    test -f "$scratch/bin/$name.stopped"
    grep -Fq "[$name] probe final line" "$scratch/bin/supervisor.log"
done
grep -q 'loginserver log forwarder exited with status' "$scratch/bin/supervisor.log"
kill "$db_pid" 2>/dev/null || true
db_pid=
drop_stack
echo 'A failed log forwarder stopped the stack and drained the remaining output'
