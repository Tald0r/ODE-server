# ============================================================================
# de-kernel (docs/reviews/RESTRUCTURING.md 2.1) — the wire primitives, built as a
# standalone target whose only include dir is src/Core. Membership is the
# single source of truth in tests/arch/kernel_files.txt (the include-graph
# test binds the same list); a kernel source that reaches for a gameserver
# header fails to compile here, which is the point. This is THE archive the
# apps link (task 2.4): every kernel .cpp is compiled here exactly once,
# macro-free — K2 bans server-type macros (and __COMBAT__) in kernel files,
# so the objects are identical for every consumer — and Core plus the
# per-server packet libraries carry only the handful of files the kernel
# cannot own.
# ============================================================================
# REGEX keeps only the entry lines: comment lines can contain ';', which
# CMake list expansion would otherwise split into pseudo-entries. Editing
# the list must retrigger configure, hence CONFIGURE_DEPENDS — without it a
# bare `ninja` keeps building the previous membership.
file(STRINGS ${CMAKE_SOURCE_DIR}/tests/arch/kernel_files.txt
     DARKEDEN_KERNEL_ENTRIES REGEX "^src/")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
             ${CMAKE_SOURCE_DIR}/tests/arch/kernel_files.txt)
set(KERNEL_SOURCES "")
foreach(entry ${DARKEDEN_KERNEL_ENTRIES})
    if(entry MATCHES "^src/.*\\.cpp$")
        list(APPEND KERNEL_SOURCES ${CMAKE_SOURCE_DIR}/${entry})
    endif()
endforeach()
add_library(de-kernel STATIC ${KERNEL_SOURCES})
target_include_directories(de-kernel PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(de-kernel PUBLIC Threads::Threads)
# The top-level include_directories() (src/server, src/server/database,
# MySQL) is a directory property this target would inherit, letting a kernel
# source include an app or DB header and compile clean. Pin the target's own
# include path to src/Core alone so the compiler enforces K1 alongside the
# perl checker.
set_target_properties(de-kernel PROPERTIES
    INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}")

# ============================================================================
# Core — what is left after the kernel flip (task 2.4): only the src/Core
# files that CANNOT be kernel members. Everything else Core used to compile
# now comes from the de-kernel archive it links PUBLIC, so existing
# consumers keep getting the same objects through the same target name.
# ============================================================================

set(CORE_SOURCES
    # tinyxml2 binding — deliberately held out of the kernel (2.4 sort).
    SXml.cpp
    # Uses the server-side Timeval — held out of the kernel (2.4 sort).
    TimeChecker.cpp
    # The factory-backed receive path split out of Datagram.cpp: it calls
    # PacketFactoryManager, which K2 bars from the kernel forever, so this
    # one function compiles here and the rest of the framing is kernel.
    DatagramFactoryRead.cpp
)

add_library(Core STATIC ${CORE_SOURCES})
# SXml.cpp parses with tinyxml2; PUBLIC so consumers inherit the include path
# and the archive without each having to name it. de-kernel is PUBLIC for the
# same reason: linking Core must keep resolving every symbol it used to
# provide itself.
target_link_libraries(Core PUBLIC tinyxml2 de-kernel)
target_include_directories(Core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# ============================================================================
# Per-server packet libraries — after the kernel flip (task 2.4) each is
# only the three files whose whole purpose is per-server `#if` compilation:
# which factories/validator sets exist on this server. Every wire class
# they used to duplicate now comes from the single macro-free de-kernel
# compile they link PUBLIC. The hand-kept per-direction source lists are
# gone with them; membership lives in tests/arch/kernel_files.txt alone.
# ============================================================================
set(PACKET_COMMON_SOURCES
    PacketFactoryManager.cpp
    PacketIDSet.cpp
    PacketValidator.cpp
)

# GameServerPackets — PACKET_COMMON compiled with __GAME_SERVER__
add_library(GameServerPackets STATIC ${PACKET_COMMON_SOURCES})
target_compile_definitions(GameServerPackets PRIVATE __GAME_SERVER__ __COMBAT__)
target_include_directories(GameServerPackets PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src/server
    ${CMAKE_SOURCE_DIR}/src/server/database
)
target_link_libraries(GameServerPackets PUBLIC de-kernel)

# LoginServerPackets — PACKET_COMMON compiled with __LOGIN_SERVER__
add_library(LoginServerPackets STATIC ${PACKET_COMMON_SOURCES})
target_compile_definitions(LoginServerPackets PRIVATE __LOGIN_SERVER__)
target_include_directories(LoginServerPackets PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src/server
    ${CMAKE_SOURCE_DIR}/src/server/database
    ${CMAKE_SOURCE_DIR}/src/server/loginserver
)
target_link_libraries(LoginServerPackets PUBLIC de-kernel)

# ============================================================================
# TestPackets — PACKET_COMMON with NO server-type macro, for the
# wire-contract tests (tests/): the full factory/validator set with the
# wire classes coming from de-kernel, which is by construction the same
# macro-free compile the deployed servers link (task 2.4).
#
# NOTE: this is NOT the client's configuration — the client compiles with
# __GAME_CLIENT__=1. The wire classes carry no such guards any more (the
# last ones left with task 2.4's split; only PacketFactoryManager.cpp /
# PacketValidator.cpp / PlayerStatus.h still mention the macro, none of it
# wire code), so the goldens reflect what the deployed server emits; a
# future __GAME_CLIENT__-guarded change to a wire method would be invisible
# here.
# ============================================================================
if(DARKEDEN_BUILD_TESTS)
    add_library(TestPackets STATIC EXCLUDE_FROM_ALL ${PACKET_COMMON_SOURCES})
    target_include_directories(TestPackets PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/src/server
        ${CMAKE_SOURCE_DIR}/src/server/database
    )
    target_link_libraries(TestPackets PUBLIC de-kernel)
endif()

# SharedServerPackets — PACKET_COMMON compiled with __SHARED_SERVER__
add_library(SharedServerPackets STATIC ${PACKET_COMMON_SOURCES})
target_compile_definitions(SharedServerPackets PRIVATE __SHARED_SERVER__)
target_include_directories(SharedServerPackets PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src/server
    ${CMAKE_SOURCE_DIR}/src/server/database
    ${CMAKE_SOURCE_DIR}/src/server/sharedserver
)
target_link_libraries(SharedServerPackets PUBLIC de-kernel)
