//////////////////////////////////////////////////////////////////////////////
// Filename    : SelfDestruction.cpp
// Written by  : elca@ewestsoft.com
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "SelfDestruction.h"
// #include "GCSkillToSelfOK1.h"
#include "EffectKillTimer.h"
#include "GCAddEffectToTile.h"
#include "SimpleTileMissileSkill.h"


//////////////////////////////////////////////////////////////////////////////
// Constructor
// Initializes the mask.
//////////////////////////////////////////////////////////////////////////////
SelfDestruction::SelfDestruction() {}

//////////////////////////////////////////////////////////////////////////////
// Monster self handler
//////////////////////////////////////////////////////////////////////////////
void SelfDestruction::execute(Monster* pMonster)

{
    Assert(pMonster != NULL);

    try {
        Zone* pZone = pMonster->getZone();
        Assert(pZone != NULL);


        ZoneCoord_t x = pMonster->getX();
        ZoneCoord_t y = pMonster->getY();

        bool bRangeCheck = checkZoneLevelToUseSkill(pMonster);

        if (bRangeCheck) {
            //--------------------------------------------------------
            // Check which surrounding creatures are hit and knocked back.
            //--------------------------------------------------------
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


            for (int i = -1; i <= 1; ++i)
                for (int j = -1; j <= 1; ++j)
                    param.addMask(i, j, 100);

            g_SimpleTileMissileSkill.execute(pMonster, x, y, param, result, 0, false);

            if (result.bSuccess) {
                EffectKillTimer* pEffect = new EffectKillTimer(pMonster);
                pEffect->setDeadline(8);
                pMonster->addEffect(pEffect);
            }
        } else {
            executeSkillFailNormal(pMonster, getSkillType(), NULL);
        }
    } catch (Throwable& t) {
        executeSkillFailException(pMonster, getSkillType());
    }
}

SelfDestruction g_SelfDestruction;
