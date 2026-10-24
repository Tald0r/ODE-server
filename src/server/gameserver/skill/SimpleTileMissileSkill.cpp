//////////////////////////////////////////////////////////////////////////////
// Filename    : SimpleTileMissileSkill.cpp
// Written by  : excel96
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "SimpleTileMissileSkill.h"

#include "GCSkillToTileOK1.h"
#include "GCSkillToTileOK2.h"
#include "GCSkillToTileOK3.h"
#include "GCSkillToTileOK4.h"
#include "GCSkillToTileOK5.h"
#include "GCSkillToTileOK6.h"
#include "GameContext.h"
#include "ZoneUtil.h"

SimpleTileMissileSkill g_SimpleTileMissileSkill;

//////////////////////////////////////////////////////////////////////////////
// class SimpleTileMissileSkill member methods
//////////////////////////////////////////////////////////////////////////////

void SimpleTileMissileSkill::execute(Slayer* pSlayer, int X, int Y, SkillSlot* pSkillSlot,
                                     const SIMPLE_SKILL_INPUT& param, SIMPLE_SKILL_OUTPUT& result,
                                     CEffectID_t CEffectID, bool bForceKnockback) {
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
        GCSkillToTileOK6 _GCSkillToTileOK6;

        SkillInfo* pSkillInfo = de::gameContext().skillInfos().getSkillInfo(param.SkillType);
        SkillDomainType_t DomainType = pSkillInfo->getDomainType();
        SkillLevel_t SkillLevel = pSkillSlot->getExpLevel();

        int RequiredMP = (int)pSkillInfo->getConsumeMP();
        bool bManaCheck = hasEnoughMana(pSlayer, RequiredMP);
        bool bTimeCheck = param.Delay == 0xffffffff || verifyRunTime(pSkillSlot);
        bool bRangeCheck = verifyDistance(pSlayer, X, Y, pSkillInfo->getRange());

        if (bManaCheck && bTimeCheck && bRangeCheck) {
            // Consume the mana.
            decreaseMana(pSlayer, RequiredMP, _GCSkillToTileOK1);

            // Work out the coordinates and the direction.
            ZoneCoord_t myX = pSlayer->getX();
            ZoneCoord_t myY = pSlayer->getY();
            Dir_t dir = calcDirection(myX, myY, X, Y);
            Damage_t Damage = 0;
            Damage_t MaxDamage = 0;
            Damage_t TotalDamage = 0;
            bool bCriticalHit = false;
            bool bHit = false;

            Level_t maxEnemyLevel = 0;
            uint EnemyNum = 0;

            VSRect rect(1, 1, pZone->getWidth() - 2, pZone->getHeight() - 2);

            list<Creature*> cList;

            list<TILE_MASK>::const_iterator itr = param.MaskList.begin();
            for (; itr != param.MaskList.end(); itr++) {
                TILE_MASK mask = (*itr);

                int tileX = X + mask.x;
                int tileY = Y + mask.y;
                int penalty = mask.penalty;

                // A tile inside the zone that is not a safe zone can be hit.
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

                        if (pTargetCreature != pSlayer) {
                            bool bPK = verifyPK(pSlayer, pTargetCreature);
                            bool bRaceCheck = pTargetCreature->isOusters() || pTargetCreature->isVampire() ||
                                              pTargetCreature->isMonster();
                            bool bZoneLevelCheck = checkZoneLevelToHitTarget(pTargetCreature);
                            bool bHitRoll = false;

                            if (param.bMagicHitRoll) {
                                bHitRoll = HitRoll::isSuccessMagic(pSlayer, pSkillInfo, pSkillSlot);
                            } else {
                                bHitRoll = HitRoll::isSuccess(pSlayer, pTargetCreature, SkillLevel);
                            }

                            if (param.SkillType == SKILL_INFINITY_THUNDERBOLT && mask.x == 0 && mask.y == 0) {
                                bRaceCheck = !pTargetCreature->isNPC();
                            }


                            if (!canAttack(pSlayer, pTargetCreature) ||
                                pTargetCreature->isFlag(Effect::EFFECT_CLASS_COMA)) {
                                bHitRoll = false;
                            }

                            if (bPK && bRaceCheck && bZoneLevelCheck && bHitRoll) {
                                Damage = 0;
                                bCriticalHit = false;

                                if (param.bAdd) {
                                    Damage += computeDamage(pSlayer, pTargetCreature, SkillLevel / 2, bCriticalHit);
                                }

                                if (param.bMagicDamage) {
                                    Damage += computeMagicDamage(pTargetCreature, param.SkillDamage, param.SkillType);
                                } else {
                                    Damage += param.SkillDamage;
                                }

                                MaxDamage = max(Damage, MaxDamage);
                                TotalDamage += Damage;

                                // The penalty is 100 by default.
                                Damage = getPercentValue(Damage, penalty);

                                ObjectID_t targetObjectID = pTargetCreature->getObjectID();
                                cList.push_back(pTargetCreature);

                                _GCSkillToTileOK1.addCListElement(targetObjectID);
                                _GCSkillToTileOK2.addCListElement(targetObjectID);
                                _GCSkillToTileOK5.addCListElement(targetObjectID);

                                // Apply the damage, leaving the target's packet null for now.
                                setDamage(pTargetCreature, Damage, pSlayer, param.SkillType, NULL, &_GCSkillToTileOK1);
                                computeAlignmentChange(pTargetCreature, Damage, pSlayer, NULL, &_GCSkillToTileOK1);

                                increaseAlignment(pSlayer, pTargetCreature, _GCSkillToTileOK1);

                                // On a critical hit, push the target back.
                                if (bCriticalHit || bForceKnockback) {
                                    knockbackCreature(pZone, pTargetCreature, pSlayer->getX(), pSlayer->getY());
                                }

                                // Count it as a hit only when the target is not a Slayer.
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
            }

            if (bHit) {
                if (bIncreaseExp) {
                    if (param.bExpForTotalDamage) {
                        TotalDamage = (Damage_t)(MaxDamage * 1.5f);
                        shareAttrExp(pSlayer, TotalDamage, param.STRMultiplier, param.DEXMultiplier,
                                     param.INTMultiplier, _GCSkillToTileOK1);
                    } else {
                        shareAttrExp(pSlayer, MaxDamage, param.STRMultiplier, param.DEXMultiplier, param.INTMultiplier,
                                     _GCSkillToTileOK1);
                    }

                    increaseDomainExp(pSlayer, DomainType, pSkillInfo->getPoint(), _GCSkillToTileOK1, maxEnemyLevel,
                                      EnemyNum);
                    increaseSkillExp(pSlayer, DomainType, pSkillSlot, pSkillInfo, _GCSkillToTileOK1);
                }
            }

            // Wear down the attacker's item durability.
            if (pSkillInfo->getDomainType() != SKILL_DOMAIN_GUN) {
                decreaseDurability(pSlayer, NULL, pSkillInfo, &_GCSkillToTileOK1, NULL);
            } else {
                decreaseDurability(pSlayer, NULL, NULL, &_GCSkillToTileOK1, NULL);
            }

            _GCSkillToTileOK1.setSkillType(param.SkillType);
            _GCSkillToTileOK1.setCEffectID(CEffectID);
            _GCSkillToTileOK1.setX(X);
            _GCSkillToTileOK1.setY(Y);
            _GCSkillToTileOK1.setRange(dir);
            _GCSkillToTileOK1.setDuration(0);
            _GCSkillToTileOK1.setGrade(param.Level);

            _GCSkillToTileOK2.setObjectID(pSlayer->getObjectID());
            _GCSkillToTileOK2.setSkillType(param.SkillType);
            _GCSkillToTileOK2.setX(X);
            _GCSkillToTileOK2.setY(Y);
            _GCSkillToTileOK2.setRange(dir);
            _GCSkillToTileOK2.setDuration(0);
            _GCSkillToTileOK2.setGrade(param.Level);

            _GCSkillToTileOK5.setObjectID(pSlayer->getObjectID());
            _GCSkillToTileOK5.setSkillType(param.SkillType);
            _GCSkillToTileOK5.setX(X);
            _GCSkillToTileOK5.setY(Y);
            _GCSkillToTileOK5.setRange(dir);
            _GCSkillToTileOK5.setDuration(0);
            _GCSkillToTileOK5.setGrade(param.Level);

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
            result.targetCreatures = cList;

            cList.push_back(pSlayer);

            pZone->broadcastPacket(myX, myY, &_GCSkillToTileOK5, cList);

            if (param.Delay != 0xffffffff) {
                // set Next Run Time
                pSkillSlot->setRunTime(param.Delay);
            }

            result.bSuccess = true;
        } else {
            executeSkillFailNormal(pSlayer, param.SkillType, NULL);
        }
    } catch (Throwable& t) {
        executeSkillFailException(pSlayer, param.SkillType);
    }
}

void SimpleTileMissileSkill::execute(Vampire* pVampire, int X, int Y, VampireSkillSlot* pVampireSkillSlot,
                                     const SIMPLE_SKILL_INPUT& param, SIMPLE_SKILL_OUTPUT& result,
                                     CEffectID_t CEffectID, bool bForceKnockback, int HitBonus) {
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

        SkillInfo* pSkillInfo = de::gameContext().skillInfos().getSkillInfo(param.SkillType);

        int RequiredMP = decreaseConsumeMP(pVampire, pSkillInfo);
        bool bManaCheck = hasEnoughMana(pVampire, RequiredMP);
        bool bTimeCheck = param.Delay == 0xffffffff || verifyRunTime(pVampireSkillSlot);
        bool bRangeCheck = verifyDistance(pVampire, X, Y, pSkillInfo->getRange());

        if (bRangeCheck && param.SkillType == SKILL_NOOSE_OF_WRAITH) {
            int ratio = 20 + pVampire->getINT() / 6;
            ratio = min(95, ratio);

            bRangeCheck = (rand() % 100) < ratio;
        }

        if (bManaCheck && bTimeCheck && bRangeCheck) {
            // Consume the mana.
            decreaseMana(pVampire, RequiredMP, _GCSkillToTileOK1);

            // Work out the coordinates and the direction.
            ZoneCoord_t myX = pVampire->getX();
            ZoneCoord_t myY = pVampire->getY();
            Dir_t dir = calcDirection(myX, myY, X, Y);
            Damage_t Damage = 0;
            bool bCriticalHit = false;

            VSRect rect(1, 1, pZone->getWidth() - 2, pZone->getHeight() - 2);

            list<Creature*> cList;

            list<TILE_MASK>::const_iterator itr = param.MaskList.begin();
            for (; itr != param.MaskList.end(); itr++) {
                TILE_MASK mask = (*itr);

                int tileX = X + mask.x;
                int tileY = Y + mask.y;
                int penalty = mask.penalty;

                // A tile inside the zone that is not a safe zone can be hit.
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

                        if (pTargetCreature != pVampire) {
                            bool bPK = verifyPK(pVampire, pTargetCreature);
                            bool bRaceCheck = pTargetCreature->isOusters() || pTargetCreature->isSlayer() ||
                                              pTargetCreature->isMonster();
                            bool bZoneLevelCheck = checkZoneLevelToHitTarget(pTargetCreature);
                            bool bHitRoll = false;

                            if (param.bMagicHitRoll) {
                                bHitRoll = HitRoll::isSuccessMagic(pVampire, pSkillInfo, pVampireSkillSlot, HitBonus);
                            } else {
                                bHitRoll = HitRoll::isSuccess(pVampire, pTargetCreature);
                            }

                            if (param.SkillType == SKILL_NOOSE_OF_WRAITH && mask.x == 0 && mask.y == 0) {
                                bRaceCheck = !pTargetCreature->isNPC();
                            }


                            if (!canAttack(pVampire, pTargetCreature) ||
                                pTargetCreature->isFlag(Effect::EFFECT_CLASS_COMA)) {
                                bHitRoll = false;
                            }

                            if (bPK && bRaceCheck && bZoneLevelCheck && bHitRoll) {
                                Damage = 0;
                                bCriticalHit = false;

                                if (param.bAdd) {
                                    Damage += computeDamage(pVampire, pTargetCreature, 0, bCriticalHit);
                                }

                                if (param.bMagicDamage) {
                                    Damage += computeMagicDamage(pTargetCreature, param.SkillDamage, param.SkillType,
                                                                 true, pVampire);
                                } else {
                                    Damage += param.SkillDamage;
                                }

                                // The penalty is 100 by default.
                                Damage = getPercentValue(Damage, penalty);

                                ObjectID_t targetObjectID = pTargetCreature->getObjectID();
                                cList.push_back(pTargetCreature);

                                _GCSkillToTileOK1.addCListElement(targetObjectID);
                                _GCSkillToTileOK2.addCListElement(targetObjectID);
                                _GCSkillToTileOK5.addCListElement(targetObjectID);

                                // Apply the damage, leaving the target's packet null for now.
                                setDamage(pTargetCreature, Damage, pVampire, param.SkillType, NULL, &_GCSkillToTileOK1);
                                computeAlignmentChange(pTargetCreature, Damage, pVampire, NULL, &_GCSkillToTileOK1);

                                increaseAlignment(pVampire, pTargetCreature, _GCSkillToTileOK1);

                                // On a critical hit, push the target back.
                                if (bCriticalHit || bForceKnockback) {
                                    knockbackCreature(pZone, pTargetCreature, pVampire->getX(), pVampire->getY());
                                }

                                if (pTargetCreature->isDead()) {
                                    int exp = computeCreatureExp(pTargetCreature, KILL_EXP);
                                    shareVampExp(pVampire, exp, _GCSkillToTileOK1);
                                }
                            }
                        }
                    }
                }
            }

            // Wear down the attacker's item durability.
            decreaseDurability(pVampire, NULL, pSkillInfo, &_GCSkillToTileOK1, NULL);

            _GCSkillToTileOK1.setSkillType(param.SkillType);
            _GCSkillToTileOK1.setCEffectID(CEffectID);
            _GCSkillToTileOK1.setX(X);
            _GCSkillToTileOK1.setY(Y);
            _GCSkillToTileOK1.setRange(dir);
            _GCSkillToTileOK1.setDuration(0);

            _GCSkillToTileOK2.setObjectID(pVampire->getObjectID());
            _GCSkillToTileOK2.setSkillType(param.SkillType);
            _GCSkillToTileOK2.setX(X);
            _GCSkillToTileOK2.setY(Y);
            _GCSkillToTileOK2.setRange(dir);
            _GCSkillToTileOK2.setDuration(0);

            _GCSkillToTileOK5.setObjectID(pVampire->getObjectID());
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
                    pMonster->addEnemy(pVampire);
                }
            }
            result.targetCreatures = cList;

            cList.push_back(pVampire);

            pZone->broadcastPacket(myX, myY, &_GCSkillToTileOK5, cList);

            if (param.Delay != 0xffffffff) {
                // set Next Run Time
                pVampireSkillSlot->setRunTime(param.Delay);
            }

            result.bSuccess = true;
        } else {
            executeSkillFailNormal(pVampire, param.SkillType, NULL);
        }
    } catch (Throwable& t) {
        executeSkillFailException(pVampire, param.SkillType);
    }
}

