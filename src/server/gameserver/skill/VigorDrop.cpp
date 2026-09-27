////////////////////////////////////////////////////////////////////////////////
// Project     : DARKEDEN
// Module      : Skill - Effect
// File Name   : EffectVigorDrop.h
// Date        : 2002.3.28
// Description :
//               Vigor Drop is implemented the same way as the --storm
//               skills.
//               When used, EffectVigorDrop is attached over a 3x3 area
//               centered on the cast location. EffectVigorDrop does not
//               deal damage on its own.
//               It attaches EffectVigorDropToCreature to the Creatures in
//               that area and disappears. EffectVigorDropToCreature deals
//               a third of VigorDrop's total damage three times in a row
//               to that Creature and then disappears.
//
// History
//     DATE      WRITER         DESCRIPTION
// =========== =========== =====================================================
//
//
////////////////////////////////////////////////////////////////////////////////

#include "VigorDrop.h"

#include "EffectVigorDrop.h"
#include "GCAddEffect.h"
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
// Slayer object handler
//////////////////////////////////////////////////////////////////////////////
void VigorDrop::execute(Slayer* pSlayer, ObjectID_t TargetObjectID, SkillSlot* pSkillSlot, CEffectID_t CEffectID)

{
    __BEGIN_TRY


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


    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
// Slayer tile handler
//  Handler used when the skill is used on a tile
//////////////////////////////////////////////////////////////////////////////
void VigorDrop::execute(Slayer* pSlayer, ZoneCoord_t X, ZoneCoord_t Y, SkillSlot* pSkillSlot, CEffectID_t CEffectID)

{
    __BEGIN_TRY


    EffectVigorDrop* pEffect = NULL;
    EffectVigorDrop* pEffect2 = NULL;

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
        SkillDomainType_t DomainType = pSkillInfo->getDomainType();

        ZoneCoord_t myX = pSlayer->getX();
        ZoneCoord_t myY = pSlayer->getY();

        int RequiredMP = (int)pSkillInfo->getConsumeMP();
        bool bManaCheck = hasEnoughMana(pSlayer, RequiredMP);
        bool bTimeCheck = verifyRunTime(pSkillSlot);
        bool bRangeCheck = verifyDistance(pSlayer, X, Y, pSkillInfo->getRange());
        bool bHitRoll = HitRoll::isSuccessMagic(pSlayer, pSkillInfo, pSkillSlot);


        bool bTileCheck = false;

        VSRect rect(0, 0, pZone->getWidth() - 1, pZone->getHeight() - 1);

        if (rect.ptInRect(X, Y)) {
            Tile& tile = pZone->getTile(X, Y);
            if (tile.canAddEffect())
                bTileCheck = true;
        }


        if (bManaCheck && bTimeCheck && bRangeCheck && bHitRoll && bTileCheck) {
            decreaseMana(pSlayer, RequiredMP, _GCSkillToTileOK1);

            // calculate damage and duration time
            SkillInput input(pSlayer, pSkillSlot);
            SkillOutput output;
            computeOutput(input, output);

            // Soul Smashing adds its rank bonus to the damage.
            if (pSlayer->hasRankBonus(RankBonus::RANK_BONUS_SOUL_SMASHING)) {
                RankBonus* pRankBonus = pSlayer->getRankBonus(RankBonus::RANK_BONUS_SOUL_SMASHING);
                Assert(pRankBonus != NULL);

                output.Damage += pRankBonus->getPoint();
            }


            Range_t Range = 3;

            // If the same effect is already on the tile, delete it and set a new one.
            Tile& tile = pZone->getTile(X, Y);
            Effect* pOldEffect = tile.getEffect(Effect::EFFECT_CLASS_ENERGY_DROP);
            if (pOldEffect != NULL) {
                ObjectID_t effectID = pOldEffect->getObjectID();
                pZone->deleteEffect(effectID);
            }

            // Create the effect object and attach it to the tile.
            pEffect = new EffectVigorDrop(pZone, X, Y);

            pEffect->setUserObjectID(pSlayer->getObjectID());
            pEffect->setDeadline(output.Duration);
            pEffect->setNextTime(0);
            pEffect->setTick(output.Tick);
            pEffect->setDamage(output.Damage);
            pEffect->setLevel(pSkillInfo->getLevel() / 2);


            //
            //
            // Create the effect object and attach it to the tile.
            pEffect2 = new EffectVigorDrop(pZone, X, Y);
            pEffect2->setUserObjectID(pSlayer->getObjectID());
            pEffect2->setDeadline(output.Duration);
            pEffect2->setNextTime(0);
            pEffect2->setTick(output.Tick);
            pEffect2->setDamage(output.Damage * 30 / 100);
            pEffect2->setLevel(pSkillInfo->getLevel() / 2);

            // Attach the effect to every creature within the effect's range.
            // When a Slayer uses the skill, other Slayers are not affected.
            // not affected.
            bool bHit = false;

            Creature* pTargetCreature;

            Level_t maxEnemyLevel = 0;
            uint EnemyNum = 0;

            list<Creature*> cList;
            cList.push_back(pSlayer);

            int oX, oY;

            for (oX = -2; oX <= 2; oX++)
                for (oY = -2; oY <= 2; oY++) {
                    int tileX = X + oX;
                    int tileY = Y + oY;
                    if (!rect.ptInRect(tileX, tileY))
                        continue;

                    Tile& tile = pZone->getTile(tileX, tileY);
                    if (!tile.canAddEffect())
                        continue;

                    pTargetCreature = NULL;
                    if (tile.hasCreature(Creature::MOVE_MODE_WALKING))
                        pTargetCreature = tile.getCreature(Creature::MOVE_MODE_WALKING);

                    EffectVigorDrop* pTempEffect = NULL;

                    if (oX == 2 || oX == -2 || oY == 2 || oY == -2)
                        pTempEffect = pEffect2;
                    else
                        pTempEffect = pEffect;

                    if (pTargetCreature != NULL && canAttack(pSlayer, pTargetCreature)) {
                        if (pTargetCreature->isVampire() || pTargetCreature->isOusters()) {
                            if (pTempEffect->affectCreature(pTargetCreature, false) == true) {
                                Player* pTargetPlayer = pTargetCreature->getPlayer();

                                bHit = true;

                                bool bCanSee = canSee(pTargetCreature, pSlayer);

                                _GCSkillToTileOK1.addCListElement(pTargetCreature->getObjectID());
                                _GCSkillToTileOK4.addCListElement(pTargetCreature->getObjectID());
                                _GCSkillToTileOK5.addCListElement(pTargetCreature->getObjectID());

                                cList.push_back(pTargetCreature);

                                if (bCanSee) {
                                    // For the creature that was hit
                                    _GCSkillToTileOK2.setObjectID(pSlayer->getObjectID());
                                    _GCSkillToTileOK2.setSkillType(SkillType);
                                    _GCSkillToTileOK2.setX(X);
                                    _GCSkillToTileOK2.setY(Y);
                                    _GCSkillToTileOK2.setDuration(output.Duration);
                                    _GCSkillToTileOK2.setRange(Range);
                                    pTargetPlayer->sendPacket(&_GCSkillToTileOK2);
                                }
                            } else {
                            }
                        } else if (pTargetCreature->isMonster()) {
                            if (pTempEffect->affectCreature(pTargetCreature, false) == true) {
                                bHit = true;
                                Monster* pMonster = dynamic_cast<Monster*>(pTargetCreature);
                                pMonster->addEnemy(pSlayer);

                                // Record the Slayer as the last one to hit.
                                pMonster->setLastHitCreatureClass(Creature::CREATURE_CLASS_SLAYER);
                            } else {
                            }

                        } else
                            continue;

                        if (maxEnemyLevel < pTargetCreature->getLevel())
                            maxEnemyLevel = pTargetCreature->getLevel();
                        EnemyNum++;
                    } // if(pTargetCreature!= NULL)
                }

            if (bHit) {
                shareAttrExp(pSlayer, output.Damage, 1, 1, 8, _GCSkillToTileOK1);
                increaseDomainExp(pSlayer, DomainType, pSkillInfo->getPoint(), _GCSkillToTileOK1, maxEnemyLevel,
                                  EnemyNum);
                increaseSkillExp(pSlayer, DomainType, pSkillSlot, pSkillInfo, _GCSkillToTileOK1);
            }

            // For the skill user
            _GCSkillToTileOK1.setSkillType(SkillType);
            _GCSkillToTileOK1.setCEffectID(CEffectID);
            _GCSkillToTileOK1.setX(X);
            _GCSkillToTileOK1.setY(Y);
            _GCSkillToTileOK1.setDuration(output.Duration);
            _GCSkillToTileOK1.setRange(Range);

            // For those who can see only the skill user
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

        SAFE_DELETE(pEffect);
        SAFE_DELETE(pEffect2);
    } catch (Throwable& t) {
        SAFE_DELETE(pEffect);
        SAFE_DELETE(pEffect2);
        executeSkillFailException(pSlayer, getSkillType());
    }


    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
// Monster tile handler
//////////////////////////////////////////////////////////////////////////////
void VigorDrop::execute(Monster* pMonster, ZoneCoord_t X, ZoneCoord_t Y)

{
    __BEGIN_TRY

    EffectVigorDrop* pEffect = NULL;
    EffectVigorDrop* pEffect2 = NULL;

    try {
        Zone* pZone = pMonster->getZone();

        Assert(pZone != NULL);

        GCSkillToTileOK1 _GCSkillToTileOK1;
        GCSkillToTileOK2 _GCSkillToTileOK2;
        GCSkillToTileOK3 _GCSkillToTileOK3;
        GCSkillToTileOK4 _GCSkillToTileOK4;
        GCSkillToTileOK5 _GCSkillToTileOK5;
        GCSkillToTileOK6 _GCSkillToTileOK6;

        SkillType_t SkillType = SKILL_ENERGY_DROP;
        SkillInfo* pSkillInfo = de::gameContext().skillInfos().getSkillInfo(SkillType);

        ZoneCoord_t myX = pMonster->getX();
        ZoneCoord_t myY = pMonster->getY();

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
            // calculate damage and duration time
            SkillInput input(pMonster);
            SkillOutput output;
            computeOutput(input, output);

            Range_t Range = 3;

            // If the same effect is already on the tile, delete it and set a new one.
            Tile& tile = pZone->getTile(X, Y);
            Effect* pOldEffect = tile.getEffect(Effect::EFFECT_CLASS_ENERGY_DROP);
            if (pOldEffect != NULL) {
                ObjectID_t effectID = pOldEffect->getObjectID();
                pZone->deleteEffect(effectID);
            }

            // Create the effect object and attach it to the tile.
            pEffect = new EffectVigorDrop(pZone, X, Y);
            pEffect->setDeadline(output.Duration);
            pEffect->setNextTime(0);
            pEffect->setTick(output.Tick);
            pEffect->setDamage(output.Damage);
            pEffect->setLevel(pSkillInfo->getLevel() / 2);


            // Create the effect object and attach it to the tile.
            pEffect2 = new EffectVigorDrop(pZone, X, Y);
            pEffect2->setDeadline(output.Duration);
            pEffect2->setNextTime(0);
            pEffect2->setTick(output.Tick);
            pEffect2->setDamage(output.Damage * 30 / 100);
            pEffect2->setLevel(pSkillInfo->getLevel() / 2);


            // Attach the effect to every creature within the effect's range.
            // When a Slayer uses the skill, other Slayers are not affected.
            // not affected.
            Creature* pTargetCreature;


            list<Creature*> cList;
            cList.push_back(pMonster);

            int oX, oY;

            for (oX = -2; oX <= 2; oX++)
                for (oY = -2; oY <= 2; oY++) {
                    int tileX = X + oX;
                    int tileY = Y + oY;

                    EffectVigorDrop* pTempEffect = NULL;

                    if (oX == 2 || oX == -2 || oY == 2 || oY == -2)
                        pTempEffect = pEffect2;
                    else
                        pTempEffect = pEffect;

                    if (!rect.ptInRect(tileX, tileY))
                        continue;

                    Tile& tile = pZone->getTile(tileX, tileY);

                    if (!tile.canAddEffect())
                        continue;

                    pTargetCreature = NULL;
                    if (tile.hasCreature(Creature::MOVE_MODE_WALKING))
                        pTargetCreature = tile.getCreature(Creature::MOVE_MODE_WALKING);

                    if (pTargetCreature != NULL) {
                        if (pTargetCreature->isPC()) {
                            if (pTempEffect->affectCreature(pTargetCreature, false) == true) {
                                Player* pTargetPlayer = pTargetCreature->getPlayer();

                                bool bCanSee = canSee(pTargetCreature, pMonster);

                                _GCSkillToTileOK1.addCListElement(pTargetCreature->getObjectID());
                                _GCSkillToTileOK4.addCListElement(pTargetCreature->getObjectID());
                                _GCSkillToTileOK5.addCListElement(pTargetCreature->getObjectID());

                                cList.push_back(pTargetCreature);

                                if (bCanSee) {
                                    // For the creature that was hit
                                    _GCSkillToTileOK2.setObjectID(pMonster->getObjectID());
                                    _GCSkillToTileOK2.setSkillType(SkillType);
                                    _GCSkillToTileOK2.setX(X);
                                    _GCSkillToTileOK2.setY(Y);
                                    _GCSkillToTileOK2.setDuration(output.Duration);
                                    _GCSkillToTileOK2.setRange(Range);
                                    pTargetPlayer->sendPacket(&_GCSkillToTileOK2);
                                }
                            }
                        }
                    } // if(pTargetCreature!= NULL)
                }

            // For those who can see only the skill user
            _GCSkillToTileOK3.setSkillType(SkillType);
            _GCSkillToTileOK3.setX(myX);
            _GCSkillToTileOK3.setY(myY);

            // For those who can see only the target
            _GCSkillToTileOK4.setSkillType(SkillType);
            _GCSkillToTileOK4.setX(X);
            _GCSkillToTileOK4.setY(Y);
            _GCSkillToTileOK4.setDuration(output.Duration);
            _GCSkillToTileOK4.setRange(Range);

            // For those who can see both the skill user and the target
            _GCSkillToTileOK5.setObjectID(pMonster->getObjectID());
            _GCSkillToTileOK5.setSkillType(SkillType);
            _GCSkillToTileOK5.setX(X);
            _GCSkillToTileOK5.setY(Y);
            _GCSkillToTileOK5.setDuration(output.Duration);
            _GCSkillToTileOK5.setRange(Range);

            // Broadcast to those who can see both the skill user and the target.
            cList = pZone->broadcastSkillPacket(myX, myY, X, Y, &_GCSkillToTileOK5, cList);

            // Broadcast to those who can see the skill user.
            pZone->broadcastPacket(myX, myY, &_GCSkillToTileOK3, cList);

            // Broadcast to those who can see the target.
            pZone->broadcastPacket(X, Y, &_GCSkillToTileOK4, cList);


        } else {
            executeSkillFailNormal(pMonster, getSkillType(), NULL);
        }
        SAFE_DELETE(pEffect);
        SAFE_DELETE(pEffect2);
    } catch (Throwable& t) {
        SAFE_DELETE(pEffect);
        SAFE_DELETE(pEffect2);
        executeSkillFailException(pMonster, getSkillType());
    }


    __END_CATCH
}
VigorDrop g_VigorDrop;
