#include "GuildUnion.h"

#include <stdio.h>

#include <algorithm>

#include "GCModifyInformation.h"
#include "GGCommand.h"
#include "GameContext.h"
#include "GameServer.h"
#include "GameServerInfoManager.h"
#include "Guild.h"
#include "GuildManager.h"
#include "KernelContext.h"
#include "LoginServerManager.h"
#include "PCFinder.h"
#include "PacketUtil.h"
#include "Player.h"
#include "ServerContext.h"
#include "VariableManager.h"
#include "guild/GuildUnionTeardown.h"
#include "repository/GuildRepository.h"

namespace {

// The master character of a guild, or "" when the guild has gone. The union
// tables name guilds by id and a row can outlive the guild it names, so a
// union row pointing at a guild the GuildManager no longer holds is a stale
// row: it is logged and skipped, never dereferenced.
string guildMasterOf(GuildID_t gID) {
    Guild* pGuild = de::gameContext().guilds().getGuild(gID);

    if (pGuild == NULL) {
        filelog("GuildUnion.log", "[%u] union row names a guild that is gone.", gID);
        return "";
    }

    return pGuild->getMaster();
}

// Tell a guild its union standing changed: its master, if online, gets its
// own union info, and everyone the guild has online has the new standing
// broadcast around them. A guild that has gone has nobody to tell.
void notifyUnionChange(GuildID_t gID) {
    const string masterName = guildMasterOf(gID);

    if (!masterName.empty()) {
        PCFinder& pcFinder = de::gameContext().playerCreatures();

        __ENTER_CRITICAL_SECTION(pcFinder)

        Creature* pTargetCreature = pcFinder.getCreature_LOCKED(masterName);
        if (pTargetCreature != NULL) {
            GCModifyInformation gcModifyInformation;
            makeGCModifyInfoGuildUnion(&gcModifyInformation, pTargetCreature);
            pTargetCreature->getPlayer()->sendPacket(&gcModifyInformation);
        }

        __LEAVE_CRITICAL_SECTION(pcFinder)
    }

    sendGCOtherModifyInfoGuildUnionByGuildID(gID);
}

// The answer a master gets for an offer that is not one to act on; OK for
// one that is.
uint refusalOf(UnionOfferAnswerVerdict verdict) {
    switch (verdict) {
    case UnionOfferAnswerVerdict::ANSWER:
        return GuildUnionOfferManager::OK;
    case UnionOfferAnswerVerdict::NO_OFFER:
        return GuildUnionOfferManager::NO_TARGET_UNION;
    case UnionOfferAnswerVerdict::NOT_YOUR_UNION:
        return GuildUnionOfferManager::NOT_YOUR_UNION;
    }

    throw Error("unknown union offer answer verdict");
}

} // namespace

GuildUnionManager::GuildUnionManager() {
    m_Mutex.setName("GuildUnionManager");
}

// The registry frees the unions, live and retired.
GuildUnionManager::~GuildUnionManager() = default;

