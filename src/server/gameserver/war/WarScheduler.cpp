#include "WarScheduler.h"

#include <stdio.h>

#include "Assert.h"
#include "DB.h"
#include "GCWarScheduleList.h"
#include "GameContext.h"
#include "GuildWar.h"
#include "KernelContext.h"
#include "Properties.h"
#include "SiegeWar.h"
#include "VariableManager.h"
#include "WarSchedule.h"
#include "WarSystem.h"
#include "Zone.h"
#include "repository/WarInfoRepository.h"

// Days from each weekday to the next war of each kind, then the start hours;
// the tables below are the schedule, not a fixed evening.
const int NextWarDay[2][8] = {
    {0, 1, 7, 6, 5, 4, 3, 2}, // guild war
    //{ 0, 2, 1, 0, 3, 2, 1, 0 }	// RaceWar Sun,Mon,Tue,Wed,Thu,Fri,Sat,Sun
    {0, 6, 5, 4, 3, 2, 1, 0} // RaceWar Sun,Mon,Tue,Wed,Thu,Fri,Sat,Sun
};

// On the test server..
const int NextWarHour[2][24] = {
    //                               *     *              *     *
    // 0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16 17 18  19  20  21  22  23
    {10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 2, 1, 5, 4, 3, 2, 1, 2, 1, 15, 14, 13, 12, 11}, // guild war

    //                                           *                          *
    // 0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16 17 18  19  20  21  22  23
    {14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 8, 7, 6, 5, 4, 3, 2, 1, 16, 15} // race war
};


WarScheduler::WarScheduler(Zone* pZone)

    : m_pZone(pZone) {
    // It runs attached to a Zone.
    // But cancelGuildSchedules() is called from outside.
    m_Mutex.setName("WarSheduler");
}

WarScheduler::~WarScheduler()

{}

bool WarScheduler::makeGCWarScheduleList(GCWarScheduleList* pGCWarScheduleList) const

{
    {
        __ENTER_CRITICAL_SECTION(m_Mutex)

        const RecentSchedules::container_type& Schedules = m_RecentSchedules.getSchedules();
        RecentSchedules::const_iterator itr = Schedules.begin();

        for (; itr != Schedules.end(); itr++) {
            const WarSchedule* pWarSchedule = dynamic_cast<WarSchedule*>(*itr);
            Assert(pWarSchedule != NULL);

            WarScheduleInfo* pWSI = new WarScheduleInfo;
            pWarSchedule->makeWarScheduleInfo(pWSI);

            pGCWarScheduleList->addWarScheduleInfo(pWSI);
        }

        __LEAVE_CRITICAL_SECTION(m_Mutex)

        // When the automatic start is configured, the race war information always
        // goes in. It is the war system's, answered under its own mutex, and needs
        // nothing from this scheduler, so it is asked after this mutex is released
        // (see the lock order in WarSystem.h).
        if (de::gameContext().variables().isAutoStartRaceWar()) {
            WarScheduleInfo* pWSI = new WarScheduleInfo;
            if (de::gameContext().warSystem().addRaceWarScheduleInfo(pWSI)) {
                pGCWarScheduleList->addWarScheduleInfo(pWSI);
            } else {
                SAFE_DELETE(pWSI);
            }
        }
    }

    return true;
}

// A war whose time has come is taken out of the queue under the mutex and
// started after it is released: starting a castle war broadcasts to every
// group and works on the castle's dungeons, guard shrine and siege zone,
// none of which belongs under this mutex (see the lock order in
// WarSystem.h). Once taken out, no other thread can reach the war through
// this scheduler, so none sees it half started. A run that throws puts
// the schedule back for the next heartbeat, as it was when the run
// happened inside the queue: a start that failed is tried again, while a
// status save that failed after the start went through runs the war's
// next step, its end, one heartbeat later.
Work* WarScheduler::heartbeat()

