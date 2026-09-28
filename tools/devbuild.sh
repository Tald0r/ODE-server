#!/usr/bin/env bash
#
# devbuild.sh — fast containerised build and test for local development.
#
# WHY THIS EXISTS
#
# The obvious way to build in a container on Windows is to bind-mount the
# checkout and run cmake against it. That is what we did, and a full build
# took ~20 minutes at ~20% CPU on an 8-core machine. The compiler was not the
# bottleneck; the mount was. Measured inside the container, against the
# checkout on the Windows filesystem versus the container's own:
#
#     stat 2000 headers      0.813s   vs  0.005s   (160x)
#     read 500 headers       1.173s   vs  0.008s   (145x)
#
# Every translation unit opens dozens of headers, and every incremental build
# re-stats the whole dependency graph, so the build spends its life waiting on
# 9p round-trips instead of compiling.
#
# The build inputs are small — src/ and tests/ are ~37 MB across ~4,500 files,
# while build/, lib/ and bin/ are ~5.6 GB of artifacts. So this script copies
# the *inputs* into a container-local volume once (incrementally thereafter)
# and keeps every artifact off the mount entirely.
#
# It also switches the dev build to the same toolchain the production
# Dockerfile already uses and explains there: Ninja rather than Unix
# Makefiles, because with Makefiles a target's objects are not compiled until
# every library it links against has finished *linking*, which idles cores at
# each Core -> Packets -> server boundary; plus ccache, so a rebuild after a
# branch switch reuses objects instead of recompiling them.
#
# USAGE (from the repository root)
#
#     tools/devbuild.sh test              # build wire_tests + run ctest
#     tools/devbuild.sh test --record     # same, but re-record goldens first
#     tools/devbuild.sh build             # build every production target
#     tools/devbuild.sh build wire_tests  # build one target
#     tools/devbuild.sh output-dir        # print this lane's artifact root
#     tools/devbuild.sh shell             # interactive shell in the workspace
#     tools/devbuild.sh clean             # drop the workspace and compiler
#                                         # caches unless only the workspace
#                                         # volume was overridden — see below
#
# Parallel checkouts (git worktrees): set DEVBUILD_WORK_VOLUME to give each
# checkout its own workspace volume — two concurrent builds sharing one
# volume race this script's rsync --delete. DEVBUILD_CCACHE_VOLUME and
# DEVBUILD_ZIG_CACHE_VOLUME may stay shared (both caches are concurrency-safe).
# `clean` under a DEVBUILD_WORK_VOLUME override removes only that workspace,
# never the shared default compiler caches.
#
# DEVBUILD_JOBS=N caps the build at N compiler jobs and pins the container to
# N CPUs (docker --cpus), for building while the host is in use. Default: all.
#
# Generated test data (goldens, the wire-layout inventory, the generated
# factory list) is copied back into the checkout after a run, so a --record
# leaves a reviewable diff exactly as an in-tree build would.
#
set -euo pipefail

IMAGE=darkeden-dev
# Overridable so parallel checkouts (git worktrees) can each build in their
# own workspace volume without racing this script's rsync --delete. The
# ccache and Zig cache volumes are safe to share between concurrent builds.
WORK_VOLUME=${DEVBUILD_WORK_VOLUME:-darkeden-work}
CCACHE_VOLUME=${DEVBUILD_CCACHE_VOLUME:-darkeden-ccache}
ZIG_CACHE_VOLUME=${DEVBUILD_ZIG_CACHE_VOLUME:-darkeden-zig-cache}
JOBS=${DEVBUILD_JOBS:-\$(nproc)}
cpu_args=()
[ -n "${DEVBUILD_JOBS:-}" ] && cpu_args=(--cpus="$DEVBUILD_JOBS")
BUILD_TYPE=${BUILD_TYPE:-Debug}

# Git Bash mangles absolute paths in docker arguments unless this is set.
export MSYS_NO_PATHCONV=1

