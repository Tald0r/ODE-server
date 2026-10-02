////////////////////////////////////////////////////////////////////////
// Filename    : GuildManager.cpp
// Description :
////////////////////////////////////////////////////////////////////////

#include "GuildManager.h"

#include <algorithm>
#include <charconv>
#include <memory>
#include <utility>

#include <string_view>
#include <system_error>

#include "Guild.h"
#include "Properties.h"
#include "StringStream.h"
#include "repository/SharedGuildRepository.h"

#ifdef __SHARED_SERVER__
#include "GameServerManager.h"
#include "SGExpelGuildMemberOK.h"
#include "SGGuildInfo.h"
#include "SharedContext.h"
#endif

#include "GCActiveGuildList.h"
#include "GCWaitGuildList.h"
#include "KernelContext.h"

namespace {

template <typename ID> ID checkedStartupID(long long value) {
    if (!std::in_range<ID>(value))
        throw Error("shared guild startup ID is outside its storage range");
    return static_cast<ID>(value);
}

unsigned int readIDComponent(const Properties& config, const char* key) {
    const std::string value = config.getProperty(key);
    std::string_view text(value);
    const auto first = text.find_first_not_of(" \t\r\n\f\v");
    if (first == std::string_view::npos)
        text = {};
    else
        text = text.substr(first, text.find_last_not_of(" \t\r\n\f\v") - first + 1);
    if (!text.empty() && text.front() == '+')
        text.remove_prefix(1);

    const auto invalid = [&] { return Error(std::string(key) + " must be a nonnegative decimal guild ID component"); };
    if (text.empty())
        throw invalid();
    unsigned int component = 0;
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), component);
    if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size())
        throw invalid();
    return component;
}

ZoneID_t prepareZoneMaximum(SharedGuildRepository& repo, int race, ZoneID_t previous) {
    const int count = repo.countGuildsOfRace(race);
    if (count < 0)
        throw Error("invalid shared guild race count");
    const auto next = checkedStartupID<ZoneID_t>(static_cast<long long>(previous) + 1);
    if (count == 0)
        return next; // MAX over an empty table would yield NULL.
    const auto stored = checkedStartupID<ZoneID_t>(repo.loadMaxGuildZoneIDOfRace(race));
    return std::max(stored, next);
}

} // namespace


////////////////////////////////////////////////////////////////////////
// class GuildManager member methods
////////////////////////////////////////////////////////////////////////

GuildManager::GuildManager() noexcept {
    m_Mutex.setName("GuildManager");
}

GuildManager::~GuildManager() noexcept {
    try {
        __ENTER_CRITICAL_SECTION(m_Mutex)

        // Free every guild object from memory.
        unordered_map<GuildID_t, Guild*>::iterator itr = m_Guilds.begin();
        for (; itr != m_Guilds.end(); itr++) {
            Guild* pGuild = itr->second;
            SAFE_DELETE(pGuild);
        }

        m_Guilds.clear();

        __LEAVE_CRITICAL_SECTION(m_Mutex)
    } catch (...) {
        // destructor must not throw
    }
}


void GuildManager::init() noexcept(false) {
#ifdef __SHARED_SERVER__
    init(defaultSharedGuildRepository(), de::kernelContext().config());
#endif
}

void GuildManager::init(SharedGuildRepository& repo, const Properties& config) {
    CriticalSection lock{m_Mutex};

    // New guild ids are handed out above the largest one in the table, so
    // the manager reads that maximum once at startup. An empty table starts
    // the numbering from the configured dimension and world.
    const int count = repo.countGuilds();
    if (count < 0)
        throw Error("invalid shared guild count");
    GuildID_t guildID;
    if (count == 0) {
        const auto dimension = readIDComponent(config, "Dimension");
        const auto world = readIDComponent(config, "WorldID");
        guildID = checkedStartupID<GuildID_t>(dimension * 10000LL + world * 3000LL + 100);
    } else {
        guildID = checkedStartupID<GuildID_t>(repo.loadMaxGuildID());
    }
    const auto slayerZone = prepareZoneMaximum(repo, Guild::GUILD_RACE_SLAYER, Guild::getMaxSlayerZoneID());
    const auto vampireZone = prepareZoneMaximum(repo, Guild::GUILD_RACE_VAMPIRE, Guild::getMaxVampireZoneID());
    const auto oustersZone = prepareZoneMaximum(repo, Guild::GUILD_RACE_OUSTERS, Guild::getMaxOustersZoneID());

    // Loading retains the old graph on failure. Once it publishes, only
    // nonthrowing counter assignments remain, still under the table lock.
    loadUnderLock(repo);
    Guild::setMaxGuildID(guildID);
    Guild::setMaxSlayerZoneID(slayerZone);
    Guild::setMaxVampireZoneID(vampireZone);
    Guild::setMaxOustersZoneID(oustersZone);
}


void GuildManager::load() noexcept(false) {
    load(defaultSharedGuildRepository());
}

void GuildManager::load(SharedGuildRepository& repo) {
    CriticalSection lock{m_Mutex};
    loadUnderLock(repo);
}

