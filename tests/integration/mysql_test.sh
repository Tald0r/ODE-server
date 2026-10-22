#!/usr/bin/env bash
#
# Handrolled container fixture for the MySQL integration tier
# (docs/reviews/RESTRUCTURING.md 3.2) — what testcontainers does for languages
# that have it: an ephemeral MySQL per run, hermetic, torn down on exit.
#
#   1. build mysql_repository_tests via tools/devbuild.sh (artifacts live
#      in the devbuild work volume);
#   2. start a throwaway MySQL 5.7 — the same image, sql_mode and initdb
#      scripts docker/docker-compose.yml uses — on a private network;
#   3. wait until the entrypoint has imported the schema (it only listens
#      on TCP after the initdb scripts finish, same trick as the compose
#      healthcheck);
#   4. run the binary from the work volume in a darkeden-dev container on
#      that network;
#   5. tear everything down, pass or fail.
#
# Run from the repository root: bash tests/integration/mysql_test.sh
# (or `make integration-test`). Needs docker and the darkeden-dev image,
# like tools/devbuild.sh itself. DEVBUILD_WORK_VOLUME is honored the same
# way — worktrees keep their own volume. DEVBUILD_IT_NET and DEVBUILD_IT_DB
# name the private network and the MySQL container, so two checkouts can
# run the tier at the same time without tearing each other's fixture down.
set -euo pipefail

NET=${DEVBUILD_IT_NET:-darkeden-it-net}
DB=${DEVBUILD_IT_DB:-darkeden-it-mysql}
MYSQL_IMAGE=mysql/mysql-server:5.7
DEV_IMAGE=darkeden-dev
WORK_VOLUME=${DEVBUILD_WORK_VOLUME:-darkeden-work}
SQL_MODE='ONLY_FULL_GROUP_BY,NO_ZERO_IN_DATE,ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION'

# Git Bash mangles absolute paths in docker arguments unless this is set.
export MSYS_NO_PATHCONV=1

repo_root=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
repo_mount=$(cd "$repo_root" && { pwd -W 2>/dev/null || pwd; })

cleanup() {
    docker rm -f "$DB" >/dev/null 2>&1 || true
    docker network rm "$NET" >/dev/null 2>&1 || true
}
trap cleanup EXIT
cleanup

echo "--- building mysql_repository_tests"
bash "$repo_root/tools/devbuild.sh" build mysql_repository_tests
output_root=$(bash "$repo_root/tools/devbuild.sh" output-dir)

echo "--- starting throwaway MySQL ($MYSQL_IMAGE)"
docker network create "$NET" >/dev/null
docker run -d --name "$DB" --network "$NET" \
    -v "$repo_mount/initdb/DARKEDEN.sql":/docker-entrypoint-initdb.d/1-DARKEDEN.sql:ro \
    -v "$repo_mount/initdb/USERINFO.sql":/docker-entrypoint-initdb.d/1b-USERINFO.sql:ro \
    -v "$repo_mount/initdb/a-setup.sql":/docker-entrypoint-initdb.d/2-a-setup.sql:ro \
    -e MYSQL_ROOT_PASSWORD=123456 \
    "$MYSQL_IMAGE" mysqld --sql_mode="$SQL_MODE" \
    --character-set-server=utf8mb4 --collation-server=utf8mb4_unicode_ci >/dev/null

echo "--- waiting for the schema import"
for _ in $(seq 1 90); do
    if docker exec "$DB" mysqladmin ping -h 127.0.0.1 -uelcastle -pelca110 --silent >/dev/null 2>&1; then
        ready=1
        break
    fi
    sleep 2
done
if [ -z "${ready:-}" ]; then
    echo "MySQL never became ready; container log tail:" >&2
    docker logs --tail 30 "$DB" >&2
    exit 1
fi

echo "--- running mysql_repository_tests"
docker run --rm --network "$NET" \
    -v "$WORK_VOLUME:/work" \
    -e IT_DB_HOST="$DB" \
    "$DEV_IMAGE" "$output_root/bin/mysql_repository_tests"