{
    Schedule* pDue = NULL;

    __ENTER_CRITICAL_SECTION(m_Mutex)

    pDue = popDueSchedule();

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    if (pDue == NULL)
        return NULL;

    try {
        pDue->run();
    } catch (...) {
        __ENTER_CRITICAL_SECTION(m_Mutex)

        addSchedule(pDue);

        __LEAVE_CRITICAL_SECTION(m_Mutex)

        throw;
    }

    Work* pWork = pDue->popWork();
    SAFE_DELETE(pDue);

    return pWork;
}

void WarScheduler::load()

{
    __ENTER_CRITICAL_SECTION(m_Mutex)

    clear();


    VSDateTime currentDateTime(VSDateTime::currentDateTime());

    WarInfoRepository& repository = defaultWarInfoRepository();

    vector<WarScheduleRow> schedules =
        repository.loadWarSchedules(de::kernelContext().config().getPropertyInt("ServerID"), (int)m_pZone->getZoneID());

    if (!schedules.empty()) {
        WarID_t warID;
        uint challengerNum;
        GuildID_t challengerGuildID[5];
        Gold_t warRegistrationFee;
        string dateTemp;
        VSDateTime warStartTime;

        for (size_t r = 0; r < schedules.size(); r++) {
            warID = (WarID_t)schedules[r].warID;
            string warTypeStr = schedules[r].warType;

            if (warTypeStr == "RACE")
                continue;

            Assert(warTypeStr == "GUILD");

            // The row has five attacker slots, so a count past them would
            // walk off both this array and the siege's own.
            challengerNum = (uint)schedules[r].attackerCount;
            if (challengerNum > MaxSiegeChallengerGuilds)
                challengerNum = MaxSiegeChallengerGuilds;

            for (int j = 0; j < 5; ++j) {
                challengerGuildID[j] = (GuildID_t)schedules[r].attackGuildID[j];
            }

            warRegistrationFee = (Gold_t)schedules[r].warFee;
            dateTemp = schedules[r].startTime;
            warStartTime = VSDateTime(dateTemp);

            // For a war that should already have started, the start time is changed.
            if (warStartTime < currentDateTime) {
                warStartTime = currentDateTime;
            }

            // The row names the castle war class it was registered as. A
            // guild war comes back with the single guild that applied for
            // it; a siege with every challenger that joined and whatever
            // reinforcement was accepted.
            War* pWar = NULL;

            if (schedules[r].castleWarKind == "GUILD") {
                GuildWar* pGuildWar =
                    new GuildWar(m_pZone->getZoneID(), challengerGuildID[0], War::WAR_STATE_WAIT, warID);
                pGuildWar->setRegistrationFee(warRegistrationFee);

                pWar = pGuildWar;
            } else {
                SiegeWar* pSiegeWar = new SiegeWar(m_pZone->getZoneID(), War::WAR_STATE_WAIT, warID);
                pSiegeWar->setRegistrationFee(warRegistrationFee);

                int reinforceGuildID = 0;

                if (repository.loadAcceptedReinforceGuild(warID, reinforceGuildID)) {
                    pSiegeWar->setReinforceGuildID(reinforceGuildID);
                }

                for (uint j = 0; j < challengerNum; ++j) {
                    pSiegeWar->addChallengerGuild(challengerGuildID[j]);
                }

                pWar = pSiegeWar;
            }

            pWar->setWarStartTime(warStartTime);

            WarSchedule* pWarSchedule = new WarSchedule(pWar, warStartTime, Schedule::SCHEDULE_TYPE_ONCE);
            addSchedule(pWarSchedule);

            filelog("WarLog.txt", "[LOAD] %s", pWar->toString().c_str());
        }
    }

    // If no race war is set, set one.

    __LEAVE_CRITICAL_SECTION(m_Mutex)
}

int WarScheduler::getWarTypeCount(WarType_t warType)

{
    int raceWarCount = 0;

    __ENTER_CRITICAL_SECTION(m_Mutex)

    RecentSchedules::const_iterator itr = m_RecentSchedules.getSchedules().begin();

    for (; itr != m_RecentSchedules.getSchedules().end(); itr++) {
        WarSchedule* pWarSchedule = dynamic_cast<WarSchedule*>((*itr));
        Assert(pWarSchedule != NULL);

        War* pWar = dynamic_cast<War*>(pWarSchedule->getWork());
        Assert(pWar != NULL);

        if (pWar->getWarType() == warType) {
            raceWarCount++;
        }
    }

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    return raceWarCount;
}

