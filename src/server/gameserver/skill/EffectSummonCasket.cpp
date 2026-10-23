//////////////////////////////////////////////////////////////////////////////
// Filename    : EffectSummonCasket.cpp
// Written by  : elca
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "EffectSummonCasket.h"

#include "Creature.h"
#include "GCRemoveEffect.h"
#include "Monster.h"
#include "Player.h"
#include "Vampire.h"
#include "ZoneUtil.h"

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
EffectSummonCasket::EffectSummonCasket(Creature* pCreature)

{
    setTarget(pCreature);
    m_Type = 0;
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectSummonCasket::affect(Creature* pCreature)

{}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectSummonCasket::affect(Zone* pZone, ZoneCoord_t x, ZoneCoord_t y, Object* pObject)

{}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectSummonCasket::unaffect()

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
void EffectSummonCasket::unaffect(Creature* pCreature)

{
    Assert(pCreature != NULL);

    Zone* pZone = pCreature->getZone();
    Assert(pZone != NULL);

    // Restores the original appearance and removes the flag.
    pCreature->removeFlag(Effect::EFFECT_CLASS_CASKET);

    // Restores the stats to their original values.
    if (pCreature->isVampire()) {
        Vampire* pTargetVampire = dynamic_cast<Vampire*>(pCreature);
        VAMPIRE_RECORD prev;

        pTargetVampire->getVampireRecord(prev);
        pTargetVampire->initAllStat();
        pTargetVampire->sendRealWearingInfo();
        pTargetVampire->sendModifyInfo(prev);

        //(int)pTargetVampire->getDefense() << endl; 		cout << "Prev Protection : " << (int)prev.Protection << "
    }

    GCRemoveEffect gcRemoveEffect;
    gcRemoveEffect.setObjectID(pCreature->getObjectID());
    gcRemoveEffect.addEffectList(Effect::EFFECT_CLASS_CASKET);
    pZone->broadcastPacket(pCreature->getX(), pCreature->getY(), &gcRemoveEffect);
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectSummonCasket::unaffect(Zone* pZone, ZoneCoord_t x, ZoneCoord_t y, Object* pObject)

{}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
string EffectSummonCasket::toString() const {
    StringStream msg;
    msg << "EffectSummonCasket(" << "ObjectID:" << getObjectID() << ")";
    return msg.toString();
}
