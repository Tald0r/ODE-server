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
done

expect_failure "$2" "loginserver malformed offset" "Invalid loginserver -i offset" -f login.conf -i 3junk
expect_failure "$2" "loginserver overflowing offset" "Invalid loginserver -i offset" -f login.conf -i 2147483648
expect_failure "$2" "loginserver overflowing sum" "LoginServerBaseID plus -i offset" -f overflow.conf -i 1
