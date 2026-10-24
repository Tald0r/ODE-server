//////////////////////////////////////////////////////////////////////////////
// Filename    : LightBall.cpp
// Written by  :
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "LightBall.h"

#include "RankBonus.h"
#include "SimpleMissileSkill.h"

//////////////////////////////////////////////////////////////////////////////
// Slayer object handler
//////////////////////////////////////////////////////////////////////////////
void LightBall::execute(Slayer* pSlayer, ObjectID_t TargetObjectID, SkillSlot* pSkillSlot, CEffectID_t CEffectID)

{
    SkillInput input(pSlayer, pSkillSlot);
    SkillOutput output;
    computeOutput(input, output);

    SIMPLE_SKILL_INPUT param;
    param.SkillType = getSkillType();
    param.SkillDamage = output.Damage;
    param.Delay = output.Delay;
    param.ItemClass = Item::ITEM_CLASS_MAX;
    param.STRMultiplier = 1;
    param.DEXMultiplier = 1;
    param.INTMultiplier = 8;
    param.bMagicHitRoll = true;
    param.bMagicDamage = false;
    param.bAdd = false;

    SIMPLE_SKILL_OUTPUT result;

    // Holy Smashing increases the damage.
    if (pSlayer->hasRankBonus(RankBonus::RANK_BONUS_HOLY_SMASHING)) {
        RankBonus* pRankBonus = pSlayer->getRankBonus(RankBonus::RANK_BONUS_HOLY_SMASHING);
        Assert(pRankBonus != NULL);

        param.SkillDamage += pRankBonus->getPoint();
    }

    g_SimpleMissileSkill.execute(pSlayer, TargetObjectID, pSkillSlot, param, result);
}

LightBall g_LightBall;
