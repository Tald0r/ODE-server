//////////////////////////////////////////////////////////////////////////////
// Filename    : IncomingPlayerManager.cpp
// Written by  : reiot@ewestsoft.com
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "IncomingPlayerManager.h"

#include <stdio.h>

#include <algorithm>
#include <memory>
#include <mutex>

#include "AcceptedServerConnection.h"
#include "Assert.h"
#include "CreatureUtil.h"
#include "DB.h"
#include "DescriptorTable.h"
#include "Encrypter.h"
#include "GCUpdateInfo.h"
#include "GLIncomingConnection.h"
#include "GLKickVerify.h"
#include "GameConnection.h"
#include "GameContext.h"
#include "GamePlayer.h"
#include "GamePlayerHandoff.h"
#include "ListenerStartup.h"
#include "LogDef.h"
#include "LoginServerManager.h"
#include "MasterLairManager.h"
#include "PKZoneInfoManager.h"
#include "PacketUtil.h"
#include "PaySystem.h"
#include "Player.h"
#include "PlayerCreature.h"
#include "PlayerMailbox.h"
#include "Properties.h"
#include "ServerPortSettings.h"
#include "Socket.h"
#include "SocketAPI.h"
#include "ZoneGroup.h"
#include "ZoneInfoManager.h"
#include "ZonePlayerManager.h"
#include "repository/SessionRepository.h"

// #include "UserGateway.h"
#include "KernelContext.h"
#include "ServerContext.h"
#include "SystemAvailabilitiesManager.h"


//////////////////////////////////////////////////////////////////////////////
// constructor
// Create the sub-managers and data members.
//////////////////////////////////////////////////////////////////////////////

namespace {
std::unique_ptr<ServerSocket> createIncomingListener() {
    try {
        const auto port = de::readServerPort(de::kernelContext().config(), "TCPPort");
        std::unique_ptr<ServerSocket> listener;
        de::retryListenerStartup([&] { listener = std::make_unique<ServerSocket>(port); },
                                 [&](const BindException& error) {
                                     cout << "IncomingPlayerManager(" << port << ") : " << error.toString() << endl;
                                 },
                                 "TCP");
        return listener;
    } catch (NoSuchElementException& error) {
        throw Error(error.toString());
    }
}
} // namespace

IncomingPlayerManager::IncomingPlayerManager()
    : IncomingPlayerManager(createIncomingListener(), de::gameContext(),
                            [] { return de::gameContext().variables().getVariable(LOG_INCOMING_CONNECTION) != 0; }) {}

IncomingPlayerManager::IncomingPlayerManager(std::unique_ptr<ServerSocket> listener, de::GameContext& context,
                                             std::function<bool()> logConnections)
    : m_pServerSocket(std::move(listener)), m_SocketID(INVALID_SOCKET), m_PollSet((int)nMaxPlayers),
      m_TimeoutMilliseconds(0), m_MinFD(-1), m_MaxFD(-1), m_CheckValue(0), m_Context(context),
      m_LogConnections(std::move(logConnections)) {
    Assert(m_pServerSocket != nullptr);
    m_Mutex.setName("IncomingPlayerManager");
    m_MutexOut.setName("IncomingPlayerManagerOut");
    m_pServerSocket->setNonBlocking(true);
    m_SocketID = m_pServerSocket->getSOCKET();
    if (!de::fitsDescriptorTable((int)m_SocketID, (int)nMaxPlayers))
        throw Error("listening socket descriptor does not fit the player table");
    m_PollSet.watch(m_SocketID, de::DescriptorPollSet::kRead | de::DescriptorPollSet::kUrgent);
    m_MinFD = m_MaxFD = m_SocketID;
    m_pConnectionInfoManager = std::make_unique<ConnectionInfoManager>();
    m_PreviousConnectionInfoManager = m_Context.exchangeConnectionInfoManager(m_pConnectionInfoManager.get());
}

// All users and queue producers must have stopped before the manager's scope ends.
IncomingPlayerManager::~IncomingPlayerManager() noexcept(false) {
    releasePlayers(false);
    m_Context.setConnectionInfoManager(m_PreviousConnectionInfoManager);
}


//////////////////////////////////////////////////////////////////////////////
// Initialize the sub-managers and data members.
//////////////////////////////////////////////////////////////////////////////

void IncomingPlayerManager::init()