UnionJoinOfferVerdict GuildUnionManager::recordJoinOffer(GuildID_t applicantGID, GuildID_t masterGID,
                                                         bool tooManyMembers) {
    UnionJoinOfferVerdict verdict = UnionJoinOfferVerdict::ALREADY_IN_UNION;
    UnionChanges changes;

    __ENTER_CRITICAL_SECTION(m_Mutex)

    // Nothing below may read an expired row: the applicant's lapsed offer or
    // ended penalty goes first, and a union only an expired offer held goes
    // with it.
    purgeOffers_LOCKED(changes);

    GuildRepository& repository = defaultGuildRepository();

    GuildUnion* pUnion = m_Unions.unionOfGuild(masterGID);

    UnionJoinOfferFacts facts;
    facts.applicantInUnion = m_Unions.unionOfGuild(applicantGID) != NULL;
    facts.tooManyMembers = tooManyMembers;
    if (pUnion == NULL)
        facts.targetStanding = UnionJoinOfferFacts::TARGET_IN_NO_UNION;
    else if (pUnion->getMasterGuildID() == masterGID)
        facts.targetStanding = UnionJoinOfferFacts::TARGET_LEADS_UNION;
    else
        facts.targetStanding = UnionJoinOfferFacts::TARGET_IS_MEMBER;
    facts.applicantHasOffer = repository.countOffers(applicantGID) > 0;
    facts.applicantHasPenalty = repository.countRecentEscapes(applicantGID) > 0;
    facts.unionMemberCount = pUnion != NULL ? repository.countUnionMembers(pUnion->getUnionID()) : 0;
    facts.unionMemberLimit = de::gameContext().variables().getVariable(GUILD_UNION_MAX);

    verdict = decideUnionJoinOffer(facts);

    uint targetUnionID = pUnion != NULL ? pUnion->getUnionID() : 0;

    if (verdict == UnionJoinOfferVerdict::OPEN_UNION_AND_RECORD)
        targetUnionID = repository.insertUnion(masterGID);

    if (verdict == UnionJoinOfferVerdict::OPEN_UNION_AND_RECORD || verdict == UnionJoinOfferVerdict::RECORD_OFFER) {
        try {
            repository.insertJoinOffer(targetUnionID, applicantGID);
        } catch (...) {
            // A union row with no offer behind it would keep its master in a
            // union nobody can reach, so the row goes with the failed offer.
            if (verdict == UnionJoinOfferVerdict::OPEN_UNION_AND_RECORD)
                repository.deleteUnion(targetUnionID);
            throw;
        }

        // Published only once the offer that justifies it is on record. The
        // other game servers keep their own copy of the union tables; without
        // the refresh, an offer to the same guild made on one of them would
        // open a second union there.
        if (verdict == UnionJoinOfferVerdict::OPEN_UNION_AND_RECORD) {
            m_Unions.publish(std::make_unique<GuildUnion>(targetUnionID, masterGID));
            changes.refresh = true;
        }
    }

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    publish(changes);

    return verdict;
}

void GuildUnionManager::purgeOffers() {
    UnionChanges changes;

    __ENTER_CRITICAL_SECTION(m_Mutex)

    purgeOffers_LOCKED(changes);

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    publish(changes);
}

void GuildUnionManager::purgeOffers_LOCKED(UnionChanges& changes) {
    GuildRepository& repository = defaultGuildRepository();

    // The offers are read before the unions (see decideUnionOfferPurge).
    const vector<UnionOfferStateRow> rows = repository.loadOfferStates();
    if (rows.empty())
        return;

    const vector<UnionRow> unionRows = repository.loadUnions();

    vector<UnionOfferState> offers;
    offers.reserve(rows.size());
    for (size_t r = 0; r < rows.size(); r++) {
        UnionOfferState offer;
        offer.unionID = static_cast<unsigned>(rows[r].unionID);
        offer.offerType = rows[r].offerType;
        offer.ownerGuildID = static_cast<unsigned>(rows[r].ownerGuildID);
        offer.expired = rows[r].expired;
        offers.push_back(offer);
    }

    vector<unsigned> unionIDs;
    unionIDs.reserve(unionRows.size());
    for (size_t u = 0; u < unionRows.size(); u++)
        unionIDs.push_back(static_cast<unsigned>(unionRows[u].unionID));

    const UnionOfferPurge purge = decideUnionOfferPurge(offers, unionIDs);

    // The delete takes the guild's row only while it is still expired, so an
    // offer made since the read survives it.
    for (size_t e = 0; e < purge.expired.size(); e++)
        repository.deleteStaleOffers(static_cast<GuildID_t>(offers[purge.expired[e]].ownerGuildID));

    for (size_t o = 0; o < purge.orphaned.size(); o++) {
        const UnionOfferState& offer = offers[purge.orphaned[o]];

        repository.deleteOfferToUnion(static_cast<GuildID_t>(offer.ownerGuildID), offer.unionID);
        filelog("GuildUnion.log", "[%u:%u] offer names a union that has gone; dropped.", offer.unionID,
                offer.ownerGuildID);
    }

    for (size_t u = 0; u < purge.unionsToCheck.size(); u++) {
        if (dissolveIfAbandoned_LOCKED(purge.unionsToCheck[u], changes))
            filelog("GuildUnion.log", "[%u] dissolved: the join offers that kept it expired.", purge.unionsToCheck[u]);
    }
}

bool GuildUnionManager::dissolveIfAbandoned(uint uID) {
    bool dissolved = false;
    UnionChanges changes;

    __ENTER_CRITICAL_SECTION(m_Mutex)

    dissolved = dissolveIfAbandoned_LOCKED(uID, changes);

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    publish(changes);

    return dissolved;
}

