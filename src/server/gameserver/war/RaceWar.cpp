///////////////////////////////////////////////////////////////////
// General war information and the routines run when a war starts and ends
///////////////////////////////////////////////////////////////////

#include "RaceWar.h"

#include <stdio.h>

#include "Assert.h"
#include "DB.h"
#include "Mutex.h"
#include "Properties.h"
#include "WarSystem.h"
#include "ZoneGroupManager.h"
#include "repository/WarInfoRepository.h"
// #include "HolyLandRaceBonus.h"
#include "CastleInfoManager.h"
#include "DragonEyeManager.h"
#include "GCNoticeEvent.h"
#include "GCSystemMessage.h"
#include "GCWarScheduleList.h"
#include "GameContext.h"
#include "HolyLandManager.h"
#include "KernelContext.h"
#include "PCManager.h"
#include "PlayerCreature.h"
#include "RaceWarInfo.h"
#include "RaceWarLimiter.h"
#include "RegenZoneManager.h"
#include "ShrineInfoManager.h"
#include "StringStream.h"
#include "VariableManager.h"
#include "Zone.h"
#include "ZoneGroup.h"
#include "ZoneInfoManager.h"
#include "ZoneUtil.h"
#include "gm/GMCommands.h"

//--------------------------------------------------------------------------------
//
// constructor / destructor
//
//--------------------------------------------------------------------------------
RaceWar::RaceWar(WarState warState, WarID_t warID) : War(warState, warID) {}

RaceWar::~RaceWar() {}

//--------------------------------------------------------------------------------
//
// executeStart
//
//--------------------------------------------------------------------------------
// What has to be handled when a war starts
//
// (!) This runs under the war system's mutex, on the main thread's war
//     heartbeat or on the thread of a GM starting the race war early. The
//     castles, the guard shrines, the holy land and the regen zone towers all
//     belong to zone groups' threads, so each change to them is posted to its
//     owner and runs at the top of that group's next tick.
//--------------------------------------------------------------------------------
void RaceWar::executeStart()

{
    __BEGIN_TRY

    sendWarStartMessage();

    // Broadcasting the blood bible positions and sending out the players who
    // are not taking part belong to WarSystem::addWar(): hasActiveRaceWar()
    // is not set yet while this runs.

    // During a war, fighting inside the castle is free
    de::gameContext().castleInfos().releaseAllSafeZone();

    // Every guardian shrine shield disappears.
    de::gameContext().shrines().removeAllShrineShield();


    // Fix the time across Adam's holy land.
    de::gameContext().holyLands().fixTimeband(de::gameContext().variables().getVariable(RACE_WAR_TIMEBAND));

    de::gameContext().holyLands().killAllMonsters();

    RegenZoneManager::getInstance()->putTryingPosition();
    RegenZoneManager::getInstance()->broadcastStatus();

    // Put the dragon eye items at their initial positions.
    de::gameContext().dragonEyes().addAllDragonEyesToZone();


    // Record in the RaceWarHistory Table
    recordRaceWarStart();

    __END_CATCH
}

void RaceWar::recordRaceWarStart()

{
    __BEGIN_TRY

    vector<RaceCurrentNumRow> raceNums = defaultWarInfoRepository().loadRaceWarCurrentNums();

    uint slayerSum = 0;
    uint vampireSum = 0;
    uint oustersSum = 0;

    string slayerOld;
    string vampireOld;
    string oustersOld;

    for (size_t r = 0; r < raceNums.size(); r++) {
        uint race = raceNums[r].race;
        uint num = raceNums[r].currentNum;

        if (race == 0)
            slayerSum = num;
        else if (race == 1)
            vampireSum = num;
        else if (race == 2)
            oustersSum = num;
    }

    vector<ShrineOwnerRow> owners = defaultWarInfoRepository().loadShrineOwners();

    for (size_t r = 0; r < owners.size(); r++) {
        uint id = owners[r].id;
        uint ownerRace = owners[r].ownerRace;

        if (ownerRace == 0)
            slayerOld = slayerOld + itos(id) + "|";
        else if (ownerRace == 1)
            vampireOld = vampireOld + itos(id) + "|";
        else if (ownerRace == 2)
            oustersOld = oustersOld + itos(id) + "|";
    }

    defaultWarInfoRepository().insertRaceWarHistory(getWarStartTime().toStringforWeb(), slayerSum, vampireSum,
                                                    oustersSum, slayerOld, vampireOld, oustersOld);

    __END_CATCH
}

//--------------------------------------------------------------------------------
//
// executeEnd
//
//--------------------------------------------------------------------------------
// What has to be handled when a war ends
//--------------------------------------------------------------------------------
void RaceWar::executeEnd()

