//////////////////////////////////////////////////////////////////////////////
// Filename    : EffectReputoFactum.cpp
// Written by  : elca
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "EffectReputoFactum.h"

#include "Creature.h"
#include "GCRemoveEffect.h"
#include "Monster.h"
#include "Player.h"
#include "Zone.h"

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
EffectReputoFactum::EffectReputoFactum(Creature* pCreature)

{
    setTarget(pCreature);
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectReputoFactum::affect(Creature* pCreature)

{}

void EffectReputoFactum::unaffect()

{
    Creature* pCreature = dynamic_cast<Creature*>(m_pTarget);
    unaffect(pCreature);
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectReputoFactum::unaffect(Creature* pCreature)

{
    __BEGIN_DEBUG


    Assert(pCreature != NULL);

    // Restoring the stats requires clearing the flag and
    // calling initAllStat.
    pCreature->removeFlag(Effect::EFFECT_CLASS_REPUTO_FACTUM);
    Zone* pZone = pCreature->getZone();
    Assert(pZone != NULL);

    GCRemoveEffect gcRemoveEffect;
    gcRemoveEffect.setObjectID(pCreature->getObjectID());
    gcRemoveEffect.addEffectList(Effect::EFFECT_CLASS_REPUTO_FACTUM_LAST);
    pZone->broadcastPacket(pCreature->getX(), pCreature->getY(), &gcRemoveEffect);


    __END_DEBUG
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
string EffectReputoFactum::toString() const {
    StringStream msg;
    msg << "EffectReputoFactum(" << "ObjectID:" << getObjectID() << ")";
    return msg.toString();
}
