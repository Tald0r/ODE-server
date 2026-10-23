//////////////////////////////////////////////////////////////////////////////
// Filename    : EffectHellFireToEnemy.cpp
// Written by  : elca
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "EffectHellFireToEnemy.h"

#include "GCModifyInformation.h"
#include "GCRemoveEffect.h"
#include "Ousters.h"
#include "Player.h"
#include "SkillUtil.h"

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
EffectHellFireToEnemy::EffectHellFireToEnemy(Creature* pCreature)

{
    Assert(pCreature != NULL);

    setTarget(pCreature);
}

void EffectHellFireToEnemy::affect()

{
    if (m_pTarget == NULL || m_pTarget->getObjectClass() != OBJECT_CLASS_CREATURE) {
        setDeadline(0);
        return;
    }

    affect(dynamic_cast<Creature*>(m_pTarget));
}

void EffectHellFireToEnemy::affect(Creature* pCreature)

{
    Assert(pCreature != NULL);

    if (pCreature->isDead()) {
        setDeadline(0);
        return;
    }

    Creature* pAttacker = pCreature->getZone()->getCreature(m_CasterOID);
    Damage_t damage = m_Damage;

    GCModifyInformation gcMI, gcAttackerMI;

    ::setDamage(pCreature, damage, pAttacker, SKILL_HELLFIRE, &gcMI, &gcAttackerMI, true, false);

    if (pCreature->isDead() && pAttacker != NULL && pAttacker->isOusters()) {
        Ousters* pOusters = dynamic_cast<Ousters*>(pAttacker);
        Exp_t exp = computeCreatureExp(pCreature, 70, pOusters);
        shareOustersExp(pOusters, exp, gcAttackerMI);
        increaseAlignment(pAttacker, pCreature, gcAttackerMI);
    }

    if (pAttacker != NULL && pAttacker->isPC()) {
        computeAlignmentChange(pCreature, damage, pAttacker, &gcMI, &gcAttackerMI);
        pAttacker->getPlayer()->sendPacket(&gcAttackerMI);
    }

    if (pCreature->isPC()) {
        pCreature->getPlayer()->sendPacket(&gcMI);
    }

    setNextTime(10);
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectHellFireToEnemy::unaffect(Creature* pCreature)

{
    Assert(pCreature != NULL);

    // Turns off the flag.
    pCreature->removeFlag(Effect::EFFECT_CLASS_HELLFIRE_TO_ENEMY);

    Zone* pZone = pCreature->getZone();
    Assert(pZone != NULL);


    // Tells clients to remove the effect.
    GCRemoveEffect gcRemoveEffect;
    gcRemoveEffect.setObjectID(pCreature->getObjectID());
    gcRemoveEffect.addEffectList(Effect::EFFECT_CLASS_HELLFIRE_TO_ENEMY);
    pZone->broadcastPacket(pCreature->getX(), pCreature->getY(), &gcRemoveEffect);
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectHellFireToEnemy::unaffect()

{
    Creature* pCreature = dynamic_cast<Creature*>(m_pTarget);
    unaffect(pCreature);
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
string EffectHellFireToEnemy::toString() const {
    StringStream msg;
    msg << "EffectHellFireToEnemy(" << "ObjectID:" << getObjectID() << ")";
    return msg.toString();
}
