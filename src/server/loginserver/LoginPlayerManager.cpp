//////////////////////////////////////////////////////////////////////
//
// Filename    : LoginPlayerManager.cpp
// Written by  : reiot@ewestsoft.com
// Description : Client manager for the login server
//
//////////////////////////////////////////////////////////////////////

// include files
#include "LoginPlayerManager.h"

#include <stdio.h>

#include <algorithm>
#include <mutex>

#include "AcceptedServerConnection.h"
#include "Assert.h"
#include "DatabaseError.h"
#include "DescriptorTable.h"
#include "DiagnosticTrace.h"
#include "KernelContext.h"
#include "ListenerStartup.h"
#include "LoginConnection.h"
#include "LoginContext.h"
#include "LoginPlayer.h"
#include "Properties.h"
#include "ReconnectLoginInfoManager.h"
#include "ServerPortSettings.h"
#include "Socket.h"
#include "SocketAPI.h"
#include "repository/LoginAccountRepository.h"


//////////////////////////////////////////////////////////////////////
//
// constructor
//
// The reconnect manager is owned here and borrowed by the context.
//
//////////////////////////////////////////////////////////////////////
LoginPlayerManager::LoginPlayerManager() : LoginPlayerManager(de::loginContext()) {}

LoginPlayerManager::LoginPlayerManager(de::LoginContext& context)
    : m_pServerSocket(NULL), m_ServerFD(INVALID_SOCKET), m_PollSet((int)nMaxPlayers), m_TimeoutMilliseconds(0),
      m_MinFD(-1), m_MaxFD(-1), m_Context(context) {
    __BEGIN_TRY

    m_Mutex.setName("LoginPlayerManager");

    m_pReconnectLoginInfoManager = std::make_unique<ReconnectLoginInfoManager>();
    m_PreviousReconnectLoginInfoManager =
        m_Context.exchangeReconnectLoginInfoManager(m_pReconnectLoginInfoManager.get());

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////
//
// destructor
//
// All callers and workers must have stopped using the manager before teardown.
//
//////////////////////////////////////////////////////////////////////
LoginPlayerManager::~LoginPlayerManager() noexcept {
    __BEGIN_TRY

    if (m_pServerSocket != NULL) {
        m_pServerSocket->close();
        delete m_pServerSocket;
        m_pServerSocket = NULL;
    }

    // Base PlayerManager only deletes pointers. End each login session first
    // so its destructor can release packet history. This releases local
    // resources without introducing database logout into manager destruction.
    for (auto*& player : m_pPlayers) {
        de::LoginConnection connection(static_cast<LoginPlayer*>(player));
        player = nullptr;
    }
    m_nPlayers = 0;
    m_Retirement.clear();

    __END_CATCH_NO_RETHROW

    m_Context.setReconnectLoginInfoManager(m_PreviousReconnectLoginInfoManager);
}


//////////////////////////////////////////////////////////////////////
//
// Initialize the sub-manager objects, then initialize itself.
//
//////////////////////////////////////////////////////////////////////
void LoginPlayerManager::init() {
    __BEGIN_TRY

    const auto port = de::readServerPort(de::kernelContext().config(), "LoginServerPort");
    m_ProxyAcceptor = de::ProxyAcceptor::fromConfig(de::kernelContext().config());

    de::retryListenerStartup([&] { m_pServerSocket = new ServerSocket(port); },
                             [](const BindException& error) { cout << error.toString() << endl; }, "TCP",
                             std::chrono::milliseconds(1));

    // Set the server socket descriptor.
    m_ServerFD = m_pServerSocket->getSOCKET();

    // The player table is indexed by descriptor and every walk over it is
    // clamped to it, so a listener the table cannot hold would be skipped by
    // all of them and no connection could ever be accepted. There is nothing
    // to serve from in that state.
    if (!de::fitsDescriptorTable((int)m_ServerFD, (int)nMaxPlayers))
        throw Error("listening socket descriptor does not fit the player table");

    // Watch the server socket for an arriving connection. Nothing is ever
    // written to it and its out-of-band data is not looked at.
    m_PollSet.watch(m_ServerFD, de::DescriptorPollSet::kRead);

    // set min/max fd
    m_MaxFD = m_MinFD = m_ServerFD;

    // How long a poll waits. This period should become an option later as
    // well. It may be longer than the one in ZonePlayerManager.
    m_TimeoutMilliseconds = 0;

    //--------------------------------------------------------------------------------
    // Tidy Player.LogOn up.
    //--------------------------------------------------------------------------------
    // Every account this login server left LOGON is logged off, and its
    // PC-room record dropped.
    try {
        LoginAccountRepository& repo = defaultLoginAccountRepository();
        int loginServerID = de::kernelContext().config().getPropertyInt("LoginServerID");

        vector<string> ids = repo.loadLoggedOnAccounts(loginServerID);

        for (size_t i = 0; i < ids.size(); i++) {
            repo.deletePCRoomUser(ids[i]);
        }

        repo.logOffAllOnServer(loginServerID);
    } catch (const DatabaseError& error) {
        // A SQL failure arrives as END_DB's DatabaseError carrying the line
        // it wrote to DBError.log; rethrown as the Error the startup path
        // expects, with that line in it.
        throw Error("LoginPlayerManager::init : " + error.message());
    }

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////
// Ask the kernel which descriptors are ready, for I/O multiplexing.
//////////////////////////////////////////////////////////////////////
void LoginPlayerManager::pollSockets() {
    __BEGIN_TRY

    __ENTER_CRITICAL_SECTION(m_Mutex)

    // A failed wait, which is what an arriving signal makes of it, leaves
    // every descriptor unready, so this tick processes nothing and the next
    // one asks again.
    m_PollSet.pollOnce(m_TimeoutMilliseconds);

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////
// Handle any OOB data.
//////////////////////////////////////////////////////////////////////
void LoginPlayerManager::processExceptions() {
    __BEGIN_TRY

    __ENTER_CRITICAL_SECTION(m_Mutex)

    const de::DescriptorRange walk = de::descriptorRange((int)m_MinFD, (int)m_MaxFD, (int)nMaxPlayers);
    for (int i = walk.first; i <= walk.last; i++) {
        if (m_PollSet.isUrgent(i) && i != m_ServerFD) {
            Assert(m_pPlayers[i] != NULL);

            std::exception_ptr failure;
            try {
                StringStream msg;
                msg << "OOB from " << m_pPlayers[i]->toString();
                cout << msg.toString() << endl;
            } catch (...) {
                failure = std::current_exception();
            }
            retirePlayer_NOLOCKED(i, UNDISCONNECTED, std::move(failure));
        }
    }

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////
// Copy the input of every connected player into its own input buffer.
//////////////////////////////////////////////////////////////////////
void LoginPlayerManager::processInputs() {
    __BEGIN_TRY

    __ENTER_CRITICAL_SECTION(m_Mutex)

    // Gateway connections whose header arrived; acceptNewConnection() owns
    // each socket it is handed.
    if (m_ProxyAcceptor) {
        for (auto& client : m_ProxyAcceptor->poll())
            acceptNewConnection(client.release());
    }

    const de::DescriptorRange walk = de::descriptorRange((int)m_MinFD, (int)m_MaxFD, (int)nMaxPlayers);
    for (int i = walk.first; i <= walk.last; i++) {
        if (m_PollSet.isReadable(i)) {
            if (i == m_ServerFD) {
                // The server socket means a new connection came in.
                acceptNewConnection();
            } else {
                Assert(m_pPlayers[i] != NULL);

                try {
                    if (m_pPlayers[i]->getSocket()->getSockError()) {
                        // The connection is already gone, so the output buffer must not be flushed.
                        retirePlayer_NOLOCKED(i, DISCONNECTED);
                    } else {
                        m_pPlayers[i]->processInput();
                    }
                } catch (const PeerClosedException&) {
                    // An orderly close (including a TCP health probe) needs
                    // retirement, not an error report. Cleanup failures still
                    // go through the retirement reporter independently.
                    retirePlayer_NOLOCKED(i, DISCONNECTED);
                } catch (const ConnectException&) {
                    retirePlayer_NOLOCKED(i, DISCONNECTED, std::current_exception());
                }
            }
        }
    }

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////
// Process the input packets of every connected player.
//////////////////////////////////////////////////////////////////////
void LoginPlayerManager::processCommands() {
    __BEGIN_TRY

    __ENTER_CRITICAL_SECTION(m_Mutex)

    // m_pPlayers[i] != NULL is more likely than i != m_ServerFD,
    // so that condition goes first.

    const de::DescriptorRange walk = de::descriptorRange((int)m_MinFD, (int)m_MaxFD, (int)nMaxPlayers);
    for (int i = walk.first; i <= walk.last; i++) {
        if (m_pPlayers[i] != NULL && i != m_ServerFD) {
            try {
                m_pPlayers[i]->processCommand();
            } catch (const ProtocolException&) {
                // The transport is still up, so flush any final reply.
                retirePlayer_NOLOCKED(i, UNDISCONNECTED, std::current_exception());
            } catch (const ConnectException&) {
                retirePlayer_NOLOCKED(i, DISCONNECTED, std::current_exception());
            }
        }
    }

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////
// Send the output of every connected player to the client.
//////////////////////////////////////////////////////////////////////
void LoginPlayerManager::processOutputs() {
    __BEGIN_TRY

    __ENTER_CRITICAL_SECTION(m_Mutex)

    const de::DescriptorRange walk = de::descriptorRange((int)m_MinFD, (int)m_MaxFD, (int)nMaxPlayers);
    for (int i = walk.first; i <= walk.last; i++) {
        if (m_PollSet.isWritable(i) && i != m_ServerFD) {
            Assert(m_pPlayers[i] != NULL);

            try {
                m_pPlayers[i]->processOutput();
            } catch (const ConnectException&) {
                retirePlayer_NOLOCKED(i, DISCONNECTED, std::current_exception());
            } catch (const ProtocolException&) {
                retirePlayer_NOLOCKED(i, DISCONNECTED, std::current_exception());
            }
        }
    }

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////
//
// accept new connection
//
// The socket the poll reported ready is accepted here.
//
// The login server accepts every connection attempt. A banned IP is
// refused later, when the client logs in (CLLoginHandler checks the
// address getHost() reports, which for a gateway connection is the
// client's own).
//
//////////////////////////////////////////////////////////////////////
void LoginPlayerManager::acceptNewConnection(Socket* forwarded) {
    __BEGIN_TRY

    Socket* accepted = forwarded;

    try {
        if (!accepted)
            accepted = m_pServerSocket->accept();
    } catch (Throwable& t) {
    }

    // A ConnectException can occur. (It actually did.)
    // Since NULL is returned when a CE occurs inside,
    // check for NULL and ignore it.
    if (accepted == NULL)
        return;

    // Owned here until the player takes it, so a refusal below or an
    // exception closes it.
    std::unique_ptr<Socket> client(accepted);

    // The player table is indexed by descriptor, and PlayerManager::addPlayer
    // asserts on one it cannot hold, which would end the server.
    if (!de::fitsDescriptorTable((int)client->getSOCKET(), (int)nMaxPlayers))
        return;

    try {
        client = de::prepareAcceptedServerConnection(std::move(client));
    } catch (const Error&) {
        // Refuse socket setup errors quietly; allocation failures still propagate.
        return;
    }

    de::diagnosticTrace([&](std::ostream& output) {
        output << "login connection accepted peer=" << client->getHost() << ':' << client->getPort();
    });

    auto player = de::makeLoginConnection(std::move(client));
    addPlayer_NOLOCKED(player.get());
    player.release();

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////
//
// Add a given player to the manager.
//
// On the login server no separate thread runs over the player array,
// so no locking is needed.
//
//////////////////////////////////////////////////////////////////////
void LoginPlayerManager::addPlayer_NOLOCKED(Player* pPlayer) {
    __BEGIN_TRY

    PlayerManager::addPlayer(pPlayer);

    SOCKET fd = pPlayer->getSocket()->getSOCKET();

    // Readjust m_MinFD and m_MaxFD.
    m_MinFD = m_MinFD < 0 ? fd : min(fd, m_MinFD);
    m_MaxFD = max(fd, m_MaxFD);

    // Watch the new descriptor. It is reported ready no earlier than the next
    // poll.
    m_PollSet.watch(fd, de::DescriptorPollSet::kRead | de::DescriptorPollSet::kWrite | de::DescriptorPollSet::kUrgent);

    __END_CATCH
}


//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
void LoginPlayerManager::addPlayer(Player* pPlayer) {
    __ENTER_CRITICAL_SECTION(m_Mutex)

    addPlayer_NOLOCKED(pPlayer);

    __LEAVE_CRITICAL_SECTION(m_Mutex)
}


//--------------------------------------------------------------------------------
//
// Remove a given player from the manager.
//
// On the login server no separate thread runs over the player array,
// so no locking is needed.
//
//--------------------------------------------------------------------------------
void LoginPlayerManager::deletePlayer_NOLOCKED(SOCKET fd) {
    __BEGIN_TRY

    PlayerManager::deletePlayer(fd);

    // Stop watching the descriptor. This also drops the readiness the last poll
    // reported for it, because otherwise an object that is gone could still be
    // processed later.
    m_PollSet.unwatch(fd);

    // After the validated removal, bookkeeping must not throw: callers may
    // already be transferring the detached player to its next owner.
    if (fd == m_MinFD || fd == m_MaxFD) {
        m_MinFD = m_MaxFD = m_ServerFD;
        for (SOCKET i = 0; i < static_cast<SOCKET>(nMaxPlayers); ++i) {
            if (m_pPlayers[i]) {
                m_MinFD = m_MinFD < 0 ? i : min(i, m_MinFD);
                m_MaxFD = max(i, m_MaxFD);
            }
        }
    }

    __END_CATCH
}

//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
void LoginPlayerManager::deletePlayer(SOCKET fd) {
    __ENTER_CRITICAL_SECTION(m_Mutex)

    deletePlayer_NOLOCKED(fd);

    __LEAVE_CRITICAL_SECTION(m_Mutex)
}

bool LoginPlayerManager::retirePlayer(SOCKET fd, bool flush, de::LoginRetirementTime now,
                                      const de::LoginRetirementActions& actions, std::exception_ptr reason) {
    std::lock_guard guard(m_Mutex);
    return retirePlayer_NOLOCKED(fd, flush, now, actions, std::move(reason));
}

bool LoginPlayerManager::retirePlayer_NOLOCKED(SOCKET fd, bool flush, de::LoginRetirementTime now,
                                               const de::LoginRetirementActions& actions, std::exception_ptr reason) {
    if (!de::fitsDescriptorTable(fd, nMaxPlayers) || !m_pPlayers[fd])
        return false;
    auto* player = dynamic_cast<LoginPlayer*>(m_pPlayers[fd]);
    if (!player)
        return false;
    deletePlayer_NOLOCKED(fd);
    m_Retirement.retire(de::LoginConnection(player), flush, now, actions, std::move(reason));
    return true;
}

bool LoginPlayerManager::retirePlayer_NOLOCKED(SOCKET fd, bool flush, std::exception_ptr reason) {
    return retirePlayer_NOLOCKED(fd, flush, std::chrono::steady_clock::now(), de::defaultLoginRetirementActions(),
                                 std::move(reason));
}

void LoginPlayerManager::retryRetiredPlayers(de::LoginRetirementTime now, const de::LoginRetirementActions& actions) {
    std::lock_guard guard(m_Mutex);
    m_Retirement.retry(now, actions);
}

void LoginPlayerManager::retryRetiredPlayers() {
    retryRetiredPlayers(std::chrono::steady_clock::now(), de::defaultLoginRetirementActions());
}

std::size_t LoginPlayerManager::pendingRetirements() const {
    std::lock_guard guard(m_Mutex);
    return m_Retirement.size();
}


//----------------------------------------------------------------------
// Look a given player connected to the login server up.
//----------------------------------------------------------------------
LoginPlayer* LoginPlayerManager::getPlayer_NOLOCKED(const string& id) const {
    __BEGIN_TRY

    LoginPlayer* pLoginPlayer = NULL;


    const de::DescriptorRange walk = de::descriptorRange((int)m_MinFD, (int)m_MaxFD, (int)nMaxPlayers);
    for (int i = walk.first; i <= walk.last; i++) {
        if (m_pPlayers[i] != NULL) {
            if (m_pPlayers[i]->getID() == id) {
                pLoginPlayer = dynamic_cast<LoginPlayer*>(m_pPlayers[i]);
                break;
            }
        }
    }

    if (pLoginPlayer == NULL)
        throw NoSuchElementException("No player with that ID exists.");

    return pLoginPlayer;

    __END_CATCH
}


//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
LoginPlayer* LoginPlayerManager::getPlayer(const string& id) const {
    LoginPlayer* pPlayer;

    __ENTER_CRITICAL_SECTION(m_Mutex)

    pPlayer = getPlayer_NOLOCKED(id);

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    return pPlayer;
}


//--------------------------------------------------------------------------------
// Send a given packet to every player connected to the login server.
//--------------------------------------------------------------------------------
void LoginPlayerManager::broadcastPacket(Packet* pPacket) {
    __BEGIN_TRY

    const de::DescriptorRange walk = de::descriptorRange((int)m_MinFD, (int)m_MaxFD, (int)nMaxPlayers);
    for (int i = walk.first; i <= walk.last; i++) {
        if (m_pPlayers[i] != NULL) {
            m_pPlayers[i]->sendPacket(pPacket);
        }
    }

    __END_CATCH
}


//--------------------------------------------------------------------------------
// Send a given packet to a given player connected to the login server.
//--------------------------------------------------------------------------------
void LoginPlayerManager::sendPacket(const string& id, Packet* pPacket) {
    __BEGIN_TRY

    const de::DescriptorRange walk = de::descriptorRange((int)m_MinFD, (int)m_MaxFD, (int)nMaxPlayers);
    for (int i = walk.first; i <= walk.last; i++) {
        if (m_pPlayers[i] != NULL) {
            if (m_pPlayers[i]->getID() == id) {
                m_pPlayers[i]->sendPacket(pPacket);
                break;
            }
        }
    }

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////
// get debug string
//////////////////////////////////////////////////////////////////////
string LoginPlayerManager::toString() const {
    __BEGIN_TRY

    StringStream msg;
    msg << "LoginPlayerManager(nPlayers:" << size() << ")";
    return msg.toString();

    __END_CATCH
}
