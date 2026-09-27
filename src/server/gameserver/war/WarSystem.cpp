#include "WarSystem.h"

#include <stdio.h>

#include <algorithm>

#include "Assert.h"
#include "CastleInfoManager.h"
#include "ClientManager.h"
#include "EventRefreshHolyLandPlayer.h"
#include "GCNoticeEvent.h"
#include "GCSystemMessage.h"
#include "GCWarList.h"
#include "GCWarScheduleList.h"
#include "GameContext.h"
#include "GamePlayer.h"
#include "GuildWarInfo.h"
#include "HolyLandManager.h"
#include "Player.h"
#include "RaceWar.h"
#include "RaceWarInfo.h"
#include "ShrineInfoManager.h"
#include "SiegeWar.h"
#include "StringPool.h"
#include "StringStream.h"
#include "VariableManager.h"
#include "War.h"
#include "WarSchedule.h"
#include "WarScheduler.h"
#include "ZoneGroupManager.h"

WarID_t WarSystem::s_WarIDSuccessor = 0;

WarSystem::WarSystem()

{
    __BEGIN_TRY

    m_Mutex.setName("WarSystem");
    m_MutexWarQueue.setName("WarSystemQueue");
    m_MutexActiveWars.setName("ActiveWars");
    m_MutexWarList.setName("WarList");

    m_bHasRaceWar = false;
    m_bRaceWarToday = false;

    m_pRaceWarSchedule = NULL;

    m_b5Minutes = false;
    m_b20Minutes = false;

    __END_CATCH
}

WarSystem::~WarSystem() {
    SAFE_DELETE(m_pRaceWarSchedule);
}

void WarSystem::init() {
    __BEGIN_TRY

    load();

    prepareRaceWar();

    __END_CATCH
}

void WarSystem::prepareRaceWar() {
    __BEGIN_TRY

    SAFE_DELETE(m_pRaceWarSchedule);

    // Prepare the race war.
    VSDateTime warStartTime = WarScheduler::getNextWarDateTime(WAR_RACE, VSDateTime::currentDateTime());

    War* pRaceWar = new RaceWar(War::WAR_STATE_WAIT);
    pRaceWar->setWarStartTime(warStartTime);
    m_pRaceWarSchedule = new Schedule(pRaceWar, warStartTime);

    filelog("WarLog.txt", "[WarID=%d,Time=%s] Adding a race war.", (int)pRaceWar->getWarID(),
            warStartTime.toString().c_str());

    //	m_RaceWarTimeParam = ((DWORD)((DWORD)warStartTime.date().month() << 24)) |
    //((DWORD)((DWORD)warStartTime.date().day() << 16)) | ((DWORD)((DWORD)warStartTime.time().hour() << 8));
    VSDateTime sendStartTime = warStartTime.addDays(-7);
    m_RaceWarTimeParam =
        ((DWORD)((DWORD)(sendStartTime.date().year() - 2000)) * 1000000) +
        ((DWORD)((DWORD)sendStartTime.date().month()) * 10000) +
        ((DWORD)((DWORD)sendStartTime.date().day()) * 100); //	   + ((DWORD)((DWORD)sendStartTime.time().hour()));

    cout << "Race war packet send date : " << m_RaceWarTimeParam.load() << endl;

    __END_CATCH
}


void WarSystem::load()

    {__BEGIN_TRY

         // There is nothing to load.

         // The war in progress has to be loaded.

         __END_CATCH}

VSDateTime WarSystem::getWarEndTime(WarType_t warType) const {
    int seconds = 0;
    switch (warType) {
    // A guild war lasts 1 hour
    case WAR_GUILD:
        seconds = de::gameContext().variables().getVariable(GUILD_WAR_TIME);
        break;

    // A race war lasts 2 hours
    case WAR_RACE:
        seconds = de::gameContext().variables().getVariable(RACE_WAR_TIME);
        break;
    }

    VSDateTime dt(VSDateTime::currentDateTime());

    return dt.addSecs(seconds);
}

bool WarSystem::addWarDelayed(War* pWar)

{
    __BEGIN_TRY

    Assert(pWar != NULL);

    if (hasActiveRaceWar() && pWar->getWarType() == WAR_RACE) {
        throw Error("A race war is already in progress.");
    }

    __ENTER_CRITICAL_SECTION(m_MutexWarQueue);

    m_WarQueue.push_back(pWar);

    __LEAVE_CRITICAL_SECTION(m_MutexWarQueue);

    return true;

    __END_CATCH
}

