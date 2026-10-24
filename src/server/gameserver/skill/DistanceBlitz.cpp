//////////////////////////////////////////////////////////////////////////////
// Filename    : DistanceBlitz.cpp
// Written by  :
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "DistanceBlitz.h"

#include "RankBonus.h"
#include "SimpleTileMissileSkill.h"

//////////////////////////////////////////////////////////////////////////////
// Vampire object handler
//////////////////////////////////////////////////////////////////////////////
void DistanceBlitz::execute(Ousters* pOusters, ObjectID_t TargetObjectID, OustersSkillSlot* pOustersSkillSlot,
                            CEffectID_t CEffectID)

{
    Assert(pOusters != NULL);
    Assert(pOustersSkillSlot != NULL);

    try {
        Zone* pZone = pOusters->getZone();
        Assert(pZone != NULL);

        Creature* pTargetCreature = pZone->getCreature(TargetObjectID);

        // A missing target fails the skill instead of throwing.
        if (pTargetCreature == NULL || !canAttack(pOusters, pTargetCreature)) {
            executeSkillFailException(pOusters, getSkillType());
            return;
        }

        execute(pOusters, pTargetCreature->getX(), pTargetCreature->getY(), pOustersSkillSlot, CEffectID);
    } catch (Throwable& t) {
        executeSkillFailException(pOusters, getSkillType());
    }
}

void DistanceBlitz::execute(Ousters* pOusters, ZoneCoord_t X, ZoneCoord_t Y, OustersSkillSlot* pOustersSkillSlot,
                            CEffectID_t CEffectID) {
    SkillInput input(pOusters, pOustersSkillSlot);
    SkillOutput output;
    computeOutput(input, output);

    SIMPLE_SKILL_INPUT param;
    param.SkillType = getSkillType();
    param.SkillDamage = output.Damage;
    param.Delay = output.Delay;
    param.ItemClass = Item::ITEM_CLASS_OUSTERS_WRISTLET;
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

    param.addMask(0, 0, 100);

    param.addMask(1, 0, 75);
    param.addMask(-1, 0, 75);
    param.addMask(0, 1, 75);
    param.addMask(0, -1, 75);
    param.addMask(1, 1, 75);
    param.addMask(1, -1, 75);
    param.addMask(-1, 1, 75);
    param.addMask(-1, -1, 75);

    param.addMask(-2, -2, 50);
    param.addMask(-2, -1, 50);
    param.addMask(-2, 0, 50);
    param.addMask(-2, 1, 50);
    param.addMask(-2, 2, 50);
    param.addMask(-1, 2, 50);
    param.addMask(0, 2, 50);
    param.addMask(1, 2, 50);
    param.addMask(2, 2, 50);
    param.addMask(2, 1, 50);
    param.addMask(2, 0, 50);
    param.addMask(2, -1, 50);
    param.addMask(2, -2, 50);
    param.addMask(1, -2, 50);
    param.addMask(0, -2, 50);
    param.addMask(-1, -2, 50);

    g_SimpleTileMissileSkill.execute(pOusters, X, Y, pOustersSkillSlot, param, result, CEffectID);
}

DistanceBlitz g_DistanceBlitz;
