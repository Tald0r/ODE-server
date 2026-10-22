# syntax=docker/dockerfile:1
#
# Build the DarkEden servers from source and package them into a runnable image.
#
#   docker build -t darkeden:local .
#
# docker/docker-compose.yml uses this Dockerfile, so `docker compose up` builds
# everything from this repository - no pre-built image is pulled from a registry.

# ----------------------------------------------------------------------------
# Stage 1: build with the same pinned Zig/Clang toolchain as Dockerfile.dev
# ----------------------------------------------------------------------------
FROM ubuntu:20.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive
ARG ZIG_VERSION=0.16.0

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    ccache \
    cmake \
    ninja-build \
    pkg-config \
    python3 \
    python3-pip \
    libmysqlclient-dev \
    liblua5.1-dev \
    zlib1g-dev \
    && python3 -m pip install --no-cache-dir "ziglang==${ZIG_VERSION}" \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /home/darkeden/vs

# Only the sources are needed to build; data/ and conf/ go into the final image.
COPY CMakeLists.txt ./
COPY cmake ./cmake
COPY third_party ./third_party
COPY src ./src
# src/Core/CMakeLists.txt reads the de-kernel membership list at configure
# time (docs/reviews/RESTRUCTURING.md 2.1), so this one file from tests/ must be in
# the build context even though the test suite itself is not built here.
COPY tests/arch/kernel_files.txt ./tests/arch/kernel_files.txt

# Release by default. Optimized builds no longer define NDEBUG (see
# CMakeLists.txt), which is what used to strip the side effects out of the
# project's Assert() and __END_CATCH_NO_RETHROW macros.
ARG BUILD_TYPE=Release
ARG ZIG_TARGET=
# Number of parallel compile jobs; defaults to all available cores.
ARG BUILD_JOBS=

# Ninja instead of Unix Makefiles: with Makefiles a target's objects are not
# compiled until every library it links against has finished *linking*, which
# leaves cores idle at each Core -> Packets -> server layer boundary. Ninja
# orders individual compile steps only by their headers, so every translation
# unit in the tree compiles concurrently across all cores.
#
# ccache in a BuildKit cache mount keeps object files across image rebuilds so
# only the sources that changed are recompiled. Needs BuildKit (the default
# for docker >= 23 and docker compose v2).
RUN --mount=type=cache,target=/ccache,sharing=locked \
    --mount=type=cache,target=/zig-cache,sharing=locked \
    toolchain_id="zig-${ZIG_VERSION}-${ZIG_TARGET:-native}-cxx20-${BUILD_TYPE}" \
    && toolchain_id="$(printf '%s' "${toolchain_id}" | tr -c 'A-Za-z0-9._-' '_')" \
    && export CCACHE_DIR="/ccache/${toolchain_id}" \
        ZIG_GLOBAL_CACHE_DIR="/zig-cache/${toolchain_id}" \
    && cmake -G Ninja -B build \
        -DCMAKE_TOOLCHAIN_FILE=cmake/zig-toolchain.cmake \
        -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
        -DCMAKE_CXX_STANDARD=20 \
        -DDARKEDEN_ZIG_TARGET="${ZIG_TARGET}" \
        -DCMAKE_C_COMPILER_LAUNCHER=ccache \
        -DCMAKE_CXX_COMPILER_LAUNCHER=ccache \
    && cmake --build build -j "${BUILD_JOBS:-$(nproc)}" \
    && ccache -s

# ----------------------------------------------------------------------------
# Stage 2: runtime - only the shared libraries the servers link against
# ----------------------------------------------------------------------------
FROM ubuntu:20.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    libmysqlclient21 \
    liblua5.1-0 \
    zlib1g \
    psmisc \
    && rm -rf /var/lib/apt/lists/*

# CMAKE_RUNTIME_OUTPUT_DIRECTORY puts loginserver/sharedserver/gameserver here.
COPY --from=builder /home/darkeden/vs/bin/ /home/darkeden/vs/bin/
COPY data /home/darkeden/vs/data/
COPY docker/conf /home/darkeden/vs/conf/
COPY docker/start.sh /home/darkeden/vs/bin/start.sh

# A Windows checkout can carry CRLF endings, which both the configuration
# parser (Properties::load) and /bin/bash choke on - normalize them here so the
# image builds correctly regardless of how the repository was cloned.
RUN sed -i 's/\r$//' /home/darkeden/vs/conf/*.conf /home/darkeden/vs/bin/start.sh \
    && chmod +x /home/darkeden/vs/bin/start.sh

WORKDIR /home/darkeden/vs/bin

# loginserver TCP, gameserver TCP, gameserver UDP
EXPOSE 9999 9998 9997/udp

CMD ["./start.sh"]