bool WarSystem::addQueuedWar()

{
    __BEGIN_TRY

    // The queue is taken out under its mutex and the wars are added after it
    // is released: adding a war locks zones, and a castle zone's thread hands
    // its starting war to addWarDelayed while its heartbeat holds that zone's
    // mutex. A war that fails to be added is dropped; the ones behind it go
    // back to the front of the queue for the next heartbeat.
    list<War*> queuedWars;

    __ENTER_CRITICAL_SECTION(m_MutexWarQueue);

    queuedWars.swap(m_WarQueue);

    __LEAVE_CRITICAL_SECTION(m_MutexWarQueue);

    while (!queuedWars.empty()) {
        War* pWar = queuedWars.front();
        queuedWars.pop_front();

        try {
            Assert(pWar != NULL);

            addWar(pWar);
        } catch (...) {
            __ENTER_CRITICAL_SECTION(m_MutexWarQueue);

            m_WarQueue.splice(m_WarQueue.begin(), queuedWars);

            __LEAVE_CRITICAL_SECTION(m_MutexWarQueue);

            throw;
        }
    }

    return true;

    __END_CATCH
}

// Called only inside WarSystem, so no LOCK is needed.
bool WarSystem::addWar(War* pWar)

{
    __BEGIN_TRY

    Assert(pWar != NULL);
    Assert(pWar->getState() == War::WAR_STATE_CURRENT);

    WarType_t warType = pWar->getWarType();
    VSDateTime warEndTime = getWarEndTime(warType);

    Schedule::ScheduleType scheduleType;

    scheduleType = Schedule::SCHEDULE_TYPE_ONCE;

    WarSchedule* pWarSchedule = new WarSchedule(pWar, warEndTime, scheduleType);

    addSchedule(pWarSchedule);

    // For now broadcast to every zone.
    if (makeGCWarList_LOCKED()) {
        GCWarList gcWarList;

        __ENTER_CRITICAL_SECTION(m_MutexWarList)

        gcWarList = m_GCWarList;

        __LEAVE_CRITICAL_SECTION(m_MutexWarList)

        de::gameContext().zoneGroups().broadcast(&gcWarList);
    }


    // Add it to the list of wars in progress.
    // heartbeat() removes it.
    if (pWar->getWarType() == WAR_GUILD) {
        // Refresh the state of the users in the holy land.
        EventRefreshHolyLandPlayer* pEvent = new EventRefreshHolyLandPlayer(NULL);
        pEvent->setDeadline(0);
        de::gameContext().clients().addEvent(pEvent);

        __ENTER_CRITICAL_SECTION(m_MutexActiveWars)

        m_ActiveWars.push_back(ActiveWarInfo(pWar->getCastleZoneID(), pWar->getAttackerGuildID()));

        __LEAVE_CRITICAL_SECTION(m_MutexActiveWars)
    } else if (pWar->getWarType() == WAR_RACE) {
        m_bHasRaceWar = true;

        // Refresh the state of the users in the holy land.
        EventRefreshHolyLandPlayer* pEvent = new EventRefreshHolyLandPlayer(NULL);
        pEvent->setDeadline(0);
        de::gameContext().clients().addEvent(pEvent);

        // Broadcast the blood bible positions across Adam's holy land.
        de::gameContext().shrines().broadcastBloodBibleStatus();

        // Send out everyone not taking part in the race war.
        de::gameContext().holyLands().remainRaceWarPlayers();
    }

    return true;

    __END_CATCH
}


bool WarSystem::makeGCWarList_LOCKED()

{
    __BEGIN_TRY

    __ENTER_CRITICAL_SECTION(m_MutexWarList)

    m_GCWarList.clear();

    if (isEmpty()) {
        return false;
    }

    const RecentSchedules::container_type& Schedules = m_RecentSchedules.getSchedules();
    RecentSchedules::const_iterator itr = Schedules.begin();

    for (; itr != Schedules.end(); itr++) {
        Schedule* pSchedule = *itr;

        WarSchedule* pWarSchedule = dynamic_cast<WarSchedule*>(pSchedule);
        Assert(pWarSchedule != NULL);

        War* pWar = pWarSchedule->getWar();
        Assert(pWar != NULL);

        WarInfo* pWarInfo = NULL;

        switch (pWar->getWarType()) {
        case WAR_GUILD:
            pWarInfo = new GuildWarInfo;
            break;
        case WAR_RACE:
            pWarInfo = new RaceWarInfo;
            break;
        default:
            throw Error("Invalid war type.");
        }

        pWarSchedule->makeWarInfo(pWarInfo);

        m_GCWarList.addWarInfo(pWarInfo);
    }

    __LEAVE_CRITICAL_SECTION(m_MutexWarList)

    return true;

    __END_CATCH
}

