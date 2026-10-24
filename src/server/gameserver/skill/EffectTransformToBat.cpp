//////////////////////////////////////////////////////////////////////////////
// Filename    : EffectTransformToBat.cpp
// Written by  : elca
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "EffectTransformToBat.h"

#include "Creature.h"
#include "GCAddMonsterFromTransformation.h"
#include "GCAddVampireFromTransformation.h"
#include "GCDeleteObject.h"
#include "GCModifyInformation.h"
#include "Monster.h"
#include "Player.h"
#include "Slayer.h"
#include "Vampire.h"
#include "ZoneUtil.h"

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
EffectTransformToBat::EffectTransformToBat(Creature* pCreature)

{
    setTarget(pCreature);
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectTransformToBat::affect(Creature* pCreature)

{}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectTransformToBat::affect(Zone* pZone, ZoneCoord_t x, ZoneCoord_t y, Object* pObject)

{}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectTransformToBat::unaffect()

{
    Creature* pCreature = dynamic_cast<Creature*>(m_pTarget);
    Assert(pCreature != NULL);

    if (pCreature->isSlayer()) {
        throw Error("EffectTransfromToWolf::unaffect() : Slayer cannot transfrom to bat!");
    }

    unaffect(pCreature);
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectTransformToBat::unaffect(Creature* pCreature)

{
    Assert(pCreature != NULL);

    Zone* pZone = pCreature->getZone();
    Assert(pZone != NULL);

    // Restores the original appearance and removes the flag.
    addUntransformCreature(pZone, pCreature, false);
    pCreature->removeFlag(Effect::EFFECT_CLASS_TRANSFORM_TO_BAT);

    // Restores the stats to their original values.
    if (pCreature->isVampire()) {
        Vampire* pVampire = dynamic_cast<Vampire*>(pCreature);
        VAMPIRE_RECORD prev;

        pVampire->getVampireRecord(prev);
        pVampire->initAllStat();
        pVampire->sendRealWearingInfo();
        pVampire->sendModifyInfo(prev);
    } else if (pCreature->isMonster()) {
        Monster* pMonster = dynamic_cast<Monster*>(pCreature);
        pMonster->initAllStat();
    } else {
        Assert(false);
    }
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectTransformToBat::unaffect(Zone* pZone, ZoneCoord_t x, ZoneCoord_t y, Object* pObject)

{}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
string EffectTransformToBat::toString() const {
    StringStream msg;

    msg << "EffectTransformToBat(" << "ObjectID:" << getObjectID() << ")";

    return msg.toString();
}
