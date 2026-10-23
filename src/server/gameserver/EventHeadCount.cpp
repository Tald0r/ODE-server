//////////////////////////////////////////////////////////////////////////////
// Filename    : EventHeadCount.cpp
// Written by  : Reiot
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "EventHeadCount.h"

#include "CreatureUtil.h"
#include "GamePlayer.h"
#include "Ousters.h"
#include "Slayer.h"
#include "Vampire.h"
#include "Zone.h"
#include "repository/PlayRecordRepository.h"

//////////////////////////////////////////////////////////////////////////////
// class EventHeadCount member methods
//////////////////////////////////////////////////////////////////////////////

EventHeadCount::EventHeadCount(GamePlayer* pGamePlayer)

    : Event(pGamePlayer) {
    Creature* pCreature = m_pGamePlayer->getCreature();
    if (pCreature == NULL || !pCreature->isPC())
        return;

    PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pCreature);
    Assert(pPC != NULL);
    m_LastLevel = getPCLevel(pPC);

    m_Count = 0;
}

EventHeadCount::~EventHeadCount()

{}

void EventHeadCount::activate()

{
    __BEGIN_DEBUG

    Assert(m_pGamePlayer != NULL);

    Creature* pCreature = m_pGamePlayer->getCreature();
    if (pCreature == NULL || !pCreature->isPC()) {
        setDeadline(18000);
        return;
    }

    PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pCreature);
    Assert(pPC != NULL);

    Level_t level = getPCLevel(pPC);

    if (level < 50 && (level > 35 || level < 30)) {
        setDeadline(18000);
        return;
    }

    defaultPlayRecordRepository().insertHeadCount(pPC->getName(), m_LastLevel, level, m_Count);

    setDeadline(18000);
    m_LastLevel = level;
    m_Count = 0;

    __END_DEBUG
}

string EventHeadCount::toString() const

{
    StringStream msg;
    msg << "EventHeadCount(" << m_Count << ")";
    return msg.toString();
}
