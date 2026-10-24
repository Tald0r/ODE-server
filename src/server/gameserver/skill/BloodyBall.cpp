//////////////////////////////////////////////////////////////////////////////
// Filename    : BloodyBall.cpp
// Written by  : excel96
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "BloodyBall.h"

#include "RankBonus.h"
#include "SimpleTileMissileSkill.h"

//////////////////////////////////////////////////////////////////////////////
// Constructor
// Initializes the mask.
//////////////////////////////////////////////////////////////////////////////
BloodyBall::BloodyBall() {
    m_pBloodyBallMask[0].set(0, 0);
    m_pBloodyBallMask[1].set(-1, -1);
    m_pBloodyBallMask[2].set(0, -1);
    m_pBloodyBallMask[3].set(1, -1);
    m_pBloodyBallMask[4].set(-1, 0);
    m_pBloodyBallMask[5].set(1, 0);
    m_pBloodyBallMask[6].set(-1, 1);
    m_pBloodyBallMask[7].set(0, 1);
    m_pBloodyBallMask[8].set(1, 1);
}


//////////////////////////////////////////////////////////////////////////////
// Vampire object handler
//////////////////////////////////////////////////////////////////////////////
void BloodyBall::execute(Vampire* pVampire, ObjectID_t TargetObjectID, VampireSkillSlot* pVampireSkillSlot,
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
void BloodyBall::execute(Vampire* pVampire, ZoneCoord_t X, ZoneCoord_t Y, VampireSkillSlot* pVampireSkillSlot,
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

    for (int i = 0; i < 9; i++) {
        param.addMask(m_pBloodyBallMask[i].x, m_pBloodyBallMask[i].y, 100);
    }

    // Knowledge of Blood gives a hit bonus of 10.
    int HitBonus = 0;
    if (pVampire->hasRankBonus(RankBonus::RANK_BONUS_KNOWLEDGE_OF_BLOOD)) {
        RankBonus* pRankBonus = pVampire->getRankBonus(RankBonus::RANK_BONUS_KNOWLEDGE_OF_BLOOD);
        Assert(pRankBonus != NULL);

        HitBonus = pRankBonus->getPoint();
    }

    g_SimpleTileMissileSkill.execute(pVampire, X, Y, pVampireSkillSlot, param, result, CEffectID, false, HitBonus);
}

//////////////////////////////////////////////////////////////////////////////
// Monster tile handler
//////////////////////////////////////////////////////////////////////////////
void BloodyBall::execute(Monster* pMonster, ZoneCoord_t X, ZoneCoord_t Y)

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

    for (int i = 0; i < 9; i++) {
        param.addMask(m_pBloodyBallMask[i].x, m_pBloodyBallMask[i].y, 100);
    }

    g_SimpleTileMissileSkill.execute(pMonster, X, Y, param, result);
}

BloodyBall g_BloodyBall;
