//////////////////////////////////////////////////////////////////////////////
// Filename    : Light.cpp
// Written by  : elca@ewestsoft.com
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "Light.h"

#include "EffectLight.h"
#include "GCAddEffect.h"
#include "GCSkillToSelfOK1.h"
#include "GCSkillToSelfOK2.h"
#include "GameContext.h"

//////////////////////////////////////////////////////////////////////////////
// Slayer self handler
//////////////////////////////////////////////////////////////////////////////
void Light::execute(Slayer* pSlayer, SkillSlot* pSkillSlot, CEffectID_t CEffectID)

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
        SkillLevel_t SkillLevel = pSkillSlot->getExpLevel();

        int RequiredMP = (int)pSkillInfo->getConsumeMP();
        bool bManaCheck = hasEnoughMana(pSlayer, RequiredMP);
        bool bTimeCheck = verifyRunTime(pSkillSlot);
        bool bRangeCheck = checkZoneLevelToUseSkill(pSlayer);
        bool bHitRoll = HitRoll::isSuccessMagic(pSlayer, pSkillInfo, pSkillSlot);
        bool bEffected = pSlayer->isFlag(Effect::EFFECT_CLASS_LIGHT);

        if (bManaCheck && bTimeCheck && bRangeCheck && bHitRoll && !bEffected) {
            decreaseMana(pSlayer, RequiredMP, _GCSkillToSelfOK1);

            // The time the skill lasts varies with proficiency.
            SkillInput input(pSlayer, pSkillSlot);
            SkillOutput output;
            computeOutput(input, output);

            Sight_t CurrentSight = pSlayer->getSight();
            Sight_t oldSight = CurrentSight;

            // Deletes an older effect if one is still attached.
            if (pSlayer->isEffect(Effect::EFFECT_CLASS_LIGHT)) {
                EffectLight* pOldEffectLight = (EffectLight*)pSlayer->findEffect(Effect::EFFECT_CLASS_LIGHT);
                CurrentSight = pOldEffectLight->getOldSight();
                pSlayer->deleteEffect(Effect::EFFECT_CLASS_LIGHT);
            }

            // Creates the effect and attaches it.
            EffectLight* pEffectLight = new EffectLight(pSlayer);
            pEffectLight->setDeadline(output.Duration);
            pEffectLight->setOldSight(CurrentSight);
            pSlayer->setFlag(Effect::EFFECT_CLASS_LIGHT);
            pSlayer->addEffect(pEffectLight);


            // Updates the sight.
            Sight_t MinSight = pSkillInfo->getMinDamage();
            Sight_t MaxSight = pSkillInfo->getMaxDamage();
            Sight_t NewSight = MinSight + (MaxSight - MinSight) * SkillLevel / 100;

            pSlayer->setSight(NewSight);

            if (NewSight != oldSight)
                _GCSkillToSelfOK1.addShortData(MODIFY_VISION, NewSight);

            _GCSkillToSelfOK1.setSkillType(SkillType);
            _GCSkillToSelfOK1.setCEffectID(CEffectID);
            _GCSkillToSelfOK1.setDuration(output.Duration);

            _GCSkillToSelfOK2.setObjectID(pSlayer->getObjectID());
            _GCSkillToSelfOK2.setSkillType(SkillType);
            _GCSkillToSelfOK2.setDuration(output.Duration);

            // EXP UP!
            SkillDomainType_t DomainType = pSkillInfo->getDomainType();
            SkillGrade Grade =
                de::gameContext().skillInfos().getGradeByDomainLevel(pSlayer->getSkillDomainLevel(DomainType));
            Exp_t ExpUp = 10 * (Grade + 1) * 2;

            shareAttrExp(pSlayer, ExpUp, 1, 1, 8, _GCSkillToSelfOK1);
            increaseDomainExp(pSlayer, DomainType, pSkillInfo->getPoint(), _GCSkillToSelfOK1);
            increaseSkillExp(pSlayer, DomainType, pSkillSlot, pSkillInfo, _GCSkillToSelfOK1);

            // Send Packet
            pPlayer->sendPacket(&_GCSkillToSelfOK1);

            pZone->broadcastPacket(pSlayer->getX(), pSlayer->getY(), &_GCSkillToSelfOK2, pSlayer);

            // Notifies that the effect has been attached.
            GCAddEffect gcAddEffect;
            gcAddEffect.setObjectID(pSlayer->getObjectID());
            gcAddEffect.setEffectID(Effect::EFFECT_CLASS_LIGHT);
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

Light g_Light;
