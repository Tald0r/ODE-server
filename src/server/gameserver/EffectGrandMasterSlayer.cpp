//////////////////////////////////////////////////////////////////////////////
// Filename    : EffectGrandMasterSlayer.cpp
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "EffectGrandMasterSlayer.h"

#include "Creature.h"
#include "GCAddEffect.h"
#include "GCRemoveEffect.h"
#include "Player.h"
#include "Slayer.h"
#include "Zone.h"

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
EffectGrandMasterSlayer::EffectGrandMasterSlayer(Creature* pCreature)

{
    __BEGIN_TRY

    setTarget(pCreature);

    __END_CATCH
}

Effect::EffectClass EffectGrandMasterSlayer::getSendEffectClass() const {
    Slayer* pSlayer = dynamic_cast<Slayer*>(m_pTarget);
    if (pSlayer == NULL)
        return getEffectClass();

    SkillLevel_t level = pSlayer->getHighestSkillDomainLevel();

    if (level == 150)
        return Effect::EFFECT_CLASS_GRAND_MASTER_SLAYER_150;
    else if (level >= 130)
        return Effect::EFFECT_CLASS_GRAND_MASTER_SLAYER_130;
    else
        return getEffectClass();
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectGrandMasterSlayer::affect()

{
    __BEGIN_TRY

    Creature* pCreature = dynamic_cast<Creature*>(m_pTarget);
    affect(pCreature);

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectGrandMasterSlayer::affect(Creature* pCreature)

{
    __BEGIN_TRY

    Assert(pCreature->isSlayer());

    pCreature->setFlag(Effect::EFFECT_CLASS_GRAND_MASTER_SLAYER);

    // Broadcast it to the surroundings.
    GCAddEffect gcAddEffect;
    gcAddEffect.setObjectID(pCreature->getObjectID());
    gcAddEffect.setEffectID(getSendEffectClass());
    gcAddEffect.setDuration(static_cast<Duration_t>(999999));
    pCreature->getZone()->broadcastPacket(pCreature->getX(), pCreature->getY(), &gcAddEffect);

    __END_CATCH
}

void EffectGrandMasterSlayer::unaffect()

{
    __BEGIN_TRY

    Creature* pCreature = dynamic_cast<Creature*>(m_pTarget);
    unaffect(pCreature);

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectGrandMasterSlayer::unaffect(Creature* pCreature)

{
    __BEGIN_TRY
    __BEGIN_DEBUG

    // cout << "EffectGrandMasterSlayer" << "unaffect BEGIN" << endl;

    Assert(pCreature != NULL);

    // To restore the attributes properly, clear the flag and
    // call initAllStat.
    pCreature->removeFlag(Effect::EFFECT_CLASS_GRAND_MASTER_SLAYER);

    Zone* pZone = pCreature->getZone();
    Assert(pZone != NULL);

    GCRemoveEffect gcRemoveEffect;
    gcRemoveEffect.setObjectID(pCreature->getObjectID());
    gcRemoveEffect.addEffectList(Effect::EFFECT_CLASS_GRAND_MASTER_SLAYER);
    pZone->broadcastPacket(pCreature->getX(), pCreature->getY(), &gcRemoveEffect);

    // cout << "EffectGrandMasterSlayer" << "unaffect END" << endl;

    __END_DEBUG
    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
string EffectGrandMasterSlayer::toString() const {
    __BEGIN_TRY

    StringStream msg;
    msg << "EffectGrandMasterSlayer(" << "ObjectID:" << getObjectID() << ")";
    return msg.toString();

    __END_CATCH
}
