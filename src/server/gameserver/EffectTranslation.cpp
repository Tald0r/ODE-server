
//////////////////////////////////////////////////////////////////////////////
// Filename    : EffectTranslation.cpp
// Written by  : elca
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "EffectTranslation.h"

#include "Creature.h"
#include "GCRemoveEffect.h"
#include "Player.h"
#include "Zone.h"

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
EffectTranslation::EffectTranslation(Creature* pCreature)

{
    setTarget(pCreature);
    setBroadcastingEffect();
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectTranslation::affect(Creature* pCreature)

{}

void EffectTranslation::unaffect()

{
    Creature* pCreature = dynamic_cast<Creature*>(m_pTarget);
    unaffect(pCreature);
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectTranslation::unaffect(Creature* pCreature)

{
    __BEGIN_DEBUG

    // cout << "EffectTranslation" << "unaffect BEGIN" << endl;

    Assert(pCreature != NULL);

    // To restore the stats to normal, the flag must be cleared and
    // initAllStat must be called.
    pCreature->removeFlag(Effect::EFFECT_CLASS_TRANSLATION);

    Zone* pZone = pCreature->getZone();
    Assert(pZone != NULL);

    GCRemoveEffect gcRemoveEffect;
    gcRemoveEffect.setObjectID(pCreature->getObjectID());
    gcRemoveEffect.addEffectList(Effect::EFFECT_CLASS_TRANSLATION);
    pZone->broadcastPacket(pCreature->getX(), pCreature->getY(), &gcRemoveEffect);

    // cout << "EffectTranslation" << "unaffect END" << endl;

    __END_DEBUG
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
string EffectTranslation::toString() const {
    StringStream msg;
    msg << "EffectTranslation(" << "ObjectID:" << getObjectID() << ")";
    return msg.toString();
}
