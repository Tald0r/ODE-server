//////////////////////////////////////////////////////////////////////////////
// Filename    : Invisibility.cpp
// Written by  : elca@ewestsoft.com
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "Invisibility.h"

#include "EffectFadeOut.h"
#include "GCDeleteObject.h"
#include "GCSkillToSelfOK1.h"
#include "GCSkillToSelfOK2.h"
#include "GameContext.h"
#include "Monster.h"
#include "RankBonus.h"

//////////////////////////////////////////////////////////////////////////////
// Vampire self
//////////////////////////////////////////////////////////////////////////////
void Invisibility::execute(Vampire* pVampire, VampireSkillSlot* pSkillSlot, CEffectID_t CEffectID)

{
    Assert(pVampire != NULL);
    Assert(pSkillSlot != NULL);

    try {
        Player* pPlayer = pVampire->getPlayer();
        Zone* pZone = pVampire->getZone();

        Assert(pPlayer != NULL);
        Assert(pZone != NULL);

        GCSkillToSelfOK1 _GCSkillToSelfOK1;
        GCSkillToSelfOK2 _GCSkillToSelfOK2;

        SkillType_t SkillType = pSkillSlot->getSkillType();
        SkillInfo* pSkillInfo = de::gameContext().skillInfos().getSkillInfo(SkillType);

        ZoneCoord_t x = pVampire->getX();
        ZoneCoord_t y = pVampire->getY();

        // Knowledge of Innate gives a hit bonus of 10.
        int HitBonus = 0;
        if (pVampire->hasRankBonus(RankBonus::RANK_BONUS_KNOWLEDGE_OF_INNATE)) {
            RankBonus* pRankBonus = pVampire->getRankBonus(RankBonus::RANK_BONUS_KNOWLEDGE_OF_INNATE);
            Assert(pRankBonus != NULL);

            HitBonus = pRankBonus->getPoint();
        }

        Tile& rTile = pZone->getTile(pVampire->getX(), pVampire->getY());

        int RequiredMP = decreaseConsumeMP(pVampire, pSkillInfo);
        bool bManaCheck = hasEnoughMana(pVampire, RequiredMP);
        bool bTimeCheck = verifyRunTime(pSkillSlot);
        bool bRangeCheck = checkZoneLevelToUseSkill(pVampire);
        bool bHitRoll = HitRoll::isSuccessMagic(pVampire, pSkillInfo, pSkillSlot, HitBonus);
        bool bEffected = pVampire->isFlag(Effect::EFFECT_CLASS_INVISIBILITY) ||
                         pVampire->isFlag(Effect::EFFECT_CLASS_FADE_OUT) || pVampire->hasRelicItem() ||
                         pVampire->isFlag(Effect::EFFECT_CLASS_HAS_FLAG) ||
                         pVampire->isFlag(Effect::EFFECT_CLASS_HAS_SWEEPER) ||
                         rTile.getEffect(Effect::EFFECT_CLASS_TRYING_POSITION) != NULL;

        if (bManaCheck && bTimeCheck && bRangeCheck && bHitRoll && !bEffected) {
            decreaseMana(pVampire, RequiredMP, _GCSkillToSelfOK1);

            SkillInput input(pVampire);
            SkillOutput output;
            computeOutput(input, output);

            // Create the fade-out effect and attach it.
            // The actual disappearance happens inside that effect.
            // (It is attached because the vampire does not vanish all at once.)
            EffectFadeOut* pEffect = new EffectFadeOut(pVampire);
            pEffect->setDuration(output.Duration);
            pEffect->setDeadline(40);
            pEffect->setInvisibility();
            pVampire->addEffect(pEffect);
            pVampire->setFlag(Effect::EFFECT_CLASS_FADE_OUT);

            _GCSkillToSelfOK1.setSkillType(SkillType);
            _GCSkillToSelfOK1.setCEffectID(CEffectID);
            _GCSkillToSelfOK1.setDuration(0);

            _GCSkillToSelfOK2.setObjectID(pVampire->getObjectID());
            _GCSkillToSelfOK2.setSkillType(SkillType);
            _GCSkillToSelfOK2.setDuration(0);

            // Send Packet
            pPlayer->sendPacket(&_GCSkillToSelfOK1);

            pZone->broadcastPacket(x, y, &_GCSkillToSelfOK2, pVampire);

            pSkillSlot->setRunTime(output.Delay);
        } else {
            executeSkillFailNormal(pVampire, getSkillType(), NULL);
        }
    } catch (Throwable& t) {
        executeSkillFailException(pVampire, getSkillType());
    }
}

//////////////////////////////////////////////////////////////////////////////
// Monster self handler
//////////////////////////////////////////////////////////////////////////////
void Invisibility::execute(Monster* pMonster)

{
    Assert(pMonster != NULL);

    try {
        Zone* pZone = pMonster->getZone();

        Assert(pZone != NULL);

        GCSkillToSelfOK2 _GCSkillToSelfOK2;

        if (pMonster->isFlag(Effect::EFFECT_CLASS_HIDE)) {
            return;
        }

        SkillType_t SkillType = SKILL_INVISIBILITY;
        SkillInfo* pSkillInfo = de::gameContext().skillInfos().getSkillInfo(SkillType);

        ZoneCoord_t x = pMonster->getX();
        ZoneCoord_t y = pMonster->getY();

        bool bRangeCheck = checkZoneLevelToUseSkill(pMonster);
        bool bHitRoll = HitRoll::isSuccessMagic(pMonster, pSkillInfo);
        bool bEffected =
            (pMonster->isFlag(Effect::EFFECT_CLASS_INVISIBILITY) || pMonster->isFlag(Effect::EFFECT_CLASS_FADE_OUT));

        if (bRangeCheck && bHitRoll && !bEffected) {
            SkillInput input(pMonster);
            SkillOutput output;
            computeOutput(input, output);

            // Create the fade-out effect and attach it.
            // The actual disappearance happens inside that effect.
            // (It is attached because the vampire does not vanish all at once.)
            EffectFadeOut* pEffect = new EffectFadeOut(pMonster);
            pEffect->setDuration(output.Duration);
            pEffect->setDeadline(40);
            pEffect->setInvisibility();
            pMonster->addEffect(pEffect);
            pMonster->setFlag(Effect::EFFECT_CLASS_FADE_OUT);

            _GCSkillToSelfOK2.setObjectID(pMonster->getObjectID());
            _GCSkillToSelfOK2.setDuration(0);
            _GCSkillToSelfOK2.setSkillType(SkillType);

            pZone->broadcastPacket(x, y, &_GCSkillToSelfOK2);
        } else {
            executeSkillFailNormal(pMonster, getSkillType(), NULL);
        }
    } catch (Throwable& t) {
        executeSkillFailException(pMonster, getSkillType());
    }
}

Invisibility g_Invisibility;
