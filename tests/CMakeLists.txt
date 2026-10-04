# =====================================================================# Wire-contract test suite (docs/RESTRUCTURING.md Phase 1)
#
# Built only with -DDARKEDEN_BUILD_TESTS=ON (make test does this). Runs
# locally and in the pinned-Zig C++20 CI job.
# =====================================================================
include(FetchContent)

# Keep the test framework on a maintained release. The project was previously
# held at 1.12.1 solely by its C++11 language baseline.
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.18.0.zip
)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# googletest declares its targets in the default `all`, so a stale
# DARKEDEN_BUILD_TESTS=ON cache would drag them into a production build.
# FetchContent's own EXCLUDE_FROM_ALL option needs CMake 3.28; this project
# requires 3.16, so set the property directly.
foreach(gtest_target gtest gtest_main)
    if(TARGET ${gtest_target})
        set_target_properties(${gtest_target} PROPERTIES EXCLUDE_FROM_ALL TRUE)
    endif()
endforeach()

# EXCLUDE_FROM_ALL: even if a stale cache leaves DARKEDEN_BUILD_TESTS=ON,
# a plain `cmake --build build` must not drag the TestPackets library and
# googletest into the production build. `make test` names the target
# explicitly.
add_executable(wire_tests EXCLUDE_FROM_ALL
    support/TestStreams.cpp
    datagram_frame_test.cpp
    diagnostic_trace_test.cpp
    outcome_test.cpp
    packet_roundtrip_test.cpp
    packet_chat_test.cpp
    packet_combat_test.cpp
    packet_creature_test.cpp
    packet_encrypter_test.cpp
    packet_exchange_test.cpp
    packet_field_bounds_test.cpp
    packet_frame_test.cpp
    packet_gameserver_handshake_test.cpp
    packet_guild_test.cpp
    packet_interserver_test.cpp
    packet_inventory_test.cpp
    packet_login_test.cpp
    packet_meta_test.cpp
    packet_movement_test.cpp
    packet_party_test.cpp
    packet_quest_war_test.cpp
    packet_read_bounds_test.cpp
    packet_session_test.cpp
    packet_skill_test.cpp
    packet_store_test.cpp
    packet_trade_test.cpp
    packet_zone_scan_test.cpp
    slayer_weapon_shape_test.cpp
    wire_layout_test.cpp
    wire_string_test.cpp
    wire_types_test.cpp
    xml_parse_test.cpp
)

target_include_directories(wire_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/support
    ${CMAKE_CURRENT_SOURCE_DIR}/generated
    ${CMAKE_SOURCE_DIR}/src/Core
)

target_compile_definitions(wire_tests PRIVATE
    WIRETEST_GOLDEN_DIR="${CMAKE_CURRENT_SOURCE_DIR}/golden"
    WIRETEST_LAYOUT_FILE="${CMAKE_CURRENT_SOURCE_DIR}/wire-layout.txt"
    WIRETEST_REGISTRATIONS_FILE="${CMAKE_CURRENT_SOURCE_DIR}/ratchet/factory_registrations.txt"
    WIRETEST_XML_PARSE_FILE="${CMAKE_CURRENT_SOURCE_DIR}/golden/xml-parse.txt"
    WIRETEST_DATA_DIR="${CMAKE_SOURCE_DIR}/data"
)

# TestPackets (defined in src/Core/CMakeLists.txt) is the define-free
# factory/validator set; the wire classes themselves come from de-kernel,
# which it links — the same macro-free compile the deployed servers link
# (task 2.4). See the note there — this is not the client's own
# configuration (the client defines __GAME_CLIENT__=1).
# The dependency between these archives is one-way since the kernel flip:
# Core's DatagramFactoryRead.cpp calls into PacketFactoryManager
# (TestPackets); TestPackets needs nothing from Core. The double listing
# is kept as insurance against the cycle ever coming back; de-kernel
# follows transitively, last.
target_link_libraries(wire_tests PRIVATE
    TestPackets
    Core
    TestPackets
    Core
    gtest_main
    Threads::Threads
)

add_test(NAME wire_tests COMMAND wire_tests)

