//////////////////////////////////////////////////////////////////////////////
// Filename    : Event.cpp
// Written by  : excel96
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "Event.h"

#include "Assert.h"

//////////////////////////////////////////////////////////////////////////////
// class Event member methods
//////////////////////////////////////////////////////////////////////////////

Event::Event(GamePlayer* pGamePlayer)

{
    m_pGamePlayer = pGamePlayer;

    // The assert is gone because EventReloadInfo may pass NULL.
    // EventReloadInfo need not have been an Event (it has nothing to do with
    // GamePlayer), but EventManager was the only thing available at the time.
    // by sigi. 2002.9.26
    //	Assert(m_pGamePlayer != NULL);

    m_Deadline.tv_sec = 0;
    m_Deadline.tv_usec = 0;
}

Event::~Event()

{}

void Event::setDeadline(Turn_t delay)

{
    // Read the current time.
    getCurrentTime(m_Deadline);

    // Work out the deadline.
    // delay is counted in units of 0.1 second.
    m_Deadline.tv_sec += delay / 10;
    m_Deadline.tv_usec += (delay % 10) * 100000;
}
