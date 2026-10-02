#!/usr/bin/env bash
# Exercise the real entry points' failure path, including exceptions that the
# old catch(Error&) blocks missed. No valid server configuration is supplied.
set -euo pipefail

if [ "$#" -ne 3 ]; then
    echo "usage: $0 gameserver loginserver sharedserver" >&2
    exit 2
fi

startup_test_dir=$(mktemp -d)
trap 'rm -rf "$startup_test_dir"' EXIT
cd "$startup_test_dir"

cat > malformed.conf <<'EOF'
HomePath : /unused
missing separator
EOF
printf 'HomePath : /unused\nmissing separator' > malformed-final.conf
printf 'HomePath : /unused\n : value\n' > missing-key.conf
mkdir config-directory
cat > login.conf <<'EOF'
LoginServerBasePort : 9900
LoginServerBaseUDPPort : 9800
LoginServerBaseID : 10
EOF
cat > overflow.conf <<'EOF'
LoginServerBasePort : 9900
LoginServerBaseUDPPort : 9800
LoginServerBaseID : 2147483647
EOF
cat > listeners.conf <<'EOF'
TCPPort : 9998
GameServerUDPPort : 9997
LoginServerPort : 9999
LoginServerUDPPort : 9996
EOF
cat > high-udp.conf <<'EOF'
LoginServerBasePort : 65000
LoginServerBaseUDPPort : 65535
LoginServerBaseID : 10
EOF

expect_failure() {
    local binary="$1" label="$2" diagnostic="$3" command_status=0
    shift 3
    "$binary" "$@" > stdout.log 2> stderr.log || command_status=$?
    if [ "$command_status" -ne 1 ] || ! grep -F -- "$diagnostic" stderr.log >/dev/null; then
        echo "[FAIL] $label: expected exit 1 and diagnostic '$diagnostic'; got exit $command_status" >&2
        cat stdout.log stderr.log >&2
        return 1
    fi
    echo "[OK] $label"
}

for server_binary in "$@"; do
    server_name=$(basename "$server_binary")
    expect_failure "$server_binary" "$server_name missing arguments" "Usage : $server_name"
    expect_failure "$server_binary" "$server_name wrong flag" "Usage : $server_name" -x missing.conf
    expect_failure "$server_binary" "$server_name missing file" "missing.conf" -f missing.conf
    expect_failure "$server_binary" "$server_name malformed file" "missing separator" -f malformed.conf
    expect_failure "$server_binary" "$server_name malformed final line" "missing separator" -f malformed-final.conf
    expect_failure "$server_binary" "$server_name missing key" "missing key" -f missing-key.conf
    expect_failure "$server_binary" "$server_name unreadable directory" "error reading properties" -f config-directory

    case "$server_name" in
        gameserver) listener_keys="TCPPort GameServerUDPPort" ;;
        loginserver) listener_keys="LoginServerPort LoginServerUDPPort" ;;
        sharedserver) listener_keys="TCPPort" ;;
        *) echo "unknown server: $server_name" >&2; exit 2 ;;
    esac
    for listener_key in $listener_keys; do
        for invalid_port in 0 -1 65536 9999junk 4294967297 none; do
            cp listeners.conf invalid-port.conf
            printf '%s : %s\n' "$listener_key" "$invalid_port" >> invalid-port.conf
            expect_failure "$server_binary" "$server_name $listener_key=$invalid_port" \
                "$listener_key must be a decimal port from 1 to 65535" -f invalid-port.conf
        done
        sed "/^$listener_key :/d" listeners.conf > missing-port.conf
        expect_failure "$server_binary" "$server_name missing $listener_key" "$listener_key" -f missing-port.conf
    done
done

expect_failure "$2" "loginserver malformed offset" "Invalid loginserver -i offset" -f login.conf -i 3junk
expect_failure "$2" "loginserver overflowing offset" "Invalid loginserver -i offset" -f login.conf -i 2147483648
expect_failure "$2" "loginserver overflowing sum" "LoginServerBaseID plus -i offset" -f overflow.conf -i 1
expect_failure "$2" "loginserver zero effective TCP port" "LoginServerPort must be a decimal port from 1 to 65535" \
    -f login.conf -i -9900
expect_failure "$2" "loginserver zero effective UDP port" "LoginServerUDPPort must be a decimal port from 1 to 65535" \
    -f login.conf -i -9800
expect_failure "$2" "loginserver oversized effective TCP port" "LoginServerPort must be a decimal port from 1 to 65535" \
    -f login.conf -i 55636
expect_failure "$2" "loginserver oversized effective UDP port" "LoginServerUDPPort must be a decimal port from 1 to 65535" \
    -f high-udp.conf -i 1
