///////////////////////////////////////////////////////////////////
// General war information and the routines run when a war starts and ends
///////////////////////////////////////////////////////////////////

#include "GuildWar.h"

#include <stdio.h>

#include "Assert.h"
#include "CastleInfoManager.h"
#include "CastleShrineInfoManager.h"
#include "DB.h"
#include "Deployment.h"
#include "GCNoticeEvent.h"
#include "GCSystemMessage.h"
#include "GCWarScheduleList.h"
#include "GameContext.h"
#include "Guild.h"
#include "GuildManager.h"
#include "GuildWarInfo.h"
#include "HolyLandRaceBonus.h"
#include "KernelContext.h"
#include "Mutex.h"
#include "PCManager.h"
#include "Properties.h"
#include "StringStream.h"
#include "WarSystem.h"
#include "WarZoneWork.h"
#include "Zone.h"
#include "ZoneGroup.h"
#include "ZoneGroupManager.h"
#include "ZoneInfoManager.h"
#include "ZoneUtil.h"
#include "gm/GMCommands.h"
#include "repository/WarInfoRepository.h"

//--------------------------------------------------------------------------------
//
// constructor / destructor
//
//--------------------------------------------------------------------------------
GuildWar::GuildWar(ZoneID_t castleZoneID, GuildID_t challenger, WarState warState, WarID_t warID)
    : War(warState, warID), m_CastleZoneID(castleZoneID), m_ChallangerGuildID(challenger), m_RegistrationFee(0) {
    m_bModifyCastleOwner = false;
}

GuildWar::~GuildWar() {}

//--------------------------------------------------------------------------------
//
// executeStart
//
//--------------------------------------------------------------------------------
// What has to be handled when a war starts
//
// (!) This runs in the WarScheduler attached to the castle zone, inside that
//     zone's heartbeat. The castle's dungeon zones and its guard shrine's zone
//     may belong to other groups, so the work on them is posted to their
//     owners and runs at the top of each owner's next tick.
//--------------------------------------------------------------------------------
void GuildWar::executeStart()

{
    __BEGIN_TRY

    sendWarStartMessage();

    // This part would be better moved into CastleInfo later.
    CastleInfo* pCastleInfo = de::gameContext().castleInfos().getCastleInfo(m_CastleZoneID);
    Assert(pCastleInfo != NULL);

    GuildID_t OwnerGuildID = pCastleInfo->getGuildID();

    // Monsters are removed only when the castle is not a common one, and only
    // from the castle's dungeon maps, not from the castle itself.
    if (OwnerGuildID != SlayerCommon && OwnerGuildID != VampireCommon) {
        const list<ZoneID_t>& zoneIDs = pCastleInfo->getZoneIDList();

        vector<ZoneID_t> dungeonZoneIDs;
        for (ZoneID_t zoneID : zoneIDs) {
            if (zoneID != m_CastleZoneID)
                dungeonZoneIDs.push_back(zoneID);
        }

        de::war::postToZones(dungeonZoneIDs, [](Zone& zone) { zone.killAllMonsters(); });
    }

    // Lift the guard shrine's shield.
    ZoneID_t guardShrineZoneID = de::gameContext().castleShrines().getGuardShrineZoneID(m_CastleZoneID);
    de::war::postToZone(guardShrineZoneID,
                        [](Zone& zone) { de::gameContext().castleShrines().removeShrineShield(&zone); });

    // Record in the GuildWarHistory Table
    recordGuildWarStart();

    __END_CATCH
}

void GuildWar::recordGuildWarStart()

{
    __BEGIN_TRY

    CastleInfo* pCastleInfo = de::gameContext().castleInfos().getCastleInfo(m_CastleZoneID);

    // It cannot be NULL, but just in case
    if (pCastleInfo == NULL)
        return;

    defaultWarInfoRepository().insertGuildWarHistory(
        (int)getWarID(), getWarStartTime().toStringforWeb(), de::kernelContext().config().getPropertyInt("ServerID"),
        pCastleInfo->getName(), (int)pCastleInfo->getGuildID(),
        de::gameContext().guilds().getGuildName(pCastleInfo->getGuildID()), getChallangerGuildID(),
        de::gameContext().guilds().getGuildName(getChallangerGuildID()));

    __END_CATCH
}

