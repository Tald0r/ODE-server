//////////////////////////////////////////////////////////////////////////////
// Filename    : EffectGrandMasterVampire.cpp
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "EffectGrandMasterVampire.h"

#include "Creature.h"
#include "GCAddEffect.h"
#include "GCRemoveEffect.h"
#include "Player.h"
#include "Vampire.h"
#include "Zone.h"

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
EffectGrandMasterVampire::EffectGrandMasterVampire(Creature* pCreature)

{
    __BEGIN_TRY

    setTarget(pCreature);

    __END_CATCH
}

Effect::EffectClass EffectGrandMasterVampire::getSendEffectClass() const {
    Vampire* pVampire = dynamic_cast<Vampire*>(m_pTarget);
    if (pVampire == NULL)
        return getEffectClass();

    Level_t level = pVampire->getLevel();

    if (level == 150)
        return Effect::EFFECT_CLASS_GRAND_MASTER_VAMPIRE_150;
    else if (level >= 130)
        return Effect::EFFECT_CLASS_GRAND_MASTER_VAMPIRE_130;
    else
        return getEffectClass();
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectGrandMasterVampire::affect()

{
    __BEGIN_TRY

    Creature* pCreature = dynamic_cast<Creature*>(m_pTarget);
    affect(pCreature);

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectGrandMasterVampire::affect(Creature* pCreature)

{
    __BEGIN_TRY

    Assert(pCreature->isVampire());

    pCreature->setFlag(Effect::EFFECT_CLASS_GRAND_MASTER_VAMPIRE);

    // Broadcast it to the surroundings.
    GCAddEffect gcAddEffect;
    gcAddEffect.setObjectID(pCreature->getObjectID());
    gcAddEffect.setEffectID(getSendEffectClass());
    gcAddEffect.setDuration(static_cast<Duration_t>(999999));
    pCreature->getZone()->broadcastPacket(pCreature->getX(), pCreature->getY(), &gcAddEffect);

    __END_CATCH
}

void EffectGrandMasterVampire::unaffect()

{
    __BEGIN_TRY

    Creature* pCreature = dynamic_cast<Creature*>(m_pTarget);
    unaffect(pCreature);

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectGrandMasterVampire::unaffect(Creature* pCreature)

{
    __BEGIN_TRY
    __BEGIN_DEBUG

    // cout << "EffectGrandMasterVampire" << "unaffect BEGIN" << endl;

    Assert(pCreature != NULL);

    // To restore the attributes properly, clear the flag and
    // call initAllStat.
    pCreature->removeFlag(Effect::EFFECT_CLASS_GRAND_MASTER_VAMPIRE);

    Zone* pZone = pCreature->getZone();
    Assert(pZone != NULL);

    GCRemoveEffect gcRemoveEffect;
    gcRemoveEffect.setObjectID(pCreature->getObjectID());
    gcRemoveEffect.addEffectList(Effect::EFFECT_CLASS_GRAND_MASTER_VAMPIRE);
    pZone->broadcastPacket(pCreature->getX(), pCreature->getY(), &gcRemoveEffect);

    // cout << "EffectGrandMasterVampire" << "unaffect END" << endl;

    __END_DEBUG
    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
string EffectGrandMasterVampire::toString() const {
    __BEGIN_TRY

    StringStream msg;
    msg << "EffectGrandMasterVampire(" << "ObjectID:" << getObjectID() << ")";
    return msg.toString();

    __END_CATCH
}
