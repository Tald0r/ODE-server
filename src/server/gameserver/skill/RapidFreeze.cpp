//////////////////////////////////////////////////////////////////////////////
// Filename    : RapidFreeze.cpp
// Written by  : rallser
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "RapidFreeze.h"

#include "EffectRapidFreeze.h"
#include "GCAddEffectToTile.h"
#include "GCSkillToTileOK1.h"
#include "GCSkillToTileOK2.h"
#include "GCSkillToTileOK3.h"
#include "GCSkillToTileOK4.h"
#include "GCSkillToTileOK5.h"
#include "GCSkillToTileOK6.h"
#include "GameContext.h"
#include "RankBonus.h"

//////////////////////////////////////////////////////////////////////////////
// Ousters object handler
//////////////////////////////////////////////////////////////////////////////
void RapidFreeze::execute(Ousters* pOusters, ObjectID_t TargetObjectID, OustersSkillSlot* pOustersSkillSlot,
                          CEffectID_t CEffectID)

{
    Assert(pOusters != NULL);
    Assert(pOustersSkillSlot != NULL);

    try {
        Zone* pZone = pOusters->getZone();
        Assert(pZone != NULL);

        Creature* pTargetCreature = pZone->getCreature(TargetObjectID);


        // An NPC cannot be attacked.
        if (pTargetCreature == NULL // The zone returns NULL when the target is gone.
            || !canAttack(pOusters, pTargetCreature) || pTargetCreature->isNPC()) {
            executeSkillFailException(pOusters, getSkillType(), 0);
            return;
        }

        execute(pOusters, pTargetCreature->getX(), pTargetCreature->getY(), pOustersSkillSlot, CEffectID);
    } catch (Throwable& t) {
        executeSkillFailException(pOusters, getSkillType(), 0);
    }
}

//////////////////////////////////////////////////////////////////////////////
// Ousters tile handler
//////////////////////////////////////////////////////////////////////////////
void RapidFreeze::execute(Ousters* pOusters, ZoneCoord_t X, ZoneCoord_t Y, OustersSkillSlot* pOustersSkillSlot,
                          CEffectID_t CEffectID)

{
    Assert(pOusters != NULL);
    Assert(pOustersSkillSlot != NULL);

    try {
        Player* pPlayer = pOusters->getPlayer();
        Zone* pZone = pOusters->getZone();

        Assert(pPlayer != NULL);
        Assert(pZone != NULL);

        Item* pWeapon = pOusters->getWearItem(Ousters::WEAR_RIGHTHAND);
        if (pWeapon == NULL || pWeapon->getItemClass() != Item::ITEM_CLASS_OUSTERS_WRISTLET ||
            !pOusters->isRealWearingEx(Ousters::WEAR_RIGHTHAND)) {
            executeSkillFailException(pOusters, pOustersSkillSlot->getSkillType(), 0);
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

        int RequiredMP = (int)pSkillInfo->getConsumeMP();
        bool bManaCheck = hasEnoughMana(pOusters, RequiredMP);
        bool bTimeCheck = verifyRunTime(pOustersSkillSlot);
        bool bRangeCheck = verifyDistance(pOusters, X, Y, pSkillInfo->getRange());
        bool bHitRoll = HitRoll::isSuccessMagic(pOusters, pSkillInfo, pOustersSkillSlot);
        bool bSatisfyRequire = pOusters->satisfySkillRequire(pSkillInfo);

        bool bTileCheck = false;
        VSRect rect(0, 0, pZone->getWidth() - 1, pZone->getHeight() - 1);
        if (rect.ptInRect(X, Y)) {
            Tile& tile = pZone->getTile(X, Y);
            if (tile.canAddEffect())
                bTileCheck = true;
        }

        if (bManaCheck && bTimeCheck && bRangeCheck && bHitRoll && bTileCheck && bSatisfyRequire) {
            decreaseMana(pOusters, RequiredMP, _GCSkillToTileOK1);

            Tile& tile = pZone->getTile(X, Y);

            // Delete the same effect if one is already present.
            Effect* pOldEffect = tile.getEffect(Effect::EFFECT_CLASS_Rapid_Freeze);
            if (pOldEffect != NULL) {
                ObjectID_t effectID = pOldEffect->getObjectID();
                pZone->deleteEffect(effectID);
            }

            checkMine(pZone, X, Y);

            // Create the effect object.
            EffectRapidFreeze* pEffect = new EffectRapidFreeze(pZone, X, Y);
            pEffect->setUserObjectID(pOusters->getObjectID());
            pEffect->setDeadline(output.Duration);
            pEffect->setNextTime(10);
            pEffect->checkPosition();
            pEffect->setDamage(output.Damage);
            pEffect->setBroadcastingEffect(false);

            // An effect attached to a tile must be assigned an object ID.
            ObjectRegistry& objectregister = pZone->getObjectRegistry();
            objectregister.registerObject(pEffect);

            // Add the effect to the zone and the tile.
            pZone->addEffect(pEffect);
            tile.addEffect(pEffect);

            GCAddEffectToTile gcAddEffect;
            gcAddEffect.setXY(X, Y);
            gcAddEffect.setEffectID(pEffect->getSendEffectClass());
            gcAddEffect.setObjectID(pEffect->getObjectID());
            gcAddEffect.setDuration(output.Duration);

            pZone->broadcastPacket(X, Y, &gcAddEffect, pOusters);

            ZoneCoord_t myX = pOusters->getX();
            ZoneCoord_t myY = pOusters->getY();

            _GCSkillToTileOK1.setSkillType(SkillType);
            _GCSkillToTileOK1.setCEffectID(CEffectID);
            _GCSkillToTileOK1.setX(X);
            _GCSkillToTileOK1.setY(Y);
            _GCSkillToTileOK1.setDuration(output.Duration);
            _GCSkillToTileOK1.setGrade(0);

            _GCSkillToTileOK3.setObjectID(pOusters->getObjectID());
            _GCSkillToTileOK3.setSkillType(SkillType);
            _GCSkillToTileOK3.setX(X);
            _GCSkillToTileOK3.setY(Y);
            _GCSkillToTileOK3.setGrade(0);

            _GCSkillToTileOK4.setSkillType(SkillType);
            _GCSkillToTileOK4.setX(X);
            _GCSkillToTileOK4.setY(Y);
            _GCSkillToTileOK4.setDuration(output.Duration);
            _GCSkillToTileOK4.setGrade(0);

            _GCSkillToTileOK5.setObjectID(pOusters->getObjectID());
            _GCSkillToTileOK5.setSkillType(SkillType);
            _GCSkillToTileOK5.setX(X);
            _GCSkillToTileOK5.setY(Y);
            _GCSkillToTileOK5.setDuration(output.Duration);
            _GCSkillToTileOK5.setGrade(0);

            pPlayer->sendPacket(&_GCSkillToTileOK1);

            list<Creature*> cList;
            cList.push_back(pOusters);

            cList = pZone->broadcastSkillPacket(myX, myY, X, Y, &_GCSkillToTileOK5, cList);

            pZone->broadcastPacket(myX, myY, &_GCSkillToTileOK3, cList);
            pZone->broadcastPacket(X, Y, &_GCSkillToTileOK4, cList);

            pOustersSkillSlot->setRunTime(output.Delay);
        } else {
            executeSkillFailNormal(pOusters, getSkillType(), NULL, 0);
        }
    } catch (Throwable& t) {
        executeSkillFailException(pOusters, getSkillType(), 0);
    }
}

RapidFreeze g_RapidFreeze;
