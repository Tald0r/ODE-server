#include "LevelWarManager.h"

#include <stdio.h>

#include "GCNoticeEvent.h"
#include "GCSweeperBonusInfo.h"
#include "GCSystemMessage.h"
#include "GameContext.h"
#include "KernelContext.h"
#include "LevelWar.h"
#include "LevelWarInfo.h"
#include "LevelWarZoneInfoManager.h"
#include "Player.h"
#include "PlayerCreature.h"
#include "Properties.h"
#include "StringPool.h"
#include "SweeperBonusManager.h"
#include "VariableManager.h"
#include "ZoneGroupManager.h"
#include "gm/GMCommands.h"
#include "repository/WarInfoRepository.h"

// Days the war takes place
int LevelWarTime[4][3] = {
    {2, 20, 0}, // Tuesday 8 o'clock
    {4, 20, 0}, // Thursday 8 o'clock
    {2, 21, 0}, // Tuesday 9 o'clock
    {4, 21, 0}, // Thursday 9 o'clock
};

void LevelWarManager::init() {
    m_SweeperSetManager.load(m_Level, m_pZone);

    SAFE_DELETE(m_pLevelWarSchedule);

    VSDateTime warStartTime = getNextLevelWarTime();
    m_pLevelWarSchedule = new Schedule(new LevelWar(this), warStartTime);

    filelog("WarLog.txt", "[Level=%d, Time=%s] Adding a level war.", m_Level, warStartTime.toString().c_str());

    addSchedule(m_pLevelWarSchedule);
}

Work* LevelWarManager::heartbeat()

{
    Work* pWork = NULL;
    pWork = Scheduler::heartbeat();

    if (m_pLevelWarSchedule != NULL && !m_bHasWar && de::gameContext().variables().isActiveLevelWar()) {
        m_bHasWarToDay = VSDateTime::currentDateTime().daysTo(m_pLevelWarSchedule->getScheduledTime()) <= 3;
    }

    if (m_bHasWar) {
        // If there is a war, refresh GCWarList
        makeGCWarList();
    }

    return pWork;
}

void LevelWarManager::startWar() {
    m_bHasWar = true;

    // Record the current time as the start time.
    // startTime and Level are both needed when the record is written.
    setLevelWarStartTime(VSDateTime::currentDateTime());

    int year = VSDate::currentDate().year() - 2000;
    int month = VSDate::currentDate().month();
    int day = VSDate::currentDate().day();
    int hour = VSTime::currentTime().hour();
    int level = 0;
    if (m_pZone->getZoneID() == 1131)
        level = 1;
    else if (m_pZone->getZoneID() == 1132)
        level = 2;
    else if (m_pZone->getZoneID() == 1133)
        level = 3;
    else if (m_pZone->getZoneID() == 1134)
        level = 4;

    fixTimeband(de::gameContext().variables().getVariable(RACE_WAR_TIMEBAND));

    killAllMonsters();

    GCNoticeEvent gcNoticeEvent;
    gcNoticeEvent.setCode(NOTICE_EVENT_LEVEL_WAR_STARTED);
    //	gcNoticeEvent.setParameter( ((DWORD)((DWORD)month << 24)) | ((DWORD)((DWORD)day << 16)) | ((DWORD)((DWORD)hour
    //<< 8)) | ((DWORD)((DWORD)level)) );
    gcNoticeEvent.setParameter((level * 100000000) + (year * 1000000) + (month * 10000) + (day * 100) + hour);
    de::gameContext().zoneGroups().broadcast(&gcNoticeEvent);

    GCSweeperBonusInfo gcSweeperBonusInfo;
    de::gameContext().sweeperBonuses().makeVoidSweeperBonusInfo(gcSweeperBonusInfo);
    de::gameContext().levelWarZones().broadcast(m_pZone->getZoneID(), &gcSweeperBonusInfo);

    // Write the record.
    recordLevelWarStart();
}

void LevelWarManager::recordLevelWarStart() {
    vector<SweeperBonusOwnerRow> owners = defaultWarInfoRepository().loadSweeperOwnerRaces(m_pZone->getZoneID());

    string slayerOld;
    string vampireOld;
    string oustersOld;
    string defaultOld;

    for (size_t r = 0; r < owners.size(); r++) {
        uint id = owners[r].type;
        uint ownerRace = owners[r].ownerRace;

        if (ownerRace == 0)
            slayerOld = slayerOld + itos(id) + "|";
        else if (ownerRace == 1)
            vampireOld = vampireOld + itos(id) + "|";
        else if (ownerRace == 2)
            oustersOld = oustersOld + itos(id) + "|";
        else
            defaultOld = defaultOld + itos(id) + "|";
    }

    defaultWarInfoRepository().insertLevelWarHistory(m_Level, getLevelWarStartTime().toStringforWeb(), slayerOld,
                                                     vampireOld, oustersOld, defaultOld);
}

