//////////////////////////////////////////////////////////////////////////////
// Filename    : SimpleLinearMeleeSkill.cpp
// Written by  : excel96
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "SimpleLinearMeleeSkill.h"

#include "GCSkillToTileOK1.h"
#include "GCSkillToTileOK2.h"
#include "GCSkillToTileOK3.h"
#include "GCSkillToTileOK4.h"
#include "GCSkillToTileOK5.h"
#include "GameContext.h"
#include "Geometry.h"

SimpleLinearMeleeSkill g_SimpleLinearMeleeSkill;

//////////////////////////////////////////////////////////////////////////////
// class SimpleLinearMeleeSkill member methods
//////////////////////////////////////////////////////////////////////////////

void SimpleLinearMeleeSkill::execute(Slayer* pSlayer, int X, int Y, SkillSlot* pSkillSlot,
                                     const SIMPLE_SKILL_INPUT& param, SIMPLE_SKILL_OUTPUT& result,
                                     CEffectID_t CEffectID /* =0 */) {
    Assert(pSlayer != NULL);
    Assert(pSkillSlot != NULL);

    try {
        Player* pPlayer = pSlayer->getPlayer();
        Zone* pZone = pSlayer->getZone();

        Assert(pPlayer != NULL);
        Assert(pZone != NULL);

        // If this skill requires a specific weapon to cast,
        // check the equipped weapon class and fail if it does not match.
        bool bIncreaseExp = true;
        if (param.ItemClass != Item::ITEM_CLASS_MAX) {
            Item* pItem = pSlayer->getWearItem(Slayer::WEAR_RIGHTHAND);
            if (pItem == NULL || pItem->getItemClass() != param.ItemClass) {
                executeSkillFailException(pSlayer, param.SkillType);
                return;
            }

            bIncreaseExp = pSlayer->isRealWearingEx(Slayer::WEAR_RIGHTHAND);
        }

        GCSkillToTileOK1 _GCSkillToTileOK1;
        GCSkillToTileOK2 _GCSkillToTileOK2;
        GCSkillToTileOK3 _GCSkillToTileOK3;
        GCSkillToTileOK4 _GCSkillToTileOK4;
        GCSkillToTileOK5 _GCSkillToTileOK5;

        SkillInfo* pSkillInfo = de::gameContext().skillInfos().getSkillInfo(param.SkillType);
        SkillDomainType_t DomainType = pSkillInfo->getDomainType();
        Level_t SkillLevel = pSkillSlot->getExpLevel();

        int RequiredMP = (int)pSkillInfo->getConsumeMP();
        bool bManaCheck = hasEnoughMana(pSlayer, RequiredMP);
        bool bTimeCheck = verifyRunTime(pSkillSlot);
        bool bRangeCheck = verifyDistance(pSlayer, X, Y, pSkillInfo->getRange());

        if (bManaCheck && bTimeCheck && bRangeCheck) {
            // Consumes MP.
            decreaseMana(pSlayer, RequiredMP, _GCSkillToTileOK1);

            // Work out the coordinates and the direction.
            ZoneCoord_t myX = pSlayer->getX();
            ZoneCoord_t myY = pSlayer->getY();
            Dir_t dir = calcDirection(myX, myY, X, Y);
            Damage_t Damage = 0;
            Damage_t MaxDamage = 0;
            bool bCriticalHit = false;

            list<POINT> ptList;
            getPointsFromLineEx(myX, myY, X, Y, pSkillInfo->getRange(), ptList);

            VSRect rect(1, 1, pZone->getWidth() - 2, pZone->getHeight() - 2);

            list<Creature*> cList;

            Level_t maxEnemyLevel = 0;
            uint EnemyNum = 0;

            bool bHit = false;

            list<POINT>::iterator ptitr = ptList.begin();
            for (; ptitr != ptList.end(); ptitr++) {
                int tileX = (*ptitr).x;
                int tileY = (*ptitr).y;

                // Inside the zone and outside a safe area there is a chance of being hit.
                if (rect.ptInRect(tileX, tileY)) {
                    // Get the tile.
                    Tile& tile = pZone->getTile(tileX, tileY);

                    list<Creature*> targetList;
                    if (tile.hasCreature(Creature::MOVE_MODE_WALKING)) {
                        Creature* pCreature = tile.getCreature(Creature::MOVE_MODE_WALKING);
                        targetList.push_back(pCreature);
                    }
                    if (tile.hasCreature(Creature::MOVE_MODE_FLYING)) {
                        Creature* pCreature = tile.getCreature(Creature::MOVE_MODE_FLYING);
                        targetList.push_back(pCreature);
                    }
                    if (tile.hasCreature(Creature::MOVE_MODE_BURROWING)) {
                        Creature* pCreature = tile.getCreature(Creature::MOVE_MODE_BURROWING);
                        targetList.push_back(pCreature);
                    }

                    list<Creature*>::iterator itr = targetList.begin();
                    for (; itr != targetList.end(); itr++) {
                        Creature* pTargetCreature = (*itr);
                        Assert(pTargetCreature != NULL);

                        bool bMoveModeCheck =
                            (pTargetCreature->getMoveMode() == Creature::MOVE_MODE_WALKING) ? true : false;

                        bool bCanHit = canHit(pSlayer, pTargetCreature, param.SkillType);
                        if ((pTargetCreature->getMoveMode() == Creature::MOVE_MODE_FLYING) && param.Level == 1) {
                            bMoveModeCheck = true;
                            bCanHit = true;
                        }
                        if ((pTargetCreature->getMoveMode() == Creature::MOVE_MODE_BURROWING) && param.Level == 2) {
                            bMoveModeCheck = true;
                            bCanHit = true;
                        }

                        bool bRaceCheck = pTargetCreature->isOusters() || pTargetCreature->isVampire() ||
                                          pTargetCreature->isMonster();
                        bool bHitRoll = false;
                        bool bPK = verifyPK(pSlayer, pTargetCreature);
                        bool bZoneLevelCheck = checkZoneLevelToHitTarget(pTargetCreature);
                        bool bSetMinDamage = false;

                        if (param.SkillType == SKILL_SPIT_STREAM && tileX == X && tileY == Y) {
                            bRaceCheck = !pTargetCreature->isNPC();
                        }

                        if (param.bMagicHitRoll) {
                            bHitRoll = HitRoll::isSuccessMagic(pSlayer, pSkillInfo, pSkillSlot);
                        } else {
                            bHitRoll = HitRoll::isSuccess(pSlayer, pTargetCreature, SkillLevel / 2);
                        }

                        // Blade skills deal 7% damage even when they miss.
                        if (param.ItemClass == Item::ITEM_CLASS_BLADE && !bHitRoll) {
                            bHitRoll = true;
                            bSetMinDamage = true;
                        }

                        if (!canAttack(pSlayer, pTargetCreature) ||
                            pTargetCreature->isFlag(Effect::EFFECT_CLASS_COMA)) {
                            bHitRoll = false;
                        }

                        if (bMoveModeCheck && bRaceCheck && bHitRoll && bCanHit && bPK && bZoneLevelCheck) {
                            bCriticalHit = false;
                            Damage = 0;

                            if (param.bAdd) {
                                Damage += computeDamage(pSlayer, pTargetCreature, SkillLevel / 5, bCriticalHit);
                            }

                            if (param.bMagicDamage) {
                                Damage += computeMagicDamage(pTargetCreature, param.SkillDamage, param.SkillType);
                            } else {
                                Damage += param.SkillDamage;
                            }

                            // A blade skill that failed its hit roll deals 7% damage.
                            if (bSetMinDamage) {
                                Damage = getPercentValue(Damage, 7);
                            }

                            MaxDamage = max(Damage, MaxDamage);

                            ObjectID_t targetObjectID = pTargetCreature->getObjectID();
                            cList.push_back(pTargetCreature);

                            _GCSkillToTileOK1.addCListElement(targetObjectID);
                            _GCSkillToTileOK2.addCListElement(targetObjectID);
                            _GCSkillToTileOK5.addCListElement(targetObjectID);

                            // Apply the damage with the target's own packet left NULL.
                            setDamage(pTargetCreature, Damage, pSlayer, param.SkillType, NULL, &_GCSkillToTileOK1);
                            computeAlignmentChange(pTargetCreature, Damage, pSlayer, NULL, &_GCSkillToTileOK1);

                            increaseAlignment(pSlayer, pTargetCreature, _GCSkillToTileOK1);

                            // On a critical hit, knocks the target back.
                            if (bCriticalHit) {
                                knockbackCreature(pZone, pTargetCreature, pSlayer->getX(), pSlayer->getY());
                            }

                            // Counts as a hit only when the target is not a Slayer.
                            if (!pTargetCreature->isSlayer()) {
                                bHit = true;
                                if (maxEnemyLevel < pTargetCreature->getLevel())
                                    maxEnemyLevel = pTargetCreature->getLevel();
                                EnemyNum++;
                            }
                        }
                    }
                }
            }

            if (bHit) {
                if (bIncreaseExp) {
                    shareAttrExp(pSlayer, MaxDamage, param.STRMultiplier, param.DEXMultiplier, param.INTMultiplier,
                                 _GCSkillToTileOK1);
                    increaseDomainExp(pSlayer, DomainType, pSkillInfo->getPoint(), _GCSkillToTileOK1, maxEnemyLevel,
                                      EnemyNum);
                    increaseSkillExp(pSlayer, DomainType, pSkillSlot, pSkillInfo, _GCSkillToTileOK1);
                }
            }

            decreaseDurability(pSlayer, NULL, pSkillInfo, &_GCSkillToTileOK1, NULL);

            _GCSkillToTileOK1.setSkillType(param.SkillType);
            _GCSkillToTileOK1.setCEffectID(CEffectID);
            _GCSkillToTileOK1.setX(X);
            _GCSkillToTileOK1.setY(Y);
            _GCSkillToTileOK1.setRange(dir);
            _GCSkillToTileOK1.setDuration(0);

            _GCSkillToTileOK2.setObjectID(pSlayer->getObjectID());
            _GCSkillToTileOK2.setSkillType(param.SkillType);
            _GCSkillToTileOK2.setX(X);
            _GCSkillToTileOK2.setY(Y);
            _GCSkillToTileOK2.setRange(dir);
            _GCSkillToTileOK2.setDuration(0);

            _GCSkillToTileOK3.setObjectID(pSlayer->getObjectID());
            _GCSkillToTileOK3.setSkillType(param.SkillType);
            _GCSkillToTileOK3.setX(X);
            _GCSkillToTileOK3.setY(Y);

            _GCSkillToTileOK4.setSkillType(param.SkillType);
            _GCSkillToTileOK4.setX(X);
            _GCSkillToTileOK4.setY(Y);
            _GCSkillToTileOK4.setRange(dir);
            _GCSkillToTileOK4.setDuration(0);

            _GCSkillToTileOK5.setObjectID(pSlayer->getObjectID());
            _GCSkillToTileOK5.setSkillType(param.SkillType);
            _GCSkillToTileOK5.setX(X);
            _GCSkillToTileOK5.setY(Y);
            _GCSkillToTileOK5.setRange(dir);
            _GCSkillToTileOK5.setDuration(0);

            pPlayer->sendPacket(&_GCSkillToTileOK1);

            // Send the packet to everyone affected by this skill.
            for (list<Creature*>::const_iterator itr = cList.begin(); itr != cList.end(); itr++) {
                Creature* pTargetCreature = *itr;
                Assert(pTargetCreature != NULL);

                if (pTargetCreature->isPC()) {
                    _GCSkillToTileOK2.clearList();

                    // Record the HP change in the packet.
                    HP_t targetHP = 0;
                    if (pTargetCreature->isSlayer()) {
                        targetHP = (dynamic_cast<Slayer*>(pTargetCreature))->getHP(ATTR_CURRENT);
                    } else if (pTargetCreature->isVampire()) {
                        targetHP = (dynamic_cast<Vampire*>(pTargetCreature))->getHP(ATTR_CURRENT);
                    } else if (pTargetCreature->isOusters()) {
                        targetHP = (dynamic_cast<Ousters*>(pTargetCreature))->getHP(ATTR_CURRENT);
                    }
                    _GCSkillToTileOK2.addShortData(MODIFY_CURRENT_HP, targetHP);

                    // Wear down the target's item durability.
                    decreaseDurability(NULL, pTargetCreature, pSkillInfo, NULL, &_GCSkillToTileOK2);

                    // Send the packet.
                    pTargetCreature->getPlayer()->sendPacket(&_GCSkillToTileOK2);
                } else if (pTargetCreature->isMonster()) {
                    // The monster takes the caster as an enemy.
                    Monster* pMonster = dynamic_cast<Monster*>(pTargetCreature);
                    pMonster->addEnemy(pSlayer);
                }
            }

            cList.push_back(pSlayer);

            cList = pZone->broadcastSkillPacket(myX, myY, X, Y, &_GCSkillToTileOK5, cList);

            pZone->broadcastPacket(myX, myY, &_GCSkillToTileOK3, cList);
            pZone->broadcastPacket(X, Y, &_GCSkillToTileOK4, cList);

            // set Next Run Time
            pSkillSlot->setRunTime(param.Delay);

            result.bSuccess = true;
        } else {
            executeSkillFailNormal(pSlayer, param.SkillType, NULL);
        }
    } catch (Throwable& t) {
        executeSkillFailException(pSlayer, param.SkillType);
    }
}
