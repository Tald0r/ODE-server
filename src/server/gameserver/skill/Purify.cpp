//////////////////////////////////////////////////////////////////////////////
// Filename    : Purify.cpp
// Written by  : Elca
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "Purify.h"

#include "Assert.h"
#include "EffectDarkness.h"
#include "GCDeleteEffectFromTile.h"
#include "GCSkillToTileOK1.h"
#include "GCSkillToTileOK2.h"
#include "GCSkillToTileOK3.h"
#include "GCSkillToTileOK4.h"
#include "GCSkillToTileOK5.h"
#include "GameContext.h"
#include "ZoneUtil.h"

//////////////////////////////////////////////////////////////////////////////
// Slayer object handler
//////////////////////////////////////////////////////////////////////////////
void Purify::execute(Slayer* pSlayer, ObjectID_t TargetObjectID, SkillSlot* pSkillSlot, CEffectID_t CEffectID)

{
    Assert(pSlayer != NULL);
    Assert(pSkillSlot != NULL);

    try {
        Zone* pZone = pSlayer->getZone();
        Assert(pZone != NULL);

        Creature* pTargetCreature = pZone->getCreature(TargetObjectID);

        // A missing target fails the skill instead of throwing.
        if (pTargetCreature == NULL) {
            executeSkillFailException(pSlayer, getSkillType());
            return;
        }

        execute(pSlayer, pTargetCreature->getX(), pTargetCreature->getY(), pSkillSlot, CEffectID);
    } catch (Throwable& t) {
        executeSkillFailException(pSlayer, getSkillType());
    }
}

//////////////////////////////////////////////////////////////////////////////
// Slayer tile handler
//////////////////////////////////////////////////////////////////////////////
void Purify::execute(Slayer* pSlayer, ZoneCoord_t X, ZoneCoord_t Y, SkillSlot* pSkillSlot, CEffectID_t CEffectID)

