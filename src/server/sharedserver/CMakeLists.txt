# ============================================================================
# SharedServer 可执行文件
# ============================================================================

set(SHAREDSERVER_RUNTIME_SOURCES
    handler/GSAddGuildHandler.cpp
    handler/GSAddGuildMemberHandler.cpp
    handler/GSExpelGuildMemberHandler.cpp
    handler/GSGuildMemberLogOnHandler.cpp
    handler/GSModifyGuildIntroHandler.cpp
    handler/GSModifyGuildMemberHandler.cpp
    handler/GSQuitGuildHandler.cpp
    handler/GSRequestGuildInfoHandler.cpp
    SharedContext.cpp
    SharedPacketDispatch.cpp
    Guild.cpp
    GuildDecision.cpp
    GuildStepRunner.cpp
    GuildManager.cpp
    ResurrectLocationManager.cpp
    GameServerPlayer.cpp
    HeartbeatManager.cpp
    SharedGameServerInfoManager.cpp
    GameServerGroupInfoManager.cpp
    GameServerManager.cpp
    SharedServer.cpp
    StringPool.cpp
    repository/MySQLSharedConfigRepository.cpp
    repository/MySQLSharedGuildRepository.cpp
)

# Link the real handlers and managers in tests without the process setup,
# listeners and shutdown performed by main(). Keep each server runtime in its
# own executable: the servers define different classes with the same names.
add_library(SharedServerRuntime OBJECT ${SHAREDSERVER_RUNTIME_SOURCES})

# 编译定义
target_compile_definitions(SharedServerRuntime PUBLIC __SHARED_SERVER__)

# 包含目录
target_include_directories(SharedServerRuntime PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src/Core
    ${CMAKE_SOURCE_DIR}/src/server
    ${CMAKE_SOURCE_DIR}/src/server/database
)

# 链接库
if(APPLE)
    set(EXTRA_LIBS z)
elseif(UNIX)
    set(EXTRA_LIBS nsl z dl)
endif()

target_link_libraries(SharedServerRuntime PUBLIC
    Database
    SharedServerPackets
    ServerCore
    Core
    # The wire archive itself (task 2.4) — also reached transitively through
    # Core/SharedServerPackets, named here so the layering is visible.
    de-kernel
    ${MYSQL_LIBRARY}
    Threads::Threads
    ${EXTRA_LIBS}
)

add_executable(sharedserver main.cpp)
target_link_libraries(sharedserver PRIVATE
    SharedServerRuntime ServerApplication ServerProcessShutdown ServerFatalHandlers)
