/////////////////////////////////////////////////////////////////////////////
// Filename    : YellowPoison.cpp
// Written by  :
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "YellowPoison.h"

#include "EffectYellowPoison.h"
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
//////////////////////////////////////////////////////////////////////////////
void YellowPoison::execute(Vampire* pVampire, ObjectID_t TargetObjectID, VampireSkillSlot* pVampireSkillSlot,
                           CEffectID_t CEffectID)

{
    Assert(pVampire != NULL);
    Assert(pVampireSkillSlot != NULL);

    try {
        Zone* pZone = pVampire->getZone();
        Assert(pZone != NULL);

        Creature* pTargetCreature = pZone->getCreature(TargetObjectID);

        if (pTargetCreature == NULL || !canAttack(pVampire, pTargetCreature)) {
            executeSkillFailException(pVampire, getSkillType());
            return;
        }

        execute(pVampire, pTargetCreature->getX(), pTargetCreature->getY(), pVampireSkillSlot, CEffectID);
    } catch (Throwable& t) {
        executeSkillFailException(pVampire, getSkillType());
    }
}

//////////////////////////////////////////////////////////////////////
//
// YellowPoison::execute()
//
//////////////////////////////////////////////////////////////////////
void YellowPoison::execute(Vampire* pVampire, ZoneCoord_t X, ZoneCoord_t Y, VampireSkillSlot* pVampireSkillSlot,
                           CEffectID_t CEffectID)

