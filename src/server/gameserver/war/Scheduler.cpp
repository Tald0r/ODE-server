///////////////////////////////////////////////////////////////////
// Implementation of the Scheduler class for scheduled work
///////////////////////////////////////////////////////////////////

#include "Scheduler.h"

#include "Assert.h"

Scheduler::Scheduler()

{}
Scheduler::~Scheduler()

{
    clear();
}

void Scheduler::clear()

{
    while (!m_RecentSchedules.empty()) {
        Schedule* pSchedule = m_RecentSchedules.top();
        m_RecentSchedules.pop();

        SAFE_DELETE(pSchedule);
    }
}

//--------------------------------------------------------------------------------
//
// addSchedule( Schedule* )
//
//--------------------------------------------------------------------------------
// A Schedule is registered in RecentSchedules and Schedules at the same time.
//--------------------------------------------------------------------------------
void Scheduler::addSchedule(Schedule* pSchedule)

{
    m_RecentSchedules.push(pSchedule);
    pSchedule->m_pScheduler = this;
}

//--------------------------------------------------------------------------------
//
// popRecentWork( Schedule* pSchedule )
//
//--------------------------------------------------------------------------------
// Removes it from m_RecentSchedules and m_Schedules
// Returns pRecentSchedule's Work and deletes pRecentSchedule
//--------------------------------------------------------------------------------
Work* Scheduler::popRecentWork()

{
    Schedule* pRecentSchedule = m_RecentSchedules.top();

    m_RecentSchedules.pop();

    Work* pWork = pRecentSchedule->popWork();
    SAFE_DELETE(pRecentSchedule);

    return pWork;
}

Schedule* Scheduler::popDueSchedule() {
    if (m_RecentSchedules.empty() || !m_RecentSchedules.top()->isDue())
        return NULL;

    Schedule* pSchedule = m_RecentSchedules.top();
    m_RecentSchedules.pop();

    return pSchedule;
}

//--------------------------------------------------------------------------------
//
// Work* heartbeat()
//
//--------------------------------------------------------------------------------
// Checks the Schedule that can run soonest and, if it ran,
// returns that Schedule's Work. The Schedule is deleted then.
//--------------------------------------------------------------------------------
Work* Scheduler::heartbeat()

{
    if (m_RecentSchedules.empty())
        return NULL;

    // priority queue's top() returns the smallest element (by the Former above).
    // Sadly there is no telling now whether the earliest or the latest comes out.
    // 2003. 1.23. by Sequoia
    // Swapping the Former class for the Latter class makes the earliest come out now.
    Schedule* pRecentSchedule = m_RecentSchedules.top();

    if (pRecentSchedule->heartbeat()) {
        return popRecentWork();
    }

    return NULL;
}