repo_root=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
# docker needs the Windows form of the path on this host; pwd -W provides it
# under Git Bash and fails harmlessly elsewhere.
repo_mount=$(cd "$repo_root" && { pwd -W 2>/dev/null || pwd; })

command=${1:-test}
shift || true

if [[ "$command" = test || "$command" = build ]] && [ "${CXX_STANDARD:-20}" != "20" ]; then
    echo "DarkEden requires CXX_STANDARD=20 (got '$CXX_STANDARD')" >&2
    exit 2
fi

if [ "$command" = "clean" ]; then
    # A worktree that overrides only its workspace volume must not wipe the
    # compiler caches every other checkout shares; remove a cache volume only
    # when it was named explicitly or when nothing was overridden (the
    # original single-checkout behavior).
    volumes=("$WORK_VOLUME")
    if [ -n "${DEVBUILD_CCACHE_VOLUME:-}" ] || [ -z "${DEVBUILD_WORK_VOLUME:-}" ]; then
        volumes+=("$CCACHE_VOLUME")
    fi
    if [ -n "${DEVBUILD_ZIG_CACHE_VOLUME:-}" ] || [ -z "${DEVBUILD_WORK_VOLUME:-}" ]; then
        volumes+=("$ZIG_CACHE_VOLUME")
    fi
    for v in "${volumes[@]}"; do
        if docker volume rm -f "$v" >/dev/null 2>&1; then
            echo "removed volume $v"
        else
            echo "could not remove volume $v (in use by a running container?)" >&2
        fi
    done
    exit 0
fi

# The image label is the authority for the actual compiler installed in the
# container. Include every compilation dimension in CMake, output and cache
# paths so changing build type, Zig version or target cannot reuse
# incompatible artifacts.
if ! zig_version=$(docker image inspect \
    --format '{{ index .Config.Labels "org.opendarkeden.zig-version" }}' \
    "$IMAGE" 2>/dev/null); then
    echo "cannot inspect $IMAGE; build it with: docker build -f Dockerfile.dev -t $IMAGE ." >&2
    exit 2
fi
if [ -z "$zig_version" ] || [ "$zig_version" = "<no value>" ]; then
    echo "$IMAGE has no Zig version label; rebuild it from Dockerfile.dev" >&2
    exit 2
fi

target_name=${ZIG_TARGET:-native}
toolchain_id=$(printf '%s' "zig-${zig_version}-${target_name}-cxx20-${BUILD_TYPE}" \
    | tr -c 'A-Za-z0-9._-' '_')
BUILD_DIR=/work/build-${toolchain_id}
OUTPUT_ROOT=/work/output-${toolchain_id}
CCACHE_DIR=/ccache/${toolchain_id}
ZIG_CACHE_DIR=/zig-cache/${toolchain_id}

if [ "$command" = "output-dir" ]; then
    echo "$OUTPUT_ROOT"
    exit 0
fi

record=0
args=()
for arg in "$@"; do
    if [ "$arg" = "--record" ]; then
        record=1
    else
        args+=("$arg")
    fi
done

# Only the build inputs are synced. Everything else in the checkout (the
# 2.7 GB of build trees, lib/, bin/, .git) never crosses the mount.
sync_in='rsync -a --delete --exclude=.git --exclude=/src/server/websocketproxyserver/target \
    /repo/cmake /repo/src /repo/tests /repo/third_party /repo/data /repo/initdb \
    /repo/CMakeLists.txt /repo/Makefile /work/ &&
    mkdir -p /work/docker && rsync -a --checksum /repo/docker/start.sh /work/docker/'

# Copy generated test data back so a re-record shows up as a normal diff:
# the goldens and inventories under tests/, and, after --record only,
# de-core's shared parity vectors, which live beside the sources they pin.
# The vectors' rows are written by hand, and the copy is the snapshot
# sync_in took when the run started, so copying it back after an ordinary
# run would revert an edit saved on the host while the suite was building.
# --checksum because the container clock and the mount can disagree on mtime.
sync_out='rsync -a --checksum \
    /work/tests/golden /work/tests/generated /work/tests/wire-layout.txt \
    /repo/tests/'
