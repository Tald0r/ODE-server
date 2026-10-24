//////////////////////////////////////////////////////////////////////////////
// Filename    : DenialMagic.cpp
// Written by  :
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "DenialMagic.h"

#include "EffectDenialMagic.h"
#include "GCAddEffect.h"
#include "GCSkillToSelfOK1.h"
#include "GCSkillToSelfOK2.h"
#include "GameContext.h"

//////////////////////////////////////////////////////////////////////////////
// Slayer self handler
//////////////////////////////////////////////////////////////////////////////
void DenialMagic::execute(Slayer* pSlayer, SkillSlot* pSkillSlot, CEffectID_t CEffectID)

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

        int RequiredMP = (int)pSkillInfo->getConsumeMP();
        bool bManaCheck = hasEnoughMana(pSlayer, RequiredMP);
        bool bTimeCheck = verifyRunTime(pSkillSlot);
        bool bRangeCheck = checkZoneLevelToUseSkill(pSlayer);
        bool bHitRoll = HitRoll::isSuccessMagic(pSlayer, pSkillInfo, pSkillSlot);
        bool bEffected = pSlayer->isFlag(Effect::EFFECT_CLASS_DENIAL_MAGIC);

        if (bManaCheck && bTimeCheck && bRangeCheck && bHitRoll && !bEffected) {
            decreaseMana(pSlayer, RequiredMP, _GCSkillToSelfOK1);

            // Compute the duration.
            SkillInput input(pSlayer, pSkillSlot);
            SkillOutput output;
            computeOutput(input, output);

            // Create the effect class and attach it.
            EffectDenialMagic* pEffect = new EffectDenialMagic(pSlayer);
            pEffect->setDeadline(output.Duration);
            pSlayer->addEffect(pEffect);
            pSlayer->setFlag(Effect::EFFECT_CLASS_DENIAL_MAGIC);

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
            gcAddEffect.setEffectID(pEffect->getEffectClass());
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

DenialMagic g_DenialMagic;
