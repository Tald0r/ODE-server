//////////////////////////////////////////////////////////////////////////////
// Filename    : GameServerManager.cpp
// Written by  : reiot@ewestsoft.com
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "GameServerManager.h"

#include <stdio.h>

#include <algorithm>
#include <chrono>
#include <memory>

#include "AcceptedServerConnection.h"
#include "Assert.h"
#include "DB.h"
#include "DescriptorTable.h"
#include "Guild.h"
#include "GuildManager.h"
#include "KeepAlive.h"
#include "KernelContext.h"
#include "ListenerStartup.h"
#include "Packet.h"
#include "PacketDiagnostics.h"
#include "Properties.h"
#include "ServerContext.h"
#include "ServerPortSettings.h"
#include "ServerShutdown.h"
#include "SharedContext.h"
#include "Socket.h"
#include "SocketAPI.h"


//////////////////////////////////////////////////////////////////////////////
// constructor
// Delete the sub-managers and the data members.
//////////////////////////////////////////////////////////////////////////////

namespace {
std::unique_ptr<ServerSocket> createSharedListener() {
    try {
        const auto port = de::readServerPort(de::kernelContext().config(), "TCPPort");
        std::unique_ptr<ServerSocket> socket;
        de::retryListenerStartup([&] { socket = std::make_unique<ServerSocket>(port); },
                                 [&](const BindException& error) {
                                     cout << "GameServerManager(" << port << ") : " << error.toString() << endl;
                                 },
                                 "TCP");

        return socket;
    } catch (NoSuchElementException& nsee) {
        // When the configuration file has no such element
        throw Error(nsee.toString());
    }
}
} // namespace

GameServerManager::GameServerManager() : GameServerManager(createSharedListener()) {}

