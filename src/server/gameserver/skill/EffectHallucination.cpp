//////////////////////////////////////////////////////////////////////////////
// Filename    : EffectHallucination.cpp
// Written by  : elca
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "EffectHallucination.h"

#include "Creature.h"
#include "GCModifyInformation.h"
#include "GCRemoveEffect.h"
#include "GCStatusCurrentHP.h"
#include "Monster.h"
#include "Player.h"
#include "Slayer.h"
#include "Vampire.h"

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
EffectHallucination::EffectHallucination(Creature* pCreature)

{
    setTarget(pCreature);
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectHallucination::affect(Creature* pCreature)

{}

void EffectHallucination::unaffect()

{
    Creature* pCreature = dynamic_cast<Creature*>(m_pTarget);
    unaffect(pCreature);
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectHallucination::unaffect(Creature* pCreature)

{
    __BEGIN_DEBUG


    Assert(pCreature != NULL);

    // Restoring the stats requires clearing the flag and
    // calling initAllStat.
    pCreature->removeFlag(Effect::EFFECT_CLASS_HALLUCINATION);

    Zone* pZone = pCreature->getZone();
    Assert(pZone != NULL);

    GCRemoveEffect gcRemoveEffect;
    gcRemoveEffect.setObjectID(pCreature->getObjectID());
    gcRemoveEffect.addEffectList(Effect::EFFECT_CLASS_HALLUCINATION);
    pZone->broadcastPacket(pCreature->getX(), pCreature->getY(), &gcRemoveEffect);

    if (pCreature->isMonster()) {
        Monster* pMonster = dynamic_cast<Monster*>(pCreature);
        Assert(pMonster != NULL);

        pMonster->deleteAllEnemy();
        pZone->monsterScan(pMonster, pMonster->getX(), pMonster->getY(), pMonster->getDir());
    }


    __END_DEBUG
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
string EffectHallucination::toString() const {
    StringStream msg;
    msg << "EffectHallucination(" << "ObjectID:" << getObjectID() << ")";
    return msg.toString();
}