bool WarSystem::makeGCWarList()

{
    __BEGIN_TRY

    bool ret = false;

    __ENTER_CRITICAL_SECTION(m_Mutex)

    ret = makeGCWarList_LOCKED();

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    return ret;

    __END_CATCH

    return false;
}

void WarSystem::sendGCWarList(Player* pPlayer)

{
    __BEGIN_TRY

    __ENTER_CRITICAL_SECTION(m_MutexWarList)

    if (!m_GCWarList.isEmpty()) {
        pPlayer->sendPacket(&m_GCWarList);
    }

    __LEAVE_CRITICAL_SECTION(m_MutexWarList)

    __END_CATCH
}

Work* WarSystem::heartbeat()

{
    __BEGIN_TRY

    Work* pWork = NULL;

    ZoneGroupManager& zoneGroups = de::gameContext().zoneGroups();
    VariableManager& variables = de::gameContext().variables();

    __ENTER_CRITICAL_SECTION(m_Mutex)

    addQueuedWar();

    Schedule* pSchedule = m_pRaceWarSchedule;
    if (pSchedule != NULL) {
        War* pWar = dynamic_cast<War*>(pSchedule->getWork());
        if (pWar != NULL && pWar->getWarType() == WAR_RACE) {
            int lastSec = VSDateTime::currentDateTime().secsTo(pSchedule->getScheduledTime());
            // 20 minutes before
            if (lastSec < 20 * 60 && !m_b20Minutes) {
                m_b20Minutes = true;

                // Refresh the state of the users in the holy land.
                EventRefreshHolyLandPlayer* pEvent = new EventRefreshHolyLandPlayer(NULL);
                pEvent->setDeadline(0);
                de::gameContext().clients().addEvent(pEvent);

                GCNoticeEvent gcNE;
                gcNE.setCode(NOTICE_EVENT_RACE_WAR_IN_20);
                zoneGroups.broadcast(&gcNE);
            } else if (lastSec < 5 * 60 && !m_b5Minutes) {
                m_b5Minutes = true;

                GCNoticeEvent gcNE;
                gcNE.setCode(NOTICE_EVENT_RACE_WAR_IN_5);
                zoneGroups.broadcast(&gcNE);
            }
            if (m_b20Minutes && !variables.isAutoStartRaceWar()) {
                if (lastSec > 20 * 60)
                    m_b20Minutes = m_b5Minutes = false;
            }

            if (lastSec < 0 && !variables.isAutoStartRaceWar()) {
                GCNoticeEvent gcNE;
                gcNE.setCode(NOTICE_EVENT_RACE_WAR_STARTED_IN_OTHER_SERVER);
                zoneGroups.broadcast(&gcNE);

                VSDateTime warStartTime = WarScheduler::getNextWarDateTime(WAR_RACE, VSDateTime::currentDateTime());
                dynamic_cast<RaceWar*>(m_pRaceWarSchedule->getWork())->setWarStartTime(warStartTime);
                m_pRaceWarSchedule->setScheduledTime(warStartTime);
            }
        }
    }

    pWork = Scheduler::heartbeat();

    if (pWork != NULL) {
        // Handling for a war that ended because its time ran out
        War* pWar = dynamic_cast<War*>(pWork);
        Assert(pWar != NULL);

        // Mostly done in War::executeEnd().
        if (pWar->getWarType() == WAR_GUILD) {
            // Remove it from the list of wars in progress.
            __ENTER_CRITICAL_SECTION(m_MutexActiveWars)

            list<ActiveWarInfo>::iterator itr =
                find(m_ActiveWars.begin(), m_ActiveWars.end(), ActiveWarInfo(pWar->getCastleZoneID()));
            Assert(itr != m_ActiveWars.end());

            m_ActiveWars.erase(itr);

            __LEAVE_CRITICAL_SECTION(m_MutexActiveWars)
        } else if (pWar->getWarType() == WAR_RACE) {
            m_bHasRaceWar = false;

            // Refresh the state of the users in the holy land.
            EventRefreshHolyLandPlayer* pEvent = new EventRefreshHolyLandPlayer(NULL);
            pEvent->setDeadline(0);
            de::gameContext().clients().addEvent(pEvent);

            m_b20Minutes = false;
        }

        SAFE_DELETE(pWork);
    }

    // Start the race war automatically.
    if (m_pRaceWarSchedule != NULL && !m_bHasRaceWar && variables.isAutoStartRaceWar()) {
        checkStartRaceWar();
        m_bRaceWarToday = VSDateTime::currentDateTime().daysTo(m_pRaceWarSchedule->getScheduledTime()) <= 4;
    }

    // Refresh the WarList.
    static Timeval nextTime = {0, 0};
    Timeval currentTime;
    getCurrentTime(currentTime);

    if (currentTime > nextTime) {
        makeGCWarList_LOCKED();
        nextTime.tv_sec = currentTime.tv_sec + 10;
    }

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    return pWork;

    __END_CATCH
}