bool GuildUnionManager::dissolveIfAbandoned_LOCKED(uint uID, UnionChanges& changes) {
    GuildRepository& repository = defaultGuildRepository();

    if (!unionIsAbandoned(repository.countUnionMembers(uID), repository.countPendingJoinOffers(uID)))
        return false;

    // The master guild, from the union object or, on a server that does not
    // carry it, from the union's row. A union with neither has gone already.
    GuildID_t masterGuildID = 0;
    GuildUnion* pUnion = m_Unions.unionByID(uID);
    if (pUnion != NULL) {
        masterGuildID = pUnion->getMasterGuildID();
    } else {
        int rowMasterGuildID = 0;
        if (!repository.loadUnionMaster(static_cast<int>(uID), rowMasterGuildID))
            return false;
        masterGuildID = static_cast<GuildID_t>(rowMasterGuildID);
    }

    destroyUnion_LOCKED(uID);

    // The master guild was the only guild in the union.
    changes.guildsToNotify.push_back(masterGuildID);
    changes.refresh = true;

    return true;
}

void GuildUnionManager::publish(const UnionChanges& changes) {
    vector<GuildID_t> told;

    for (size_t g = 0; g < changes.guildsToNotify.size(); g++) {
        const GuildID_t gID = changes.guildsToNotify[g];
        if (std::find(told.begin(), told.end(), gID) != told.end())
            continue;

        told.push_back(gID);
        notifyUnionChange(gID);
    }

    if (changes.refresh)
        sendRefreshCommand();
}

void GuildUnionManager::sendModifyUnionInfo(uint gID) {
    char Msg[80];
    snprintf(Msg, sizeof(Msg), "*modifyunioninfo %d", gID);

    GGCommand ggCommand;
    ggCommand.setCommand(Msg);


    // Send to each server.
    GameServerInfoManager& serverInfos = de::serverContext().serverInfos();
    HashMapGameServerInfo** pGameServerInfos = serverInfos.getGameServerInfos();


    static int myWorldID = de::kernelContext().config().getPropertyInt("WorldID");
    static int myServerID = de::kernelContext().config().getPropertyInt("ServerID");

    int maxWorldID = serverInfos.getMaxWorldID();
    int maxServerGroupID = serverInfos.getMaxServerGroupID();


    for (int worldID = 1; worldID < maxWorldID; worldID++) {
        for (int groupID = 0; groupID < maxServerGroupID; groupID++) {
            HashMapGameServerInfo& gameServerInfo = pGameServerInfos[worldID][groupID];

            if (!gameServerInfo.empty()) {
                HashMapGameServerInfo::const_iterator itr = gameServerInfo.begin();
                for (; itr != gameServerInfo.end(); itr++) {
                    GameServerInfo* pGameServerInfo = itr->second;

                    if (pGameServerInfo->getWorldID() == myWorldID) {
                        // Only for servers other than the current one (handled above).
                        if (pGameServerInfo->getGroupID() == myServerID) {
                        } else {
                            de::gameContext().loginServer().sendPacket(pGameServerInfo->getIP(),
                                                                       pGameServerInfo->getUDPPort(), &ggCommand);
                        }
                    }
                }
            }
        }
    }
}

void GuildUnionManager::sendRefreshCommand() {
    GGCommand ggCommand;
    ggCommand.setCommand("*refreshguildunion");


    // Send to each server.
    GameServerInfoManager& serverInfos = de::serverContext().serverInfos();
    HashMapGameServerInfo** pGameServerInfos = serverInfos.getGameServerInfos();


    static int myWorldID = de::kernelContext().config().getPropertyInt("WorldID");
    static int myServerID = de::kernelContext().config().getPropertyInt("ServerID");

    int maxWorldID = serverInfos.getMaxWorldID();
    int maxServerGroupID = serverInfos.getMaxServerGroupID();


    for (int worldID = 1; worldID < maxWorldID; worldID++) {
        for (int groupID = 0; groupID < maxServerGroupID; groupID++) {
            HashMapGameServerInfo& gameServerInfo = pGameServerInfos[worldID][groupID];

            if (!gameServerInfo.empty()) {
                HashMapGameServerInfo::const_iterator itr = gameServerInfo.begin();
                for (; itr != gameServerInfo.end(); itr++) {
                    GameServerInfo* pGameServerInfo = itr->second;

                    if (pGameServerInfo->getWorldID() == myWorldID) {
                        // Only for servers other than the current one (handled above).
                        if (pGameServerInfo->getGroupID() == myServerID) {
                        } else {
                            de::gameContext().loginServer().sendPacket(pGameServerInfo->getIP(),
                                                                       pGameServerInfo->getUDPPort(), &ggCommand);
                        }
                    }
                }
            }
        }
    }
}


