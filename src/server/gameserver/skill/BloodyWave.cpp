//////////////////////////////////////////////////////////////////////////////
// Filename    : BloodyWave.cpp
// Written by  : excel96
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "BloodyWave.h"

#include "RankBonus.h"
#include "SimpleTileMissileSkill.h"

//////////////////////////////////////////////////////////////////////////////
// Constructor
// Initializes the mask.
//////////////////////////////////////////////////////////////////////////////
BloodyWave::BloodyWave() {
    m_pBloodyWaveMask[0].set(0, -2);
    m_pBloodyWaveMask[1].set(-2, 0);
    m_pBloodyWaveMask[2].set(2, 0);
    m_pBloodyWaveMask[3].set(0, 2);
    m_pBloodyWaveMask[4].set(-1, -1);
    m_pBloodyWaveMask[5].set(0, -1);
    m_pBloodyWaveMask[6].set(1, -1);
    m_pBloodyWaveMask[7].set(-1, 0);
    m_pBloodyWaveMask[8].set(1, 0);
    m_pBloodyWaveMask[9].set(-1, 1);
    m_pBloodyWaveMask[10].set(0, 1);
    m_pBloodyWaveMask[11].set(1, 1);
}

//////////////////////////////////////////////////////////////////////////////
// Vampire self
//////////////////////////////////////////////////////////////////////////////
void BloodyWave::execute(Vampire* pVampire, VampireSkillSlot* pVampireSkillSlot, CEffectID_t CEffectID)

{
    Assert(pVampire != NULL);
    Assert(pVampireSkillSlot != NULL);

    try {
        execute(pVampire, pVampire->getX(), pVampire->getY(), pVampireSkillSlot, CEffectID);
    } catch (Throwable& t) {
        executeSkillFailException(pVampire, getSkillType());
    }
}


//////////////////////////////////////////////////////////////////////////////
// Vampire object handler
//////////////////////////////////////////////////////////////////////////////
void BloodyWave::execute(Vampire* pVampire, ObjectID_t TargetObjectID, VampireSkillSlot* pVampireSkillSlot,
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
void BloodyWave::execute(Vampire* pVampire, ZoneCoord_t X, ZoneCoord_t Y, VampireSkillSlot* pVampireSkillSlot,
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

    for (int i = 0; i < maxBloodyWaveMask; i++) {
        param.addMask(m_pBloodyWaveMask[i].x, m_pBloodyWaveMask[i].y, 100);
    }

    // Chance to force a knockback
    bool bForceKnockback = rand() % 100 < output.ToHit;

    // Knowledge of Blood gives a hit bonus of 10.
    int HitBonus = 0;
    if (pVampire->hasRankBonus(RankBonus::RANK_BONUS_KNOWLEDGE_OF_BLOOD)) {
        RankBonus* pRankBonus = pVampire->getRankBonus(RankBonus::RANK_BONUS_KNOWLEDGE_OF_BLOOD);
        Assert(pRankBonus != NULL);

        HitBonus = pRankBonus->getPoint();
    }

    g_SimpleTileMissileSkill.execute(pVampire, pVampire->getX(), pVampire->getY(), pVampireSkillSlot, param, result, 0,
                                     HitBonus, bForceKnockback);
}

//////////////////////////////////////////////////////////////////////////////
// Monster tile handler
//////////////////////////////////////////////////////////////////////////////
void BloodyWave::execute(Monster* pMonster, ZoneCoord_t X, ZoneCoord_t Y)

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

    for (int i = 0; i < maxBloodyWaveMask; i++) {
        param.addMask(m_pBloodyWaveMask[i].x, m_pBloodyWaveMask[i].y, 100);
    }

    bool bForceKnockback = false;

    // A master always forces a knockback.
    if (pMonster->isMaster()) {
        bForceKnockback = true;
    }

    g_SimpleTileMissileSkill.execute(pMonster, pMonster->getX(), pMonster->getY(), param, result, 0, bForceKnockback);
}

BloodyWave g_BloodyWave;