void LevelWarManager::manualStart() {
    if (!isEmpty()) {
        addSchedule(new Schedule(popRecentWork(), VSDateTime::currentDateTime()));
    } else {
        m_pLevelWarSchedule = new Schedule(new LevelWar(this), VSDateTime::currentDateTime());
        addSchedule(m_pLevelWarSchedule);
    }
}

void LevelWarManager::endWar() {
    m_bHasWar = false;
    m_SweeperSetManager.returnAllSweeper();

    resumeTimeband();

    GCSweeperBonusInfo gcSweeperBonusInfo;
    de::gameContext().sweeperBonuses().makeSweeperBonusInfo(gcSweeperBonusInfo);
    de::gameContext().levelWarZones().broadcast(m_pZone->getZoneID(), &gcSweeperBonusInfo);

    GCNoticeEvent gcNoticeEvent;
    gcNoticeEvent.setCode(NOTICE_EVENT_LEVEL_WAR_OVER);
    m_pZone->broadcastPacket(&gcNoticeEvent);

    VSDateTime warStartTime = getNextLevelWarTime();
    m_pLevelWarSchedule = new Schedule(new LevelWar(this), warStartTime);

    filelog("WarLog.txt", "[Level=%d, Time=%s] Adding a level war.", m_Level, warStartTime.toString().c_str());
    addSchedule(m_pLevelWarSchedule);

    char sLoad[100];
    snprintf(sLoad, sizeof(sLoad), "*world *load sweeper_owner %d", m_Level);
    de::gm::opworld(NULL, sLoad, 0, true);

    // Write the record
    recordLevelWarEnd();
}

void LevelWarManager::recordLevelWarEnd() {
    vector<SweeperBonusOwnerRow> owners = defaultWarInfoRepository().loadSweeperOwnerRaces(m_pZone->getZoneID());

    string slayerNew;
    string vampireNew;
    string oustersNew;
    string defaultNew;

    for (size_t r = 0; r < owners.size(); r++) {
        uint id = owners[r].type;
        uint ownerRace = owners[r].ownerRace;

        if (ownerRace == 0)
            slayerNew = slayerNew + itos(id) + "|";
        else if (ownerRace == 1)
            vampireNew = vampireNew + itos(id) + "|";
        else if (ownerRace == 2)
            oustersNew = oustersNew + itos(id) + "|";
        else
            defaultNew = defaultNew + itos(id) + "|";
    }

    defaultWarInfoRepository().updateLevelWarHistory(slayerNew, vampireNew, oustersNew, defaultNew, m_Level,
                                                     getLevelWarStartTime().toStringforWeb());

    // Run the script through the system function.
    char cmd[100];
    snprintf(cmd, sizeof(cmd), "/home/darkeden/vs/bin/script/recordLevelWarHistory.py %d %s %d %d ", m_Level,
             getLevelWarStartTime().toStringforWeb().c_str(), de::kernelContext().config().getPropertyInt("Dimension"),
             de::kernelContext().config().getPropertyInt("WorldID"));

    filelog("script.log", cmd);
    system(cmd);
}

bool LevelWarManager::putSweeper(PlayerCreature* pPC, Item* pItem, MonsterCorpse* pCorpse) {
    int race = m_SweeperSetManager.getSafeIndex(pCorpse);

    if (pPC->isSlayer() && race != 0)
        return false;

    if (pPC->isVampire() && race != 1)
        return false;

    if (pPC->isOusters() && race != 2)
        return false;

    return m_SweeperSetManager.putSweeper(pItem, pCorpse);
}

VSDateTime LevelWarManager::getNextLevelWarTime() const {
    VSDateTime now = VSDateTime::currentDateTime();
    int dayOfWeek = now.date().dayOfWeek();
    bool isSameDay = false;

    if (dayOfWeek == LevelWarTime[m_Level - 1][0])
        isSameDay = now.time() < VSTime(LevelWarTime[m_Level - 1][1], LevelWarTime[m_Level - 1][2]);

    if (isSameDay)
        return VSDateTime(now.date(), VSTime(LevelWarTime[m_Level - 1][1], LevelWarTime[m_Level - 1][2]));

    int lastDays = LevelWarTime[m_Level - 1][0] - dayOfWeek;
    if (lastDays <= 0)
        lastDays += 7;

    return VSDateTime(now.date().addDays(lastDays), VSTime(LevelWarTime[m_Level - 1][1], LevelWarTime[m_Level - 1][2]));
}