bool GuildUnionManager::addGuild(uint uID, GuildID_t gID) {
    __BEGIN_TRY

    __ENTER_CRITICAL_SECTION(m_Mutex)

    if (!m_Unions.addMember(uID, gID))
        return false;

    defaultGuildRepository().insertUnionMember(uID, gID);

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    sendRefreshCommand();

    return true;

    __END_CATCH
}

bool GuildUnionManager::removeGuildFromUnion(GuildID_t gID) {
    __BEGIN_TRY

    UnionTeardown teardown;
    UnionChanges changes;

    __ENTER_CRITICAL_SECTION(m_Mutex)

    GuildRepository& guildRows = defaultGuildRepository();

    // A guild going away takes its own offer rows with it, and a union its
    // join offer alone kept goes too.
    int offeredUnionID = 0;
    const bool hadJoinOffer = guildRows.loadJoinOfferUnion(gID, offeredUnionID);
    guildRows.deleteOffers(gID);
    if (hadJoinOffer)
        dissolveIfAbandoned_LOCKED(static_cast<uint>(offeredUnionID), changes);

    // The union holding the guild. The lookup maps answer first; a guild they
    // have forgotten may still have a member row naming it, so the tables are
    // asked before the answer is "no union".
    GuildUnion* pUnion = m_Unions.unionOfGuild(gID);
    uint unionID = 0;
    GuildID_t unionMasterGuildID = 0;
    bool unionKnown = false;

    if (pUnion != NULL) {
        unionKnown = true;
        unionID = pUnion->getUnionID();
        unionMasterGuildID = pUnion->getMasterGuildID();
    } else {
        int rowUnionID = 0;
        int rowOwnerGuildID = 0;
        int rowMasterGuildID = 0;

        if (guildRows.loadUnionOfGuild(gID, rowUnionID, rowOwnerGuildID) &&
            guildRows.loadUnionMaster(rowUnionID, rowMasterGuildID)) {
            unionKnown = true;
            unionID = rowUnionID;
            unionMasterGuildID = rowMasterGuildID;

            // The union object, if this server carries one for that id.
            pUnion = m_Unions.unionByID(unionID);
        }
    }

    if (unionKnown) {
        // The union's member guilds, from its rows and from the union object
        // both: a guild one of them has lost is still a guild to let out.
        // decideUnionTeardown names each of them once however often it is
        // listed.
        const vector<int> memberRows = guildRows.loadUnionMemberGuilds(unionID);

        vector<GuildID_t> memberGuilds;
        memberGuilds.reserve(memberRows.size());
        for (size_t m = 0; m < memberRows.size(); m++)
            memberGuilds.push_back(static_cast<GuildID_t>(memberRows[m]));

        if (pUnion != NULL) {
            const list<GuildID_t> knownGuilds = pUnion->getGuildList();
            for (list<GuildID_t>::const_iterator itr = knownGuilds.begin(); itr != knownGuilds.end(); ++itr)
                memberGuilds.push_back(*itr);
        }

        teardown = decideUnionTeardown(unionKnown, unionMasterGuildID, memberGuilds, gID);
    }

    for (size_t m = 0; m < teardown.membersToRemove.size(); m++) {
        const GuildID_t memberID = teardown.membersToRemove[m];

        // The guild stops resolving to this union whether or not the union
        // itself survives, and its member row goes. A guild the union object
        // did not list only has the row to lose.
        const bool listed = m_Unions.removeMember(unionID, memberID);

        if (!guildRows.deleteUnionMember(unionID, memberID) && listed)
            filelog("GuildUnion.log", "[%u:%u] no member row to remove.", unionID, memberID);
    }

    if (teardown.action == UnionTeardown::DISSOLVE) {
        destroyUnion_LOCKED(unionID);
    } else if (teardown.action == UnionTeardown::REMOVE_MEMBER) {
        // The last member out leaves the union to its pending join offers.
        dissolveIfAbandoned_LOCKED(unionID, changes);
    }

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    if (teardown.action != UnionTeardown::NOTHING) {
        changes.guildsToNotify.insert(changes.guildsToNotify.end(), teardown.guildsToNotify.begin(),
                                      teardown.guildsToNotify.end());
        changes.refresh = true;
    }

    publish(changes);

    return teardown.action != UnionTeardown::NOTHING;

    __END_CATCH
}

