//////////////////////////////////////////////////////////////////////////////
// Filename    : SacredStamp.cpp
// Written by  :
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "SacredStamp.h"

#include "RankBonus.h"
#include "SimpleTileMissileSkill.h"

//////////////////////////////////////////////////////////////////////////////
// Slayer object handler
//////////////////////////////////////////////////////////////////////////////
void SacredStamp::execute(Slayer* pSlayer, ObjectID_t targetObjectID, SkillSlot* pSkillSlot, CEffectID_t CEffectID) {
    Zone* pZone = pSlayer->getZone();
    Assert(pZone != NULL);

    Creature* pTargetCreature = pZone->getCreature(targetObjectID);

    if (pTargetCreature == NULL) {
        executeSkillFailException(pSlayer, getSkillType());
        return;
    }

    execute(pSlayer, pTargetCreature->getX(), pTargetCreature->getY(), pSkillSlot, CEffectID);
}

void SacredStamp::execute(Slayer* pSlayer, ZoneCoord_t X, ZoneCoord_t Y, SkillSlot* pSkillSlot, CEffectID_t CEffectID) {
    SkillInput input(pSlayer, pSkillSlot);
    SkillOutput output;
    computeOutput(input, output);

    SIMPLE_SKILL_INPUT param;
    param.SkillType = getSkillType();
    param.SkillDamage = output.Damage;
    param.Delay = output.Delay;
    param.ItemClass = Item::ITEM_CLASS_MAX;
    param.STRMultiplier = 1;
    param.DEXMultiplier = 1;
    param.INTMultiplier = 8;
    param.bMagicHitRoll = true;
    param.bMagicDamage = true;
    param.bAdd = false;

    for (int i = -2; i <= 2; ++i)
        for (int j = -2; j <= 2; ++j) {
            param.addMask(i, j, 100);
        }

    SIMPLE_SKILL_OUTPUT result;

    // Holy Smashing adds its rank bonus to the damage.
    if (pSlayer->hasRankBonus(RankBonus::RANK_BONUS_HOLY_SMASHING)) {
        RankBonus* pRankBonus = pSlayer->getRankBonus(RankBonus::RANK_BONUS_HOLY_SMASHING);
        Assert(pRankBonus != NULL);

        param.SkillDamage += pRankBonus->getPoint();
    }

    g_SimpleTileMissileSkill.execute(pSlayer, X, Y, pSkillSlot, param, result);
}

SacredStamp g_SacredStamp;
