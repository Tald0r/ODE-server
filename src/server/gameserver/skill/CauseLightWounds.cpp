//////////////////////////////////////////////////////////////////////////////
// Filename    : CauseLightWounds.cpp
// Written by  :
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "CauseLightWounds.h"

#include "RankBonus.h"
#include "SimpleMissileSkill.h"

//////////////////////////////////////////////////////////////////////////////
// Slayer object handler
//////////////////////////////////////////////////////////////////////////////
void CauseLightWounds::execute(Slayer* pSlayer, ObjectID_t TargetObjectID, SkillSlot* pSkillSlot, CEffectID_t CEffectID)

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

    // Soul Smashing adds its rank bonus to the damage.
    if (pSlayer->hasRankBonus(RankBonus::RANK_BONUS_SOUL_SMASHING)) {
        RankBonus* pRankBonus = pSlayer->getRankBonus(RankBonus::RANK_BONUS_SOUL_SMASHING);
        Assert(pRankBonus != NULL);

        param.SkillDamage += pRankBonus->getPoint();
    }


    g_SimpleMissileSkill.execute(pSlayer, TargetObjectID, pSkillSlot, param, result);
}

CauseLightWounds g_CauseLightWounds;
