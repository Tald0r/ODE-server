//////////////////////////////////////////////////////////////////////////////
// Filename		: EffectSleep.cpp
// Written By	:
// Description	:
//////////////////////////////////////////////////////////////////////////////

#include "EffectSleep.h"

#include "DB.h"
#include "GCRemoveEffect.h"
#include "Monster.h"
#include "SkillHandler.h"
#include "Slayer.h"
#include "Vampire.h"

EffectSleep::EffectSleep(Creature* pCreature)

{
    setTarget(pCreature);
}

void EffectSleep::affect()

{
    Creature* pCreature = dynamic_cast<Creature*>(m_pTarget);
    affect(pCreature);
}

void EffectSleep::affect(Creature* pCreature)

{}

void EffectSleep::unaffect(Creature* pCreature)

{
    Assert(pCreature != NULL);

    pCreature->removeFlag(Effect::EFFECT_CLASS_SLEEP);

    Zone* pZone = pCreature->getZone();
    Assert(pZone != NULL);

    GCRemoveEffect gcRemoveEffect;
    gcRemoveEffect.setObjectID(pCreature->getObjectID());
    gcRemoveEffect.addEffectList(Effect::EFFECT_CLASS_SLEEP);
    pZone->broadcastPacket(pCreature->getX(), pCreature->getY(), &gcRemoveEffect, pCreature);

    if (pCreature->isPC()) {
        Player* pPlayer = pCreature->getPlayer();
        Assert(pPlayer != NULL);

        pPlayer->sendPacket(&gcRemoveEffect);
    }
}

void EffectSleep::unaffect()

{
    Creature* pCreature = dynamic_cast<Creature*>(m_pTarget);
    unaffect(pCreature);
}

string EffectSleep::toString() const {
    StringStream msg;
    msg << "EffectSleep(" << "ObjectID:" << getObjectID() << ")";
    return msg.toString();
}
