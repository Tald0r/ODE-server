//////////////////////////////////////////////////////////////////////////////
// Filename    : ThunderBolt.cpp
// Written by  : elca@ewestsoft.com
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "ThunderBolt.h"

#include "SimpleMeleeSkill.h"
#include "SimpleTileMissileSkill.h"

void ThunderBolt::execute(Slayer* pSlayer, ZoneCoord_t X, ZoneCoord_t Y, SkillSlot* pSkillSlot, CEffectID_t CEffectID)

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

    param.addMask(0, 0, 100);
    param.addMask(-1, -1, 100);
    param.addMask(0, -1, 100);
    param.addMask(1, -1, 100);
    param.addMask(-1, 0, 100);
    param.addMask(1, 0, 100);
    param.addMask(-1, 1, 100);
    param.addMask(0, 1, 100);
    param.addMask(1, 1, 100);

    g_SimpleTileMissileSkill.execute(pSlayer, X, Y, pSkillSlot, param, result);
}

//////////////////////////////////////////////////////////////////////////////
// Slayer object handler
//////////////////////////////////////////////////////////////////////////////
void ThunderBolt::execute(Slayer* pSlayer, ObjectID_t TargetObjectID, SkillSlot* pSkillSlot, CEffectID_t CEffectID)

{
    try {
        Zone* pZone = pSlayer->getZone();
        Assert(pZone != NULL);

        Creature* pTargetCreature = pZone->getCreature(TargetObjectID);

        // A missing target fails the skill instead of throwing.
        if (pTargetCreature == NULL) {
            executeSkillFailException(pSlayer, getSkillType());
            return;
        }

        ZoneCoord_t X = pTargetCreature->getX();
        ZoneCoord_t Y = pTargetCreature->getY();


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

        param.addMask(0, 0, 100);
        param.addMask(-1, -1, 100);
        param.addMask(0, -1, 100);
        param.addMask(1, -1, 100);
        param.addMask(-1, 0, 100);
        param.addMask(1, 0, 100);
        param.addMask(-1, 1, 100);
        param.addMask(0, 1, 100);
        param.addMask(1, 1, 100);

        g_SimpleTileMissileSkill.execute(pSlayer, X, Y, pSkillSlot, param, result);

    } catch (Throwable& t) {
        executeSkillFailException(pSlayer, getSkillType());
    }
}

ThunderBolt g_ThunderBolt;
