//////////////////////////////////////////////////////////////////////////////
// Filename		: EffectExplosionWater.cpp
// Written By	:
// Description	:
//////////////////////////////////////////////////////////////////////////////

#include "EffectExplosionWater.h"

#include "DB.h"
#include "GCRemoveEffect.h"
#include "SkillHandler.h"

EffectExplosionWater::EffectExplosionWater(Creature* pCreature)

{
    setTarget(pCreature);
}

void EffectExplosionWater::affect()

{
    Creature* pCreature = dynamic_cast<Creature*>(m_pTarget);
    affect(pCreature);
}

void EffectExplosionWater::affect(Creature* pCreature)

{}

void EffectExplosionWater::unaffect(Creature* pCreature)

{
    Assert(pCreature != NULL);

    pCreature->removeFlag(Effect::EFFECT_CLASS_EXPLOSION_WATER);

    Zone* pZone = pCreature->getZone();
    Assert(pZone != NULL);

    GCRemoveEffect gcRemoveEffect;
    gcRemoveEffect.setObjectID(pCreature->getObjectID());
    gcRemoveEffect.addEffectList(Effect::EFFECT_CLASS_EXPLOSION_WATER);
    pZone->broadcastPacket(pCreature->getX(), pCreature->getY(), &gcRemoveEffect, pCreature);

    if (pCreature->isPC()) {
        Player* pPlayer = pCreature->getPlayer();
        Assert(pPlayer != NULL);

        pPlayer->sendPacket(&gcRemoveEffect);
    }
}

void EffectExplosionWater::unaffect()

{
    Creature* pCreature = dynamic_cast<Creature*>(m_pTarget);
    unaffect(pCreature);
}

string EffectExplosionWater::toString() const {
    StringStream msg;
    msg << "EffectExplosionWater(" << "ObjectID:" << getObjectID() << ")";
    return msg.toString();
}
