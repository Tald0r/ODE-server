//////////////////////////////////////////////////////////////////////////////
// Filename    : EffectLoud.cpp
// Written by  : elca
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "EffectLoud.h"

#include "GCRemoveEffect.h"
#include "Ousters.h"

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
EffectLoud::EffectLoud(Creature* pCreature)

{
    Assert(pCreature != NULL);
    // Assert(pCreature->isOusters());

    setTarget(pCreature);
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectLoud::unaffect(Creature* pCreature)

{
    // cout << "EffectLoud" << "unaffect BEGIN" << endl;

    Assert(pCreature != NULL);
    //	Assert(pCreature->isOusters());

    // Clear the flag.
    pCreature->removeFlag(Effect::EFFECT_CLASS_LOUD);

    Zone* pZone = pCreature->getZone();
    Assert(pZone != NULL);

    //	Ousters* pTargetOusters = dynamic_cast<Ousters*>(pCreature);
    //	Assert( pTargetOusters != NULL );

    // Notify that the effect should be removed.
    GCRemoveEffect gcRemoveEffect;
    gcRemoveEffect.setObjectID(pCreature->getObjectID());
    gcRemoveEffect.addEffectList(Effect::EFFECT_CLASS_LOUD);
    pZone->broadcastPacket(pCreature->getX(), pCreature->getY(), &gcRemoveEffect);

    // cout << "EffectLoud" << "unaffect END" << endl;
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectLoud::unaffect()

{
    Creature* pCreature = dynamic_cast<Creature*>(m_pTarget);
    unaffect(pCreature);
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
string EffectLoud::toString() const {
    StringStream msg;
    msg << "EffectLoud(" << ")";
    return msg.toString();
}
