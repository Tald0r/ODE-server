//////////////////////////////////////////////////////////////////////////////
// Filename    : EffectShrineShield.cpp
// Written by  : elca
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "EffectShrineShield.h"

#include "Creature.h"
#include "GCAddEffect.h"
#include "GCModifyInformation.h"
#include "GCRemoveEffect.h"
#include "GCStatusCurrentHP.h"
#include "GCSystemMessage.h"
#include "Monster.h"
#include "MonsterCorpse.h"
#include "Player.h"
#include "Slayer.h"
#include "Vampire.h"
#include "ZoneGroupManager.h"

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
EffectShrineShield::EffectShrineShield(Creature* pCreature)

{
    m_ShrineID = 0;
    setTarget(pCreature);
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
EffectShrineShield::EffectShrineShield(Item* pItem)

{
    setTarget(pItem);
}
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectShrineShield::affect(Creature* pCreature)

{}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectShrineShield::affect(Item* pItem)

{}


void EffectShrineShield::unaffect()

{
    switch (m_pTarget->getObjectClass()) {
    case OBJECT_CLASS_CREATURE: {
        Creature* pCreature = dynamic_cast<Creature*>(m_pTarget);
        unaffect(pCreature);
    } break;

    case OBJECT_CLASS_ITEM: {
        Item* pItem = dynamic_cast<Item*>(m_pTarget);
        unaffect(pItem);
    } break;

    default:
        throw Error("Wrong Object Class");
    }
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectShrineShield::unaffect(Creature* pCreature)

{
    __BEGIN_DEBUG

    // cout << "EffectShrineShield" << "unaffect BEGIN" << endl;

    Assert(pCreature != NULL);

    // To restore the stats to normal, the flag must be cleared and
    // initAllStat must be called.
    pCreature->removeFlag(Effect::EFFECT_CLASS_SHRINE_SHIELD);

    Zone* pZone = pCreature->getZone();
    Assert(pZone != NULL);

    GCRemoveEffect gcRemoveEffect;
    gcRemoveEffect.setObjectID(pCreature->getObjectID());
    gcRemoveEffect.addEffectList(Effect::EFFECT_CLASS_SHRINE_SHIELD);
    pZone->broadcastPacket(pCreature->getX(), pCreature->getY(), &gcRemoveEffect);

    // cout << "EffectShrineShield" << "unaffect END" << endl;

    __END_DEBUG
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectShrineShield::unaffect(Item* pItem)

{
    __BEGIN_DEBUG

    // cout << "EffectShrineShield" << "unaffect BEGIN" << endl;

    Assert(pItem != NULL);

    Assert(pItem->getItemClass() == Item::ITEM_CLASS_CORPSE);
    Assert(pItem->getItemType() == MONSTER_CORPSE);

    MonsterCorpse* pCorpse = dynamic_cast<MonsterCorpse*>(pItem);

    pCorpse->removeFlag(Effect::EFFECT_CLASS_SHRINE_SHIELD);

    Zone* pZone = pCorpse->getZone();
    Assert(pZone != NULL);

    GCRemoveEffect gcRemoveEffect;
    gcRemoveEffect.setObjectID(pItem->getObjectID());
    gcRemoveEffect.addEffectList(Effect::EFFECT_CLASS_SHRINE_SHIELD);
    pZone->broadcastPacket(pCorpse->getX(), pCorpse->getY(), &gcRemoveEffect);

    // cout << "EffectShrineShield" << "unaffect END" << endl;

    __END_DEBUG
}


//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
string EffectShrineShield::toString() const {
    StringStream msg;
    msg << "EffectShrineShield(" << "ObjectID:" << getObjectID() << ")";
    return msg.toString();
}
