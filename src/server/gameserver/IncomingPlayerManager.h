//////////////////////////////////////////////////////////////////////////////
// Filename    : IncomingPlayerManager.h
// Written by  : reiot@ewestsoft.com
// Description :
//////////////////////////////////////////////////////////////////////////////

#ifndef __INCOMING_PLAYER_MANAGER_H__
#define __INCOMING_PLAYER_MANAGER_H__

#include <functional>
#include <memory>

#include "ConnectionInfoManager.h"
#include "DatagramSocket.h"
#include "DescriptorPollSet.h"
#include "Exception.h"
#include "GamePlayer.h"
#include "Mutex.h"
#include "PlayerManager.h"
#include "ProxyAcceptor.h"
#include "ServerSocket.h"
#include "Types.h"

namespace de {
class GameContext;
}

//////////////////////////////////////////////////////////////////////////////
// class IncomingPlayerManager;
//
// PlayerManager covers every player connected to the game server and
// ZonePlayerManager covers the players belonging to each zone group, while
// IncomingPlayerManager manages players that are connected to the game
// server but whose creature has not been loaded yet.
//
// After a new connection is authenticated, the creature is loaded and tied to the player.
// Once the creature is loaded, the player and the creature are handed to another zone group.
//
// Threads and m_Mutex. The main thread (ClientManager::run) is the only
// thread that changes the player table, the descriptor range and the poll
// set: it accepts connections, merges the players the zone threads queue
// with pushPlayer() in heartbeat(), and removes players from its own walks
// and from CGReady's handler, which it dispatches. Every one of those writes
// takes m_Mutex. No other thread looks a player up here: a pointer handed
// out under m_Mutex would outlive it while this thread disconnects and
// deletes the player, so the login link's replies for a player logging out
// reach it through its mailbox (PlayerMailbox.h, de::postToAccount), which
// processCommands() drains. A zone thread takes m_Mutex only to queue a
// player with pushPlayer() while it holds its group's mutex (group mutex ->
// m_Mutex).
//
// pollSockets() therefore follows ZonePlayerManager: fill() and collect()
// under m_Mutex, the wait between them without it. The input, output,
// exception and command walks run without m_Mutex, like
// ZonePlayerManager's: the thread walking is the only thread that writes
// what they read, so they cannot see a half-made change. Holding the mutex
// across them would also be wrong in two ways. Their removals call
// deletePlayer() and the accept path calls addPlayer(), which take the
// non-recursive m_Mutex themselves. And they disconnect and destroy
// players, which saves to the database and takes the player finder, guild
// and SharedServerManager locks: a zone thread's pushPlayer(), made under
// its group mutex, would wait behind that work on every tick, where today
// it waits only behind the kicked players heartbeat() removes under
// m_Mutex. clearPlayers() and destruction write without m_Mutex and require
// the caller to stop concurrent walks and queue producers first.
//////////////////////////////////////////////////////////////////////////////

class IncomingPlayerManager : public PlayerManager {
public:
    IncomingPlayerManager();
    // Own the listener and publish connection info for this scope. The context
    // and its previous binding must outlive it. No database initialization is
    // performed here; an empty logging predicate disables admission logs.
    IncomingPlayerManager(std::unique_ptr<ServerSocket> listener, de::GameContext& context,
                          std::function<bool()> logConnections = {});
    ~IncomingPlayerManager() noexcept(false);

public:
    // initialize
    void init();

    // broadcast packet to all players
    void broadcast(Packet* pPacket);

    // The following methods are called by the main thread, once a tick.

    // Ask the kernel which of this manager's descriptors are ready.
    void pollSockets();

    // process all players' inputs
    void processInputs();

    // process all players' outputs
    void processOutputs();

    // process all players' exceptions
    void processExceptions();

    // process all players' commands
    void processCommands();

    // Accept a connection from the public listener, or admit `forwarded`, a
    // gateway connection. Ownership is retained through authorization and
    // registration, including if construction or diagnostics throws.
    bool acceptNewConnection(Socket* forwarded = nullptr);

    void copyPlayers();

    // add/delete player
    void addPlayer(Player* pGamePlayer);
    void addPlayer_NOBLOCKED(Player* pGamePlayer);
    void deletePlayer(SOCKET fd);
    void deletePlayer_NOBLOCKED(SOCKET fd);

    // lock/unlock
    void lock() {
        m_Mutex.lock();
    }
    void unlock() {
        m_Mutex.unlock();
    }

    // push Player to queue
    void pushPlayer(GamePlayer* pGamePlayer);

    void pushOutPlayer(GamePlayer* pGamePlayer);

    // Queue's Player Add Manager
    void heartbeat();

    void deleteQueuePlayer(GamePlayer* pGamePlayer);

    // Disconnect and release all owned players, emptying both queues and the
    // table. The caller must first stop concurrent users and queue producers.
    void clearPlayers();

private:
    // TCP server socket and socket descriptor
    std::unique_ptr<ServerSocket> m_pServerSocket;
    std::unique_ptr<de::ProxyAcceptor> m_ProxyAcceptor;
    SOCKET m_SocketID;

    // The socket descriptors of the players this manager owns, with what each
    // one was last reported ready for. It has a slot per player table slot,
    // so every descriptor the table can hold can be watched.
    de::DescriptorPollSet m_PollSet;

    // How long each poll waits, in milliseconds.
    int m_TimeoutMilliseconds;

    // min_fd, max_fd
    // Used to speed up iterating over the player table.
    SOCKET m_MinFD;
    SOCKET m_MaxFD;

    // mutex
    mutable Mutex m_Mutex;

    list<GamePlayer*> m_PlayerListQueue;
    list<GamePlayer*> m_PlayerOutListQueue;

    int m_CheckValue; // by sigi. for debugging. 2002.11.11

    mutable Mutex m_MutexOut;

    void releasePlayers(bool disconnect) noexcept;

    de::GameContext& m_Context;
    std::function<bool()> m_LogConnections;
    std::unique_ptr<ConnectionInfoManager> m_pConnectionInfoManager;
    ConnectionInfoManager* m_PreviousConnectionInfoManager = nullptr;
};

#endif
