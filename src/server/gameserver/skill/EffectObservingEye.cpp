//////////////////////////////////////////////////////////////////////////////
// Filename    : EffectObservingEye.cpp
// Written by  : elca
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "EffectObservingEye.h"

#include "GCModifyInformation.h"
#include "GCOtherModifyInfo.h"
#include "GCRemoveEffect.h"
#include "GCStatusCurrentHP.h"
#include "Monster.h"
#include "PacketUtil.h"
#include "Player.h"
#include "Slayer.h"
#include "Vampire.h"

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
EffectObservingEye::EffectObservingEye(Creature* pCreature)

{
    Assert(pCreature != NULL);
    Assert(pCreature->isSlayer());

    setTarget(pCreature);
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectObservingEye::affect(Creature* pCreature)

{}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectObservingEye::affect(Zone* pZone, ZoneCoord_t x, ZoneCoord_t y, Object* pObject)

{}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectObservingEye::unaffect()

{
    Creature* pCreature = dynamic_cast<Creature*>(m_pTarget);
    unaffect(pCreature);
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectObservingEye::unaffect(Creature* pCreature)

{
    Assert(pCreature != NULL);
    Assert(pCreature->isSlayer()); // Only a Slayer can be affected.

    pCreature->removeFlag(Effect::EFFECT_CLASS_OBSERVING_EYE);

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
    gcRemoveEffect.addEffectList(Effect::EFFECT_CLASS_OBSERVING_EYE);
    pZone->broadcastPacket(pCreature->getX(), pCreature->getY(), &gcRemoveEffect);

    // Removes the creatures that were visible through this effect.
    pZone->updateInvisibleScan(pCreature);

    GCOtherModifyInfo gcOtherModifyInfo;
    makeGCOtherModifyInfo(&gcOtherModifyInfo, pSlayer, &prev);
    pZone->broadcastPacket(pSlayer->getX(), pSlayer->getY(), &gcOtherModifyInfo, pSlayer);
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectObservingEye::unaffect(Zone* pZone, ZoneCoord_t x, ZoneCoord_t y, Object* pObject)

{}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
bool EffectObservingEye::canSeeInvisibility(Creature* pTarget) const

{
    if (!pTarget->isFlag(Effect::EFFECT_CLASS_INVISIBILITY))
        return true;

    Level_t level;

    if (pTarget->isVampire()) {
        Vampire* pVampire = dynamic_cast<Vampire*>(pTarget);
        Assert(pVampire != NULL);

        level = pVampire->getLevel();
    } else if (pTarget->isMonster()) {
        Monster* pMonster = dynamic_cast<Monster*>(pTarget);
        Assert(pMonster != NULL);

        level = pMonster->getLevel();
    } else {
        // Anything that is not a Vampire or a monster counts as not seen.
        return false;
    }

    if (m_SkillLevel < level + 40)
        return false;
    else
        return true;
}


//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
string EffectObservingEye::toString() const {
    StringStream msg;
    msg << "EffectObservingEye(" << "ObjectID:" << getObjectID() << ")";
    return msg.toString();
}