void GuildManager::loadUnderLock(SharedGuildRepository& repo) {
    std::unordered_map<GuildID_t, std::unique_ptr<Guild>> replacement;

    for (const auto& row : repo.loadGuildsInStates(Guild::GUILD_STATE_WAIT, Guild::GUILD_STATE_ACTIVE)) {
        if (row.state < Guild::GUILD_STATE_ACTIVE || row.state >= Guild::GUILD_STATE_MAX)
            throw Error("invalid shared guild state");
        if (row.state != Guild::GUILD_STATE_WAIT && row.state != Guild::GUILD_STATE_ACTIVE)
            continue;
        if (!std::in_range<GuildID_t>(row.id) || !std::in_range<ServerGroupID_t>(row.serverGroupID) ||
            !std::in_range<ZoneID_t>(row.zoneID) || row.type < Guild::GUILD_TYPE_NORMAL ||
            row.type >= Guild::GUILD_TYPE_MAX || row.race < Guild::GUILD_RACE_SLAYER ||
            row.race >= Guild::GUILD_RACE_MAX)
            throw Error("invalid shared guild ID, type or race");

        auto guild = std::make_unique<Guild>();
        const auto guildID = static_cast<GuildID_t>(row.id);
        guild->setID(guildID);
        guild->setName(row.name);
        guild->setType(static_cast<GuildType_t>(row.type));
        guild->setRace(static_cast<GuildRace_t>(row.race));
        guild->setState(static_cast<GuildState_t>(row.state));
        guild->setServerGroupID(static_cast<ServerGroupID_t>(row.serverGroupID));
        guild->setZoneID(static_cast<ZoneID_t>(row.zoneID));
        guild->setMaster(row.master);
        guild->setDate(row.date);
        guild->setIntro(row.intro);
        if (!replacement.emplace(guildID, std::move(guild)).second)
            throw DuplicatedException();
    }

    for (const auto& row : repo.loadActiveMembers()) {
        if (!std::in_range<GuildID_t>(row.guildID))
            throw Error("invalid shared guild member GuildID");
        const auto guildID = static_cast<GuildID_t>(row.guildID);
        const auto guild = replacement.find(guildID);
        if (guild == replacement.end())
            continue;
        if (row.rank < GuildMember::GUILDMEMBER_RANK_NORMAL || row.rank > GuildMember::GUILDMEMBER_RANK_WAIT)
            throw Error("invalid shared guild member rank");

        auto member = std::make_unique<GuildMember>();
        member->setGuildID(guildID);
        member->setName(row.name);
        member->setRank(static_cast<GuildMemberRank_t>(row.rank));
        if (row.rank == GuildMember::GUILDMEMBER_RANK_WAIT)
            member->setRequestDateTime(row.requestDateTime);
        member->setLogOn(row.logOn);
        guild->second->addMember(member.get());
        member.release(); // addMember owns the completed row after insertion.
    }

    // Prepare the existing runtime index while every row still has an owner.
    HashMapGuild published;
    for (const auto& [id, guild] : replacement)
        published.emplace(id, guild.get());

    // Nothing after the swap can throw. The manager takes ownership of the
    // new graph and releases every guild/member in the previous one.
    m_Guilds.swap(published);
    for (auto& [id, guild] : replacement)
        guild.release();
    for (auto& [id, guild] : published)
        delete guild;
}

void GuildManager::addGuild(Guild* pGuild) noexcept(false) {
    __BEGIN_TRY

    Assert(pGuild != NULL);

    __ENTER_CRITICAL_SECTION(m_Mutex)

    unordered_map<GuildID_t, Guild*>::iterator itr = m_Guilds.find(pGuild->getID());
    if (itr != m_Guilds.end())
        throw DuplicatedException();
    m_Guilds[pGuild->getID()] = pGuild;

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    __END_CATCH
}


void GuildManager::addGuild_NOBLOCKED(Guild* pGuild) noexcept(false) {
    __BEGIN_TRY

    Assert(pGuild != NULL);

    unordered_map<GuildID_t, Guild*>::iterator itr = m_Guilds.find(pGuild->getID());
    if (itr != m_Guilds.end())
        throw DuplicatedException();
    m_Guilds[pGuild->getID()] = pGuild;

    __END_CATCH
}


void GuildManager::deleteGuild(GuildID_t id) noexcept(false) {
    __BEGIN_TRY

    __ENTER_CRITICAL_SECTION(m_Mutex)

    unordered_map<GuildID_t, Guild*>::iterator itr = m_Guilds.find(id);
    if (itr == m_Guilds.end())
        throw NoSuchElementException();

    m_Guilds.erase(itr);

#ifdef __SHARED_SERVER__
    defaultSharedGuildRepository().purgeGuild(id);
#endif

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    __END_CATCH
}


Guild* GuildManager::getGuild(GuildID_t id) noexcept(false) {
    __BEGIN_TRY

    // The guild that was found
    Guild* pGuild;

    __ENTER_CRITICAL_SECTION(m_Mutex)

    unordered_map<GuildID_t, Guild*>::iterator itr = m_Guilds.find(id);

    if (itr == m_Guilds.end()) {
        return NULL;
    }

    pGuild = itr->second;

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    return pGuild;

    __END_CATCH
}