bool WarSystem::checkStartRaceWar()

{
    __BEGIN_TRY

    if (m_pRaceWarSchedule->heartbeat()) {
        Work* pWork = m_pRaceWarSchedule->popWork();
        Assert(pWork != NULL);

        War* pRaceWar = dynamic_cast<War*>(pWork);
        addWarDelayed(pRaceWar);

        prepareRaceWar();
        m_b5Minutes = false;

        return true;
    }

    return false;

    __END_CATCH
}

bool WarSystem::getAttackGuildID(ZoneID_t zoneID, GuildID_t& guildID) const

{
    __BEGIN_TRY

    bool bHasCastleActiveWar = false;

    __ENTER_CRITICAL_SECTION(m_MutexActiveWars)

    list<ActiveWarInfo>::const_iterator itr = find(m_ActiveWars.begin(), m_ActiveWars.end(), ActiveWarInfo(zoneID));

    if (itr != m_ActiveWars.end()) {
        bHasCastleActiveWar = true;
        guildID = (*itr).AttackGuildID;
    }

    __LEAVE_CRITICAL_SECTION(m_MutexActiveWars)

    return bHasCastleActiveWar;

    __END_CATCH
}

bool WarSystem::hasCastleActiveWar(ZoneID_t zoneID) const

{
    __BEGIN_TRY

    bool bHasCastleActiveWar = false;

    // The running wars are answered from m_ActiveWars under its own leaf
    // mutex rather than by walking the schedules under m_Mutex: callers
    // reach this from inside a zone effect that already holds locks of its
    // own, and taking m_Mutex here deadlocks against them.
    __ENTER_CRITICAL_SECTION(m_MutexActiveWars)

    list<ActiveWarInfo>::const_iterator itr = find(m_ActiveWars.begin(), m_ActiveWars.end(), ActiveWarInfo(zoneID));

    if (itr != m_ActiveWars.end()) {
        bHasCastleActiveWar = true;
    }

    __LEAVE_CRITICAL_SECTION(m_MutexActiveWars)

    return bHasCastleActiveWar;

    __END_CATCH
}

WarSchedule* WarSystem::getActiveWarSchedule_LOCKED(ZoneID_t zoneID)

{
    __BEGIN_TRY

    const RecentSchedules::container_type& schedules = m_RecentSchedules.getSchedules();
    RecentSchedules::const_iterator itr = schedules.begin();

    for (; itr != schedules.end(); itr++) {
        WarSchedule* pWarSchedule = dynamic_cast<WarSchedule*>(*itr);
        Assert(pWarSchedule != NULL);

        War* pWar = dynamic_cast<War*>(pWarSchedule->getWork());
        if (pWar == NULL) {
            cout << "A Schedule in WarSystem holds a Work that is not a War, or is NULL." << endl;
            continue;
        }

        // Both castle war classes report WAR_GUILD, so the castle is asked of
        // the war itself; a war fought over no castle answers zero.
        if (pWar->getWarType() == WAR_GUILD && pWar->getCastleZoneID() == zoneID) {
            return pWarSchedule;
        }
    }

    return NULL;

    __END_CATCH
}

