//////////////////////////////////////////////////////////////////////////////
// Filename    : SimpleCreatureEffect.cpp
// Written by  : elca
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "SimpleCreatureEffect.h"

#include "GCRemoveEffect.h"
#include "Ousters.h"
#include "Player.h"

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
SimpleCreatureEffect::SimpleCreatureEffect(EffectClass eClass, Creature* pCreature) : m_EffectClass(eClass) {
    Assert(pCreature != NULL);

    setTarget(pCreature);
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void SimpleCreatureEffect::unaffect(Creature* pCreature)

{
    Assert(pCreature != NULL);

    // Clear the flag.
    pCreature->removeFlag(getEffectClass());

    Zone* pZone = pCreature->getZone();
    Assert(pZone != NULL);

    // Notify that the effect should be removed.
    GCRemoveEffect gcRemoveEffect;
    gcRemoveEffect.setObjectID(pCreature->getObjectID());
    gcRemoveEffect.addEffectList(getSendEffectClass());

    if (isBroadcastingEffect()) {
        pZone->broadcastPacket(pCreature->getX(), pCreature->getY(), &gcRemoveEffect);
    } else {
        Player* pPlayer = pCreature->getPlayer();
        if (pPlayer != NULL)
            pPlayer->sendPacket(&gcRemoveEffect);
    }
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void SimpleCreatureEffect::unaffect()

{
    Creature* pCreature = dynamic_cast<Creature*>(m_pTarget);
    unaffect(pCreature);
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
string SimpleCreatureEffect::toString() const {
    StringStream msg;
    msg << "SimpleCreatureEffect(" << "EffectClass:" << (int)m_EffectClass << ")";
    return msg.toString();
}
