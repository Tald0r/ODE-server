//////////////////////////////////////////////////////////////////////////////
// Filename    : EffectPotentialExplosion.cpp
// Written by  : elca
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "EffectPotentialExplosion.h"

#include "GCOtherModifyInfo.h"
#include "GCRemoveEffect.h"
#include "Monster.h"
#include "PacketUtil.h"
#include "Player.h"
#include "Slayer.h"
#include "Vampire.h"

EffectPotentialExplosion::EffectPotentialExplosion(Creature* pCreature)

{
    setTarget(pCreature);
}

void EffectPotentialExplosion::affect(Creature* pCreature)

{}

void EffectPotentialExplosion::affect(Zone* pZone, ZoneCoord_t x, ZoneCoord_t y, Object* pObject)

{}

void EffectPotentialExplosion::unaffect()

{
    Creature* pCreature = dynamic_cast<Creature*>(m_pTarget);
    unaffect(pCreature);
}

void EffectPotentialExplosion::unaffect(Creature* pCreature)

{
    __BEGIN_DEBUG


    Assert(pCreature != NULL);
    Assert(pCreature->isSlayer());

    pCreature->removeFlag(Effect::EFFECT_CLASS_POTENTIAL_EXPLOSION);

    Slayer* pSlayer = dynamic_cast<Slayer*>(pCreature);

    SLAYER_RECORD prev;
    pSlayer->getSlayerRecord(prev);
    pSlayer->initAllStat();
    pSlayer->sendRealWearingInfo();
    pSlayer->sendModifyInfo(prev);

    Zone* pZone = pSlayer->getZone();
    Assert(pZone != NULL);

    GCRemoveEffect gcRemoveEffect;
    gcRemoveEffect.setObjectID(pSlayer->getObjectID());
    gcRemoveEffect.addEffectList(Effect::EFFECT_CLASS_POTENTIAL_EXPLOSION);
    pZone->broadcastPacket(pSlayer->getX(), pSlayer->getY(), &gcRemoveEffect);

    GCOtherModifyInfo gcOtherModifyInfo;
    makeGCOtherModifyInfo(&gcOtherModifyInfo, pSlayer, &prev);
    pZone->broadcastPacket(pSlayer->getX(), pSlayer->getY(), &gcOtherModifyInfo, pSlayer);


    __END_DEBUG
}

void EffectPotentialExplosion::unaffect(Zone* pZone, ZoneCoord_t x, ZoneCoord_t y, Object* pObject)

{}

string EffectPotentialExplosion::toString() const {
    StringStream msg;
    msg << "EffectPotentialExplosion(" << "ObjectID:" << getObjectID() << ")";
    return msg.toString();
}
