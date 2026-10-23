//////////////////////////////////////////////////////////////////////////////
// Filename    : DuckingWallop.cpp
// Written by  : excel96
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "DuckingWallop.h"

#include "GCSkillToTileOK1.h"
#include "GCSkillToTileOK2.h"
#include "GCSkillToTileOK3.h"
#include "GCSkillToTileOK4.h"
#include "GCSkillToTileOK5.h"
#include "GCSkillToTileOK6.h"
#include "GameContext.h"
#include "RankBonus.h"
#include "SimpleTileMissileSkill.h"
#include "Utility.h"
#include "ZoneUtil.h"


//////////////////////////////////////////////////////////////////////////////
// Constructor
// Initializes the mask.
//////////////////////////////////////////////////////////////////////////////
DuckingWallop::DuckingWallop() {
    const int width[7] = {0, 1, 1, 1, 1, 1, 1};

    TPOINT mask[8];
    mask[0].x = -1;
    mask[0].y = 0;
    mask[1].x = -1;
    mask[1].y = 1;
    mask[2].x = 0;
    mask[2].y = 1;
    mask[3].x = 1;
    mask[3].y = 1;
    mask[4].x = 1;
    mask[4].y = 0;
    mask[5].x = 1;
    mask[5].y = -1;
    mask[6].x = 0;
    mask[6].y = -1;
    mask[7].x = -1;
    mask[7].y = -1;

    for (int k = 0; k < 8; k++) {
        int l = 0;
        for (int i = 1; i <= 6; i++) {
            int x = 0;
            int y = 0;

            for (int j = 0; j <= width[i]; j++) {
                x = mask[k].x * i;
                y = mask[k].y * i;

                if (j == 0) {
                    m_DamageRatio[l] = 75;
                    m_pDuckingWallopMask[k][l++].set(x, y);
                } else {
                    int left = (k % 2 == 0 ? (k + 2) % 8 : (k + 3) % 8);
                    int right = (k % 2 == 0 ? (k + 6) % 8 : (k + 5) % 8);

                    int xl = x + (mask[left].x * j);
                    int yl = y + (mask[left].y * j);

                    int xr = x + (mask[right].x * j);
                    int yr = y + (mask[right].y * j);

                    m_DamageRatio[l] = 100;
                    m_pDuckingWallopMask[k][l++].set(xl, yl);
                    m_DamageRatio[l] = 100;
                    m_pDuckingWallopMask[k][l++].set(xr, yr);
                }
            }
        }
    }
}

//////////////////////////////////////////////////////////////////////////////
// Vampire object handler
//////////////////////////////////////////////////////////////////////////////
void DuckingWallop::execute(Ousters* pOusters, ObjectID_t TargetObjectID, OustersSkillSlot* pOustersSkillSlot,
                            CEffectID_t CEffectID)

{
    Assert(pOusters != NULL);
    Assert(pOustersSkillSlot != NULL);

    try {
        Zone* pZone = pOusters->getZone();
        Assert(pZone != NULL);

        Creature* pTargetCreature = pZone->getCreature(TargetObjectID);

        // A missing target fails the skill instead of throwing.
        if (pTargetCreature == NULL) {
            executeSkillFailException(pOusters, getSkillType());

            return;
        }

        execute(pOusters, pTargetCreature->getX(), pTargetCreature->getY(), pOustersSkillSlot, CEffectID);
    } catch (Throwable& t) {
        executeSkillFailException(pOusters, getSkillType());
    }
}

//////////////////////////////////////////////////////////////////////////////
// Vampire tile handler
//////////////////////////////////////////////////////////////////////////////
void DuckingWallop::execute(Ousters* pOusters, ZoneCoord_t X, ZoneCoord_t Y, OustersSkillSlot* pOustersSkillSlot,
                            CEffectID_t CEffectID)

