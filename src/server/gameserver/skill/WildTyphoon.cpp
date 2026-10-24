//////////////////////////////////////////////////////////////////////////////
// Filename    : WildTyphoon.cpp
// Written by  : elca@ewestsoft.com
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "WildTyphoon.h"

#include <list>

#include "SimpleMeleeSkill.h"
#include "SimpleTileMissileSkill.h"

const int dir_advance[8][2] = {{-1, 0}, {-1, 1}, {0, 1}, {1, 1}, {1, 0}, {1, -1}, {0, -1}, {-1, -1}};

void WildTyphoon::execute(Slayer* pSlayer, ObjectID_t targetObjectID, SkillSlot* pSkillSlot, CEffectID_t CEffectID) {
    Zone* pZone = pSlayer->getZone();
    Assert(pZone != NULL);

    Creature* pTargetCreature = pZone->getCreature(targetObjectID);

    if (pTargetCreature == NULL) {
        executeSkillFailException(pSlayer, getSkillType());
        return;
    }

    execute(pSlayer, pTargetCreature->getX(), pTargetCreature->getY(), pSkillSlot, CEffectID);
}

void WildTyphoon::execute(Slayer* pSlayer, ZoneCoord_t X, ZoneCoord_t Y, SkillSlot* pSkillSlot, CEffectID_t CEffectID)

{
    Zone* pZone = pSlayer->getZone();
    Assert(pZone != NULL);

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
    param.bExpForTotalDamage = false;

    Dir_t dir = getDirectionToPosition(pSlayer->getX(), pSlayer->getY(), X, Y);

    SIMPLE_SKILL_OUTPUT result;

    // The target tile and the area around it.
    param.addMask(0 + dir_advance[dir][0], 0 + dir_advance[dir][1], 100);
    param.addMask(-1 + dir_advance[dir][0], -1 + dir_advance[dir][1], 100);
    param.addMask(0 + dir_advance[dir][0], -1 + dir_advance[dir][1], 100);
    param.addMask(1 + dir_advance[dir][0], -1 + dir_advance[dir][1], 100);
    param.addMask(-1 + dir_advance[dir][0], 0 + dir_advance[dir][1], 100);
    param.addMask(1 + dir_advance[dir][0], 0 + dir_advance[dir][1], 100);
    param.addMask(-1 + dir_advance[dir][0], 1 + dir_advance[dir][1], 100);
    param.addMask(0 + dir_advance[dir][0], 1 + dir_advance[dir][1], 100);
    param.addMask(1 + dir_advance[dir][0], 1 + dir_advance[dir][1], 100);

    g_SimpleTileMissileSkill.execute(pSlayer, X, Y, pSkillSlot, param, result);

    list<Creature*>::iterator itr = result.targetCreatures.begin();
    list<Creature*>::iterator endItr = result.targetCreatures.end();

    for (; itr != endItr; ++itr) {
        Creature* pCreature = *itr;
        if (pCreature != NULL) {
            // Only monsters get a delay added.
            // Players are handled on the client.
            int stunRatio = pSlayer->getSTR() / 5;
            if (pCreature->isMonster() && (rand() % 100) < stunRatio) {
                Monster* pMonster = dynamic_cast<Monster*>(pCreature);

                // Set the delay ( + 2 seconds )
                if (!pMonster->isMaster()) {
                    Timeval delay;
                    delay.tv_sec = 2;
                    delay.tv_usec = 0; // 500000;
                    pMonster->addAccuDelay(delay);
                }
            }
        }
    }
}

WildTyphoon g_WildTyphoon;