{
    __BEGIN_TRY

    Properties& config = de::kernelContext().config();

    m_ProxyAcceptor = de::ProxyAcceptor::fromConfig(config);

    string dist_host = config.getProperty("UI_DB_HOST");
    string dist_db = "DARKEDEN";
    string dist_user = config.getProperty("UI_DB_USER");
    string dist_password = config.getProperty("UI_DB_PASSWORD");
    uint dist_port = 0;
    if (config.hasKey("UI_DB_PORT"))
        dist_port = config.getPropertyInt("UI_DB_PORT");

    Connection* pDistConnection = new Connection(dist_host, dist_db, dist_user, dist_password, dist_port);
    de::serverContext().database().addDistConnection(((int)(long)Thread::self()), pDistConnection);
    cout << "******************************************************" << endl;
    cout << " THREAD CONNECT UIIRIBUTION DB " << endl;
    cout << " TID Number = " << (int)(long)Thread::self() << endl;
    cout << "******************************************************" << endl;

    // Tidy up Player.LogOn: clear the PC-room records of everyone this
    // server left in GAME, then flip them to LOGOFF. Billing~ by sigi 2002.5.31
    SessionRepository& repository = defaultSessionRepository();

    vector<string> inGame =
        repository.loadPlayersInGame(config.getPropertyInt("WorldID"), config.getPropertyInt("ServerID"));

    for (size_t p = 0; p < inGame.size(); p++) {
        repository.deletePCRoomUser(inGame[p]);
    }

    repository.logOffPlayersOfServer(config.getPropertyInt("WorldID"), config.getPropertyInt("ServerID"));

    repository.deleteUserIPsOfServer(config.getPropertyInt("ServerID"));

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void IncomingPlayerManager::copyPlayers()

{
    __BEGIN_TRY

    __ENTER_CRITICAL_SECTION(m_Mutex)

    PlayerManager::copyPlayers();

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void IncomingPlayerManager::broadcast(Packet* pPacket)

{
    __BEGIN_TRY

    __ENTER_CRITICAL_SECTION(m_Mutex)

    const de::DescriptorRange walk = de::descriptorRange((int)m_MinFD, (int)m_MaxFD, (int)nMaxPlayers);
    for (int i = walk.first; i <= walk.last; i++) {
        if (i != m_SocketID && m_pPlayers[i] != NULL)
            m_pPlayers[i]->sendPacket(pPacket);
    }

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////////////
// Ask the kernel which descriptors are ready.
// When none is, the steps that follow find nothing to process.
//////////////////////////////////////////////////////////////////////////////
void IncomingPlayerManager::pollSockets() {
    __BEGIN_TRY

    // The membership is read and the answers are stored under the mutex that
    // guards the table, while the wait itself runs without it, so a thread
    // looking a player up is never held for the length of a poll.
    __ENTER_CRITICAL_SECTION(m_Mutex)

    m_PollSet.fill();

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    // A failed wait, which is what an arriving signal makes of it, leaves
    // every descriptor unready, so this tick processes nothing and the next
    // one asks again.
    m_PollSet.wait(m_TimeoutMilliseconds);

    __ENTER_CRITICAL_SECTION(m_Mutex)

    // A player added or removed while the wait ran keeps no answer from it.
    m_PollSet.collect();

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////////////
// process all players' inputs
// When the server socket's read flag is on, a new connection has arrived and
// is handled; when another socket's read flag is on, a new packet has
// arrived, so that player's processInput() is called.
//////////////////////////////////////////////////////////////////////////////
void IncomingPlayerManager::processInputs() {
    __BEGIN_TRY

    // Gateway connections whose header arrived; acceptNewConnection() owns
    // each socket it is handed.
    if (m_ProxyAcceptor) {
        for (auto& client : m_ProxyAcceptor->poll())
            acceptNewConnection(client.release());
    }

    if (m_MinFD == -1 && m_MaxFD == -1) // no player exist
    {
        return;
    }

    const de::DescriptorRange walk = de::descriptorRange((int)m_MinFD, (int)m_MaxFD, (int)nMaxPlayers);
    for (int i = walk.first; i <= walk.last; i++) {
        if (m_PollSet.isReadable(i)) {
            if (i == m_SocketID) {
                //  The server socket means a new connection has arrived.
                // by sigi. 2002.12.8
                for (int i = 0; i < 50; i++) // Accept only 50 of them.
                {
                    if (!acceptNewConnection())
                        break;
                }
            } else {
                if (m_pPlayers[i] != NULL) {
                    GamePlayer* pTempPlayer = dynamic_cast<GamePlayer*>(m_pPlayers[i]);
                    Assert(pTempPlayer != NULL);
                    Assert(m_pPlayers[i] != NULL);

                    if (pTempPlayer->getSocket()->getSockError()) {
                        FILELOG_INCOMING_CONNECTION("ICMPISocketErr.log", "[Input] PlayerID : %s, PlayerStatus : %d",
                                                    pTempPlayer->getID().c_str(), (int)pTempPlayer->getPlayerStatus());

                        try {
                            // The connection is already closed, so the output buffer must not be flushed.
                            pTempPlayer->disconnect(DISCONNECTED);
                        } catch (Throwable& t) {
                            cerr << t.toString() << endl;
                        }


                        // by sigi. 2002.12.30
                        //						UserGateway::getInstance()->passUser(
                        // UserGateway::USER_OUT_INCOMING_INPUT_ERROR );

                        // No player means it was removed somewhere else,
                        // but nowhere else calls deletePlayer.
                        // Only each PlayerManager may delete a Player.
                        // So it disappeared in ProcessCommand.
                        try {
                            deletePlayer(i);
                            deleteQueuePlayer(pTempPlayer);
                        } catch (Throwable& t) {
                            filelog("deletePlayer.log", "called in IncomingPlayerManager.cpp line 389");
                            filelog("IncomingPlayerManager.txt", "Exception catched here: 0");
                        }

                        Creature* pCreature = pTempPlayer->getCreature();
                        if (pCreature != NULL)
                            pCreature->setValue(1);
                        SAFE_DELETE(pTempPlayer);
                    } else {
                        try {
                            pTempPlayer->processInput();
                        } catch (ConnectException& ce) {
                            FILELOG_INCOMING_CONNECTION("ICMPIConectionErr.log",
                                                        "[Input] %s, PlayerID : %s, PlayerStatus : %d",
                                                        ce.toString().c_str(), pTempPlayer->getID().c_str(),
                                                        (int)pTempPlayer->getPlayerStatus());
                            // The socket is blocking, so no exception other than ConnectException and Error occurs.
                            // On disconnect, log it, save the player information (if it was loaded) and
                            // delete the player object.
                            try {
                                pTempPlayer->disconnect();
                            } catch (Throwable& t) {
                                cerr << t.toString() << endl;
                            }

                            // by sigi. 2002.12.30
                            //							UserGateway::getInstance()->passUser(
                            // UserGateway::USER_OUT_INCOMING_INPUT_DISCONNECT );

                            // No player means it was removed somewhere else,
                            // but nowhere else calls deletePlayer.
                            // Only each PlayerManager may delete a Player.
                            // So it disappeared in ProcessCommand.
                            try {
                                deletePlayer(i);
                                deleteQueuePlayer(pTempPlayer);
                            } catch (Throwable& t) {
                                filelog("deletePlayer.log", "called in IncomingPlayerManager.cpp line 430");
                                filelog("IncomingPlayerManager.txt", "Exception catched here: 1");
                            }

                            Creature* pCreature = pTempPlayer->getCreature();
                            if (pCreature != NULL)
                                pCreature->setValue(2);
                            SAFE_DELETE(pTempPlayer);
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

void IncomingPlayerManager::processCommands() {
    __BEGIN_TRY
    __BEGIN_DEBUG

    if (m_MinFD == -1 && m_MaxFD == -1) // no player exist
    {
        return;
    }

    const de::DescriptorRange walk = de::descriptorRange((int)m_MinFD, (int)m_MaxFD, (int)nMaxPlayers);
    for (int i = walk.first; i <= walk.last; i++) {
        if (i != m_SocketID && m_pPlayers[i] != NULL) {
            GamePlayer* pTempPlayer = dynamic_cast<GamePlayer*>(m_pPlayers[i]);
            Assert(pTempPlayer != NULL);
            Assert(m_pPlayers[i] != NULL);

            if (pTempPlayer->getSocket()->getSockError()) {
                FILELOG_INCOMING_CONNECTION("ICMPCSocketErr.log", "[Command] PlayerID : %s, PlayerStatus : %d",
                                            pTempPlayer->getID().c_str(), (int)pTempPlayer->getPlayerStatus());
                try {
                    // The connection is already closed, so the output buffer must not be flushed.
                    pTempPlayer->disconnect();
                } catch (Throwable& t) {
                    cerr << t.toString() << endl;
                }

                // by sigi. 2002.12.30
                //				UserGateway::getInstance()->passUser( UserGateway::USER_OUT_INCOMING_COMMAND_ERROR );

                // No player means it was removed somewhere else,
                // but nowhere else calls deletePlayer.
                // Only each PlayerManager may delete a Player.
                // So it disappeared in ProcessCommand.
                try {
                    deletePlayer(i);
                    deleteQueuePlayer(pTempPlayer);
                } catch (Throwable& t) {
                    filelog("deletePlayer.log", "called in IncomingPlayerManager.cpp line 503");
                    filelog("IncomingPlayerManager.txt", "Exception catched here: 2");
                }

                Creature* pCreature = pTempPlayer->getCreature();
                if (pCreature != NULL)
                    pCreature->setValue(3);
                try {
                    SAFE_DELETE(pTempPlayer);
                } catch (Throwable& t) {
                    cerr << t.toString() << endl;
                    diagnosticFilelog("Destructer.log", "IncommingPlayerManager.cpp +509 : %s", t.toString().c_str());
                }
            } else {
                // This manager owns pTempPlayer while it logs in or changes
                // zone: run the player-scoped commands other threads posted
                // for it (PlayerMailbox.h); zone-scoped ones wait for a zone
                // thread.
                de::drainPlayerMailboxOnMainThread(*pTempPlayer);

                try {
                    pTempPlayer->processCommand(false);
                } catch (ProtocolException& pe) {
                    try {
                        FILELOG_INCOMING_CONNECTION(
                            "ICMPCProtocolExcpt.log", "[Command] %s, PlayerID : %s, PlayerStatus : %d",
                            pe.toString().c_str(), pTempPlayer->getID().c_str(), (int)pTempPlayer->getPlayerStatus());
                        pTempPlayer->disconnect();
                    } catch (Throwable& t) {
                        cerr << t.toString() << endl;
                    }

                    // by sigi. 2002.12.30
                    //					UserGateway::getInstance()->passUser(
                    // UserGateway::USER_OUT_INCOMING_COMMAND_DISCONNECT );

                    // No player means it was removed somewhere else,
                    // but nowhere else calls deletePlayer.
                    // Only each PlayerManager may delete a Player.
                    // So it disappeared in ProcessCommand.
                    try {
                        deletePlayer(i);
                        deleteQueuePlayer(pTempPlayer);
                    } catch (Throwable& t) {
                        filelog("deletePlayer.log", "called in IncomingPlayerManager.cpp line 549");
                        filelog("IncomingPlayerManager.txt", "Exception catched here: 3");
                    }

                    Creature* pCreature = pTempPlayer->getCreature();
                    if (pCreature != NULL)
                        pCreature->setValue(4);
                    try {
                        SAFE_DELETE(pTempPlayer);
                    } catch (Throwable& t) {
                        cerr << t.toString() << endl;
                        diagnosticFilelog("Destructer.log", "IncommingPlayerManager.cpp +509 : %s",
                                          t.toString().c_str());
                    }
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

void IncomingPlayerManager::processOutputs() {
    __BEGIN_TRY

    if (m_MinFD == -1 && m_MaxFD == -1) // no player exist
    {
        return;
    }

    const de::DescriptorRange walk = de::descriptorRange((int)m_MinFD, (int)m_MaxFD, (int)nMaxPlayers);
    for (int i = walk.first; i <= walk.last; i++) {
        if (m_PollSet.isWritable(i)) {
            if (i == m_SocketID) {
                FILELOG_INCOMING_CONNECTION(
                    "ICMFD.txt",
                    "[ i == m_SocketID ] FD : %d, ServerSocket : %d, MinFD : %d, MaxFD : %d, nPlayers : %d:", i,
                    m_SocketID, m_MinFD, m_MaxFD, m_nPlayers);
                throw IOException("server socket reported ready to write.");
            }

            if (m_pPlayers[i] != NULL) {
                GamePlayer* pTempPlayer = dynamic_cast<GamePlayer*>(m_pPlayers[i]);

                Assert(pTempPlayer != NULL);
                Assert(m_pPlayers[i] != NULL);

                if (pTempPlayer->getSocket()->getSockError()) {
                    FILELOG_INCOMING_CONNECTION("ICMPOSocketErr.log", "[Output] PlayerID : %s, PlayerStatus : %d",
                                                pTempPlayer->getID().c_str(), (int)pTempPlayer->getPlayerStatus());
                    try {
                        // The connection is already closed, so the output buffer must not be flushed.
                        pTempPlayer->disconnect(DISCONNECTED);
                    } catch (Throwable& t) {
                        cerr << t.toString() << endl;
                    }

                    // by sigi. 2002.12.30
                    //					UserGateway::getInstance()->passUser(
                    // UserGateway::USER_OUT_INCOMING_OUTPUT_ERROR );

                    // No player means it was removed somewhere else,
                    // but nowhere else calls deletePlayer.
                    // Only each PlayerManager may delete a Player.
                    // So it disappeared in ProcessCommand.
                    try {
                        deletePlayer(i);
                        deleteQueuePlayer(pTempPlayer);
                    } catch (Throwable& t) {
                        filelog("deletePlayer.log", "called in IncomingPlayerManager.cpp line 637");
                        filelog("IncomingPlayerManager.txt", "Exception catched here: 4");
                    }

                    Creature* pCreature = pTempPlayer->getCreature();
                    if (pCreature != NULL)
                        pCreature->setValue(5);
                    SAFE_DELETE(pTempPlayer);
                } else {
                    try {
                        pTempPlayer->processOutput();
                    } catch (ConnectException& ce) {
                        FILELOG_INCOMING_CONNECTION(
                            "ICMPOConnectExcept.log", "[Output] %s, PlayerID : %s, PlayerStatus : %d",
                            ce.toString().c_str(), pTempPlayer->getID().c_str(), (int)pTempPlayer->getPlayerStatus());

                        try {
                            // The connection is already closed, so the output buffer must not be flushed.
                            pTempPlayer->disconnect(DISCONNECTED);
                        } catch (Throwable& t) {
                            cerr << t.toString() << endl;
                        }

                        // by sigi. 2002.12.30
                        //						UserGateway::getInstance()->passUser(
                        // UserGateway::USER_OUT_INCOMING_OUTPUT_DISCONNECT );

                        // No player means it was removed somewhere else,
                        // but nowhere else calls deletePlayer.
                        // Only each PlayerManager may delete a Player.
                        // So it disappeared in ProcessCommand.
                        try {
                            deletePlayer(i);
                            deleteQueuePlayer(pTempPlayer);
                        } catch (Throwable& t) {
                            filelog("deletePlayer.log", "called in IncomingPlayerManager.cpp line 681");
                            filelog("IncomingPlayerManager.txt", "Exception catched here: 5");
                        }

                        Creature* pCreature = pTempPlayer->getCreature();
                        if (pCreature != NULL)
                            pCreature->setValue(6);
                        SAFE_DELETE(pTempPlayer);
                    } catch (ProtocolException& cp) {
                        FILELOG_INCOMING_CONNECTION(
                            "ICMPOProtocolExcept.log", "[Output] %s, PlayerID : %s, PlayerStatus : %d",
                            cp.toString().c_str(), pTempPlayer->getID().c_str(), (int)pTempPlayer->getPlayerStatus());

                        // The connection is already closed, so the output buffer must not be flushed.

                        try {
                            pTempPlayer->disconnect(DISCONNECTED);
                        } catch (Throwable& t) {
                            cerr << t.toString() << endl;
                        }

                        // by sigi. 2002.12.30
                        //						UserGateway::getInstance()->passUser(
                        // UserGateway::USER_OUT_INCOMING_OUTPUT_DISCONNECT2 );

                        // No player means it was removed somewhere else,
                        // but nowhere else calls deletePlayer.
                        // Only each PlayerManager may delete a Player.
                        // So it disappeared in ProcessCommand.
                        try {
                            deletePlayer(i);
                            deleteQueuePlayer(pTempPlayer);
                        } catch (Throwable& t) {
                            filelog("deletePlayer.log", "called in IncomingPlayerManager.cpp line 720");
                            filelog("IncomingPlayerManager.txt", "Exception catched here: 6");
                        }

                        Creature* pCreature = pTempPlayer->getCreature();
                        if (pCreature != NULL)
                            pCreature->setValue(7);
                        SAFE_DELETE(pTempPlayer);
                    }
                }
            }

            // pTempPlayer->processOutput();
        }
    }

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////////////
// process all players' exceptions
// There is no plan to send OOB data so far.
// So if OOB is on, it is treated as an error and the connection is cut.
//////////////////////////////////////////////////////////////////////////////

void IncomingPlayerManager::processExceptions() {
    __BEGIN_TRY

    if (m_MinFD == -1 && m_MaxFD == -1) // no player exist
    {
        return;
    }

    const de::DescriptorRange walk = de::descriptorRange((int)m_MinFD, (int)m_MaxFD, (int)nMaxPlayers);
    for (int i = walk.first; i <= walk.last; i++) {
        if (m_PollSet.isUrgent(i)) {
            if (i != m_SocketID) {
                if (m_pPlayers[i] != NULL) {
                    GamePlayer* pTempPlayer = dynamic_cast<GamePlayer*>(m_pPlayers[i]);
                    Assert(pTempPlayer != NULL);
                    Assert(i != m_SocketID);
                    Assert(m_pPlayers[i] != NULL);
                    StringStream msg;
                    msg << "OOB from " << pTempPlayer->toString();

                    FILELOG_INCOMING_CONNECTION("ICMPEOOB.log", "PlayerID : %s, PlayerStatus : %d",
                                                pTempPlayer->getID().c_str(), (int)pTempPlayer->getPlayerStatus());
                    try {
                        pTempPlayer->disconnect();
                    } catch (Throwable& t) {
                        // cerr << t.toString() << endl;
                    }

                    // by sigi. 2002.12.30
                    //					UserGateway::getInstance()->passUser( UserGateway::USER_OUT_INCOMING_EXCEPTION
                    //);

                    // No player means it was removed somewhere else,
                    // but nowhere else calls deletePlayer.
                    // Only each PlayerManager may delete a Player.
                    // So it disappeared in ProcessCommand.
                    try {
                        deletePlayer(i);
                        deleteQueuePlayer(pTempPlayer);
                    } catch (Throwable& t) {
                        filelog("deletePlayer.log", "called in IncomingPlayerManager.cpp line 799");
                        filelog("IncomingPlayerManager.txt", "Exception catched here: 7");
                    }

                    Creature* pCreature = pTempPlayer->getCreature();
                    if (pCreature != NULL)
                        pCreature->setValue(8);
                    SAFE_DELETE(pTempPlayer);
                }
            } else {
                // cerr << "Exception in Loginserver to Gameserver" << endl;
            }
        }
    }

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////////////
// The socket the poll reported ready is accepted here.
//////////////////////////////////////////////////////////////////////////////
bool IncomingPlayerManager::acceptNewConnection(Socket* forwarded) {
    __BEGIN_TRY

    std::unique_ptr<Socket> client(forwarded);
    de::GameConnection player;
    std::string peerHost;
    m_CheckValue = 0;
    int fd = -9999;
    const int minFD = (int)m_MinFD;
    const int maxFD = (int)m_MaxFD;
    const auto logEnabled = [&] { return m_LogConnections && m_LogConnections(); };

    try {
        m_CheckValue = 1;
        if (!client)
            client.reset(m_pServerSocket->accept());
        m_CheckValue = 2;
    } catch (Throwable&) {
        m_CheckValue += 10000;
    }
    if (!client) {
        m_CheckValue = 50;
        return false;
    }

    try {
        fd = (int)client->getSOCKET();
        // Diagnostics must not borrow the socket after player construction:
        // a failed constructor can already have destroyed its adopted socket.
        peerHost = client->getHost();
        if (logEnabled())
            filelog("acceptNewConnection.log", "Accept FD : %d ( MinFD : %d , MaxFD : %d ) %s", fd, minFD, maxFD,
                    peerHost.c_str());
        if (fd <= 0 || fd >= nMaxPlayers) {
            if (logEnabled())
                filelog("acceptNewConnectionError.log", "Accept FD : %d ( MinFD : %d , MaxFD : %d ) %s", fd, minFD,
                        maxFD, peerHost.c_str());
            throw Error();
        }

        m_CheckValue = 4; // Socket preparation is one owned stage.
        client = de::prepareAcceptedServerConnection(std::move(client));
        m_CheckValue = 10;
        m_pConnectionInfoManager->getConnectionInfo(peerHost);
        m_CheckValue = 11;
        player = de::makeGameConnection(std::move(client));
        m_CheckValue = 13;
        try {
            m_CheckValue = 14;
            addPlayer(player.get());
            player.release();
            m_CheckValue = 15;
        } catch (DuplicatedException& error) {
            if (logEnabled())
                filelog("ancDupExcept.log", "[Output] %s, FD : %d ( MinFD : %d , MaxFD : %d ) %s",
                        error.toString().c_str(), fd, minFD, maxFD, peerHost.c_str());
            m_CheckValue += 3000;
            player.reset();
            m_CheckValue += 1000;
        }
    } catch (NoSuchElementException&) {
        if (logEnabled())
            filelog("ancNoSuch.log", "FD : %d ( MinFD : %d , MaxFD : %d ) %s", fd, minFD, maxFD, peerHost.c_str());
        m_CheckValue += 25000;
    } catch (Throwable&) {
        if (logEnabled())
            filelog("ancThrowable.log", "FD : %d ( MinFD : %d , MaxFD : %d ) %s checkValue : %d", fd, minFD, maxFD,
                    peerHost.c_str(), m_CheckValue);
        m_CheckValue += 30000;
    } catch (const std::exception&) {
        m_CheckValue += 40000;
        if (logEnabled())
            filelog("ancException.log", "FD : %d ( MinFD : %d , MaxFD : %d ) %s checkValue : %d", fd, minFD, maxFD,
                    peerHost.c_str(), m_CheckValue);
    } catch (...) {
        m_CheckValue += 50000;
        if (logEnabled())
            filelog("ancEtc.log", "FD : %d ( MinFD : %d , MaxFD : %d ) %s checkValue : %d", fd, minFD, maxFD,
                    peerHost.c_str(), m_CheckValue);
    }
    m_CheckValue = 33;
    return true;

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////
//
// Add the player object for a new connection to the IPM.
//
//////////////////////////////////////////////////////////////////////
void IncomingPlayerManager::addPlayer(Player* pGamePlayer) {
    __BEGIN_TRY

    __ENTER_CRITICAL_SECTION(m_Mutex)

    // call base class's method
    PlayerManager::addPlayer(pGamePlayer);

    SOCKET fd = pGamePlayer->getSocket()->getSOCKET();

    // Readjust m_MinFD and m_MaxFD.
    m_MinFD = min(fd, m_MinFD);
    m_MaxFD = max(fd, m_MaxFD);

    // Watch the new descriptor. It is reported ready no earlier than the next
    // poll.
    m_PollSet.watch(fd, de::DescriptorPollSet::kRead | de::DescriptorPollSet::kWrite | de::DescriptorPollSet::kUrgent);

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////
//
// Add the player object for a new connection to the IPM.
//
//////////////////////////////////////////////////////////////////////
void IncomingPlayerManager::addPlayer_NOBLOCKED(Player* pGamePlayer) {
    __BEGIN_TRY

    // call base class's method
    PlayerManager::addPlayer(pGamePlayer);

    SOCKET fd = pGamePlayer->getSocket()->getSOCKET();

    // Readjust m_MinFD and m_MaxFD.
    m_MinFD = min(fd, m_MinFD);
    m_MaxFD = max(fd, m_MaxFD);

    // Watch the new descriptor. It is reported ready no earlier than the next
    // poll.
    m_PollSet.watch(fd, de::DescriptorPollSet::kRead | de::DescriptorPollSet::kWrite | de::DescriptorPollSet::kUrgent);

    __END_CATCH
}

void IncomingPlayerManager::deletePlayer_NOBLOCKED(SOCKET fd) {
    __BEGIN_TRY

    // call base class's method
    // filelog("deletePlayer.log", "Call in deletePlayer_NOBLOCKED");
    PlayerManager::deletePlayer(fd);

    Assert(m_pPlayers[fd] == NULL);

    // Readjust m_MinFD and m_MaxFD.
    // The case fd == m_MinFD && fd == m_MaxFD is handled by the first if.
    if (fd == m_MinFD) {
        // Find the smallest fd from the front.
        // Note that the m_MinFD slot is currently NULL.
        const de::DescriptorRange walk = de::descriptorRange((int)m_MinFD, (int)m_MaxFD, (int)nMaxPlayers);
        int i = walk.first;
        for (i = walk.first; i <= walk.last; i++) {
            if (m_pPlayers[i] != NULL || i == m_SocketID) {
                m_MinFD = i;
                break;
            }
        }

        // When no suitable m_MinFD was found,
        // this is the case m_MinFD == m_MaxFD.
        // In that case set both to -1.
        if (i > walk.last)
            m_MinFD = m_MaxFD = -1;
    } else if (fd == m_MaxFD) {
        // Find the largest fd from the back.
        // Mind the SocketID! (For the SocketID the Player pointer is NULL.)
        const de::DescriptorRange walk = de::descriptorRange((int)m_MinFD, (int)m_MaxFD, (int)nMaxPlayers);
        int i = walk.last;
        for (i = walk.last; i >= walk.first; i--) {
            if (m_pPlayers[i] != NULL || i == m_SocketID) {
                m_MaxFD = i;
                break;
            }
        }

        // When no suitable m_MinFD was found,
        if (i < walk.first) {
            FILELOG_INCOMING_CONNECTION("ICMFD.txt",
                                        "[ i < m_MinFD nbl] nPlayers : %d, MinFD : %d, MaxFD : %d, ServerSocket : %d",
                                        m_nPlayers, (int)m_MinFD, (int)m_MaxFD, (int)m_SocketID);
            throw UnknownError("m_MinFD & m_MaxFD problem.");
        }
    }

    // Stop watching the descriptor. This also drops the readiness the last poll
    // reported for it, because otherwise an object that is gone could still be
    // processed.
    m_PollSet.unwatch(fd);

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////
//
// Delete a given player from the IPM.
//
// A player is removed from the IPM for the following reasons.
//
//  (1) The object moves to the ZPM --> the player object must not be deleted.
//  (2) The connection drops before entering the game --> the player object must be deleted.
//
// So deleting the player has to happen outside.
//
//////////////////////////////////////////////////////////////////////
void IncomingPlayerManager::deletePlayer(SOCKET fd) {
    __BEGIN_TRY

    __ENTER_CRITICAL_SECTION(m_Mutex)

    // call base class's method
    // filelog("deletePlayer.log", "Call in deletePlayer(...) in IncomingPlayerManager.cpp");
    PlayerManager::deletePlayer(fd);

    Assert(m_pPlayers[fd] == NULL);

    // Readjust m_MinFD and m_MaxFD.
    // The case fd == m_MinFD && fd == m_MaxFD is handled by the first if.
    if (fd == m_MinFD) {
        // Find the smallest fd from the front.
        // Note that the m_MinFD slot is currently NULL.
        const de::DescriptorRange walk = de::descriptorRange((int)m_MinFD, (int)m_MaxFD, (int)nMaxPlayers);
        int i = walk.first;
        for (i = walk.first; i <= walk.last; i++) {
            if (m_pPlayers[i] != NULL || i == m_SocketID) {
                m_MinFD = i;
                break;
            }
        }

        // When no suitable m_MinFD was found,
        // this is the case m_MinFD == m_MaxFD.
        // In that case set both to -1.
        if (i > walk.last)
            m_MinFD = m_MaxFD = -1;
    } else if (fd == m_MaxFD) {
        // Find the largest fd from the back.
        // Mind the SocketID! (For the SocketID the Player pointer is NULL.)
        const de::DescriptorRange walk = de::descriptorRange((int)m_MinFD, (int)m_MaxFD, (int)nMaxPlayers);
        int i = walk.last;
        for (i = walk.last; i >= walk.first; i--) {
            if (m_pPlayers[i] != NULL || i == m_SocketID) {
                m_MaxFD = i;
                break;
            }
        }

        // When no suitable m_MinFD was found,
        if (i < walk.first) {
            FILELOG_INCOMING_CONNECTION("ICMFD.txt",
                                        "[ i < m_MinFD ] nPlayers : %d, MinFD : %d, MaxFD : %d, ServerSocket : %d",
                                        m_nPlayers, (int)m_MinFD, (int)m_MaxFD, (int)m_SocketID);
            throw UnknownError("m_MinFD & m_MaxFD problem.");
        }
    }

    // Stop watching the descriptor. This also drops the readiness the last poll
    // reported for it, because otherwise an object that is gone could still be
    // processed.
    m_PollSet.unwatch(fd);

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    __END_CATCH
}

void IncomingPlayerManager::pushPlayer(GamePlayer* pGamePlayer)

{
    __BEGIN_TRY

    __ENTER_CRITICAL_SECTION(m_Mutex)

    Assert(pGamePlayer != nullptr);
    m_PlayerListQueue.push_back(pGamePlayer);

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    __END_CATCH
}

void IncomingPlayerManager::pushOutPlayer(GamePlayer* pGamePlayer)

{
    __BEGIN_TRY

    __ENTER_CRITICAL_SECTION(m_MutexOut)

    Assert(pGamePlayer != nullptr);
    m_PlayerOutListQueue.push_back(pGamePlayer);

    __LEAVE_CRITICAL_SECTION(m_MutexOut)

    __END_CATCH
}

void IncomingPlayerManager::moveToOutgoing(GamePlayer* player) {
    std::lock_guard tableLock(m_Mutex);
    std::lock_guard queueLock(m_MutexOut);
    Assert(player != nullptr);
    const auto descriptor = de::enqueueRegisteredGamePlayer(m_pPlayers, m_PlayerOutListQueue, *player);
    --m_nPlayers;
    m_PollSet.unwatch(descriptor);
    const auto range = de::gamePlayerDescriptorRange(m_pPlayers, m_SocketID);
    m_MinFD = range.empty() ? -1 : range.first;
    m_MaxFD = range.empty() ? -1 : range.last;
}

void IncomingPlayerManager::heartbeat()

{
    __BEGIN_TRY

    Properties& config = de::kernelContext().config();

    __ENTER_CRITICAL_SECTION(m_Mutex)

    //--------------------------------------------------
    // Add the PlayerQueue's Player to the manager.
    //--------------------------------------------------

    // Coming from the ZPM into the IPM is handled differently depending on Status.
    // There are two ways to go from the ZPM to the IPM.
    // 1. Zone change: the GPS_WAITING_FOR_CG_READY state.
    // 2. Logout: GPS_AFTER_SENDING_GL_INCOMING_CONNECTION
    while (!m_PlayerListQueue.empty()) {
        GamePlayer* pGamePlayer = m_PlayerListQueue.front();

        if (pGamePlayer == NULL) {
            m_PlayerListQueue.pop_front();
            filelog("ZoneBug.txt", "%s : %s", "Zone::heartbeat(1)", "pGamePlayer is NULL.");
            continue;
        }

        //-----------------------------------------------------------------------------
        // * elcastle 's Note
        //-----------------------------------------------------------------------------
        // A KICKED flag set during the handover means an abnormal termination.
        // In that case simply disconnecting is enough.
        // On logout the KICKED flag is not set at this stage.
        // It is set at the LGIncomingConnectionOK stage, so do not confuse the two.
        // For a normal logout, tripping this check is not normal.
        // On a socket error or an abnormal termination the connection is cut here.
        // Both the disconnect and the connect are done here in case kernel level
        // support for the socket's Using resource is unstable.
        // In practice unstable behaviour does show up.
        //-----------------------------------------------------------------------------
        if (pGamePlayer->isPenaltyFlag(PENALTY_TYPE_KICKED)) {
            auto endingPlayer = de::takeFirstGamePlayer(m_PlayerListQueue);
            // The connection is already closed, so the output buffer must not be flushed.
            int fd = -1;
            Socket* pSocket = pGamePlayer->getSocket();
            if (pSocket != NULL)
                fd = (int)pSocket->getSOCKET();

            FILELOG_INCOMING_CONNECTION("incomingDisconnect.log", "FD : %d, %s", fd,
                                        (pSocket == NULL ? "NULL" : pSocket->getHost().c_str()));

            // by sigi. 2002.12.30
            if (pGamePlayer->getReconnectPacket() != NULL) {
                //				UserGateway::getInstance()->passUser( UserGateway::USER_OUT_NORMAL );
            } else {
                //				UserGateway::getInstance()->passUser( UserGateway::USER_OUT_KICKED );
            }

            try {
                pGamePlayer->disconnect(DISCONNECTED);

                // An existing character is being removed in order to log in.
                // In that case the result packet has to be sent to the LoginServer.
                // by sigi. 2002.5.4
                if (pGamePlayer->isKickForLogin()) {
                    // send GLKickVerify to LoginServer. 2002.5.6
                    GLKickVerify glKickVerify;
                    glKickVerify.setKicked(true);
                    glKickVerify.setID(pGamePlayer->getSocket()->getSOCKET());
                    glKickVerify.setPCName(pGamePlayer->getCreature()->getName());

                    de::gameContext().loginServer().sendPacket(pGamePlayer->getKickRequestHost(),
                                                               pGamePlayer->getKickRequestPort(), &glKickVerify);

                    cout << "LGKickVerify Send Packet to ServerIP : " << pGamePlayer->getKickRequestHost() << endl;
                    cout << "LGKickVerify Send Packet to ServerPort : " << pGamePlayer->getKickRequestPort() << endl;
                }

            } catch (Throwable& t) {
                cerr << t.toString() << endl;
            }

            Creature* pCreature = pGamePlayer->getCreature();
            if (pCreature != NULL)
                pCreature->setValue(9);
            continue;
        }


        de::transferFirstGamePlayer(m_PlayerListQueue, [&](GamePlayer* player) { addPlayer_NOBLOCKED(player); });

        // filelog("ZoneHeartbeatTrace.txt", "Added Player[%s]", pGamePlayer->getID().c_str());

        // Once Adding is completely finished, the following is done, depending on Status.
        // Send the matching packet once the move from the ZPM to the IPM is complete.

        // This is the zone change case.
        if (pGamePlayer->getPlayerStatus() == GPS_WAITING_FOR_CG_READY) {
            Creature* pCreature = pGamePlayer->getCreature();
            Assert(pCreature != NULL);

            Zone* pOldZone = pCreature->getZone();

            // by sigi. 2002.5.15
            Zone* pZone = pCreature->getNewZone();
            // Assert(pZone != NULL);

            if (pOldZone != NULL) {
                // The player is leaving a master lair.
                if (pOldZone->isMasterLair()) {
                    MasterLairManager* pMasterLairManager = pOldZone->getMasterLairManager();
                    Assert(pMasterLairManager != NULL);
                    pMasterLairManager->leaveCreature(pCreature);
                }

                // The player is leaving a PK zone.
                if (pCreature->isPLAYER() && pZone != NULL && pOldZone->getZoneID() != pZone->getZoneID()) {
                    if (de::gameContext().pkZoneInfos().isPKZone(pOldZone->getZoneID()))
                        de::gameContext().pkZoneInfos().leavePKZone(pOldZone->getZoneID());
                }
            }

            if (pZone == NULL) {
                pZone = pCreature->getZone();
                Assert(pZone != NULL);
            } else {
                pCreature->setZone(pZone);
                pCreature->setNewZone(NULL);

                pCreature->setXY(pCreature->getNewX(), pCreature->getNewY());

                // The player is entering a new Zone.
                pCreature->registerObject();
            }

            // Register the encryption code. It is currently based on objectID.
#ifdef __USE_ENCRYPTER__
            pGamePlayer->setEncryptCode();
#endif

            // Send the System Availabilities information.
            SEND_SYSTEM_AVAILABILITIES(pGamePlayer);

            //--------------------------------------------------------------------------------
            // Build and send the GCUpdateInfo packet.
            //--------------------------------------------------------------------------------
            GCUpdateInfo gcUpdateInfo;

            makeGCUpdateInfo(&gcUpdateInfo, pCreature);

            pGamePlayer->sendPacket(&gcUpdateInfo);

            // This is the logout case.
        } else if (pGamePlayer->getPlayerStatus() == GPS_AFTER_SENDING_GL_INCOMING_CONNECTION) {
            //			cout << "Logout..." << pGamePlayer->getID() << endl;

            // Send GLIncomingConnection to the login server.
            // PlayerName and ClientIP are sent along with it.
            GLIncomingConnection glIncomingConnection;
            glIncomingConnection.setPlayerID(pGamePlayer->getID());
            glIncomingConnection.setClientIP(pGamePlayer->getSocket()->getHost());

            static int portNum = config.getPropertyInt("LoginServerUDPPortNum");

            int port;

            if (portNum > 1) {
                port = config.getPropertyInt("LoginServerBaseUDPPort") + rand() % portNum;
            } else {
                port = config.getPropertyInt("LoginServerUDPPort");
            }

            // cout << "ReconnectAddress = " << config.getProperty("LoginServerIP").c_str() << ":" << port << endl;

            // Just send it.
            de::gameContext().loginServer().sendPacket(config.getProperty("LoginServerIP"), port,
                                                       &glIncomingConnection);
        }

        // filelog("ZoneHeartbeatTrace.txt", "After pop front");
    }

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    // Handle the people waiting in the outgoing queue.
    // These are the people going to the ZPM. Simply adding them is enough.
    // Which zone they go to is read from the Creature's Zone, so the Zone must be set beforehand.

    // by sigi. 2002.12.10
    __ENTER_CRITICAL_SECTION(m_MutexOut)

    de::transferGamePlayerBatch(
        m_PlayerOutListQueue,
        [](GamePlayer* pGamePlayer) {
            Assert(pGamePlayer != NULL);

            Creature* pCreature = pGamePlayer->getCreature();
            Assert(pCreature != NULL);

            // getNewZone() is the Zone that is newly entered.
            Zone* pZone = pCreature->getNewZone();

            // If newZone was not set, fall back to the existing zone.
            // load() does not set NewZone.
            if (pZone == NULL) {
                pZone = pCreature->getZone();
                Assert(pZone != NULL);
            }

            // Find the PlayerManager of the zone being entered.
            ZoneGroup* pZoneGroup = pZone->getZoneGroup();
            Assert(pZoneGroup != NULL);
            ZonePlayerManager* pZonePlayerManager = pZoneGroup->getZonePlayerManager();
            Assert(pZonePlayerManager != NULL);

            // Push.
            pZonePlayerManager->pushPlayer(pGamePlayer);
        },
        [](std::exception_ptr) {
            filelog("IncomingPlayerManager.txt", "AssertionError! IncomingPlayManager.cpp line 1594");
        });

    __LEAVE_CRITICAL_SECTION(m_MutexOut)

    __END_CATCH
}

void IncomingPlayerManager::deleteQueuePlayer(GamePlayer* pGamePlayer) {
    __BEGIN_TRY


    // This lock looks unnecessary.
    // by sigi. 2002.5.9
    // A different lock is used.
    __ENTER_CRITICAL_SECTION(m_MutexOut)

    Assert(pGamePlayer != NULL);

    list<GamePlayer*>::iterator itr =
        find_if(m_PlayerOutListQueue.begin(), m_PlayerOutListQueue.end(), isSamePlayer(pGamePlayer));

    if (itr != m_PlayerOutListQueue.end()) {
        m_PlayerOutListQueue.erase(itr);
    }

    __LEAVE_CRITICAL_SECTION(m_MutexOut)

    __END_CATCH
}

////////////////////////////////////////////////////////////////////////
// Clean up every user in the IncomingPlayerManager.
////////////////////////////////////////////////////////////////////////
void IncomingPlayerManager::clearPlayers() {
    releasePlayers(true);
}

void IncomingPlayerManager::releasePlayers(bool disconnect) noexcept {
    de::releaseGamePlayers(m_pPlayers, m_PlayerListQueue, m_PlayerOutListQueue, m_PollSet, disconnect);
    std::fill(std::begin(m_pCopyPlayers), std::end(m_pCopyPlayers), nullptr);
    m_nPlayers = 0;
    m_MinFD = m_MaxFD = m_SocketID;
}