{
    SkillType_t SkillType = getSkillType();

    // Knowledge of Blood gives a hit bonus of 10.
    if (pOusters->hasRankBonus(RankBonus::RANK_BONUS_KNOWLEDGE_OF_BLOOD)) {
        RankBonus* pRankBonus = pOusters->getRankBonus(RankBonus::RANK_BONUS_KNOWLEDGE_OF_BLOOD);
        Assert(pRankBonus != NULL);
    }

    try {
        SkillInput input(pOusters, pOustersSkillSlot);
        SkillOutput output;
        computeOutput(input, output);

        Dir_t dir = getDirectionToPosition(pOusters->getX(), pOusters->getY(), X, Y);

        // Chance of a forced knockback.

        Player* pPlayer = pOusters->getPlayer();
        Zone* pZone = pOusters->getZone();
        Assert(pPlayer != NULL);
        Assert(pZone != NULL);

        VSRect rect(1, 1, pZone->getWidth() - 2, pZone->getHeight() - 2);
        if (!rect.ptInRect(X, Y)) {
            executeSkillFailException(pOusters, SkillType);
            return;
        }

        GCSkillToTileOK1 _GCSkillToTileOK1;
        GCSkillToTileOK2 _GCSkillToTileOK2;
        GCSkillToTileOK5 _GCSkillToTileOK5;

        SkillInfo* pSkillInfo = de::gameContext().skillInfos().getSkillInfo(SkillType);

        int RequiredMP = (int)pSkillInfo->getConsumeMP();
        bool bManaCheck = hasEnoughMana(pOusters, RequiredMP);
        bool bTimeCheck = verifyRunTime(pOustersSkillSlot);
        bool bRangeCheck = verifyDistance(pOusters, X, Y, pSkillInfo->getRange());

        ZoneCoord_t myX = pOusters->getX();
        ZoneCoord_t myY = pOusters->getY();
        int TargetX = (int)myX + dirMoveMask[dir].x * 6;
        int TargetY = (int)myY + dirMoveMask[dir].y * 6;

        bool bPassLine = isPassLine(pZone, myX, myY, TargetX, TargetY);

        if (bManaCheck && bTimeCheck && bRangeCheck && bPassLine &&
            pZone->moveFastPC(pOusters, myX, myY, TargetX, TargetY, getSkillType())) {
            // Consume the mana.
            decreaseMana(pOusters, RequiredMP, _GCSkillToTileOK1);

            // Work out the coordinates and the direction.

            list<Creature*> cList;

            // Knockback can make a creature take damage recursively.
            // So check the mask starting from the farthest tile.
            for (int i = 17; i >= 0; i--) {
                int tileX = myX + m_pDuckingWallopMask[dir][i].x;
                int tileY = myY + m_pDuckingWallopMask[dir][i].y;

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

                        if (!canAttack(pOusters, pTargetCreature) ||
                            pTargetCreature->isFlag(Effect::EFFECT_CLASS_COMA)) {
                            continue;
                        }

                        if (pTargetCreature != pOusters) {
                            bool bPK = verifyPK(pOusters, pTargetCreature);
                            bool bRaceCheck = pTargetCreature->isSlayer() || pTargetCreature->isMonster() ||
                                              pTargetCreature->isVampire();
                            bool bZoneLevelCheck = checkZoneLevelToHitTarget(pTargetCreature);
                            bool bHitRoll = HitRoll::isSuccess(pOusters, pTargetCreature);

                            // min : 20, max : 100
                            if (bPK && bRaceCheck && bZoneLevelCheck && bHitRoll) {
                                Damage_t Damage = computeDamage(pOusters, pTargetCreature);
                                Damage += output.Damage;

                                ObjectID_t targetObjectID = pTargetCreature->getObjectID();
                                cList.push_back(pTargetCreature);

                                _GCSkillToTileOK1.addCListElement(targetObjectID);
                                _GCSkillToTileOK2.addCListElement(targetObjectID);
                                _GCSkillToTileOK5.addCListElement(targetObjectID);

                                // Apply the damage, leaving the target's packet null for now.
                                setDamage(pTargetCreature, Damage, pOusters, SkillType, NULL, &_GCSkillToTileOK1);
                                computeAlignmentChange(pTargetCreature, Damage, pOusters, NULL, &_GCSkillToTileOK1);

                                increaseAlignment(pOusters, pTargetCreature, _GCSkillToTileOK1);

                                if (pTargetCreature->isDead()) {
                                    int exp = computeCreatureExp(pTargetCreature, 100);
                                    shareOustersExp(pOusters, exp, _GCSkillToTileOK1);
                                }
                            }
                        }
                    }
                }
            }


            // Wear down the attacker's item durability.
            decreaseDurability(pOusters, NULL, pSkillInfo, &_GCSkillToTileOK1, NULL);

            _GCSkillToTileOK1.setSkillType(SkillType);
            _GCSkillToTileOK1.setCEffectID(0);
            _GCSkillToTileOK1.setX(X);
            _GCSkillToTileOK1.setY(Y);
            _GCSkillToTileOK1.setRange(dir);
            _GCSkillToTileOK1.setDuration(0);

            _GCSkillToTileOK2.setObjectID(pOusters->getObjectID());
            _GCSkillToTileOK2.setSkillType(SkillType);
            _GCSkillToTileOK2.setX(X);
            _GCSkillToTileOK2.setY(Y);
            _GCSkillToTileOK2.setRange(dir);
            _GCSkillToTileOK2.setDuration(0);

            _GCSkillToTileOK5.setObjectID(pOusters->getObjectID());
            _GCSkillToTileOK5.setSkillType(SkillType);
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
                    pMonster->addEnemy(pOusters);
                }
            }

            cList.push_back(pOusters);

            pZone->broadcastPacket(myX, myY, &_GCSkillToTileOK5, cList);

            // set Next Run Time
            pOustersSkillSlot->setRunTime(output.Delay);
        } else {
            executeSkillFailNormal(pOusters, SkillType, NULL);
        }
    } catch (Throwable& t) {
        executeSkillFailException(pOusters, SkillType);
    }
}

DuckingWallop g_DuckingWallop;
