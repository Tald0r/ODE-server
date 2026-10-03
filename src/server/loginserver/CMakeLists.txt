# ============================================================================
# LoginServer 可执行文件
# ============================================================================

set(LOGINSERVER_RUNTIME_SOURCES
    handler/CGConnectSetKeyHandler.cpp
    handler/CLChangeServerHandler.cpp
    handler/CLCreatePCHandler.cpp
    handler/CLDeletePCHandler.cpp
    handler/CLGetPCListHandler.cpp
    handler/CLGetServerListHandler.cpp
    handler/CLGetWorldListHandler.cpp
    handler/CLLoginHandler.cpp
    handler/CLLogoutHandler.cpp
    handler/CLQueryCharacterNameHandler.cpp
    handler/CLQueryPlayerIDHandler.cpp
    handler/CLReconnectLoginHandler.cpp
    handler/CLRegisterPlayerHandler.cpp
    handler/CLSelectPCHandler.cpp
    handler/CLSelectServerHandler.cpp
    handler/CLSelectWorldHandler.cpp
    handler/CLVersionCheckHandler.cpp
    handler/GLIncomingConnectionErrorHandler.cpp
    handler/GLIncomingConnectionHandler.cpp
    handler/GLIncomingConnectionOKHandler.cpp
    handler/GLKickVerifyHandler.cpp
    handler/GMServerInfoHandler.cpp
    LoginContext.cpp
    LoginConnection.cpp
    LoginWorldList.cpp
    LoginServerList.cpp
    LoginCharacterList.cpp
    LoginCharacterListRefresh.cpp
    LoginServerSelection.cpp
    LoginKickPreparation.cpp
    LoginKickDispatch.cpp
    LoginDatagramSend.cpp
    LoginKickVerification.cpp
    LoginKickRetry.cpp
    LoginCompletion.cpp
    LoginAccountSession.cpp
    LoginPlayerRetirement.cpp
    LoginIncomingReply.cpp
    LoginIncomingRequest.cpp
    LoginCharacterSelection.cpp
    LoginCharacterCreation.cpp
    LoginCharacterDeletion.cpp
    LoginCharacterNameQuery.cpp
    LoginAccountNameQuery.cpp
    LoginPacketDispatch.cpp
    CharacterCreation.cpp
    CharacterDeletion.cpp
    CharacterSelection.cpp
    LoginDecision.cpp
    ReconnectDecision.cpp
    Registration.cpp
    WorldSelection.cpp
    ClientManager.cpp
    UserInfoManager.cpp
    GameServerGroupInfoManager.cpp
    GameServerManager.cpp
    LoginPlayer.cpp
    LoginPlayerManager.cpp
    LoginServer.cpp
    ZoneGroupInfoManager.cpp
    ZoneInfoManager.cpp
    ItemDestroyer.cpp
    ReconnectLoginInfoManager.cpp
    repository/MySQLLoginAccountRepository.cpp
    repository/MySQLLoginCharacterPurgeRepository.cpp
    repository/MySQLLoginCharacterRepository.cpp
    repository/MySQLLoginConfigRepository.cpp
)

# Argon2id hashing and verification of account passwords; shared by the
# loginserver, the hashpw tool and tests/password_hash_test.cpp.
add_library(PasswordHash STATIC PasswordHash.cpp)
target_include_directories(PasswordHash PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(PasswordHash PUBLIC argon2)

# Operator tool: prints the argon2id hash of a password read from stdin for
# the Player.Password column, or checks a password against a stored value.
add_executable(hashpw tools/hashpw.cpp)
target_link_libraries(hashpw PRIVATE PasswordHash)

# The executable and runtime tests link these same objects. main() alone owns
# process setup and shutdown; the server macro and headers travel with the
# runtime so consumers compile the same class definitions.
add_library(LoginServerRuntime OBJECT ${LOGINSERVER_RUNTIME_SOURCES})

# 编译定义
target_compile_definitions(LoginServerRuntime PUBLIC __LOGIN_SERVER__)

# 包含目录
target_include_directories(LoginServerRuntime PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src/Core
    ${CMAKE_SOURCE_DIR}/src/server
    ${CMAKE_SOURCE_DIR}/src/server/database
    # The CL handlers include gameserver's OptionInfo.h; name the dir here
    # (Core's own gameserver export is gone since task 2.4).
    ${CMAKE_SOURCE_DIR}/src/server/gameserver
)

# 链接库
if(APPLE)
    set(EXTRA_LIBS z)
elseif(UNIX)
    set(EXTRA_LIBS nsl z dl)
endif()

target_link_libraries(LoginServerRuntime PUBLIC
    LoginServerDatabase
    LoginServerPackets
    ServerCore
    ServerWorkerShutdown
    ServerStartSequence
    ListenerStartup
    ServerPortSettings
    AcceptedServerConnection
    Core
    # The wire archive itself (task 2.4) — also reached transitively through
    # Core/LoginServerPackets, named here so the layering is visible.
    de-kernel
    PasswordHash
    ${MYSQL_LIBRARY}
    Threads::Threads
    ${EXTRA_LIBS}
)

add_executable(loginserver main.cpp)
target_link_libraries(loginserver PRIVATE
    LoginServerRuntime ServerApplication ServerProcessShutdown ServerFatalHandlers ServerProcessEnvironment)
