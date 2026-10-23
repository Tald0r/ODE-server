//////////////////////////////////////////////////////////////////////////////
// Filename    : EffectParalyze.cpp
// Written by  : elca
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "EffectParalyze.h"

#include "Creature.h"
#include "DB.h"
#include "GCModifyInformation.h"
#include "GCRemoveEffect.h"
#include "GCStatusCurrentHP.h"
#include "Monster.h"
#include "Player.h"
#include "SkillHandler.h"
#include "Slayer.h"
#include "Vampire.h"

EffectParalyze::EffectParalyze(Creature* pCreature)

{
    setTarget(pCreature);
}

void EffectParalyze::affect()

{
    Creature* pCreature = dynamic_cast<Creature*>(m_pTarget);
    affect(pCreature);
}

void EffectParalyze::affect(Creature* pCreature)

{}

void EffectParalyze::unaffect(Creature* pCreature)

{
    Assert(pCreature != NULL);
    pCreature->removeFlag(Effect::EFFECT_CLASS_PARALYZE);

    Zone* pZone = pCreature->getZone();
    Assert(pZone != NULL);

    GCRemoveEffect gcRemoveEffect;
    gcRemoveEffect.setObjectID(pCreature->getObjectID());
    gcRemoveEffect.addEffectList(Effect::EFFECT_CLASS_PARALYZE);
    pZone->broadcastPacket(pCreature->getX(), pCreature->getY(), &gcRemoveEffect);
}

void EffectParalyze::unaffect()

{
    Creature* pCreature = dynamic_cast<Creature*>(m_pTarget);
    unaffect(pCreature);
}

string EffectParalyze::toString() const {
    StringStream msg;
    msg << "EffectParalyze(" << "ObjectID:" << getObjectID() << ")";
    return msg.toString();
}
