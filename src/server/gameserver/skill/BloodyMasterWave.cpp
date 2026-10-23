//////////////////////////////////////////////////////////////////////////////
// Filename    : BloodyMasterWave.cpp
// Written by  : excel96
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "BloodyMasterWave.h"

#include "SimpleTileMissileSkill.h"

//////////////////////////////////////////////////////////////////////////////
// Constructor
// Initializes the mask.
//////////////////////////////////////////////////////////////////////////////
BloodyMasterWave::BloodyMasterWave() {
    m_pBloodyMasterWaveMask[0].set(0, -3);
    m_pBloodyMasterWaveMask[1].set(-3, 0);
    m_pBloodyMasterWaveMask[2].set(3, 0);
    m_pBloodyMasterWaveMask[3].set(0, 3);

    m_pBloodyMasterWaveMask[4].set(-2, -1);
    m_pBloodyMasterWaveMask[5].set(-2, 1);
    m_pBloodyMasterWaveMask[6].set(-1, -2);
    m_pBloodyMasterWaveMask[7].set(-1, 2);
    m_pBloodyMasterWaveMask[8].set(1, -2);
    m_pBloodyMasterWaveMask[9].set(1, 2);
    m_pBloodyMasterWaveMask[10].set(2, -1);
    m_pBloodyMasterWaveMask[11].set(2, 1);

    m_pBloodyMasterWaveMask[12].set(0, -2);
    m_pBloodyMasterWaveMask[13].set(-2, 0);
    m_pBloodyMasterWaveMask[14].set(2, 0);
    m_pBloodyMasterWaveMask[15].set(0, 2);

    m_pBloodyMasterWaveMask[16].set(-1, -1);
    m_pBloodyMasterWaveMask[17].set(0, -1);
    m_pBloodyMasterWaveMask[18].set(1, -1);
    m_pBloodyMasterWaveMask[19].set(-1, 0);

    m_pBloodyMasterWaveMask[20].set(1, 0);
    m_pBloodyMasterWaveMask[21].set(-1, 1);
    m_pBloodyMasterWaveMask[22].set(0, 1);
    m_pBloodyMasterWaveMask[23].set(1, 1);
}

//////////////////////////////////////////////////////////////////////////////
// Vampire self
//////////////////////////////////////////////////////////////////////////////
void BloodyMasterWave::execute(Vampire* pVampire, VampireSkillSlot* pVampireSkillSlot, CEffectID_t CEffectID)

{
    Assert(pVampire != NULL);
    Assert(pVampireSkillSlot != NULL);

    try {
        execute(pVampire, pVampire->getX(), pVampire->getY(), pVampireSkillSlot, CEffectID);
    } catch (Throwable& t) {
        executeSkillFailException(pVampire, getSkillType());
        ////cout << t.toString() << endl;
    }
}

//////////////////////////////////////////////////////////////////////////////
// Vampire object handler
//////////////////////////////////////////////////////////////////////////////
void BloodyMasterWave::execute(Vampire* pVampire, ObjectID_t TargetObjectID, VampireSkillSlot* pVampireSkillSlot,
                               CEffectID_t CEffectID)

{
    Assert(pVampire != NULL);
    Assert(pVampireSkillSlot != NULL);

    try {
        // A missing target fails the skill instead of throwing.

        execute(pVampire, pVampire->getX(), pVampire->getY(), pVampireSkillSlot, CEffectID);
    } catch (Throwable& t) {
        executeSkillFailException(pVampire, getSkillType());
    }
}

//////////////////////////////////////////////////////////////////////////////
// Vampire tile handler
//////////////////////////////////////////////////////////////////////////////
void BloodyMasterWave::execute(Vampire* pVampire, ZoneCoord_t X, ZoneCoord_t Y, VampireSkillSlot* pVampireSkillSlot,
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
    param.bMagicHitRoll = true;
    param.bMagicDamage = true;
    param.bAdd = false;

    SIMPLE_SKILL_OUTPUT result;

    for (int i = 0; i < maxBloodyMasterWaveMask; i++) {
        param.addMask(m_pBloodyMasterWaveMask[i].x, m_pBloodyMasterWaveMask[i].y, 100);
    }

    // Chance to force a knockback
    bool bForceKnockback = rand() % 100 < output.ToHit;

    g_SimpleTileMissileSkill.execute(pVampire, pVampire->getX(), pVampire->getY(), pVampireSkillSlot, param, result, 0,
                                     bForceKnockback);
}

//////////////////////////////////////////////////////////////////////////////
// Monster tile handler
//////////////////////////////////////////////////////////////////////////////
void BloodyMasterWave::execute(Monster* pMonster, ZoneCoord_t X, ZoneCoord_t Y)

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
    param.bMagicHitRoll = true;
    param.bMagicDamage = true;
    param.bAdd = false;

    SIMPLE_SKILL_OUTPUT result;

    for (int i = 0; i < maxBloodyMasterWaveMask; i++) {
        param.addMask(m_pBloodyMasterWaveMask[i].x, m_pBloodyMasterWaveMask[i].y, 100);
    }

    bool bForceKnockback = false;

    // A master always forces a knockback.
    if (pMonster->isMaster()) {
        bForceKnockback = true;
    }

    g_SimpleTileMissileSkill.execute(pMonster, pMonster->getX(), pMonster->getY(), param, result, 0, bForceKnockback);
}

BloodyMasterWave g_BloodyMasterWave;
