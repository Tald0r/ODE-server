//////////////////////////////////////////////////////////////////////////////
// Filename    : SummonMiga.cpp
// Written by  : excel96
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "SummonMiga.h"

#include "CreatureUtil.h"
#include "EffectKillTimer.h"
#include "GCAddEffect.h"
#include "GCAddEffectToTile.h"
#include "GCSkillToTileOK1.h"
#include "GCSkillToTileOK2.h"
#include "GCSkillToTileOK3.h"
#include "GCSkillToTileOK4.h"
#include "GCSkillToTileOK5.h"
#include "GCSkillToTileOK6.h"
#include "GameContext.h"
#include "MonsterAI.h"

SummonMiga::SummonMiga() {}

//////////////////////////////////////////////////////////////////////////////
// Ousters object handler
//////////////////////////////////////////////////////////////////////////////
void SummonMiga::execute(Ousters* pOusters, ObjectID_t TargetObjectID, OustersSkillSlot* pOustersSkillSlot,
                         CEffectID_t CEffectID)

{
    Assert(pOusters != NULL);
    Assert(pOustersSkillSlot != NULL);

    BYTE Grade = 0;
    if (pOustersSkillSlot->getExpLevel() <= 10)
        Grade = 0;
    else if (pOustersSkillSlot->getExpLevel() <= 20)
        Grade = 1;
    else if (pOustersSkillSlot->getExpLevel() < 30)
        Grade = 2;
    else
        Grade = 3;

    try {
        Zone* pZone = pOusters->getZone();
        Assert(pZone != NULL);

        Creature* pTargetCreature = pZone->getCreature(TargetObjectID);


        // An NPC cannot be attacked.
        if (pTargetCreature == NULL // The zone returns NULL when the target is gone.
            || pTargetCreature->isNPC()) {
            executeSkillFailException(pOusters, getSkillType(), Grade);
            return;
        }

        execute(pOusters, pTargetCreature->getX(), pTargetCreature->getY(), pOustersSkillSlot, CEffectID);
    } catch (Throwable& t) {
        executeSkillFailException(pOusters, getSkillType(), Grade);
    }
}

//////////////////////////////////////////////////////////////////////////////
// Ousters tile handler
//////////////////////////////////////////////////////////////////////////////
void SummonMiga::execute(Ousters* pOusters, ZoneCoord_t X, ZoneCoord_t Y, OustersSkillSlot* pOustersSkillSlot,
                         CEffectID_t CEffectID)

