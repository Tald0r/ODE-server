# ============================================================================
# de-core — pure domain logic (docs/RESTRUCTURING.md Phase 3).
#
# Freestanding by construction: no Core, no server targets, no MySQL/Lua/
# transport, no server-type macros. Consumers include it as
# "domain/<Header>.h" via the PUBLIC include dir below, and the library is
# unit-tested in isolation by formula_tests (tests/CMakeLists.txt).
#
# Paths are relative to this directory, never to the top-level source dir,
# so the rules below mean the same thing wherever the directory is built.
# ============================================================================

set(DECORE_INCLUDE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/..")

# The same source must give the same numbers on every toolchain that builds
# it: no fused multiply-add and no fast-math reassociation. Clang, and so
# `zig c++`, contracts `a * b + c` into a fused multiply-add whenever the
# target CPU has one, at -O0 too: on every arm64 target, and on x86-64 when
# the CPU has FMA. A Zig build without a target compiles for the build
# machine's CPU, which on any recent x86-64 machine has FMA.
set(DECORE_FP_FLAGS -ffp-contract=off -fno-fast-math)

add_library(de-core STATIC
    Formulas.cpp
    ItemDurability.cpp
    ItemGrade.cpp
    ItemPrice.cpp
    SkillOutputFormulas.cpp
)

target_include_directories(de-core PUBLIC ${DECORE_INCLUDE_ROOT})
# "Freestanding by construction" must actually be constructed: the top-level
# include_directories() would leak src/Core and src/server into this target,
# letting a Core/transport include compile clean. Pin the target's own
# include path so the compiler enforces the boundary, not just review.
set_target_properties(de-core PROPERTIES INCLUDE_DIRECTORIES "${DECORE_INCLUDE_ROOT}")
target_compile_options(de-core PRIVATE ${DECORE_FP_FLAGS})

# de-core-strict compiles the sources the client repository vendors (its
# third_party/decore copy, byte for byte) under a warning set at least as
# strict as the client's zero-warning budget: GCC/Clang -Wall -Wextra
# -Wundef, plus -Wimplicit-fallthrough and -Wfloat-conversion as a proxy for
# MSVC's C4244. A warning here would be a warning in every client build, so
# it is an error. The objects are thrown away; tests/CMakeLists.txt makes
# wire_tests depend on this target so the suite build compiles it.
# SkillOutputFormulas.cpp is not vendored and so is not listed.
set(DECORE_VENDORED_SOURCES
    Formulas.cpp
    ItemDurability.cpp
    ItemGrade.cpp
    ItemPrice.cpp
)
add_library(de-core-strict OBJECT EXCLUDE_FROM_ALL ${DECORE_VENDORED_SOURCES})
set_target_properties(de-core-strict PROPERTIES INCLUDE_DIRECTORIES "${DECORE_INCLUDE_ROOT}")
target_compile_options(de-core-strict PRIVATE
    ${DECORE_FP_FLAGS}
    -Wall
    -Wextra
    -Wundef
    -Wimplicit-fallthrough
    -Wfloat-conversion
    -Werror
)
