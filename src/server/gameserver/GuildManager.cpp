////////////////////////////////////////////////////////////////////////
// Filename    : GuildManager.cpp
// Description :
////////////////////////////////////////////////////////////////////////

#include "GuildManager.h"

#include "GameContext.h"
#include "Guild.h"
#include "Properties.h"
#include "StringStream.h"
#include "repository/GuildRepository.h"

#ifdef __GAME_SERVER__
#include "CastleInfoManager.h"
#include "PlayerCreature.h"
#include "Zone.h"
#include "ZoneGroup.h"
#include "ZoneGroupManager.h"
#include "ZoneInfoManager.h"
#include "ZoneUtil.h"
#include "war/WarScheduler.h"
#endif
#include "GCActiveGuildList.h"
#include "GCWaitGuildList.h"

////////////////////////////////////////////////////////////////////////
// class GuildManager member methods
////////////////////////////////////////////////////////////////////////

GuildManager::GuildManager()

{
    m_Mutex.setName("GuildManager");
}

GuildManager::~GuildManager()

{
    // No lock: nothing else may use the manager while it is destroyed.
    // Delete every guild object from memory.
    unordered_map<GuildID_t, Guild*>::iterator itr = m_Guilds.begin();
    for (; itr != m_Guilds.end(); itr++) {
        Guild* pGuild = itr->second;
        SAFE_DELETE(pGuild);
    }

    m_Guilds.clear();

    for (size_t i = 0; i < m_RetiredGuilds.size(); i++)
        SAFE_DELETE(m_RetiredGuilds[i]);
    m_RetiredGuilds.clear();
}


void GuildManager::init()

{
    // The body (the MaxGuildID / per-race MaxZoneID probes, then load())
    // lived here under __SHARED_SERVER__, which no build of this file
    // defines: the sharedserver compiles its own GuildManager.cpp. Gone
    // with its SQL; the gameserver's GuildManager::init() was always empty.
}


void GuildManager::load()

{
    __ENTER_CRITICAL_SECTION(m_Mutex)

    GuildRepository& repository = defaultGuildRepository();

    {
        // Read the guilds from the DB.
        vector<GuildListRow> guilds = repository.loadGuildsInStates(Guild::GUILD_STATE_WAIT, Guild::GUILD_STATE_ACTIVE);

        for (size_t g = 0; g < guilds.size(); g++) {
            const GuildListRow& row = guilds[g];
            GuildState_t state = row.state;

            // Only guilds waiting for registration or active in this world are added.
            if (state == Guild::GUILD_STATE_WAIT || state == Guild::GUILD_STATE_ACTIVE) {
                Guild* pGuild = new Guild();

                pGuild->setID(row.id);
                pGuild->setName(row.name);
                pGuild->setType(row.type);
                pGuild->setRace(row.race);
                pGuild->setState(state);
                pGuild->setServerGroupID(row.serverGroupID);
                pGuild->setZoneID(row.zoneID);
                pGuild->setMaster(row.master);
                pGuild->setDate(row.date);
                pGuild->setIntro(row.intro);

                addGuild_NOBLOCKED(pGuild);
            }
        }

        // Read the guild members from the DB.
        vector<GuildMemberListRow> members = repository.loadActiveMembers();

        for (size_t m = 0; m < members.size(); m++) {
            GuildMember* pMember = new GuildMember();

            pMember->setGuildID(members[m].guildID);
            pMember->setName(members[m].name);
            pMember->setRank(members[m].rank);

            if (pMember->getRank() == GuildMember::GUILDMEMBER_RANK_WAIT)
                pMember->setRequestDateTime(members[m].requestDateTime);

            pMember->setLogOn(members[m].logOn);

            Guild* pGuild = getGuild_NOBLOCKED(pMember->getGuildID());

            if (pGuild != NULL)
                pGuild->addMember(pMember);
        }
    }

    __LEAVE_CRITICAL_SECTION(m_Mutex)
}


void GuildManager::addGuild(Guild* pGuild) {
    Assert(pGuild != NULL);

    __ENTER_CRITICAL_SECTION(m_Mutex)

    if (m_Guilds.contains(pGuild->getID()))
        throw DuplicatedException();
    m_Guilds[pGuild->getID()] = pGuild;

    __LEAVE_CRITICAL_SECTION(m_Mutex)
}


void GuildManager::addGuild_NOBLOCKED(Guild* pGuild) {
    Assert(pGuild != NULL);

    if (m_Guilds.contains(pGuild->getID()))
        throw DuplicatedException();
    m_Guilds[pGuild->getID()] = pGuild;
}


