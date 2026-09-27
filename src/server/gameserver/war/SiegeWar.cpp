///////////////////////////////////////////////////////////////////
// General war information and the routines run when a war starts and ends
///////////////////////////////////////////////////////////////////

#include "SiegeWar.h"

#include <stdio.h>

#include "Assert.h"
#include "CGSay.h"
#include "CastleInfoManager.h"
#include "GCNPCResponse.h"
#include "GCNoticeEvent.h"
#include "GCSystemMessage.h"
#include "GCWarScheduleList.h"
#include "GGCommand.h"
#include "GameContext.h"
#include "GameServerInfoManager.h"
#include "Guild.h"
#include "GuildManager.h"
#include "GuildWarInfo.h"
#include "HolyLandRaceBonus.h"
#include "KernelContext.h"
#include "LoginServerManager.h"
#include "Mutex.h"
#include "PCManager.h"
#include "Properties.h"
#include "ServerContext.h"
#include "SiegeManager.h"
#include "StringStream.h"
#include "WarSystem.h"
#include "WarZoneWork.h"
#include "Zone.h"
#include "ZoneGroup.h"
#include "ZoneGroupManager.h"
#include "ZoneInfoManager.h"
#include "ZoneUtil.h"
#include "repository/WarInfoRepository.h"

//--------------------------------------------------------------------------------
//
// constructor / destructor
//
//--------------------------------------------------------------------------------
SiegeWar::SiegeWar(ZoneID_t castleZoneID, WarState warState, WarID_t warID)
    : War(warState, warID), m_CastleZoneID(castleZoneID), m_RegistrationFee(0) {
    m_bModifyCastleOwner = false;
    for (int i = 0; i < 5; ++i)
        m_ChallangerGuildID[i] = 0;

    m_ChallangerGuildCount = 0;
    m_ReinforceGuildID = 0;
    m_RecentReinforceCandidate = 0;
}

SiegeWar::~SiegeWar() {}

int SiegeWar::getGuildSide(GuildID_t guildID) const {
    if (guildID == m_ReinforceGuildID)
        return 2;

    for (int i = 0; i < 5; ++i) {
        if (m_ChallangerGuildID[i] == guildID)
            return 3 + i;
    }

    CastleInfo* pCastleInfo = de::gameContext().castleInfos().getCastleInfo(m_CastleZoneID);
    Assert(pCastleInfo != NULL);

    GuildID_t OwnerGuildID = pCastleInfo->getGuildID();
    if (guildID == OwnerGuildID)
        return 1;

    return 0;
}

bool SiegeWar::addChallengerGuild(GuildID_t gID) {
    // Five slots, so the fifth challenger fills the last one and a sixth is
    // refused rather than written past the end.
    if (m_ChallangerGuildCount >= MaxSiegeChallengerGuilds)
        return false;
    if (gID == 0 || gID == 66 || gID == 99)
        return false;

    m_ChallangerGuildID[m_ChallangerGuildCount++] = gID;
    return true;
}

//--------------------------------------------------------------------------------
//
// executeStart
//
//--------------------------------------------------------------------------------
// What has to be handled when a war starts
//
// (!) This runs in the WarScheduler attached to the castle zone, inside that
//     zone's heartbeat. The siege is fought in a zone of its own, which may
//     belong to another group, so setting it up is posted to its owner.
//--------------------------------------------------------------------------------
void SiegeWar::executeStart()

{
    __BEGIN_TRY

    sendWarStartMessage();
    clearReinforceRegisters();

    // This part would be better moved into CastleInfo later.
    CastleInfo* pCastleInfo = de::gameContext().castleInfos().getCastleInfo(m_CastleZoneID);
    Assert(pCastleInfo != NULL);

    ZoneID_t siegeZoneID = SiegeManager::Instance().getSiegeZoneID(m_CastleZoneID);
    Assert(siegeZoneID != 0);

    de::war::postToZone(siegeZoneID, [](Zone& zone) { SiegeManager::Instance().start(zone.getZoneID()); });

    __END_CATCH
}
//--------------------------------------------------------------------------------
//
// executeEnd
//
//--------------------------------------------------------------------------------
// What has to be handled when a war ends
//--------------------------------------------------------------------------------
void SiegeWar::executeEnd()

