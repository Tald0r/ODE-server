//////////////////////////////////////////////////////////////////////
//
// Filename    : ZonePlayerManager.h
// Written by  : reiot@ewestsoft.com
// Description :
//
//////////////////////////////////////////////////////////////////////

#ifndef __ZONE_PLAYER_MANAGER_H__
#define __ZONE_PLAYER_MANAGER_H__

// include files
#include "DescriptorPollSet.h"
#include "Effect.h"
#include "Exception.h"
#include "Mutex.h"
#include "Player.h"
#include "PlayerManager.h"
#include "Socket.h"
#include "Types.h"

class GamePlayer;
class BroadcastFilter;

//////////////////////////////////////////////////////////////////////
//
// class ZonePlayerManager;
//
// Manages the players that belong to this ZoneGroup.
//
//////////////////////////////////////////////////////////////////////

class ZonePlayerManager : public PlayerManager {
public:
    typedef pair<BroadcastFilter*, SocketOutputStream*> PairFilterStream;

public:
    // constructor
    ZonePlayerManager();

    // All users and queue producers must stop before destruction.
    ~ZonePlayerManager() noexcept;

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

    // broadcast packet
    void broadcastPacket(Packet* pPacket);
    void broadcastPacket_NOBLOCKED(Packet* pPacket);
    void pushBroadcastPacket(Packet* pPacket, BroadcastFilter* pFilter = NULL);
    void flushBroadcastPacket();

    // add player to zone player manager
    using PlayerManager::addPlayer;
    void addPlayer(GamePlayer* pGamePlayer);
    void addPlayer_NOBLOCKED(GamePlayer* pGamePlayer);

    // delete player from zone player manager
    void deletePlayer(SOCKET fd);
    void deletePlayer_NOBLOCKED(SOCKET fd);
    void deletePlayer(Player* pPlayer) {
        deletePlayer(pPlayer->getSocket()->getSOCKET());
    }

    // get player
    Player* getPlayer(SOCKET fd);

    // get Player by PhoneNumber
    Player* getPlayerByPhoneNumber(PhoneNumber_t PhoneNumber);

    // All Member Save
    void save();

    void copyPlayers();

    // Adopt an unregistered player on success; the caller retains ownership if
    // queue allocation fails. Registered players use moveToOutgoing instead.
    void pushPlayer(GamePlayer* pGamePlayer);
    void pushOutPlayer(GamePlayer* pGamePlayer);
    // Called by this zone's owning thread under its group mutex. Failure keeps
    // the table owner; success removes its table/poll entry before returning.
    void moveToOutgoing(GamePlayer* player);
    void processPlayerListQueue();

    // Queue's Player Add Manager
    void heartbeat();

    // delete Queue Player
    void deleteQueuePlayer(GamePlayer* pGamePlayer);

    void removeFlag(Effect::EffectClass EC);

    void lock() {
        m_Mutex.lock();
    }
    void unlock() {
        m_Mutex.unlock();
    }

    // Disconnect and release every player. Requires stopped concurrent users
    // and queue producers, just like destruction; repeated clear is safe.
    void clearPlayers();

    void setZGID(ZoneGroupID_t id) {
        m_ZGID = id;
    }
    ZoneGroupID_t getZGID() const {
        return m_ZGID;
    }

private:
    void releasePlayers(bool disconnect) noexcept;

    // The socket descriptors of the players in this group, with what each one
    // was last reported ready for. It has a slot per player table slot, so
    // every descriptor the table can hold can be watched.
    de::DescriptorPollSet m_PollSet;

    // How long each poll waits, in milliseconds.
    int m_TimeoutMilliseconds;

    // min_fd, max_fd
    // Used to speed up iteration over the player table.
    SOCKET m_MinFD;
    SOCKET m_MaxFD;

    // mutex
    mutable Mutex m_Mutex;
    mutable Mutex m_Mutex2;
    mutable Mutex m_MutexBroadcast;

    list<GamePlayer*> m_PlayerListQueue;
    list<GamePlayer*> m_PlayerOutListQueue;
    list<PairFilterStream> m_BroadcastQueue;

    ZoneGroupID_t m_ZGID;
};

#endif
