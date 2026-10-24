//////////////////////////////////////////////////////////////////////////////
// Filename    : SatelliteBomb.cpp
// Written by  : excel96
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "SatelliteBomb.h"

#include "EffectSatelliteBombAim.h"
#include "EffectSatelliteBombPoint.h"
#include "GCAddEffect.h"
#include "GCAddEffectToTile.h"
#include "GCSkillToTileOK1.h"
#include "GCSkillToTileOK2.h"
#include "GCSkillToTileOK3.h"
#include "GCSkillToTileOK4.h"
#include "GCSkillToTileOK5.h"
#include "GCSkillToTileOK6.h"
#include "GameContext.h"
#include "ItemUtil.h"

//////////////////////////////////////////////////////////////////////////////
// Slayer object handler for the satellite bomb skill
//////////////////////////////////////////////////////////////////////////////
void SatelliteBomb::execute(Slayer* pSlayer, ObjectID_t TargetObjectID, SkillSlot* pSkillSlot, CEffectID_t CEffectID)

{
    // Validate the arguments.
    Assert(pSlayer != NULL);
    Assert(pSkillSlot != NULL);

    try {
        // Get the zone, check it, and look the target up by its object id.
        Zone* pZone = pSlayer->getZone();
        Assert(pZone != NULL);

        Creature* pTargetCreature = pZone->getCreature(TargetObjectID);


        // Check whether the target can be attacked.
        if (pTargetCreature == NULL || !canAttack(pSlayer, pTargetCreature) || pTargetCreature->isNPC()) {
            // Tell the client the skill failed.
            executeSkillFailException(pSlayer, getSkillType());
            return;
        }
        // Run the skill on the target's tile.
        execute(pSlayer, pTargetCreature->getX(), pTargetCreature->getY(), pSkillSlot, CEffectID);
    }
    // Error handling.
    catch (Throwable& t) {
        // Tell the client the skill failed.
        executeSkillFailException(pSlayer, getSkillType());
    }
}

//////////////////////////////////////////////////////////////////////////////
// Skill entry point: pSlayer the caster, X,Y the target tile, pSkillSlot the skill slot, CEffectID the effect id
//////////////////////////////////////////////////////////////////////////////
void SatelliteBomb::execute(Slayer* pSlayer, ZoneCoord_t X, ZoneCoord_t Y, SkillSlot* pSkillSlot, CEffectID_t CEffectID)

