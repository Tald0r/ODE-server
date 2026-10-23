//////////////////////////////////////////////////////////////////////////////
// Filename    : SummonGoreGland.cpp
// Written by  : excel96
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "SummonGoreGland.h"

#include "EffectKillTimer.h"
#include "GCAddEffect.h"
#include "GCSkillToTileOK1.h"
#include "GCSkillToTileOK2.h"
#include "GCSkillToTileOK3.h"
#include "GCSkillToTileOK4.h"
#include "GCSkillToTileOK5.h"
#include "GCSkillToTileOK6.h"
#include "GameContext.h"
#include "RankBonus.h"

//////////////////////////////////////////////////////////////////////////////
// Vampire object handler
//////////////////////////////////////////////////////////////////////////////
void SummonGoreGland::execute(Vampire* pVampire, ObjectID_t TargetObjectID, VampireSkillSlot* pVampireSkillSlot,
                              CEffectID_t CEffectID)

{
    Assert(pVampire != NULL);
    Assert(pVampireSkillSlot != NULL);

    try {
        Zone* pZone = pVampire->getZone();
        Assert(pZone != NULL);

        Creature* pTargetCreature = pZone->getCreature(TargetObjectID);

        // An NPC cannot be attacked.
        if (pTargetCreature == NULL // The zone returns NULL when the target is gone.
            || pTargetCreature->isNPC()) {
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
void SummonGoreGland::execute(Vampire* pVampire, ZoneCoord_t X, ZoneCoord_t Y, VampireSkillSlot* pVampireSkillSlot,
                              CEffectID_t CEffectID)

{
    try {
        Player* pPlayer = pVampire->getPlayer();
        Zone* pZone = pVampire->getZone();

        Assert(pPlayer != NULL);
        Assert(pZone != NULL);

        GCSkillToTileOK1 _GCSkillToTileOK1;
        GCSkillToTileOK2 _GCSkillToTileOK2;
        GCSkillToTileOK3 _GCSkillToTileOK3;
        GCSkillToTileOK4 _GCSkillToTileOK4;
        GCSkillToTileOK5 _GCSkillToTileOK5;
        GCSkillToTileOK6 _GCSkillToTileOK6;

        SkillType_t SkillType = pVampireSkillSlot->getSkillType();
        SkillInfo* pSkillInfo = de::gameContext().skillInfos().getSkillInfo(SkillType);

        int RequiredMP = decreaseConsumeMP(pVampire, pSkillInfo);
        bool bManaCheck = hasEnoughMana(pVampire, RequiredMP);
        bool bTimeCheck = verifyRunTime(pVampireSkillSlot);
        bool bRangeCheck = verifyDistance(pVampire, X, Y, pSkillInfo->getRange());
        bool bHitRoll = HitRoll::isSuccessMagic(pVampire, pSkillInfo, pVampireSkillSlot);

        bool bTileCheck = true;
        VSRect rect(0, 0, pZone->getWidth() - 1, pZone->getHeight() - 1);
        if (!rect.ptInRect(X, Y)) {
            bTileCheck = false;
        }

        if (bManaCheck && bTimeCheck && bRangeCheck && bHitRoll && bTileCheck) {
            decreaseMana(pVampire, RequiredMP, _GCSkillToTileOK1);

            Range_t Range = 1; // Always 1.

            // Compute the damage and the duration.
            SkillInput input(pVampire);
            SkillOutput output;
            computeOutput(input, output);

            Monster* pMonster = new Monster(753);
            pMonster->setTreasure(false);
            pMonster->setClanType(33);
            pMonster->setScanEnemy(true);
            pMonster->setName("Gore Gland");

            EffectKillTimer* pEffect = new EffectKillTimer(pMonster);
            pEffect->setDeadline(output.Duration);
            pMonster->addEffect(pEffect);

            pMonster->setINT(pVampire->getINT());
            pMonster->setHP(500 + (8 * pVampire->getINT()), ATTR_MAX);
            pMonster->setHP(500 + (8 * pVampire->getINT()), ATTR_CURRENT);
            pMonster->setOwnerObjectID(pVampire->getObjectID());

            try {
                pZone->addCreature(pMonster, X, Y, 0);
            } catch (Exception& e) {
                filelog("SummonGoreGland.log", "%s", e.toString().c_str());
                return;
            }

            pZone->monsterScan(pMonster, X, Y, 0);

            ZoneCoord_t myX = pVampire->getX();
            ZoneCoord_t myY = pVampire->getY();

            _GCSkillToTileOK1.setSkillType(SkillType);
            _GCSkillToTileOK1.setCEffectID(CEffectID);
            _GCSkillToTileOK1.setX(X);
            _GCSkillToTileOK1.setY(Y);
            _GCSkillToTileOK1.setDuration(output.Duration);
            _GCSkillToTileOK1.setRange(Range);

            _GCSkillToTileOK3.setObjectID(pVampire->getObjectID());
            _GCSkillToTileOK3.setSkillType(SkillType);
            _GCSkillToTileOK3.setX(X);
            _GCSkillToTileOK3.setY(Y);

            _GCSkillToTileOK4.setSkillType(SkillType);
            _GCSkillToTileOK4.setX(X);
            _GCSkillToTileOK4.setY(Y);
            _GCSkillToTileOK4.setDuration(output.Duration);
            _GCSkillToTileOK4.setRange(Range);

            _GCSkillToTileOK5.setObjectID(pVampire->getObjectID());
            _GCSkillToTileOK5.setSkillType(SkillType);
            _GCSkillToTileOK5.setX(X);
            _GCSkillToTileOK5.setY(Y);
            _GCSkillToTileOK5.setDuration(output.Duration);
            _GCSkillToTileOK5.setRange(Range);

            pPlayer->sendPacket(&_GCSkillToTileOK1);

            list<Creature*> cList;
            cList.push_back(pVampire);

            cList = pZone->broadcastSkillPacket(myX, myY, X, Y, &_GCSkillToTileOK5, cList);

            pZone->broadcastPacket(myX, myY, &_GCSkillToTileOK3, cList);
            pZone->broadcastPacket(X, Y, &_GCSkillToTileOK4, cList);

            pVampireSkillSlot->setRunTime(output.Delay);
        } else {
            executeSkillFailNormal(pVampire, getSkillType(), NULL);
        }
    } catch (Throwable& t) {
        executeSkillFailException(pVampire, getSkillType());
    }
}

SummonGoreGland g_SummonGoreGland;