{
    __BEGIN_TRY

    CastleInfoManager& castleInfos = de::gameContext().castleInfos();

    //----------------------------------------------------------------------------
    // Report that the war has ended.
    //----------------------------------------------------------------------------
    sendWarEndMessage();

    // The owner change and the fee credited after it run on the castle zone's
    // thread, as GuildWar::executeEnd explains; the castle-following servers
    // are told here, and post the change to their own castle group.
    if (m_bModifyCastleOwner) {
        char sCommand[100];
        snprintf(sCommand, sizeof(sCommand), "*command setCastleOwnerGuild %u %u", m_CastleZoneID, m_WinnerGuildID);
        GGCommand ggCommand;
        ggCommand.setCommand(sCommand);

        static int myWorldID = de::kernelContext().config().getPropertyInt("WorldID");
        static int myServerID = de::kernelContext().config().getPropertyInt("ServerID");

        HashMapGameServerInfo* pInfos = de::serverContext().serverInfos().getGameServerInfos()[myWorldID];
        int maxServerGroupID = de::serverContext().serverInfos().getMaxServerGroupID();

        for (int groupID = 0; groupID < maxServerGroupID; ++groupID) {
            HashMapGameServerInfo& gameServerInfo = pInfos[groupID];

            if (!gameServerInfo.empty()) {
                HashMapGameServerInfo::const_iterator itr = gameServerInfo.begin();
                for (; itr != gameServerInfo.end(); itr++) {
                    GameServerInfo* pGameServerInfo = itr->second;

                    if (pGameServerInfo->getWorldID() == myWorldID) {
                        // Only when it is not the current server.. (handled above)
                        if (pGameServerInfo->getGroupID() == myServerID) {
                        } else if (pGameServerInfo->getCastleFollowingServerID() == myServerID) {
                            de::gameContext().loginServer().sendPacket(pGameServerInfo->getIP(),
                                                                       pGameServerInfo->getUDPPort(), &ggCommand);
                            cout << "send change castle packet to " << pGameServerInfo->getIP() << ", "
                                 << pGameServerInfo->getUDPPort() << endl;
                        }
                    }
                }
            }
        }
    } else {
        // Set WinnerGuildID to the current owner
        CastleInfo* pCastleInfo = castleInfos.getCastleInfo(m_CastleZoneID);
        m_WinnerGuildID = pCastleInfo->getGuildID();
    }

    castleInfos.postCastleWarEnd(m_CastleZoneID, m_bModifyCastleOwner, m_WinnerRace, m_WinnerGuildID,
                                 m_RegistrationFee);
    m_RegistrationFee = 0;

    ZoneID_t siegeZoneID = SiegeManager::Instance().getSiegeZoneID(m_CastleZoneID);
    Assert(siegeZoneID != 0);

    // Emptying the siege zone kills its monsters and hands every player in it
    // a transport, both on the siege zone's own thread; the transports then
    // leave through the ordinary zone transfer.
    filelog("SiegeWar.log", "[%u] executeEnd : reset zone %u", getWarID(), siegeZoneID);
    de::war::postToZone(siegeZoneID, [](Zone& zone) { SiegeManager::Instance().reset(zone.getZoneID()); });

    __END_CATCH
}

string SiegeWar::getWarName() const

{
    __BEGIN_TRY

    return "Siege War";

    __END_CATCH
}

//--------------------------------------------------------------------------------
//
//	isModifyCastleOwner( PlayerCreature* pPC )
//
//--------------------------------------------------------------------------------
// The case where the castle owner changes
//--------------------------------------------------------------------------------
bool SiegeWar::isModifyCastleOwner(PlayerCreature* pPC)