{
    try {
        // Get the Player that owns this Slayer.
        Player* pPlayer = pSlayer->getPlayer();
        // Get the zone the Slayer is in.
        Zone* pZone = pSlayer->getZone();
        // Both have to be present.
        Assert(pPlayer != NULL);
        Assert(pZone != NULL);

        // Check that a gun is equipped in the right hand.
        Item* pItem = pSlayer->getWearItem(Slayer::WEAR_RIGHTHAND);
        if (pItem == NULL || isArmsWeapon(pItem) == false) {
            // Tell the client the skill failed.
            executeSkillFailException(pSlayer, getSkillType());
            return;
        }

        GCSkillToTileOK1 _GCSkillToTileOK1;
        GCSkillToTileOK3 _GCSkillToTileOK3;
        GCSkillToTileOK4 _GCSkillToTileOK4;
        GCSkillToTileOK5 _GCSkillToTileOK5;
        GCSkillToTileOK6 _GCSkillToTileOK6;
        // Get the type of the skill being used.
        SkillType_t SkillType = pSkillSlot->getSkillType();
        // Get that skill's info from the skill info manager.
        SkillInfo* pSkillInfo = de::gameContext().skillInfos().getSkillInfo(SkillType);

        // RequiredMP  the MP the skill consumes.
        // bManaCheck  whether the caster has enough MP.
        // bTimeCheck  whether the skill's delay has elapsed.
        // bRangeCheck whether the target tile is in range.
        // bEffected   whether the skill is already active on the caster.
        // bTileCheck  whether the target tile can take an effect.
        int RequiredMP = pSkillInfo->getConsumeMP();
        bool bManaCheck = hasEnoughMana(pSlayer, RequiredMP);
        bool bTimeCheck = verifyRunTime(pSkillSlot);
        bool bRangeCheck = verifyDistance(pSlayer, X, Y, pSkillInfo->getRange());
        bool bEffected = pSlayer->isFlag(Effect::EFFECT_CLASS_SKILL_SATELLITE_BOMB_AIM);

        bool bTileCheck = false;
        VSRect rect(0, 0, pZone->getWidth() - 1, pZone->getHeight() - 1);
        if (rect.ptInRect(X, Y)) {
            Tile& tile = pZone->getTile(X, Y);
            if (tile.canAddEffect())
                bTileCheck = true;
        }
        bool bUseSkill = false;
        bUseSkill = true;
        // Run the skill only when every check passes.
        if (bManaCheck && bTimeCheck && bRangeCheck && !bEffected && bTileCheck && bUseSkill) {
            // Consume the MP.
            decreaseMana(pSlayer, RequiredMP, _GCSkillToTileOK1);

            // Build the skill input and output and compute the skill's values.
            SkillInput input(pSlayer, pSkillSlot);
            SkillOutput output;
            computeOutput(input, output);

            Tile& tile = pZone->getTile(X, Y);

            // Remove an existing SATELLITE_BOMB_POINT effect from the tile.
            Effect* pOldEffect = tile.getEffect(Effect::EFFECT_CLASS_SKILL_SATELLITE_BOMB_POINT);
            if (pOldEffect != NULL) {
                ObjectID_t effectID = pOldEffect->getObjectID();
                pZone->deleteEffect(effectID);
            }

            // Create the aim effect.
            EffectSatelliteBombAim* pEffect = new EffectSatelliteBombAim(pSlayer, pZone, X, Y);
            // Set the effect's damage.
            pEffect->setDamage(output.Damage);
            // Set the effect's duration.
            pEffect->setDeadline(output.Duration);

            pSlayer->addEffect(pEffect);
            pSlayer->setFlag(Effect::EFFECT_CLASS_SKILL_SATELLITE_BOMB_AIM);

            ZoneCoord_t myX = pSlayer->getX();
            ZoneCoord_t myY = pSlayer->getY();

            _GCSkillToTileOK1.setSkillType(SkillType);
            _GCSkillToTileOK1.setCEffectID(CEffectID);
            _GCSkillToTileOK1.setX(X);
            _GCSkillToTileOK1.setY(Y);
            _GCSkillToTileOK1.setDuration(output.Duration);
            _GCSkillToTileOK1.setRange(output.Range);

            _GCSkillToTileOK3.setObjectID(pSlayer->getObjectID());
            _GCSkillToTileOK3.setSkillType(SkillType);
            _GCSkillToTileOK3.setX(X);
            _GCSkillToTileOK3.setY(Y);

            _GCSkillToTileOK4.setSkillType(SkillType);
            _GCSkillToTileOK4.setX(X);
            _GCSkillToTileOK4.setY(Y);
            _GCSkillToTileOK4.setDuration(output.Duration);
            _GCSkillToTileOK4.setRange(output.Range);

            _GCSkillToTileOK5.setObjectID(pSlayer->getObjectID());
            _GCSkillToTileOK5.setSkillType(SkillType);
            _GCSkillToTileOK5.setX(X);
            _GCSkillToTileOK5.setY(Y);
            _GCSkillToTileOK5.setDuration(output.Duration);
            _GCSkillToTileOK5.setRange(output.Range);

            pPlayer->sendPacket(&_GCSkillToTileOK1);

            list<Creature*> cList;
            cList.push_back(pSlayer);

            cList = pZone->broadcastSkillPacket(myX, myY, X, Y, &_GCSkillToTileOK5, cList);

            pZone->broadcastPacket(myX, myY, &_GCSkillToTileOK3, cList);
            pZone->broadcastPacket(X, Y, &_GCSkillToTileOK4, cList);

            // Broadcast the SatelliteBombAim effect.
            GCAddEffect gcAddAimEffect;
            gcAddAimEffect.setObjectID(pSlayer->getObjectID());
            gcAddAimEffect.setEffectID(Effect::EFFECT_CLASS_SKILL_SATELLITE_BOMB_AIM);
            gcAddAimEffect.setDuration(output.Duration);
            pZone->broadcastPacket(myX, myY, &gcAddAimEffect);

            ///////////////////////////////////////////////////////////////////
            // Create the SatelliteBombPoint effect, attach it and broadcast it.
            ///////////////////////////////////////////////////////////////////
            EffectSatelliteBombPoint* pPointEffect = new EffectSatelliteBombPoint(pZone, X, Y);
            pPointEffect->setDeadline(output.Duration);

            ObjectRegistry& objectRegister = pZone->getObjectRegistry();
            objectRegister.registerObject(pPointEffect);

            pZone->addEffect(pPointEffect);
            tile.addEffect(pPointEffect);

            GCAddEffectToTile gcAddEffectToTile;
            gcAddEffectToTile.setObjectID(pPointEffect->getObjectID());
            gcAddEffectToTile.setEffectID(pPointEffect->getEffectClass());
            gcAddEffectToTile.setXY(X, Y);
            gcAddEffectToTile.setDuration(output.Duration);

            pZone->broadcastPacket(X, Y, &gcAddEffectToTile);

            pSkillSlot->setRunTime(output.Delay);
        } else {
            executeSkillFailNormal(pSlayer, getSkillType(), NULL);
        }
    } catch (Throwable& t) {
        executeSkillFailException(pSlayer, getSkillType());
    }
}

