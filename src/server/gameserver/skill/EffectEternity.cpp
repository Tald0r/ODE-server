//////////////////////////////////////////////////////////////////////////////
// Filename    : EffectEternity.cpp
// Written by  : elca
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "EffectEternity.h"

#include "CreatureUtil.h"
#include "GCAddEffectToTile.h"
#include "GCModifyInformation.h"
#include "GCRemoveEffect.h"
#include "GCStatusCurrentHP.h"
#include "Player.h"
#include "Slayer.h"

EffectEternity::EffectEternity(Creature* pCreature)

{
    setTarget(pCreature);
    m_TargetObjectID = pCreature->getObjectID();

    m_pZone = pCreature->getZone();
}

void EffectEternity::affect(Creature* pCreature)

{}

void EffectEternity::unaffect()

{
    Zone* pZone = m_pZone;
    Assert(pZone != NULL);

    Creature* pCreature = pZone->getCreature(m_TargetObjectID);
    if (pCreature == NULL)
        return;
    unaffect(pCreature);
}

void EffectEternity::unaffect(Creature* pCreature)

{
    __BEGIN_DEBUG

    Assert(pCreature != NULL);
    Assert(pCreature->isSlayer());

    Slayer* pSlayer = dynamic_cast<Slayer*>(pCreature);
    Assert(pSlayer != NULL);

    Zone* pZone = pSlayer->getZone();
    Assert(pZone != NULL);

    if (!pSlayer->isFlag(Effect::EFFECT_CLASS_COMA))
        return;

    // Attach the effect to the tile.
    GCAddEffectToTile gcAddEffect;
    gcAddEffect.setEffectID(Effect::EFFECT_CLASS_ETERNITY);
    gcAddEffect.setXY(pSlayer->getX(), pSlayer->getY());
    gcAddEffect.setDuration(31);
    pZone->broadcastPacket(pSlayer->getX(), pSlayer->getY(), &gcAddEffect);

    pSlayer->deleteEffect(Effect::EFFECT_CLASS_COMA);
    pSlayer->removeFlag(Effect::EFFECT_CLASS_COMA);

    HP_t NewHP = getPercentValue(pSlayer->getHP(ATTR_MAX), 100 + m_HPPenalty);
    NewHP = min(NewHP, pSlayer->getHP(ATTR_MAX));
    NewHP = max((HP_t)1, NewHP);

    pSlayer->setHP(NewHP);

    // Announce that the coma effect is gone.
    GCRemoveEffect gcRemoveEffect;
    gcRemoveEffect.setObjectID(pSlayer->getObjectID());
    gcRemoveEffect.addEffectList((EffectID_t)Effect::EFFECT_CLASS_COMA);
    pZone->broadcastPacket(pSlayer->getX(), pSlayer->getY(), &gcRemoveEffect);

    GCStatusCurrentHP gcHP;
    gcHP.setObjectID(pSlayer->getObjectID());
    gcHP.setCurrentHP(pSlayer->getHP());
    pZone->broadcastPacket(pSlayer->getX(), pSlayer->getY(), &gcHP);

    // Send the effects again.
    pSlayer->getEffectManager()->sendEffectInfo(pSlayer, pZone, pSlayer->getX(), pSlayer->getY());

    addSimpleCreatureEffect(pSlayer, Effect::EFFECT_CLASS_NO_DAMAGE, 30, false);
    addSimpleCreatureEffect(pSlayer, Effect::EFFECT_CLASS_ETERNITY_PAUSE, 30);

    __END_DEBUG
}

string EffectEternity::toString() const {
    StringStream msg;
    msg << "EffectEternity(" << "ObjectID:" << getObjectID() << ")";
    return msg.toString();
}