void LevelWarManager::fixTimeband(uint timeband)

{
    __BEGIN_TRY

    Assert(m_pZone != NULL);
    m_pZone->stopTime();
    m_pZone->setTimeband(timeband);
    m_pZone->resetDarkLightInfo();

    __END_CATCH
}

void LevelWarManager::resumeTimeband()

{
    __BEGIN_TRY

    Assert(m_pZone != NULL);
    m_pZone->resumeTime();
    m_pZone->resetDarkLightInfo();

    __END_CATCH
}

void LevelWarManager::killAllMonsters()

{
    __BEGIN_TRY

    Assert(m_pZone != NULL);
    m_pZone->killAllMonsters_UNLOCK();

    __END_CATCH
}

int LevelWarManager::getStartHour() {
    __BEGIN_TRY

    if (m_pLevelWarSchedule == NULL)
        return -1;

    return m_pLevelWarSchedule->getScheduledTime().time().hour();

    __END_CATCH
}

bool LevelWarManager::makeGCWarList()

{
    __BEGIN_TRY

    __ENTER_CRITICAL_SECTION(m_MutexWarList)

    m_GCWarList.clear();

    if (!hasWar()) {
        return false;
    }

    if (isEmpty()) {
        return false;
    }

    const RecentSchedules::container_type& Schedules = m_RecentSchedules.getSchedules();
    RecentSchedules::const_iterator itr = Schedules.begin();
    RecentSchedules::const_iterator endItr = Schedules.end();

    for (; itr != endItr; itr++) {
        Schedule* pSchedule = *itr;

        LevelWarInfo* pLevelWarInfo = new LevelWarInfo;

        const Work* pWork = pSchedule->getWork();
        Assert(pWork != NULL);

        VSDateTime dt(VSDateTime::currentDateTime());

        int endHour = pSchedule->getScheduledTime().time().hour();
        int endMin = pSchedule->getScheduledTime().time().minute();
        int endSec = pSchedule->getScheduledTime().time().second();

        int curHour = dt.time().hour();
        int curMin = dt.time().minute();
        int curSec = dt.time().second();

        int endSecs = endHour * 60 * 60 + endMin * 60 + endSec;
        int curSecs = curHour * 60 * 60 + curMin * 60 + curSec;

        int remainSec = 0;
        if (endSecs > curSecs)
            remainSec = endSecs - curSecs;

        pLevelWarInfo->setLevel(m_Level);
        pLevelWarInfo->setRemainTime(remainSec);

        if (remainSec != 0)
            m_GCWarList.addWarInfo(pLevelWarInfo);
    }

    __LEAVE_CRITICAL_SECTION(m_MutexWarList)

    return true;

    __END_CATCH
}

void LevelWarManager::sendGCWarList(Player* pPlayer)

{
    __BEGIN_TRY

    __ENTER_CRITICAL_SECTION(m_MutexWarList)

    if (!m_GCWarList.isEmpty()) {
        pPlayer->sendPacket(&m_GCWarList);
    }

    __LEAVE_CRITICAL_SECTION(m_MutexWarList)

    __END_CATCH
}
void LevelWarManager::broadcastGCWarList()

{
    __BEGIN_TRY

    __ENTER_CRITICAL_SECTION(m_MutexWarList)

    m_pZone->broadcastPacket(&m_GCWarList);

    __LEAVE_CRITICAL_SECTION(m_MutexWarList)

    __END_CATCH
}

void LevelWarManager::freeUserTimeCheck()

{
    __BEGIN_TRY

    int hour = VSTime::currentTime().hour();

    if (m_bCanEnterFreeUser && hour != LevelWarTime[m_Level - 1][1]) {
        m_bCanEnterFreeUser = false;

        // Keep only the paying players in the Zone.
        m_pZone->remainPayPlayer();
    } else if (!m_bCanEnterFreeUser && hour == LevelWarTime[m_Level - 1][1]) {
        m_bCanEnterFreeUser = true;

        GCSystemMessage gcSystemMessage;
        char msg[100];

        snprintf(msg, sizeof(msg), de::gameContext().strings().c_str(STRID_LEVEL_WAR_ZONE_FREE_OPEN), m_Level, hour,
                 hour + 1);

        gcSystemMessage.setMessage(msg);
        de::gameContext().zoneGroups().broadcast(&gcSystemMessage);
    }

    __END_CATCH
}
