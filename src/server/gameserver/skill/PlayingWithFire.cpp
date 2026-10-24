//////////////////////////////////////////////////////////////////////////////
// Filename    : PlayingWithFire.cpp
// Written by  : elca@ewestsoft.com
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "PlayingWithFire.h"
// #include "GCSkillToSelfOK1.h"
// #include "GCSkillToSelfOK2.h"
#include "SimpleTileMeleeSkill.h"


//////////////////////////////////////////////////////////////////////////////
// Constructor
// Initializes the mask.
//////////////////////////////////////////////////////////////////////////////
PlayingWithFire::PlayingWithFire() {
    // The surrounding 8 tiles
    m_pPlayingWithFireMask[0].set(1, 1);
    m_pPlayingWithFireMask[1].set(-1, -1);
    m_pPlayingWithFireMask[2].set(0, -1);
    m_pPlayingWithFireMask[3].set(1, -1);
    m_pPlayingWithFireMask[4].set(-1, 0);
    m_pPlayingWithFireMask[5].set(1, 0);
    m_pPlayingWithFireMask[6].set(-1, 1);
    m_pPlayingWithFireMask[7].set(0, 1);
}

//////////////////////////////////////////////////////////////////////////////
// Vampire self handler
//////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////
// Monster self handler
//////////////////////////////////////////////////////////////////////////////
void PlayingWithFire::execute(Monster* pMonster)

{
    cout << "TID[" << Thread::self() << "]" << getSkillHandlerName() << " Begin" << endl;

    Assert(pMonster != NULL);

    try {
        Zone* pZone = pMonster->getZone();
        Assert(pZone != NULL);

        if (pMonster->isFlag(Effect::EFFECT_CLASS_HIDE)) {
            return;
        }
        if (pMonster->isFlag(Effect::EFFECT_CLASS_INVISIBILITY)) {
            addVisibleCreature(pZone, pMonster, true);
        }


        ZoneCoord_t x = pMonster->getX();
        ZoneCoord_t y = pMonster->getY();

        bool bRangeCheck = checkZoneLevelToUseSkill(pMonster);
        bool bMoveModeCheck = pMonster->isWalking();

        if (bRangeCheck && bMoveModeCheck) {
            //--------------------------------------------------------
            // Make the Critical Ground skill visible.
            //--------------------------------------------------------


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

            for (int i = -10; i <= 10; ++i)
                for (int j = -10; j <= 10; ++j)
                    param.addMask(i, j, 100);


            // Knock back the creatures that are hit.
            bool bForceKnockback = true;
            g_SimpleTileMeleeSkill.execute(pMonster, x, y, param, result, 0, bForceKnockback);
        } else {
            executeSkillFailNormal(pMonster, getSkillType(), NULL);
        }
    } catch (Throwable& t) {
        executeSkillFailException(pMonster, getSkillType());
    }
}

PlayingWithFire g_PlayingWithFire;
