//////////////////////////////////////////////////////////////////////////////
// Filename    : IncomingPlayerManager.cpp
// Written by  : reiot@ewestsoft.com
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "IncomingPlayerManager.h"

#include <stdio.h>

#include <algorithm>
#include <memory>

#include "Assert.h"
#include "CreatureUtil.h"
#include "DB.h"
#include "DescriptorTable.h"
#include "Encrypter.h"
#include "GCUpdateInfo.h"
#include "GLIncomingConnection.h"
#include "GLKickVerify.h"
#include "GameContext.h"
#include "GamePlayer.h"
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

IncomingPlayerManager::IncomingPlayerManager()

    : m_pServerSocket(NULL), m_SocketID(INVALID_SOCKET), m_PollSet((int)nMaxPlayers), m_TimeoutMilliseconds(0),
      m_MinFD(-1), m_MaxFD(-1) {
    __BEGIN_TRY

    m_Mutex.setName("IncomingPlayerManager");
    m_MutexOut.setName("IncomingPlayerManagerOut");
    m_PlayerListQueue.clear();

    try {
        const auto port = de::readServerPort(de::kernelContext().config(), "TCPPort");
        de::retryListenerStartup(
            [&] {
                auto socket = std::make_unique<ServerSocket>(port);
                socket->setNonBlocking(true);
                m_pServerSocket = socket.release();
            },
            [&](const BindException& error) {
                cout << "IncomingPlayerManager(" << port << ") : " << error.toString() << endl;
            },
            "TCP");

        // Set the server socket descriptor.
        m_SocketID = m_pServerSocket->getSOCKET();
    } catch (NoSuchElementException& nsee) {
        // No such element in the configuration file
        throw Error(nsee.toString());
    }

    m_pConnectionInfoManager = new ConnectionInfoManager();
    de::gameContext().setConnectionInfoManager(m_pConnectionInfoManager);

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////////////
// destructor
//////////////////////////////////////////////////////////////////////////////

IncomingPlayerManager::~IncomingPlayerManager() noexcept(false)

{
    __BEGIN_TRY

    SAFE_DELETE(m_pConnectionInfoManager);

    __END_CATCH_NO_RETHROW
}


//////////////////////////////////////////////////////////////////////////////
// Initialize the sub-managers and data members.
//////////////////////////////////////////////////////////////////////////////

void IncomingPlayerManager::init()

{
    __BEGIN_TRY

    Properties& config = de::kernelContext().config();

    m_ProxyAcceptor = de::ProxyAcceptor::fromConfig(config);

    // The player table is indexed by descriptor and every walk over it is
    // clamped to it, so a listener the table cannot hold would be skipped by
    // all of them and no connection could ever be accepted. There is nothing
    // to serve from in that state.
    if (!de::fitsDescriptorTable((int)m_SocketID, (int)nMaxPlayers))
        throw Error("listening socket descriptor does not fit the player table");

    // Watch the server socket for an arriving connection and for out-of-band
    // data. (Writing to it is never checked.)
    m_PollSet.watch(m_SocketID, de::DescriptorPollSet::kRead | de::DescriptorPollSet::kUrgent);

    // set min/max fd
    m_MinFD = m_MaxFD = m_SocketID;

    // How long a poll waits. This period should become an option later as
    // well. It may be longer than the one in ZonePlayerManager.
    m_TimeoutMilliseconds = 0;

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
                    filelog("Destructer.log", "IncommingPlayerManager.cpp +509 : %s", t.toString().c_str());
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
                        filelog("Destructer.log", "IncommingPlayerManager.cpp +509 : %s", t.toString().c_str());
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
bool IncomingPlayerManager::acceptNewConnection(Socket* forwarded)

{
    __BEGIN_TRY

    m_CheckValue = 0;

    int fd = -9999;
    int MinFD = (int)m_MinFD;
    int MaxFD = (int)m_MaxFD;

    // When a connection is awaited in blocking mode,
    // the returned value can never be NULL.
    // A NonBlockingIOException cannot occur either.
    Socket* client = forwarded;

    try {
        m_CheckValue = 1;
        if (!client)
            client = m_pServerSocket->accept();
        m_CheckValue = 2;
    } catch (Throwable& t) {
        m_CheckValue += 10000;
    }

    if (client == NULL) {
        m_CheckValue = 50;
        return false;
    }

    try {
        fd = (int)client->getSOCKET();
        FILELOG_INCOMING_CONNECTION("acceptNewConnection.log", "Accept FD : %d ( MinFD : %d , MaxFD : %d ) %s", fd,
                                    MinFD, MaxFD, client->getHost().c_str());

        if (fd <= 0 || fd >= nMaxPlayers) {
            FILELOG_INCOMING_CONNECTION("acceptNewConnectionError.log", "Accept FD : %d ( MinFD : %d , MaxFD : %d ) %s",
                                        fd, MinFD, MaxFD, client->getHost().c_str());

            throw Error();
        }

        // Added for error handling; the cause still has to be found.
        // It is probably a problem in the Thread's socket management.
        // Temporary until the Thread related work is finished.
        if (client->getSockError()) {
            m_CheckValue = 4;
            throw Error();
        }

        m_CheckValue = 5;
        client->setNonBlocking(true);
        m_CheckValue = 6;

        // Added for error handling; the cause still has to be found.
        // It is probably a problem in the Thread's socket management.
        // Temporary until the Thread related work is finished.
        if (client->getSockError()) {
            m_CheckValue = 7;
            throw Error();
        }
        // set socket option (!NonBlocking, NoLinger)
        m_CheckValue = 8;
        client->setLinger(0);
        m_CheckValue = 9;

        m_CheckValue = 10;

        //----------------------------------------------------------------------
        // Verify that it is in the Incoming List.
        //----------------------------------------------------------------------
        // toString() sometimes hits CI == NULL. Beware.

        // If an exception is thrown in here, the connection is cut.
        m_pConnectionInfoManager->getConnectionInfo(client->getHost());
        m_CheckValue = 11;

        // Create the player object using the client socket as a parameter.
        GamePlayer* pGamePlayer = new GamePlayer(client);
        m_CheckValue = 12;

        // set player status to GPS_BEGIN_SESSION
        pGamePlayer->setPlayerStatus(GPS_BEGIN_SESSION);
        m_CheckValue = 13;

        // Register it with the IPM.
        // addPlayer_NOBLOCKED(pGamePlayer);
        try {
            m_CheckValue = 14;
            addPlayer(pGamePlayer);
            m_CheckValue = 15;

            // by sigi. 2002.12.30
            //			UserGateway::getInstance()->passUser( UserGateway::USER_IN_NORMAL );
        } catch (DuplicatedException& de) {
            FILELOG_INCOMING_CONNECTION("ancDupExcept.log", "[Output] %s, FD : %d ( MinFD : %d , MaxFD : %d ) %s",
                                        de.toString().c_str(), fd, MinFD, MaxFD, client->getHost().c_str());

            // The player owns the socket now, and deleting it closes and
            // deletes the socket; the session never began.
            m_CheckValue += 3000;
            pGamePlayer->setPlayerStatus(GPS_END_SESSION);
            SAFE_DELETE(pGamePlayer);
            m_CheckValue += 1000;
        }
    } catch (NoSuchElementException&) {
        FILELOG_INCOMING_CONNECTION("ancNoSuch.log", "FD : %d ( MinFD : %d , MaxFD : %d ) %s", fd, MinFD, MaxFD,
                                    client->getHost().c_str());

        m_CheckValue += 20000;
        m_CheckValue += 1000;

        m_CheckValue += 1000;

        //----------------------------------------acceptNewConnection core!!!
        // The connection is not authenticated, so it is cut.
        // client->send("Error : Unauthorized access",27);

        m_CheckValue += 1000;
        client->close();
        m_CheckValue += 1000;
        SAFE_DELETE(client);
        m_CheckValue += 1000;
    } catch (Throwable& t) {
        FILELOG_INCOMING_CONNECTION("ancThrowable.log", "FD : %d ( MinFD : %d , MaxFD : %d ) %s checkValue : %d", fd,
                                    MinFD, MaxFD, client->getHost().c_str(), m_CheckValue);
        m_CheckValue += 30000;
        try {
            m_CheckValue = 25;
            if (client != NULL) {
                client->close();
                m_CheckValue = 26;
                SAFE_DELETE(client);
                m_CheckValue = 27;
            }
            m_CheckValue = 28;
        } catch (Throwable& t) {
            m_CheckValue += 1000;
        } catch (...) {
            m_CheckValue += 2000;
        }
    } catch (exception& e) {
        m_CheckValue += 40000;
        FILELOG_INCOMING_CONNECTION("ancException.log", "FD : %d ( MinFD : %d , MaxFD : %d ) %s checkValue : %d", fd,
                                    MinFD, MaxFD, client->getHost().c_str(), m_CheckValue);
    } catch (...) {
        m_CheckValue += 50000;
        FILELOG_INCOMING_CONNECTION("ancEtc.log", "FD : %d ( MinFD : %d , MaxFD : %d ) %s checkValue : %d", fd, MinFD,
                                    MaxFD, client->getHost().c_str(), m_CheckValue);
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

    m_PlayerListQueue.push_back(pGamePlayer);

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    __END_CATCH
}

void IncomingPlayerManager::pushOutPlayer(GamePlayer* pGamePlayer)

{
    __BEGIN_TRY

    __ENTER_CRITICAL_SECTION(m_MutexOut)

    m_PlayerOutListQueue.push_back(pGamePlayer);

    __LEAVE_CRITICAL_SECTION(m_MutexOut)

    __END_CATCH
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
            filelog("ZoneBug.txt", "%s : %s", "Zone::heartbeat(1)", "pGamePlayer is NULL.");
            continue;
        }

        m_PlayerListQueue.pop_front();

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
            SAFE_DELETE(pGamePlayer);

            continue;
        }


        addPlayer_NOBLOCKED(pGamePlayer);

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

    while (!m_PlayerOutListQueue.empty()) {
        GamePlayer* pGamePlayer = m_PlayerOutListQueue.front();

        m_PlayerOutListQueue.pop_front();

        try {
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
        } catch (...) {
            filelog("IncomingPlayerManager.txt", "AssertionError! IncomingPlayManager.cpp line 1594");
        }
    }

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
void IncomingPlayerManager::clearPlayers()

{
    __BEGIN_TRY

    // Clean up the entries in PlayerListQueue.
    while (!m_PlayerListQueue.empty()) {
        GamePlayer* pGamePlayer = m_PlayerListQueue.front();

        m_PlayerListQueue.pop_front();

        if (pGamePlayer != NULL) {
            try {
                pGamePlayer->disconnect();
            } catch (Throwable& t) {
                // Ignored
            }

            SAFE_DELETE(pGamePlayer);
        }
    }

    // Clean up the entries in PlayerOutListQueue.
    while (!m_PlayerOutListQueue.empty()) {
        GamePlayer* pGamePlayer = m_PlayerOutListQueue.front();

        m_PlayerOutListQueue.pop_front();

        if (pGamePlayer != NULL) {
            try {
                pGamePlayer->disconnect();
            } catch (Throwable& t) {
                // Ignored
            }

            SAFE_DELETE(pGamePlayer);
        }
    }


    if (m_MinFD == -1 && m_MaxFD == -1)
        return;

    // Clean up the players.
    const de::DescriptorRange walk = de::descriptorRange((int)m_MinFD, (int)m_MaxFD, (int)nMaxPlayers);
    for (int i = walk.first; i <= walk.last; i++) {
        if (i != m_SocketID && m_pPlayers[i] != NULL) {
            GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(m_pPlayers[i]);

            if (pGamePlayer != NULL) {
                try {
                    pGamePlayer->disconnect();
                } catch (Throwable& t) {
                    // Ignored
                }

                SAFE_DELETE(pGamePlayer);
            }
        }
    }

    __END_CATCH
}
