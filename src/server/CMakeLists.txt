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

# Required listener-port policy, independent of binding sockets or managers.
add_library(ServerPortSettings STATIC ServerPortSettings.cpp)
target_include_directories(ServerPortSettings PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ServerPortSettings PUBLIC de-kernel)

# Shared lifecycle control without a concrete server, listeners or database.
add_library(ServerLifecycle STATIC ServerLifecycle.cpp)
target_include_directories(ServerLifecycle PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ServerLifecycle PUBLIC de-kernel)

# Application orchestration with an owned configuration and an explicit context.
add_library(ServerApplication STATIC ServerApplication.cpp)
target_include_directories(ServerApplication PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ServerApplication PUBLIC ServerStartup ServerPortSettings ServerLifecycle)

# Shared worker drain order and diagnostics, without a server runtime or database.
add_library(ServerWorkerShutdown STATIC ServerWorkerShutdown.cpp)
target_include_directories(ServerWorkerShutdown PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ServerWorkerShutdown PUBLIC ServerCore de-kernel Threads::Threads)

# Process signal/deadline setup, independent of any concrete server or kernel.
add_library(ServerProcessShutdown STATIC ServerProcessShutdown.cpp)
target_include_directories(ServerProcessShutdown PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ServerProcessShutdown PUBLIC Threads::Threads)

# Fatal handlers use only the standard library and POSIX file descriptors.
add_library(ServerFatalHandlers STATIC ServerFatalHandlers.cpp)
target_include_directories(ServerFatalHandlers PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# Process limits, random seeding and game signal policy, without a server runtime.
add_library(ServerProcessEnvironment STATIC ServerProcessEnvironment.cpp)
target_include_directories(ServerProcessEnvironment PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# Listener retries are independent of manager construction and concrete sockets.
add_library(ListenerStartup STATIC ListenerStartup.cpp)
target_include_directories(ListenerStartup PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ListenerStartup PUBLIC de-kernel Threads::Threads)

# Outbound socket setup is independent of retry loops and protocol sessions.
add_library(OutboundServerConnection STATIC OutboundServerConnection.cpp)
target_include_directories(OutboundServerConnection PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(OutboundServerConnection PUBLIC de-kernel)

# ============================================================================
# 子目录
# ============================================================================
add_subdirectory(database)
add_subdirectory(gameserver)
add_subdirectory(loginserver)
add_subdirectory(sharedserver)
