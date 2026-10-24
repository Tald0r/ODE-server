//////////////////////////////////////////////////////////////////////////////
// Filename    : MultiThrowingAxe.cpp
// Written by  : excel96
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "MultiThrowingAxe.h"

#include "EffectMeteorStrike.h"
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
// Monster tile handler
//////////////////////////////////////////////////////////////////////////////
void MultiThrowingAxe::execute(Monster* pMonster, ZoneCoord_t X, ZoneCoord_t Y)

{
    cout << "TID[" << Thread::self() << "]" << getSkillHandlerName() << "begin(monster) " << endl;

    try {
        Zone* pZone = pMonster->getZone();
        Assert(pZone != NULL);

        GCSkillToTileOK2 _GCSkillToTileOK2;
        GCSkillToTileOK3 _GCSkillToTileOK3;
        GCSkillToTileOK4 _GCSkillToTileOK4;
        GCSkillToTileOK5 _GCSkillToTileOK5;
        GCSkillToTileOK6 _GCSkillToTileOK6;

        SkillType_t SkillType = SKILL_MULTI_THROWING_AXE;
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

        Dir_t dir = getDirection(pMonster->getX(), pMonster->getY(), X, Y);
        Dir_t dir2 = (dir + 7) % 8; // left
        Dir_t dir3 = (dir + 1) % 8; // right

        ZoneCoord_t tX[3], tY[3];

        // The original target (the middle of the three)
        X = tX[0] = pMonster->getX() + dirMoveMask[dir].x * 7;
        Y = tY[0] = pMonster->getY() + dirMoveMask[dir].y * 7;

        // Left of the original target
        tX[1] = pMonster->getX() + dirMoveMask[dir2].x * 7;
        tY[1] = pMonster->getY() + dirMoveMask[dir2].y * 7;

        // Right of the original target
        tX[2] = pMonster->getX() + dirMoveMask[dir3].x * 7;
        tY[2] = pMonster->getY() + dirMoveMask[dir3].y * 7;


        if (bRangeCheck && bHitRoll && bTileCheck) {
            Range_t Range = 1; // Always 1.

            // Compute the damage and the duration.
            SkillInput input(pMonster);
            input.SkillLevel = pMonster->getLevel();
            SkillOutput output;
            computeOutput(input, output);

            for (int i = 0; i < 3; i++) {
                if (!isValidZoneCoord(pZone, tX[i], tY[i]))
                    continue;
                Tile& tile = pZone->getTile(tX[i], tY[i]);

                // Create the effect object.
                EffectMeteorStrike* pEffect = new EffectMeteorStrike(pZone, tX[i], tY[i]);
                pEffect->setNextTime(output.Duration);
                pEffect->setUserObjectID(pMonster->getObjectID());
                pEffect->setBroadcastingEffect(false);
                pEffect->setDamage(output.Damage);
                pEffect->setSplashRatio(1, 75);
                pEffect->setSplashRatio(2, 50);

                // An effect attached to a tile must be given an OID.
                ObjectRegistry& objectregister = pZone->getObjectRegistry();
                objectregister.registerObject(pEffect);

                // Add the effect to the zone and to the tile.
                pZone->addEffect(pEffect);
                tile.addEffect(pEffect);
            }
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
            cList.push_back(pMonster);

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

MultiThrowingAxe g_MultiThrowingAxe;