{
    __BEGIN_TRY

    Assert(pPC != NULL);

    CastleInfo* pCastleInfo = de::gameContext().castleInfos().getCastleInfo(m_CastleZoneID);
    Assert(pCastleInfo != NULL);

    if (pPC->isFlag(Effect::EFFECT_CLASS_SIEGE_ATTACKER_1))
        return true;
    if (pPC->isFlag(Effect::EFFECT_CLASS_SIEGE_ATTACKER_2))
        return true;
    if (pPC->isFlag(Effect::EFFECT_CLASS_SIEGE_ATTACKER_3))
        return true;
    if (pPC->isFlag(Effect::EFFECT_CLASS_SIEGE_ATTACKER_4))
        return true;
    if (pPC->isFlag(Effect::EFFECT_CLASS_SIEGE_ATTACKER_5))
        return true;

    return false;

    __END_CATCH
}

//--------------------------------------------------------------------------------
//
// getWinnerGuildID( PlayerCreature* pPC )
//
//--------------------------------------------------------------------------------
// Hands back the GuildID of the guild that won the war.
//--------------------------------------------------------------------------------
GuildID_t SiegeWar::getWinnerGuildID(PlayerCreature* pPC)

{
    __BEGIN_TRY

    Assert(pPC != NULL);

    // in a guild war : pPC's GuildID when the applying guild is pPC's guild
    // 					 otherwise the original castle owner's GuildID when it matches that
    //					 otherwise COMMON_GUILD_ID

    return pPC->getGuildID();

    __END_CATCH
}

bool SiegeWar::endWar(PlayerCreature* pPC)

{
    __BEGIN_TRY

    Assert(pPC != NULL);

    // < castle owner change >
    if (isModifyCastleOwner(pPC)) {
        m_WinnerRace = pPC->getRace();
        m_WinnerGuildID = getWinnerGuildID(pPC);
        m_bModifyCastleOwner = true;

        return true;
    }

    return false;

    __END_CATCH
}

//--------------------------------------------------------------------------------
// When the war ends
//--------------------------------------------------------------------------------
void SiegeWar::sendWarEndMessage() const

{
    __BEGIN_TRY

    War::sendWarEndMessage();

    // The packet that confirms the safe zone release?
    GCNoticeEvent gcNoticeEvent;
    gcNoticeEvent.setCode(NOTICE_EVENT_WAR_OVER);
    gcNoticeEvent.setParameter(m_CastleZoneID);
    de::gameContext().zoneGroups().broadcast(&gcNoticeEvent);

    __END_CATCH
}

void SiegeWar::makeWarScheduleInfo(WarScheduleInfo* pWSI) const

{
    __BEGIN_TRY

    pWSI->warType = getWarType();

    for (int i = 0; i < 5; ++i) {
        pWSI->challengerGuildID[i] = m_ChallangerGuildID[i];
        if (m_ChallangerGuildID[i] != 0)
            pWSI->challengerGuildName[i] = de::gameContext().guilds().getGuildName(m_ChallangerGuildID[i]);
    }

    pWSI->reinforceGuildID = m_ReinforceGuildID;
    if (m_ReinforceGuildID != 0)
        pWSI->reinforceGuildName = de::gameContext().guilds().getGuildName(m_ReinforceGuildID);

    __END_CATCH
}

void SiegeWar::makeWarInfo(WarInfo* pWarInfo) const

