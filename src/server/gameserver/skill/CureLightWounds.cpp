//////////////////////////////////////////////////////////////////////////////
// Filename    : CureLightWounds.cpp
// Written by  : elca@ewestsoft.com
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "CureLightWounds.h"

#include "SimpleCureSkill.h"

const uint LightBloodDrainLevel = 24;

//////////////////////////////////////////////////////////////////////////////
// Slayer object handler
//////////////////////////////////////////////////////////////////////////////
void CureLightWounds::execute(Slayer* pSlayer, ObjectID_t TargetObjectID, SkillSlot* pSkillSlot, CEffectID_t CEffectID)

{
    SkillInput input(pSlayer, pSkillSlot);
    SkillOutput output;
    input.TargetType = SkillInput::TARGET_OTHER;
    computeOutput(input, output);

    SIMPLE_SKILL_INPUT param;
    param.SkillType = getSkillType();
    param.SkillDamage = output.Damage;
    param.Delay = output.Delay;
    param.STRMultiplier = 1;
    param.DEXMultiplier = 1;
    param.INTMultiplier = 8;
    param.Level = LightBloodDrainLevel;

    SIMPLE_SKILL_OUTPUT result;

    g_SimpleCureSkill.execute(pSlayer, TargetObjectID, pSkillSlot, param, result);
}

//////////////////////////////////////////////////////////////////////////////
// Slayer self handler
//////////////////////////////////////////////////////////////////////////////
void CureLightWounds::execute(Slayer* pSlayer, SkillSlot* pSkillSlot, CEffectID_t CEffectID)

{
    SkillInput input(pSlayer, pSkillSlot);
    SkillOutput output;
    input.TargetType = SkillInput::TARGET_SELF;
    computeOutput(input, output);

    SIMPLE_SKILL_INPUT param;
    param.SkillType = getSkillType();
    param.SkillDamage = output.Damage;
    param.Delay = output.Delay;
    param.STRMultiplier = 1;
    param.DEXMultiplier = 1;
    param.INTMultiplier = 8;
    param.Level = LightBloodDrainLevel;

    SIMPLE_SKILL_OUTPUT result;

    g_SimpleCureSkill.execute(pSlayer, pSkillSlot, param, result);
}

CureLightWounds g_CureLightWounds;
