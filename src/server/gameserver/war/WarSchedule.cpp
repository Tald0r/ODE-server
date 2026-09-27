#include "WarSchedule.h"

#include <stdio.h>

#include "CastleInfoManager.h"
#include "DB.h"
#include "GCWarList.h"
#include "GCWarScheduleList.h"
#include "GuildManager.h"
#include "KernelContext.h"
#include "Properties.h"
#include "War.h"
#include "Zone.h"
#include "repository/WarInfoRepository.h"

WarSchedule::WarSchedule(Work* pWork, const VSDateTime& Time, ScheduleType type // = SCHEDULE_TYPE_ONCE
                         )

    : Schedule(pWork, Time, type){__BEGIN_TRY __END_CATCH}

      WarSchedule::~WarSchedule()

{}

void WarSchedule::makeWarScheduleInfo(WarScheduleInfo* pWSI) const

{
    __BEGIN_TRY

    Assert(m_pWork != NULL);

    War* pWar = dynamic_cast<War*>(m_pWork);
    Assert(pWar != NULL);

    pWar->makeWarScheduleInfo(pWSI);
    pWSI->year = m_ScheduledTime.date().year();
    pWSI->month = m_ScheduledTime.date().month();
    pWSI->day = m_ScheduledTime.date().day();
    pWSI->hour = m_ScheduledTime.time().hour();

    __END_CATCH
}

void WarSchedule::makeWarInfo(WarInfo* pWarInfo) const

{
    __BEGIN_TRY

    Assert(pWarInfo != NULL);

    const Work* pWork = getWork();
    Assert(pWork != NULL);

    const War* pWar = dynamic_cast<const War*>(pWork);
    Assert(pWar != NULL);

    //---------------------------------------------------
    // Get the remaining war time.. this should be pulled out separately...
    //---------------------------------------------------
    VSDateTime dt(VSDateTime::currentDateTime());
    int endHour = m_ScheduledTime.time().hour();
    int endMin = m_ScheduledTime.time().minute();
    int endSec = m_ScheduledTime.time().second();
    int curHour = dt.time().hour();
    int curMin = dt.time().minute();
    int curSec = dt.time().second();
    int endSecs = endHour * 60 * 60 + endMin * 60 + endSec;
    int curSecs = curHour * 60 * 60 + curMin * 60 + curSec;

    int remainSec = 0;
    if (endSecs > curSecs)
        remainSec = endSecs - curSecs;

    DWORD startTime = ((DWORD)((DWORD)(m_ScheduledTime.date().year() - 2000)) * 1000000) +
                      ((DWORD)((DWORD)m_ScheduledTime.date().month()) * 10000) +
                      ((DWORD)((DWORD)m_ScheduledTime.date().day()) * 100) +
                      ((DWORD)((DWORD)m_ScheduledTime.time().hour()));


    //---------------------------------------------------
    // Set the WarInfo values
    //---------------------------------------------------
    pWar->makeWarInfo(pWarInfo);
    pWarInfo->setRemainTime(remainSec);
    pWarInfo->setStartTime(startTime);


    __END_CATCH
}

void WarSchedule::create()

{
    __BEGIN_TRY

    War* pWar = dynamic_cast<War*>(m_pWork);
    Assert(pWar != NULL);

    if (pWar->getWarType() != WAR_GUILD)
        return;

    // Both castle war classes report WAR_GUILD, so the row is built from the
    // war's own castle, attacker and fee rather than from a cast to one of
    // them, and carries the kind that tells the two apart on a reload.
    if (!defaultWarInfoRepository().insertWarSchedule(
            (int)pWar->getWarID(), de::kernelContext().config().getPropertyInt("ServerID"),
            (int)pWar->getCastleZoneID(), pWar->getWarType2DBString(), (int)pWar->getAttackerGuildID(),
            (int)pWar->getRegistrationFee(), m_ScheduledTime.toDateTime(), pWar->getState2DBString(),
            pWar->getCastleWarKind2DBString())) {
        filelog("WarError.log", "WarSchedule::create() : War info is already in the table, or the table is invalid.");
        return;
    }

    __END_CATCH
}

void WarSchedule::save()

{
    __BEGIN_TRY

    War* pWar = dynamic_cast<War*>(m_pWork);
    Assert(pWar != NULL);

    if (pWar->getWarType() != WAR_GUILD)
        return;

    // Either castle war class: the row is built from the war's own castle,
    // attackers, fee and kind, so a guild war rewrites exactly the row
    // create() wrote for it -- one attacker in the first slot -- and a siege
    // writes every challenger that has joined it.
    if (!defaultWarInfoRepository().replaceWarSchedule(
            (int)pWar->getWarID(), de::kernelContext().config().getPropertyInt("ServerID"),
            (int)pWar->getCastleZoneID(), pWar->getWarType2DBString(), (int)pWar->getAttackerCount(),
            (int)pWar->getAttackerGuildIDAt(0), (int)pWar->getAttackerGuildIDAt(1), (int)pWar->getAttackerGuildIDAt(2),
            (int)pWar->getAttackerGuildIDAt(3), (int)pWar->getAttackerGuildIDAt(4), (int)pWar->getRegistrationFee(),
            m_ScheduledTime.toDateTime(), pWar->getState2DBString(), pWar->getCastleWarKind2DBString())) {
        filelog("WarError.log", "WarSchedule::save() : the war's row could not be written.");
        return;
    }

    __END_CATCH
}

void WarSchedule::tinysave(const string& query)

{
    __BEGIN_TRY

    War* pWar = dynamic_cast<War*>(m_pWork);
    Assert(pWar != NULL);

    defaultWarInfoRepository().tinysaveWarSchedule(query, pWar->getWarID(),
                                                   de::kernelContext().config().getPropertyInt("ServerID"));


    __END_CATCH
}

void WarSchedule::run()

{
    __BEGIN_TRY

    Schedule::run();

    if (m_pWork != NULL) {
        War* pWar = dynamic_cast<War*>(m_pWork);
        Assert(pWar != NULL);

        char pState[20];
        snprintf(pState, sizeof(pState), "Status='%s'", pWar->getState2DBString().c_str());
        tinysave(string(pState));
    }

    __END_CATCH
}
