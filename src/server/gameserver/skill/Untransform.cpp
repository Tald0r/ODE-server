//////////////////////////////////////////////////////////////////////////////
// Filename    : Untransform.cpp
// Written by  : elca@ewestsoft.com
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "Untransform.h"

#include "GCDeleteObject.h"
#include "GCRemoveEffect.h"
#include "GCSkillFailed1.h"
#include "GCSkillToSelfOK1.h"
#include "GCSkillToSelfOK3.h"
#include "ZoneUtil.h"

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void Untransform::execute(Slayer* pSlayer, SkillSlot* pSkillSlot, CEffectID_t CEffectID)

{
    Assert(pSlayer != NULL);

    try {
        if (pSlayer->isFlag(Effect::EFFECT_CLASS_INSTALL_TURRET)) {
            Zone* pZone = pSlayer->getZone();
            Assert(pZone != NULL);

            Effect* pEffect = pSlayer->findEffect(Effect::EFFECT_CLASS_INSTALL_TURRET);
            if (pEffect != NULL)
                pEffect->setDeadline(0);

            GCSkillToSelfOK1 gcOK1;

            gcOK1.setSkillType(SKILL_UN_TRANSFORM);
            pSlayer->getPlayer()->sendPacket(&gcOK1);

            // EffectSummonSylph::unaffect does all of this.

            // Sends to the zone that the effect was removed
            //
            //
            //			// Recalculates defense and protection and sends them

        } else {
            GCSkillFailed1 gcFail;
            gcFail.setSkillType(SKILL_UN_TRANSFORM);
            pSlayer->getPlayer()->sendPacket(&gcFail);
        }
    } catch (Throwable& t) {
        // No GCSkillFailed1 here: the client asks not to be sent one.
        diagnosticFilelog("errorLog.txt", "Untransform::execute(Slayer*) : %s", t.toString().c_str());
    }
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void Untransform::execute(Vampire* pVampire)

{
    Assert(pVampire != NULL);

    try {
        Zone* pZone = pVampire->getZone();
        Assert(pZone != NULL);
        addUntransformCreature(pZone, pVampire, true);
    } catch (Throwable& t) {
        executeSkillFailException(pVampire, getSkillType());
    }
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void Untransform::execute(Ousters* pOusters)

{
    Assert(pOusters != NULL);

    try {
        if (pOusters->isFlag(Effect::EFFECT_CLASS_SUMMON_SYLPH)) {
            Zone* pZone = pOusters->getZone();
            Assert(pZone != NULL);

            Effect* pEffect = pOusters->findEffect(Effect::EFFECT_CLASS_SUMMON_SYLPH);
            if (pEffect != NULL)
                pEffect->setDeadline(0);

            GCSkillToSelfOK1 gcOK1;

            gcOK1.setSkillType(SKILL_UN_TRANSFORM);
            pOusters->getPlayer()->sendPacket(&gcOK1);

            // EffectSummonSylph::unaffect does all of this.

            // Sends to the zone that the effect was removed
            //
            //
            //			// Recalculates defense and protection and sends them

        } else {
            GCSkillFailed1 gcFail;
            gcFail.setSkillType(SKILL_UN_TRANSFORM);
            pOusters->getPlayer()->sendPacket(&gcFail);
        }
    } catch (Throwable& t) {
        // No GCSkillFailed1 here: the client asks not to be sent one.
        diagnosticFilelog("errorLog.txt", "Untransform::execute(Ousters*) : %s", t.toString().c_str());
    }
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void Untransform::execute(Monster* pMonster)

{
    Assert(pMonster != NULL);

    try {
        Zone* pZone = pMonster->getZone();
        Assert(pZone != NULL);
        addUntransformCreature(pZone, pMonster, true);
    } catch (Throwable& t) {
        diagnosticFilelog("errorLog.txt", "Untransform::execute(Monster*) : %s", t.toString().c_str());
    }
}

Untransform g_Untransform;
