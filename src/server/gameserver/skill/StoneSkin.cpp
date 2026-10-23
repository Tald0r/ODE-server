//////////////////////////////////////////////////////////////////////////////
// Filename    : StoneSkin.cpp
// Written by  :
// Description :
//////////////////////////////////////////////////////////////////////////////
#include "StoneSkin.h"

#include "EffectStoneSkin.h"
#include "GCAddEffect.h"
#include "GCSkillToObjectOK1.h"
#include "GCSkillToObjectOK2.h"
#include "GCSkillToObjectOK3.h"
#include "GCSkillToObjectOK4.h"
#include "GCSkillToObjectOK5.h"
#include "GCSkillToSelfOK1.h"
#include "GCSkillToSelfOK2.h"
#include "GameContext.h"
#include "HitRoll.h"
#include "RankBonus.h"

//////////////////////////////////////////////////////////////////////////////
// Slayer self handler
//////////////////////////////////////////////////////////////////////////////
void StoneSkin::execute(Vampire* pVampire, VampireSkillSlot* pVampireSkillSlot, CEffectID_t CEffectID)

{
    Assert(pVampire != NULL);
    Assert(pVampireSkillSlot != NULL);

    try {
        Player* pPlayer = pVampire->getPlayer();
        Zone* pZone = pVampire->getZone();

        Assert(pPlayer != NULL);
        Assert(pZone != NULL);

        GCSkillToSelfOK1 _GCSkillToSelfOK1;
        GCSkillToSelfOK2 _GCSkillToSelfOK2;

        SkillType_t SkillType = pVampireSkillSlot->getSkillType();
        SkillInfo* pSkillInfo = de::gameContext().skillInfos().getSkillInfo(SkillType);

        // Hit bonus when Knowledge of Curse is present
        int HitBonus = 0;
        if (pVampire->hasRankBonus(RankBonus::RANK_BONUS_KNOWLEDGE_OF_INNATE)) {
            RankBonus* pRankBonus = pVampire->getRankBonus(RankBonus::RANK_BONUS_KNOWLEDGE_OF_INNATE);
            Assert(pRankBonus != NULL);

            HitBonus = pRankBonus->getPoint();
        }

        int RequiredMP = (int)pSkillInfo->getConsumeMP();
        bool bManaCheck = hasEnoughMana(pVampire, RequiredMP);
        bool bTimeCheck = verifyRunTime(pVampireSkillSlot);
        bool bRangeCheck = checkZoneLevelToUseSkill(pVampire);
        bool bHitRoll = HitRoll::isSuccessMagic(pVampire, pSkillInfo, pVampireSkillSlot, HitBonus);
        bool bEffected = pVampire->isFlag(Effect::EFFECT_CLASS_STONE_SKIN);

        if (bManaCheck && bTimeCheck && bRangeCheck && bHitRoll && !bEffected) {
            decreaseMana(pVampire, RequiredMP, _GCSkillToSelfOK1);

            // The damage bonus varies with the skill level.
            SkillInput input(pVampire);
            SkillOutput output;
            input.SkillLevel = pVampire->getSTR() + pVampire->getDEX() + pVampire->getINT();
            input.DomainLevel = pVampire->getLevel();
            computeOutput(input, output);

            // Create the effect class and attach it.
            EffectStoneSkin* pEffect = new EffectStoneSkin(pVampire);
            pEffect->setDeadline(output.Duration);
            pEffect->setBonus(output.Damage);
            pVampire->addEffect(pEffect);
            pVampire->setFlag(Effect::EFFECT_CLASS_STONE_SKIN);

            // Send the stats that this changes.
            VAMPIRE_RECORD prev;
            pVampire->getVampireRecord(prev);
            pVampire->initAllStat();
            pVampire->sendRealWearingInfo();
            pVampire->sendModifyInfo(prev);

            // Build the packet and send it.
            _GCSkillToSelfOK1.setSkillType(SkillType);
            _GCSkillToSelfOK1.setCEffectID(CEffectID);
            _GCSkillToSelfOK1.setDuration(output.Duration);

            _GCSkillToSelfOK2.setObjectID(pVampire->getObjectID());
            _GCSkillToSelfOK2.setSkillType(SkillType);
            _GCSkillToSelfOK2.setDuration(output.Duration);

            pPlayer->sendPacket(&_GCSkillToSelfOK1);

            pZone->broadcastPacket(pVampire->getX(), pVampire->getY(), &_GCSkillToSelfOK2, pVampire);

            // Notifies that the effect has been attached.
            GCAddEffect gcAddEffect;
            gcAddEffect.setObjectID(pVampire->getObjectID());
            gcAddEffect.setEffectID(Effect::EFFECT_CLASS_STONE_SKIN);
            gcAddEffect.setDuration(output.Duration);
            pZone->broadcastPacket(pVampire->getX(), pVampire->getY(), &gcAddEffect);

            // set Next Run Time
            pVampireSkillSlot->setRunTime(output.Delay);
        } else {
            executeSkillFailNormal(pVampire, getSkillType(), NULL);
        }
    } catch (Throwable& t) {
        executeSkillFailException(pVampire, getSkillType());
    }
}

StoneSkin g_StoneSkin;
