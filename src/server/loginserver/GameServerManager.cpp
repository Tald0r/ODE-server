//////////////////////////////////////////////////////////////////////
//
// Filename    : GameServerManager.cpp
// Written By  : Reiot
// Description :
//
//////////////////////////////////////////////////////////////////////

#include "GameServerManager.h"

#include <unistd.h>

#include <array>
#include <chrono>
#include <memory>

#include "DB.h"
#include "Datagram.h"
#include "DatagramPacket.h"
#include "KernelContext.h"
#include "LGKickCharacter.h"
#include "ListenerStartup.h"
#include "LoginDatagramSend.h"
#include "PacketDispatcher.h"
#include "Properties.h"
#include "RepeatedErrorReport.h"
#include "ServerContext.h"
#include "ServerPortSettings.h"
#include "ServerShutdown.h"
#include "SocketAPI.h"

//////////////////////////////////////////////////////////////////////
// constructor
//////////////////////////////////////////////////////////////////////
GameServerManager::GameServerManager() : m_pDatagramSocket(NULL) {
    __BEGIN_TRY

    const auto port = de::readServerPort(de::kernelContext().config(), "LoginServerUDPPort");
    de::retryListenerStartup(
        [&] {
            auto socket = std::make_unique<DatagramSocket>(port);
            // Idle UDP traffic must not keep the worker inside recvfrom during shutdown.
            SocketAPI::setsocketnonblocking_ex(socket->getSOCKET(), true);
            m_pDatagramSocket = socket.release();
        },
        [](const BindException& error) { cout << error.toString() << endl; }, "UDP");

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////
// destructor
//////////////////////////////////////////////////////////////////////
GameServerManager::~GameServerManager() noexcept {
    // The worker touches m_pDatagramSocket, so it must be joined before the
    // socket below is destroyed. A base destructor would run too late.
    stop();
    join();

    __BEGIN_TRY

    if (m_pDatagramSocket != NULL) {
        delete m_pDatagramSocket;
        m_pDatagramSocket = NULL;
    }

    __END_CATCH_NO_RETHROW
}

//////////////////////////////////////////////////////////////////////
// stop thread
//////////////////////////////////////////////////////////////////////
void GameServerManager::stop() {
    __BEGIN_TRY

    ManagedThread::stop();

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////
// main method
//////////////////////////////////////////////////////////////////////
void GameServerManager::run() {
    Properties& config = de::kernelContext().config();
    std::array<de::RepeatedErrorReport, 3> failures;
    constexpr std::array<const char*, 3> categories = {"protocol", "connect", "other"};
    const auto reportFailure = [&](std::size_t category, const Throwable& error) noexcept {
        try {
            if (const auto suppressed = failures[category].failure(de::RepeatedErrorReport::Clock::now())) {
                std::ostream diagnostics(cout.rdbuf());
                diagnostics << "GameServerManager UDP " << categories[category]
                            << " failure: suppressed_failures=" << *suppressed << " " << error.toString() << endl;
            }
        } catch (...) {
            // Packet cleanup must still run when formatting or output fails.
        }
    };

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

        while (!stopRequested()) {
            Datagram* pDatagram = NULL;
            DatagramPacket* pDatagramPacket = NULL;

            try {
                // Pull the datagram object out.
                pDatagram = m_pDatagramSocket->receive();

                if (pDatagram != NULL) // guards against some exceptions
                {
                    // Pull the datagram packet object out.
                    pDatagram->read(pDatagramPacket);

                    if (pDatagramPacket != NULL) {
                        // Process the datagram packet object that arrived.
                        PacketDispatcher::dispatch(pDatagramPacket, NULL);

                        // Delete the datagram packet object.
                        delete pDatagramPacket;
                        pDatagramPacket = NULL;
                    }

                    // Delete the datagram object.
                    delete pDatagram;
                    pDatagram = NULL;
                }
            } catch (ProtocolException& pe) {
                reportFailure(0, pe);

                // A protocol error in server-to-server communication means
                // a programming error or a hacking attempt.
                // For now the error is simply ignored.
                // throw Error( pe.toString() );
                delete pDatagramPacket;
                delete pDatagram;
            } catch (ConnectException& ce) {
                reportFailure(1, ce);

                // Hmm, what is this..
                // Treat it as an error for now.
                // throw Error( ce.toString() );
                delete pDatagramPacket;
                delete pDatagram;
            } catch (Throwable& t) {
                reportFailure(2, t);
                delete pDatagramPacket;
                delete pDatagram;
            }
            // Stop-aware idle: a shutdown request wakes this immediately
            // instead of costing another polling interval.
            pauseFor(std::chrono::milliseconds(1));
        }

        try {
            std::ostream diagnostics(cout.rdbuf());
            diagnostics << "GameServerManager thread exiting... " << endl;
        } catch (...) {
        }
    } catch (Throwable& t) {
        try {
            std::ostream diagnostics(cout.rdbuf());
            diagnostics << "GameServerManager thread exiting... : " << t.toString() << endl;
        } catch (...) {
        }
    }
    for (std::size_t category = 0; category < failures.size(); ++category) {
        try {
            if (const auto suppressed = failures[category].takeSuppressed(); suppressed != 0) {
                std::ostream diagnostics(cout.rdbuf());
                diagnostics << "GameServerManager UDP " << categories[category]
                            << " failures at shutdown: suppressed_failures=" << suppressed << endl;
            }
        } catch (...) {
        }
    }
}


//////////////////////////////////////////////////////////////////////
// send datagram to datagram-socket
//////////////////////////////////////////////////////////////////////
void GameServerManager::sendDatagram(Datagram* pDatagram) {
    __BEGIN_TRY

    try {
        m_pDatagramSocket->send(pDatagram);
    } catch (ConnectException& t) {
        cout << "GameServerManager::sendDatagram Exception Check!!" << endl;
        cout << t.toString() << endl;
        throw ConnectException("GameServerManager::sendDatagram failed");
    }

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////
// send datagram-packet to datagram-socket
//////////////////////////////////////////////////////////////////////
void GameServerManager::sendPacket(const string& host, uint port, const DatagramPacket* pPacket) {
    Assert(pPacket != nullptr);
    if (!de::sendLoginDatagram(
            host, port, *pPacket, [&](Datagram& datagram) { return m_pDatagramSocket->send(&datagram); }, cout))
        throw ConnectException("GameServerManager::sendPacket failed");
}
