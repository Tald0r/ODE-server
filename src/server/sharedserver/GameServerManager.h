//////////////////////////////////////////////////////////////////////////////
// Filename    : GameServerManager.h
// Written by  : reiot@ewestsoft.com
// Description :
//////////////////////////////////////////////////////////////////////////////

#ifndef __GAME_SERVER_MANAGER_H__
#define __GAME_SERVER_MANAGER_H__

#include <memory>

#include "DescriptorPollSet.h"
#include "Exception.h"
#include "GameServerPlayer.h"
#include "ManagedThread.h"
#include "Mutex.h"
#include "ServerSocket.h"
#include "Timeval.h"
#include "Types.h"


//////////////////////////////////////////////////////////////////////////////
//
// class GameServerManager;
//
// The sharedserver's one worker: it owns the listening socket and the game
// server connections, dispatches their GS packets and runs the guild
// heartbeat, all from run().
//
// Threads and m_Mutex. This thread is the only one that touches the game
// server table, the descriptor range and the poll set. It accepts and drops
// connections (addGameServerPlayer / deleteGameServerPlayer) and broadcasts
// (from the GS handlers and the guild heartbeat it runs), each of which
// takes m_Mutex; the sharedserver's main thread only waits for shutdown.
//
// pollSockets() follows ZonePlayerManager: fill() and collect() under
// m_Mutex, the wait between them without it. The input, output, exception
// and command walks run without m_Mutex, as ZonePlayerManager's do: the
// thread walking is the only writer of what they read, and the mutex is
// non-recursive while the calls inside the walks take it themselves -- the
// add and delete above, and broadcast() from the handlers processCommands()
// dispatches.
//
//////////////////////////////////////////////////////////////////////////////

class GameServerManager : public ManagedThread {
public:
    GameServerManager();
    // Adopt a bound/listening socket, including if construction fails. This
    // form needs no configuration binding; both forms make it nonblocking.
    explicit GameServerManager(std::unique_ptr<ServerSocket> listener);
    ~GameServerManager() noexcept;

public:
    // Maximum number of game servers the shared server accepts
    const static uint nMaxGameServers = 100;

    // initialize
    void init();

    void run() override;

    // broadcast packet to all players
    void broadcast(Packet* pPacket);
    void broadcast(Packet* pPacket, Player* pPlayer);


    // Ask the kernel which of this manager's descriptors are ready.
    void pollSockets();

    // process all inputs
    void processInputs();

    // process all outputs
    void processOutputs();

    // process all exceptions
    void processExceptions();

    // process all commands
    void processCommands();

    // accept new connection
    void acceptNewConnection();

    // Adopt only after successful registration. Deletion detaches the entry;
    // its caller owns the removed player and must destroy it.
    void addGameServerPlayer(GameServerPlayer* pGameServerPlayer);
    void deleteGameServerPlayer(SOCKET fd);

    // lock/unlock
    void lock() {
        m_Mutex.lock();
    }
    void unlock() {
        m_Mutex.unlock();
    }

private:
    // TCP server socket and socket descriptor
    std::unique_ptr<ServerSocket> m_pServerSocket;
    SOCKET m_SocketID;

    // The socket descriptors of the game servers that belong here, with what
    // each one was last reported ready for. It has a slot per game server
    // table slot, so every descriptor the table can hold can be watched.
    de::DescriptorPollSet m_PollSet;

    // How long each poll waits, in milliseconds.
    int m_TimeoutMilliseconds;

    // min_fd, max_fd
    // Used to speed the iteration over the game server table up.
    SOCKET m_MinFD;
    SOCKET m_MaxFD;

    // mutex
    mutable Mutex m_Mutex;


    // Array of game server pointers, indexed by socket descriptor. Every
    // slot starts empty: the loops read slots no connection has filled.
    GameServerPlayer* m_pGameServerPlayers[nMaxGameServers] = {};
};

#endif
