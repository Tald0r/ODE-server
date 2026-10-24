//////////////////////////////////////////////////////////////////////////////
// Filename    : CrossGuard.cpp
// Written by  :
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "CrossGuard.h"

#include "EffectCrossGuard.h"
#include "GCAddEffect.h"
#include "GCSkillToSelfOK1.h"
#include "GCSkillToSelfOK2.h"
#include "GameContext.h"

//////////////////////////////////////////////////////////////////////////////
// Slayer self handler
//////////////////////////////////////////////////////////////////////////////
void CrossGuard::execute(Ousters* pOusters, OustersSkillSlot* pOustersSkillSlot, CEffectID_t CEffectID)

{
    Assert(pOusters != NULL);
    Assert(pOustersSkillSlot != NULL);

    BYTE Grade = 0;
    if (pOustersSkillSlot->getExpLevel() < 15)
        Grade = 0;
    else if (pOustersSkillSlot->getExpLevel() < 30)
        Grade = 1;
    else
        Grade = 2;

    try {
        Player* pPlayer = pOusters->getPlayer();
        Zone* pZone = pOusters->getZone();

        Assert(pPlayer != NULL);
        Assert(pZone != NULL);

        Item* pWeapon = pOusters->getWearItem(Ousters::WEAR_RIGHTHAND);

        if (pWeapon == NULL || pWeapon->getItemClass() != Item::ITEM_CLASS_OUSTERS_CHAKRAM ||
            !pOusters->isRealWearingEx(Ousters::WEAR_RIGHTHAND)) {
            executeSkillFailException(pOusters, getSkillType(), Grade);
            return;
        }

        GCSkillToSelfOK1 _GCSkillToSelfOK1;
        GCSkillToSelfOK2 _GCSkillToSelfOK2;

        SkillType_t SkillType = pOustersSkillSlot->getSkillType();
        SkillInfo* pSkillInfo = de::gameContext().skillInfos().getSkillInfo(SkillType);

        int RequiredMP = (int)pSkillInfo->getConsumeMP();
        bool bManaCheck = hasEnoughMana(pOusters, RequiredMP);
        bool bTimeCheck = verifyRunTime(pOustersSkillSlot);
        bool bRangeCheck = checkZoneLevelToUseSkill(pOusters);
        bool bHitRoll = HitRoll::isSuccessMagic(pOusters, pSkillInfo, pOustersSkillSlot);
        bool bEffected = pOusters->isFlag(Effect::EFFECT_CLASS_CROSS_GUARD);
        bool bSatisfyRequire = pOusters->satisfySkillRequire(pSkillInfo);

        if (bManaCheck && bTimeCheck && bRangeCheck && bHitRoll && !bEffected && bSatisfyRequire) {
            decreaseMana(pOusters, RequiredMP, _GCSkillToSelfOK1);

            // Compute the duration.
            SkillInput input(pOusters, pOustersSkillSlot);
            SkillOutput output;
            computeOutput(input, output);

            // Create the effect class and attach it.
            EffectCrossGuard* pEffect = new EffectCrossGuard(pOusters);
            pEffect->setDeadline(output.Duration);
            pEffect->setBonus(output.Damage);
            pOusters->addEffect(pEffect);
            pOusters->setFlag(Effect::EFFECT_CLASS_CROSS_GUARD);

            OUSTERS_RECORD prev;
            pOusters->getOustersRecord(prev);
            pOusters->initAllStat();
            pOusters->addModifyInfo(prev, _GCSkillToSelfOK1);

            _GCSkillToSelfOK1.setSkillType(SkillType);
            _GCSkillToSelfOK1.setCEffectID(CEffectID);
            _GCSkillToSelfOK1.setDuration(output.Duration);
            _GCSkillToSelfOK1.setGrade(Grade);

            _GCSkillToSelfOK2.setObjectID(pOusters->getObjectID());
            _GCSkillToSelfOK2.setSkillType(SkillType);
            _GCSkillToSelfOK2.setDuration(output.Duration);
            _GCSkillToSelfOK2.setGrade(Grade);

            pPlayer->sendPacket(&_GCSkillToSelfOK1);
            pZone->broadcastPacket(pOusters->getX(), pOusters->getY(), &_GCSkillToSelfOK2, pOusters);

            GCAddEffect gcAddEffect;
            gcAddEffect.setObjectID(pOusters->getObjectID());
            gcAddEffect.setEffectID(pEffect->getEffectClass());
            gcAddEffect.setDuration(output.Duration);
            pZone->broadcastPacket(pOusters->getX(), pOusters->getY(), &gcAddEffect);

            pOustersSkillSlot->setRunTime(output.Delay);
        } else {
            executeSkillFailNormal(pOusters, getSkillType(), NULL, Grade);
        }
    } catch (Throwable& t) {
        executeSkillFailException(pOusters, getSkillType(), Grade);
    }
}

CrossGuard g_CrossGuard;
