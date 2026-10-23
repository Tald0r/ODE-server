//////////////////////////////////////////////////////////////////////////////
// Filename    : Prominence.cpp
// Written by  : excel96
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "Prominence.h"

#include "EffectProminence.h"
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
void Prominence::execute(Ousters* pOusters, ObjectID_t TargetObjectID, OustersSkillSlot* pOustersSkillSlot,
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
        Zone* pZone = pOusters->getZone();
        Assert(pZone != NULL);

        Creature* pTargetCreature = pZone->getCreature(TargetObjectID);


        // An NPC cannot be attacked.
        if (pTargetCreature == NULL // The zone returns NULL when the target is gone.
            || !canAttack(pOusters, pTargetCreature) || pTargetCreature->isNPC()) {
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
void Prominence::execute(Ousters* pOusters, ZoneCoord_t X, ZoneCoord_t Y, OustersSkillSlot* pOustersSkillSlot,
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
        bool bRangeCheck = verifyDistance(pOusters, X, Y, output.Range);
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

            int oX, oY;

            for (oX = X - 1; oX <= X + 1; ++oX)
                for (oY = Y - 1; oY <= Y + 1; ++oY) {
                    if (!rect.ptInRect(oX, oY))
                        continue;

                    Tile& tile = pZone->getTile(oX, oY);
                    if (!tile.canAddEffect())
                        continue;

                    // Cannot be added where mercy ground is present.
                    if (tile.getEffect(Effect::EFFECT_CLASS_MERCY_GROUND) != NULL)
                        continue;
                    if (tile.getEffect(Effect::EFFECT_CLASS_TRYING_POSITION) != NULL)
                        continue;
                    if (tile.getEffect(Effect::EFFECT_CLASS_HEAVEN_GROUND) != NULL)
                        continue;
                    if (tile.getEffect(Effect::EFFECT_CLASS_SUMMON_CLAY) != NULL)
                        continue;


                    // Deletes the same effect if it already exists.
                    Effect* pOldEffect = tile.getEffect(Effect::EFFECT_CLASS_PROMINENCE);
                    if (pOldEffect != NULL) {
                        ObjectID_t effectID = pOldEffect->getObjectID();
                        pZone->deleteEffect(effectID);
                    }

                    checkMine(pZone, oX, oY);

                    // Creates the effect object.
                    EffectProminence* pEffect = new EffectProminence(pZone, oX, oY);
                    pEffect->setUserObjectID(pOusters->getObjectID());
                    pEffect->setDeadline(output.Duration);
                    pEffect->setNextTime(0);
                    pEffect->setTick(output.Tick);

                    pEffect->setDamage(output.Damage);
                    pEffect->setLevel(pOustersSkillSlot->getExpLevel());

                    if (Grade > 0) {
                        if (Grade == 1)
                            pEffect->setSendEffectClass(Effect::EFFECT_CLASS_PROMINENCE_2);
                        else
                            pEffect->setSendEffectClass(Effect::EFFECT_CLASS_PROMINENCE_3);
                    }

                    // An effect attached to a tile has to be given an object ID.
                    ObjectRegistry& objectregister = pZone->getObjectRegistry();
                    objectregister.registerObject(pEffect);

                    // Adds the effect to the zone and to the tile.
                    pZone->addEffect(pEffect);
                    tile.addEffect(pEffect);

                    GCAddEffectToTile gcAddEffect;
                    gcAddEffect.setXY(oX, oY);
                    gcAddEffect.setEffectID(pEffect->getSendEffectClass());
                    gcAddEffect.setObjectID(pEffect->getObjectID());
                    gcAddEffect.setDuration(output.Duration);

                    pZone->broadcastPacket(oX, oY, &gcAddEffect, pOusters);
                }

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

Prominence g_Prominence;
