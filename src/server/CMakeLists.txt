# ============================================================================
# Server 模块
# ============================================================================

# ============================================================================
# ServerCore 库 - 服务器公共代码
# ============================================================================
set(SERVER_CORE_SOURCES
    CondVar.cpp
    Mutex.cpp
    PlayerManager.cpp
    GameWorldInfoManager.cpp
    SystemAPI.cpp
    Thread.cpp
    ThreadAttr.cpp
    Timeval.cpp
    pthreadAPI.cpp
    Profile.cpp
    ProxyAcceptor.cpp
    PacketProfile.cpp
    GameServerInfoManager.cpp
    ServerContext.cpp
    PaySystem.cpp
    repository/MySQLPayPlayRepository.cpp
    repository/MySQLServerInfoRepository.cpp
)

add_library(ServerCore STATIC ${SERVER_CORE_SOURCES})
target_include_directories(ServerCore PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src/Core
)

# Entry-point configuration, testable without any server runtime or database.
add_library(ServerStartup STATIC ServerStartup.cpp)
target_include_directories(ServerStartup PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ServerStartup PUBLIC de-kernel)

# Shared lifecycle control without a concrete server, listeners or database.
add_library(ServerLifecycle STATIC ServerLifecycle.cpp)
target_include_directories(ServerLifecycle PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ServerLifecycle PUBLIC de-kernel)

# ============================================================================
# 子目录
# ============================================================================
add_subdirectory(database)
add_subdirectory(gameserver)
add_subdirectory(loginserver)
add_subdirectory(sharedserver)
