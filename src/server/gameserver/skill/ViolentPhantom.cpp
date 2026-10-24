//////////////////////////////////////////////////////////////////////////////
// Filename    : ViolentPhantom.cpp
// Written by  : excel96
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "ViolentPhantom.h"

#include "RankBonus.h"
#include "SimpleTileMeleeSkill.h"

//////////////////////////////////////////////////////////////////////////////
// Constructor
// Initializes the mask.
//////////////////////////////////////////////////////////////////////////////
ViolentPhantom::ViolentPhantom() {
    m_pViolentPhantomMask[0].set(0, 0);
    m_pViolentPhantomMask[1].set(-1, -1);
    m_pViolentPhantomMask[2].set(0, -1);
    m_pViolentPhantomMask[3].set(1, -1);
    m_pViolentPhantomMask[4].set(-1, 0);
    m_pViolentPhantomMask[5].set(1, 0);
    m_pViolentPhantomMask[6].set(-1, 1);
    m_pViolentPhantomMask[7].set(0, 1);
    m_pViolentPhantomMask[8].set(1, 1);
}


//////////////////////////////////////////////////////////////////////////////
// Vampire object handler
//////////////////////////////////////////////////////////////////////////////
void ViolentPhantom::execute(Vampire* pVampire, ObjectID_t TargetObjectID, VampireSkillSlot* pVampireSkillSlot,
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
void ViolentPhantom::execute(Vampire* pVampire, ZoneCoord_t X, ZoneCoord_t Y, VampireSkillSlot* pVampireSkillSlot,
                             CEffectID_t CEffectID)

{
    SkillInput input(pVampire);
    SkillOutput output;
    computeOutput(input, output);

    SIMPLE_SKILL_INPUT param;
    param.SkillType = getSkillType();
    param.SkillDamage = output.Damage;
    param.Delay = output.Delay;
    param.ItemClass = Item::ITEM_CLASS_MAX;
    param.STRMultiplier = 0;
    param.DEXMultiplier = 0;
    param.INTMultiplier = 0;
    param.bMagicHitRoll = false;
    param.bMagicDamage = false;
    param.bAdd = true;

    SIMPLE_SKILL_OUTPUT result;

    for (int i = 0; i < 9; i++) {
        param.addMask(m_pViolentPhantomMask[i].x, m_pViolentPhantomMask[i].y, 100);
    }

    // Knowledge of Blood gives a hit bonus of 10.
    g_SimpleTileMeleeSkill.execute(pVampire, X, Y, pVampireSkillSlot, param, result, CEffectID);
}

//////////////////////////////////////////////////////////////////////////////
// Monster tile handler
//////////////////////////////////////////////////////////////////////////////
void ViolentPhantom::execute(Monster* pMonster, ZoneCoord_t X, ZoneCoord_t Y)

{
    SkillInput input(pMonster);
    SkillOutput output;
    computeOutput(input, output);

    SIMPLE_SKILL_INPUT param;
    param.SkillType = getSkillType();
    param.SkillDamage = output.Damage;
    param.Delay = output.Delay;
    param.ItemClass = Item::ITEM_CLASS_MAX;
    param.STRMultiplier = 0;
    param.DEXMultiplier = 0;
    param.INTMultiplier = 0;
    param.bMagicHitRoll = false;
    param.bMagicDamage = false;
    param.bAdd = true;

    SIMPLE_SKILL_OUTPUT result;

    for (int i = 0; i < 9; i++) {
        param.addMask(m_pViolentPhantomMask[i].x, m_pViolentPhantomMask[i].y, 100);
    }

    g_SimpleTileMeleeSkill.execute(pMonster, X, Y, param, result);
}

ViolentPhantom g_ViolentPhantom;
