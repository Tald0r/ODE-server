//////////////////////////////////////////////////////////////////////////////
// Filename    : Typhoon.cpp
// Written by  : elca@ewestsoft.com
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "Typhoon.h"

#include "EffectTyphoon.h"
#include "SimpleMeleeSkill.h"

//////////////////////////////////////////////////////////////////////////////
// Slayer object handler
//////////////////////////////////////////////////////////////////////////////
void Typhoon::execute(Slayer* pSlayer, ObjectID_t TargetObjectID, SkillSlot* pSkillSlot, CEffectID_t CEffectID)

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
    param.bMagicDamage = false;
    param.bAdd = true;

    SIMPLE_SKILL_OUTPUT result;


    g_SimpleMeleeSkill.execute(pSlayer, TargetObjectID, pSkillSlot, param, result);

    if (result.bSuccess) {
        Zone* pZone = pSlayer->getZone();
        Assert(pZone != NULL);

        Creature* pCreature = pZone->getCreature(TargetObjectID);

        if (pCreature != NULL) {
            // Only monsters get a delay added.
            // Players are handled on the client.
            if (pCreature->isMonster()) {
                Monster* pMonster = dynamic_cast<Monster*>(pCreature);

                // Sets the delay (+1 second)
                if (!pMonster->isMaster()) {
                    Timeval delay;
                    delay.tv_sec = 1;
                    delay.tv_usec = 0; // 500000;
                    pMonster->addAccuDelay(delay);
                }
            }
        }
    }

    // Deals damage to the target, then attaches the effect.
}

Typhoon g_Typhoon;