GameServerManager::GameServerManager(std::unique_ptr<ServerSocket> listener)
    : m_pServerSocket(std::move(listener)), m_SocketID(INVALID_SOCKET), m_PollSet((int)nMaxGameServers),
      m_TimeoutMilliseconds(0), m_MinFD(-1), m_MaxFD(-1) {
    __BEGIN_TRY

    Assert(m_pServerSocket != nullptr);
    m_Mutex.setName("GameServerManager");
    m_pServerSocket->setNonBlocking();
    m_SocketID = m_pServerSocket->getSOCKET();

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////////////
// destructor
//////////////////////////////////////////////////////////////////////////////

GameServerManager::~GameServerManager() noexcept {
    // The worker owns the listening socket and the per-descriptor player
    // table, so it must be joined before those members go away. A base
    // destructor would run too late.
    stop();
    join();
    for (auto*& player : m_pGameServerPlayers) {
        delete player;
        player = nullptr;
    }
}


//////////////////////////////////////////////////////////////////////////////
// Initialize the sub-managers and its own members.
//////////////////////////////////////////////////////////////////////////////

void GameServerManager::init() {
    __BEGIN_TRY

    // The game server table is indexed by descriptor and every walk over it
    // is clamped to it, so a listener the table cannot hold would be skipped
    // by all of them and no connection could ever be accepted. There is
    // nothing to serve from in that state.
    if (!de::fitsDescriptorTable((int)m_SocketID, (int)nMaxGameServers))
        throw Error("listening socket descriptor does not fit the game server table");

    // Watch the server socket for an arriving connection and for out-of-band
    // data. (Writing to it need not be checked.)
    m_PollSet.watch(m_SocketID, de::DescriptorPollSet::kRead | de::DescriptorPollSet::kUrgent);

    // set min/max fd
    m_MinFD = m_MaxFD = m_SocketID;

    // How long a poll waits. This period should become an option later as
    // well.
    m_TimeoutMilliseconds = 0;

    __END_CATCH
}


void GameServerManager::run() {
    __BEGIN_TRY
    __BEGIN_DEBUG

    try {
        Timeval dummyQueryTime;
        getCurrentTime(dummyQueryTime);

        while (!stopRequested()) {
            try {
                // Stop-aware idle: a shutdown request wakes this immediately
                // instead of costing another polling interval.
                pauseFor(std::chrono::milliseconds(1));

                pollSockets();

                processInputs();

                processOutputs();
            } catch (Throwable& t) {
                filelog("SSGSManager.txt", "%s", t.toString().c_str());
            }

            processCommands();

            de::sharedContext().guilds().heartbeat();

            Timeval currentTime;
            getCurrentTime(currentTime);

            if (dummyQueryTime < currentTime) {
                de::serverContext().database().executeDummyQuery(
                    de::serverContext().database().getConnection("DARKEDEN"));

                dummyQueryTime = de::nextKeepAliveDeadline(dummyQueryTime, rand());
            }
        }
    } catch (Throwable& t) {
        filelog("sharedserverBug.txt", "%s", t.toString().c_str());
        throw;
    }

    __END_DEBUG
    __END_CATCH
}


//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void GameServerManager::broadcast(Packet* pPacket) {
    __BEGIN_TRY

    __ENTER_CRITICAL_SECTION(m_Mutex)

    try {
        const de::DescriptorRange walk = de::descriptorRange((int)m_MinFD, (int)m_MaxFD, (int)nMaxGameServers);
        for (int i = walk.first; i <= walk.last; i++) {
            if (i != m_SocketID && m_pGameServerPlayers[i] != NULL)
                m_pGameServerPlayers[i]->sendPacket(pPacket);
        }
    } catch (const ProtocolException&) {
        de::logPacketMetadata("SSException.log", "broadcast protocol failure", *pPacket);
    }

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void GameServerManager::broadcast(Packet* pPacket, Player* pPlayer) {
    __BEGIN_TRY

    __ENTER_CRITICAL_SECTION(m_Mutex)

    const de::DescriptorRange walk = de::descriptorRange((int)m_MinFD, (int)m_MaxFD, (int)nMaxGameServers);
    for (int i = walk.first; i <= walk.last; i++) {
        if (i != m_SocketID && m_pGameServerPlayers[i] != NULL && m_pGameServerPlayers[i] != pPlayer)
            m_pGameServerPlayers[i]->sendPacket(pPacket);
    }

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////////////
// Ask the kernel which descriptors are ready.
// When none is, no player needs processing.
//////////////////////////////////////////////////////////////////////////////
void GameServerManager::pollSockets() {
    __BEGIN_TRY

    // The membership is read and the answers are stored under the mutex that
    // guards the table, while the wait itself runs without it.
    __ENTER_CRITICAL_SECTION(m_Mutex)

    m_PollSet.fill();

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    // A failed wait, which is what an arriving signal makes of it, leaves
    // every descriptor unready, so this tick processes nothing and the next
    // one asks again.
    m_PollSet.wait(m_TimeoutMilliseconds);

    __ENTER_CRITICAL_SECTION(m_Mutex)

    // A game server added or removed while the wait ran keeps no answer from
    // it.
    m_PollSet.collect();

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////////////
// process all players' inputs
// When the server socket's read flag is set a new connection arrived,
// so handle it; when another socket's read flag is set a new packet
// arrived, so call that player's processInput().
//////////////////////////////////////////////////////////////////////////////
void GameServerManager::processInputs() {
    __BEGIN_TRY


    if (m_MinFD == -1 && m_MaxFD == -1) // no player exist
    {
        return;
    }

    const de::DescriptorRange walk = de::descriptorRange((int)m_MinFD, (int)m_MaxFD, (int)nMaxGameServers);
    for (int i = walk.first; i <= walk.last; i++) {
        if (m_PollSet.isReadable(i)) {
            if (i == m_SocketID) {
                //  The server socket means a new connection arrived.
                acceptNewConnection();
            } else {
                if (m_pGameServerPlayers[i] != NULL) {
                    GameServerPlayer* pGameServerPlayer = m_pGameServerPlayers[i];
                    Assert(pGameServerPlayer != NULL);
                    Assert(m_pGameServerPlayers[i] != NULL);

                    if (pGameServerPlayer->getSocket()->getSockError()) {
                        try {
                            // The connection is already gone, so the output buffer must not be flushed.
                            pGameServerPlayer->disconnect(DISCONNECTED);
                        } catch (Throwable& t) {
                            cerr << t.toString() << endl;
                        }

                        deleteGameServerPlayer(i);

                        delete pGameServerPlayer;
                    } else {
                        try {
                            pGameServerPlayer->processInput();
                        } catch (ConnectException& ce) {
                            // The socket is blocking, so no exception other than ConnectException
                            // and Error
                            // is thrown. On a connection error, log it, save the player's
                            // information (if it was loaded) and then delete the player object.
                            try {
                                pGameServerPlayer->disconnect();
                            } catch (Throwable& t) {
                                cerr << t.toString() << endl;
                            }

                            deleteGameServerPlayer(i);

                            delete pGameServerPlayer;
                        }
                    } // else
                } // else
            } // if
        }
    }


    __END_CATCH
}


//////////////////////////////////////////////////////////////////////////////
// process all players' commands
//////////////////////////////////////////////////////////////////////////////

void GameServerManager::processCommands() {
    __BEGIN_TRY
    __BEGIN_DEBUG


    if (m_MinFD == -1 && m_MaxFD == -1) // no player exist
    {
        return;
    }


    const de::DescriptorRange walk = de::descriptorRange((int)m_MinFD, (int)m_MaxFD, (int)nMaxGameServers);
    for (int i = walk.first; i <= walk.last; i++) {
        if (i != m_SocketID && m_pGameServerPlayers[i] != NULL) {
            GameServerPlayer* pGameServerPlayer = m_pGameServerPlayers[i];
            Assert(pGameServerPlayer != NULL);
            Assert(m_pGameServerPlayers[i] != NULL);

            if (pGameServerPlayer->getSocket()->getSockError()) {
                try {
                    // The connection is already gone, so the output buffer must not be flushed.
                    pGameServerPlayer->disconnect();
                } catch (Throwable& t) {
                    cerr << t.toString() << endl;
                }

                deleteGameServerPlayer(i);

                delete pGameServerPlayer;
            } else {
                try {
                    pGameServerPlayer->processCommand();
                } catch (ProtocolException& pe) {
                    try {
                        pGameServerPlayer->disconnect();
                        cout << pe.toString().c_str() << endl;
                    } catch (Throwable& t) {
                        cerr << t.toString() << endl;
                    }

                    deleteGameServerPlayer(i);

                    delete pGameServerPlayer;
                }
            }
        }
    }


    __END_DEBUG
    __END_CATCH
}


//////////////////////////////////////////////////////////////////////////////
// process all players' outputs
//////////////////////////////////////////////////////////////////////////////

void GameServerManager::processOutputs() {
    __BEGIN_TRY


    if (m_MinFD == -1 && m_MaxFD == -1) // no player exist
    {
        return;
    }


    const de::DescriptorRange walk = de::descriptorRange((int)m_MinFD, (int)m_MaxFD, (int)nMaxGameServers);
    for (int i = walk.first; i <= walk.last; i++) {
        if (m_PollSet.isWritable(i)) {
            if (i == m_SocketID)
                throw IOException("server socket reported ready to write.");

            if (m_pGameServerPlayers[i] != NULL) {
                GameServerPlayer* pGameServerPlayer = m_pGameServerPlayers[i];

                Assert(pGameServerPlayer != NULL);
                Assert(m_pGameServerPlayers[i] != NULL);

                if (pGameServerPlayer->getSocket()->getSockError()) {
                    try {
                        // The connection is already gone, so the output buffer must not be flushed.
                        pGameServerPlayer->disconnect(DISCONNECTED);
                    } catch (Throwable& t) {
                        cerr << t.toString() << endl;
                    }

                    deleteGameServerPlayer(i);

                    delete pGameServerPlayer;
                } else {
                    try {
                        pGameServerPlayer->processOutput();
                    } catch (ConnectException& ce) {
                        StringStream msg;
                        msg << "DISCONNECT " << pGameServerPlayer->getID() << "(" << ce.toString() << ")";

                        try {
                            // The connection is already gone, so the output buffer must not be flushed.
                            pGameServerPlayer->disconnect(DISCONNECTED);
                        } catch (Throwable& t) {
                            cerr << t.toString() << endl;
                        }

                        deleteGameServerPlayer(i);

                        delete pGameServerPlayer;
                    } catch (ProtocolException& cp) {
                        StringStream msg;
                        msg << "DISCONNECT " << pGameServerPlayer->getID() << "(" << cp.toString() << ")";

                        // The connection is already gone, so the output buffer must not be flushed.

                        try {
                            pGameServerPlayer->disconnect(DISCONNECTED);
                        } catch (Throwable& t) {
                            cerr << t.toString() << endl;
                        }

                        deleteGameServerPlayer(i);

                        delete pGameServerPlayer;
                    }
                }
            }
        }
    }


    __END_CATCH
}


//////////////////////////////////////////////////////////////////////////////
// process all players' exceptions
// There is no plan to send OOB data at the moment.
// So if OOB data does arrive, treat it as an error and cut the connection.
//////////////////////////////////////////////////////////////////////////////

void GameServerManager::processExceptions() {
    __BEGIN_TRY


    if (m_MinFD == -1 && m_MaxFD == -1) // no player exist
    {
        return;
    }


    const de::DescriptorRange walk = de::descriptorRange((int)m_MinFD, (int)m_MaxFD, (int)nMaxGameServers);
    for (int i = walk.first; i <= walk.last; i++) {
        if (m_PollSet.isUrgent(i)) {
            if (i != m_SocketID) {
                if (m_pGameServerPlayers[i] != NULL) {
                    GameServerPlayer* pGameServerPlayer = m_pGameServerPlayers[i];
                    Assert(pGameServerPlayer != NULL);
                    Assert(i != m_SocketID);
                    Assert(m_pGameServerPlayers[i] != NULL);
                    StringStream msg;
                    msg << "OOB from " << pGameServerPlayer->toString();

                    try {
                        pGameServerPlayer->disconnect();
                    } catch (Throwable& t) {
                    }

                    deleteGameServerPlayer(i);

                    delete pGameServerPlayer;
                }
            } else {
            }
        }
    }


    __END_CATCH
}


//////////////////////////////////////////////////////////////////////////////
// The socket the poll reported ready is accepted here.
//////////////////////////////////////////////////////////////////////////////
void GameServerManager::acceptNewConnection() {
    __BEGIN_TRY

    std::unique_ptr<Socket> client;
    try {
        client.reset(m_pServerSocket->accept());
    } catch (Throwable&) {
    }
    if (!client)
        return;

    try {
        client = de::prepareAcceptedServerConnection(std::move(client));
        // Allocation precedes release. Once construction begins, Player's
        // base destructor owns cleanup even if stream creation fails.
        std::unique_ptr<GameServerPlayer> player(new GameServerPlayer(client.release()));
        try {
            addGameServerPlayer(player.get());
        } catch (OutOfBoundException&) {
            const auto* socket = player->getSocket();
            filelog("SSGSManager.txt", "REFUSED %s:%u : socket descriptor %d does not fit the game server table",
                    socket->getHost().c_str(), socket->getPort(), (int)socket->getSOCKET());
            return;
        }
        player.release(); // The table adopts only after successful publication.
    } catch (Throwable&) {
    } catch (std::exception&) {
    } catch (...) {
    }

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////
//
// Add the player object for the new connection to the IPM.
//
//////////////////////////////////////////////////////////////////////
void GameServerManager::addGameServerPlayer(GameServerPlayer* pGameServerPlayer) {
    __BEGIN_TRY

    __ENTER_CRITICAL_SECTION(m_Mutex)

    SOCKET fd = pGameServerPlayer->getSocket()->getSOCKET();

    // The table is indexed by the descriptor, so one it cannot hold is refused
    // rather than stored past its end.
    if (!de::fitsDescriptorTable((int)fd, (int)nMaxGameServers))
        throw OutOfBoundException();

    if (m_pGameServerPlayers[fd] != nullptr)
        throw DuplicatedException();

    // Readjust m_MinFD and m_MaxFD.
    m_MinFD = min(fd, m_MinFD);
    m_MaxFD = max(fd, m_MaxFD);

    // Watch the new descriptor. It is reported ready no earlier than the next
    // poll.
    m_PollSet.watch(fd, de::DescriptorPollSet::kRead | de::DescriptorPollSet::kWrite | de::DescriptorPollSet::kUrgent);

    m_pGameServerPlayers[fd] = pGameServerPlayer;

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////
//
// Remove a given player from the IPM.
//
//////////////////////////////////////////////////////////////////////
void GameServerManager::deleteGameServerPlayer(SOCKET fd) {
    __BEGIN_TRY

    __ENTER_CRITICAL_SECTION(m_Mutex)

    // The table is indexed by the descriptor, so one it cannot hold is
    // refused rather than cleared past its end.
    if (!de::fitsDescriptorTable((int)fd, (int)nMaxGameServers))
        throw OutOfBoundException();

    m_pGameServerPlayers[fd] = NULL;

    // Readjust m_MinFD and m_MaxFD.
    // The fd == m_MinFD && fd == m_MaxFD case is handled by the first if.
    if (fd == m_MinFD) {
        // Find the smallest fd from the front.
        // Note that the m_MinFD slot is NULL at this point.
        const de::DescriptorRange walk = de::descriptorRange((int)m_MinFD, (int)m_MaxFD, (int)nMaxGameServers);
        int i = walk.first;
        for (i = walk.first; i <= walk.last; i++) {
            if (m_pGameServerPlayers[i] != NULL || i == m_SocketID) {
                m_MinFD = i;
                break;
            }
        }

        // When no suitable m_MinFD was found,
        // this is the m_MinFD == m_MaxFD case.
        // Set both to -1 then.
        if (i > walk.last)
            m_MinFD = m_MaxFD = -1;
    } else if (fd == m_MaxFD) {
        // Find the largest fd from the back.
        // Watch out for SocketID! (for SocketID the Player pointer is NULL.)
        const de::DescriptorRange walk = de::descriptorRange((int)m_MinFD, (int)m_MaxFD, (int)nMaxGameServers);
        int i = walk.last;
        for (i = walk.last; i >= walk.first; i--) {
            if (m_pGameServerPlayers[i] != NULL || i == m_SocketID) {
                m_MaxFD = i;
                break;
            }
        }

        // When no suitable m_MinFD was found,
        if (i < walk.first) {
            throw UnknownError("m_MinFD & m_MaxFD problem.");
        }
    }

    // Stop watching the descriptor. This also drops the readiness the last poll
    // reported for it, because otherwise an object that is gone could still be
    // processed later.
    m_PollSet.unwatch(fd);

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    __END_CATCH
}
