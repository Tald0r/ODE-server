//////////////////////////////////////////////////////////////////////////////
// Filename    : EffectBurningSolCharging.cpp
// Written by  : Sequoia
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "EffectBurningSolCharging.h"

#include "GCAddEffect.h"
#include "GCRemoveEffect.h"
#include "Monster.h"
#include "Slayer.h"
#include "Vampire.h"

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
EffectBurningSolCharging::EffectBurningSolCharging(Creature* pCreature)

{
    Assert(pCreature != NULL);
    Assert(pCreature->isSlayer());

    setTarget(pCreature);
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectBurningSolCharging::affect()

{
    Creature* pCreature = dynamic_cast<Creature*>(m_pTarget);
    affect(pCreature);
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectBurningSolCharging::affect(Creature* pCreature)

{
    static Turn_t delay[] = {10, 10, 300};
    if (m_Level >= 3)
        return;

    setNextTime(delay[m_Level]);


    // Tells clients to remove the effect.
    GCRemoveEffect gcRemoveEffect;
    gcRemoveEffect.setObjectID(pCreature->getObjectID());
    gcRemoveEffect.addEffectList(getSendEffectClass());
    pCreature->getZone()->broadcastPacket(pCreature->getX(), pCreature->getY(), &gcRemoveEffect);

    m_Level++;

    GCAddEffect gcAddEffect;
    gcAddEffect.setObjectID(pCreature->getObjectID());
    gcAddEffect.setEffectID(getSendEffectClass());
    gcAddEffect.setDuration(getRemainDuration());
    pCreature->getZone()->broadcastPacket(pCreature->getX(), pCreature->getY(), &gcAddEffect);
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectBurningSolCharging::unaffect()

{
    Creature* pCreature = dynamic_cast<Creature*>(m_pTarget);
    unaffect(pCreature);
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectBurningSolCharging::unaffect(Creature* pCreature)

{
    Assert(pCreature != NULL);

    // Turns off the flag.
    pCreature->removeFlag(Effect::EFFECT_CLASS_BURNING_SOL_CHARGE_1);

    Zone* pZone = pCreature->getZone();
    Assert(pZone != NULL);

    // Tells clients to remove the effect.
    GCRemoveEffect gcRemoveEffect;
    gcRemoveEffect.setObjectID(pCreature->getObjectID());
    gcRemoveEffect.addEffectList(getSendEffectClass());
    pZone->broadcastPacket(pCreature->getX(), pCreature->getY(), &gcRemoveEffect);
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
string EffectBurningSolCharging::toString() const {
    StringStream msg;
    msg << "EffectBurningSolCharging(" << "ObjectID:" << getObjectID() << ",Level:" << m_Level << ")";
    return msg.toString();
}
