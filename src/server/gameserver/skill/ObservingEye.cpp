//////////////////////////////////////////////////////////////////////////////
// Filename    : ObservingEye.cpp
// Written by  :
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "ObservingEye.h"

#include "EffectObservingEye.h"
#include "GCAddEffect.h"
#include "GCOtherModifyInfo.h"
#include "GCSkillToObjectOK1.h"
#include "GCSkillToObjectOK2.h"
#include "GCSkillToObjectOK3.h"
#include "GCSkillToObjectOK4.h"
#include "GCSkillToObjectOK5.h"
#include "GCSkillToSelfOK1.h"
#include "GCSkillToSelfOK2.h"
#include "GameContext.h"
#include "PacketUtil.h"

//////////////////////////////////////////////////////////////////////////////
// Slayer self handler
//////////////////////////////////////////////////////////////////////////////
void ObservingEye::execute(Slayer* pSlayer, SkillSlot* pSkillSlot, CEffectID_t CEffectID)

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

        ZoneCoord_t myX = pSlayer->getX();
        ZoneCoord_t myY = pSlayer->getY();

        int RequiredMP = (int)pSkillInfo->getConsumeMP();
        bool bManaCheck = hasEnoughMana(pSlayer, RequiredMP);
        bool bTimeCheck = verifyRunTime(pSkillSlot);
        bool bRangeCheck = checkZoneLevelToUseSkill(pSlayer);
        bool bHitRoll = HitRoll::isSuccessMagic(pSlayer, pSkillInfo, pSkillSlot);
        bool bEffected = pSlayer->isFlag(Effect::EFFECT_CLASS_OBSERVING_EYE);

        if (bManaCheck && bTimeCheck && bRangeCheck && bHitRoll && !bEffected) {
            decreaseMana(pSlayer, RequiredMP, _GCSkillToSelfOK1);

            SkillInput input(pSlayer, pSkillSlot);
            SkillOutput output;
            computeOutput(input, output);

            // Creates the effect and attaches it.
            EffectObservingEye* pEffect = new EffectObservingEye(pSlayer);
            pEffect->setDeadline(output.Duration);
            // Use output.Damage as the vision bonus.
            pEffect->setDamageBonus(output.Damage * 2 - 1);
            pEffect->setCriticalHitBonus(output.Damage * 10 - 49);
            pEffect->setVisionBonus(output.Damage);
            pEffect->setSkillLevel(max(100, (int)pSlayer->getSkillDomainLevel(SKILL_DOMAIN_GUN)));
            pSlayer->setFlag(Effect::EFFECT_CLASS_OBSERVING_EYE);
            pSlayer->addEffect(pEffect);

            // With this effect attached, what was hidden becomes visible.
            pZone->updateInvisibleScan(pSlayer);

            // Recomputes the stats now that the effect is attached.
            SLAYER_RECORD prev;
            pSlayer->getSlayerRecord(prev);
            pSlayer->initAllStat();
            pSlayer->sendRealWearingInfo();
            pSlayer->addModifyInfo(prev, _GCSkillToSelfOK1);

            // Raises experience.
            SkillGrade Grade =
                de::gameContext().skillInfos().getGradeByDomainLevel(pSlayer->getSkillDomainLevel(DomainType));
            Exp_t ExpUp = 10 * (Grade + 1);
            shareAttrExp(pSlayer, ExpUp, 1, 8, 1, _GCSkillToSelfOK1);
            increaseDomainExp(pSlayer, DomainType, pSkillInfo->getPoint(), _GCSkillToSelfOK1);

            _GCSkillToSelfOK1.setSkillType(SkillType);
            _GCSkillToSelfOK1.setCEffectID(CEffectID);
            _GCSkillToSelfOK1.setDuration(0);

            _GCSkillToSelfOK2.setObjectID(pSlayer->getObjectID());
            _GCSkillToSelfOK2.setSkillType(SkillType);
            _GCSkillToSelfOK2.setDuration(0);

            pPlayer->sendPacket(&_GCSkillToSelfOK1);
            pZone->broadcastPacket(myX, myY, &_GCSkillToSelfOK2, pSlayer);

            GCAddEffect gcAddEffect;
            gcAddEffect.setObjectID(pSlayer->getObjectID());
            gcAddEffect.setEffectID(Effect::EFFECT_CLASS_OBSERVING_EYE);
            gcAddEffect.setDuration(output.Duration);
            pZone->broadcastPacket(myX, myY, &gcAddEffect);

            GCOtherModifyInfo gcOtherModifyInfo;
            makeGCOtherModifyInfo(&gcOtherModifyInfo, pSlayer, &prev);
            pZone->broadcastPacket(myX, myY, &gcOtherModifyInfo, pSlayer);

            pSkillSlot->setRunTime(output.Delay);
        } else {
            executeSkillFailNormal(pSlayer, getSkillType(), NULL);
        }
    } catch (Throwable& t) {
        executeSkillFailException(pSlayer, getSkillType());
    }
}

ObservingEye g_ObservingEye;
