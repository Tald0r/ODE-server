//////////////////////////////////////////////////////////////////////////////
// Filename    : CureSeriousWounds.cpp
// Written by  : elca@ewestsoft.com
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "CureSeriousWounds.h"

#include "SimpleTileCureSkill.h"

const uint SeriousBloodDrainLevel = 49;

//////////////////////////////////////////////////////////////////////////////
// Slayer object handler
//////////////////////////////////////////////////////////////////////////////
void CureSeriousWounds::execute(Slayer* pSlayer, ObjectID_t TargetObjectID, SkillSlot* pSkillSlot,
                                CEffectID_t CEffectID)

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
    param.Level = SeriousBloodDrainLevel;

    SIMPLE_SKILL_OUTPUT result;

    g_SimpleTileCureSkill.execute(pSlayer, TargetObjectID, pSkillSlot, param, result);
}

//////////////////////////////////////////////////////////////////////////////
// Slayer self handler
//////////////////////////////////////////////////////////////////////////////
void CureSeriousWounds::execute(Slayer* pSlayer, SkillSlot* pSkillSlot, CEffectID_t CEffectID)

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
    param.Level = SeriousBloodDrainLevel;

    SIMPLE_SKILL_OUTPUT result;

    g_SimpleTileCureSkill.execute(pSlayer, pSkillSlot, param, result);
}

void CureSeriousWounds::execute(Slayer* pSlayer, ZoneCoord_t X, ZoneCoord_t Y, SkillSlot* pSkillSlot,
                                CEffectID_t CEffectID)

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
    param.Level = SeriousBloodDrainLevel;

    SIMPLE_SKILL_OUTPUT result;

    g_SimpleTileCureSkill.execute(pSlayer, X, Y, pSkillSlot, param, result);
}

CureSeriousWounds g_CureSeriousWounds;
