//////////////////////////////////////////////////////////////////////////////
// Filename    : AcidBall.cpp
// Written by  :
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "AcidBall.h"

#include "RankBonus.h"
#include "SimpleMissileSkill.h"

//////////////////////////////////////////////////////////////////////////////
// Vampire object handler
//////////////////////////////////////////////////////////////////////////////
void AcidBall::execute(Vampire* pVampire, ObjectID_t TargetObjectID, VampireSkillSlot* pVampireSkillSlot,
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

    // Knowledge of Acid gives a hit bonus of 10.
    int HitBonus = 0;
    if (pVampire->hasRankBonus(RankBonus::RANK_BONUS_KNOWLEDGE_OF_ACID)) {
        RankBonus* pRankBonus = pVampire->getRankBonus(RankBonus::RANK_BONUS_KNOWLEDGE_OF_ACID);
        Assert(pRankBonus != NULL);

        HitBonus = pRankBonus->getPoint();
    }

    g_SimpleMissileSkill.execute(pVampire, TargetObjectID, pVampireSkillSlot, param, result, CEffectID, HitBonus);
}

//////////////////////////////////////////////////////////////////////////////
// Monster object handler
//////////////////////////////////////////////////////////////////////////////
void AcidBall::execute(Monster* pMonster, Creature* pEnemy)

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

    // The master's multi-target attack is switched off; the branch never runs.
    if (0) // pMonster->isMaster())
    {
        int x = pMonster->getX();
        int y = pMonster->getY();

        int Splash = 3 + rand() % 5; // 3 to 7 creatures
        int range = 2;               // 5x5
        list<Creature*> creatureList;
        getSplashVictims(pMonster->getZone(), x, y, Creature::CREATURE_CLASS_MAX, creatureList, Splash, range);

        list<Creature*>::iterator itr = creatureList.begin();
        int i = 0;
        for (; itr != creatureList.end(); itr++) {
            Creature* pTargetCreature = (*itr);
            Assert(pTargetCreature != NULL);

            if (pMonster != pTargetCreature) {
                cout << "Master's AcidBall: " << i << endl;
                i++;
                g_SimpleMissileSkill.execute(pMonster, pTargetCreature, param, result);
            }
        }
    } else {
        g_SimpleMissileSkill.execute(pMonster, pEnemy, param, result);
    }
}

AcidBall g_AcidBall;
