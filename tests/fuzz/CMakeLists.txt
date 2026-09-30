# =====================================================================
# Packet-read fuzz targets: the bytes a client sends, through the gates
# the receive loops apply (GameFrameGate::next, which
# GamePlayer::processCommand runs, and LoginPlayer::processCommand), into
# the packets' read(). StreamFuzz.h describes the input format and the
# environment switches.
#
# Two builds of the same two targets:
#
#   fuzz_replay_game / fuzz_replay_login - the targets with a plain main()
#     (fuzz_replay_main.cpp) that replays saved inputs. Built by every
#     DARKEDEN_BUILD_TESTS tree, with the ordinary toolchain, and run by
#     ctest over the seed corpus and tests/fuzz/regressions/.
#
#   fuzz_game_stream / fuzz_login_stream - the same targets linked with
#     libFuzzer, under DARKEDEN_BUILD_FUZZERS (Clang only; the top-level
#     CMakeLists.txt instruments every target for it).
#
# Each target is compiled as its server (the PlayerStatus values and the
# factory table differ per server) and links that server's packet
# library, which carries the server's factory and validator selection on
# top of de-kernel. The two libraries define the same symbols, so a
# binary links exactly one of them.
# =====================================================================

set(DARKEDEN_FUZZ_TARGETS game login)
set(DARKEDEN_FUZZ_game_SOURCE fuzz_game_stream.cpp)
set(DARKEDEN_FUZZ_game_PACKETS GameServerPackets)
set(DARKEDEN_FUZZ_game_DEFINES __GAME_SERVER__ __COMBAT__)
set(DARKEDEN_FUZZ_game_SERVER gameserver)
# The production gate, which the game target runs beside its mirror.
set(DARKEDEN_FUZZ_game_EXTRA_SOURCES ${CMAKE_SOURCE_DIR}/src/server/gameserver/GameFrameGate.cpp)
set(DARKEDEN_FUZZ_game_EXTRA_INCLUDES ${CMAKE_SOURCE_DIR}/src/server/gameserver)
# GPS_NORMAL, where every packet a client sends is admitted (and, under
# DE_FUZZ_ANY_ID, every registered id); the target static_asserts the
# value.
set(DARKEDEN_FUZZ_game_STATUSES 3)
set(DARKEDEN_FUZZ_login_SOURCE fuzz_login_stream.cpp)
set(DARKEDEN_FUZZ_login_PACKETS LoginServerPackets)
set(DARKEDEN_FUZZ_login_DEFINES __LOGIN_SERVER__)
set(DARKEDEN_FUZZ_login_SERVER loginserver)
# LPS_BEGIN_SESSION and LPS_PC_MANAGEMENT, which between them admit every
# CL packet a client sends; the target static_asserts the values.
set(DARKEDEN_FUZZ_login_STATUSES 1,4)

function(darkeden_fuzz_configure target)
    target_include_directories(${target} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/tests/support
        ${CMAKE_SOURCE_DIR}/src/Core
    )
    set_target_properties(${target} PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
    )
endfunction()

set(DARKEDEN_FUZZ_SCRATCH ${CMAKE_CURRENT_BINARY_DIR}/scratch)
file(MAKE_DIRECTORY ${DARKEDEN_FUZZ_SCRATCH})

# The replay tests' environment. In a DARKEDEN_BUILD_FUZZERS tree every
# target carries ASan, whose leak check would fail the all-ids game replay
# on the reads that leak (GCUpdateInfo::read and five more, recorded in
# docs/FIXES.md; the client link refuses all six); the libFuzzer runs turn
# it off the same way.
set(DARKEDEN_FUZZ_REPLAY_ENV "")
if(DARKEDEN_BUILD_FUZZERS)
    list(APPEND DARKEDEN_FUZZ_REPLAY_ENV "ASAN_OPTIONS=detect_leaks=0")
endif()

