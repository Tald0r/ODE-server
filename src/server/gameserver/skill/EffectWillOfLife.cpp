//////////////////////////////////////////////////////////////////////////////
// Filename    : EffectWillOfLife.cpp
// Written by  : elca
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "EffectWillOfLife.h"

#include "Creature.h"
#include "GCModifyInformation.h"
#include "GCRemoveEffect.h"
#include "GCStatusCurrentHP.h"
#include "Monster.h"
#include "Player.h"
#include "Vampire.h"

EffectWillOfLife::EffectWillOfLife(Creature* pCreature)

{
    setTarget(pCreature);
}

void EffectWillOfLife::affect()

{
    if (m_pTarget != NULL && m_pTarget->getObjectClass() == Object::OBJECT_CLASS_CREATURE) {
        affect(dynamic_cast<Creature*>(m_pTarget));
    }
}

void EffectWillOfLife::affect(Creature* pCreature)

{
    if (pCreature == NULL)
        return;

    if (!pCreature->isVampire())
        return;

    Vampire* pVampire = dynamic_cast<Vampire*>(pCreature);
    Assert(pVampire != NULL);

    HP_t curHP = pVampire->getHP(ATTR_CURRENT);
    HP_t maxHP = pVampire->getHP(ATTR_MAX) - pVampire->getSilverDamage();

    HP_t nextHP = curHP + getBonus();

    if (maxHP - getBonus() <= curHP)
        nextHP = maxHP;

    pVampire->setHP(nextHP);

    setNextTime(10);
}

void EffectWillOfLife::unaffect()

{
    Creature* pCreature = dynamic_cast<Creature*>(m_pTarget);
    unaffect(pCreature);
}

void EffectWillOfLife::unaffect(Creature* pCreature)

{
    __BEGIN_DEBUG


    Assert(pCreature != NULL);
    Assert(pCreature->isVampire());

    pCreature->removeFlag(Effect::EFFECT_CLASS_WILL_OF_LIFE);

    Vampire* pVampire = dynamic_cast<Vampire*>(pCreature);

    // Tells clients that the effect is gone.
    GCRemoveEffect gcRemoveEffect;
    gcRemoveEffect.setObjectID(pVampire->getObjectID());
    gcRemoveEffect.addEffectList(Effect::EFFECT_CLASS_WILL_OF_LIFE);
    pVampire->getZone()->broadcastPacket(pVampire->getX(), pVampire->getY(), &gcRemoveEffect);

    GCStatusCurrentHP gcHP;
    gcHP.setObjectID(pVampire->getObjectID());
    gcHP.setCurrentHP(pVampire->getHP(ATTR_CURRENT));
    pVampire->getPlayer()->sendPacket(&gcHP);


    __END_DEBUG
}

string EffectWillOfLife::toString() const {
    StringStream msg;
    msg << "EffectWillOfLife(" << "ObjectID:" << getObjectID() << ")";
    return msg.toString();
}
