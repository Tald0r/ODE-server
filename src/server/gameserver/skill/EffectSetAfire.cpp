//////////////////////////////////////////////////////////////////////////////
// Filename    : EffectSetAfire.cpp
// Written by  : elca
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "EffectSetAfire.h"

#include "GCModifyInformation.h"
#include "GCRemoveEffect.h"
#include "GCStatusCurrentHP.h"
#include "Player.h"
#include "SkillUtil.h"
#include "Slayer.h"
#include "ZoneUtil.h"

EffectSetAfire::EffectSetAfire(Creature* pCreature)

{
    setTarget(pCreature);
    m_bBroadcastingEffect = false;
}

void EffectSetAfire::affect() {
    if (m_pTarget == NULL || m_pTarget->getObjectClass() != OBJECT_CLASS_CREATURE) {
        setDeadline(0);
        return;
    }
    affect(dynamic_cast<Creature*>(m_pTarget));
}

void EffectSetAfire::affect(Creature* pCreature)

{
    Assert(pCreature != NULL);
    if (pCreature->isDead()) {
        setDeadline(0);
        return;
    }

    Creature* pAttacker = pCreature->getZone()->getCreature(m_CasterOID);
    Damage_t damage = m_Damage / m_TotalTimes;
    if (m_Times == m_TotalTimes)
        damage += m_Damage % m_TotalTimes;
    if (!canAttack(pAttacker, pCreature))
        return;

    GCModifyInformation gcMI, gcAttackerMI;

    ::setDamage(pCreature, damage, pAttacker, SKILL_SET_AFIRE, &gcMI, &gcAttackerMI);

    if (pCreature->isDead() && pAttacker != NULL && pAttacker->isVampire()) {
        Vampire* pVampire = dynamic_cast<Vampire*>(pAttacker);
        Exp_t exp = computeCreatureExp(pCreature, KILL_EXP);
        shareVampExp(pVampire, exp, gcAttackerMI);
        increaseAlignment(pAttacker, pCreature, gcAttackerMI);
        setDeadline(0);
    }

    if (pAttacker != NULL && pAttacker->isPC()) {
        computeAlignmentChange(pCreature, damage, pAttacker, &gcMI, &gcAttackerMI);
        pAttacker->getPlayer()->sendPacket(&gcAttackerMI);
    }

    if (pCreature->isPC()) {
        pCreature->getPlayer()->sendPacket(&gcMI);
    }

    --m_Times;

    if (m_Times == 0) {
        setDeadline(0);
        if (pAttacker != NULL) {
            knockbackCreature(pCreature->getZone(), pCreature, pAttacker->getX(), pAttacker->getY());
            knockbackCreature(pCreature->getZone(), pCreature, pAttacker->getX(), pAttacker->getY());
            knockbackCreature(pCreature->getZone(), pCreature, pAttacker->getX(), pAttacker->getY());
        }
    } else
        setNextTime(m_Tick);
}

void EffectSetAfire::unaffect()

{
    Creature* pCreature = dynamic_cast<Creature*>(m_pTarget);
    unaffect(pCreature);
}

void EffectSetAfire::unaffect(Creature* pCreature)

{
    __BEGIN_DEBUG


    Assert(pCreature != NULL);

    Zone* pZone = pCreature->getZone();
    Assert(pZone != NULL);


    __END_DEBUG
}

string EffectSetAfire::toString() const {
    StringStream msg;
    msg << "EffectSetAfire(" << "ObjectID:" << getObjectID() << ")";
    return msg.toString();
}
