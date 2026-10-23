//////////////////////////////////////////////////////////////////////////////
// Filename    : SharpChakram.cpp
// Written by  :
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "SharpChakram.h"

#include "EffectSharpChakram.h"
#include "GCAddEffect.h"
#include "GCSkillToObjectOK1.h"
#include "GCSkillToObjectOK2.h"
#include "GCSkillToObjectOK3.h"
#include "GCSkillToObjectOK4.h"
#include "GCSkillToObjectOK5.h"
#include "GCSkillToObjectOK6.h"
#include "GCSkillToSelfOK1.h"
#include "GCSkillToSelfOK2.h"
#include "GameContext.h"

//////////////////////////////////////////////////////////////////////////////
// Slayer self handler
//////////////////////////////////////////////////////////////////////////////
void SharpChakram::execute(Ousters* pOusters, OustersSkillSlot* pOustersSkillSlot, CEffectID_t CEffectID)

{
    Assert(pOusters != NULL);
    Assert(pOustersSkillSlot != NULL);

    try {
        Player* pPlayer = pOusters->getPlayer();
        Zone* pZone = pOusters->getZone();

        Assert(pPlayer != NULL);
        Assert(pZone != NULL);

        Item* pWeapon = pOusters->getWearItem(Ousters::WEAR_RIGHTHAND);
        if (pWeapon == NULL || pWeapon->getItemClass() != Item::ITEM_CLASS_OUSTERS_CHAKRAM ||
            !pOusters->isRealWearingEx(Ousters::WEAR_RIGHTHAND)) {
            executeSkillFailException(pOusters, pOustersSkillSlot->getSkillType());
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
        bool bEffected = pOusters->isFlag(Effect::EFFECT_CLASS_SHARP_CHAKRAM);
        bool bSatisfyRequire = pOusters->satisfySkillRequire(pSkillInfo);

        if (bManaCheck && bTimeCheck && bRangeCheck && bHitRoll && !bEffected && bSatisfyRequire) {
            decreaseMana(pOusters, RequiredMP, _GCSkillToSelfOK1);

            // Compute the duration.
            SkillInput input(pOusters, pOustersSkillSlot);
            SkillOutput output;
            computeOutput(input, output);

            // Create the effect class and attach it.
            EffectSharpChakram* pEffect = new EffectSharpChakram(pOusters);
            pEffect->setDeadline(output.Duration);
            pEffect->setBonus(output.Damage);
            pOusters->addEffect(pEffect);
            pOusters->setFlag(Effect::EFFECT_CLASS_SHARP_CHAKRAM);

            pOusters->initAllStatAndSend();

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
            pZone->broadcastPacket(pOusters->getX(), pOusters->getY(), &gcAddEffect);

            pOustersSkillSlot->setRunTime(output.Delay);
        } else {
            executeSkillFailNormal(pOusters, getSkillType(), NULL);
        }
    } catch (Throwable& t) {
        executeSkillFailException(pOusters, getSkillType());
    }
}

SharpChakram g_SharpChakram;
