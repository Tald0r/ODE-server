//////////////////////////////////////////////////////////////////////////////
// Filename    : EffectIncreaseAttr.cpp
// Written by  : elca
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "EffectIncreaseAttr.h"

#include "GCModifyInformation.h"
#include "GCOtherModifyInfo.h"
#include "GCRemoveEffect.h"
#include "GCStatusCurrentHP.h"
#include "Monster.h"
#include "Ousters.h"
#include "PacketUtil.h"
#include "Player.h"
#include "Slayer.h"
#include "Vampire.h"

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
EffectIncreaseAttr::EffectIncreaseAttr(Creature* pCreature)

{
    Assert(pCreature != NULL);
    Assert(pCreature->isSlayer());

    setTarget(pCreature);

    // Server-only Effect.
    m_bBroadcastingEffect = false;
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectIncreaseAttr::affect(Creature* pCreature)

{}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectIncreaseAttr::affect(Zone* pZone, ZoneCoord_t x, ZoneCoord_t y, Object* pObject)

{}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectIncreaseAttr::unaffect()

{
    // cout << "EffectIncreaseAttr" << "unaffect BEGIN" << endl;

    Creature* pCreature = dynamic_cast<Creature*>(m_pTarget);
    unaffect(pCreature);

    // cout << "EffectIncreaseAttr" << "unaffect END" << endl;
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectIncreaseAttr::unaffect(Creature* pCreature)

{
    // cout << "EffectIncreaseAttr" << "unaffect BEGIN" << endl;

    Assert(pCreature != NULL);

    if (pCreature->isSlayer()) {
        pCreature->removeFlag(Effect::EFFECT_CLASS_INCRASE_ATTR);

        Slayer* pSlayer = dynamic_cast<Slayer*>(pCreature);

        SLAYER_RECORD prev;
        pSlayer->getSlayerRecord(prev);
        pSlayer->initAllStat();
        pSlayer->sendRealWearingInfo();
        pSlayer->sendModifyInfo(prev);

        Zone* pZone = pCreature->getZone();
        Assert(pZone != NULL);

        GCRemoveEffect gcRemoveEffect;
        gcRemoveEffect.setObjectID(pCreature->getObjectID());
        gcRemoveEffect.addEffectList(Effect::EFFECT_CLASS_INCRASE_ATTR);
        pZone->broadcastPacket(pCreature->getX(), pCreature->getY(), &gcRemoveEffect);

        GCOtherModifyInfo gcOtherModifyInfo;
        makeGCOtherModifyInfo(&gcOtherModifyInfo, pSlayer, &prev);
        pZone->broadcastPacket(pSlayer->getX(), pSlayer->getY(), &gcOtherModifyInfo, pSlayer);
    } else if (pCreature->isVampire()) {
        pCreature->removeFlag(Effect::EFFECT_CLASS_INCRASE_ATTR);

        Vampire* pVampire = dynamic_cast<Vampire*>(pCreature);

        VAMPIRE_RECORD prev;
        pVampire->getVampireRecord(prev);
        pVampire->initAllStat();
        pVampire->sendRealWearingInfo();
        pVampire->sendModifyInfo(prev);

        Zone* pZone = pCreature->getZone();
        Assert(pZone != NULL);

        GCRemoveEffect gcRemoveEffect;
        gcRemoveEffect.setObjectID(pCreature->getObjectID());
        gcRemoveEffect.addEffectList(Effect::EFFECT_CLASS_INCRASE_ATTR);
        pZone->broadcastPacket(pCreature->getX(), pCreature->getY(), &gcRemoveEffect);

        GCOtherModifyInfo gcOtherModifyInfo;
        makeGCOtherModifyInfo(&gcOtherModifyInfo, pVampire, &prev);
        pZone->broadcastPacket(pVampire->getX(), pVampire->getY(), &gcOtherModifyInfo, pVampire);
    } else if (pCreature->isOusters()) {
        pCreature->removeFlag(Effect::EFFECT_CLASS_INCRASE_ATTR);

        Ousters* pOusters = dynamic_cast<Ousters*>(pCreature);

        OUSTERS_RECORD prev;
        pOusters->getOustersRecord(prev);
        pOusters->initAllStat();
        pOusters->sendRealWearingInfo();
        pOusters->sendModifyInfo(prev);

        Zone* pZone = pCreature->getZone();
        Assert(pZone != NULL);

        GCRemoveEffect gcRemoveEffect;
        gcRemoveEffect.setObjectID(pCreature->getObjectID());
        gcRemoveEffect.addEffectList(Effect::EFFECT_CLASS_INCRASE_ATTR);
        pZone->broadcastPacket(pCreature->getX(), pCreature->getY(), &gcRemoveEffect);

        GCOtherModifyInfo gcOtherModifyInfo;
        makeGCOtherModifyInfo(&gcOtherModifyInfo, pOusters, &prev);
        pZone->broadcastPacket(pOusters->getX(), pOusters->getY(), &gcOtherModifyInfo, pOusters);
    }

    // cout << "EffectIncreaseAttr" << "unaffect END" << endl;
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectIncreaseAttr::unaffect(Zone* pZone, ZoneCoord_t x, ZoneCoord_t y, Object* pObject)

{}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
string EffectIncreaseAttr::toString() const {
    StringStream msg;
    msg << "EffectIncreaseAttr(" << "ObjectID:" << getObjectID() << ")";
    return msg.toString();
}
