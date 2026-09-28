#!/usr/bin/env bash
# Byte-compares the de-core subset the client vendors with this checkout.
#
# The client carries a byte-identical copy of part of src/domain at
# third_party/decore/domain/, listed one file per line in
# third_party/decore/MANIFEST ("<sha256>  domain/<file>"). The subset is
# defined here: the sources de-core-strict compiles (DECORE_VENDORED_SOURCES
# in src/domain/CMakeLists.txt), every domain header they include, directly
# or through another header, and the parity vectors in src/domain/vectors/.
#
# A change to src/domain is done only when this is clean against a client
# working tree resynced from this checkout (the client's
# `perl tools/decore/sync.pl <server-root>`), so run it before merging:
#
#     bash tests/tools/decore_client_diff.sh ../opendarkeden-client
#
# The client root is required: no default is right for every pair of
# checkouts. Exit status: 0 when every file matches and the three lists
# agree, 1 on any difference, 2 on a usage error.
set -uo pipefail

if [ $# -ne 1 ]; then
    echo "usage: bash tests/tools/decore_client_diff.sh <client-root>" >&2
    exit 2
fi

here=$(cd "$(dirname "$0")" && pwd)
server_root=$(cd "$here/../.." && pwd)
client_root=$1
[ -d "$client_root" ] || { echo "no such directory: $client_root" >&2; exit 2; }
vendor="$client_root/third_party/decore"
manifest="$vendor/MANIFEST"

if [ ! -f "$manifest" ]; then
    echo "[FAIL] $manifest is missing: the client has no vendored de-core"
    exit 1
fi

tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT

# The server's subset, as paths relative to src/ (domain/<file>).
perl -e '
    use strict;
    use warnings;
    my $root = shift;
    my $cmake = "$root/src/domain/CMakeLists.txt";
    open(my $fh, "<", $cmake) or die "$cmake: $!\n";
    my $text = do { local $/; <$fh> };
    close $fh;
    $text =~ /set\(DECORE_VENDORED_SOURCES\s+([^)]*)\)/
        or die "no DECORE_VENDORED_SOURCES list in $cmake\n";
    my @queue = map { "domain/$_" } split " ", $1;
    my %seen;
    while (defined(my $path = shift @queue)) {
        next if $seen{$path}++;
        open(my $src, "<", "$root/src/$path") or die "$root/src/$path: $!\n";
        while (<$src>) {
            next unless /^\s*#\s*include\s*"([^"]+)"/;
            my $inc = $1;
            $inc = "domain/$inc" unless $inc =~ m{/};
            push @queue, $inc;
        }
        close $src;
    }
    opendir(my $vd, "$root/src/domain/vectors") or die "$root/src/domain/vectors: $!\n";
    $seen{"domain/vectors/$_"} = 1 for grep { -f "$root/src/domain/vectors/$_" } readdir $vd;
    closedir $vd;
    print "$_\n" for sort keys %seen;
' "$server_root" > "$tmp/server" || exit 2

# The manifest's list, and what is actually under the client's domain/.
tr -d '\r' < "$manifest" | awk 'NF == 2 { print $2 }' | LC_ALL=C sort > "$tmp/manifest"
if [ -d "$vendor/domain" ]; then
    (cd "$vendor" && find domain -type f | LC_ALL=C sort) > "$tmp/client"
else
    : > "$tmp/client"
fi
LC_ALL=C sort -o "$tmp/server" "$tmp/server"

fail=0
report() { # <label> <lines>
    if [ -n "$2" ]; then
        echo "[FAIL] $1:"
        echo "$2" | sed 's/^/         /'
        fail=1
    else
        echo "[OK]   $1: none"
    fi
}

report "vendored on the server, missing from the client's MANIFEST" \
    "$(LC_ALL=C comm -23 "$tmp/server" "$tmp/manifest")"
report "in the client's MANIFEST, not vendored on the server" \
    "$(LC_ALL=C comm -13 "$tmp/server" "$tmp/manifest")"
report "under the client's domain/, not in its MANIFEST" \
    "$(LC_ALL=C comm -13 "$tmp/manifest" "$tmp/client")"

differ=""
while IFS= read -r path; do
    if [ ! -f "$vendor/$path" ]; then
        differ="$differ$path (missing in the client)"$'\n'
    elif ! cmp -s "$server_root/src/$path" "$vendor/$path"; then
        differ="$differ$path"$'\n'
    fi
done < <(LC_ALL=C comm -12 "$tmp/server" "$tmp/manifest")
report "files that differ from this checkout" "${differ%$'\n'}"

echo "server subset: $(wc -l < "$tmp/server" | tr -d ' ') files, client MANIFEST: $(wc -l < "$tmp/manifest" | tr -d ' ') files"
exit $fail