War* WarSystem::getActiveWar_LOCKED(ZoneID_t zoneID) const {
    const RecentSchedules::container_type& schedules = m_RecentSchedules.getSchedules();
    RecentSchedules::const_iterator itr = schedules.begin();

    for (; itr != schedules.end(); itr++) {
        War* pWar = dynamic_cast<War*>((*itr)->getWork());
        if (pWar == NULL)
            continue;

        // Both castle war classes report WAR_GUILD, so the castle is asked of
        // the war itself; a war fought over no castle answers zero.
        if (pWar->getWarType() == WAR_GUILD && pWar->getCastleZoneID() == zoneID) {
            return pWar;
        }
    }

    return NULL;
}

// The war is looked up and asked under m_Mutex, which the heartbeat holds
// when it ends the war and frees it. A castle with no war running has no
// owner to take, which is the answer when the war ended while the player was
// on the way.
bool WarSystem::isModifyCastleOwner(ZoneID_t castleZoneID, PlayerCreature* pPC) {
    Assert(pPC != NULL);

    bool bModify = false;

    __ENTER_CRITICAL_SECTION(m_Mutex)

    War* pWar = getActiveWar_LOCKED(castleZoneID);
    if (pWar != NULL)
        bModify = pWar->isModifyCastleOwner(pPC);

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    return bModify;
}

bool WarSystem::getSiegeGuildSide(ZoneID_t castleZoneID, GuildID_t guildID, int& side) const {
    bool bSiege = false;

    __ENTER_CRITICAL_SECTION(m_Mutex)

    const SiegeWar* pSiegeWar = dynamic_cast<const SiegeWar*>(getActiveWar_LOCKED(castleZoneID));
    if (pSiegeWar != NULL) {
        side = pSiegeWar->getGuildSide(guildID);
        bSiege = true;
    }

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    return bSiege;
}

// The flag is read first so that the schedule list, and the lock over it, are
// only touched while a race war is actually running. The flag is only a hint:
// the war is looked up and asked under m_Mutex, because the heartbeat frees it
// under that lock when it ends, so a pointer carried past the lock could name
// a war that is gone.
bool WarSystem::mayModifyShrineOwner(PlayerCreature* pPC) const {
    if (!hasActiveRaceWar())
        return false;

    bool bMay = false;

    __ENTER_CRITICAL_SECTION(m_Mutex)

    War* pWar = getActiveRaceWarAtSameThread();
    if (pWar != NULL)
        bMay = pWar->mayModifyShrineOwner(pPC);

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    return bMay;
}

// pPC won the war concerning castleZoneID.
bool WarSystem::endWar(PlayerCreature* pPC, ZoneID_t castleZoneID)

{
    __BEGIN_TRY

    Assert(pPC != NULL);

    bool bEndWar = false;

    __ENTER_CRITICAL_SECTION(m_Mutex)

    WarSchedule* pWarSchedule = getActiveWarSchedule_LOCKED(castleZoneID);

    if (pWarSchedule != NULL) {
        Work* pWork = pWarSchedule->getWork();
        Assert(pWork != NULL);

        War* pWar = dynamic_cast<War*>(pWork);
        Assert(pWar != NULL);

        if (pWar->endWar(pPC)) {
            // Remove the war (let it drop out on its own by changing the time)
            pWarSchedule->setScheduledTime(VSDateTime::currentDateTime());

            // The heap has to be rebuilt.
            m_RecentSchedules.arrange();

            bEndWar = true;
        }
    }

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    return bEndWar;

    __END_CATCH
}

// Removes the war in progress for castleZoneID.
bool WarSystem::removeWar(ZoneID_t castleZoneID)

{
    __BEGIN_TRY

    bool bRemoved = false;

    __ENTER_CRITICAL_SECTION(m_Mutex)

    WarSchedule* pWarSchedule = getActiveWarSchedule_LOCKED(castleZoneID);

    if (pWarSchedule != NULL) {
        // Remove the war (let it drop out on its own by changing the time)
        pWarSchedule->setScheduledTime(VSDateTime::currentDateTime());

        // The heap has to be rebuilt.
        m_RecentSchedules.arrange();

        bRemoved = true;
    }

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    return bRemoved;

    __END_CATCH
}

// Removes the war in progress for castleZoneID.
bool WarSystem::removeRaceWar()

