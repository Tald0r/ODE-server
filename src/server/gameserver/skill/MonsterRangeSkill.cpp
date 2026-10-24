//////////////////////////////////////////////////////////////////////////////
// Filename    : MonsterRangeSkill.cpp
// Written by  :
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "MonsterRangeSkill.h"

#include "RankBonus.h"
#include "SimpleMissileSkill.h"

//////////////////////////////////////////////////////////////////////////////
// Monster object handler
//////////////////////////////////////////////////////////////////////////////
void MonsterRangeSkill::execute(Monster* pMonster, Creature* pEnemy)

{
    SIMPLE_SKILL_INPUT param;
    param.SkillType = getSkillType();
    param.SkillDamage = m_Damage;
    param.Delay = 0;
    param.ItemClass = Item::ITEM_CLASS_MAX;
    param.STRMultiplier = 0;
    param.DEXMultiplier = 0;
    param.INTMultiplier = 0;
    param.bMagicHitRoll = m_bMagic;
    param.bMagicDamage = m_bMagic;
    param.bAdd = m_bAdd;

    SIMPLE_SKILL_OUTPUT result;

    g_SimpleMissileSkill.execute(pMonster, pEnemy, param, result);
}