void SatelliteBomb::execute(Monster* pMonster, ZoneCoord_t X, ZoneCoord_t Y) {
    try {
        cout << pMonster->getName() << " use gunshotguiadance" << endl;
        Zone* pZone = pMonster->getZone();
        Assert(pZone != NULL);

        GCSkillToTileOK2 _GCSkillToTileOK2;
        GCSkillToTileOK3 _GCSkillToTileOK3;
        GCSkillToTileOK4 _GCSkillToTileOK4;
        GCSkillToTileOK5 _GCSkillToTileOK5;
        GCSkillToTileOK6 _GCSkillToTileOK6;

        SkillType_t SkillType = getSkillType();
        bool bTileCheck = false;
        VSRect rect(0, 0, pZone->getWidth() - 1, pZone->getHeight() - 1);
        if (rect.ptInRect(X, Y)) {
            Tile& tile = pZone->getTile(X, Y);
            if (tile.canAddEffect())
                bTileCheck = true;
        }

        if (bTileCheck) {
            Tile& tile = pZone->getTile(X, Y);

            ZoneCoord_t myX = pMonster->getX();
            ZoneCoord_t myY = pMonster->getY();

            // Delete the same effect if it is already there.
            Effect* pOldEffect = tile.getEffect(Effect::EFFECT_CLASS_SKILL_SATELLITE_BOMB_POINT);
            if (pOldEffect != NULL) {
                ObjectID_t effectID = pOldEffect->getObjectID();
                pZone->deleteEffect(effectID);
            }

            // Compute the damage and the duration.
            SkillInput input(pMonster);
            SkillOutput output;
            computeOutput(input, output);

            // Create the effect object.
            EffectSatelliteBombAim* pEffect = new EffectSatelliteBombAim(pMonster, pZone, X, Y);
            pEffect->setDeadline(output.Duration);
            pEffect->setDamage(output.Damage);

            pMonster->setFlag(Effect::EFFECT_CLASS_SKILL_SATELLITE_BOMB_AIM);
            pMonster->addEffect(pEffect);

            _GCSkillToTileOK3.setObjectID(pMonster->getObjectID());
            _GCSkillToTileOK3.setSkillType(SkillType);
            _GCSkillToTileOK3.setX(X);
            _GCSkillToTileOK3.setY(Y);

            _GCSkillToTileOK4.setSkillType(SkillType);
            _GCSkillToTileOK4.setX(X);
            _GCSkillToTileOK4.setY(Y);
            _GCSkillToTileOK4.setDuration(output.Duration);
            _GCSkillToTileOK4.setRange(1);

            _GCSkillToTileOK5.setObjectID(pMonster->getObjectID());
            _GCSkillToTileOK5.setSkillType(SkillType);
            _GCSkillToTileOK5.setX(X);
            _GCSkillToTileOK5.setY(Y);
            _GCSkillToTileOK5.setDuration(output.Duration);
            _GCSkillToTileOK5.setRange(1);

            list<Creature*> cList;
            cList.push_back(pMonster);

            cList = pZone->broadcastSkillPacket(myX, myY, X, Y, &_GCSkillToTileOK5, cList);

            pZone->broadcastPacket(myX, myY, &_GCSkillToTileOK3, cList);
            pZone->broadcastPacket(X, Y, &_GCSkillToTileOK4, cList);

            // Broadcast the SatelliteBombAim effect.
            GCAddEffect gcAddAimEffect;
            gcAddAimEffect.setObjectID(pMonster->getObjectID());
            gcAddAimEffect.setEffectID(Effect::EFFECT_CLASS_SKILL_SATELLITE_BOMB_AIM);
            gcAddAimEffect.setDuration(output.Duration);
            pZone->broadcastPacket(myX, myY, &gcAddAimEffect);

            EffectSatelliteBombPoint* pPointEffect = new EffectSatelliteBombPoint(pZone, X, Y);
            pPointEffect->setDeadline(output.Duration);

            ObjectRegistry& objectRegister = pZone->getObjectRegistry();
            objectRegister.registerObject(pPointEffect);

            pZone->addEffect(pPointEffect);
            tile.addEffect(pPointEffect);

            GCAddEffectToTile gcAddEffectToTile;
            gcAddEffectToTile.setObjectID(pPointEffect->getObjectID());
            gcAddEffectToTile.setEffectID(pPointEffect->getEffectClass());
            gcAddEffectToTile.setXY(X, Y);
            gcAddEffectToTile.setDuration(output.Duration);

            pZone->broadcastPacket(X, Y, &gcAddEffectToTile);
        } else {
            executeSkillFailNormal(pMonster, getSkillType(), NULL);
        }
    } catch (Throwable& t) {
        executeSkillFailException(pMonster, getSkillType());
    }
}

SatelliteBomb g_SatelliteBomb;
