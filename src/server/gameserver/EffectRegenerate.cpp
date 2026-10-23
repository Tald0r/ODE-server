//////////////////////////////////////////////////////////////////////////////
// Filename    : EffectRegenerate.cpp
// Written by  : elca
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "EffectRegenerate.h"

#include "Creature.h"
#include "GCStatusCurrentHP.h"
#include "Monster.h"
#include "Zone.h"

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
EffectRegenerate::EffectRegenerate(Creature* pCreature)

{
    // Server-only Effect.
    m_bBroadcastingEffect = false;

    setTarget(pCreature);
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
EffectRegenerate::~EffectRegenerate()

{}

void EffectRegenerate::affect() {
    setNextTime(m_Tick);

    Monster* pMonster = dynamic_cast<Monster*>(m_pTarget);
    if (pMonster == NULL)
        return;

    HP_t regenPoint = min((int)m_Point, pMonster->getHP(ATTR_MAX) - pMonster->getHP(ATTR_CURRENT));

    if (regenPoint != 0) {
        pMonster->setHP(pMonster->getHP() + regenPoint);
        GCStatusCurrentHP gcHP;
        gcHP.setObjectID(pMonster->getObjectID());
        gcHP.setCurrentHP(pMonster->getHP());
        pMonster->getZone()->broadcastPacket(pMonster->getX(), pMonster->getY(), &gcHP);

        cout << pMonster->getName() << "'s HP became " << pMonster->getHP() << "." << endl;
    }
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectRegenerate::unaffect()

{
    // cout << "EffectRegenerate" << "unaffect BEGIN" << endl;

    // cout << "EffectRegenerate" << "unaffect END" << endl;
}

string EffectRegenerate::toString() const {
    StringStream msg;

    msg << "EffectRegenerate(" << "ObjectID:" << getObjectID() << ")";

    return msg.toString();
}