{
    __BEGIN_TRY

    ShrineInfoManager& shrines = de::gameContext().shrines();

    //----------------------------------------------------------------------------
    // Report that the war has ended.
    //----------------------------------------------------------------------------
    sendWarEndMessage();


    //----------------------------------------------------------------------------
    // Give the blood bible fragments back.
    //----------------------------------------------------------------------------
    // As at the start, the zone work below is posted to the groups that own
    // the zones: a bible's return to whoever holds it, the shields, safe zones,
    // transports, time, towers and flags to each zone's or player's group.
    // The war participant list is this thread's own; the broadcasts to the
    // holy land's zones are posted to their groups too.
    shrines.returnAllBloodBible();

    shrines.addAllShrineShield();

    de::gameContext().castleInfos().resetAllSafeZone();

    de::gameContext().castleInfos().transportAllOtherRace();


    // Broadcast the blood bible positions across Adam's holy land.
    shrines.broadcastBloodBibleStatus();

    // Let the time that was fixed across Adam's holy land run again.
    de::gameContext().holyLands().resumeTimeband();

    // Remove every entry from the war participant list.
    RaceWarLimiter::clearPCList();

    // Set the participant count back to 0.
    RaceWarLimiter::getInstance()->clearCurrent();
    RegenZoneManager::getInstance()->deleteTryingPosition();
    RegenZoneManager::getInstance()->reload();

    // Remove the Flag from every character too.
    de::gameContext().zoneGroups().removeFlag(Effect::EFFECT_CLASS_RACE_WAR_JOIN_TICKET);

    de::gm::opworld(NULL, "*world *load blood_bible_owner", 0, true);

    // Remove the dragon eye items.
    de::gameContext().dragonEyes().removeAllDragonEyes();

    // Record in the RaceWarHistory Table
    recordRaceWarEnd();

    __END_CATCH
}

void RaceWar::recordRaceWarEnd()

{
    __BEGIN_TRY

    vector<ShrineOwnerRow> owners = defaultWarInfoRepository().loadShrineOwners();

    string slayerNew;
    string vampireNew;
    string oustersNew;

    for (size_t r = 0; r < owners.size(); r++) {
        uint id = owners[r].id;
        uint ownerRace = owners[r].ownerRace;

        if (ownerRace == 0)
            slayerNew = slayerNew + itos(id) + "|";
        else if (ownerRace == 1)
            vampireNew = vampireNew + itos(id) + "|";
        else if (ownerRace == 2)
            oustersNew = oustersNew + itos(id) + "|";
    }

    defaultWarInfoRepository().updateRaceWarBloodBibles(slayerNew, vampireNew, oustersNew,
                                                        getWarStartTime().toStringforWeb());

    // running a script -- who would have thought the system function would be used
    char cmd[100];
    snprintf(cmd, sizeof(cmd), "/home/darkeden/vs/bin/script/recordRaceWarHistory.py %s %d %d ",
             getWarStartTime().toStringforWeb().c_str(), de::kernelContext().config().getPropertyInt("Dimension"),
             de::kernelContext().config().getPropertyInt("WorldID"));

    filelog("script.log", cmd);
    system(cmd);

    __END_CATCH
}

// A shrine set changes owner for the race of the player who places the matching
// blood bible on its holy shrine, and the race war is open to whoever the holy
// land itself is open to while it runs: everyone when the participant limiter
// is off, and otherwise only the players who signed up and carry the join
// ticket. The same pair of tests decides who may enter a holy land at all.
bool RaceWar::mayModifyShrineOwner(PlayerCreature* pPC) {
    if (pPC == NULL)
        return false;

    if (!de::gameContext().variables().isActiveRaceWarLimiter())
        return true;

    return pPC->isFlag(Effect::EFFECT_CLASS_RACE_WAR_JOIN_TICKET);
}

string RaceWar::getWarName() const

{
    __BEGIN_TRY

    return "Race War";

    __END_CATCH
}

//--------------------------------------------------------------------------------
// When the war ends
//--------------------------------------------------------------------------------
void RaceWar::sendWarEndMessage() const

{
    __BEGIN_TRY

    War::sendWarEndMessage();

    // The packet that confirms the safe zone release?
    GCNoticeEvent gcNoticeEvent;
    gcNoticeEvent.setCode(NOTICE_EVENT_RACE_WAR_OVER);
    de::gameContext().zoneGroups().broadcast(&gcNoticeEvent);

    __END_CATCH
}


void RaceWar::makeWarScheduleInfo(WarScheduleInfo* pWSI) const

{
    __BEGIN_TRY

    pWSI->warType = getWarType();

    __END_CATCH
}

void RaceWar::makeWarInfo(WarInfo* pWarInfo) const

{
    __BEGIN_TRY

    Assert(pWarInfo != NULL);
    Assert(pWarInfo->getWarType() == WAR_RACE);

    RaceWarInfo* pRaceWarInfo = dynamic_cast<RaceWarInfo*>(pWarInfo);
    Assert(pRaceWarInfo != NULL);

    const unordered_map<ZoneID_t, CastleInfo*>& castleInfos = de::gameContext().castleInfos().getCastleInfos();

    unordered_map<ZoneID_t, CastleInfo*>::const_iterator itr = castleInfos.begin();

    for (; itr != castleInfos.end(); itr++) {
        CastleInfo* pCastleInfo = itr->second;
        pRaceWarInfo->addCastleID(pCastleInfo->getZoneID());
    }

    __END_CATCH
}


string RaceWar::toString() const

{
    __BEGIN_TRY

    StringStream msg;

    msg << "RaceWar(" << "WarID:" << (int)getWarID() << ",State:" << (int)getState() << ")";

    return msg.toString();

    __END_CATCH
}