//--------------------------------------------------------------------------------
//
// executeEnd
//
//--------------------------------------------------------------------------------
// What has to be handled when a war ends
//--------------------------------------------------------------------------------
void GuildWar::executeEnd()

{
    __BEGIN_TRY

    CastleInfoManager& castleInfos = de::gameContext().castleInfos();

    //----------------------------------------------------------------------------
    // Report that the war has ended.
    //----------------------------------------------------------------------------
    sendWarEndMessage();

    //----------------------------------------------------------------------------
    // Change the castle owner
    //----------------------------------------------------------------------------
    // This runs on the main thread, and the owner change belongs to the castle
    // zone's thread (a change of the owning race reloads the castle's war
    // scheduler), so the change, and the registration fee credited after it,
    // are posted there and run at the top of its next tick. Until then the
    // castle keeps its old owner with the war already over. The history below
    // records the winner decided here and does not wait for the change.
    if (m_bModifyCastleOwner) {
        if (de::isNetMarbleDeployment()) {
            char sCommand[100];
            snprintf(sCommand, sizeof(sCommand), "*world *command setCastleOwnerGuild %u %u", m_CastleZoneID,
                     m_WinnerGuildID);
            de::gm::opworld(NULL, sCommand, 0, true);
        }
    } else {
        // Set WinnerGuildID to the current owner
        CastleInfo* pCastleInfo = castleInfos.getCastleInfo(m_CastleZoneID);
        m_WinnerGuildID = pCastleInfo->getGuildID();
    }

    castleInfos.postCastleWarEnd(m_CastleZoneID, m_bModifyCastleOwner, m_WinnerRace, m_WinnerGuildID,
                                 m_RegistrationFee);
    m_RegistrationFee = 0;

    //----------------------------------------------------------------------------
    // Give the castle symbol back.
    //----------------------------------------------------------------------------
    // A symbol lies on the ground or in a corpse in some zone, or is carried by
    // a player, on any group's thread: each return is posted to whoever holds
    // the symbol, which hands it on to the guard shrine's zone.
    de::gameContext().castleShrines().returnAllCastleSymbol(m_CastleZoneID);

    //----------------------------------------------------------------------------
    // Restore the guard shrine's shield, on the guard zone's own thread.
    //----------------------------------------------------------------------------
    ZoneID_t guardShrineZoneID = de::gameContext().castleShrines().getGuardShrineZoneID(m_CastleZoneID);
    de::war::postToZone(guardShrineZoneID,
                        [](Zone& zone) { de::gameContext().castleShrines().addShrineShield(&zone); });

    // Record in the GuildWarHistory Table
    recordGuildWarEnd();

    __END_CATCH
}

void GuildWar::recordGuildWarEnd()

{
    __BEGIN_TRY

    defaultWarInfoRepository().updateGuildWarWinner(
        (int)m_WinnerGuildID, de::gameContext().guilds().getGuildName(m_WinnerGuildID), (int)getWarID());

    // running a script -- who would have thought the system function would be used
    char cmd[100];
    snprintf(cmd, sizeof(cmd), "/home/darkeden/vs/bin/script/recordGuildWarHistory.py %d %d %d ", (int)getWarID(),
             de::kernelContext().config().getPropertyInt("Dimension"),
             de::kernelContext().config().getPropertyInt("WorldID"));

    filelog("script.log", cmd);
    system(cmd);

    __END_CATCH
}

string GuildWar::getWarName() const

{
    __BEGIN_TRY

    ZoneInfo* pZoneInfo = NULL;
    Guild* pGuild = NULL;

    try {
        pGuild = de::gameContext().guilds().getGuild(m_ChallangerGuildID);
        pZoneInfo = de::gameContext().zoneInfos().getZoneInfo(m_CastleZoneID);

        if (pGuild == NULL || pZoneInfo == NULL)
            return "Guild War";
    } catch (Throwable& t) {
        return "Guild War";
    }

    Assert(pZoneInfo != NULL);
    Assert(pGuild != NULL);

    StringStream msg;

    msg << pGuild->getName() << " guild attacks ";
    msg << pZoneInfo->getFullName() << " (guild war)";

    return msg.toString();

    __END_CATCH
}

