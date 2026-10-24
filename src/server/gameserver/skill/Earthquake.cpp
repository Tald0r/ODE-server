//////////////////////////////////////////////////////////////////////////////
// Filename    : Earthquake.cpp
// Written by  : excel96
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "Earthquake.h"

#include "SimpleLinearMeleeSkill.h"

//////////////////////////////////////////////////////////////////////////////
// Slayer object handler
//////////////////////////////////////////////////////////////////////////////
void Earthquake::execute(Slayer* pSlayer, ObjectID_t TargetObjectID, SkillSlot* pSkillSlot, CEffectID_t CEffectID)

{
    Assert(pSlayer != NULL);
    Assert(pSkillSlot != NULL);

    try {
        Zone* pZone = pSlayer->getZone();
        Creature* pTargetCreature = pZone->getCreature(TargetObjectID);

        // A missing target fails the skill instead of throwing.
        if (pTargetCreature == NULL) {
            executeSkillFailException(pSlayer, getSkillType());
            return;
        }

        execute(pSlayer, pTargetCreature->getX(), pTargetCreature->getY(), pSkillSlot, CEffectID);
    } catch (Throwable& t) {
        executeSkillFailException(pSlayer, getSkillType());
    }
}

//////////////////////////////////////////////////////////////////////////////
// Slayer self handler
//////////////////////////////////////////////////////////////////////////////
void Earthquake::execute(Slayer* pSlayer, ZoneCoord_t X, ZoneCoord_t Y, SkillSlot* pSkillSlot, CEffectID_t CEffectID)

{
    SkillInput input(pSlayer, pSkillSlot);
    SkillOutput output;
    computeOutput(input, output);

    SIMPLE_SKILL_INPUT param;
    param.SkillType = getSkillType();
    param.SkillDamage = output.Damage;
    param.Delay = output.Delay;
    param.ItemClass = Item::ITEM_CLASS_BLADE;
    param.STRMultiplier = 8;
    param.DEXMultiplier = 1;
    param.INTMultiplier = 1;
    param.Level = 2;
    param.bMagicHitRoll = false;
    param.bMagicDamage = false;
    param.bAdd = true;

    SIMPLE_SKILL_OUTPUT result;

    g_SimpleLinearMeleeSkill.execute(pSlayer, X, Y, pSkillSlot, param, result);
}

Earthquake g_Earthquake;