void SimpleTileMissileSkill::execute(Ousters* pOusters, int X, int Y, OustersSkillSlot* pOustersSkillSlot,
                                     const SIMPLE_SKILL_INPUT& param, SIMPLE_SKILL_OUTPUT& result,
                                     CEffectID_t CEffectID, bool bForceKnockback, int HitBonus) {
    try {
        Player* pPlayer = pOusters->getPlayer();
        Zone* pZone = pOusters->getZone();
        Assert(pPlayer != NULL);
        Assert(pZone != NULL);

        GCSkillToTileOK1 _GCSkillToTileOK1;
        GCSkillToTileOK2 _GCSkillToTileOK2;
        GCSkillToTileOK3 _GCSkillToTileOK3;
        GCSkillToTileOK4 _GCSkillToTileOK4;
        GCSkillToTileOK5 _GCSkillToTileOK5;
        GCSkillToTileOK6 _GCSkillToTileOK6;

        SkillInfo* pSkillInfo = de::gameContext().skillInfos().getSkillInfo(param.SkillType);

        int RequiredMP = (int)pSkillInfo->getConsumeMP();
        bool bManaCheck = hasEnoughMana(pOusters, RequiredMP);
        bool bTimeCheck = param.Delay == 0xffffffff || verifyRunTime(pOustersSkillSlot);
        bool bRangeCheck = verifyDistance(pOusters, X, Y, pSkillInfo->getRange());

        if (bManaCheck && bTimeCheck && bRangeCheck) {
            // Consume the mana.
            decreaseMana(pOusters, RequiredMP, _GCSkillToTileOK1);

            // Work out the coordinates and the direction.
            ZoneCoord_t myX = pOusters->getX();
            ZoneCoord_t myY = pOusters->getY();
            Dir_t dir = calcDirection(myX, myY, X, Y);
            Damage_t Damage = 0;
            bool bCriticalHit = false;

            VSRect rect(1, 1, pZone->getWidth() - 2, pZone->getHeight() - 2);

            list<Creature*> cList;

            list<TILE_MASK>::const_iterator itr = param.MaskList.begin();
            for (; itr != param.MaskList.end(); itr++) {
                TILE_MASK mask = (*itr);

                int tileX = X + mask.x;
                int tileY = Y + mask.y;
                int penalty = mask.penalty;

                bool canSteal = true;

                if ((param.SkillType == SKILL_DESTRUCTION_SPEAR || param.SkillType == SKILL_ICE_LANCE) &&
                    (mask.x != 0 || mask.y != 0)) {
                    canSteal = false;
                }

                // A tile inside the zone that is not a safe zone can be hit.
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

                        if (pTargetCreature != pOusters) {
                            bool bPK = verifyPK(pOusters, pTargetCreature);
                            bool bRaceCheck = pTargetCreature->isOusters() || pTargetCreature->isNPC();
                            bool bZoneLevelCheck = checkZoneLevelToHitTarget(pTargetCreature);

                            bool bHitRoll = false;

                            if (param.SkillType == SKILL_DESTRUCTION_SPEAR && mask.x == 0 && mask.y == 0) {
                                bRaceCheck = pTargetCreature->isNPC();
                            }

                            if (param.bMagicHitRoll) {
                                bHitRoll = HitRoll::isSuccessMagic(pOusters, pSkillInfo, pOustersSkillSlot, HitBonus);
                            } else {
                                bHitRoll = HitRoll::isSuccess(pOusters, pTargetCreature);
                            }

                            if (!canAttack(pOusters, pTargetCreature) ||
                                pTargetCreature->isFlag(Effect::EFFECT_CLASS_COMA)) {
                                bHitRoll = false;
                            }

                            if (bPK && !bRaceCheck && bZoneLevelCheck && bHitRoll) {
                                Damage = 0;
                                bCriticalHit = false;

                                if (param.bAdd) {
                                    Damage += computeDamage(pOusters, pTargetCreature, 0, bCriticalHit);
                                }

                                if (param.bMagicDamage) {
                                    Damage += computeOustersMagicDamage(pOusters, pTargetCreature, param.SkillDamage,
                                                                        param.SkillType);
                                } else {
                                    Damage += param.SkillDamage;
                                }

                                computeCriticalBonus(pOusters, param.SkillType, Damage, bCriticalHit);

                                // The penalty is 100 by default.
                                Damage = getPercentValue(Damage, penalty);

                                ObjectID_t targetObjectID = pTargetCreature->getObjectID();
                                cList.push_back(pTargetCreature);

                                _GCSkillToTileOK1.addCListElement(targetObjectID);
                                _GCSkillToTileOK2.addCListElement(targetObjectID);
                                _GCSkillToTileOK5.addCListElement(targetObjectID);

                                // Apply the damage, leaving the target's packet null for now.
                                setDamage(pTargetCreature, Damage, pOusters, param.SkillType, NULL, &_GCSkillToTileOK1,
                                          true, canSteal);
                                computeAlignmentChange(pTargetCreature, Damage, pOusters, NULL, &_GCSkillToTileOK1);

                                increaseAlignment(pOusters, pTargetCreature, _GCSkillToTileOK1);

                                // On a critical hit, push the target back.
                                if (bCriticalHit || bForceKnockback) {
                                    knockbackCreature(pZone, pTargetCreature, pOusters->getX(), pOusters->getY());
                                }

                                if (pTargetCreature->isDead()) {
                                    int exp = computeCreatureExp(pTargetCreature, 70, pOusters);
                                    shareOustersExp(pOusters, exp, _GCSkillToTileOK1);
                                }
                            }
                        }
                    }
                }
            }

            // Wear down the attacker's item durability.
            decreaseDurability(pOusters, NULL, pSkillInfo, &_GCSkillToTileOK1, NULL);

            _GCSkillToTileOK1.setSkillType(param.SkillType);
            _GCSkillToTileOK1.setCEffectID(CEffectID);
            _GCSkillToTileOK1.setX(X);
            _GCSkillToTileOK1.setY(Y);
            _GCSkillToTileOK1.setRange(dir);
            _GCSkillToTileOK1.setDuration(0);
            _GCSkillToTileOK1.setGrade(param.Grade);

            _GCSkillToTileOK2.setObjectID(pOusters->getObjectID());
            _GCSkillToTileOK2.setSkillType(param.SkillType);
            _GCSkillToTileOK2.setX(X);
            _GCSkillToTileOK2.setY(Y);
            _GCSkillToTileOK2.setRange(dir);
            _GCSkillToTileOK2.setDuration(0);
            _GCSkillToTileOK2.setGrade(param.Grade);

            _GCSkillToTileOK5.setObjectID(pOusters->getObjectID());
            _GCSkillToTileOK5.setSkillType(param.SkillType);
            _GCSkillToTileOK5.setX(X);
            _GCSkillToTileOK5.setY(Y);
            _GCSkillToTileOK5.setRange(dir);
            _GCSkillToTileOK5.setDuration(0);
            _GCSkillToTileOK5.setGrade(param.Grade);

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
                    pMonster->addEnemy(pOusters);
                }
            }
            result.targetCreatures = cList;

            cList.push_back(pOusters);

            pZone->broadcastPacket(myX, myY, &_GCSkillToTileOK5, cList);

            if (param.Delay != 0xffffffff) {
                // set Next Run Time
                pOustersSkillSlot->setRunTime(param.Delay);
            }

            result.bSuccess = true;
        } else {
            executeSkillFailNormal(pOusters, param.SkillType, NULL, param.Grade);
        }
    } catch (Throwable& t) {
        executeSkillFailException(pOusters, param.SkillType, param.Grade);
    }
}

