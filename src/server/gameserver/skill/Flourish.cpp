//////////////////////////////////////////////////////////////////////////////
// Filename    : Flourish.cpp
// Written by  : elca@ewestsoft.com
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "Flourish.h"

#include "RankBonus.h"
#include "SimpleMeleeSkill.h"

//////////////////////////////////////////////////////////////////////////////
// Ousters object handler
//////////////////////////////////////////////////////////////////////////////
void Flourish::execute(Ousters* pOusters, ObjectID_t TargetObjectID, OustersSkillSlot* pOustersSkillSlot,
                       CEffectID_t CEffectID)

{
    SkillInput input(pOusters, pOustersSkillSlot);
    SkillOutput output;
    computeOutput(input, output);

    SIMPLE_SKILL_INPUT param;
    param.SkillType = getSkillType();
    param.SkillDamage = output.Damage;
    param.Delay = output.Delay;
    param.ItemClass = Item::ITEM_CLASS_OUSTERS_CHAKRAM;
    param.STRMultiplier = 0;
    param.DEXMultiplier = 0;
    param.INTMultiplier = 0;
    param.bMagicHitRoll = false;
    param.bMagicDamage = false;
    param.bAdd = true;

    if (input.SkillLevel < 15)
        param.Grade = 0;
    else if (input.SkillLevel < 30)
        param.Grade = 1;
    else
        param.Grade = 2;

    SIMPLE_SKILL_OUTPUT result;

    g_SimpleMeleeSkill.execute(pOusters, TargetObjectID, pOustersSkillSlot, param, result, CEffectID);
}


Flourish g_Flourish;
