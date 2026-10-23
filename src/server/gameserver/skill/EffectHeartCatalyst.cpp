//////////////////////////////////////////////////////////////////////////////
// Filename    : EffectHeartCatalyst.cpp
// Written by  : elca
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "EffectHeartCatalyst.h"

#include "GCRemoveEffect.h"
#include "GCStatusCurrentHP.h"
#include "Item.h"
#include "ItemUtil.h"
#include "Player.h"
#include "Slayer.h"

EffectHeartCatalyst::EffectHeartCatalyst(Creature* pCreature)

{
    setTarget(pCreature);
    // The accessor flips this on every call, so the value has no meaning.
    m_GiveExp = true;
}

void EffectHeartCatalyst::affect() {
    Creature* pCreature = dynamic_cast<Creature*>(m_pTarget);
    affect(pCreature);
}

void EffectHeartCatalyst::affect(Creature* pCreature)

{
    Assert(pCreature != NULL);

    if (!pCreature->isSlayer()) {
        setDeadline(0);
        return;
    }

    Slayer* pSlayer = dynamic_cast<Slayer*>(pCreature);
    Assert(pSlayer != NULL);

    Item* pWeapon = pSlayer->getWearItem(Slayer::WEAR_RIGHTHAND);
    if (pWeapon == NULL || !isArmsWeapon(pWeapon)) {
        setDeadline(0);
        return;
    }

    if (pSlayer->getHP(ATTR_CURRENT) >= pSlayer->getHP(ATTR_MAX)) {
        setNextTime(m_Tick);
        return;
    }

    HP_t CurrentHP = pSlayer->getHP(ATTR_CURRENT);
    HP_t NewHP = min((int)pSlayer->getHP(ATTR_MAX), CurrentHP + m_HealPoint);

    pSlayer->setHP(NewHP);
    GCStatusCurrentHP gcHP;
    gcHP.setObjectID(pSlayer->getObjectID());
    gcHP.setCurrentHP(pSlayer->getHP());

    pSlayer->getZone()->broadcastPacket(pSlayer->getX(), pSlayer->getY(), &gcHP);

    setNextTime(m_Tick);
}

void EffectHeartCatalyst::unaffect()

{
    Creature* pCreature = dynamic_cast<Creature*>(m_pTarget);
    unaffect(pCreature);
}

void EffectHeartCatalyst::unaffect(Creature* pCreature)

{
    __BEGIN_DEBUG


    Assert(pCreature != NULL);
    Assert(pCreature->isSlayer());

    pCreature->removeFlag(Effect::EFFECT_CLASS_HEART_CATALYST);

    Slayer* pSlayer = dynamic_cast<Slayer*>(pCreature);

    SLAYER_RECORD prev;
    pSlayer->getSlayerRecord(prev);
    pSlayer->initAllStat();
    pSlayer->sendRealWearingInfo();
    pSlayer->sendModifyInfo(prev);

    Zone* pZone = pSlayer->getZone();
    Assert(pZone != NULL);

    // Tells clients that the effect is gone.
    GCRemoveEffect gcRemoveEffect;
    gcRemoveEffect.setObjectID(pSlayer->getObjectID());
    gcRemoveEffect.addEffectList(Effect::EFFECT_CLASS_HEART_CATALYST);
    pZone->broadcastPacket(pSlayer->getX(), pSlayer->getY(), &gcRemoveEffect);


    __END_DEBUG
}

string EffectHeartCatalyst::toString() const {
    StringStream msg;
    msg << "EffectHeartCatalyst(" << "ObjectID:" << getObjectID() << ")";
    return msg.toString();
}