void WarScheduler::tinysave(WarID_t warID, const string& query)

{
    __ENTER_CRITICAL_SECTION(m_Mutex)

    RecentSchedules::const_iterator itr = m_RecentSchedules.getSchedules().begin();

    for (; itr != m_RecentSchedules.getSchedules().end(); itr++) {
        WarSchedule* pWarSchedule = dynamic_cast<WarSchedule*>((*itr));

        if (pWarSchedule->getWarID() == warID) {
            pWarSchedule->tinysave(query);
            return;
        }
    }

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    filelog("WarError.log", "WarScheduler::tinySave() no WarSchedule with WarID:%d in the DB.", warID);
}

VSDateTime WarScheduler::getLastWarDateTime(WarType_t warType) const {
    const RecentSchedules::container_type& schedules = m_RecentSchedules.getSchedules();
    RecentSchedules::const_iterator itr = schedules.begin();

    bool bFound = false;
    VSDateTime dt = VSDateTime::currentDateTime();

    for (; itr != schedules.end(); itr++) {
        WarSchedule* pSchedule = dynamic_cast<WarSchedule*>(*itr);
        if (pSchedule->getWar()->getWarType() == warType) {
            if (bFound) {
                if (dt < pSchedule->getScheduledTime())
                    dt = pSchedule->getScheduledTime();
            } else {
                dt = pSchedule->getScheduledTime();
                bFound = true;
            }
        }
    }

    return dt;
}

// Get the war time after dt.
VSDateTime WarScheduler::getNextWarDateTime(WarType_t warType, const VSDateTime& dt) {
    int startHour = 0;

    VSDateTime nextWarDateTime;
    VSTime nextWarTime;

    if (de::gameContext().variables().isWarPeriodWeek()) // that is a bit much
    {
        switch (warType) {
        case WAR_GUILD:
            // Monday, Wednesday and Friday after dt, 8 pm (~9 pm)
            startHour = 20;
            break;

        case WAR_RACE:
            // Sunday after dt, 7 pm (~9 pm)
            startHour = 19;
            break;
        }

        nextWarDateTime = dt.addDays(NextWarDay[warType][dt.date().dayOfWeek()]);
        nextWarTime = VSTime(startHour, 0, 0);
        nextWarDateTime.setTime(nextWarTime);

        if (nextWarDateTime < VSDateTime::currentDateTime()) {
            nextWarDateTime = nextWarDateTime.addDays(1);
            nextWarDateTime = nextWarDateTime.addDays(NextWarDay[warType][nextWarDateTime.date().dayOfWeek()]);
        }
    } else {
        nextWarDateTime = dt.addSecs(NextWarHour[warType][dt.time().hour()] * 60 * 60);
        nextWarTime = VSTime(nextWarDateTime.time().hour(), 0, 0);
        nextWarDateTime.setTime(nextWarTime);
    }

    return nextWarDateTime;
}

VSDateTime WarScheduler::getNextWarDateTime(WarType_t warType) const {
    return getNextWarDateTime(warType, getLastWarDateTime(warType));
}

bool WarScheduler::addWar(War* pWar)

{
    WarType_t warType = pWar->getWarType();
    VSDateTime warStartTime = getNextWarDateTime(warType);
    pWar->setWarStartTime(warStartTime);

    Schedule::ScheduleType scheduleType;

    if (warType == WAR_GUILD) {
        scheduleType = Schedule::SCHEDULE_TYPE_ONCE;
    } else // if (warType==WAR_RACE)
    {
        scheduleType = Schedule::SCHEDULE_TYPE_PERIODIC;
    }

    WarSchedule* pWarSchedule = new WarSchedule(pWar, warStartTime, scheduleType);

    __ENTER_CRITICAL_SECTION(m_Mutex)

    addSchedule(pWarSchedule);

    filelog("WarLog.txt", "[%d][WarID=%d] A %s war was requested; adding it to the schedule.",
            (int)m_pZone->getZoneID(), (int)pWar->getWarID(), (pWar->getWarType() == WAR_GUILD ? "guild" : "race"));

    pWarSchedule->create();

    __LEAVE_CRITICAL_SECTION(m_Mutex)


    return true;
}