{
    __BEGIN_TRY

    bool bRemoved = false;

    __ENTER_CRITICAL_SECTION(m_Mutex)

    const RecentSchedules::container_type& schedules = m_RecentSchedules.getSchedules();
    RecentSchedules::const_iterator itr = schedules.begin();

    for (; itr != schedules.end(); itr++) {
        Schedule* pSchedule = *itr;
        War* pWar = dynamic_cast<War*>(pSchedule->getWork());
        if (pWar == NULL)
            continue;

        if (pWar->getWarType() == WAR_RACE) {
            // Remove the war (let it drop out on its own by changing the time)
            pSchedule->setScheduledTime(VSDateTime::currentDateTime());

            // The heap has to be rebuilt.
            m_RecentSchedules.arrange();

            bRemoved = true;
        }
    }

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    return bRemoved;

    __END_CATCH
}

// Sends a given player the list of wars in progress.
void WarSystem::broadcastWarList(GamePlayer* pGamePlayer) const

{
    __BEGIN_TRY

    StringPool& strings = de::gameContext().strings();

    __ENTER_CRITICAL_SECTION(m_Mutex)

    const RecentSchedules::container_type& schedules = m_RecentSchedules.getSchedules();
    RecentSchedules::const_iterator itr = schedules.begin();

    GCSystemMessage gcSystemMessage;
    bool warExist = false;

    if (isEmpty()) {
        gcSystemMessage.setMessage(strings.getString(STRID_NO_WAR_IN_ACTIVE));
        pGamePlayer->sendPacket(&gcSystemMessage);

        return;
    }

    for (; itr != schedules.end(); itr++) {
        WarSchedule* pSchedule = dynamic_cast<WarSchedule*>(*itr);
        if (pSchedule == NULL)
            continue;

        War* pWar = dynamic_cast<War*>(pSchedule->getWork());
        if (pWar == NULL)
            continue;

        warExist = true;


        char msg[100];
        snprintf(msg, sizeof(msg), strings.c_str(STRID_WAR_STATUS), pWar->getWarName().c_str(),
                 (pSchedule->getScheduledTime()).toString().c_str());
        gcSystemMessage.setMessage(msg);
        pGamePlayer->sendPacket(&gcSystemMessage);
    }

    if (!warExist) {
        gcSystemMessage.setMessage(strings.getString(STRID_NO_WAR_IN_ACTIVE));
        pGamePlayer->sendPacket(&gcSystemMessage);
    }

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    __END_CATCH
}

bool WarSystem::startRaceWar()

{
    __BEGIN_TRY

    if (hasActiveRaceWar())
        return false;

    __ENTER_CRITICAL_SECTION(m_Mutex);

    if (m_pRaceWarSchedule != NULL) {
        if (m_b5Minutes) {
            m_pRaceWarSchedule->setScheduledTime(VSDateTime::currentDateTime());
        } else if (m_b20Minutes) {
            m_pRaceWarSchedule->setScheduledTime(VSDateTime::currentDateTime().addSecs(60 * 5));
        } else {
            m_pRaceWarSchedule->setScheduledTime(VSDateTime::currentDateTime().addSecs(60 * 20));
        }

        checkStartRaceWar();
    }

    __LEAVE_CRITICAL_SECTION(m_Mutex);

    return true;

    __END_CATCH
}

War* WarSystem::getActiveRaceWarAtSameThread() const

{
    __BEGIN_TRY

    const RecentSchedules::container_type& schedules = m_RecentSchedules.getSchedules();
    RecentSchedules::const_iterator itr = schedules.begin();

    for (; itr != schedules.end(); itr++) {
        War* pWar = dynamic_cast<War*>((*itr)->getWork());
        if (pWar == NULL)
            continue;
        if (pWar->getWarType() == WAR_RACE) {
            return pWar;
        }
    }

    return NULL;

    __END_CATCH
}

bool WarSystem::addRaceWarScheduleInfo(WarScheduleInfo* pWSI)

{
    __BEGIN_TRY

    if (m_pRaceWarSchedule == NULL)
        return false;

    Assert(pWSI != NULL);

    __ENTER_CRITICAL_SECTION(m_Mutex);

    const VSDateTime& DT = m_pRaceWarSchedule->getScheduledTime();

    Work* pWork = m_pRaceWarSchedule->getWork();
    Assert(pWork != NULL);

    War* pWar = dynamic_cast<War*>(pWork);
    Assert(pWar != NULL);

    pWar->makeWarScheduleInfo(pWSI);
    pWSI->year = DT.date().year();
    pWSI->month = DT.date().month();
    pWSI->day = DT.date().day();
    pWSI->hour = DT.time().hour();

    __LEAVE_CRITICAL_SECTION(m_Mutex);

    return true;

    __END_CATCH
}
