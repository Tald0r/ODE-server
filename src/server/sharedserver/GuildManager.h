//////////////////////////////////////////////////////////////////////////////
// Filename    : GuildManager.h
// Description :
//////////////////////////////////////////////////////////////////////////////

#ifndef __SHARED_SERVER_GUILD_MANAGER_H__
#define __SHARED_SERVER_GUILD_MANAGER_H__

#include <unordered_map>

#include "Assert.h"
#include "Exception.h"
#include "Mutex.h"
#include "Timeval.h"
#include "Types.h"

//////////////////////////////////////////////////////////////////////////////
// class GuildManager
// Keeps the guilds that are currently active or waiting in a map in memory
// and handles the registration/deletion of new guilds and members.
//
//////////////////////////////////////////////////////////////////////////////

class Guild;
class SharedGuildRepository;

typedef unordered_map<GuildID_t, Guild*> HashMapGuild;
typedef unordered_map<GuildID_t, Guild*>::iterator HashMapGuildItor;
typedef unordered_map<GuildID_t, Guild*>::const_iterator HashMapGuildConstItor;

#ifdef __SHARED_SERVER__
class SGGuildInfo;
#endif

class GCWaitGuildList;
class GCActiveGuildList;
class PlayerCreature;

class GuildManager {
    ///// Member methods /////

public: // constructor & destructor
    GuildManager() noexcept;
    ~GuildManager() noexcept;


public: // initializing related methods
    void init() noexcept(false);
    // Load with quiescent users of borrowed guild/member pointers. Failure
    // retains the previous graph; successful replacement invalidates those pointers.
    void load() noexcept(false);
    void load(SharedGuildRepository& repository);


public: // memory related methods
    void addGuild(Guild* pGuild) noexcept(false);
    void addGuild_NOBLOCKED(Guild* pGuild) noexcept(false);
    void deleteGuild(GuildID_t id) noexcept(false);
    Guild* getGuild(GuildID_t id) noexcept(false);
    Guild* getGuild_NOBLOCKED(GuildID_t id) noexcept(false);

    void clear() noexcept(false);
    void clear_NOBLOCKED();


public: // misc methods
    ushort getGuildSize() const noexcept {
        return m_Guilds.size();
    }
    HashMapGuild& getGuilds() noexcept {
        return m_Guilds;
    }
    const HashMapGuild& getGuilds_const() const noexcept {
        return m_Guilds;
    }

#ifdef __SHARED_SERVER__
public:
    void makeSGGuildInfo(SGGuildInfo& sgGuildInfo) noexcept(false);
#endif

    void makeWaitGuildList(GCWaitGuildList& gcWaitGuildList, GuildRace_t race) noexcept(false);
    void makeActiveGuildList(GCActiveGuildList& gcWaitGuildList, GuildRace_t race) noexcept(false);

public:
    void lock() {
        m_Mutex.lock();
    }
    void unlock() {
        m_Mutex.unlock();
    }


public:
    void heartbeat() noexcept(false);

public:
    bool isGuildMaster(GuildID_t guildID, PlayerCreature* pPC) noexcept(false);

    string getGuildName(GuildID_t guildID) noexcept(false);

    // Does the guild hold a castle?
    bool hasCastle(GuildID_t guildID) noexcept(false);
    bool hasCastle(GuildID_t guildID, ServerID_t& serverID, ZoneID_t& zoneID) noexcept(false);

    // Has the guild filed a war schedule?
    bool hasWarSchedule(GuildID_t guildID) noexcept(false);

    // Is there a war currently running?
    bool hasActiveWar(GuildID_t guidlID) noexcept(false);

public: // debug
    string toString(void) const noexcept;


    ///// Member data /////

protected:
    unordered_map<GuildID_t, Guild*> m_Guilds; // map of guild information

    Timeval m_WaitMemberClearTime; // time at which heartbeat clears the Wait members

    // mutex
    mutable Mutex m_Mutex;
};

#endif // __GUILDINFO_H__
