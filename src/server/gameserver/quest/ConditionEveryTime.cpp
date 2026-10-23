////////////////////////////////////////////////////////////////////////////////
// Filename    : ConditionEveryTime.cpp
// Written By  :
// Description :
////////////////////////////////////////////////////////////////////////////////

#include "ConditionEveryTime.h"

////////////////////////////////////////////////////////////////////////////////
// is satisfied?
////////////////////////////////////////////////////////////////////////////////
bool ConditionEveryTime::isSatisfied(Creature* pCreature1, Creature* pCreature2, void* pParam) const

{
    Timeval currentTime;

    getCurrentTime(currentTime);

    bool expired = pCreature1 != NULL && pCreature2 == NULL && currentTime > m_NextTurn;

    ////////////////////////////////////////////////////////////////////////////////
    // When the time has expired, push the next run time m_Turn further on.
    ////////////////////////////////////////////////////////////////////////////////
    if (expired) {
        Timeval delay;
        delay.tv_sec = m_Turn / 10;
        delay.tv_usec = (m_Turn % 10) * 100000;

        m_NextTurn = m_NextTurn + delay;
    }

    return expired;
}

////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////
void ConditionEveryTime::read(PropertyBuffer& propertyBuffer)

{
    try {
        // read turn
        m_Turn = propertyBuffer.getPropertyInt("Turn");
    } catch (NoSuchElementException& nsee) {
        throw Error(nsee.toString());
    }

    // set next turn (current time + m_Turn)
    Timeval delay;
    delay.tv_sec = m_Turn / 10;
    delay.tv_usec = (m_Turn % 10) * 100000;

    getCurrentTime(m_NextTurn);
    m_NextTurn = m_NextTurn + delay;
}

////////////////////////////////////////////////////////////////////////////////
// get debug string
////////////////////////////////////////////////////////////////////////////////
string ConditionEveryTime::toString() const

{
    StringStream msg;
    msg << "ConditionEveryTime(" << "Turn:" << (int)m_Turn << ",NextTurn:" << (int)m_NextTurn.tv_sec << "."
        << (int)m_NextTurn.tv_usec << ")";
    return msg.toString();
}
