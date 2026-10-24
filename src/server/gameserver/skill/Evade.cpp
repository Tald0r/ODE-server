//////////////////////////////////////////////////////////////////////////////
// Filename    : Evade.cpp
// Written by  :
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "Evade.h"

#include "EffectEvade.h"
#include "GCAddEffect.h"
#include "GCSkillToSelfOK1.h"
#include "GCSkillToSelfOK2.h"
#include "GameContext.h"

//////////////////////////////////////////////////////////////////////////////
// Slayer self handler
//////////////////////////////////////////////////////////////////////////////
void Evade::execute(Ousters* pOusters, OustersSkillSlot* pOustersSkillSlot, CEffectID_t CEffectID)

{
    Assert(pOusters != NULL);
    Assert(pOustersSkillSlot != NULL);

    try {
        Player* pPlayer = pOusters->getPlayer();
        Zone* pZone = pOusters->getZone();

        Assert(pPlayer != NULL);
        Assert(pZone != NULL);

        GCSkillToSelfOK1 _GCSkillToSelfOK1;
        GCSkillToSelfOK2 _GCSkillToSelfOK2;

        Item* pWeapon = pOusters->getWearItem(Ousters::WEAR_RIGHTHAND);
        if (pWeapon == NULL || pWeapon->getItemClass() != Item::ITEM_CLASS_OUSTERS_CHAKRAM ||
            !pOusters->isRealWearingEx(Ousters::WEAR_RIGHTHAND)) {
            executeSkillFailException(pOusters, getSkillType());
            return;
        }

        SkillType_t SkillType = pOustersSkillSlot->getSkillType();
        SkillInfo* pSkillInfo = de::gameContext().skillInfos().getSkillInfo(SkillType);

        int RequiredMP = (int)pSkillInfo->getConsumeMP();
        bool bManaCheck = hasEnoughMana(pOusters, RequiredMP);
        bool bTimeCheck = verifyRunTime(pOustersSkillSlot);
        bool bRangeCheck = checkZoneLevelToUseSkill(pOusters);
        bool bHitRoll = HitRoll::isSuccessMagic(pOusters, pSkillInfo, pOustersSkillSlot);
        bool bEffected = pOusters->isFlag(Effect::EFFECT_CLASS_EVADE);
        bool bSatisfyRequire = pOusters->satisfySkillRequire(pSkillInfo);

        if (bManaCheck && bTimeCheck && bRangeCheck && bHitRoll && !bEffected && bSatisfyRequire) {
            decreaseMana(pOusters, RequiredMP, _GCSkillToSelfOK1);

            // Compute the duration.
            SkillInput input(pOusters, pOustersSkillSlot);
            SkillOutput output;
            computeOutput(input, output);

            // Create the effect class and attach it.
            EffectEvade* pEffect = new EffectEvade(pOusters);
            pEffect->setDeadline(output.Duration);
            pEffect->setBonus(output.Damage);
            pOusters->addEffect(pEffect);
            pOusters->setFlag(Effect::EFFECT_CLASS_EVADE);

            OUSTERS_RECORD prev;
            pOusters->getOustersRecord(prev);
            pOusters->initAllStat();
            pOusters->addModifyInfo(prev, _GCSkillToSelfOK1);

            _GCSkillToSelfOK1.setSkillType(SkillType);
            _GCSkillToSelfOK1.setCEffectID(CEffectID);
            _GCSkillToSelfOK1.setDuration(output.Duration);

            _GCSkillToSelfOK2.setObjectID(pOusters->getObjectID());
            _GCSkillToSelfOK2.setSkillType(SkillType);
            _GCSkillToSelfOK2.setDuration(output.Duration);

            pPlayer->sendPacket(&_GCSkillToSelfOK1);
            pZone->broadcastPacket(pOusters->getX(), pOusters->getY(), &_GCSkillToSelfOK2, pOusters);

            GCAddEffect gcAddEffect;
            gcAddEffect.setObjectID(pOusters->getObjectID());
            gcAddEffect.setEffectID(pEffect->getEffectClass());
            gcAddEffect.setDuration(output.Duration);
            pZone->broadcastPacket(pOusters->getX(), pOusters->getY(), &gcAddEffect, pOusters);

            pOustersSkillSlot->setRunTime(output.Delay);
        } else {
            executeSkillFailNormal(pOusters, getSkillType(), NULL);
        }
    } catch (Throwable& t) {
        executeSkillFailException(pOusters, getSkillType());
    }
}

Evade g_Evade;
