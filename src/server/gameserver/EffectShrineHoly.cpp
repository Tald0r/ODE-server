//////////////////////////////////////////////////////////////////////////////
// Filename    : EffectShrineHoly.cpp
// Written by  : elca
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "EffectShrineHoly.h"

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
EffectShrineHoly::EffectShrineHoly(Creature* pCreature)

{
    m_ShrineID = 0;

    setTarget(pCreature);
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
EffectShrineHoly::EffectShrineHoly(Item* pItem)

{
    setTarget(pItem);
}
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectShrineHoly::affect(Creature* pCreature)

{}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectShrineHoly::affect(Item* pItem)

{}


void EffectShrineHoly::unaffect()

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
void EffectShrineHoly::unaffect(Creature* pCreature)

{
    __BEGIN_DEBUG

    // cout << "EffectShrineHoly" << "unaffect BEGIN" << endl;

    Assert(pCreature != NULL);

    // To restore the attributes properly, clear the flag and
    // call initAllStat.
    pCreature->removeFlag(Effect::EFFECT_CLASS_SHRINE_HOLY);

    Zone* pZone = pCreature->getZone();
    Assert(pZone != NULL);

    GCRemoveEffect gcRemoveEffect;
    gcRemoveEffect.setObjectID(pCreature->getObjectID());
    gcRemoveEffect.addEffectList(Effect::EFFECT_CLASS_SHRINE_HOLY);
    pZone->broadcastPacket(pCreature->getX(), pCreature->getY(), &gcRemoveEffect);

    // cout << "EffectShrineHoly" << "unaffect END" << endl;

    __END_DEBUG
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectShrineHoly::unaffect(Item* pItem)

{
    __BEGIN_DEBUG

    // cout << "EffectShrineHoly" << "unaffect BEGIN" << endl;

    Assert(pItem != NULL);

    Assert(pItem->getItemClass() == Item::ITEM_CLASS_CORPSE);
    Assert(pItem->getItemType() == MONSTER_CORPSE);

    MonsterCorpse* pCorpse = dynamic_cast<MonsterCorpse*>(pItem);

    pCorpse->removeFlag(Effect::EFFECT_CLASS_SHRINE_HOLY);

    Zone* pZone = pCorpse->getZone();
    Assert(pZone != NULL);

    GCRemoveEffect gcRemoveEffect;
    gcRemoveEffect.setObjectID(pItem->getObjectID());
    gcRemoveEffect.addEffectList(Effect::EFFECT_CLASS_SHRINE_HOLY);
    pZone->broadcastPacket(pCorpse->getX(), pCorpse->getY(), &gcRemoveEffect);

    // cout << "EffectShrineHoly" << "unaffect END" << endl;

    __END_DEBUG
}


//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
string EffectShrineHoly::toString() const {
    StringStream msg;
    msg << "EffectShrineHoly(" << "ObjectID:" << getObjectID() << ")";
    return msg.toString();
}