void GuildUnionManager::destroyUnion_LOCKED(uint uID) {
    // The rows go whether or not this server carries the union in memory. An
    // offer to join the union, or to leave it, has nothing left to answer
    // it; an ESCAPE row naming it is the former member's penalty and stays.
    GuildRepository& repository = defaultGuildRepository();
    repository.deleteUnion(uID);
    repository.deleteOffersToUnion(uID);

    // Every guild that resolved to the union stops resolving to it. A thread
    // still holding the pointer finds it retired rather than freed.
    m_Unions.retire(uID);
}

bool GuildUnionManager::removeGuild(uint uID, GuildID_t gID, bool* pDissolved) {
    __BEGIN_TRY

    bool dissolved = false;
    UnionChanges changes;

    __ENTER_CRITICAL_SECTION(m_Mutex)

    if (!m_Unions.removeMember(uID, gID))
        return false;

    GuildRepository& repository = defaultGuildRepository();

    if (!repository.deleteUnionMember(uID, gID))
        filelog("GuildUnion.log", "[%u:%u] no member row to remove.", uID, gID);

    // A guild out of the union has no union left to ask to let it go.
    repository.deleteQuitOffer(gID);

    // Whether the union outlives its member is the abandoned-union rule's to
    // say: a pending join offer keeps it.
    dissolved = dissolveIfAbandoned_LOCKED(uID, changes);

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    changes.refresh = true;
    publish(changes);

    if (pDissolved != NULL)
        *pDissolved = dissolved;

    return true;

    __END_CATCH
}

void GuildUnionManager::reload() {
    UnionChanges changes;

    __ENTER_CRITICAL_SECTION(m_Mutex)

    load_LOCKED(changes);

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    publish(changes);
}

void GuildUnionManager::load() {
    __BEGIN_TRY

    UnionChanges changes;

    __ENTER_CRITICAL_SECTION(m_Mutex)

    load_LOCKED(changes);

    if (changes.refresh)
        m_RefreshOwed = true;

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    __END_CATCH
}

void GuildUnionManager::sendOwedRefresh() {
    bool owed = false;

    __ENTER_CRITICAL_SECTION(m_Mutex)

    owed = m_RefreshOwed;
    m_RefreshOwed = false;

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    if (owed)
        sendRefreshCommand();
}

void GuildUnionManager::load_LOCKED(UnionChanges& changes) {
    // A restart carries no expired offer, nor a union only an expired offer
    // kept, nor an offer to a union that has gone.
    purgeOffers_LOCKED(changes);

    GuildRepository& repository = defaultGuildRepository();

    const vector<UnionRow> unions = repository.loadUnions();

    vector<std::unique_ptr<GuildUnion>> fresh;
    fresh.reserve(unions.size());

    for (size_t u = 0; u < unions.size(); u++) {
        const uint uID = unions[u].unionID;

        list<GuildID_t> memberGuilds;
        const vector<int> memberRows = repository.loadUnionMemberGuilds(uID);
        for (size_t m = 0; m < memberRows.size(); m++)
            memberGuilds.push_back(static_cast<GuildID_t>(memberRows[m]));

        const GuildID_t masterGuildID = static_cast<GuildID_t>(unions[u].masterGuildID);
        fresh.push_back(std::make_unique<GuildUnion>(uID, masterGuildID, memberGuilds));
    }

    m_Unions.replaceAll(std::move(fresh));
}

