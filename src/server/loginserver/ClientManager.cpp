//////////////////////////////////////////////////////////////////////
//
// Filename    : ClientManager.cpp
// Written by  : reiot@ewestsoft.com
// Description : Client manager for the login server
//
//////////////////////////////////////////////////////////////////////

#include "ClientManager.h"

#include "Assert.h"
#include "DatabaseManager.h"
#include "GameServerGroupInfoManager.h"
#include "GameWorldInfoManager.h"
#include "KeepAlive.h"
#include "KernelContext.h"
#include "LoginContext.h"
#include "LoginPlayerManager.h"
#include "PacketProfile.h"
#include "Profile.h"
#include "Properties.h"
#include "ReconnectLoginInfoManager.h"
#include "ServerContext.h"
#include "ServerShutdown.h"
#include "Timeval.h"

//////////////////////////////////////////////////////////////////////
//
// constructor
//
// This is where the sub-manager objects are created.
//
//////////////////////////////////////////////////////////////////////
ClientManager::ClientManager() {
    __BEGIN_TRY

    // Create the login player manager.
    m_pLoginPlayerManager = new LoginPlayerManager();
    de::loginContext().setLoginPlayerManager(m_pLoginPlayerManager);

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////
//
// destructor
//
// This is where the sub-manager objects are deleted.
//
//////////////////////////////////////////////////////////////////////
ClientManager::~ClientManager() noexcept(false) {
    __BEGIN_TRY

    // Delete the login player manager.
    if (m_pLoginPlayerManager != NULL) {
        delete m_pLoginPlayerManager;
        m_pLoginPlayerManager = NULL;
    }

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////
//
// Initialize the sub-manager objects, then initialize itself.
//
//////////////////////////////////////////////////////////////////////
void ClientManager::init() {
    __BEGIN_TRY

    m_pLoginPlayerManager->init();

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////
//
// Start the service.
//
//////////////////////////////////////////////////////////////////////
void ClientManager::start() {
    __BEGIN_TRY

    run(); // Call the run() method directly.

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////
//
// Stop its own service, then stop the sub-managers' services.
//
//////////////////////////////////////////////////////////////////////
void ClientManager::stop() {
    __BEGIN_TRY

    // The loop below runs on the main thread; a signal handler only stores
    // the request, and run() returns on its next turn.
    ServerShutdown::request();

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////
// This is the client manager's main loop.
//////////////////////////////////////////////////////////////////////
void ClientManager::run() {
    __BEGIN_TRY

    Assert(m_pLoginPlayerManager != NULL);

    Timeval NextTime;
    getCurrentTime(NextTime);

    // Time GameWorldInfo and GameServerInfo were last reloaded
    Timeval ReloadNextTime = NextTime;
    // Interval at which GameWorldInfo and GameServerInfo are reloaded, in minutes
    int ReloadGap = de::kernelContext().config().getPropertyInt("ServerInfoReloadTime") * 60;
    ReloadNextTime.tv_sec += ReloadGap;

    NextTime.tv_sec += 10;

    Timeval dummyQueryTime;
    getCurrentTime(dummyQueryTime);

    while (!ServerShutdown::isRequested()) {
        usleep(1000); // FIX: lower the CPU usage, 100 microseconds raised to 1000 (1ms)

        beginProfileEx("LS_MAIN");

        beginProfileEx("LPM_POLL");
        m_pLoginPlayerManager->pollSockets();
        endProfileEx("LPM_POLL");

        beginProfileEx("LPM_EXCEPTION");
        m_pLoginPlayerManager->processExceptions();
        endProfileEx("LPM_EXCEPTION");

        beginProfileEx("LPM_INPUT");
        m_pLoginPlayerManager->processInputs();
        endProfileEx("LPM_INPUT");

        beginProfileEx("LPM_COMMAND");
        m_pLoginPlayerManager->processCommands();
        endProfileEx("LPM_COMMAND");

        beginProfileEx("LPM_OUTPUT");
        m_pLoginPlayerManager->processOutputs();
        endProfileEx("LPM_OUTPUT");

        beginProfileEx("LPM_HEARTBEAT");
        m_pLoginPlayerManager->retryRetiredPlayers();
        de::loginContext().reconnectLogins().heartbeat();
        endProfileEx("LPM_HEARTBEAT");

        endProfileEx("LS_MAIN");

        Timeval currentTime;
        getCurrentTime(currentTime);

        if (NextTime < currentTime) {
            (g_ProfileSampleManager.getProfileSampleSet())->outputProfileToFile("Profile", false, false);

            NextTime.tv_sec = currentTime.tv_sec + 10;
            NextTime.tv_usec = currentTime.tv_usec;

            // Reset the profile data every turn.
            // To measure time per interval rather than cumulative data...
            initProfileEx();

            g_PacketProfileManager.init();
        }

        if (ReloadNextTime < currentTime) {
            de::serverContext().worldInfos().load();

            de::loginContext().gameServerGroups().load();

            ReloadNextTime.tv_sec += ReloadGap;
        }

        // Run a meaningless query now and then so the DB connection does not time out.
        // by bezz. 2003.04.21
        if (dummyQueryTime < currentTime) {
            de::serverContext().database().executeDummyQuery(de::serverContext().database().getConnection("DARKEDEN"));

            dummyQueryTime = de::nextKeepAliveDeadline(dummyQueryTime, rand());
        }
    }

    __END_CATCH
}
