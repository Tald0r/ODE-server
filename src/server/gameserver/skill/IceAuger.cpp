//////////////////////////////////////////////////////////////////////////////
// Filename    : IceAuger.cpp
// Written by  :
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "IceAuger.h"

#include "RankBonus.h"
#include "SimpleMissileSkill.h"

//////////////////////////////////////////////////////////////////////////////
// Monster object handler
//////////////////////////////////////////////////////////////////////////////
void IceAuger::execute(Monster* pMonster, Creature* pEnemy)

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

    g_SimpleMissileSkill.execute(pMonster, pEnemy, param, result);
}

IceAuger g_IceAuger;