{
    Assert(pSlayer != NULL);
    Assert(pSkillSlot != NULL);

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

        SkillType_t SkillType = pSkillSlot->getSkillType();
        SkillInfo* pSkillInfo = de::gameContext().skillInfos().getSkillInfo(SkillType);
        SkillDomainType_t DomainType = pSkillInfo->getDomainType();
        SkillLevel_t SkillLevel = pSkillSlot->getExpLevel();

        int RequiredMP = (int)pSkillInfo->getConsumeMP();
        bool bManaCheck = hasEnoughMana(pSlayer, RequiredMP);
        bool bTimeCheck = verifyRunTime(pSkillSlot);
        bool bRangeCheck = verifyDistance(pSlayer, X, Y, pSkillInfo->getRange());

        bool bTileCheck = false;
        VSRect rect(0, 0, pZone->getWidth() - 1, pZone->getHeight() - 1);
        if (rect.ptInRect(X, Y)) {
            Tile& tile = pZone->getTile(X, Y);
            if (tile.hasCreature(Creature::MOVE_MODE_BURROWING))
                bTileCheck = true;
        }

        if (bManaCheck && bTimeCheck && bRangeCheck && bTileCheck) {
            decreaseMana(pSlayer, RequiredMP, _GCSkillToTileOK1);

            SkillGrade Grade =
                de::gameContext().skillInfos().getGradeByDomainLevel(pSlayer->getSkillDomainLevel(DomainType));

            SkillInput input(pSlayer, pSkillSlot);
            SkillOutput output;
            computeOutput(input, output);

            Damage_t Damage = output.Damage;
            Range_t Range = output.Range;

            int oX = 0, oY = 0;
            list<Creature*> cList; // List of affected creatures
            for (oY = -Range; oY <= Range; oY++) {
                for (oX = -Range; oX <= Range; oX++) {
                    int tileX = X + oX;
                    int tileY = Y + oY;

                    if (rect.ptInRect(tileX, tileY)) {
                        Tile& tile = pZone->getTile(tileX, tileY);

                        // If a burrowing creature is on the current tile...
                        if (tile.hasCreature(Creature::MOVE_MODE_BURROWING)) {
                            Creature* pTargetCreature = tile.getCreature(Creature::MOVE_MODE_BURROWING);
                            Assert(pTargetCreature != NULL);

                            int ratio = 0;

                            if (pTargetCreature->isVampire()) {
                                Vampire* pVampire = dynamic_cast<Vampire*>(pTargetCreature);
                                ratio = 75 + SkillLevel - pVampire->getLevel();
                            } else if (pTargetCreature->isMonster()) {
                                Monster* pMonster = dynamic_cast<Monster*>(pTargetCreature);
                                ratio = 75 + SkillLevel - pMonster->getLevel();
                            } else {
                                Assert(false);
                            }

                            bool UnBurrow = false;
                            if (rand() % 100 < ratio)
                                UnBurrow = true;

                            if (UnBurrow) {
                                // Add it to the list of creatures pulled out of the ground.
                                cList.push_back(pTargetCreature);

                                ObjectID_t targetObjectID = pTargetCreature->getObjectID();

                                _GCSkillToTileOK1.addCListElement(targetObjectID);
                                _GCSkillToTileOK2.addCListElement(targetObjectID);
                                _GCSkillToTileOK5.addCListElement(targetObjectID);

                                // Pull the burrowed creature out and deal damage to it.
                                addUnburrowCreature(pZone, pTargetCreature, tileX, tileY, pTargetCreature->getDir());
                                setDamage(pTargetCreature, Damage, pSlayer, SkillType, &_GCSkillToTileOK2,
                                          &_GCSkillToTileOK1);
                            }
                        }
                    }
                }
            }

            // Raises experience.
            Exp_t ExpUp = 10 * (Grade + 1);
            shareAttrExp(pSlayer, ExpUp, 1, 1, 8, _GCSkillToTileOK1);
            increaseDomainExp(pSlayer, DomainType, pSkillInfo->getPoint(), _GCSkillToTileOK1);
            increaseSkillExp(pSlayer, DomainType, pSkillSlot, pSkillInfo, _GCSkillToTileOK1);

            ZoneCoord_t myX = pSlayer->getX();
            ZoneCoord_t myY = pSlayer->getY();

            _GCSkillToTileOK1.setSkillType(SkillType);
            _GCSkillToTileOK1.setCEffectID(CEffectID);
            _GCSkillToTileOK1.setX(X);
            _GCSkillToTileOK1.setY(Y);
            _GCSkillToTileOK1.setDuration(0);

            _GCSkillToTileOK2.setObjectID(pSlayer->getObjectID());
            _GCSkillToTileOK2.setSkillType(SkillType);
            _GCSkillToTileOK2.setX(X);
            _GCSkillToTileOK2.setY(Y);
            _GCSkillToTileOK2.setDuration(0);
            _GCSkillToTileOK2.setRange(Range);

            _GCSkillToTileOK3.setObjectID(pSlayer->getObjectID());
            _GCSkillToTileOK3.setSkillType(SkillType);
            _GCSkillToTileOK3.setX(X);
            _GCSkillToTileOK3.setY(Y);

            _GCSkillToTileOK4.setSkillType(SkillType);
            _GCSkillToTileOK4.setX(X);
            _GCSkillToTileOK4.setY(Y);
            _GCSkillToTileOK4.setRange(Range);
            _GCSkillToTileOK4.setDuration(0);

            _GCSkillToTileOK5.setObjectID(pSlayer->getObjectID());
            _GCSkillToTileOK5.setSkillType(SkillType);
            _GCSkillToTileOK5.setX(X);
            _GCSkillToTileOK5.setY(Y);
            _GCSkillToTileOK5.setRange(Range);
            _GCSkillToTileOK5.setDuration(0);

            // Tell the creatures pulled out of the ground that their HP dropped.
            for (list<Creature*>::const_iterator itr = cList.begin(); itr != cList.end(); itr++) {
                Creature* pTargetCreature = *itr;

                if (pTargetCreature->isPC()) {
                    _GCSkillToTileOK2.clearList();
                    HP_t targetHP = 0;
                    if (pTargetCreature->isSlayer()) {
                        targetHP = (dynamic_cast<Slayer*>(pTargetCreature))->getHP(ATTR_CURRENT);
                    } else if (pTargetCreature->isVampire()) {
                        targetHP = (dynamic_cast<Vampire*>(pTargetCreature))->getHP(ATTR_CURRENT);
                    } else if (pTargetCreature->isOusters()) {
                        targetHP = (dynamic_cast<Ousters*>(pTargetCreature))->getHP(ATTR_CURRENT);
                    }

                    _GCSkillToTileOK2.addShortData(MODIFY_CURRENT_HP, targetHP);
                    pTargetCreature->getPlayer()->sendPacket(&_GCSkillToTileOK2);
                } else if (pTargetCreature->isMonster()) {
                    (dynamic_cast<Monster*>(pTargetCreature))->addEnemy(pSlayer);
                }
            }

            pPlayer->sendPacket(&_GCSkillToTileOK1);

            cList.push_back(pSlayer);

            cList = pZone->broadcastSkillPacket(myX, myY, X, Y, &_GCSkillToTileOK5, cList);

            pZone->broadcastPacket(myX, myY, &_GCSkillToTileOK3, cList);
            pZone->broadcastPacket(X, Y, &_GCSkillToTileOK4, cList);

            // Set NextTime
            pSkillSlot->setRunTime(output.Delay);
        } else {
            executeSkillFailNormal(pSlayer, getSkillType(), NULL);
        }
    } catch (Throwable& t) {
        executeSkillFailException(pSlayer, getSkillType());
    }
}

Purify g_Purify;
