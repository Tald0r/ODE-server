//////////////////////////////////////////////////////////////////////////////
// Filename    : GrenadeAttack.cpp
// Written by  : excel96
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "GrenadeAttack.h"

#include "EffectMeteorStrike.h"
#include "GCSkillToTileOK1.h"
#include "GCSkillToTileOK2.h"
#include "GCSkillToTileOK3.h"
#include "GCSkillToTileOK4.h"
#include "GCSkillToTileOK5.h"
#include "GCSkillToTileOK6.h"
#include "GameContext.h"
#include "RankBonus.h"

//////////////////////////////////////////////////////////////////////////////
// Monster tile handler
//////////////////////////////////////////////////////////////////////////////
void GrenadeAttack::execute(Monster* pMonster, ZoneCoord_t X, ZoneCoord_t Y)

{
    try {
        Zone* pZone = pMonster->getZone();
        Assert(pZone != NULL);

        GCSkillToTileOK2 _GCSkillToTileOK2;
        GCSkillToTileOK3 _GCSkillToTileOK3;
        GCSkillToTileOK4 _GCSkillToTileOK4;
        GCSkillToTileOK5 _GCSkillToTileOK5;
        GCSkillToTileOK6 _GCSkillToTileOK6;

        SkillType_t SkillType = SKILL_GRENADE_ATTACK;
        SkillInfo* pSkillInfo = de::gameContext().skillInfos().getSkillInfo(SkillType);

        bool bRangeCheck = verifyDistance(pMonster, X, Y, pSkillInfo->getRange());
        bool bHitRoll = HitRoll::isSuccessMagic(pMonster, pSkillInfo);

        bool bTileCheck = false;
        VSRect rect(0, 0, pZone->getWidth() - 1, pZone->getHeight() - 1);
        if (rect.ptInRect(X, Y)) {
            Tile& tile = pZone->getTile(X, Y);
            if (tile.canAddEffect())
                bTileCheck = true;
        }

        if (bRangeCheck && bHitRoll && bTileCheck) {
            Tile& tile = pZone->getTile(X, Y);
            Range_t Range = 1; // Always 1.


            // Compute the damage and the duration.
            SkillInput input(pMonster);
            input.SkillLevel = pMonster->getLevel();
            SkillOutput output;
            computeOutput(input, output);

            // Create the effect object.
            EffectMeteorStrike* pEffect = new EffectMeteorStrike(pZone, X, Y);
            pEffect->setNextTime(output.Duration);
            pEffect->setUserObjectID(pMonster->getObjectID());
            pEffect->setDamage(output.Damage);

            // An effect attached to a tile must be assigned an object ID.
            ObjectRegistry& objectregister = pZone->getObjectRegistry();
            objectregister.registerObject(pEffect);

            // Add the effect to the zone and the tile.
            pZone->addEffect(pEffect);
            tile.addEffect(pEffect);

            // Apply the effect immediately if a creature is on the tile.


            ZoneCoord_t myX = pMonster->getX();
            ZoneCoord_t myY = pMonster->getY();


            _GCSkillToTileOK3.setObjectID(pMonster->getObjectID());
            _GCSkillToTileOK3.setSkillType(SkillType);
            _GCSkillToTileOK3.setX(X);
            _GCSkillToTileOK3.setY(Y);

            _GCSkillToTileOK4.setSkillType(SkillType);
            _GCSkillToTileOK4.setX(X);
            _GCSkillToTileOK4.setY(Y);
            _GCSkillToTileOK4.setDuration(output.Duration);
            _GCSkillToTileOK4.setRange(Range);

            _GCSkillToTileOK5.setObjectID(pMonster->getObjectID());
            _GCSkillToTileOK5.setSkillType(SkillType);
            _GCSkillToTileOK5.setX(X);
            _GCSkillToTileOK5.setY(Y);
            _GCSkillToTileOK5.setDuration(output.Duration);
            _GCSkillToTileOK5.setRange(Range);

            list<Creature*> cList;
            cList = pZone->broadcastSkillPacket(myX, myY, X, Y, &_GCSkillToTileOK5, cList);

            pZone->broadcastPacket(myX, myY, &_GCSkillToTileOK3, cList);
            pZone->broadcastPacket(X, Y, &_GCSkillToTileOK4, cList);
        } else {
            executeSkillFailNormal(pMonster, getSkillType(), NULL);
        }
    } catch (Throwable& t) {
        executeSkillFailException(pMonster, getSkillType());
    }
}

GrenadeAttack g_GrenadeAttack;
