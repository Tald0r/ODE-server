//////////////////////////////////////////////////////////////////////////////
// Filename    : Halo.cpp
// Written by  :
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "Halo.h"

#include "EffectHalo.h"
#include "SimpleMissileSkill.h"
#include "Utility.h"
#include "Zone.h"
#include "ZoneUtil.h"

#define HALO_BASIS_DIRECTION_LOW 0.35
#define HALO_BASIS_DIRECTION_HIGH 3.0

static BYTE getDirection_Halo(int originX, int originY, int destX, int destY) {
    int stepX = destX - originX, stepY = destY - originY;

    // Check for zero.
    float k = (stepX == 0) ? 0 : (float)(stepY) / stepX; // Slope


    //--------------------------------------------------
    // Determine the direction.
    //--------------------------------------------------
    if (stepY == 0) {
        // X axis
        // - -;;
        if (stepX == 0)
            return DOWN;
        else if (stepX > 0)
            return RIGHT;
        else
            return LEFT;
    } else if (stepY < 0) // Upward
    {
        // Upward along the y axis
        if (stepX == 0) {
            return UP;
        }
        // First quadrant
        else if (stepX > 0) {
            if (k < -HALO_BASIS_DIRECTION_HIGH)
                return UP;
            else if (k <= -HALO_BASIS_DIRECTION_LOW)
                return RIGHTUP;
            else
                return RIGHT;
        }
        // Second quadrant
        else {
            if (k > HALO_BASIS_DIRECTION_HIGH)
                return UP;
            else if (k >= HALO_BASIS_DIRECTION_LOW)
                return LEFTUP;
            else
                return LEFT;
        }
    }
    // Downward
    else {
        // Downward along the y axis
        if (stepX == 0) {
            return DOWN;
        }
        // Fourth quadrant
        else if (stepX > 0) {
            if (k > HALO_BASIS_DIRECTION_HIGH)
                return DOWN;
            else if (k >= HALO_BASIS_DIRECTION_LOW)
                return RIGHTDOWN;
            else
                return RIGHT;
        }
        // Third quadrant
        else {
            if (k < -HALO_BASIS_DIRECTION_HIGH)
                return DOWN;
            else if (k <= -HALO_BASIS_DIRECTION_LOW)
                return LEFTDOWN;
            else
                return LEFT;
        }
    }
}


//////////////////////////////////////////////////////////////////////////////
// Vampire object handler
//////////////////////////////////////////////////////////////////////////////
void Halo::execute(Ousters* pOusters, ObjectID_t TargetObjectID, OustersSkillSlot* pOustersSkillSlot,
                   CEffectID_t CEffectID)

{
    SkillInput input(pOusters, pOustersSkillSlot);
    SkillOutput output;
    computeOutput(input, output);

    SIMPLE_SKILL_INPUT param;
    param.SkillType = getSkillType();
    param.SkillDamage = output.Damage;
    param.Delay = output.Delay;
    param.ItemClass = Item::ITEM_CLASS_OUSTERS_CHAKRAM;
    param.STRMultiplier = 0;
    param.DEXMultiplier = 0;
    param.INTMultiplier = 0;
    param.bMagicHitRoll = false;
    param.bMagicDamage = false;
    param.bAdd = true;

    SIMPLE_SKILL_OUTPUT result;

    g_SimpleMissileSkill.execute(pOusters, TargetObjectID, pOustersSkillSlot, param, result, CEffectID);

    if (result.bSuccess) {
        cout << "Halo succeeded" << endl;
        Dir_t dir = getDirection_Halo(pOusters->getX(), pOusters->getY(), result.pTargetCreature->getX(),
                                      result.pTargetCreature->getY());

        ZoneCoord_t effectX = result.pTargetCreature->getX() + dirMoveMask[(int)dir].x * 3;
        ZoneCoord_t effectY = result.pTargetCreature->getY() + dirMoveMask[(int)dir].y * 3;

        Zone* pZone = pOusters->getZone();
        if (isValidZoneCoord(pZone, effectX, effectY)) {
            EffectHalo* pEffect = new EffectHalo(pZone, effectX, effectY);
            pEffect->setTargetOID(TargetObjectID);
            pEffect->setUserOID(pOusters->getObjectID());
            pEffect->setDamage(output.Range);
            pEffect->setDeadline(output.Duration);
            pEffect->setNextTime(10);
            pEffect->setBroadcastingEffect(false);

            pZone->registerObject(pEffect);
            pZone->getTile(effectX, effectY).addEffect(pEffect);
            pZone->addEffect(pEffect);
        }
    }
}

Halo g_Halo;
