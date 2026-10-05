//////////////////////////////////////////////////////////////////////
//
// Filename    : SharedServerManager.cpp
// Written By  : Reiot
// Description :
//
//////////////////////////////////////////////////////////////////////

// include files
#include "SharedServerManager.h"

#include <unistd.h>

#include "Assert.h"
#include "DB.h"
#include "GSRequestGuildInfo.h"
#include "KeepAlive.h"
#include "KernelContext.h"
#include "OutboundServerConnection.h"
#include "Properties.h"
#include "ServerContext.h"
#include "ServerPortSettings.h"
#include "SharedServerClient.h"
#include "ThreadManager.h"
#include "ThreadPool.h"
#include "Timeval.h"

//////////////////////////////////////////////////////////////////////
// constructor
//////////////////////////////////////////////////////////////////////
SharedServerManager::SharedServerManager()

{
    __BEGIN_TRY

    m_pSharedServerClient = NULL;

    m_Mutex.setName("SharedServerManager");

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////
// destructor
//////////////////////////////////////////////////////////////////////
SharedServerManager::~SharedServerManager() noexcept

{
    stop();
    join();
    __BEGIN_TRY

    SAFE_DELETE(m_pSharedServerClient);

    __END_CATCH_NO_RETHROW
}

//////////////////////////////////////////////////////////////////////
// stop thread
//////////////////////////////////////////////////////////////////////
void SharedServerManager::stop()

{
    __BEGIN_TRY

    ManagedThread::stop();

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////
// main method
//////////////////////////////////////////////////////////////////////
void SharedServerManager::run()

{
    Properties& config = de::kernelContext().config();

    try {
        string host = config.getProperty("DB_HOST");
        string db = config.getProperty("DB_DB");
        string user = config.getProperty("DB_USER");
        string password = config.getProperty("DB_PASSWORD");
        uint port = 0;
        if (config.hasKey("DB_PORT"))
            port = config.getPropertyInt("DB_PORT");

        Connection* pConnection = new Connection(host, db, user, password, port);
        de::serverContext().database().addConnection((int)(long)Thread::self(), pConnection);
        cout << "************************************************************************" << endl;
        cout << "OPEN LOGIN DB" << endl;
        cout << "************************************************************************" << endl;

        Timeval dummyQueryTime;
        getCurrentTime(dummyQueryTime);

        while (!stopRequested()) {
            usleep(1000); // FIX: reduce CPU usage

            // Try to connect if not connected.
            if (m_pSharedServerClient == NULL) {
                try {
                    string SharedServerIP = config.getProperty("SharedServerIP");
                    const auto SharedServerPort = de::readServerPort(config, "SharedServerPort");
                    auto socket = de::connectOutboundServer(SharedServerIP, SharedServerPort);

                    __ENTER_CRITICAL_SECTION(m_Mutex)
                    // Allocation happens before release; Player owns the socket
                    // once construction starts, including failed stream setup.
                    m_pSharedServerClient = new SharedServerClient(socket.release());
                    __LEAVE_CRITICAL_SECTION(m_Mutex)

                    cout << "connection to sharedserver established" << endl;

                    // Request the guild information.
                    GSRequestGuildInfo gsRequestGuildInfo;
                    m_pSharedServerClient->sendPacket(&gsRequestGuildInfo);
                } catch (Throwable& t) {
                    cout << "connect to sharedserver fail" << endl;

                    __ENTER_CRITICAL_SECTION(m_Mutex)

                    try {
                        SAFE_DELETE(m_pSharedServerClient);
                    } catch (Throwable& t) {
                        diagnosticFilelog("sharedServerClient.txt", "[1]%s", t.toString().c_str());
                    }
                    __LEAVE_CRITICAL_SECTION(m_Mutex)

                    // Wait before the next connection attempt.
                    usleep(500000);
                }
            }

            // Process I/O if the socket is connected.
            __ENTER_CRITICAL_SECTION(m_Mutex)

            if (m_pSharedServerClient != NULL) {
                try {
                    m_pSharedServerClient->processInput();
                    m_pSharedServerClient->processOutput();
                    m_pSharedServerClient->processCommand();
                } catch (Throwable& t) {
                    cout << t.toString().c_str() << endl;

                    try {
                        SAFE_DELETE(m_pSharedServerClient);
                    } catch (Throwable& t) {
                        diagnosticFilelog("sharedServerClient.txt", "[2]%s", t.toString().c_str());
                    }
                }
            }

            __LEAVE_CRITICAL_SECTION(m_Mutex)

            // dummy query
            Timeval currentTime;
            getCurrentTime(currentTime);

            if (dummyQueryTime < currentTime) {
                de::serverContext().database().executeDummyQuery(pConnection);

                dummyQueryTime = de::nextKeepAliveDeadline(dummyQueryTime, rand());
            }
        }

    } catch (Throwable& t) {
        diagnosticFilelog("SHAREDSERVERMANAGER.log", "SharedServerManager::run() 4 : %s", t.toString().c_str());

        cerr << t.toString() << endl;
    }
}

//////////////////////////////////////////////////////////////////////
// send packet to shared server
//////////////////////////////////////////////////////////////////////
void SharedServerManager::sendPacket(Packet* pPacket) {
    __ENTER_CRITICAL_SECTION(m_Mutex)

    if (m_pSharedServerClient != NULL) {
        m_pSharedServerClient->sendPacket(pPacket);
    }

    __LEAVE_CRITICAL_SECTION(m_Mutex)
}
