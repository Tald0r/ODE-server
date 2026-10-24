//////////////////////////////////////////////////////////////////////////////
// Filename    : EffectGrandMasterOusters.cpp
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "EffectGrandMasterOusters.h"

#include "Creature.h"
#include "GCAddEffect.h"
#include "GCRemoveEffect.h"
#include "Ousters.h"
#include "Player.h"
#include "Zone.h"

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
EffectGrandMasterOusters::EffectGrandMasterOusters(Creature* pCreature)

{
    setTarget(pCreature);
}

Effect::EffectClass EffectGrandMasterOusters::getSendEffectClass() const {
    Ousters* pOusters = dynamic_cast<Ousters*>(m_pTarget);
    if (pOusters == NULL)
        return getEffectClass();

    Level_t level = pOusters->getLevel();

    if (level == 150)
        return Effect::EFFECT_CLASS_GRAND_MASTER_OUSTERS_150;
    else if (level >= 130)
        return Effect::EFFECT_CLASS_GRAND_MASTER_OUSTERS_130;
    else
        return getEffectClass();
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectGrandMasterOusters::affect()

{
    Creature* pCreature = dynamic_cast<Creature*>(m_pTarget);
    affect(pCreature);
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectGrandMasterOusters::affect(Creature* pCreature)

{
    Assert(pCreature->isOusters());

    pCreature->setFlag(Effect::EFFECT_CLASS_GRAND_MASTER_OUSTERS);

    // Broadcast to the surroundings.
    GCAddEffect gcAddEffect;
    gcAddEffect.setObjectID(pCreature->getObjectID());
    gcAddEffect.setEffectID(getSendEffectClass());
    gcAddEffect.setDuration(static_cast<Duration_t>(999999));
    pCreature->getZone()->broadcastPacket(pCreature->getX(), pCreature->getY(), &gcAddEffect);
}

void EffectGrandMasterOusters::unaffect()

{
    Creature* pCreature = dynamic_cast<Creature*>(m_pTarget);
    unaffect(pCreature);
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectGrandMasterOusters::unaffect(Creature* pCreature)

{
    __BEGIN_DEBUG

    Assert(pCreature != NULL);

    pCreature->removeFlag(Effect::EFFECT_CLASS_GRAND_MASTER_OUSTERS);

    Zone* pZone = pCreature->getZone();
    Assert(pZone != NULL);

    GCRemoveEffect gcRemoveEffect;
    gcRemoveEffect.setObjectID(pCreature->getObjectID());
    gcRemoveEffect.addEffectList(Effect::EFFECT_CLASS_GRAND_MASTER_OUSTERS);
    pZone->broadcastPacket(pCreature->getX(), pCreature->getY(), &gcRemoveEffect);

    __END_DEBUG
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
string EffectGrandMasterOusters::toString() const {
    StringStream msg;
    msg << "EffectGrandMasterOusters(" << "ObjectID:" << getObjectID() << ")";
    return msg.toString();
}