{
    Assert(pVampire != NULL);
    Assert(pVampireSkillSlot != NULL);

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

        // Knowledge of Poison gives a hit bonus of 10.
        int HitBonus = 0;
        if (pVampire->hasRankBonus(RankBonus::RANK_BONUS_KNOWLEDGE_OF_POISON)) {
            RankBonus* pRankBonus = pVampire->getRankBonus(RankBonus::RANK_BONUS_KNOWLEDGE_OF_POISON);
            Assert(pRankBonus != NULL);

            HitBonus = pRankBonus->getPoint();
        }

        int RequiredMP = decreaseConsumeMP(pVampire, pSkillInfo);
        bool bManaCheck = hasEnoughMana(pVampire, RequiredMP);
        bool bTimeCheck = verifyRunTime(pVampireSkillSlot);
        bool bRangeCheck = verifyDistance(pVampire, X, Y, pSkillInfo->getRange());
        bool bHitRoll = HitRoll::isSuccessMagic(pVampire, pSkillInfo, pVampireSkillSlot, HitBonus);

        bool bTileCheck = false;
        VSRect rect(0, 0, pZone->getWidth() - 1, pZone->getHeight() - 1);
        if (rect.ptInRect(X, Y)) {
            Tile& tile = pZone->getTile(X, Y);
            if (tile.canAddEffect())
                bTileCheck = true;
            // Skip a tile that already holds a SummonClay effect.
            if (tile.getEffect(Effect::EFFECT_CLASS_SUMMON_CLAY))
                bTileCheck = false;
            // end by Coffee
        }


        if (bManaCheck && bTimeCheck && bRangeCheck && bHitRoll && bTileCheck) {
            decreaseMana(pVampire, RequiredMP, _GCSkillToTileOK1);

            Tile& tile = pZone->getTile(X, Y);
            Range_t Range = 1;

            // Delete an existing effect of the same kind.
            Effect* pOldEffect = NULL;
            if ((pOldEffect = tile.getEffect(Effect::EFFECT_CLASS_YELLOW_POISON))) {
                ObjectID_t effectID = pOldEffect->getObjectID();
                pZone->deleteEffect(effectID); // fix me
            }

            SkillInput input(pVampire);
            SkillOutput output;
            computeOutput(input, output);

            // Create the effect object.
            EffectYellowPoison* pEffect = new EffectYellowPoison(pZone, X, Y);
            pEffect->setVampire();
            pEffect->setDeadline(output.Duration);
            pEffect->setDuration(output.Duration);
            pEffect->setLevel(pSkillInfo->getLevel());

            // An effect attached to a tile must be registered for an OID.
            ObjectRegistry& objectregister = pZone->getObjectRegistry();
            objectregister.registerObject(pEffect);

            // Add the effect to the zone and the tile.
            pZone->addEffect(pEffect);
            tile.addEffect(pEffect);

            // Affect a creature standing on the tile right away.
            bool bEffected = false;
            Creature* pTargetCreature = NULL;

            if (tile.hasCreature(Creature::MOVE_MODE_WALKING))
                pTargetCreature = tile.getCreature(Creature::MOVE_MODE_WALKING);

            ZoneCoord_t myX = pVampire->getX();
            ZoneCoord_t myY = pVampire->getY();

            if (pTargetCreature != NULL && (pTargetCreature->isSlayer() || pTargetCreature->isOusters())) {
                if (pEffect->affectCreature(pTargetCreature, false) == true) {
                    bEffected = true;

                    // The check above accepts only Slayers and Ousters, so this
                    // test is always true; it guards against a monster should the
                    // race check ever be widened.
                    if (pTargetCreature->isPC()) {
                        Player* pTargetPlayer = pTargetCreature->getPlayer();
                        bool bCanSee = canSee(pTargetCreature, pVampire);

                        if (bCanSee) {
                            _GCSkillToTileOK2.setObjectID(pVampire->getObjectID());
                            _GCSkillToTileOK2.setSkillType(SkillType);
                            _GCSkillToTileOK2.setX(X);
                            _GCSkillToTileOK2.setY(Y);
                            _GCSkillToTileOK2.setDuration(output.Duration);
                            _GCSkillToTileOK2.setRange(Range);
                            pTargetPlayer->sendPacket(&_GCSkillToTileOK2);
                        } else {
                            _GCSkillToTileOK6.setOrgXY(myX, myY);
                            _GCSkillToTileOK6.setSkillType(SkillType);
                            _GCSkillToTileOK6.setX(X);
                            _GCSkillToTileOK6.setY(Y);
                            _GCSkillToTileOK6.setDuration(output.Duration);
                            _GCSkillToTileOK6.setRange(Range);
                            _GCSkillToTileOK6.addCListElement(pTargetCreature->getObjectID());
                            pTargetPlayer->sendPacket(&_GCSkillToTileOK6);
                        }
                    }
                }
            }

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

            if (bEffected) {
                _GCSkillToTileOK1.addCListElement(pTargetCreature->getObjectID());
                _GCSkillToTileOK4.addCListElement(pTargetCreature->getObjectID());
                _GCSkillToTileOK5.addCListElement(pTargetCreature->getObjectID());
            }

            pPlayer->sendPacket(&_GCSkillToTileOK1);

            list<Creature*> cList;
            cList.push_back(pVampire);
            if (bEffected)
                cList.push_back(pTargetCreature);

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

//////////////////////////////////////////////////////////////////////////////
// Monster object handler
//////////////////////////////////////////////////////////////////////////////
void YellowPoison::execute(Monster* pMonster, Creature* pEnemy)

{
    Assert(pMonster != NULL);
    Assert(pEnemy != NULL);

    execute(pMonster, pEnemy->getX(), pEnemy->getY());
}

//////////////////////////////////////////////////////////////////////////////
// Monster tile handler
//////////////////////////////////////////////////////////////////////////////
void YellowPoison::execute(Monster* pMonster, ZoneCoord_t X, ZoneCoord_t Y)

{
    Assert(pMonster != NULL);

    try {
        Zone* pZone = pMonster->getZone();
        Assert(pZone != NULL);

        if (pMonster->isFlag(Effect::EFFECT_CLASS_HIDE)) {
            return;
        }
        if (pMonster->isFlag(Effect::EFFECT_CLASS_INVISIBILITY)) {
            addVisibleCreature(pZone, pMonster, true);
        }

        GCSkillToTileOK2 _GCSkillToTileOK2;
        GCSkillToTileOK3 _GCSkillToTileOK3;
        GCSkillToTileOK4 _GCSkillToTileOK4;
        GCSkillToTileOK5 _GCSkillToTileOK5;
        GCSkillToTileOK6 _GCSkillToTileOK6;

        SkillType_t SkillType = SKILL_YELLOW_POISON;
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
            Range_t Range = 1;

            // Delete the same effect if one is already present.
            Effect* pOldEffect = NULL;
            if ((pOldEffect = tile.getEffect(Effect::EFFECT_CLASS_YELLOW_POISON))) {
                ObjectID_t effectID = pOldEffect->getObjectID();
                pZone->deleteEffect(effectID); // fix me
            }

            SkillInput input(pMonster);
            SkillOutput output;
            computeOutput(input, output);

            // Create the effect object.
            EffectYellowPoison* pEffect = new EffectYellowPoison(pZone, X, Y);
            pEffect->setDeadline(output.Duration);
            pEffect->setDuration(output.Duration);
            pEffect->setLevel(pSkillInfo->getLevel() / 2);

            // An effect attached to a tile must be registered for an ObjectID.
            ObjectRegistry& objectregister = pZone->getObjectRegistry();
            objectregister.registerObject(pEffect);

            // Add the effect to the zone and the tile.
            pZone->addEffect(pEffect);
            tile.addEffect(pEffect);

            // Affect a creature standing on the tile right away.
            bool bEffected = false;
            Creature* pTargetCreature = NULL;

            if (tile.hasCreature(Creature::MOVE_MODE_WALKING))
                pTargetCreature = tile.getCreature(Creature::MOVE_MODE_WALKING);

            ZoneCoord_t myX = pMonster->getX();
            ZoneCoord_t myY = pMonster->getY();

            if (pTargetCreature != NULL && (pTargetCreature->isSlayer() || pTargetCreature->isOusters())) {
                if (pEffect->affectCreature(pTargetCreature, false) == true) {
                    bEffected = true;

                    // The check above accepts only Slayers and Ousters, so this
                    // test is always true; it guards against a monster should the
                    // race check ever be widened.
                    if (pTargetCreature->isPC()) {
                        Player* pTargetPlayer = pTargetCreature->getPlayer();
                        bool bCanSee = canSee(pTargetCreature, pMonster);

                        if (bCanSee) {
                            _GCSkillToTileOK2.setObjectID(pMonster->getObjectID());
                            _GCSkillToTileOK2.setSkillType(SkillType);
                            _GCSkillToTileOK2.setX(X);
                            _GCSkillToTileOK2.setY(Y);
                            _GCSkillToTileOK2.setDuration(output.Duration);
                            _GCSkillToTileOK2.setRange(Range);
                            pTargetPlayer->sendPacket(&_GCSkillToTileOK2);
                        } else {
                            _GCSkillToTileOK6.setOrgXY(myX, myY);
                            _GCSkillToTileOK6.setSkillType(SkillType);
                            _GCSkillToTileOK6.setX(X);
                            _GCSkillToTileOK6.setY(Y);
                            _GCSkillToTileOK6.setDuration(output.Duration);
                            _GCSkillToTileOK6.setRange(Range);
                            _GCSkillToTileOK6.addCListElement(pTargetCreature->getObjectID());
                            pTargetPlayer->sendPacket(&_GCSkillToTileOK6);
                        }
                    }
                }
            }

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

            if (bEffected) {
                _GCSkillToTileOK4.addCListElement(pTargetCreature->getObjectID());
                _GCSkillToTileOK5.addCListElement(pTargetCreature->getObjectID());
            }

            list<Creature*> cList;
            cList.push_back(pMonster);
            if (bEffected)
                cList.push_back(pTargetCreature);

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

YellowPoison g_YellowPoison;