uint GuildUnionOfferManager::offerJoin(GuildID_t gID, GuildID_t masterGID) {
    __BEGIN_TRY

    // The guild sizes are read here, outside the union manager's lock, which
    // the guild manager's locks may not be taken under.
    Guild* pReqGuild = de::gameContext().guilds().getGuild(gID);
    Guild* pMasterGuild = de::gameContext().guilds().getGuild(masterGID);

    const bool tooManyMembers = pReqGuild != NULL && pMasterGuild != NULL &&
                                (pReqGuild->getActiveMemberCount() > MAX_GUILDMEMBER_ACTIVE_COUNT ||
                                 pMasterGuild->getActiveMemberCount() > MAX_GUILDMEMBER_ACTIVE_COUNT);

    switch (GuildUnionManager::Instance().recordJoinOffer(gID, masterGID, tooManyMembers)) {
    case UnionJoinOfferVerdict::RECORD_OFFER:
    case UnionJoinOfferVerdict::OPEN_UNION_AND_RECORD:
        return OK;
    case UnionJoinOfferVerdict::ALREADY_IN_UNION:
        return ALREADY_IN_UNION;
    case UnionJoinOfferVerdict::TOO_MANY_MEMBER:
        return TOO_MANY_MEMBER;
    case UnionJoinOfferVerdict::TARGET_IS_NOT_MASTER:
        return TARGET_IS_NOT_MASTER;
    case UnionJoinOfferVerdict::YOU_HAVE_PENALTY:
        return YOU_HAVE_PENALTY;
    case UnionJoinOfferVerdict::ALREADY_OFFER_SOMETHING:
        return ALREADY_OFFER_SOMETHING;
    case UnionJoinOfferVerdict::NOT_ENOUGH_SLOT:
        return NOT_ENOUGH_SLOT;
    }

    throw Error("unknown union join offer verdict");

    __END_CATCH
}

uint GuildUnionOfferManager::offerQuit(GuildID_t gID) {
    __BEGIN_TRY

    GuildUnionManager& unions = GuildUnionManager::Instance();

    // A lapsed offer of the guild's does not stand in the way of this one.
    unions.purgeOffers();

    GuildUnion* pUnion = unions.getGuildUnion(gID);

    if (pUnion == NULL) {
        return NOT_IN_UNION;
    } else if (pUnion->getMasterGuildID() == gID) {
        return MASTER_CANNOT_QUIT;
    }

    if (hasOffer(gID)) {
        return ALREADY_OFFER_SOMETHING;
    }

    defaultGuildRepository().insertQuitOffer(pUnion->getUnionID(), gID);

    return OK;

    __END_CATCH
}

bool GuildUnionOfferManager::makeOfferList(uint uID, GCUnionOfferList& offerList) {
    GuildUnionManager& unions = GuildUnionManager::Instance();

    // The master is shown only offers that can still be answered.
    unions.purgeOffers();
    if (unions.getGuildUnionByUnionID(uID) == NULL)
        return false;

    GuildRepository& repository = defaultGuildRepository();

    vector<UnionOfferRow> offers = repository.loadOffers(uID);

    if (offers.empty()) {
        return false;
    }

    for (size_t o = 0; o < offers.size(); o++) {
        SingleGuildUnionOffer* offer = new SingleGuildUnionOffer;

        offer->setGuildType(offers[o].offerType);
        offer->setGuildID(offers[o].ownerGuildID);

        DWORD dwDate = offers[o].date;
        offer->setDate(dwDate * 100);

        string guildName;
        string guildMaster;
        if (!repository.loadGuildNameAndMaster(offers[o].ownerGuildID, guildName, guildMaster)) {
            delete offer;
            return false;
        }

        offer->setGuildName(guildName);
        offer->setGuildMaster(guildMaster);

        offerList.addUnionOfferList(offer);
    }

    return true;
}

