//////////////////////////////////////////////////////////////////////////////
// Filename    : EffectDragonEye.cpp
// Written by  : elca
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "EffectDragonEye.h"

#include "Ousters.h"
#include "Slayer.h"
#include "Vampire.h"

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
EffectDragonEye::EffectDragonEye(Creature* pCreature)

    : EffectHasRelic(pCreature) {}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
EffectDragonEye::EffectDragonEye(Item* pItem)

    : EffectHasRelic(pItem) {}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectDragonEye::affect(Creature* pCreature)

{
    // Double the HP
    if (pCreature->isSlayer()) {
        Slayer* pSlayer = dynamic_cast<Slayer*>(pCreature);
        SLAYER_RECORD prev;
        pSlayer->getSlayerRecord(prev);
        pSlayer->initAllStat();
        pSlayer->sendModifyInfo(prev);
    } else if (pCreature->isVampire()) {
        Vampire* pVampire = dynamic_cast<Vampire*>(pCreature);
        VAMPIRE_RECORD prev;
        pVampire->getVampireRecord(prev);
        pVampire->initAllStat();
        pVampire->sendModifyInfo(prev);
    } else if (pCreature->isOusters()) {
        Ousters* pOusters = dynamic_cast<Ousters*>(pCreature);
        OUSTERS_RECORD prev;
        pOusters->getOustersRecord(prev);
        pOusters->initAllStat();
        pOusters->sendModifyInfo(prev);
    }
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectDragonEye::unaffect(Creature* pCreature)

{
    // Restore the HP
    if (pCreature->isSlayer()) {
        Slayer* pSlayer = dynamic_cast<Slayer*>(pCreature);
        SLAYER_RECORD prev;
        pSlayer->getSlayerRecord(prev);
        pSlayer->initAllStat();
        pSlayer->sendModifyInfo(prev);
    } else if (pCreature->isVampire()) {
        Vampire* pVampire = dynamic_cast<Vampire*>(pCreature);
        VAMPIRE_RECORD prev;
        pVampire->getVampireRecord(prev);
        pVampire->initAllStat();
        pVampire->sendModifyInfo(prev);
    } else if (pCreature->isOusters()) {
        Ousters* pOusters = dynamic_cast<Ousters*>(pCreature);
        OUSTERS_RECORD prev;
        pOusters->getOustersRecord(prev);
        pOusters->initAllStat();
        pOusters->sendModifyInfo(prev);
    }

    EffectHasRelic::unaffect(pCreature);
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectDragonEye::affect(Item* pItem)

{}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
string EffectDragonEye::toString() const {
    StringStream msg;
    msg << "EffectDragonEye(" << "ObjectID:" << getObjectID() << ")";
    return msg.toString();
}
