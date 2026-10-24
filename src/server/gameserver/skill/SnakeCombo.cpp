//////////////////////////////////////////////////////////////////////////////
// Filename    : SnakeCombo.cpp
// Written by  : elca@ewestsoft.com
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "SnakeCombo.h"

#include "SimpleMeleeSkill.h"

//////////////////////////////////////////////////////////////////////////////
// Slayer object handler
//////////////////////////////////////////////////////////////////////////////
void SnakeCombo::execute(Slayer* pSlayer, ObjectID_t TargetObjectID, SkillSlot* pSkillSlot, CEffectID_t CEffectID)

{
    SkillInput input(pSlayer, pSkillSlot);
    SkillOutput output;
    computeOutput(input, output);

    SIMPLE_SKILL_INPUT param;
    param.SkillType = getSkillType();
    param.SkillDamage = output.Damage;
    param.Delay = output.Delay;
    param.ItemClass = Item::ITEM_CLASS_SWORD;
    param.STRMultiplier = 8;
    param.DEXMultiplier = 1;
    param.INTMultiplier = 1;
    param.bMagicHitRoll = false;
    param.bMagicDamage = false;
    param.bAdd = true;

    SIMPLE_SKILL_OUTPUT result;

    g_SimpleMeleeSkill.execute(pSlayer, TargetObjectID, pSkillSlot, param, result);
}

SnakeCombo g_SnakeCombo;
