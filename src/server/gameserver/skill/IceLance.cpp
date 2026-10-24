//////////////////////////////////////////////////////////////////////////////
// Filename    : IceLance.cpp
// Written by  :
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "IceLance.h"

#include "RankBonus.h"
#include "SimpleTileMissileSkill.h"

//////////////////////////////////////////////////////////////////////////////
// Ousters object handler
//////////////////////////////////////////////////////////////////////////////
void IceLance::execute(Ousters* pOusters, ObjectID_t TargetObjectID, OustersSkillSlot* pOustersSkillSlot,
                       CEffectID_t CEffectID)

{
    Assert(pOusters != NULL);
    Assert(pOustersSkillSlot != NULL);

    try {
        Zone* pZone = pOusters->getZone();
        Assert(pZone != NULL);

        Creature* pTargetCreature = pZone->getCreature(TargetObjectID);

        // A missing target fails the skill instead of throwing.
        if (pTargetCreature == NULL) {
            executeSkillFailException(pOusters, getSkillType());

            return;
        }

        execute(pOusters, pTargetCreature->getX(), pTargetCreature->getY(), pOustersSkillSlot, CEffectID);
    } catch (Throwable& t) {
        executeSkillFailException(pOusters, getSkillType());
    }
}

void IceLance::execute(Ousters* pOusters, ZoneCoord_t X, ZoneCoord_t Y, OustersSkillSlot* pOustersSkillSlot,
                       CEffectID_t CEffectID)

{
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
    param.bMagicHitRoll = true;
    param.bMagicDamage = true;
    param.bAdd = false;


    int offset = 0;

    OustersSkillSlot* pMastery = pOusters->hasSkill(SKILL_ICE_LANCE_MASTERY);
    if (pMastery != NULL) {
        offset = 1;
        param.SkillDamage += (pMastery->getExpLevel() * 2 / 3) + 10;
        param.Grade = 4;
    }

    for (int i = -offset; i <= offset; ++i)
        for (int j = -offset; j <= offset; ++j)
            param.addMask(i, j, 100);

    SIMPLE_SKILL_OUTPUT result;

    g_SimpleTileMissileSkill.execute(pOusters, X, Y, pOustersSkillSlot, param, result, CEffectID);
}

void IceLance::execute(Monster* pMonster, ZoneCoord_t X, ZoneCoord_t Y)

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

    param.SkillDamage += 30;
    param.Grade = 4;

    for (int i = -1; i <= 1; ++i)
        for (int j = -1; j <= 1; ++j)
            param.addMask(i, j, 100);

    g_SimpleTileMissileSkill.execute(pMonster, X, Y, param, result);
}

IceLance g_IceLance;