foreach(name ${DARKEDEN_FUZZ_TARGETS})
    set(source ${DARKEDEN_FUZZ_${name}_SOURCE})
    set(packets ${DARKEDEN_FUZZ_${name}_PACKETS})
    set(defines ${DARKEDEN_FUZZ_${name}_DEFINES})
    set(extra_sources ${DARKEDEN_FUZZ_${name}_EXTRA_SOURCES})
    set(extra_includes ${DARKEDEN_FUZZ_${name}_EXTRA_INCLUDES})
    set(corpus ${CMAKE_CURRENT_BINARY_DIR}/corpus/${name})

    add_executable(fuzz_replay_${name} EXCLUDE_FROM_ALL ${source} ${extra_sources} fuzz_replay_main.cpp)
    darkeden_fuzz_configure(fuzz_replay_${name})
    target_include_directories(fuzz_replay_${name} PRIVATE ${extra_includes})
    target_compile_definitions(fuzz_replay_${name} PRIVATE ${defines})
    target_link_libraries(fuzz_replay_${name} PRIVATE ${packets} Threads::Threads)

    # The seed corpus is written into the build tree from the goldens by a
    # setup test, so a new golden is a new seed with nothing to regenerate
    # by hand.
    add_test(NAME fuzz_seed_corpus_${name}
        COMMAND perl ${CMAKE_SOURCE_DIR}/tools/fuzz/golden2corpus.pl
            ${CMAKE_SOURCE_DIR}/tests/golden
            ${CMAKE_SOURCE_DIR}/tests/wire-layout.txt
            ${CMAKE_SOURCE_DIR}/tests/ratchet/factory_registrations.txt
            ${DARKEDEN_FUZZ_${name}_SERVER} ${DARKEDEN_FUZZ_${name}_STATUSES} ${corpus})
    set_tests_properties(fuzz_seed_corpus_${name} PROPERTIES FIXTURES_SETUP fuzz_corpus_${name})

    # Every seed and every recorded crash input runs clean: a read may
    # refuse a body only with a ProtocolException, the one kind the receive
    # loops turn into a dropped connection (the targets abort on anything
    # else). The scratch working directory catches assertion_failed.log.
    set(inputs ${corpus})
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/regressions/${name})
        list(APPEND inputs ${CMAKE_CURRENT_SOURCE_DIR}/regressions/${name})
    endif()
    add_test(NAME fuzz_replay_${name}
        COMMAND fuzz_replay_${name} ${inputs}
        WORKING_DIRECTORY ${DARKEDEN_FUZZ_SCRATCH})
    set_tests_properties(fuzz_replay_${name} PROPERTIES
        FIXTURES_REQUIRED fuzz_corpus_${name}
        TIMEOUT 120)
    if(DARKEDEN_FUZZ_REPLAY_ENV)
        set_tests_properties(fuzz_replay_${name} PROPERTIES ENVIRONMENT "${DARKEDEN_FUZZ_REPLAY_ENV}")
    endif()

    if(TARGET wire_tests)
        # devbuild's test path builds wire_tests and then runs all of
        # ctest, so the replay binaries ride along like the other suites.
        add_dependencies(wire_tests fuzz_replay_${name})
    endif()

    if(DARKEDEN_BUILD_FUZZERS)
        add_executable(fuzz_${name}_stream ${source} ${extra_sources})
        darkeden_fuzz_configure(fuzz_${name}_stream)
        target_include_directories(fuzz_${name}_stream PRIVATE ${extra_includes})
        target_compile_definitions(fuzz_${name}_stream PRIVATE ${defines})
        target_link_libraries(fuzz_${name}_stream PRIVATE ${packets} Threads::Threads)
        target_link_options(fuzz_${name}_stream PRIVATE -fsanitize=fuzzer)
    endif()
endforeach()

# The reads behind the gameserver's client-link gate. The validator
# refuses every registered id a client does not send before its read(),
# so fuzz_replay_game proves that gate, and most of the game seeds and
# recorded game inputs (the GC, GS and SG ones) stop at it. This replays
# them with the gate as it was before it was narrowed: DE_FUZZ_ANY_ID
# admits every registered id in GPS_NORMAL and DE_FUZZ_NO_STORE_SKIP turns
# off the store-info refusal (GameFrameGate::next's), so every seed and every
# recorded input reaches its read() and the fixes they were recorded for
# stay replayed. The factory's GCMyStoreInfo and GCOtherStoreInfo hold no
# stall record, so their read() refuses the body with a ProtocolException
# before the record: GCMyStoreInfo right after its OpenUI byte,
# GCOtherStoreInfo after its object id and request flag. StoreInfo::read is
# not reachable from a client on this server, and its item-count bound is
# guarded by StoreInfoTest in wire_tests instead.
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/regressions/game)
    add_test(NAME fuzz_replay_game_any_id
        COMMAND fuzz_replay_game ${CMAKE_CURRENT_BINARY_DIR}/corpus/game
            ${CMAKE_CURRENT_SOURCE_DIR}/regressions/game
        WORKING_DIRECTORY ${DARKEDEN_FUZZ_SCRATCH})
    set(any_id_env DE_FUZZ_ANY_ID=1 DE_FUZZ_NO_STORE_SKIP=1 ${DARKEDEN_FUZZ_REPLAY_ENV})
    set_tests_properties(fuzz_replay_game_any_id PROPERTIES
        FIXTURES_REQUIRED fuzz_corpus_game
        ENVIRONMENT "${any_id_env}"
        TIMEOUT 120)
endif()

# The seed corpora for a libFuzzer run, written where the replay tests
# write them: `cmake --build <dir> --target fuzz_seed_corpus`.
add_custom_target(fuzz_seed_corpus
    COMMAND perl ${CMAKE_SOURCE_DIR}/tools/fuzz/golden2corpus.pl
        ${CMAKE_SOURCE_DIR}/tests/golden ${CMAKE_SOURCE_DIR}/tests/wire-layout.txt
        ${CMAKE_SOURCE_DIR}/tests/ratchet/factory_registrations.txt
        gameserver ${DARKEDEN_FUZZ_game_STATUSES} ${CMAKE_CURRENT_BINARY_DIR}/corpus/game
    COMMAND perl ${CMAKE_SOURCE_DIR}/tools/fuzz/golden2corpus.pl
        ${CMAKE_SOURCE_DIR}/tests/golden ${CMAKE_SOURCE_DIR}/tests/wire-layout.txt
        ${CMAKE_SOURCE_DIR}/tests/ratchet/factory_registrations.txt
        loginserver ${DARKEDEN_FUZZ_login_STATUSES} ${CMAKE_CURRENT_BINARY_DIR}/corpus/login
    VERBATIM)
