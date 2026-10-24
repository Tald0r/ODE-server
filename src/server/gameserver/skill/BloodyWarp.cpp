//////////////////////////////////////////////////////////////////////////////
// Filename    : BloodyWarp.cpp
// Written by  : excel96
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "BloodyWarp.h"

#include "GameContext.h"
#include "SkillHandlerManager.h"
#include "Zone.h"

//////////////////////////////////////////////////////////////////////////////
// Constructor
// Initializes the mask.
//////////////////////////////////////////////////////////////////////////////
POINT
BloodyWarp::getWarpPosition(int myX, int myY, int targetX, int targetY) {
    POINT pt;

    // Warp to a random position a few tiles past the target
    {
        int stepX = targetX - myX;
        int stepY = targetY - myY;
        int signX = (stepX > 0 ? 1 : stepX < 0 ? -1 : 0);
        int signY = (stepY > 0 ? 1 : stepY < 0 ? -1 : 0);
        int cx = targetX + signX * (2 + rand() % 2);
        int cy = targetY + signY * (2 + rand() % 2);

        pt.x = cx;
        pt.y = cy;
    }

    return pt;
}


//////////////////////////////////////////////////////////////////////////////
// Vampire object handler
//////////////////////////////////////////////////////////////////////////////
void BloodyWarp::execute(Vampire* pVampire, ObjectID_t TargetObjectID, VampireSkillSlot* pVampireSkillSlot,
                         CEffectID_t CEffectID)

{
    Assert(pVampire != NULL);
    Assert(pVampireSkillSlot != NULL);

    try {
        Zone* pZone = pVampire->getZone();
        Assert(pZone != NULL);

        Creature* pTargetCreature = pZone->getCreature(TargetObjectID);
        Assert(pTargetCreature != NULL);

        // A missing target fails the skill instead of throwing.
        if (pTargetCreature == NULL) {
            executeSkillFailException(pVampire, getSkillType());

            return;
        }

        execute(pVampire, pTargetCreature->getX(), pTargetCreature->getY(), pVampireSkillSlot, CEffectID);
    } catch (Throwable& t) {
        executeSkillFailException(pVampire, getSkillType());
    }
}

//////////////////////////////////////////////////////////////////////////////
// Vampire tile handler
//////////////////////////////////////////////////////////////////////////////
void BloodyWarp::execute(Vampire* pVampire, ZoneCoord_t X, ZoneCoord_t Y, VampireSkillSlot* pVampireSkillSlot,
                         CEffectID_t CEffectID)

{
    { executeSkillFailNormal(pVampire, getSkillType(), NULL); }
}

//////////////////////////////////////////////////////////////////////////////
// Monster tile handler
//////////////////////////////////////////////////////////////////////////////
void BloodyWarp::execute(Monster* pMonster, ZoneCoord_t X, ZoneCoord_t Y)

{
    Zone* pZone = pMonster->getZone();
    Assert(pZone != NULL);

    SkillInput input(pMonster);
    SkillOutput output;
    computeOutput(input, output);

    int myX = pMonster->getX();
    int myY = pMonster->getY();

    POINT pt = getWarpPosition(myX, myY, X, Y);

    // Use BLOODY_WALL at the enemy position.
    SkillType_t SkillType = SKILL_BLOODY_WALL;

    if (pMonster->getMonsterType() >= 717)
        SkillType = SKILL_BLOODY_SNAKE;

    SkillHandler* pSkillHandler = de::gameContext().skillHandlers().getSkillHandler(SkillType);
    Assert(pSkillHandler != NULL);

    pSkillHandler->execute(pMonster, X, Y);


    if (pZone->moveFastMonster(pMonster, myX, myY, pt.x, pt.y, getSkillType())) {
        // Use BLOODY_WAVE at the monster destination.
        SkillType = (pMonster->isMaster() ? SKILL_BLOODY_MASTER_WAVE : SKILL_BLOODY_WAVE);

        pSkillHandler = de::gameContext().skillHandlers().getSkillHandler(SkillType);
        Assert(pSkillHandler != NULL);

        pSkillHandler->execute(pMonster, X, Y);
    } else {
        executeSkillFailNormal(pMonster, getSkillType(), NULL);
    }
}

BloodyWarp g_BloodyWarp;
