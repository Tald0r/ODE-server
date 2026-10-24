//////////////////////////////////////////////////////////////////////////////
// Filename    : DetectHidden.cpp
// Written by  : elca@ewestsoft.com
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "DetectHidden.h"

#include "EffectDetectHidden.h"
#include "GCAddEffect.h"
#include "GCSkillToSelfOK1.h"
#include "GCSkillToSelfOK2.h"
#include "GameContext.h"
#include "Party.h"

//////////////////////////////////////////////////////////////////////////////
// Slayer self handler
//////////////////////////////////////////////////////////////////////////////
void DetectHidden::execute(Slayer* pSlayer, SkillSlot* pSkillSlot, CEffectID_t CEffectID)

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
        bool bEffected = pSlayer->isFlag(Effect::EFFECT_CLASS_DETECT_HIDDEN);

        if (bManaCheck && bTimeCheck && bRangeCheck && bHitRoll && !bEffected) {
            decreaseMana(pSlayer, RequiredMP, _GCSkillToSelfOK1);

            // The time the skill lasts varies with proficiency.
            SkillInput input(pSlayer, pSkillSlot);
            SkillOutput output;
            computeOutput(input, output);

            // Create the effect object and attach it.
            EffectDetectHidden* pEffectDetectHidden = new EffectDetectHidden(pSlayer);
            pEffectDetectHidden->setDeadline(output.Duration);
            pSlayer->addEffect(pEffectDetectHidden);
            pSlayer->setFlag(Effect::EFFECT_CLASS_DETECT_HIDDEN);

            // With this effect attached, what was hidden becomes visible.
            pZone->updateHiddenScan(pSlayer);

            // Raises experience.
            SkillGrade Grade =
                de::gameContext().skillInfos().getGradeByDomainLevel(pSlayer->getSkillDomainLevel(DomainType));
            Exp_t ExpUp = 10 * (Grade + 1) * 2;
            shareAttrExp(pSlayer, ExpUp, 1, 1, 8, _GCSkillToSelfOK1);
            increaseDomainExp(pSlayer, DomainType, pSkillInfo->getPoint(), _GCSkillToSelfOK1);
            increaseSkillExp(pSlayer, DomainType, pSkillSlot, pSkillInfo, _GCSkillToSelfOK1);

            _GCSkillToSelfOK1.setSkillType(SkillType);
            _GCSkillToSelfOK1.setCEffectID(CEffectID);
            _GCSkillToSelfOK1.setDuration(output.Duration);

            _GCSkillToSelfOK2.setObjectID(pSlayer->getObjectID());
            _GCSkillToSelfOK2.setSkillType(SkillType);
            _GCSkillToSelfOK2.setDuration(output.Duration);

            pPlayer->sendPacket(&_GCSkillToSelfOK1);

            pZone->broadcastPacket(pSlayer->getX(), pSlayer->getY(), &_GCSkillToSelfOK2, pSlayer);

            // Notifies that the effect has been attached.
            GCAddEffect gcAddEffect;
            gcAddEffect.setObjectID(pSlayer->getObjectID());
            gcAddEffect.setEffectID(Effect::EFFECT_CLASS_DETECT_HIDDEN);
            gcAddEffect.setDuration(output.Duration);
            pZone->broadcastPacket(pSlayer->getX(), pSlayer->getY(), &gcAddEffect);

            pSkillSlot->setRunTime(output.Delay);

            uint PartyID = pSlayer->getPartyID();
            if (PartyID != 0) {
                LocalPartyManager* pLPM = pZone->getLocalPartyManager();
                pLPM->shareDetectHidden(PartyID, pSlayer, output.Duration);
            }
        } else {
            executeSkillFailNormal(pSlayer, getSkillType(), NULL);
        }
    } catch (Throwable& t) {
        executeSkillFailException(pSlayer, getSkillType());
    }
}

DetectHidden g_DetectHidden;