//--------------------------------------------------------------------------------
//
//	isModifyCastleOwner( PlayerCreature* pPC )
//
//--------------------------------------------------------------------------------
// The case where the castle owner changes
//--------------------------------------------------------------------------------
bool GuildWar::isModifyCastleOwner(PlayerCreature* pPC)

{
    __BEGIN_TRY

    Assert(pPC != NULL);

    CastleInfo* pCastleInfo = de::gameContext().castleInfos().getCastleInfo(m_CastleZoneID);
    Assert(pCastleInfo != NULL);

    // common castle : attacking guild --> the attacking guild's castle
    // common castle : ordinary --> back to the original place
    // guild castle : attacking guild --> the attacking guild's castle
    // guild castle : defending guild --> back to the original place
    // guild castle : ordinary --> common castle

    // in a guild war : the guild that applied for the war, or
    // 					 an ordinary player when it is a guild castle
    if (pPC->getGuildID() == m_ChallangerGuildID ||
        (!pCastleInfo->isCommon() && pPC->getCommonGuildID() == pPC->getGuildID())) {
        return true;
    }

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
GuildID_t GuildWar::getWinnerGuildID(PlayerCreature* pPC)

{
    __BEGIN_TRY

    Assert(pPC != NULL);

    // in a guild war : pPC's GuildID when the applying guild is pPC's guild
    // 					 otherwise the original castle owner's GuildID when it matches that
    //					 otherwise COMMON_GUILD_ID
    CastleInfo* pCastleInfo = de::gameContext().castleInfos().getCastleInfo(m_CastleZoneID);
    Assert(pCastleInfo != NULL);

    if (m_ChallangerGuildID == pPC->getGuildID() || pPC->getGuildID() == pCastleInfo->getGuildID()) {
        return pPC->getGuildID();
    }

    return pPC->getCommonGuildID();

    __END_CATCH
}

bool GuildWar::endWar(PlayerCreature* pPC)

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
void GuildWar::sendWarEndMessage() const

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

void GuildWar::makeWarScheduleInfo(WarScheduleInfo* pWSI) const

{
    __BEGIN_TRY

    pWSI->warType = getWarType();
    pWSI->challengerGuildID[0] = getChallangerGuildID();
    for (int i = 1; i < 5; ++i)
        pWSI->challengerGuildID[i] = 0;
    pWSI->reinforceGuildID = 0;

    pWSI->challengerGuildName[0] = de::gameContext().guilds().getGuildName(getChallangerGuildID());

    __END_CATCH
}

void GuildWar::makeWarInfo(WarInfo* pWarInfo) const

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
    GuildID_t challangerGuildID = getChallangerGuildID();

    pGuildWarInfo->addJoinGuild(ownGuildID);        // the current castle owner
    pGuildWarInfo->addJoinGuild(challangerGuildID); // the guild attacking the castle
    pGuildWarInfo->setCastleID(getCastleZoneID());

    // The attacking guild's name
    static const string commonSlayerGuild("None");
    static const string commonVampireGuild("None");

    string attackGuildName;
    string defenseGuildName;

    if (challangerGuildID == SlayerCommon)
        attackGuildName = commonSlayerGuild;
    else if (challangerGuildID == VampireCommon)
        attackGuildName = commonVampireGuild;
    else
        attackGuildName = de::gameContext().guilds().getGuildName(getChallangerGuildID());

    if (ownGuildID == SlayerCommon)
        defenseGuildName = commonSlayerGuild;
    else if (ownGuildID == VampireCommon)
        defenseGuildName = commonVampireGuild;
    else
        defenseGuildName = de::gameContext().guilds().getGuildName(ownGuildID);

    pGuildWarInfo->setAttackGuildName(attackGuildName);
    pGuildWarInfo->setDefenseGuildName(defenseGuildName);

    __END_CATCH
}


string GuildWar::toString() const

{
    __BEGIN_TRY

    StringStream msg;

    msg << "GuildWar(" << "WarID:" << (int)getWarID() << ",State:" << (int)getState()
        << ",CastleZoneID:" << (int)m_CastleZoneID << ",WarType:" << getWarType2DBString()
        << ",ChallengerGuildID:" << (int)m_ChallangerGuildID << ",RegistrationFee:" << (int)m_RegistrationFee << ")";

    return msg.toString();

    __END_CATCH
}