Guild* GuildManager::getGuild_NOBLOCKED(GuildID_t id) noexcept(false) {
    __BEGIN_TRY

    // The guild that was found
    Guild* pGuild;

    unordered_map<GuildID_t, Guild*>::iterator itr = m_Guilds.find(id);

    if (itr == m_Guilds.end()) {
        return NULL;
    }

    pGuild = itr->second;

    return pGuild;

    __END_CATCH
}


void GuildManager::clear() noexcept(false) {
    __BEGIN_TRY

    __ENTER_CRITICAL_SECTION(m_Mutex)

    HashMapGuildItor itr = m_Guilds.begin();
    for (; itr != m_Guilds.end(); itr++) {
        SAFE_DELETE(itr->second);
    }

    m_Guilds.clear();

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    __END_CATCH
}

void GuildManager::clear_NOBLOCKED() {
    __BEGIN_TRY

    HashMapGuildItor itr = m_Guilds.begin();
    for (; itr != m_Guilds.end(); itr++) {
        SAFE_DELETE(itr->second);
    }

    m_Guilds.clear();

    __END_CATCH
}

#ifdef __SHARED_SERVER__
void GuildManager::makeSGGuildInfo(SGGuildInfo& sgGuildInfo) noexcept(false) {
    CriticalSection lock{m_Mutex};
    if (m_Guilds.size() + sgGuildInfo.getGuildInfoListNum() > GuildInfo2::kMaxCount)
        throw InvalidProtocolException("too many guild infos");

    SGGuildInfo prepared;
    for (const auto& [id, guild] : m_Guilds) {
        auto info = std::make_unique<GuildInfo2>();
        guild->makeInfo(info.get());
        prepared.addGuildInfo(info.release()); // Consumed even if list insertion fails.
    }
    sgGuildInfo.prependGuildInfosFrom(prepared);
}
#endif

void GuildManager::makeWaitGuildList(GCWaitGuildList& gcWaitGuildList, GuildRace_t race) noexcept(false) {
    __BEGIN_TRY

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

    __END_CATCH
}

void GuildManager::makeActiveGuildList(GCActiveGuildList& gcActiveGuildList, GuildRace_t race) noexcept(false) {
    __BEGIN_TRY

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

    __END_CATCH
}

void GuildManager::heartbeat() noexcept(false) {
    __BEGIN_TRY

#ifdef __SHARED_SERVER__
    Timeval currentTime;
    getCurrentTime(currentTime);

    ////////////////////////////////////////////////////////
    // Drop the members whose join request has passed the waiting time.
    ////////////////////////////////////////////////////////
    if (currentTime > m_WaitMemberClearTime) {
        __ENTER_CRITICAL_SECTION(m_Mutex)

        VSDateTime currentDateTime = VSDateTime::currentDateTime();

        HashMapGuildConstItor itr = m_Guilds.begin();
        for (; itr != m_Guilds.end(); itr++) {
            Guild* pGuild = itr->second;

            list<string> mList;

            pGuild->expireTimeOutWaitMember(currentDateTime, mList);

            list<string>::const_iterator itr2 = mList.begin();

            for (; itr2 != mList.end(); itr2++) {
                // Tell the game servers that the request was cancelled.
                SGExpelGuildMemberOK sgExpelGuildMemberOK;
                sgExpelGuildMemberOK.setGuildID(pGuild->getID());
                sgExpelGuildMemberOK.setName(*itr2);
                sgExpelGuildMemberOK.setSender(pGuild->getMaster());

                de::sharedContext().gameServers().broadcast(&sgExpelGuildMemberOK);
            }
        }

        m_WaitMemberClearTime.tv_sec = currentTime.tv_sec + 3600; // one hour period

        __LEAVE_CRITICAL_SECTION(m_Mutex)
    }
#endif

    __END_CATCH
}

string GuildManager::toString() const noexcept {
    StringStream msg;
    return msg.toString();
}

bool GuildManager::isGuildMaster(GuildID_t guildID, PlayerCreature* pPC) noexcept(false) {
    return false;
}

// Does the guild hold a castle?
bool GuildManager::hasCastle(GuildID_t guildID) noexcept(false) {
    return false;
}

// Does the guild hold a castle?
bool GuildManager::hasCastle(GuildID_t guildID, ServerID_t& serverID, ZoneID_t& zoneID) noexcept(false) {
    return false;
}

// Has the guild filed a war schedule?
bool GuildManager::hasWarSchedule(GuildID_t guildID) noexcept(false) {
    return false;
}

bool GuildManager::hasActiveWar(GuildID_t guildID) noexcept(false) {
    return false;
}


string GuildManager::getGuildName(GuildID_t guildID) noexcept(false) {
    __BEGIN_TRY

    Guild* pGuild = getGuild(guildID);

    if (pGuild != NULL)
        return pGuild->getName();

    return "";

    __END_CATCH
}
