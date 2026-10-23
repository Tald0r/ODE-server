
//////////////////////////////////////////////////////////////////////////////
// Filename    : MultiAmputate.cpp
// Written by  :
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "MultiAmputate.h"

#include "Item.h"
#include "RankBonus.h"
#include "SimpleMissileSkill.h"
#include "SkillUtil.h"

//////////////////////////////////////////////////////////////////////////////
// Slayer object handler
//////////////////////////////////////////////////////////////////////////////
void MultiAmputate::execute(Slayer* pSlayer, ObjectID_t TargetObjectID, SkillSlot* pSkillSlot, CEffectID_t CEffectID)

{
    SkillInput input(pSlayer, pSkillSlot);
    SkillOutput output;
    computeOutput(input, output);

    SIMPLE_SKILL_INPUT param;
    param.SkillType = getSkillType();
    param.SkillDamage = output.Damage;
    param.Delay = output.Delay;
    param.ItemClass = Item::ITEM_CLASS_BLADE;
    param.STRMultiplier = 8;
    param.DEXMultiplier = 1;
    param.INTMultiplier = 1;
    param.bMagicHitRoll = false;
    param.bMagicDamage = true;
    param.bAdd = true;

    SIMPLE_SKILL_OUTPUT result;

    Creature* pTargetCreature = pSlayer->getZone()->getCreature(TargetObjectID);
    bool bRangeCheck = pTargetCreature != NULL && verifyDistance(pSlayer, pTargetCreature, output.Range);

    // The check above only tests the max range.
    if (!bRangeCheck) {
        executeSkillFailNormal(pSlayer, param.SkillType, pTargetCreature);
        return;
    }

    Item* pItem = pSlayer->getWearItem(Slayer::WEAR_RIGHTHAND);
    if (pItem == NULL || pItem->getItemClass() != param.ItemClass) {
        executeSkillFailException(pSlayer, param.SkillType);
        return;
    }

    g_SimpleMissileSkill.execute(pSlayer, TargetObjectID, pSkillSlot, param, result);
}

MultiAmputate g_MultiAmputate;