uint GuildUnionOfferManager::acceptJoin(GuildID_t gID, uint answeringUnionID) {
    __BEGIN_TRY

    GuildRepository& repository = defaultGuildRepository();
    GuildUnionManager& unions = GuildUnionManager::Instance();

    unions.purgeOffers();

    int unionID = 0;
    const bool offerFound = repository.loadJoinOfferUnion(gID, unionID);
    const uint refusal =
        refusalOf(decideUnionOfferAnswer(offerFound, static_cast<unsigned>(unionID), answeringUnionID));
    if (refusal != OK)
        return refusal;

    clearOffer(gID);

    const uint uID = unionID;
    uint result = OK;

    if (unions.getGuildUnion(gID) != NULL) {
        result = ALREADY_IN_UNION;
    } else if (unions.getGuildUnionByUnionID(uID) == NULL) {
        result = NO_TARGET_UNION;
    } else if (repository.countUnionMembers(uID) >= de::gameContext().variables().getVariable(GUILD_UNION_MAX)) {
        result = NOT_ENOUGH_SLOT;
    } else if (!unions.addGuild(uID, gID)) {
        // Another change came between the checks above and this one: the
        // guild joined some union, or this union was dissolved.
        result = unions.getGuildUnion(gID) != NULL ? ALREADY_IN_UNION : NO_TARGET_UNION;
    }

    // The offer is gone whether or not the join went through, so a union
    // opened for it that got no member goes too.
    if (result != OK)
        unions.dissolveIfAbandoned(uID);

    return result;

    __END_CATCH
}

uint GuildUnionOfferManager::acceptQuit(GuildID_t gID, uint answeringUnionID) {
    __BEGIN_TRY

    GuildUnionManager& unions = GuildUnionManager::Instance();

    unions.purgeOffers();

    int unionID = 0;
    const bool offerFound = defaultGuildRepository().loadQuitOfferUnion(gID, unionID);
    const uint refusal =
        refusalOf(decideUnionOfferAnswer(offerFound, static_cast<unsigned>(unionID), answeringUnionID));
    if (refusal != OK)
        return refusal;

    clearOffer(gID);

    GuildUnion* pUnion = unions.getGuildUnion(gID);
    if (pUnion == NULL) {
        return NOT_IN_UNION;
    }

    const uint uID = unionID;
    if (uID != pUnion->getUnionID()) {
        return NOT_YOUR_UNION;
    }

    // The guild may have left, or the union been dissolved, since the checks
    // above. The union goes with the guild when nothing else holds it.
    if (!unions.removeGuild(uID, gID))
        return NOT_IN_UNION;

    return OK;

    __END_CATCH
}

uint GuildUnionOfferManager::denyJoin(GuildID_t gID, uint answeringUnionID) {
    __BEGIN_TRY

    GuildUnionManager& unions = GuildUnionManager::Instance();

    unions.purgeOffers();

    int unionID = 0;
    const bool offerFound = defaultGuildRepository().loadJoinOfferUnion(gID, unionID);
    const uint refusal =
        refusalOf(decideUnionOfferAnswer(offerFound, static_cast<unsigned>(unionID), answeringUnionID));
    if (refusal != OK)
        return refusal;

    clearOffer(gID);

    const uint uID = unionID;
    uint result = OK;

    if (unions.getGuildUnion(gID) != NULL)
        result = ALREADY_IN_UNION;
    else if (unions.getGuildUnionByUnionID(uID) == NULL)
        result = NO_TARGET_UNION;

    // A union opened for this offer that has no member and no other pending
    // offer goes with the offer, on every game server.
    unions.dissolveIfAbandoned(uID);

    return result;

    __END_CATCH
}

uint GuildUnionOfferManager::denyQuit(GuildID_t gID, uint answeringUnionID) {
    __BEGIN_TRY

    GuildUnionManager& unions = GuildUnionManager::Instance();

    unions.purgeOffers();

    int unionID = 0;
    const bool offerFound = defaultGuildRepository().loadQuitOfferUnion(gID, unionID);
    const uint refusal =
        refusalOf(decideUnionOfferAnswer(offerFound, static_cast<unsigned>(unionID), answeringUnionID));
    if (refusal != OK)
        return refusal;

    clearOffer(gID);

    GuildUnion* pUnion = unions.getGuildUnion(gID);
    if (pUnion == NULL) {
        return NOT_IN_UNION;
    }

    const uint uID = unionID;
    if (uID != pUnion->getUnionID()) {
        return NOT_YOUR_UNION;
    }

    return OK;

    __END_CATCH
}

void GuildUnionOfferManager::clearOffer(GuildID_t gID) {
    __BEGIN_TRY

    defaultGuildRepository().deleteOffers(gID);

    __END_CATCH
}

bool GuildUnionOfferManager::hasOffer(GuildID_t gID) {
    __BEGIN_TRY

    if (defaultGuildRepository().countOffers(gID) > 0) {
        return true;
    }

    return false;

    __END_CATCH
}