{
    __BEGIN_TRY

    Assert(pWarInfo != NULL);
    Assert(pWarInfo->getWarType() == WAR_GUILD);

    GuildWarInfo* pGuildWarInfo = dynamic_cast<GuildWarInfo*>(pWarInfo);
    Assert(pGuildWarInfo != NULL);

    //---------------------------------------------------
    // Get the current castle owner
    //---------------------------------------------------
    CastleInfo* pCastleInfo = de::gameContext().castleInfos().getCastleInfo(getCastleZoneID());
    if (pCastleInfo == NULL) {
        filelog("WarError.log", "No CastleInfo(%d)", getCastleZoneID());
        return;
    }

    GuildID_t ownGuildID = pCastleInfo->getGuildID();

    pGuildWarInfo->addJoinGuild(ownGuildID); // the current castle owner

    for (uint i = 0; i < m_ChallangerGuildCount; ++i)
        pGuildWarInfo->addJoinGuild(m_ChallangerGuildID[i]);

    pGuildWarInfo->setCastleID(getCastleZoneID());

    // The attacking guild's name
    static const string commonGuild("None");

    string attackGuildName;
    string defenseGuildName;

    attackGuildName = de::gameContext().guilds().getGuildName(m_ChallangerGuildID[0]);
    if (m_ChallangerGuildCount > 1) {
        char buffer[40];
        snprintf(buffer, 40, "%s and %u more", attackGuildName.c_str(), m_ChallangerGuildCount - 1);
        attackGuildName = buffer;
    }

    if (pCastleInfo->isCommon())
        defenseGuildName = commonGuild;
    else
        defenseGuildName = de::gameContext().guilds().getGuildName(ownGuildID);

    pGuildWarInfo->setAttackGuildName(attackGuildName);
    pGuildWarInfo->setDefenseGuildName(defenseGuildName);

    __END_CATCH
}


string SiegeWar::toString() const

{
    __BEGIN_TRY

    StringStream msg;

    msg << "SiegeWar(" << "WarID:" << (int)getWarID() << ",State:" << (int)getState()
        << ",CastleZoneID:" << (int)m_CastleZoneID << ",WarType:" << getWarType2DBString()
        << ",ChallengerGuildID:" << (int)m_ChallangerGuildID[0] << ",RegistrationFee:" << (int)m_RegistrationFee << ")";

    return msg.toString();

    __END_CATCH
}

BYTE SiegeWar::canReinforce(GuildID_t gID) {
    if (m_ReinforceGuildID != 0)
        return NPC_RESPONSE_ALREADY_REINFORCE_ACCEPTED;
    if (de::gameContext().guilds().hasWarSchedule(gID))
        return NPC_RESPONSE_WAR_ALREADY_REGISTERED;

    WarInfoRepository& repository = defaultWarInfoRepository();
    const int serverID = de::kernelContext().config().getPropertyInt("ServerID");

    if (repository.countWaitingReinforceRegistrations(getWarID(), serverID) > 3)
        return NPC_RESPONSE_TOO_MANY_GUILD_REGISTERED;

    if (repository.countDeniedReinforceRegistrations(getWarID(), serverID, gID) > 0)
        return NPC_RESPONSE_REINFORCE_DENYED;

    return NPC_RESPONSE_WAR_REGISTRATION_OK;
}

GuildID_t SiegeWar::recentReinforceGuild() {
    GuildID_t ret = 0;

    defaultWarInfoRepository().loadWaitingReinforceGuild(getWarID(),
                                                         de::kernelContext().config().getPropertyInt("ServerID"), ret);

    m_RecentReinforceCandidate = ret;

    return ret;
}

BYTE SiegeWar::registerReinforce(GuildID_t gID) {
    BYTE ret = canReinforce(gID);
    if (ret != NPC_RESPONSE_WAR_REGISTRATION_OK)
        return ret;

    defaultWarInfoRepository().insertReinforceRegistration(
        getWarID(), de::kernelContext().config().getPropertyInt("ServerID"), gID);

    return ret;
}

bool SiegeWar::acceptReinforce() {
    bool ret = defaultWarInfoRepository().acceptReinforceRegistration(
        getWarID(), de::kernelContext().config().getPropertyInt("ServerID"), m_RecentReinforceCandidate);

    if (ret)
        m_ReinforceGuildID = m_RecentReinforceCandidate;

    return ret;
}

bool SiegeWar::denyReinforce() {
    bool ret = defaultWarInfoRepository().denyReinforceRegistration(
        getWarID(), de::kernelContext().config().getPropertyInt("ServerID"), m_RecentReinforceCandidate);

    return ret;
}

void SiegeWar::clearReinforceRegisters() {
    defaultWarInfoRepository().deleteReinforceRegistrations(getWarID(),
                                                            de::kernelContext().config().getPropertyInt("ServerID"));
}