void GuildManager::deleteGuild(GuildID_t id) {
    __ENTER_CRITICAL_SECTION(m_Mutex)

    unordered_map<GuildID_t, Guild*>::iterator itr = m_Guilds.find(id);
    if (itr == m_Guilds.end())
        throw NoSuchElementException();

#ifdef __GAME_SERVER__
    CastleInfoManager& castles = de::gameContext().castleInfos();

    {
        const unordered_map<ZoneID_t, CastleInfo*>& castleInfos = castles.getCastleInfos();

        unordered_map<ZoneID_t, CastleInfo*>::const_iterator itr = castleInfos.begin();
        unordered_map<ZoneID_t, CastleInfo*>::const_iterator endItr = castleInfos.end();

        for (; itr != endItr; ++itr) {
            Zone* pZone = getZoneByZoneID(itr->first);
            if (pZone != NULL) {
                // A scheduler belongs to its zone's group, whose thread runs
                // its heartbeat and executes the wars it hands out. Asking it
                // anything here, on the shared-server link thread with the guild
                // mutex held, would take its mutex in the order the zone thread
                // reverses when a war schedule names a guild, so both the test
                // and the cancel are posted to the owner and run under the group
                // mutex. The scheduler is looked up there because a zone reload
                // replaces it.
                //
                // The cancel writes the guild's waiting wars out of the table
                // before reloading from it; a plain reload reads the war back
                // in, because the row is still waiting. The sharedserver
                // cancels the rows the guild holds the first attacker slot of
                // when it purges the guild, but not the ones it joined as a
                // later challenger; it also deletes the guild's reinforcement
                // registrations, before or after this runs, and either order
                // leaves the reloaded siege without the reinforcement.
                //
                // A castle the guild holds turns common first, in the same
                // command: the change writes the castle's row and broadcasts
                // into its zone, which is the castle thread's work, and it is
                // decided there, against the owner the castle has when the
                // command runs, so an owner change posted before this one (a
                // war the guild just won) is turned common too, and one that
                // handed the castle on is left alone.
                ZoneGroup* pZoneGroup = pZone->getZoneGroup();
                if (pZoneGroup != NULL)
                    pZoneGroup->post([pZone, id] {
                        de::gameContext().castleInfos().settleGuildDeletion(pZone->getZoneID(), id);

                        WarScheduler* pScheduler = pZone->getWarScheduler();
                        if (pScheduler != NULL && pScheduler->hasSchedule(id))
                            pScheduler->cancelGuildSchedulesOf(id);
                    });
            }
        }
    }

    // The guild's union standing is not settled here. Clearing it reads
    // guild masters back out of this manager (GuildUnionManager::
    // removeGuildFromUnion), which would take m_Mutex a second time, so the
    // caller runs the teardown before it calls this -- while the guild is
    // still in the table.
#endif

    // Retire, don't free (see m_RetiredGuilds): other threads may still
    // hold the pointer. retire() marks the guild and its members gone, so a
    // holder's next membership or rank check answers for a guild that is no
    // longer there.
    itr->second->retire();
    m_RetiredGuilds.push_back(itr->second);
    m_Guilds.erase(itr);

    __LEAVE_CRITICAL_SECTION(m_Mutex)
}


Guild* GuildManager::getGuild(GuildID_t id)

{
    // Guild pointer to return
    Guild* pGuild;

    __ENTER_CRITICAL_SECTION(m_Mutex)

    unordered_map<GuildID_t, Guild*>::iterator itr = m_Guilds.find(id);

    if (itr == m_Guilds.end()) {
        return NULL;
    }

    pGuild = itr->second;

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    return pGuild;
}


Guild* GuildManager::getGuild_NOBLOCKED(GuildID_t id)

{
    // Guild pointer to return
    Guild* pGuild;

    unordered_map<GuildID_t, Guild*>::iterator itr = m_Guilds.find(id);

    if (itr == m_Guilds.end()) {
        return NULL;
    }

    pGuild = itr->second;

    return pGuild;
}


void GuildManager::clear()

{
    __ENTER_CRITICAL_SECTION(m_Mutex)

    retireAll_NOBLOCKED();

    __LEAVE_CRITICAL_SECTION(m_Mutex)
}

void GuildManager::clear_NOBLOCKED() {
    retireAll_NOBLOCKED();
}

// clear()/clear_NOBLOCKED() run mid-session: SGGuildInfo (the sharedserver's
// answer to GSRequestGuildInfo on every (re)connect) empties the table
// before reloading it. Zone threads may be holding Guild* and GuildMember*
// from before, so the guilds are retired here like a deleted one, never
// freed; only the destructor frees the retired list.
void GuildManager::retireAll_NOBLOCKED() {
    m_RetiredGuilds.reserve(m_RetiredGuilds.size() + m_Guilds.size());
    for (HashMapGuildItor itr = m_Guilds.begin(); itr != m_Guilds.end(); itr++) {
        itr->second->retire();
        m_RetiredGuilds.push_back(itr->second);
    }
    m_Guilds.clear();
}

