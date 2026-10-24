#include "EffectTrying.h"

#include "Creature.h"
#include "GCDeleteEffectFromTile.h"
#include "GCRemoveEffect.h"
#include "Zone.h"

EffectTrying::EffectTrying(Creature* pCreature) {
    setTarget(pCreature);
}

void EffectTrying::unaffect() {
    Creature* pCreature = dynamic_cast<Creature*>(m_pTarget);
    Assert(pCreature != NULL);

    GCRemoveEffect gcRemoveEffect;
    gcRemoveEffect.addEffectList(getSendEffectClass());
    gcRemoveEffect.setObjectID(pCreature->getObjectID());

    pCreature->getZone()->broadcastPacket(pCreature->getX(), pCreature->getY(), &gcRemoveEffect);
}
