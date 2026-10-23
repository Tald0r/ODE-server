///////////////////////////////////////////////////////////////////
// Implementation of the Schedule class for scheduled work
///////////////////////////////////////////////////////////////////

#include "Schedule.h"

#include "Exception.h"
#include "StringStream.h"
#include "Work.h"

Schedule::Schedule(Work* pWork, const VSDateTime& Time, ScheduleType type)

    : m_ScheduleType(type), m_pWork(pWork), m_ScheduledTime(Time) {}

Schedule::~Schedule() noexcept {
    try {
        SAFE_DELETE(m_pWork);
    } catch (...) {
        // destructor must not throw
    }
}

Work* Schedule::popWork() {
    Work* pWork = m_pWork;

    m_pWork = NULL;

    return pWork;
}

bool Schedule::heartbeat()

{
    if (isDue()) {
        run();
        return true;
    }

    return false;
}

bool Schedule::isDue() const {
    VSDateTime current = VSDateTime(VSDate::currentDate(), VSTime::currentTime());

    return current >= m_ScheduledTime;
}

void Schedule::run() {
    filelog("Schedule.txt", "Execute(%s >= %s) : %s", VSDateTime::currentDateTime().toString().c_str(),
            m_ScheduledTime.toString().c_str(), m_pWork->toString().c_str());

    m_pWork->execute();
}

string Schedule::toString() const

{
    StringStream msg;

    msg << "Schedule(" << "ScheduleType:" << (int)m_ScheduleType << ",ScheduledTime:" << m_ScheduledTime.toString();

    if (m_pWork == NULL)
        msg << ",Work:NULL";
    else
        msg << ",Work:" << m_pWork->toString();

    msg << ")";

    return msg.toString();
}
