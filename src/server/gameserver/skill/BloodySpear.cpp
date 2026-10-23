//////////////////////////////////////////////////////////////////////////////
// Filename    : BloodySpear.cpp
// Written by  :
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "BloodySpear.h"

#include "RankBonus.h"
#include "SimpleMissileSkill.h"

//////////////////////////////////////////////////////////////////////////////
// Vampire object handler
//////////////////////////////////////////////////////////////////////////////
void BloodySpear::execute(Vampire* pVampire, ObjectID_t TargetObjectID, VampireSkillSlot* pVampireSkillSlot,
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

    // Knowledge of Blood gives a hit bonus of 10.
    int HitBonus = 0;
    if (pVampire->hasRankBonus(RankBonus::RANK_BONUS_KNOWLEDGE_OF_BLOOD)) {
        RankBonus* pRankBonus = pVampire->getRankBonus(RankBonus::RANK_BONUS_KNOWLEDGE_OF_BLOOD);
        Assert(pRankBonus != NULL);

        HitBonus = pRankBonus->getPoint();
    }

    g_SimpleMissileSkill.execute(pVampire, TargetObjectID, pVampireSkillSlot, param, result, CEffectID, HitBonus);
}

//////////////////////////////////////////////////////////////////////////////
// Monster object handler
//////////////////////////////////////////////////////////////////////////////
void BloodySpear::execute(Monster* pMonster, Creature* pEnemy)

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

    if (pMonster->isMaster()) {
        if (pEnemy == NULL)
            return;

        int X = pEnemy->getX();
        int Y = pEnemy->getY();
        int oX, oY;

        Zone* pZone = pEnemy->getZone();
        Assert(pZone != NULL);

        VSRect rect(0, 0, pZone->getWidth() - 1, pZone->getHeight() - 1);

        for (oX = -3; oX <= 3; oX++)
            for (oY = -3; oY <= 3; oY++) {
                int tileX = X + oX;
                int tileY = Y + oY;
                if (!rect.ptInRect(tileX, tileY))
                    continue;

                checkMine(pZone, tileX, tileY);

                Tile& tile = pZone->getTile(tileX, tileY);

                Creature* pTargetCreature = NULL;
                if (tile.hasCreature(Creature::MOVE_MODE_WALKING))
                    pTargetCreature = tile.getCreature(Creature::MOVE_MODE_WALKING);

                if (pTargetCreature != NULL
                    // It must not be an NPC either.
                    && !pTargetCreature->isNPC()
                    // Check that it is a valid attack target
                    && pMonster->isEnemyToAttack(pTargetCreature)) {
                    g_SimpleMissileSkill.execute(pMonster, pTargetCreature, param, result);
                }
            }
    } else {
        g_SimpleMissileSkill.execute(pMonster, pEnemy, param, result);
    }
}

BloodySpear g_BloodySpear;
