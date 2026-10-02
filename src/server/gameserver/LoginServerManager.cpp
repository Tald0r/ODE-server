//////////////////////////////////////////////////////////////////////
//
// Filename    : LoginServerManager.cpp
// Written By  : Reiot
// Description :
//
//////////////////////////////////////////////////////////////////////

// include files
#include "LoginServerManager.h"

#include <unistd.h>

#include <memory>

#include "Assert.h"
#include "DB.h"
#include "Datagram.h"
#include "DatagramPacket.h"
#include "GameContext.h"
#include "KeepAlive.h"
#include "KernelContext.h"
#include "ListenerStartup.h"
#include "PacketDispatcher.h"
#include "Properties.h"
#include "ServerContext.h"
#include "ServerPortSettings.h"
#include "ThreadManager.h"
#include "ThreadPool.h"
#include "TimeChecker.h"
#include "Timeval.h"

//////////////////////////////////////////////////////////////////////
// constructor
//////////////////////////////////////////////////////////////////////
LoginServerManager::LoginServerManager() : m_pDatagramSocket(NULL) {
    __BEGIN_TRY

    Properties& config = de::kernelContext().config();

    m_Mutex.setName("LoginServerManager");

    const auto port = de::readServerPort(config, "GameServerUDPPort");
    de::retryListenerStartup(
        [&] {
            auto socket = std::make_unique<DatagramSocket>(port);
            SocketAPI::setsocketnonblocking_ex(socket->getSOCKET(), true);
            m_pDatagramSocket = socket.release();
        },
        [&](const BindException& error) {
            cout << "LoginServerManager(" << port << ") : " << error.toString() << endl;
        },
        "UDP");

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////
// destructor
//////////////////////////////////////////////////////////////////////
LoginServerManager::~LoginServerManager() noexcept {
    stop();
    join();
    __BEGIN_TRY

    SAFE_DELETE(m_pDatagramSocket);

    __END_CATCH_NO_RETHROW
}

//////////////////////////////////////////////////////////////////////
// stop thread
//////////////////////////////////////////////////////////////////////
void LoginServerManager::stop() {
    __BEGIN_TRY

    ManagedThread::stop();

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////
// main method
//////////////////////////////////////////////////////////////////////
void LoginServerManager::run() {
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
        cout << "************************************************************************" << endl;
        cout << "************************************************************************" << endl;
        cout << "************************************************************************" << endl;
        cout << "OPEN LOGIN DB" << endl;
        cout << "************************************************************************" << endl;
        cout << "************************************************************************" << endl;
        cout << "************************************************************************" << endl;

        Timeval dummyQueryTime;
        getCurrentTime(dummyQueryTime);

        while (!stopRequested()) {
            usleep(1000); // Reduce CPU usage: sleep 1ms rather than 100 microseconds.

            Datagram* pDatagram = NULL;
            DatagramPacket* pDatagramPacket = NULL;

            try {
                // Pull out a datagram object.
                pDatagram = m_pDatagramSocket->receive();

                if (pDatagram != NULL) // Avoids some exceptions.
                {
                    // cout << "[Datagram] " << pDatagram->getHost() << ":" << pDatagram->getPort() << endl;
                    pDatagram->read(pDatagramPacket);

                    if (pDatagramPacket != NULL) {
                        // cout << "[DatagramPacket] " << pDatagram->getHost() << ":" << pDatagram->getPort() << endl;
                        //  Execute the datagram packet object that was pulled out.
                        __ENTER_CRITICAL_SECTION(m_Mutex)

                        PacketDispatcher::dispatch(pDatagramPacket, NULL);

                        __LEAVE_CRITICAL_SECTION(m_Mutex)

                        // Delete the datagram packet object.
                        SAFE_DELETE(pDatagramPacket);
                    }

                    // Delete the datagram object.
                    SAFE_DELETE(pDatagram);
                }
            } catch (ProtocolException& pe) {
                cerr << "----------------------------------------------------------------------" << endl;
                cerr << "GameServerManager::run Exception Check(ProtocolException)" << endl;
                cerr << pe.toString() << endl;
                cerr << "----------------------------------------------------------------------" << endl;
                SAFE_DELETE(pDatagramPacket);
                SAFE_DELETE(pDatagram);

                // A protocol error in server-to-server communication means
                // either a programming bug or a hacking attempt.
                // Only the former applies for now, so it is treated as an error.
                // throw Error(pe.toString());

                filelog("LOGINSERVERMANAGER.log", "LoginServerManager::run() 1 : %s", pe.toString().c_str());
            } catch (ConnectException& ce) {
                cerr << "----------------------------------------------------------------------" << endl;
                cerr << "GameServerManager::run Exception Check(ConnectException)" << endl;
                cerr << ce.toString() << endl;
                cerr << "----------------------------------------------------------------------" << endl;
                SAFE_DELETE(pDatagramPacket);
                SAFE_DELETE(pDatagram);

                // Unclear what causes this.
                // Treated as an error for now.
                // throw Error(ce.toString());

                filelog("LOGINSERVERMANAGER.log", "LoginServerManager::run() 2 : %s", ce.toString().c_str());
            } catch (Throwable& t) {
                cerr << "----------------------------------------------------------------------" << endl;
                cerr << "GameServerManager::run Exception Check(Throwable)" << endl;
                cerr << t.toString() << endl;
                cerr << "----------------------------------------------------------------------" << endl;
                SAFE_DELETE(pDatagramPacket);
                SAFE_DELETE(pDatagram);

                filelog("LOGINSERVERMANAGER.log", "LoginServerManager::run() 3 : %s", t.toString().c_str());
            }

            usleep(1000); // Reduce CPU usage.

            Timeval currentTime;
            getCurrentTime(currentTime);

            if (dummyQueryTime < currentTime) {
                de::serverContext().database().executeDummyQuery(pConnection);

                dummyQueryTime = de::nextKeepAliveDeadline(dummyQueryTime, rand());
            }

            // Update the time checker.
            // ClientManager does not run it when no user enters ClientManager,
            // so it is done here as well.
            de::gameContext().timeChecker().heartbeat();
        }
    } catch (Throwable& t) {
        filelog("LOGINSERVERMANAGER.log", "LoginServerManager::run() 4 : %s", t.toString().c_str());

        cerr << t.toString() << endl;
    }
}

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
void LoginServerManager::sendDatagram(Datagram* pDatagram) {
    __BEGIN_TRY

    m_pDatagramSocket->send(pDatagram);

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////
// send datagram packet to login server
//////////////////////////////////////////////////////////////////////
void LoginServerManager::sendPacket(const string& host, uint port, DatagramPacket* pPacket) {
    __BEGIN_TRY
    __BEGIN_DEBUG

    try {
        // Set up a datagram object and specify the host and port of the peer to send to.
        Datagram datagram;

        datagram.setHost(host);
        datagram.setPort(port);

        // Put the datagram packet into the datagram.
        datagram.write(pPacket);

        // Send the datagram through the datagram socket.
        m_pDatagramSocket->send(&datagram);
    } catch (Throwable& t) {
        // cerr << "====================================================================" << endl;
        // cerr << t.toString() << endl;
        // cerr << "====================================================================" << endl;
    }

    __END_DEBUG
    __END_CATCH
}
