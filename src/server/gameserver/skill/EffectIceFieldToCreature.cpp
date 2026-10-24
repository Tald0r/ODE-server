//////////////////////////////////////////////////////////////////////////////
// Filename    : EffectIceFieldToCreature.cpp
// Written by  : elca
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "EffectIceFieldToCreature.h"

#include "GCRemoveEffect.h"
#include "Ousters.h"

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
EffectIceFieldToCreature::EffectIceFieldToCreature(Creature* pCreature, bool bFrozenArmor)

{
    Assert(pCreature != NULL);

    setTarget(pCreature);
    m_bFrozenArmor = bFrozenArmor;
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectIceFieldToCreature::unaffect(Creature* pCreature)

{
    Assert(pCreature != NULL);

    // Turns off the flag.
    pCreature->removeFlag(Effect::EFFECT_CLASS_ICE_FIELD_TO_CREATURE);

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
void EffectIceFieldToCreature::unaffect()

{
    Creature* pCreature = dynamic_cast<Creature*>(m_pTarget);
    unaffect(pCreature);
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
string EffectIceFieldToCreature::toString() const {
    StringStream msg;
    msg << "EffectIceFieldToCreature(" << "ObjectID:" << getObjectID() << ")";
    return msg.toString();
}
