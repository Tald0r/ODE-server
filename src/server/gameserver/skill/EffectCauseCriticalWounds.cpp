//////////////////////////////////////////////////////////////////////////////
// Filename		: EffectCauseCriticalWounds.cpp
// Written By	:
// Description	:
//////////////////////////////////////////////////////////////////////////////

#include "EffectCauseCriticalWounds.h"

#include "DB.h"
#include "GCRemoveEffect.h"
#include "SkillHandler.h"

EffectCauseCriticalWounds::EffectCauseCriticalWounds(Creature* pCreature)

{
    setTarget(pCreature);
}

void EffectCauseCriticalWounds::affect()

{
    Creature* pCreature = dynamic_cast<Creature*>(m_pTarget);
    affect(pCreature);
}

void EffectCauseCriticalWounds::affect(Creature* pCreature)

{}

void EffectCauseCriticalWounds::unaffect(Creature* pCreature)

{
    Assert(pCreature != NULL);
    Assert(pCreature->isVampire() || pCreature->isOusters() || pCreature->isMonster());

    pCreature->removeFlag(Effect::EFFECT_CLASS_CAUSE_CRITICAL_WOUNDS);

    Zone* pZone = pCreature->getZone();
    Assert(pZone != NULL);

    GCRemoveEffect gcRemoveEffect;
    gcRemoveEffect.setObjectID(pCreature->getObjectID());
    gcRemoveEffect.addEffectList(Effect::EFFECT_CLASS_CAUSE_CRITICAL_WOUNDS);
    pZone->broadcastPacket(pCreature->getX(), pCreature->getY(), &gcRemoveEffect, pCreature);

    if (pCreature->isPC()) {
        Player* pPlayer = pCreature->getPlayer();
        Assert(pPlayer != NULL);

        pPlayer->sendPacket(&gcRemoveEffect);
    }
}

void EffectCauseCriticalWounds::unaffect()

{
    Creature* pCreature = dynamic_cast<Creature*>(m_pTarget);
    unaffect(pCreature);
}

string EffectCauseCriticalWounds::toString() const {
    StringStream msg;
    msg << "EffectCauseCriticalWounds(" << "ObjectID:" << getObjectID() << ")";
    return msg.toString();
}
