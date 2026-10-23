//////////////////////////////////////////////////////////////////////////////
// Filename    : Effect.cpp
// Written by  : excel96
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "Effect.h"

//////////////////////////////////////////////////////////////////////////////
// constructor
//////////////////////////////////////////////////////////////////////////////
Effect::Effect()

{
    m_pZone = NULL;
    m_X = 0;
    m_Y = 0;
    m_pTarget = NULL;
    m_bBroadcastingEffect = true;

    setNextTime(99999999);
    setDeadline(99999999);
}

//////////////////////////////////////////////////////////////////////////////
// constructor
//////////////////////////////////////////////////////////////////////////////
Effect::Effect(Zone* pZone, ZoneCoord_t x, ZoneCoord_t y, Object* pTarget, Turn_t delay)

{
    m_pZone = pZone;
    m_X = x;
    m_Y = y;
    m_pTarget = pTarget;

    setNextTime(99999999);
    setDeadline(delay);
}

//////////////////////////////////////////////////////////////////////////////
// destructor
//////////////////////////////////////////////////////////////////////////////
Effect::~Effect()

{}

void Effect::setNextTime(Turn_t delay)

{
    // Read the current time.
    getCurrentTime(m_NextTime);

    // delay is counted in units of 0.1 second.
    m_NextTime.tv_sec += delay / 10;
    m_NextTime.tv_usec += (delay % 10) * 100000;
}

void Effect::setDeadline(Turn_t delay) {
    // Read the current time.
    getCurrentTime(m_Deadline);

    // Work out the deadline.
    // delay is counted in units of 0.1 second.
    m_Deadline.tv_sec += delay / 10;
    m_Deadline.tv_usec += (delay % 10) * 100000;
}

// Remaining duration, in units of 1/10 second
Duration_t Effect::getRemainDuration() {
    Timeval currentTime;
    getCurrentTime(currentTime);
    return (m_Deadline.tv_sec - currentTime.tv_sec) * 10 + (m_Deadline.tv_usec - currentTime.tv_usec) / 100000;
}