void SimpleTileMissileSkill::execute(Monster* pMonster, int X, int Y, const SIMPLE_SKILL_INPUT& param,
                                     SIMPLE_SKILL_OUTPUT& result, CEffectID_t CEffectID, bool bForceKnockback) {
    try {
        Zone* pZone = pMonster->getZone();
        Assert(pZone != NULL);

        GCSkillToTileOK2 _GCSkillToTileOK2;
        GCSkillToTileOK3 _GCSkillToTileOK3;
        GCSkillToTileOK4 _GCSkillToTileOK4;
        GCSkillToTileOK5 _GCSkillToTileOK5;
        GCSkillToTileOK6 _GCSkillToTileOK6;

        SkillInfo* pSkillInfo = de::gameContext().skillInfos().getSkillInfo(param.SkillType);

        bool bRangeCheck = verifyDistance(pMonster, X, Y, pSkillInfo->getRange());
        bool bHitRoll = HitRoll::isSuccessMagic(pMonster, pSkillInfo);

        if (param.SkillType == SKILL_ICE_LANCE)
            bRangeCheck = true;

        if (bRangeCheck && bHitRoll) {
            // Work out the coordinates and the direction.
            ZoneCoord_t myX = pMonster->getX();
            ZoneCoord_t myY = pMonster->getY();
            Dir_t dir = calcDirection(myX, myY, X, Y);
            Damage_t Damage = 0;
            bool bCriticalHit = false;

            VSRect rect(1, 1, pZone->getWidth() - 2, pZone->getHeight() - 2);

            list<Creature*> cList;

            list<TILE_MASK>::const_iterator itr = param.MaskList.begin();
            for (; itr != param.MaskList.end(); itr++) {
                TILE_MASK mask = (*itr);

                int tileX = X + mask.x;
                int tileY = Y + mask.y;
                int penalty = mask.penalty;

                // A tile inside the zone that is not a safe zone can be hit.
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
                        Creature* pEnemy = (*itr);
                        Assert(pEnemy != NULL);

                        cout << pMonster->getName() << " checks enemy " << pEnemy->getName() << endl;

                        // When the creature is a valid attack target...
                        if (pMonster->isEnemyToAttack(pEnemy)) {
                            bool bPK = verifyPK(pMonster, pEnemy);
                            bool bRaceCheck = !pEnemy->isNPC(); // pEnemy->isSlayer() || pEnemy->isVampire();
                            bool bZoneLevelCheck = checkZoneLevelToHitTarget(pEnemy);

                            if (bPK && bRaceCheck && bZoneLevelCheck) {
                                Damage = 0;
                                bCriticalHit = false;

                                if (param.bAdd) {
                                    Damage += computeDamage(pMonster, pEnemy, 0, bCriticalHit);
                                }


                                if (param.bMagicDamage) {
                                    Damage += computeMagicDamage(pEnemy, param.SkillDamage, param.SkillType);
                                } else {
                                    Damage += param.SkillDamage;
                                }

                                // The penalty is 100 by default.
                                Damage = getPercentValue(Damage, penalty);


                                ObjectID_t targetObjectID = pEnemy->getObjectID();
                                cList.push_back(pEnemy);

                                _GCSkillToTileOK2.addCListElement(targetObjectID);
                                _GCSkillToTileOK5.addCListElement(targetObjectID);

                                if (param.SkillType == SKILL_GORE_GLAND_FIRE ||
                                    param.SkillType == SKILL_SUMMON_MIGA_ATTACK) {
                                    // Deal the damage with no packets for the creature that is hit.
                                    setDamage(pEnemy, Damage, pMonster, param.SkillType, NULL, NULL, false);
                                } else {
                                    setDamage(pEnemy, Damage, pMonster, param.SkillType, NULL, NULL);
                                }


                                // On a critical hit, push the target back.
                                if (bCriticalHit || bForceKnockback) {
                                    knockbackCreature(pZone, pEnemy, pMonster->getX(), pMonster->getY());
                                }
                            }
                        }
                    }
                }
            }

            _GCSkillToTileOK2.setObjectID(pMonster->getObjectID());
            _GCSkillToTileOK2.setSkillType(param.SkillType);
            _GCSkillToTileOK2.setX(X);
            _GCSkillToTileOK2.setY(Y);
            _GCSkillToTileOK2.setRange(dir);
            _GCSkillToTileOK2.setDuration(0);

            _GCSkillToTileOK5.setObjectID(pMonster->getObjectID());
            _GCSkillToTileOK5.setSkillType(param.SkillType);
            _GCSkillToTileOK5.setX(X);
            _GCSkillToTileOK5.setY(Y);
            _GCSkillToTileOK5.setRange(dir);
            _GCSkillToTileOK5.setDuration(0);

            // Send the packet to everyone affected by this skill.
            for (list<Creature*>::const_iterator itr = cList.begin(); itr != cList.end(); itr++) {
                Creature* pEnemy = *itr;
                Assert(pEnemy != NULL);

                if (pEnemy->isPC()) {
                    _GCSkillToTileOK2.clearList();

                    // Record the HP change in the packet.
                    HP_t targetHP = 0;
                    if (pEnemy->isSlayer()) {
                        targetHP = (dynamic_cast<Slayer*>(pEnemy))->getHP(ATTR_CURRENT);
                    } else if (pEnemy->isVampire()) {
                        targetHP = (dynamic_cast<Vampire*>(pEnemy))->getHP(ATTR_CURRENT);
                    } else if (pEnemy->isOusters()) {
                        targetHP = (dynamic_cast<Ousters*>(pEnemy))->getHP(ATTR_CURRENT);
                    }

                    _GCSkillToTileOK2.addShortData(MODIFY_CURRENT_HP, targetHP);

                    // Wear down the target's item durability.
                    decreaseDurability(NULL, pEnemy, pSkillInfo, NULL, &_GCSkillToTileOK2);

                    // Send the packet.
                    pEnemy->getPlayer()->sendPacket(&_GCSkillToTileOK2);

                } else if (pEnemy->isMonster()) {
                    // The monster takes the caster as an enemy.
                    Monster* pTargetMonster = dynamic_cast<Monster*>(pEnemy);
                    pTargetMonster->addEnemy(pMonster);
                }
            }
            result.targetCreatures = cList;

            cList.push_back(pMonster);

            pZone->broadcastPacket(myX, myY, &_GCSkillToTileOK5, cList);

            result.bSuccess = true;
        } else {
            executeSkillFailNormal(pMonster, param.SkillType, NULL);
        }
    } catch (Throwable& t) {
        executeSkillFailException(pMonster, param.SkillType);
    }
}
