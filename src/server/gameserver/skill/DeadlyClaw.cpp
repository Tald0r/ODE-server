//////////////////////////////////////////////////////////////////////////////
// Filename    : DeadlyClaw.cpp
// Written by  : excel96
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "DeadlyClaw.h"

#include "RankBonus.h"
#include "SimpleTileMeleeSkill.h"

//////////////////////////////////////////////////////////////////////////////
// Constructor
// Initializes the mask.
//////////////////////////////////////////////////////////////////////////////
DeadlyClaw::DeadlyClaw() {
    m_pDeadlyClawMask[0].set(0, 0);
    m_pDeadlyClawMask[1].set(-1, -1);
    m_pDeadlyClawMask[2].set(0, -1);
    m_pDeadlyClawMask[3].set(1, -1);
    m_pDeadlyClawMask[4].set(-1, 0);
    m_pDeadlyClawMask[5].set(1, 0);
    m_pDeadlyClawMask[6].set(-1, 1);
    m_pDeadlyClawMask[7].set(0, 1);
    m_pDeadlyClawMask[8].set(1, 1);
}


//////////////////////////////////////////////////////////////////////////////
// Vampire object handler
//////////////////////////////////////////////////////////////////////////////
void DeadlyClaw::execute(Vampire* pVampire, ObjectID_t TargetObjectID, VampireSkillSlot* pVampireSkillSlot,
                         CEffectID_t CEffectID)

{
    Assert(pVampire != NULL);
    Assert(pVampireSkillSlot != NULL);

    try {
        Zone* pZone = pVampire->getZone();
        Assert(pZone != NULL);

        Creature* pTargetCreature = pZone->getCreature(TargetObjectID);

        // A missing target fails the skill instead of throwing.
        if (pTargetCreature == NULL) {
            executeSkillFailException(pVampire, getSkillType());

            return;
        }

        execute(pVampire, pTargetCreature->getX(), pTargetCreature->getY(), pVampireSkillSlot, CEffectID);
    } catch (Throwable& t) {
        executeSkillFailException(pVampire, getSkillType());
    }
}

//////////////////////////////////////////////////////////////////////////////
// Vampire tile handler
//////////////////////////////////////////////////////////////////////////////
void DeadlyClaw::execute(Vampire* pVampire, ZoneCoord_t X, ZoneCoord_t Y, VampireSkillSlot* pVampireSkillSlot,
                         CEffectID_t CEffectID)

{
    SkillInput input(pVampire);
    SkillOutput output;
    computeOutput(input, output);

    SIMPLE_SKILL_INPUT param;
    param.SkillType = getSkillType();
    param.SkillDamage = output.Damage;
    param.Delay = 0;
    param.ItemClass = Item::ITEM_CLASS_MAX;
    param.STRMultiplier = 50;
    param.DEXMultiplier = 0;
    param.INTMultiplier = 0;
    param.bMagicHitRoll = false;
    param.bMagicDamage = false;
    param.bAdd = true;

    SIMPLE_SKILL_OUTPUT result;

    for (int i = 0; i < 9; i++) {
        param.addMask(m_pDeadlyClawMask[i].x, m_pDeadlyClawMask[i].y, 100);
    }

    g_SimpleTileMeleeSkill.execute(pVampire, X, Y, pVampireSkillSlot, param, result, CEffectID);
}

//////////////////////////////////////////////////////////////////////////////
// Monster tile handler
//////////////////////////////////////////////////////////////////////////////
void DeadlyClaw::execute(Monster* pMonster, ZoneCoord_t X, ZoneCoord_t Y)

{
    SkillInput input(pMonster);
    SkillOutput output;
    computeOutput(input, output);

    SIMPLE_SKILL_INPUT param;
    param.SkillType = getSkillType();
    param.SkillDamage = output.Damage;
    param.Delay = 0;
    param.ItemClass = Item::ITEM_CLASS_MAX;
    param.STRMultiplier = 50;
    param.DEXMultiplier = 0;
    param.INTMultiplier = 0;
    param.bMagicHitRoll = false;
    param.bMagicDamage = false;
    param.bAdd = true;

    SIMPLE_SKILL_OUTPUT result;

    for (int i = 0; i < 9; i++) {
        param.addMask(m_pDeadlyClawMask[i].x, m_pDeadlyClawMask[i].y, 100);
    }

    g_SimpleTileMeleeSkill.execute(pMonster, X, Y, param, result);
}

DeadlyClaw g_DeadlyClaw;
