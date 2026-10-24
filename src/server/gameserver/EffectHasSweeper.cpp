
#include "EffectHasSweeper.h"

#include "Creature.h"
#include "GCRemoveEffect.h"
#include "Zone.h"

EffectHasSweeper::EffectHasSweeper(Creature* pCreature) {
    setTarget(pCreature);
    m_Part = 0;
}

void EffectHasSweeper::unaffect() {
    Creature* pCreature = dynamic_cast<Creature*>(m_pTarget);
    Assert(pCreature != NULL);

    pCreature->removeFlag(getEffectClass());

    GCRemoveEffect gcRE;
    gcRE.setObjectID(pCreature->getObjectID());
    gcRE.addEffectList(getSendEffectClass());

    pCreature->getZone()->broadcastPacket(pCreature->getX(), pCreature->getY(), &gcRE);
}

string EffectHasSweeper::toString() const {
    return "EffectHasSweeper";
}