{
    Assert(pOusters != NULL);
    Assert(pOustersSkillSlot != NULL);

    BYTE Grade = 0;
    if (pOustersSkillSlot->getExpLevel() < 15)
        Grade = 0;
    else if (pOustersSkillSlot->getExpLevel() < 30)
        Grade = 1;
    else
        Grade = 2;

    try {
        Player* pPlayer = pOusters->getPlayer();
        Zone* pZone = pOusters->getZone();

        Assert(pPlayer != NULL);
        Assert(pZone != NULL);

        Item* pWeapon = pOusters->getWearItem(Ousters::WEAR_RIGHTHAND);
        if (pWeapon == NULL || pWeapon->getItemClass() != Item::ITEM_CLASS_OUSTERS_WRISTLET ||
            !pOusters->isRealWearingEx(Ousters::WEAR_RIGHTHAND)) {
            executeSkillFailException(pOusters, pOustersSkillSlot->getSkillType(), Grade);
            return;
        }

        GCSkillToTileOK1 _GCSkillToTileOK1;
        GCSkillToTileOK2 _GCSkillToTileOK2;
        GCSkillToTileOK3 _GCSkillToTileOK3;
        GCSkillToTileOK4 _GCSkillToTileOK4;
        GCSkillToTileOK5 _GCSkillToTileOK5;
        GCSkillToTileOK6 _GCSkillToTileOK6;

        SkillType_t SkillType = pOustersSkillSlot->getSkillType();
        SkillInfo* pSkillInfo = de::gameContext().skillInfos().getSkillInfo(SkillType);

        // Compute the damage and the duration.
        SkillInput input(pOusters, pOustersSkillSlot);
        SkillOutput output;
        computeOutput(input, output);

        int RequiredMP = (int)pSkillInfo->getConsumeMP() + pOustersSkillSlot->getExpLevel() / 3;
        bool bManaCheck = hasEnoughMana(pOusters, RequiredMP);
        bool bTimeCheck = verifyRunTime(pOustersSkillSlot);
        bool bRangeCheck = verifyDistance(pOusters, X, Y, pSkillInfo->getRange());
        bool bHitRoll = HitRoll::isSuccessMagic(pOusters, pSkillInfo, pOustersSkillSlot);
        bool bSatisfyRequire = pOusters->satisfySkillRequire(pSkillInfo);

        bool bTileCheck = false;

        TPOINT pt = findSuitablePosition(pZone, X, Y, Creature::MOVE_MODE_WALKING);
        if (pt.x != -1) {
            bTileCheck = true;
        }

        if (bManaCheck && bTimeCheck && bRangeCheck && bHitRoll && bTileCheck && bSatisfyRequire) {
            decreaseMana(pOusters, RequiredMP, _GCSkillToTileOK1);

            Monster* pMiga = new Monster(MIGA_TYPE);
            pMiga->setName("Miga");
            pMiga->setClanType(33);
            pMiga->setTreasure(false);
            pMiga->setScanEnemy(true);
            pMiga->setOwnerObjectID(pOusters->getObjectID());
            pMiga->setFlag(Effect::EFFECT_CLASS_NO_DAMAGE);
            pMiga->removeFlag(Effect::EFFECT_CLASS_HIDE);
            pMiga->setMoveMode(Creature::MOVE_MODE_WALKING);
            pMiga->setDEX(input.SkillLevel);
            pMiga->setINT(pOusters->getINT());

            pZone->addCreature(pMiga, X, Y, 2);
            X = pMiga->getX();
            Y = pMiga->getY();

            cout << pMiga->toString() << " summoned at " << X << ", " << Y << "." << endl;

            EffectKillTimer* pCreatureEffect = new EffectKillTimer(pMiga);
            pCreatureEffect->setDeadline(output.Duration);

            pMiga->setFlag(pCreatureEffect->getEffectClass());
            pMiga->addEffect(pCreatureEffect);
            pZone->monsterScan(pMiga, X, Y, 0);

            GCAddEffect gcAddEffect;
            gcAddEffect.setObjectID(pMiga->getObjectID());
            gcAddEffect.setEffectID(pCreatureEffect->getSendEffectClass());
            pZone->broadcastPacket(X, Y, &gcAddEffect);

            ZoneCoord_t myX = pOusters->getX();
            ZoneCoord_t myY = pOusters->getY();

            _GCSkillToTileOK1.setSkillType(SkillType);
            _GCSkillToTileOK1.setCEffectID(CEffectID);
            _GCSkillToTileOK1.setX(X);
            _GCSkillToTileOK1.setY(Y);
            _GCSkillToTileOK1.setDuration(output.Duration);
            _GCSkillToTileOK1.setGrade(Grade);

            _GCSkillToTileOK3.setObjectID(pOusters->getObjectID());
            _GCSkillToTileOK3.setSkillType(SkillType);
            _GCSkillToTileOK3.setX(X);
            _GCSkillToTileOK3.setY(Y);
            _GCSkillToTileOK3.setGrade(Grade);

            _GCSkillToTileOK4.setSkillType(SkillType);
            _GCSkillToTileOK4.setX(X);
            _GCSkillToTileOK4.setY(Y);
            _GCSkillToTileOK4.setDuration(output.Duration);
            _GCSkillToTileOK4.setGrade(Grade);

            _GCSkillToTileOK5.setObjectID(pOusters->getObjectID());
            _GCSkillToTileOK5.setSkillType(SkillType);
            _GCSkillToTileOK5.setX(X);
            _GCSkillToTileOK5.setY(Y);
            _GCSkillToTileOK5.setDuration(output.Duration);
            _GCSkillToTileOK5.setGrade(Grade);

            pPlayer->sendPacket(&_GCSkillToTileOK1);

            list<Creature*> cList;
            cList.push_back(pOusters);

            cList = pZone->broadcastSkillPacket(myX, myY, X, Y, &_GCSkillToTileOK5, cList);

            pZone->broadcastPacket(myX, myY, &_GCSkillToTileOK3, cList);
            pZone->broadcastPacket(X, Y, &_GCSkillToTileOK4, cList);

            pOustersSkillSlot->setRunTime(output.Delay);
        } else {
            executeSkillFailNormal(pOusters, getSkillType(), NULL, Grade);
        }
    } catch (Throwable& t) {
        executeSkillFailException(pOusters, getSkillType(), Grade);
    }
}

SummonMiga g_SummonMiga;