if [ "$record" = "1" ]; then
    sync_out="$sync_out && rsync -a --checksum /work/src/domain/vectors/ /repo/src/domain/vectors/"
fi

configure='cmake -G Ninja -B '"$BUILD_DIR"' -S /work \
    -DCMAKE_TOOLCHAIN_FILE=/work/cmake/zig-toolchain.cmake \
    -DCMAKE_BUILD_TYPE='"$BUILD_TYPE"' \
    -DCMAKE_CXX_STANDARD=20 \
    -DDARKEDEN_ZIG_TARGET='"${ZIG_TARGET:-}"' \
    -DDARKEDEN_OUTPUT_ROOT='"$OUTPUT_ROOT"' \
    -DDARKEDEN_BUILD_TESTS=ON \
    -DCMAKE_C_COMPILER_LAUNCHER=ccache \
    -DCMAKE_CXX_COMPILER_LAUNCHER=ccache'

case "$command" in
    test)
        target=${args[0]:-wire_tests}
        recorder=""
        [ "$record" = "1" ] && recorder='echo "--- recording goldens"; (cd '"$BUILD_DIR"' && UPDATE_GOLDENS=1 '"$OUTPUT_ROOT"'/bin/wire_tests >/dev/null && UPDATE_GOLDENS=1 '"$OUTPUT_ROOT"'/bin/formula_tests >/dev/null);'
        # The recorder and ctest run as one group so that a failed build skips
        # both: a bare `$recorder` ending in `;` would end the && chain and let
        # ctest run a stale binary and report it green.
        script="$sync_in && $configure >/dev/null && cmake --build $BUILD_DIR --target $target -j$JOBS && { $recorder (cd $BUILD_DIR && ctest --output-on-failure); }; rc=\$?; $sync_out; exit \$rc"
        ;;
    build)
        target=${args[0]:-}
        target_arg=""
        [ -n "$target" ] && target_arg="--target $target"
        script="$sync_in && $configure >/dev/null && cmake --build $BUILD_DIR $target_arg -j$JOBS"
        ;;
    shell)
        script="$sync_in; echo 'workspace is /work (sources synced from the mounted checkout)'; exec bash"
        ;;
    *)
        echo "usage: tools/devbuild.sh {test|build|shell|clean|output-dir} [target] [--record]" >&2
        exit 2
        ;;
esac

# A TTY is only available (and only wanted) when a human is driving; asking
# for one from a script or CI makes docker refuse to start the container.
tty_args=()
if [ -t 0 ] && [ -t 1 ]; then
    tty_args=(-it)
elif [ "$command" = "shell" ]; then
    echo "devbuild.sh shell needs a terminal" >&2
    exit 2
fi

# The checkout stays read-only except tests/ and src/domain/vectors/, the
# places sync_out writes: nested rw mounts over the ro one. Without them a
# --record run's rsync back fails on the read-only filesystem — and
# silently, since exit $rc reports ctest's status, not the copy's.
#
# The ${a[@]+"${a[@]}"} spelling expands an empty array to nothing: macOS
# ships bash 3.2, where a bare "${a[@]}" of an empty array is an unbound
# variable under set -u.
exec docker run --rm ${tty_args[@]+"${tty_args[@]}"} ${cpu_args[@]+"${cpu_args[@]}"} \
    -v "$repo_mount:/repo:ro" \
    -v "$repo_mount/tests:/repo/tests" \
    -v "$repo_mount/src/domain/vectors:/repo/src/domain/vectors" \
    -v "$WORK_VOLUME:/work" \
    -v "$CCACHE_VOLUME:/ccache" \
    -v "$ZIG_CACHE_VOLUME:/zig-cache" \
    -e CCACHE_DIR="$CCACHE_DIR" \
    -e ZIG_GLOBAL_CACHE_DIR="$ZIG_CACHE_DIR" \
    -e ZIG_TARGET="${ZIG_TARGET:-}" \
    -w /work \
    "$IMAGE" bash -c "$script"
