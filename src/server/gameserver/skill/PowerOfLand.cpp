////////////////////////////////////////////////////////////////////////////////
// Project     : DARKEDEN
// Module      : Skill - Effect
// File Name   : PowerOfLand.cpp
////////////////////////////////////////////////////////////////////////////////

#include "PowerOfLand.h"

#include "EffectTileStorm.h"
#include "GCAddEffect.h"
#include "GCAddEffectToTile.h"
#include "GCSkillToTileOK1.h"
#include "GCSkillToTileOK2.h"
#include "GCSkillToTileOK3.h"
#include "GCSkillToTileOK4.h"
#include "GCSkillToTileOK5.h"
#include "GCSkillToTileOK6.h"
#include "GameContext.h"

//////////////////////////////////////////////////////////////////////////////
// Slayer object handler
//////////////////////////////////////////////////////////////////////////////
void PowerOfLand::execute(Slayer* pSlayer, ObjectID_t TargetObjectID, SkillSlot* pSkillSlot, CEffectID_t CEffectID)

{
    // Slayer Object Assertion
    Assert(pSlayer != NULL);
    Assert(pSkillSlot != NULL);

    try {
        Zone* pZone = pSlayer->getZone();
        Assert(pZone != NULL);

        Creature* pTargetCreature = pZone->getCreature(TargetObjectID);

        // A missing target fails the skill instead of throwing.
        if (pTargetCreature == NULL || !canAttack(pSlayer, pTargetCreature) || pTargetCreature->isNPC()) {
            executeSkillFailException(pSlayer, getSkillType());

            return;
        }

        execute(pSlayer, pTargetCreature->getX(), pTargetCreature->getY(), pSkillSlot, CEffectID);
    } catch (Throwable& t) {
        executeSkillFailException(pSlayer, getSkillType());
    }
}

//////////////////////////////////////////////////////////////////////////////
// Slayer tile handler
// Handler used when a Slayer uses the skill on a tile
//////////////////////////////////////////////////////////////////////////////
void PowerOfLand::execute(Slayer* pSlayer, ZoneCoord_t X, ZoneCoord_t Y, SkillSlot* pSkillSlot, CEffectID_t CEffectID)

{
    try {
        Player* pPlayer = pSlayer->getPlayer();
        Zone* pZone = pSlayer->getZone();

        Assert(pPlayer != NULL);
        Assert(pZone != NULL);

        GCSkillToTileOK1 _GCSkillToTileOK1;
        GCSkillToTileOK2 _GCSkillToTileOK2;
        GCSkillToTileOK3 _GCSkillToTileOK3;
        GCSkillToTileOK4 _GCSkillToTileOK4;
        GCSkillToTileOK5 _GCSkillToTileOK5;
        GCSkillToTileOK6 _GCSkillToTileOK6;

        SkillType_t SkillType = pSkillSlot->getSkillType();
        SkillInfo* pSkillInfo = de::gameContext().skillInfos().getSkillInfo(SkillType);

        ZoneCoord_t myX = pSlayer->getX();
        ZoneCoord_t myY = pSlayer->getY();

        int RequiredMP = (int)pSkillInfo->getConsumeMP();
        bool bManaCheck = hasEnoughMana(pSlayer, RequiredMP);
        bool bTimeCheck = verifyRunTime(pSkillSlot);
        bool bRangeCheck = verifyDistance(pSlayer, X, Y, pSkillInfo->getRange());

        // The skill is treated as a success, and the damage is computed
        // per creature in EffectTileStorm::affect().
        // 2003.1.8 by bezz

        bool bTileCheck = false;
        Tile& tile = pZone->getTile(X, Y);
        VSRect rect(0, 0, pZone->getWidth() - 1, pZone->getHeight() - 1);
        if (rect.ptInRect(X, Y)) {
            if (tile.canAddEffect())
                bTileCheck = true;
        }

        if (bManaCheck && bTimeCheck && bRangeCheck && bTileCheck) {
            decreaseMana(pSlayer, RequiredMP, _GCSkillToTileOK1);

            // calculate damage and duration time
            SkillInput input(pSlayer, pSkillSlot);
            SkillOutput output;
            computeOutput(input, output);

            Range_t Range = 5;

            EffectTileStorm* pEffect = new EffectTileStorm(pZone, X, Y);
            pEffect->setUserObjectID(pSlayer->getObjectID());
            pEffect->setDamage(output.Damage);
            pEffect->setSkillType(SkillType);
            pEffect->setStormTime(1);
            pEffect->setTick(output.Tick);
            pEffect->setNextTime(output.Tick);

            ObjectRegistry& objectregister = pZone->getObjectRegistry();
            objectregister.registerObject(pEffect);

            pZone->addEffect(pEffect);
            tile.addEffect(pEffect);

            // For the skill user
            _GCSkillToTileOK1.setSkillType(SkillType);
            _GCSkillToTileOK1.setCEffectID(CEffectID);
            _GCSkillToTileOK1.setX(X);
            _GCSkillToTileOK1.setY(Y);
            _GCSkillToTileOK1.setDuration(output.Duration);
            _GCSkillToTileOK1.setRange(Range);

            // For those who can see only the skill user
            _GCSkillToTileOK3.setObjectID(pSlayer->getObjectID());
            _GCSkillToTileOK3.setSkillType(SkillType);
            _GCSkillToTileOK3.setX(X);
            _GCSkillToTileOK3.setY(Y);

            // For those who can see only the target
            _GCSkillToTileOK4.setSkillType(SkillType);
            _GCSkillToTileOK4.setX(X);
            _GCSkillToTileOK4.setY(Y);
            _GCSkillToTileOK4.setDuration(output.Duration);
            _GCSkillToTileOK4.setRange(Range);

            // For those who can see both the skill user and the target
            _GCSkillToTileOK5.setObjectID(pSlayer->getObjectID());
            _GCSkillToTileOK5.setSkillType(SkillType);
            _GCSkillToTileOK5.setX(X);
            _GCSkillToTileOK5.setY(Y);
            _GCSkillToTileOK5.setDuration(output.Duration);
            _GCSkillToTileOK5.setRange(Range);

            // Send the packet to the skill user.
            pPlayer->sendPacket(&_GCSkillToTileOK1);

            // Broadcast to those who can see both the skill user and the target.
            // Record who received the OK5 packet after broadcasting.
            // Those recorded here are excluded from later broadcasts.
            list<Creature*> cList;
            cList.push_back(pSlayer);
            cList = pZone->broadcastSkillPacket(myX, myY, X, Y, &_GCSkillToTileOK5, cList);

            // Broadcast to those who can see the skill user.
            pZone->broadcastPacket(myX, myY, &_GCSkillToTileOK3, cList);

            // Broadcast to those who can see the target.
            pZone->broadcastPacket(X, Y, &_GCSkillToTileOK4, cList);

            // Set the skill delay.
            pSkillSlot->setRunTime(output.Delay);

        } else {
            executeSkillFailNormal(pSlayer, getSkillType(), NULL);
        }
    } catch (Throwable& t) {
        executeSkillFailException(pSlayer, getSkillType());
    }
}


PowerOfLand g_PowerOfLand;
