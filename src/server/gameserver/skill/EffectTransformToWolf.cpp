//////////////////////////////////////////////////////////////////////////////
// Filename    : EffectTransformToWolf.cpp
// Written by  : elca
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "EffectTransformToWolf.h"

#include "Creature.h"
#include "GCAddMonsterFromTransformation.h"
#include "GCAddVampireFromTransformation.h"
#include "GCDeleteObject.h"
#include "GCModifyInformation.h"
#include "GCStatusCurrentHP.h"
#include "Monster.h"
#include "Player.h"
#include "Slayer.h"
#include "Vampire.h"
#include "ZoneUtil.h"

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
EffectTransformToWolf::EffectTransformToWolf(Creature* pCreature)

{
    setTarget(pCreature);
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectTransformToWolf::affect(Creature* pCreature)

{}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectTransformToWolf::affect(Zone* pZone, ZoneCoord_t x, ZoneCoord_t y, Object* pObject)

{}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectTransformToWolf::unaffect()

{
    Creature* pCreature = dynamic_cast<Creature*>(m_pTarget);
    Assert(pCreature != NULL);

    if (pCreature->isSlayer()) {
        throw Error("EffectTransfromToWolf::unaffect() : Slayer cannot transfrom to wolf!");
    }

    unaffect(pCreature);
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectTransformToWolf::unaffect(Creature* pCreature)

{
    Assert(pCreature != NULL);

    Zone* pZone = pCreature->getZone();
    Assert(pZone != NULL);

    // Restores the original appearance and removes the flag.
    addUntransformCreature(pZone, pCreature, false);
    pCreature->removeFlag(Effect::EFFECT_CLASS_TRANSFORM_TO_WOLF);

    // Restores the stats to their original values.
    if (pCreature->isVampire()) {
        Vampire* pTargetVampire = dynamic_cast<Vampire*>(pCreature);
        VAMPIRE_RECORD prev;

        pTargetVampire->getVampireRecord(prev);
        pTargetVampire->initAllStat();
        pTargetVampire->sendRealWearingInfo();
        pTargetVampire->sendModifyInfo(prev);
    } else if (pCreature->isMonster()) {
        Monster* pMonster = dynamic_cast<Monster*>(pCreature);
        pMonster->initAllStat();
    } else {
        Assert(false);
    }
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectTransformToWolf::unaffect(Zone* pZone, ZoneCoord_t x, ZoneCoord_t y, Object* pObject)

{}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
string EffectTransformToWolf::toString() const {
    StringStream msg;
    msg << "EffectTransformToWolf(" << "ObjectID:" << getObjectID() << ")";
    return msg.toString();
}
