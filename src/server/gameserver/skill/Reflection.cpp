//////////////////////////////////////////////////////////////////////////////
// Filename    : Reflection.cpp
// Written by  :
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "Reflection.h"

#include "EffectReflection.h"
#include "GCAddEffect.h"
#include "GCSkillToSelfOK1.h"
#include "GCSkillToSelfOK2.h"
#include "GameContext.h"

//////////////////////////////////////////////////////////////////////////////
// Slayer self handler
//////////////////////////////////////////////////////////////////////////////
void Reflection::execute(Slayer* pSlayer, SkillSlot* pSkillSlot, CEffectID_t CEffectID)

{
    Assert(pSlayer != NULL);
    Assert(pSkillSlot != NULL);

    try {
        Player* pPlayer = pSlayer->getPlayer();
        Zone* pZone = pSlayer->getZone();

        Assert(pPlayer != NULL);
        Assert(pZone != NULL);

        GCSkillToSelfOK1 _GCSkillToSelfOK1;
        GCSkillToSelfOK2 _GCSkillToSelfOK2;

        SkillType_t SkillType = pSkillSlot->getSkillType();
        SkillInfo* pSkillInfo = de::gameContext().skillInfos().getSkillInfo(SkillType);
        SkillDomainType_t DomainType = pSkillInfo->getDomainType();
        SkillLevel_t SkillLevel = pSkillSlot->getExpLevel();

        int RequiredMP = (int)pSkillInfo->getConsumeMP();
        bool bManaCheck = hasEnoughMana(pSlayer, RequiredMP);
        bool bTimeCheck = verifyRunTime(pSkillSlot);
        bool bRangeCheck = checkZoneLevelToUseSkill(pSlayer);
        bool bHitRoll = HitRoll::isSuccessMagic(pSlayer, pSkillInfo, pSkillSlot);
        bool bEffected = pSlayer->isFlag(Effect::EFFECT_CLASS_REFLECTION);

        if (bManaCheck && bTimeCheck && bRangeCheck && bHitRoll && !bEffected) {
            decreaseMana(pSlayer, RequiredMP, _GCSkillToSelfOK1);

            // Compute the duration.
            SkillInput input(pSlayer, pSkillSlot);
            SkillOutput output;
            computeOutput(input, output);

            // Create the effect class and attach it.
            EffectReflection* pEffect = new EffectReflection(pSlayer);
            pEffect->setDeadline(output.Duration);
            pEffect->setLevel(SkillLevel);
            pSlayer->addEffect(pEffect);
            pSlayer->setFlag(Effect::EFFECT_CLASS_REFLECTION);

            // Raises experience.
            SkillGrade Grade =
                de::gameContext().skillInfos().getGradeByDomainLevel(pSlayer->getSkillDomainLevel(DomainType));
            Exp_t ExpUp = 10 * (Grade + 1);
            shareAttrExp(pSlayer, ExpUp, 1, 1, 8, _GCSkillToSelfOK1);
            increaseDomainExp(pSlayer, DomainType, pSkillInfo->getPoint(), _GCSkillToSelfOK1);
            increaseSkillExp(pSlayer, DomainType, pSkillSlot, pSkillInfo, _GCSkillToSelfOK1);

            _GCSkillToSelfOK1.setSkillType(SkillType);
            _GCSkillToSelfOK1.setCEffectID(CEffectID);
            _GCSkillToSelfOK1.setDuration(0);

            _GCSkillToSelfOK2.setObjectID(pSlayer->getObjectID());
            _GCSkillToSelfOK2.setSkillType(SkillType);
            _GCSkillToSelfOK2.setDuration(0);

            pPlayer->sendPacket(&_GCSkillToSelfOK1);
            pZone->broadcastPacket(pSlayer->getX(), pSlayer->getY(), &_GCSkillToSelfOK2, pSlayer);

            GCAddEffect gcAddEffect;
            gcAddEffect.setObjectID(pSlayer->getObjectID());
            gcAddEffect.setEffectID(Effect::EFFECT_CLASS_REFLECTION);
            gcAddEffect.setDuration(output.Duration);
            pZone->broadcastPacket(pSlayer->getX(), pSlayer->getY(), &gcAddEffect);

            pSkillSlot->setRunTime(output.Delay);
        } else {
            executeSkillFailNormal(pSlayer, getSkillType(), NULL);
        }
    } catch (Throwable& t) {
        executeSkillFailException(pSlayer, getSkillType());
    }
}


// Does reflection happen?
bool CheckReflection(Creature* pAttacker, Creature* pTargetCreature, SkillType_t SkillType) {
    // Only a Slayer can use this skill.
    if (pAttacker == NULL || pTargetCreature == NULL || pTargetCreature->isSlayer() == false) {
        return false;
    }

    // If the skill is active and the target is not paralyzed...
    if (pTargetCreature->isFlag(Effect::EFFECT_CLASS_REFLECTION) &&
        !pTargetCreature->isFlag(Effect::EFFECT_CLASS_PARALYZE)) {
        Slayer* pTargetSlayer = dynamic_cast<Slayer*>(pTargetCreature);
        SkillSlot* pSkillSlot = pTargetSlayer->hasSkill(SKILL_REFLECTION);
        SkillInfo* pSkillInfo = de::gameContext().skillInfos().getSkillInfo(SKILL_REFLECTION);
        Zone* pZone = pAttacker->getZone();

        Assert(pTargetSlayer != NULL);
        Assert(pSkillSlot != NULL);
        Assert(pSkillInfo != NULL);
        Assert(pZone != NULL);

        // If the hit roll succeeds...

        int SuccessRate = 30 + pSkillSlot->getExpLevel() / 5;
        SuccessRate = min(SuccessRate, 50); // At most 50%

        if (rand() % 100 < SuccessRate) {
            GCSkillToSelfOK1 _GCSkillToSelfOK1;
            GCSkillToSelfOK2 _GCSkillToSelfOK2;

            // Shows the cure effect to the target itself.
            if (pTargetCreature->isPC()) {
                _GCSkillToSelfOK1.setSkillType(SKILL_CURE_EFFECT);
                _GCSkillToSelfOK1.setDuration(0);
                pTargetCreature->getPlayer()->sendPacket(&_GCSkillToSelfOK1);
            }

            // Shows the cure effect to the others.
            _GCSkillToSelfOK2.setObjectID(pTargetCreature->getObjectID());
            _GCSkillToSelfOK2.setSkillType(SKILL_CURE_EFFECT);
            _GCSkillToSelfOK2.setDuration(0);
            pZone->broadcastPacket(pTargetCreature->getX(), pTargetCreature->getY(), &_GCSkillToSelfOK2,
                                   pTargetCreature);


            return true;
        }
    }

    return false;
}


Reflection g_Reflection;
