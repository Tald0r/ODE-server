//////////////////////////////////////////////////////////////////////////////
// Filename    : LandMineExplosion.cpp
// Written by  : elca@ewestsoft.com
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "LandMineExplosion.h"
// #include "GCSkillToSelfOK1.h"
#include "GCAddEffectToTile.h"
#include "SimpleTileMeleeSkill.h"


//////////////////////////////////////////////////////////////////////////////
// Constructor
// Initializes the mask.
//////////////////////////////////////////////////////////////////////////////
LandMineExplosion::LandMineExplosion() {
    int index = 0;

    for (int i = -3; i <= 3; ++i)
        for (int j = -3; j <= 3; ++j) {
            if (i == 0 && j == 0)
                continue;
            m_pLandMineExplosionMask[index++].set(i, j);
        }

    // The surrounding 8 tiles
}

//////////////////////////////////////////////////////////////////////////////
// Monster self handler
//////////////////////////////////////////////////////////////////////////////
void LandMineExplosion::execute(Monster* pMonster)

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

            for (int i = 0; i < 48; i++) {
                param.addMask(m_pLandMineExplosionMask[i].x, m_pLandMineExplosionMask[i].y, 100);
            }

            // Knock back the creatures that are hit.
            g_SimpleTileMeleeSkill.execute(pMonster, x, y, param, result, 0, false);

            GCAddEffectToTile gcAE;
            gcAE.setXY(x, y);
            gcAE.setEffectID(Effect::EFFECT_CLASS_LAND_MINE_EXPLOSION);
            gcAE.setObjectID(0);
            gcAE.setDuration(0);
            pZone->broadcastPacket(x, y, &gcAE);
        } else {
            executeSkillFailNormal(pMonster, getSkillType(), NULL);
        }
    } catch (Throwable& t) {
        executeSkillFailException(pMonster, getSkillType());
    }
}

LandMineExplosion g_LandMineExplosion;