bool WarScheduler::canAddWar(WarType_t warType)

{
    return getSize() < MaxWarSchedule;
}

void WarScheduler::cancelGuildSchedules()

{
    defaultWarInfoRepository().cancelGuildWarSchedules(de::kernelContext().config().getPropertyInt("ServerID"),
                                                       m_pZone->getZoneID());

    // Load it again.
    load();
}

void WarScheduler::cancelGuildSchedulesOf(GuildID_t gID) {
    vector<WarID_t> attackedWarIDs;
    vector<WarID_t> otherWarIDs;

    __ENTER_CRITICAL_SECTION(m_Mutex)

    const RecentSchedules::container_type& schedules = m_RecentSchedules.getSchedules();
    RecentSchedules::const_iterator itr = schedules.begin();

    for (; itr != schedules.end(); itr++) {
        WarSchedule* pSchedule = dynamic_cast<WarSchedule*>(*itr);
        if (pSchedule == NULL)
            continue;

        // Both castle war classes report WAR_GUILD and answer for their own
        // attackers, so the guild is matched through the war itself. A war the
        // guild attacks goes with the guild; a siege it only reinforces belongs
        // to its challengers and loses the reinforcement alone.
        War* pWar = pSchedule->getWar();
        if (pWar == NULL || pWar->getWarType() != WAR_GUILD || pWar->getState() != War::WAR_STATE_WAIT)
            continue;

        if (pWar->isAttackerGuild(gID))
            attackedWarIDs.push_back(pWar->getWarID());
        else
            otherWarIDs.push_back(pWar->getWarID());
    }

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    if (attackedWarIDs.empty() && otherWarIDs.empty())
        return;

    // The rows are changed before the reload, because the reload builds the
    // schedules back out of them: changing them afterwards would leave the
    // war standing until something reloaded again.
    WarInfoRepository& repository = defaultWarInfoRepository();
    const int serverID = de::kernelContext().config().getPropertyInt("ServerID");

    for (size_t i = 0; i < attackedWarIDs.size(); ++i) {
        if (repository.cancelWarSchedule(attackedWarIDs[i], serverID))
            filelog("WarLog.txt", "[CANCEL][WarID=%d] guild %d attacked in it and no longer exists",
                    (int)attackedWarIDs[i], (int)gID);
    }

    // A reinforcement registration, accepted or still waiting, is denied
    // rather than the siege cancelled; the reload then rebuilds the siege
    // without it. A war the guild had nothing to do with changes no row.
    for (size_t i = 0; i < otherWarIDs.size(); ++i) {
        if (repository.denyReinforceRegistration(otherWarIDs[i], serverID, gID))
            filelog("WarLog.txt", "[CANCEL][WarID=%d] guild %d reinforced it and no longer exists", (int)otherWarIDs[i],
                    (int)gID);
    }

    load();
}

bool WarScheduler::hasSchedule(GuildID_t gID) {
    // The callers are on other threads, so the walk takes the mutex every
    // sibling takes.
    __ENTER_CRITICAL_SECTION(m_Mutex)

    const RecentSchedules::container_type& schedules = m_RecentSchedules.getSchedules();
    RecentSchedules::const_iterator itr = schedules.begin();

    for (; itr != schedules.end(); itr++) {
        WarSchedule* pSchedule = dynamic_cast<WarSchedule*>(*itr);
        if (pSchedule == NULL)
            continue;

        // Both castle war classes report WAR_GUILD and answer for their own
        // participants, so no cast to one of them is made here.
        War* pWar = dynamic_cast<War*>(pSchedule->getWork());
        if (pWar != NULL && pWar->getWarType() == WAR_GUILD && pWar->isWarParticipant(gID) &&
            pWar->getState() == War::WAR_STATE_WAIT) {
            return true;
        }
    }

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    return false;
}