void GuildManager::makeWaitGuildList(GCWaitGuildList& gcWaitGuildList, GuildRace_t race)

{
    __ENTER_CRITICAL_SECTION(m_Mutex)

    HashMapGuildConstItor itr = m_Guilds.begin();
    for (; itr != m_Guilds.end(); itr++) {
        Guild* pGuild = itr->second;
        if (pGuild->getState() == Guild::GUILD_STATE_WAIT && pGuild->getRace() == race) {
            GuildInfo* pGuildInfo = new GuildInfo();
            pGuild->makeInfo(pGuildInfo);

            gcWaitGuildList.addGuildInfo(pGuildInfo);
        }
    }

    __LEAVE_CRITICAL_SECTION(m_Mutex)
}

void GuildManager::makeActiveGuildList(GCActiveGuildList& gcActiveGuildList, GuildRace_t race)

{
    __ENTER_CRITICAL_SECTION(m_Mutex)

    HashMapGuildConstItor itr = m_Guilds.begin();
    for (; itr != m_Guilds.end(); itr++) {
        Guild* pGuild = itr->second;
        if (pGuild->getState() == Guild::GUILD_STATE_ACTIVE && pGuild->getRace() == race) {
            GuildInfo* pGuildInfo = new GuildInfo();
            pGuild->makeInfo(pGuildInfo);

            gcActiveGuildList.addGuildInfo(pGuildInfo);
        }
    }

    __LEAVE_CRITICAL_SECTION(m_Mutex)
}

void GuildManager::heartbeat()

{}

string GuildManager::toString() const

{
    StringStream msg;
    return msg.toString();
}

bool GuildManager::isGuildMaster(GuildID_t guildID, PlayerCreature* pPC)

{
#ifdef __GAME_SERVER__
    {
        Guild* pGuild = getGuild(guildID);

        if (pGuild == NULL)
            return false;

        //	cout << "isGuildMaster : " << pGuild->getMaster() << ", " << pPC->getName() << endl;
        return (pGuild->getMaster() == pPC->getName());
    }
#else
    return false;
#endif
}

// Does the guild own a castle?
bool GuildManager::hasCastle(GuildID_t guildID)

{
    bool bHasCastle = false;

#ifdef __GAME_SERVER__

    if (defaultGuildRepository().countCastlesOfGuild((int)guildID) > 0) {
        bHasCastle = true;
    }

#endif

    return bHasCastle;
}

// Does the guild own a castle?
bool GuildManager::hasCastle(GuildID_t guildID, ServerID_t& serverID, ZoneID_t& zoneID)

{
    bool bHasCastle = false;

#ifdef __GAME_SERVER__

    int castleServerID = 0;
    int castleZoneID = 0;
    if (defaultGuildRepository().loadCastleOfGuild((int)guildID, castleServerID, castleZoneID)) {
        serverID = castleServerID;
        zoneID = castleZoneID;

        bHasCastle = true;
    }

#endif

    return bHasCastle;
}

// Has the guild applied for a war?
bool GuildManager::hasWarSchedule(GuildID_t guildID)

{
    bool bHasWarSchedule = false;

#ifdef __GAME_SERVER__

    if (defaultGuildRepository().countWarSchedulesOfAttacker((int)guildID) > 0) {
        bHasWarSchedule = true;
    }

    if (defaultGuildRepository().countReinforceRegistrations((int)guildID) > 0) {
        bHasWarSchedule = true;
    }

#endif

    return bHasWarSchedule;
}

bool GuildManager::hasActiveWar(GuildID_t guildID)

{
    bool bHasActiveWar = false;

#ifdef __GAME_SERVER__

    ServerID_t serverID;
    ZoneID_t zoneID;

    if (hasCastle(guildID, serverID, zoneID)) {
        // The guild owns a castle: is a guild war against that castle running?
        if (defaultGuildRepository().countStartedWarsAtCastle(serverID, zoneID) > 0) {
            bHasActiveWar = true;
        }
    } else {
        // No castle: is the guild attacking some castle in a running war?
        if (defaultGuildRepository().countStartedWarsOfAttacker((int)guildID) > 0) {
            bHasActiveWar = true;
        }
    }

#endif

    return bHasActiveWar;
}


string GuildManager::getGuildName(GuildID_t guildID)

{
    Guild* pGuild = getGuild(guildID);

    if (pGuild != NULL)
        return pGuild->getName();

    return "";
}
