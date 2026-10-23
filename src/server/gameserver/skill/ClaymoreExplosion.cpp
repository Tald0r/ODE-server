//////////////////////////////////////////////////////////////////////////////
// Filename    : ClaymoreExplosion.cpp
// Written by  : elca@ewestsoft.com
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "ClaymoreExplosion.h"
// #include "GCSkillToSelfOK1.h"
#include "GCAddEffectToTile.h"
#include "SimpleTileMeleeSkill.h"


//////////////////////////////////////////////////////////////////////////////
// Constructor
// Initializes the mask.
//////////////////////////////////////////////////////////////////////////////
ClaymoreExplosion::ClaymoreExplosion() {
    int index = 0;

    for (int i = 0; i < 5; ++i)
        for (int j = i - 4; j <= 0; ++j) {
            m_pClaymoreExplosionMask[index++].set(j, i);
        }

    // The surrounding 8 tiles
}

//////////////////////////////////////////////////////////////////////////////
// Monster self handler
//////////////////////////////////////////////////////////////////////////////
void ClaymoreExplosion::execute(Monster* pMonster)

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

            for (int i = 0; i < 15; i++) {
                param.addMask(m_pClaymoreExplosionMask[i].x, m_pClaymoreExplosionMask[i].y, 100);
            }

            // Hit the masked tiles; the knockback is not forced.
            g_SimpleTileMeleeSkill.execute(pMonster, x, y, param, result, 0, false);
            GCAddEffectToTile gcAE;
            gcAE.setXY(x, y);
            gcAE.setEffectID(Effect::EFFECT_CLASS_CLAYMORE_EXPLOTION);
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

ClaymoreExplosion g_ClaymoreExplosion;