# Exercise the production composition roots and handlers without main(). Each
# runtime is compiled once with its server's definitions, and linked by both
# the deployed executable and its test. Separate processes are essential:
# their dispatch tables and several identically named classes are different.
function(add_server_runtime_test target source runtime)
    # Compile the test main directly: if a runtime acquires a main(), the
    # duplicate symbol must fail the link, never replace gtest_main from an
    # archive and accidentally run server startup under ctest.
    add_executable(${target} EXCLUDE_FROM_ALL ${source} support/server_runtime_main.cpp)
    target_link_libraries(${target} PRIVATE ${runtime} gtest)
    add_dependencies(wire_tests ${target})
    add_test(NAME ${target} COMMAND ${target}
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
    set_tests_properties(${target} PROPERTIES TIMEOUT 60)
endfunction()

add_server_runtime_test(game_server_runtime_tests game_server_runtime_test.cpp GameServerRuntime)
target_sources(game_server_runtime_tests PRIVATE
    game_connection_test.cpp incoming_game_connection_test.cpp game_connection_admission_test.cpp
    game_player_handoff_test.cpp game_player_transfer_test.cpp game_broadcast_queue_test.cpp
    game_broadcast_delivery_test.cpp support/AllocationProbe.cpp)
add_server_runtime_test(login_server_runtime_tests login_server_runtime_test.cpp LoginServerRuntime)
target_sources(login_server_runtime_tests PRIVATE
    login_connection_test.cpp login_world_list_test.cpp login_catalogue_test.cpp login_world_topology_test.cpp
    login_server_list_test.cpp login_routing_catalogue_test.cpp login_character_topology_test.cpp
    login_character_list_test.cpp
    login_character_list_refresh_test.cpp
    login_server_selection_test.cpp
    login_kick_preparation_test.cpp
    login_kick_cache_test.cpp
    login_kick_dispatch_test.cpp
    login_datagram_send_test.cpp
    login_kick_verification_test.cpp
    login_kick_retry_test.cpp
    login_completion_test.cpp
    login_account_session_test.cpp
    login_player_retirement_test.cpp
    login_incoming_reply_test.cpp
    login_incoming_request_test.cpp
    login_character_selection_test.cpp
    login_character_creation_test.cpp
    login_character_deletion_test.cpp
    login_character_name_query_test.cpp
    login_account_name_query_test.cpp
    login_registration_test.cpp
    login_authentication_test.cpp
    support/AllocationProbe.cpp)
add_server_runtime_test(shared_server_runtime_tests shared_server_runtime_test.cpp SharedServerRuntime)
target_sources(shared_server_runtime_tests PRIVATE
    player_socket_replacement_test.cpp shared_connection_test.cpp shared_server_construction_test.cpp
    shared_catalogue_test.cpp shared_startup_data_test.cpp shared_guild_loading_test.cpp shared_guild_startup_test.cpp
    shared_guild_reply_test.cpp support/AllocationProbe.cpp)

# Common production world loading, without any server runtime or process startup.
add_executable(world_catalogue_tests EXCLUDE_FROM_ALL
    world_catalogue_test.cpp support/AllocationProbe.cpp support/server_runtime_main.cpp)
target_link_libraries(world_catalogue_tests PRIVATE ServerCore Database ${MYSQL_LIBRARY} gtest)
add_dependencies(wire_tests world_catalogue_tests)
add_test(NAME world_catalogue_tests COMMAND world_catalogue_tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
set_tests_properties(world_catalogue_tests PROPERTIES TIMEOUT 30)

# Common game-server rows and flags, linked independently of executable startup.
add_executable(server_catalogue_tests EXCLUDE_FROM_ALL
    server_catalogue_test.cpp support/AllocationProbe.cpp support/server_runtime_main.cpp)
target_link_libraries(server_catalogue_tests PRIVATE ServerCore Database ${MYSQL_LIBRARY} gtest)
add_dependencies(wire_tests server_catalogue_tests)
add_test(NAME server_catalogue_tests COMMAND server_catalogue_tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
set_tests_properties(server_catalogue_tests PROPERTIES TIMEOUT 30)

add_executable(server_startup_tests EXCLUDE_FROM_ALL server_startup_test.cpp)
target_link_libraries(server_startup_tests PRIVATE ServerStartup gtest_main)
add_dependencies(wire_tests server_startup_tests)
add_test(NAME server_startup_tests COMMAND server_startup_tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
set_tests_properties(server_startup_tests PROPERTIES TIMEOUT 30)

add_executable(server_lifecycle_tests EXCLUDE_FROM_ALL server_lifecycle_test.cpp)
target_link_libraries(server_lifecycle_tests PRIVATE ServerLifecycle gtest_main)
add_dependencies(wire_tests server_lifecycle_tests)
add_test(NAME server_lifecycle_tests COMMAND server_lifecycle_tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
set_tests_properties(server_lifecycle_tests PROPERTIES TIMEOUT 30)

add_executable(server_process_shutdown_tests EXCLUDE_FROM_ALL server_process_shutdown_test.cpp)
target_link_libraries(server_process_shutdown_tests PRIVATE ServerProcessShutdown ServerLifecycle gtest_main)
add_dependencies(wire_tests server_process_shutdown_tests)
add_test(NAME server_process_shutdown_tests COMMAND server_process_shutdown_tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
set_tests_properties(server_process_shutdown_tests PROPERTIES TIMEOUT 30)

add_executable(server_fatal_handler_tests EXCLUDE_FROM_ALL server_fatal_handler_test.cpp)
target_link_libraries(server_fatal_handler_tests PRIVATE ServerFatalHandlers gtest_main Threads::Threads)
add_dependencies(wire_tests server_fatal_handler_tests)
add_test(NAME server_fatal_handler_tests COMMAND server_fatal_handler_tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
set_tests_properties(server_fatal_handler_tests PROPERTIES TIMEOUT 30)

add_executable(server_application_tests EXCLUDE_FROM_ALL server_application_test.cpp)
target_link_libraries(server_application_tests PRIVATE ServerApplication gtest_main)
add_dependencies(wire_tests server_application_tests)
add_test(NAME server_application_tests COMMAND server_application_tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
set_tests_properties(server_application_tests PROPERTIES TIMEOUT 30)

add_executable(server_worker_shutdown_tests EXCLUDE_FROM_ALL server_worker_shutdown_test.cpp)
target_link_libraries(server_worker_shutdown_tests PRIVATE ServerWorkerShutdown ServerLifecycle gtest_main)
add_dependencies(wire_tests server_worker_shutdown_tests)
add_test(NAME server_worker_shutdown_tests COMMAND server_worker_shutdown_tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
set_tests_properties(server_worker_shutdown_tests PROPERTIES TIMEOUT 30)

add_executable(server_start_sequence_tests EXCLUDE_FROM_ALL server_start_sequence_test.cpp)
target_link_libraries(server_start_sequence_tests PRIVATE
    ServerStartSequence ServerWorkerShutdown ServerLifecycle gtest_main)
add_dependencies(wire_tests server_start_sequence_tests)
add_test(NAME server_start_sequence_tests COMMAND server_start_sequence_tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
set_tests_properties(server_start_sequence_tests PROPERTIES TIMEOUT 30)

add_executable(properties_parser_tests EXCLUDE_FROM_ALL properties_parser_test.cpp)
target_link_libraries(properties_parser_tests PRIVATE de-kernel gtest_main)
add_dependencies(wire_tests properties_parser_tests)
add_test(NAME properties_parser_tests COMMAND properties_parser_tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
set_tests_properties(properties_parser_tests PROPERTIES TIMEOUT 30)

add_executable(server_process_environment_tests EXCLUDE_FROM_ALL server_process_environment_test.cpp)
target_link_libraries(server_process_environment_tests PRIVATE ServerProcessEnvironment ServerProcessShutdown gtest_main)
add_dependencies(wire_tests server_process_environment_tests)
add_test(NAME server_process_environment_tests COMMAND server_process_environment_tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
set_tests_properties(server_process_environment_tests PROPERTIES TIMEOUT 30)

add_executable(listener_startup_tests EXCLUDE_FROM_ALL listener_startup_test.cpp)
target_link_libraries(listener_startup_tests PRIVATE ListenerStartup gtest_main)
add_dependencies(wire_tests listener_startup_tests)
add_test(NAME listener_startup_tests COMMAND listener_startup_tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
set_tests_properties(listener_startup_tests PROPERTIES TIMEOUT 30)

add_executable(server_port_settings_tests EXCLUDE_FROM_ALL server_port_settings_test.cpp)
target_link_libraries(server_port_settings_tests PRIVATE ServerPortSettings gtest_main)
add_dependencies(wire_tests server_port_settings_tests)
add_test(NAME server_port_settings_tests COMMAND server_port_settings_tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

add_executable(socket_construction_tests EXCLUDE_FROM_ALL
    socket_construction_test.cpp support/AllocationProbe.cpp)
target_link_libraries(socket_construction_tests PRIVATE de-kernel gtest_main)
add_dependencies(wire_tests socket_construction_tests)
add_test(NAME socket_construction_tests COMMAND socket_construction_tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
set_tests_properties(socket_construction_tests PROPERTIES TIMEOUT 30)

add_executable(socket_stream_setup_tests EXCLUDE_FROM_ALL
    socket_stream_setup_test.cpp player_socket_replacement_test.cpp support/AllocationProbe.cpp)
target_link_libraries(socket_stream_setup_tests PRIVATE de-kernel gtest_main)
add_dependencies(wire_tests socket_stream_setup_tests)
add_test(NAME socket_stream_setup_tests COMMAND socket_stream_setup_tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
set_tests_properties(socket_stream_setup_tests PROPERTIES TIMEOUT 30)

add_executable(player_connection_key_tests EXCLUDE_FROM_ALL
    player_connection_key_test.cpp support/AllocationProbe.cpp)
target_link_libraries(player_connection_key_tests PRIVATE de-kernel gtest_main)
add_dependencies(wire_tests player_connection_key_tests)
add_test(NAME player_connection_key_tests COMMAND player_connection_key_tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
set_tests_properties(player_connection_key_tests PROPERTIES TIMEOUT 30)

add_executable(outbound_server_connection_tests EXCLUDE_FROM_ALL
    outbound_server_connection_test.cpp support/AllocationProbe.cpp)
target_link_libraries(outbound_server_connection_tests PRIVATE OutboundServerConnection gtest_main)
add_dependencies(wire_tests outbound_server_connection_tests)
add_test(NAME outbound_server_connection_tests COMMAND outbound_server_connection_tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
set_tests_properties(outbound_server_connection_tests PROPERTIES TIMEOUT 30)

add_executable(accepted_server_connection_tests EXCLUDE_FROM_ALL accepted_server_connection_test.cpp)
target_link_libraries(accepted_server_connection_tests PRIVATE AcceptedServerConnection gtest_main)
add_dependencies(wire_tests accepted_server_connection_tests)
add_test(NAME accepted_server_connection_tests COMMAND accepted_server_connection_tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
set_tests_properties(accepted_server_connection_tests PROPERTIES TIMEOUT 30)

# Verify that every real main returns failure for invalid startup input,
# instead of aborting, falling through into managers, or starting workers.
add_dependencies(wire_tests gameserver loginserver sharedserver)
add_test(NAME server_startup_cli COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/server_startup_cli_test.sh
    $<TARGET_FILE:gameserver> $<TARGET_FILE:loginserver> $<TARGET_FILE:sharedserver>)
set_tests_properties(server_startup_cli PROPERTIES TIMEOUT 30)

add_executable(proxy_acceptor_tests EXCLUDE_FROM_ALL
    proxy_acceptor_test.cpp
    socket_close_test.cpp
    ${CMAKE_SOURCE_DIR}/src/server/ProxyAcceptor.cpp)
target_include_directories(proxy_acceptor_tests PRIVATE ${CMAKE_SOURCE_DIR}/src/server)
target_link_libraries(proxy_acceptor_tests PRIVATE de-kernel gtest_main Threads::Threads)
add_dependencies(wire_tests proxy_acceptor_tests)
add_test(NAME proxy_acceptor_tests COMMAND proxy_acceptor_tests)
set_tests_properties(proxy_acceptor_tests PROPERTIES TIMEOUT 30)
# A hang here would otherwise sit for ctest's 1500s default with no output.
set_tests_properties(wire_tests PROPERTIES TIMEOUT 120)

# Repository/domain unit tests (docs/RESTRUCTURING.md 3.2): pure rules and
# fake-backed repository contracts. Header-only against the sources — no
# game or database libraries linked, which is the point of the seam.
add_executable(repository_tests EXCLUDE_FROM_ALL
    repository_test.cpp
)
target_include_directories(repository_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/support
    ${CMAKE_SOURCE_DIR}/src/Core
    ${CMAKE_SOURCE_DIR}/src/server/gameserver
)
target_link_libraries(repository_tests PRIVATE
    gtest_main
    Threads::Threads
)
# devbuild's test path builds the wire_tests target then runs all of ctest;
# hang this off it so the suite is always built.
add_dependencies(wire_tests repository_tests)

add_test(NAME repository_tests COMMAND repository_tests)
set_tests_properties(repository_tests PROPERTIES TIMEOUT 60)

# MySQL-backed integration tier (docs/RESTRUCTURING.md 3.2): the REAL
# repository implementations against a throwaway MySQL 5.7 with the
# initdb/ schema — the authority the fakes above are corrected against.
# Deliberately NOT registered with ctest: it needs the database that
# tests/integration/mysql_test.sh provides (a handrolled container
# fixture); run `make integration-test` from the host. Compiles the MySQL
# impl sources directly and links the macro-free Database library — the
# repositories carry no server-type macros, so the objects match what the
# gameserver builds. The Exchange purchase's write path (ExchangeDecision
# and ExchangePurchase) is compiled in too, to run it against real
# transactions.
add_executable(mysql_repository_tests EXCLUDE_FROM_ALL
    integration/mysql_repository_test.cpp
    integration/mysql_login_purge_repository_test.cpp
    integration/mysql_loginserver_repository_test.cpp
    integration/mysql_servercore_repository_test.cpp
    integration/mysql_sharedserver_repository_test.cpp
    ${CMAKE_SOURCE_DIR}/src/server/gameserver/repository/MySQLBalanceInfoRepository.cpp
    ${CMAKE_SOURCE_DIR}/src/server/gameserver/repository/MySQLBloodBibleSignRepository.cpp
    ${CMAKE_SOURCE_DIR}/src/server/gameserver/repository/MySQLBulletinBoardRepository.cpp
    ${CMAKE_SOURCE_DIR}/src/server/gameserver/repository/MySQLComebackEventRepository.cpp
    ${CMAKE_SOURCE_DIR}/src/server/gameserver/repository/MySQLContentInfoRepository.cpp
    ${CMAKE_SOURCE_DIR}/src/server/gameserver/exchange/ExchangeDecision.cpp
    ${CMAKE_SOURCE_DIR}/src/server/gameserver/exchange/ExchangePurchase.cpp
    ${CMAKE_SOURCE_DIR}/src/server/gameserver/repository/MySQLExchangeRepository.cpp
    ${CMAKE_SOURCE_DIR}/src/server/gameserver/repository/MySQLMessageRepository.cpp
    ${CMAKE_SOURCE_DIR}/src/server/gameserver/repository/MySQLRegenZoneRepository.cpp
    ${CMAKE_SOURCE_DIR}/src/server/gameserver/repository/MySQLZoneInfoRepository.cpp
    ${CMAKE_SOURCE_DIR}/src/server/gameserver/repository/MySQLCharacterRepository.cpp
    ${CMAKE_SOURCE_DIR}/src/server/gameserver/repository/MySQLCharacterPurgeRepository.cpp
    ${CMAKE_SOURCE_DIR}/src/server/gameserver/repository/MySQLGoldRepository.cpp
    ${CMAKE_SOURCE_DIR}/src/server/gameserver/repository/MySQLGoodsRepository.cpp
    ${CMAKE_SOURCE_DIR}/src/server/gameserver/repository/MySQLCoupleRepository.cpp
    ${CMAKE_SOURCE_DIR}/src/server/gameserver/repository/MySQLFlagWarRepository.cpp
    ${CMAKE_SOURCE_DIR}/src/server/gameserver/repository/MySQLMofusPointRepository.cpp
    ${CMAKE_SOURCE_DIR}/src/server/gameserver/repository/MySQLFriendRepository.cpp
    ${CMAKE_SOURCE_DIR}/src/server/gameserver/repository/MySQLSystemAvailabilityRepository.cpp
    ${CMAKE_SOURCE_DIR}/src/server/gameserver/repository/MySQLGuildRepository.cpp
    ${CMAKE_SOURCE_DIR}/src/server/gameserver/repository/MySQLItemObjectRepository.cpp
    ${CMAKE_SOURCE_DIR}/src/server/gameserver/repository/MySQLItemRepository.cpp
    ${CMAKE_SOURCE_DIR}/src/server/gameserver/repository/MySQLNicknameRepository.cpp
    ${CMAKE_SOURCE_DIR}/src/server/gameserver/repository/MySQLPlayRecordRepository.cpp
    ${CMAKE_SOURCE_DIR}/src/server/gameserver/repository/MySQLEffectSaveRepository.cpp
    ${CMAKE_SOURCE_DIR}/src/server/gameserver/repository/MySQLFlagSetRepository.cpp
    ${CMAKE_SOURCE_DIR}/src/server/gameserver/repository/MySQLGameInfoRepository.cpp
    ${CMAKE_SOURCE_DIR}/src/server/gameserver/repository/MySQLQuestInfoRepository.cpp
    ${CMAKE_SOURCE_DIR}/src/server/gameserver/repository/MySQLQuestItemRepository.cpp
    ${CMAKE_SOURCE_DIR}/src/server/gameserver/repository/MySQLRankBonusRepository.cpp
    ${CMAKE_SOURCE_DIR}/src/server/gameserver/repository/MySQLSMSAddressRepository.cpp
    ${CMAKE_SOURCE_DIR}/src/server/gameserver/repository/MySQLSpecialEventRepository.cpp
    ${CMAKE_SOURCE_DIR}/src/server/gameserver/repository/MySQLSessionRepository.cpp
    ${CMAKE_SOURCE_DIR}/src/server/gameserver/repository/MySQLSkillSaveRepository.cpp
    ${CMAKE_SOURCE_DIR}/src/server/gameserver/repository/MySQLStashRepository.cpp
    ${CMAKE_SOURCE_DIR}/src/server/gameserver/repository/MySQLWarInfoRepository.cpp
    ${CMAKE_SOURCE_DIR}/src/server/loginserver/repository/MySQLLoginAccountRepository.cpp
    ${CMAKE_SOURCE_DIR}/src/server/loginserver/repository/MySQLLoginCharacterPurgeRepository.cpp
    ${CMAKE_SOURCE_DIR}/src/server/loginserver/repository/MySQLLoginCharacterRepository.cpp
    ${CMAKE_SOURCE_DIR}/src/server/loginserver/repository/MySQLLoginConfigRepository.cpp
    ${CMAKE_SOURCE_DIR}/src/server/sharedserver/repository/MySQLSharedConfigRepository.cpp
    ${CMAKE_SOURCE_DIR}/src/server/sharedserver/repository/MySQLSharedGuildRepository.cpp
)
target_include_directories(mysql_repository_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/integration
    ${CMAKE_SOURCE_DIR}/src/Core
    ${CMAKE_SOURCE_DIR}/src/server
    ${CMAKE_SOURCE_DIR}/src/server/gameserver
    ${CMAKE_SOURCE_DIR}/src/server/gameserver/exchange
    ${CMAKE_SOURCE_DIR}/src/server/loginserver
    ${CMAKE_SOURCE_DIR}/src/server/sharedserver
)
target_link_libraries(mysql_repository_tests PRIVATE
    Database
    ServerCore
    de-kernel
    ${MYSQL_LIBRARY}
    gtest
    Threads::Threads
)

# Pure-formula unit tests (docs/RESTRUCTURING.md 3.3). Linking ONLY de-core
# and gtest is deliberate: it proves de-core stays freestanding — pulling
# Core, transport, or a gameserver header into it breaks this link.
add_executable(formula_tests EXCLUDE_FROM_ALL
    formula_test.cpp
)
target_link_libraries(formula_tests PRIVATE
    de-core
    gtest_main
    Threads::Threads
)
# The shared parity vectors formula_test.cpp reads (and, under
# UPDATE_GOLDENS=1, rewrites). They live beside the sources they pin because
# the client repository vendors them together.
set_source_files_properties(formula_test.cpp PROPERTIES
    COMPILE_DEFINITIONS "DECORE_VECTOR_DIR=\"${CMAKE_SOURCE_DIR}/src/domain/vectors\""
)
# `make dev-test` builds the wire_tests target; ride along so ctest always
# has a fresh binary (same pattern as repository_tests above).
add_dependencies(wire_tests formula_tests)
# The strict compile of the client-vendored de-core sources
# (src/domain/CMakeLists.txt) rides along the same way: a warning there
# fails the suite build.
add_dependencies(wire_tests de-core-strict)

add_test(NAME formula_tests COMMAND formula_tests)
set_tests_properties(formula_tests PROPERTIES TIMEOUT 60)

# ConcreteItem's grade and durability wiring (docs/RESTRUCTURING.md 3.6):
# every item class built on ConcreteItem, probed through the exact
# ConcreteItem base its header declares, against de-core's per-class table.
# This links the adapter (ConcreteItem.cpp), de-core, the game context's
# translation unit and de-kernel (for Assert), and none of the item sources:
# the members of Item, ItemInfo, EffectManager and ItemInfoManager that the
# probes reference are stubbed by the test, as gm_console_command_tests
# stubs the command bodies. A failing Assert appends to assertion_failed.log
# in the working directory, hence the build-tree WORKING_DIRECTORY.
add_executable(concrete_item_tests EXCLUDE_FROM_ALL
    concrete_item_test.cpp
    ${CMAKE_SOURCE_DIR}/src/server/gameserver/ConcreteItem.cpp
    ${CMAKE_SOURCE_DIR}/src/server/gameserver/GameContext.cpp
)
target_include_directories(concrete_item_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src/server/gameserver
    ${CMAKE_SOURCE_DIR}/src/server/gameserver/item
)
# The headers the test checks its class list against.
target_compile_definitions(concrete_item_tests PRIVATE
    CONCRETE_ITEM_HEADER_DIR="${CMAKE_SOURCE_DIR}/src/server/gameserver/item"
)
target_link_libraries(concrete_item_tests PRIVATE de-core de-kernel gtest_main Threads::Threads)
add_dependencies(wire_tests concrete_item_tests)
add_test(NAME concrete_item_tests COMMAND concrete_item_tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
set_tests_properties(concrete_item_tests PROPERTIES TIMEOUT 60)

# C++20 structured-concurrency contract. This stays isolated from the game
# libraries so lifecycle failures cannot be hidden by unrelated fixtures.
add_executable(cooperative_thread_tests EXCLUDE_FROM_ALL
    cooperative_thread_test.cpp
)
target_include_directories(cooperative_thread_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src/server
)
target_link_libraries(cooperative_thread_tests PRIVATE
    gtest_main
    Threads::Threads
)
add_dependencies(wire_tests cooperative_thread_tests)

add_test(NAME cooperative_thread_tests COMMAND cooperative_thread_tests)
set_tests_properties(cooperative_thread_tests PROPERTIES TIMEOUT 10)

add_executable(managed_thread_tests EXCLUDE_FROM_ALL
    managed_thread_test.cpp
    managed_thread_lifecycle_test.cpp
    worker_pool_test.cpp
    support/AllocationProbe.cpp
)
target_link_libraries(managed_thread_tests PRIVATE ServerWorkers gtest_main)
add_dependencies(wire_tests managed_thread_tests)
add_test(NAME managed_thread_tests COMMAND managed_thread_tests)
# Headroom for the stop-aware-wait tests: a regression there sits out a
# five-second polling interval per case instead of waking immediately.
set_tests_properties(managed_thread_tests PROPERTIES TIMEOUT 30)

add_executable(server_synchronization_tests EXCLUDE_FROM_ALL server_synchronization_test.cpp)
target_link_libraries(server_synchronization_tests PRIVATE ServerSynchronization gtest_main)
add_dependencies(wire_tests server_synchronization_tests)
add_test(NAME server_synchronization_tests COMMAND server_synchronization_tests)
set_tests_properties(server_synchronization_tests PROPERTIES TIMEOUT 20)

# War-system Schedule/Scheduler and the VSDateTime clock they run on —
# ported from the deleted gameserver/test and gameserver/testAlone cppunit
# suites. Links only de-kernel plus the two war/ sources under test; the
# heartbeat appends to Schedule.txt via filelog(), so run from the build
# tree rather than the source tree.
add_executable(scheduler_tests EXCLUDE_FROM_ALL
    scheduler_test.cpp
    ${CMAKE_SOURCE_DIR}/src/server/gameserver/war/Schedule.cpp
    ${CMAKE_SOURCE_DIR}/src/server/gameserver/war/Scheduler.cpp
)
target_include_directories(scheduler_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src/server/gameserver
)
target_link_libraries(scheduler_tests PRIVATE de-kernel gtest_main Threads::Threads)
add_dependencies(wire_tests scheduler_tests)
add_test(NAME scheduler_tests COMMAND scheduler_tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
set_tests_properties(scheduler_tests PROPERTIES TIMEOUT 30)

# A castle's tax balance changes (src/server/gameserver/CastleTaxBalance.h):
# header-only compare-and-swap over the kernel's types; de-kernel is linked
# for the include paths and what Types.h pulls in.
add_executable(castle_tax_balance_tests EXCLUDE_FROM_ALL
    castle_tax_balance_test.cpp
)
target_include_directories(castle_tax_balance_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src/server/gameserver
)
target_link_libraries(castle_tax_balance_tests PRIVATE de-kernel gtest_main Threads::Threads)
add_dependencies(wire_tests castle_tax_balance_tests)
add_test(NAME castle_tax_balance_tests COMMAND castle_tax_balance_tests)
set_tests_properties(castle_tax_balance_tests PROPERTIES TIMEOUT 30)

# RAII critical sections (src/Core/Exception.h). Exception.h is a kernel
# header, so the test includes it directly; de-kernel is linked only for
# Throwable's out-of-line dependencies (StringStream).
add_executable(critical_section_tests EXCLUDE_FROM_ALL
    critical_section_test.cpp
)
target_include_directories(critical_section_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src/Core
)
target_link_libraries(critical_section_tests PRIVATE de-kernel gtest_main Threads::Threads)
add_dependencies(wire_tests critical_section_tests)
add_test(NAME critical_section_tests COMMAND critical_section_tests)
set_tests_properties(critical_section_tests PROPERTIES TIMEOUT 30)

# Cross-thread mailbox (src/server/Mailbox.h): header-only, no dependencies
# beyond the standard library, so nothing but gtest is linked.
add_executable(mailbox_tests EXCLUDE_FROM_ALL
    mailbox_test.cpp
)
target_include_directories(mailbox_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src/server
)
target_link_libraries(mailbox_tests PRIVATE gtest_main Threads::Threads)
add_dependencies(wire_tests mailbox_tests)
add_test(NAME mailbox_tests COMMAND mailbox_tests)
set_tests_properties(mailbox_tests PROPERTIES TIMEOUT 30)

# Copy-on-write snapshot (src/server/Snapshot.h): header-only, standard
# library only.
add_executable(snapshot_tests EXCLUDE_FROM_ALL
    snapshot_test.cpp
)
target_include_directories(snapshot_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src/server
)
target_link_libraries(snapshot_tests PRIVATE gtest_main Threads::Threads)
add_dependencies(wire_tests snapshot_tests)
add_test(NAME snapshot_tests COMMAND snapshot_tests)
set_tests_properties(snapshot_tests PROPERTIES TIMEOUT 60)

# The manager registry a converted subsystem takes instead of reading the
# g_p* globals (src/server/gameserver/GameContext.h). Its header only
# forward-declares the managers, so this links the context's own
# translation unit and de-kernel (for Assert) and nothing of the
# gameserver -- which is the property that makes a subsystem holding a
# GameContext& testable. A failing Assert appends to
# assertion_failed.log in the working directory, hence the build-tree
# WORKING_DIRECTORY (same reason as scheduler_tests).
add_executable(game_context_tests EXCLUDE_FROM_ALL
    game_context_test.cpp
    ${CMAKE_SOURCE_DIR}/src/server/gameserver/GameContext.cpp
)
target_include_directories(game_context_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src/Core
    ${CMAKE_SOURCE_DIR}/src/server/gameserver
)
target_link_libraries(game_context_tests PRIVATE de-kernel gtest_main Threads::Threads)
add_dependencies(wire_tests game_context_tests)
add_test(NAME game_context_tests COMMAND game_context_tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
set_tests_properties(game_context_tests PROPERTIES TIMEOUT 30)

# The loginserver's own context registry (docs/RESTRUCTURING.md task 3.5),
# built over stand-in pointers: this links the context's translation unit
# and de-kernel (for Assert) and nothing of the loginserver -- which is the
# property that makes a subsystem holding a LoginContext& testable. A
# failing Assert appends to assertion_failed.log in the working directory,
# hence the build-tree WORKING_DIRECTORY (same reason as scheduler_tests).
add_executable(login_context_tests EXCLUDE_FROM_ALL
    login_context_test.cpp
    ${CMAKE_SOURCE_DIR}/src/server/loginserver/LoginContext.cpp
)
target_include_directories(login_context_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src/Core
    ${CMAKE_SOURCE_DIR}/src/server/loginserver
)
target_link_libraries(login_context_tests PRIVATE de-kernel gtest_main Threads::Threads)
add_dependencies(wire_tests login_context_tests)
add_test(NAME login_context_tests COMMAND login_context_tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
set_tests_properties(login_context_tests PROPERTIES TIMEOUT 30)

# The sharedserver's own context registry (docs/RESTRUCTURING.md task 3.5),
# built the same way: the context's translation unit, de-kernel for Assert,
# and nothing of the sharedserver. A failing Assert appends to
# assertion_failed.log in the working directory, hence the build-tree
# WORKING_DIRECTORY (same reason as scheduler_tests).
add_executable(shared_context_tests EXCLUDE_FROM_ALL
    shared_context_test.cpp
    ${CMAKE_SOURCE_DIR}/src/server/sharedserver/SharedContext.cpp
)
target_include_directories(shared_context_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src/Core
    ${CMAKE_SOURCE_DIR}/src/server/sharedserver
)
target_link_libraries(shared_context_tests PRIVATE de-kernel gtest_main Threads::Threads)
add_dependencies(wire_tests shared_context_tests)
add_test(NAME shared_context_tests COMMAND shared_context_tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
set_tests_properties(shared_context_tests PROPERTIES TIMEOUT 30)

# The registry for the managers whose classes live in src/Core
# (docs/RESTRUCTURING.md task 3.5). Its translation unit is a kernel file,
# so de-kernel already carries it and this names only the test source; the
# managers themselves stay undefined, which is what lets the registry be
# kernel while two of the three compile per server. A failing Assert appends
# to assertion_failed.log in the working directory, hence the build-tree
# WORKING_DIRECTORY (same reason as scheduler_tests).
add_executable(kernel_context_tests EXCLUDE_FROM_ALL
    kernel_context_test.cpp
)
target_include_directories(kernel_context_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src/Core
)
target_link_libraries(kernel_context_tests PRIVATE de-kernel gtest_main Threads::Threads)
add_dependencies(wire_tests kernel_context_tests)
add_test(NAME kernel_context_tests COMMAND kernel_context_tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
set_tests_properties(kernel_context_tests PROPERTIES TIMEOUT 30)

# The registry for the managers ServerCore defines (docs/RESTRUCTURING.md
# task 3.5), built over stand-in pointers: this links the context's
# translation unit and de-kernel (for Assert) and nothing of ServerCore --
# so the suite needs neither MySQL nor a server. A failing Assert appends to
# assertion_failed.log in the working directory, hence the build-tree
# WORKING_DIRECTORY (same reason as scheduler_tests).
add_executable(server_context_tests EXCLUDE_FROM_ALL
    server_context_test.cpp
    ${CMAKE_SOURCE_DIR}/src/server/ServerContext.cpp
)
target_include_directories(server_context_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src/Core
    ${CMAKE_SOURCE_DIR}/src/server
)
target_link_libraries(server_context_tests PRIVATE de-kernel gtest_main Threads::Threads)
add_dependencies(wire_tests server_context_tests)
add_test(NAME server_context_tests COMMAND server_context_tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
set_tests_properties(server_context_tests PROPERTIES TIMEOUT 30)

# The configuration block one database connection is built from. The reader
# touches Properties and nothing else, so this links its translation unit and
# de-kernel -- no MySQL, no DatabaseManager and no live server.
add_executable(connection_settings_tests EXCLUDE_FROM_ALL
    connection_settings_test.cpp
    ${CMAKE_SOURCE_DIR}/src/server/database/ConnectionSettings.cpp
)
target_include_directories(connection_settings_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src/Core
    ${CMAKE_SOURCE_DIR}/src/server/database
)
target_link_libraries(connection_settings_tests PRIVATE de-kernel gtest_main Threads::Threads)
add_dependencies(wire_tests connection_settings_tests)
add_test(NAME connection_settings_tests COMMAND connection_settings_tests)
set_tests_properties(connection_settings_tests PROPERTIES TIMEOUT 30)

# The bound the connection managers walk their descriptor-indexed player
# tables with. DescriptorTable.h is header-only and mentions no socket, no
# player and no server, so this links nothing but gtest.
add_executable(descriptor_table_tests EXCLUDE_FROM_ALL descriptor_table_test.cpp)
target_include_directories(descriptor_table_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src/server
)
target_link_libraries(descriptor_table_tests PRIVATE gtest_main Threads::Threads)
add_dependencies(wire_tests descriptor_table_tests)
add_test(NAME descriptor_table_tests COMMAND descriptor_table_tests)
set_tests_properties(descriptor_table_tests PROPERTIES TIMEOUT 30)

# The readiness set the connection managers multiplex those tables with.
# DescriptorPollSet.h is header-only and needs nothing but poll(), so this
# links nothing but gtest.
add_executable(descriptor_poll_set_tests EXCLUDE_FROM_ALL descriptor_poll_set_test.cpp)
target_include_directories(descriptor_poll_set_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src/server
)
target_link_libraries(descriptor_poll_set_tests PRIVATE gtest_main Threads::Threads)
add_dependencies(wire_tests descriptor_poll_set_tests)
add_test(NAME descriptor_poll_set_tests COMMAND descriptor_poll_set_tests)
set_tests_properties(descriptor_poll_set_tests PROPERTIES TIMEOUT 30)

# When a worker next runs its database keep-alive query. KeepAlive.h is
# header-only arithmetic on a timeval, so this links nothing but gtest.
add_executable(keep_alive_tests EXCLUDE_FROM_ALL keep_alive_test.cpp)
target_include_directories(keep_alive_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src/server
)
target_link_libraries(keep_alive_tests PRIVATE gtest_main Threads::Threads)
add_dependencies(wire_tests keep_alive_tests)
add_test(NAME keep_alive_tests COMMAND keep_alive_tests)
set_tests_properties(keep_alive_tests PROPERTIES TIMEOUT 30)

add_executable(shutdown_probe EXCLUDE_FROM_ALL support/shutdown_probe.cpp)
target_include_directories(shutdown_probe PRIVATE ${CMAKE_SOURCE_DIR}/src/server)
target_link_libraries(shutdown_probe PRIVATE Threads::Threads)
add_dependencies(wire_tests shutdown_probe)
add_test(NAME shutdown_supervisor COMMAND bash
    ${CMAKE_CURRENT_SOURCE_DIR}/shutdown_supervisor_test.sh
    $<TARGET_FILE:shutdown_probe> ${CMAKE_SOURCE_DIR}/docker/start.sh)
# Two scenarios, each paying start.sh's 6s of staged startup, and the second
# also waiting out the supervisor's 8s bound on a server that ignores SIGTERM.
set_tests_properties(shutdown_supervisor PROPERTIES TIMEOUT 90)

# Call-site diagnostics (docs/TOOLCHAIN.md, "Diagnostics without location
# macros"): Assert/ProtocolAssert and the __END_CATCH stack trace, now carried
# by std::source_location instead of __FILE__/__LINE__/__PRETTY_FUNCTION__.
# Assert.h and Exception.h are de-kernel sources, so this links nothing else.
# A failing assert appends to assertion_failed.log in the working directory,
# hence the build-tree WORKING_DIRECTORY (same reason as scheduler_tests).
add_executable(diagnostics_tests EXCLUDE_FROM_ALL
    diagnostics_test.cpp
)
target_include_directories(diagnostics_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src/Core
)
target_link_libraries(diagnostics_tests PRIVATE de-kernel gtest_main Threads::Threads)
add_dependencies(wire_tests diagnostics_tests)
add_test(NAME diagnostics_tests COMMAND diagnostics_tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
set_tests_properties(diagnostics_tests PROPERTIES TIMEOUT 30)

add_executable(database_timeout_tests EXCLUDE_FROM_ALL database_timeout_test.cpp)
target_link_libraries(database_timeout_tests PRIVATE Database ServerCore de-kernel ${MYSQL_LIBRARY} gtest_main Threads::Threads)
add_dependencies(wire_tests database_timeout_tests)
add_test(NAME database_timeout_tests COMMAND database_timeout_tests)
set_tests_properties(database_timeout_tests PROPERTIES TIMEOUT 20)

# The database manager's per-thread connection tables, registered and looked
# up from many threads at once. Unconnected Connection handles stand in for
# real ones, so no database is needed; the Database library brings the MySQL
# client in for them.
add_executable(database_manager_tests EXCLUDE_FROM_ALL database_manager_test.cpp)
target_link_libraries(database_manager_tests PRIVATE Database ServerCore de-kernel ${MYSQL_LIBRARY} gtest_main Threads::Threads)
add_dependencies(wire_tests database_manager_tests)
add_test(NAME database_manager_tests COMMAND database_manager_tests)
set_tests_properties(database_manager_tests PROPERTIES TIMEOUT 30)

# Account password hashing (src/server/loginserver/PasswordHash.cpp) over the
# vendored argon2 (third_party/argon2). Upstream's own known-answer vectors
# pin the vendored library; the seed-account case reads initdb/DARKEDEN.sql
# (tools/devbuild.sh syncs initdb/ for it) and pins the shipped hashes.
add_executable(password_hash_tests EXCLUDE_FROM_ALL
    password_hash_test.cpp
)
target_compile_definitions(password_hash_tests PRIVATE
    PASSWORD_TEST_INITDB_DIR="${CMAKE_SOURCE_DIR}/initdb"
)
target_link_libraries(password_hash_tests PRIVATE PasswordHash gtest_main Threads::Threads)
add_dependencies(wire_tests password_hash_tests)
add_test(NAME password_hash_tests COMMAND password_hash_tests)
# A dozen 64 MiB argon2id computations; generous for a loaded CI runner.
set_tests_properties(password_hash_tests PROPERTIES TIMEOUT 120)

# The loginserver's character-creation decision (docs/RESTRUCTURING.md 3.1):
# decideCreatePC() returns Outcome<CreatedCharacter, CreatePCRejection>, so
# every rejection and the accepted rows are testable against a fake
# LoginCharacterRepository with no database. Only de-kernel is linked (the
# type tables, and filelog for the Ousters warning) — pulling the transport
# or the login server's own state into the decision would break this link.
# The Ousters warning appends to CreatePC.log in the working directory,
# hence the build-tree WORKING_DIRECTORY (same reason as scheduler_tests).
add_executable(character_creation_tests EXCLUDE_FROM_ALL
    character_creation_test.cpp
    ${CMAKE_SOURCE_DIR}/src/server/loginserver/CharacterCreation.cpp
)
target_include_directories(character_creation_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/support
    ${CMAKE_SOURCE_DIR}/src/Core
    ${CMAKE_SOURCE_DIR}/src/server/loginserver
)
target_link_libraries(character_creation_tests PRIVATE de-kernel gtest_main Threads::Threads)
add_dependencies(wire_tests character_creation_tests)
add_test(NAME character_creation_tests COMMAND character_creation_tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
set_tests_properties(character_creation_tests PROPERTIES TIMEOUT 60)

# The loginserver's character-selection decision (docs/RESTRUCTURING.md 3.1):
# decideSelectPC() returns Outcome<SelectedCharacter, SelectPCRejection> over a
# fake LoginCharacterRepository and a fake SelectPCTopology, so every rejection
# and the routing of an accepted selection are testable with neither a database
# nor a game server. Only de-kernel is linked (the type tables) — pulling the
# transport, the server-info managers or the login server's own state into the
# decision would break this link.
add_executable(character_selection_tests EXCLUDE_FROM_ALL
    character_selection_test.cpp
    ${CMAKE_SOURCE_DIR}/src/server/loginserver/CharacterSelection.cpp
)
target_include_directories(character_selection_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/support
    ${CMAKE_SOURCE_DIR}/src/Core
    ${CMAKE_SOURCE_DIR}/src/server/loginserver
)
target_link_libraries(character_selection_tests PRIVATE de-kernel gtest_main Threads::Threads)
add_dependencies(wire_tests character_selection_tests)
add_test(NAME character_selection_tests COMMAND character_selection_tests)
set_tests_properties(character_selection_tests PROPERTIES TIMEOUT 60)

add_test(
    NAME ratchets
    COMMAND bash ${CMAKE_SOURCE_DIR}/tests/ratchet/ratchets.sh
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

# Include-graph architecture test (docs/RESTRUCTURING.md 2.2): layering
# rules + shrink-only baseline in tests/arch/. Perl, not Python — the
# dev image ships perl and the repo's generators already use it.
add_test(
    NAME arch_includes
    COMMAND perl ${CMAKE_SOURCE_DIR}/tests/arch/check_includes.pl
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

# Critical-section audit (src/Core/Exception.h). The __ENTER/__LEAVE block is
# owned by a scoped CriticalSection guard, so a hand-written x.unlock() inside
# one now unlocks a non-recursive pthread mutex twice - undefined behaviour, and
# invisible in a diff. This fails on that, on a __LEAVE_CRITICAL_SECTION whose
# argument disagrees with its __ENTER's, on an unclosed section, and on finding
# no sections at all (a wrong working directory must not read as a pass). It
# scans ./src, hence WORKING_DIRECTORY at the repo root. Perl, like tests/arch/ -
# the dev image ships it and has no python3. -q drops the informational lines;
# the ERROR lines and the verdict always print.
add_test(
    NAME critical_section_audit
    COMMAND perl ${CMAKE_SOURCE_DIR}/tests/tools/critical_section_audit.pl -q
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
set_tests_properties(critical_section_audit PROPERTIES TIMEOUT 60)

# The loginserver's login decision (docs/RESTRUCTURING.md 3.1): decideLogin()
# returns Outcome<LoginAccepted, LoginRejection>, so every refusal, the
# precedence between them and the content of the OK reply are testable
# against a fake LoginAccountRepository with no database. PasswordHash is
# linked because the password check runs against the real argon2id
# implementation; de-kernel carries VSDateTime and filelog. Pulling the
# transport or LoginPlayer into the decision would break this link; the account
# owner is a value-only header shared with the runtime. The refusal logs (keydiff.txt) and the days-left log
# (PayPlayDateLog.txt) are appended in the working directory, hence the
# build-tree WORKING_DIRECTORY (same reason as scheduler_tests).
add_executable(login_decision_tests EXCLUDE_FROM_ALL
    login_decision_test.cpp
    ${CMAKE_SOURCE_DIR}/src/server/loginserver/LoginDecision.cpp
)
target_include_directories(login_decision_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/support
    ${CMAKE_SOURCE_DIR}/src/Core
    ${CMAKE_SOURCE_DIR}/src/server/loginserver
)
target_link_libraries(login_decision_tests PRIVATE PasswordHash de-kernel gtest_main Threads::Threads)
add_dependencies(wire_tests login_decision_tests)
add_test(NAME login_decision_tests COMMAND login_decision_tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
# A handful of 64 MiB argon2id computations in the password cases.
set_tests_properties(login_decision_tests PROPERTIES TIMEOUT 180)

# The Exchange system's decisions (docs/RESTRUCTURING.md 3.1):
# decideBuyListing() / decideCancelListing() / decideBuyerClaim() /
# decideSellerClaim() return Outcome<..., ExchangeRejection>, so every
# refusal, the precedence between them and the terms an accepted buy is
# priced on are testable against a fake ExchangeRepository with no
# database. formatExchangeError() is here too: its strings are what
# GCExchangeBuy puts on the wire. completeExchangePurchase(), the
# purchase's writes under its transaction guard, runs against the same fake,
# which honours the transaction and throws DatabaseError on request (hence
# the database include directory, for that header alone). ExchangeService
# itself is not linked - it needs a PlayerCreature and an Item, which is why
# the decisions were split out of it. de-kernel carries the ExchangeListing
# wire struct.
add_executable(exchange_decision_tests EXCLUDE_FROM_ALL
    exchange_decision_test.cpp
    ${CMAKE_SOURCE_DIR}/src/server/gameserver/exchange/ExchangeDecision.cpp
    ${CMAKE_SOURCE_DIR}/src/server/gameserver/exchange/ExchangePurchase.cpp
)
target_include_directories(exchange_decision_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/support
    ${CMAKE_SOURCE_DIR}/src/Core
    ${CMAKE_SOURCE_DIR}/src/server/database
    ${CMAKE_SOURCE_DIR}/src/server/gameserver
    ${CMAKE_SOURCE_DIR}/src/server/gameserver/exchange
)
target_link_libraries(exchange_decision_tests PRIVATE de-kernel gtest_main Threads::Threads)
add_dependencies(wire_tests exchange_decision_tests)
add_test(NAME exchange_decision_tests COMMAND exchange_decision_tests)
set_tests_properties(exchange_decision_tests PROPERTIES TIMEOUT 60)

# The loginserver's character-deletion decision (docs/RESTRUCTURING.md 3.1):
# decideDeletePC() returns Outcome<void, DeletePCRejection> over a fake
# LoginCharacterPurgeRepository, so the ownership check, the compare-and-set
# retirement and the precedence between the refusals are testable with no
# database. Only de-kernel is linked (the type tables) — pulling the
# transport or the login server's own state into the decision would break
# this link.
add_executable(character_deletion_tests EXCLUDE_FROM_ALL
    character_deletion_test.cpp
    ${CMAKE_SOURCE_DIR}/src/server/loginserver/CharacterDeletion.cpp
)
target_include_directories(character_deletion_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/support
    ${CMAKE_SOURCE_DIR}/src/Core
    ${CMAKE_SOURCE_DIR}/src/server/loginserver
)
target_link_libraries(character_deletion_tests PRIVATE de-kernel gtest_main Threads::Threads)
add_dependencies(wire_tests character_deletion_tests)
add_test(NAME character_deletion_tests COMMAND character_deletion_tests)
set_tests_properties(character_deletion_tests PROPERTIES TIMEOUT 60)

# The loginserver's reconnect decision (docs/RESTRUCTURING.md 3.1):
# decideReconnectLogin() returns Outcome<ReconnectAccepted,
# ReconnectRejection> over a fake LoginAccountRepository and a fake
# ReconnectSession, so every refusal, the precedence between them and the
# session state the decision writes are testable with neither a database nor
# a socket. Only de-kernel is linked (the type tables) — pulling the
# transport or the login server's own state into the decision would break
# this link.
add_executable(reconnect_decision_tests EXCLUDE_FROM_ALL
    reconnect_decision_test.cpp
    ${CMAKE_SOURCE_DIR}/src/server/loginserver/ReconnectDecision.cpp
)
target_include_directories(reconnect_decision_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/support
    ${CMAKE_SOURCE_DIR}/src/Core
    ${CMAKE_SOURCE_DIR}/src/server/loginserver
)
target_link_libraries(reconnect_decision_tests PRIVATE de-kernel gtest_main Threads::Threads)
add_dependencies(wire_tests reconnect_decision_tests)
add_test(NAME reconnect_decision_tests COMMAND reconnect_decision_tests)
set_tests_properties(reconnect_decision_tests PROPERTIES TIMEOUT 60)

# The gameserver's client-heartbeat plausibility rule: verifyHeartbeat() takes
# the arrival time as a number and the session's state as a struct, so the
# whole rule -- the gap, the count of early heartbeats a session may carry and
# what an on-time one pays back -- is testable without a player or a clock.
# Nothing is linked but gtest: the decision includes no game header, and
# pulling one in would break this link.
add_executable(speed_hack_decision_tests EXCLUDE_FROM_ALL
    speed_hack_decision_test.cpp
    ${CMAKE_SOURCE_DIR}/src/server/gameserver/SpeedHackDecision.cpp
)
target_include_directories(speed_hack_decision_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src/server/gameserver
)
target_link_libraries(speed_hack_decision_tests PRIVATE gtest_main Threads::Threads)
add_dependencies(wire_tests speed_hack_decision_tests)
add_test(NAME speed_hack_decision_tests COMMAND speed_hack_decision_tests)
set_tests_properties(speed_hack_decision_tests PROPERTIES TIMEOUT 60)

# The gameserver's per-frame receive gate: GameFrameGate decides the frame
# at the front of a client's input stream (wait, read, skip an ignored
# frame, or refuse) and counts the client's sequence bytes, so the cases can
# hand the stream a frame in parts, as TCP may, with no player and no
# socket. GamePlayer.cpp itself links the whole gameserver, which is why the
# gate was split out of processCommand. Compiled as the gameserver, like
# game_client_link_tests, for its factory table and validator. A failing
# Assert appends to assertion_failed.log in the working directory, hence the
# build-tree WORKING_DIRECTORY.
add_executable(game_frame_gate_tests EXCLUDE_FROM_ALL
    game_frame_gate_test.cpp
    ${CMAKE_SOURCE_DIR}/src/server/gameserver/GameFrameGate.cpp
)
target_compile_definitions(game_frame_gate_tests PRIVATE __GAME_SERVER__ __COMBAT__)
target_include_directories(game_frame_gate_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/support
    ${CMAKE_SOURCE_DIR}/src/Core
    ${CMAKE_SOURCE_DIR}/src/server/gameserver
)
target_link_libraries(game_frame_gate_tests PRIVATE GameServerPackets gtest_main Threads::Threads)
add_dependencies(wire_tests game_frame_gate_tests)
add_test(NAME game_frame_gate_tests COMMAND game_frame_gate_tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
set_tests_properties(game_frame_gate_tests PROPERTIES TIMEOUT 60)

# The loginserver's account-registration decision (docs/RESTRUCTURING.md
# 3.1): decideRegisterPlayer() returns Outcome<LoginNewAccount,
# RegisterPlayerRefusal> over a fake LoginAccountRepository, so every
# refusal, the precedence between them and the Player row an accepted
# registration produces are testable with no database. PasswordHash is
# linked for the explicit hashing adapter tested with the real argon2id;
# de-kernel carries the type tables. Pulling the transport or the login
# server's own state into the decision would break this link.
add_executable(registration_tests EXCLUDE_FROM_ALL
    registration_test.cpp
    ${CMAKE_SOURCE_DIR}/src/server/loginserver/Registration.cpp
)
target_include_directories(registration_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/support
    ${CMAKE_SOURCE_DIR}/src/Core
    ${CMAKE_SOURCE_DIR}/src/server/loginserver
)
target_link_libraries(registration_tests PRIVATE PasswordHash de-kernel gtest_main Threads::Threads)
add_dependencies(wire_tests registration_tests)
add_test(NAME registration_tests COMMAND registration_tests)
# A handful of 64 MiB argon2id computations in the accepted cases.
set_tests_properties(registration_tests PROPERTIES TIMEOUT 180)

# The gameserver's guild join and registration decisions
# (docs/RESTRUCTURING.md 3.1): decideGuildJoinAttempt,
# decideGuildRegistration, decideGuildJoinConfirm and decideGuildRequirements
# return Outcome<..., GuildJoinRejection> over a fake GuildRepository, so
# every refusal, the precedence between them, the quit-penalty date
# arithmetic and the response code each refusal carries to each race are
# testable with no database. The handlers themselves are not linked - they
# need a creature and a socket, which is why the decisions were split out of
# them. de-kernel carries the type tables and the NPC response codes.
add_executable(guild_join_decision_tests EXCLUDE_FROM_ALL
    guild_join_decision_test.cpp
    ${CMAKE_SOURCE_DIR}/src/server/gameserver/guild/GuildJoinDecision.cpp
)
target_include_directories(guild_join_decision_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/support
    ${CMAKE_SOURCE_DIR}/src/Core
    ${CMAKE_SOURCE_DIR}/src/server/gameserver
    ${CMAKE_SOURCE_DIR}/src/server/gameserver/guild
)
target_link_libraries(guild_join_decision_tests PRIVATE de-kernel gtest_main Threads::Threads)
add_dependencies(wire_tests guild_join_decision_tests)
add_test(NAME guild_join_decision_tests COMMAND guild_join_decision_tests)
set_tests_properties(guild_join_decision_tests PROPERTIES TIMEOUT 60)

# What becomes of a guild union when one of its guilds is disbanded or thrown
# out: whether the union survives, which GuildUnionMember rows go and which
# guilds are told. decideUnionTeardown answers from plain values - the union
# manager that performs the answer needs the guild tables, the PC finder and a
# socket, which is why the decision was split out of it. de-kernel carries the
# type tables.
add_executable(guild_union_teardown_tests EXCLUDE_FROM_ALL
    guild_union_teardown_test.cpp
    ${CMAKE_SOURCE_DIR}/src/server/gameserver/guild/GuildUnionTeardown.cpp
)
target_include_directories(guild_union_teardown_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src/Core
    ${CMAKE_SOURCE_DIR}/src/server/gameserver/guild
)
target_link_libraries(guild_union_teardown_tests PRIVATE de-kernel gtest_main Threads::Threads)
add_dependencies(wire_tests guild_union_teardown_tests)
add_test(NAME guild_union_teardown_tests COMMAND guild_union_teardown_tests)
set_tests_properties(guild_union_teardown_tests PROPERTIES TIMEOUT 60)

# Whether a guild may offer to join a union, and when a union with nobody
# left goes: every refusal is decided before a union is opened for the
# offer, so a refused offer leaves no union behind. The rules answer from
# plain values; the union manager that applies them needs the union tables.
add_executable(guild_union_join_offer_tests EXCLUDE_FROM_ALL
    guild_union_join_offer_test.cpp
    ${CMAKE_SOURCE_DIR}/src/server/gameserver/guild/GuildUnionJoinOffer.cpp
)
target_include_directories(guild_union_join_offer_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src/server/gameserver/guild
)
target_link_libraries(guild_union_join_offer_tests PRIVATE gtest_main Threads::Threads)
add_dependencies(wire_tests guild_union_join_offer_tests)
add_test(NAME guild_union_join_offer_tests COMMAND guild_union_join_offer_tests)
set_tests_properties(guild_union_join_offer_tests PROPERTIES TIMEOUT 60)

# Which union a guild resolves to and what a retired union answers
# (docs/FIXES.md): the union manager is changed and read from several
# threads, so the unions it takes away are retired rather than freed and the
# lookup tables are read and written under one mutex. GuildUnionRegistry.cpp
# is the only source linked; ServerCore carries Mutex. A failing Assert
# appends to assertion_failed.log in the working directory, hence the
# build-tree WORKING_DIRECTORY.
add_executable(guild_union_registry_tests EXCLUDE_FROM_ALL
    guild_union_registry_test.cpp
    ${CMAKE_SOURCE_DIR}/src/server/gameserver/guild/GuildUnionRegistry.cpp
)
target_include_directories(guild_union_registry_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src/Core
    ${CMAKE_SOURCE_DIR}/src/server
    ${CMAKE_SOURCE_DIR}/src/server/gameserver/guild
)
target_link_libraries(guild_union_registry_tests PRIVATE ServerCore de-kernel gtest_main Threads::Threads)
add_dependencies(wire_tests guild_union_registry_tests)
add_test(NAME guild_union_registry_tests COMMAND guild_union_registry_tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
set_tests_properties(guild_union_registry_tests PROPERTIES TIMEOUT 60)

# What a retired guild and a retired guild member answer (docs/FIXES.md):
# both are taken out of their map and parked rather than freed, because a
# zone thread may already hold the pointer, so the object stays readable and
# must say it has gone. Guild.cpp is the only source linked - the retirement
# paths reach no table, so the test defines the repository accessor over the
# fake, which refuses every call; ServerCore carries Mutex and de-kernel the
# member/guild wire structs. A failing Assert appends to
# assertion_failed.log in the working directory, hence the build-tree
# WORKING_DIRECTORY (same reason as scheduler_tests).
add_executable(guild_retirement_tests EXCLUDE_FROM_ALL
    guild_retirement_test.cpp
    ${CMAKE_SOURCE_DIR}/src/server/gameserver/Guild.cpp
)
target_include_directories(guild_retirement_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/support
    ${CMAKE_SOURCE_DIR}/src/Core
    ${CMAKE_SOURCE_DIR}/src/server
    ${CMAKE_SOURCE_DIR}/src/server/gameserver
)
target_link_libraries(guild_retirement_tests PRIVATE ServerCore de-kernel gtest_main Threads::Threads)
add_dependencies(wire_tests guild_retirement_tests)
add_test(NAME guild_retirement_tests COMMAND guild_retirement_tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
set_tests_properties(guild_retirement_tests PROPERTIES TIMEOUT 60)

# The gameserver's party invite protocol (docs/RESTRUCTURING.md 3.1):
# decidePartyInvite returns Outcome<PartyInviteEvents, PartyInviteRejection>
# over a recording fake topology, so every branch of every CGPartyInvite code,
# the precedence between the refusals, the packet and recipient each answer
# carries, and the queries a branch does and does not make are testable with
# no zone and no party manager. The handler itself is not linked - it needs a
# creature and a socket, which is why the decision was split out of it.
# de-kernel carries the packet code tables.
add_executable(party_invite_decision_tests EXCLUDE_FROM_ALL
    party_invite_decision_test.cpp
    ${CMAKE_SOURCE_DIR}/src/server/gameserver/party/PartyInviteDecision.cpp
)
target_include_directories(party_invite_decision_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src/Core
    ${CMAKE_SOURCE_DIR}/src/server/gameserver
    ${CMAKE_SOURCE_DIR}/src/server/gameserver/party
)
target_link_libraries(party_invite_decision_tests PRIVATE de-kernel gtest_main Threads::Threads)
add_dependencies(wire_tests party_invite_decision_tests)
add_test(NAME party_invite_decision_tests COMMAND party_invite_decision_tests)
set_tests_properties(party_invite_decision_tests PROPERTIES TIMEOUT 60)

# The loginserver's world and server-group decisions
# (docs/RESTRUCTURING.md 3.1): decideSelectWorld returns Outcome<void,
# SelectWorldRejection> and decideSelectServer returns
# Outcome<SelectedServer, SelectServerRejection> over a recording fake
# topology, so world/group membership, missing-ID normalization, empty-table
# and closed refusals, the queries a decision does and does not make, and every
# boundary of the population ladder serverListFor shares with CLGetServerListHandler are
# testable with no login server tables and no socket. The handlers themselves
# are not linked here. login_server_runtime_tests also tests the production
# topology over explicitly supplied managers. Only de-kernel is linked here
# (the type tables carrying the server and world status enums).
add_executable(world_selection_tests EXCLUDE_FROM_ALL
    world_selection_test.cpp
    ${CMAKE_SOURCE_DIR}/src/server/loginserver/WorldSelection.cpp
)
target_include_directories(world_selection_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/support
    ${CMAKE_SOURCE_DIR}/src/Core
    ${CMAKE_SOURCE_DIR}/src/server/loginserver
)
target_link_libraries(world_selection_tests PRIVATE de-kernel gtest_main Threads::Threads)
add_dependencies(wire_tests world_selection_tests)
add_test(NAME world_selection_tests COMMAND world_selection_tests)
set_tests_properties(world_selection_tests PROPERTIES TIMEOUT 60)

# The gameserver's trade prepare protocol (docs/RESTRUCTURING.md 3.1):
# decideTradePrepare returns Outcome<TradePrepareEvents,
# TradePrepareRejection> over a recording fake topology, so every branch of
# every CGTradePrepare code, the precedence between the refusals, the packet,
# recipient and object id each answer carries, the TradeManager mutation that
# follows it, and the queries a branch does and does not make are testable
# with no zone and no trade manager. The handler itself is not linked - it
# needs a creature and a socket, which is why the decision was split out of
# it. de-kernel carries the packet code tables.
add_executable(trade_prepare_decision_tests EXCLUDE_FROM_ALL
    trade_prepare_decision_test.cpp
    ${CMAKE_SOURCE_DIR}/src/server/gameserver/trade/TradePrepareDecision.cpp
)
target_include_directories(trade_prepare_decision_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src/Core
    ${CMAKE_SOURCE_DIR}/src/server/gameserver
    ${CMAKE_SOURCE_DIR}/src/server/gameserver/trade
)
target_link_libraries(trade_prepare_decision_tests PRIVATE de-kernel gtest_main Threads::Threads)
add_dependencies(wire_tests trade_prepare_decision_tests)
add_test(NAME trade_prepare_decision_tests COMMAND trade_prepare_decision_tests)
set_tests_properties(trade_prepare_decision_tests PROPERTIES TIMEOUT 60)

# The gameserver's three trade table requests (docs/RESTRUCTURING.md 3.1):
# decideTradeTableGate, decideTradeAddItem, decideTradeRemoveItem and
# decideTradeMoney answer Outcome<TradeTableEvents, TradeTableRejection> over a
# recording fake topology, so the gate the three share and its precedence,
# every item, gift box and gold branch, the packet and recipient each answer
# carries, the trade record mutations that go with it in order, and the queries
# a branch does and does not make are testable with no zone and no trade
# manager. The handlers themselves are not linked - they need a creature, an
# inventory and a socket, which is why the decisions were split out of them.
# de-kernel carries the packet code tables.
add_executable(trade_table_decision_tests EXCLUDE_FROM_ALL
    trade_table_decision_test.cpp
    ${CMAKE_SOURCE_DIR}/src/server/gameserver/trade/TradeTableDecision.cpp
)
target_include_directories(trade_table_decision_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src/Core
    ${CMAKE_SOURCE_DIR}/src/server/gameserver
    ${CMAKE_SOURCE_DIR}/src/server/gameserver/trade
)
target_link_libraries(trade_table_decision_tests PRIVATE de-kernel gtest_main Threads::Threads)
add_dependencies(wire_tests trade_table_decision_tests)
add_test(NAME trade_table_decision_tests COMMAND trade_table_decision_tests)
set_tests_properties(trade_table_decision_tests PROPERTIES TIMEOUT 60)

# The sharedserver's guild mutations (docs/RESTRUCTURING.md 3.1):
# decideAddGuild, decideGuildActivation, decideExpelGuildMember,
# decideQuitGuild, decideGuildBreakup and decideModifyGuildMember answer
# Outcome<..., SharedGuildRejection>, so every branch of founding a guild,
# carrying its registration through, expelling a member, quitting, breaking
# the guild up and changing a rank, the precedence between the refusals, and
# the ordered trace each answer produces - the character-side writes through a
# recording SharedGuildRepository fake, the roster and guild mutations and the
# packets recorded beside them - are testable with no guild tables and no
# database. The handlers themselves are not linked - they need the
# GuildManager, a socket and the StringPool, which is why the decisions were
# split out of them. de-kernel carries the type tables.
add_executable(shared_guild_decision_tests EXCLUDE_FROM_ALL
    shared_guild_decision_test.cpp
    ${CMAKE_SOURCE_DIR}/src/server/sharedserver/GuildDecision.cpp
)
target_include_directories(shared_guild_decision_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/support
    ${CMAKE_SOURCE_DIR}/src/Core
    ${CMAKE_SOURCE_DIR}/src/server/sharedserver
)
target_link_libraries(shared_guild_decision_tests PRIVATE de-kernel gtest_main Threads::Threads)
add_dependencies(wire_tests shared_guild_decision_tests)
add_test(NAME shared_guild_decision_tests COMMAND shared_guild_decision_tests)
set_tests_properties(shared_guild_decision_tests PROPERTIES TIMEOUT 60)

# What END_DB throws (src/server/database/DB.h): the DatabaseError a failed
# statement raises, its message read after the handler that caught it is
# gone, and the reach the __END_CATCH family gives it. Needs no database —
# the SQLQueryException the driver would raise is thrown inside the
# BEGIN_DB block — but DB.h reaches the MySQL headers through Connection.h
# and the macro deletes a Statement, so it links the Database library like
# database_timeout_tests above.
add_executable(database_error_tests EXCLUDE_FROM_ALL database_error_test.cpp)
target_link_libraries(database_error_tests PRIVATE Database ServerCore de-kernel ${MYSQL_LIBRARY} gtest_main Threads::Threads)
add_dependencies(wire_tests database_error_tests)
add_test(NAME database_error_tests COMMAND database_error_tests)
set_tests_properties(database_error_tests PROPERTIES TIMEOUT 30)

# The GM chat command tables (docs/RESTRUCTURING.md 4.1): CommandRouter plus
# the production registrations, which are where every command's name, prefix
# length and permission level are declared - the commands a player may type
# and the ones another game server may relay. The command bodies are stubbed by
# the test itself - the real ones need a zone, an inventory and a database -
# so what is linked here is the table and the matching rule, and nothing of
# the gameserver. de-kernel is not needed either: the router carries no packet
# and no wire type.
add_executable(gm_command_router_tests EXCLUDE_FROM_ALL
    gm_command_router_test.cpp
    ${CMAKE_SOURCE_DIR}/src/server/gameserver/gm/CommandRouter.cpp
    ${CMAKE_SOURCE_DIR}/src/server/gameserver/gm/CommandRegistration.cpp
    ${CMAKE_SOURCE_DIR}/src/server/gameserver/gm/RelayCommandRegistration.cpp
)
target_include_directories(gm_command_router_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src/server/gameserver
)
target_link_libraries(gm_command_router_tests PRIVATE gtest_main Threads::Threads)
add_dependencies(wire_tests gm_command_router_tests)
add_test(NAME gm_command_router_tests COMMAND gm_command_router_tests)
set_tests_properties(gm_command_router_tests PROPERTIES TIMEOUT 60)

# The *command console sub-command table (docs/RESTRUCTURING.md 4.1): the
# production registration, which is where every sub-command name, permission
# level and null-player admission is declared, over the exact-match
# SubcommandTable. The
# bodies are stubbed by the test - the real ones need a zone, an inventory and
# a database - so what is linked is the table and the matching rule.
# de-kernel carries GCSystemMessage, the reply a body fills in.
add_executable(gm_console_command_tests EXCLUDE_FROM_ALL
    gm_console_command_test.cpp
    ${CMAKE_SOURCE_DIR}/src/server/gameserver/gm/SubcommandTable.cpp
    ${CMAKE_SOURCE_DIR}/src/server/gameserver/gm/ConsoleCommandRegistration.cpp
)
target_include_directories(gm_console_command_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src/Core
    ${CMAKE_SOURCE_DIR}/src/server/gameserver
)
target_link_libraries(gm_console_command_tests PRIVATE de-kernel gtest_main Threads::Threads)
add_dependencies(wire_tests gm_console_command_tests)
add_test(NAME gm_console_command_tests COMMAND gm_console_command_tests)
set_tests_properties(gm_console_command_tests PROPERTIES TIMEOUT 60)

# The race table a player creature's rows are addressed to
# (docs/RESTRUCTURING.md 4.3): characterRaceOf() is the one place that decides
# it now that the three race classes share their persistence bodies, so the
# mapping is pinned here. Beside it, the weapon bits a slayer's
# character-list outlook carries (SlayerListWeapon.h), which
# Slayer::getShapeInfo writes to Slayer.Shape. Header-only, so nothing but
# gtest is linked.
add_executable(player_race_tests EXCLUDE_FROM_ALL
    player_race_test.cpp
    slayer_list_weapon_test.cpp
)
target_include_directories(player_race_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src/Core
    ${CMAKE_SOURCE_DIR}/src/server/gameserver
)
target_link_libraries(player_race_tests PRIVATE gtest_main Threads::Threads)
add_dependencies(wire_tests player_race_tests)
add_test(NAME player_race_tests COMMAND player_race_tests)
set_tests_properties(player_race_tests PROPERTIES TIMEOUT 60)

# --- race skill slot -------------------------------------------------------
# RaceSkillSlot carries the run-time arithmetic and the delay-change save
# rule for both the vampire and the ousters slot. The slot compiles on its
# own -- the only Core source it needs is the string stream the exception
# stack traces are built with. Without __GAME_SERVER__ the slot reads the
# wall clock rather than the gameserver's per-tick gCurrentTime, so the
# run-time tests allow a tick of drift.
add_executable(race_skill_slot_tests EXCLUDE_FROM_ALL
    race_skill_slot_test.cpp
    ${CMAKE_SOURCE_DIR}/src/server/gameserver/skill/RaceSkillSlot.cpp
    ${CMAKE_SOURCE_DIR}/src/Core/StringStream.cpp
)
target_include_directories(race_skill_slot_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src/Core
    ${CMAKE_SOURCE_DIR}/src/server
    ${CMAKE_SOURCE_DIR}/src/server/gameserver
)
target_link_libraries(race_skill_slot_tests PRIVATE gtest_main Threads::Threads)
add_dependencies(wire_tests race_skill_slot_tests)
add_test(NAME race_skill_slot_tests COMMAND race_skill_slot_tests)
set_tests_properties(race_skill_slot_tests PROPERTIES TIMEOUT 60)

# --- slayer skill slot -----------------------------------------------------
# SkillSlot is the slayer's slot and repeats the same run-time arithmetic.
# It compiles on its own too; the one symbol it needs from the rest of the
# gameserver is the skill-save repository, which the test stands in for.
add_executable(skill_slot_tests EXCLUDE_FROM_ALL
    skill_slot_test.cpp
    ${CMAKE_SOURCE_DIR}/src/server/gameserver/skill/SkillSlot.cpp
    ${CMAKE_SOURCE_DIR}/src/Core/StringStream.cpp
)
target_include_directories(skill_slot_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src/Core
    ${CMAKE_SOURCE_DIR}/src/server
    ${CMAKE_SOURCE_DIR}/src/server/gameserver
)
target_link_libraries(skill_slot_tests PRIVATE gtest_main Threads::Threads)
add_dependencies(wire_tests skill_slot_tests)
add_test(NAME skill_slot_tests COMMAND skill_slot_tests)
set_tests_properties(skill_slot_tests PROPERTIES TIMEOUT 60)

# --- client-link packet ids ------------------------------------------------
# Which packet ids each server's validator lets a client's connection read:
# only what a client sends. Each test is compiled as its server (the
# PlayerStatus values and the factory table differ per server) and links
# that server's packet library, like the fuzz targets below. A failing
# Assert appends to assertion_failed.log in the working directory, hence
# the build-tree WORKING_DIRECTORY.
add_executable(game_client_link_tests EXCLUDE_FROM_ALL game_client_link_test.cpp)
target_compile_definitions(game_client_link_tests PRIVATE __GAME_SERVER__ __COMBAT__)
target_include_directories(game_client_link_tests PRIVATE ${CMAKE_SOURCE_DIR}/src/Core)
target_link_libraries(game_client_link_tests PRIVATE GameServerPackets gtest_main Threads::Threads)
add_dependencies(wire_tests game_client_link_tests)
add_test(NAME game_client_link_tests COMMAND game_client_link_tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
set_tests_properties(game_client_link_tests PROPERTIES TIMEOUT 30)

add_executable(login_client_link_tests EXCLUDE_FROM_ALL login_client_link_test.cpp)
target_compile_definitions(login_client_link_tests PRIVATE __LOGIN_SERVER__)
target_include_directories(login_client_link_tests PRIVATE ${CMAKE_SOURCE_DIR}/src/Core)
target_link_libraries(login_client_link_tests PRIVATE LoginServerPackets gtest_main Threads::Threads)
add_dependencies(wire_tests login_client_link_tests)
add_test(NAME login_client_link_tests COMMAND login_client_link_tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
set_tests_properties(login_client_link_tests PROPERTIES TIMEOUT 30)

# --- packet-read fuzz targets ----------------------------------------------
# The replay builds of the fuzz targets and their ctests over the seed
# corpus and tests/fuzz/regressions/; see tests/fuzz/CMakeLists.txt.
add_subdirectory(fuzz)
