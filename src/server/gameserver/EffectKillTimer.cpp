//////////////////////////////////////////////////////////////////////////////
// Filename    : EffectKillTimer.cpp
// Written by  : elca
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "EffectKillTimer.h"

#include "Creature.h"
#include "Monster.h"
#include "Zone.h"

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
EffectKillTimer::EffectKillTimer(Creature* pCreature, bool kill)

{
    // Server-only Effect.
    m_bBroadcastingEffect = false;

    setTarget(pCreature);
    m_bKill = kill;
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
EffectKillTimer::~EffectKillTimer()

{}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectKillTimer::unaffect()

{
    // cout << "EffectKillTimer" << "unaffect BEGIN" << endl;

    Creature* pCreature = dynamic_cast<Creature*>(m_pTarget);

    if (pCreature == NULL)
        return;
    if (pCreature->isMonster()) {
        Monster* pMonster = dynamic_cast<Monster*>(pCreature);
        if (m_bKill) {
            pMonster->setHP(0);
        } else {
            pMonster->getZone()->deleteCreature(pMonster, pMonster->getX(), pMonster->getY());
        }
    }

    // cout << "EffectKillTimer" << "unaffect END" << endl;
}

string EffectKillTimer::toString() const {
    StringStream msg;

    msg << "EffectKillTimer(" << "ObjectID:" << getObjectID() << ")";

    return msg.toString();
}
