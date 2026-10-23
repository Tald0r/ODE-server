//////////////////////////////////////////////////////////////////////////////
// Filename    : EffectShrineGuard.cpp
// Written by  : elca
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "EffectShrineGuard.h"

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
EffectShrineGuard::EffectShrineGuard(Creature* pCreature)

{
    m_ShrineID = 0;
    setTarget(pCreature);
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
EffectShrineGuard::EffectShrineGuard(Item* pItem)

{
    setTarget(pItem);
}
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectShrineGuard::affect(Creature* pCreature)

{}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectShrineGuard::affect(Item* pItem)

{}


void EffectShrineGuard::unaffect()

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
void EffectShrineGuard::unaffect(Creature* pCreature)

{
    __BEGIN_DEBUG

    // cout << "EffectShrineGuard" << "unaffect BEGIN" << endl;

    Assert(pCreature != NULL);

    // To restore the attributes properly, clear the flag and
    // call initAllStat.
    pCreature->removeFlag(Effect::EFFECT_CLASS_SHRINE_GUARD);

    Zone* pZone = pCreature->getZone();
    Assert(pZone != NULL);

    GCRemoveEffect gcRemoveEffect;
    gcRemoveEffect.setObjectID(pCreature->getObjectID());
    gcRemoveEffect.addEffectList(Effect::EFFECT_CLASS_SHRINE_GUARD);
    pZone->broadcastPacket(pCreature->getX(), pCreature->getY(), &gcRemoveEffect);

    // cout << "EffectShrineGuard" << "unaffect END" << endl;

    __END_DEBUG
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectShrineGuard::unaffect(Item* pItem)

{
    __BEGIN_DEBUG

    // cout << "EffectShrineGuard" << "unaffect BEGIN" << endl;

    Assert(pItem != NULL);

    Assert(pItem->getItemClass() == Item::ITEM_CLASS_CORPSE);
    Assert(pItem->getItemType() == MONSTER_CORPSE);

    MonsterCorpse* pCorpse = dynamic_cast<MonsterCorpse*>(pItem);

    pCorpse->removeFlag(Effect::EFFECT_CLASS_SHRINE_GUARD);

    Zone* pZone = pCorpse->getZone();
    Assert(pZone != NULL);

    GCRemoveEffect gcRemoveEffect;
    gcRemoveEffect.setObjectID(pItem->getObjectID());
    gcRemoveEffect.addEffectList(Effect::EFFECT_CLASS_SHRINE_GUARD);
    pZone->broadcastPacket(pCorpse->getX(), pCorpse->getY(), &gcRemoveEffect);

    // cout << "EffectShrineGuard" << "unaffect END" << endl;

    __END_DEBUG
}


//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
string EffectShrineGuard::toString() const {
    StringStream msg;
    msg << "EffectShrineGuard(" << "ObjectID:" << getObjectID() << ")";
    return msg.toString();
}
