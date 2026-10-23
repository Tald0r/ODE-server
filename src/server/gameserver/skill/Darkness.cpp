//////////////////////////////////////////////////////////////////////////////
// Filename    : Darkness.cpp
// Written by  :
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "Darkness.h"

#include "EffectDarkness.h"
#include "GCAddEffect.h"
#include "GCSkillFailed1.h"
#include "GCSkillToTileOK1.h"
#include "GCSkillToTileOK2.h"
#include "GCSkillToTileOK3.h"
#include "GCSkillToTileOK4.h"
#include "GCSkillToTileOK5.h"
#include "GCSkillToTileOK6.h"
#include "GameContext.h"
#include "MonsterCorpse.h"
#include "RankBonus.h"
#include "ctf/FlagManager.h"

int normalizeCoord_DARKNESS(int x, int y, int edge) {
    return x * (edge * 2 + 1) + y;
}

//////////////////////////////////////////////////////////////////////////////
// Vampire object handler
//////////////////////////////////////////////////////////////////////////////
void Darkness::execute(Vampire* pVampire, ObjectID_t TargetObjectID, VampireSkillSlot* pVampireSkillSlot,
                       CEffectID_t CEffectID)

{
    Assert(pVampire != NULL);
    Assert(pVampireSkillSlot != NULL);

    try {
        Zone* pZone = pVampire->getZone();
        Assert(pZone != NULL);

        Creature* pTargetCreature = pZone->getCreature(TargetObjectID);

        // A missing target fails the skill instead of throwing.
        if (pTargetCreature == NULL) {
            executeSkillFailException(pVampire, getSkillType());
            return;
        }

        execute(pVampire, pTargetCreature->getX(), pTargetCreature->getY(), pVampireSkillSlot, CEffectID);
    } catch (Throwable& t) {
        executeSkillFailException(pVampire, getSkillType());
    }
}

//////////////////////////////////////////////////////////////////////////////
// Vampire tile handler
//////////////////////////////////////////////////////////////////////////////
void Darkness::execute(Vampire* pVampire, ZoneCoord_t X, ZoneCoord_t Y, VampireSkillSlot* pVampireSkillSlot,
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

        ZoneCoord_t myX = pVampire->getX();
        ZoneCoord_t myY = pVampire->getY();

        // Knowledge of Innate gives a hit bonus of 10.
        int HitBonus = 0;
        if (pVampire->hasRankBonus(RankBonus::RANK_BONUS_KNOWLEDGE_OF_INNATE)) {
            RankBonus* pRankBonus = pVampire->getRankBonus(RankBonus::RANK_BONUS_KNOWLEDGE_OF_INNATE);
            Assert(pRankBonus != NULL);

            HitBonus = pRankBonus->getPoint();
        }

        int RequiredMP = decreaseConsumeMP(pVampire, pSkillInfo);
        bool bManaCheck = hasEnoughMana(pVampire, RequiredMP);
        bool bTimeCheck = verifyRunTime(pVampireSkillSlot);
        bool bRangeCheck = verifyDistance(pVampire, X, Y, pSkillInfo->getRange());
        bool bHitRoll = HitRoll::isSuccessMagic(pVampire, pSkillInfo, pVampireSkillSlot, HitBonus);
        bool bSlayerSafeZone = pZone->getZoneLevel(X, Y) & SLAYER_SAFE_ZONE;

        bool bTileCheck = false;
        VSRect rect(0, 0, pZone->getWidth() - 1, pZone->getHeight() - 1);
        if (rect.ptInRect(X, Y))
            bTileCheck = true;

        if (bManaCheck && bTimeCheck && bRangeCheck && bHitRoll && bTileCheck && !bSlayerSafeZone) {
            decreaseMana(pVampire, RequiredMP, _GCSkillToTileOK1);

            // Compute the effect's duration.
            SkillInput input(pVampire);
            SkillOutput output;
            computeOutput(input, output);

            // Wisdom of Darkness adds its bonus points to the duration as a percentage.
            if (pVampire->hasRankBonus(RankBonus::RANK_BONUS_WISDOM_OF_DARKNESS)) {
                RankBonus* pRankBonus = pVampire->getRankBonus(RankBonus::RANK_BONUS_WISDOM_OF_DARKNESS);
                Assert(pRankBonus != NULL);

                output.Duration += getPercentValue(output.Duration, pRankBonus->getPoint());
            }

            Range_t Range = 3;

            int oX, oY;

            list<Creature*> cList; // denier list

            int edge = 1;

            // Wide Darkness widens the area to its bonus point size and changes the skill type.
            if (pVampire->hasRankBonus(RankBonus::RANK_BONUS_WIDE_DARKNESS)) {
                RankBonus* pRankBonus = pVampire->getRankBonus(RankBonus::RANK_BONUS_WIDE_DARKNESS);
                Assert(pRankBonus != NULL);

                Range = pRankBonus->getPoint();
                edge = (pRankBonus->getPoint() - 1) / 2;

                SkillType = SKILL_DARKNESS_WIDE;
            }
            //////////////////////////////////////////////////////////////////////////
            // check
            for (oY = -edge; oY <= edge; oY++)
                for (oX = -edge; oX <= edge; oX++) {
                    int tileX = X + oX;
                    int tileY = Y + oY;
                    if (rect.ptInRect(tileX, tileY)) {
                        Tile& tile = pZone->getTile(tileX, tileY);
                        if (tile.canAddEffect()) {
                            if (tile.getEffect(Effect::EFFECT_CLASS_SUMMON_CLAY) != NULL) {
                                executeSkillFailNormal(pVampire, getSkillType(), NULL);
                                return;
                            }
                        }
                    }
                }

            for (oY = -edge; oY <= edge; oY++)
                for (oX = -edge; oX <= edge; oX++) {
                    int tileX = X + oX;
                    int tileY = Y + oY;
                    if (rect.ptInRect(tileX, tileY)) {
                        Tile& tile = pZone->getTile(tileX, tileY);


                        if (tile.hasItem()) {
                            Item* pItem = tile.getItem();
                            if (pItem != NULL && pItem->getItemClass() == Item::ITEM_CLASS_CORPSE &&
                                pItem->getItemType() == MONSTER_CORPSE) {
                                MonsterCorpse* pMonsterCorpse = dynamic_cast<MonsterCorpse*>(pItem);
                                if (de::gameContext().flags().isFlagPole(pMonsterCorpse)) {
                                    //								canAddMap[normalizeCoord_DARKNESS( oX+1, oY, edge )]
                                    continue;
                                }
                            }
                        }

                        if (tile.getEffect(Effect::EFFECT_CLASS_TRYING_POSITION) != NULL)
                            continue;

                        // If the effect can be added to this tile.
                        if (tile.canAddEffect()) {
                            // Cannot be added where Mercy Ground is present.
                            if (tile.getEffect(Effect::EFFECT_CLASS_MERCY_GROUND) != NULL)
                                continue;
                            if (tile.getEffect(Effect::EFFECT_CLASS_DARKNESS_FORBIDDEN) != NULL)
                                continue;

                            // Delete the same effect if one is already present.
                            Effect* pOldEffect = tile.getEffect(Effect::EFFECT_CLASS_DARKNESS);
                            if (pOldEffect != NULL) {
                                ObjectID_t effectID = pOldEffect->getObjectID();
                                pZone->deleteEffect(effectID); // fix me
                            }

                            // Create the effect object.
                            EffectDarkness* pEffect = new EffectDarkness(pZone, tileX, tileY);
                            pEffect->setDeadline(output.Duration);
                            pEffect->setLevel(pVampire->getINT());
                            pEffect->setDuration(output.Duration);
                            pEffect->setStartTime();

                            // An effect attached to a tile must be assigned an object ID.
                            ObjectRegistry& objectregister = pZone->getObjectRegistry();
                            objectregister.registerObject(pEffect);
                            pZone->addEffect(pEffect);
                            tile.addEffect(pEffect);

                            const forward_list<Object*>& oList = tile.getObjectList();
                            for (forward_list<Object*>::const_iterator itr = oList.begin(); itr != oList.end(); itr++) {
                                Object* pTarget = *itr;
                                Creature* pTargetCreature = NULL;
                                if (pTarget->getObjectClass() == Object::OBJECT_CLASS_CREATURE &&
                                    ((pTargetCreature = dynamic_cast<Creature*>(pTarget))->isSlayer() ||
                                     pTargetCreature->isOusters())) {
                                    cList.push_back(pTargetCreature);
                                    _GCSkillToTileOK2.addCListElement(pTargetCreature->getObjectID());
                                    _GCSkillToTileOK4.addCListElement(pTargetCreature->getObjectID());
                                    _GCSkillToTileOK5.addCListElement(pTargetCreature->getObjectID());
                                }

                                pEffect->affectObject(pTarget, false);
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

            _GCSkillToTileOK2.setObjectID(pVampire->getObjectID());
            _GCSkillToTileOK2.setSkillType(SkillType);
            _GCSkillToTileOK2.setX(X);
            _GCSkillToTileOK2.setY(Y);
            _GCSkillToTileOK2.setDuration(output.Duration);
            _GCSkillToTileOK2.setRange(Range);

            _GCSkillToTileOK3.setObjectID(pVampire->getObjectID());
            _GCSkillToTileOK3.setSkillType(SkillType);
            _GCSkillToTileOK3.setX(X);
            _GCSkillToTileOK3.setY(Y);

            _GCSkillToTileOK4.setSkillType(SkillType);
            _GCSkillToTileOK4.setX(X);
            _GCSkillToTileOK4.setY(Y);
            _GCSkillToTileOK4.setRange(Range);
            _GCSkillToTileOK4.setDuration(output.Duration);

            _GCSkillToTileOK5.setObjectID(pVampire->getObjectID());
            _GCSkillToTileOK5.setSkillType(SkillType);
            _GCSkillToTileOK5.setX(X);
            _GCSkillToTileOK5.setY(Y);
            _GCSkillToTileOK5.setRange(Range);
            _GCSkillToTileOK5.setDuration(output.Duration);

            _GCSkillToTileOK6.setOrgXY(myX, myY);
            _GCSkillToTileOK6.setSkillType(SkillType);
            _GCSkillToTileOK6.setX(X);
            _GCSkillToTileOK6.setY(Y);
            _GCSkillToTileOK6.setDuration(output.Duration);
            _GCSkillToTileOK6.setRange(Range);

            for (list<Creature*>::const_iterator itr = cList.begin(); itr != cList.end(); itr++) {
                Creature* pTargetCreature = *itr;
                if (canSee(pTargetCreature, pVampire))
                    pTargetCreature->getPlayer()->sendPacket(&_GCSkillToTileOK2);
                else
                    pTargetCreature->getPlayer()->sendPacket(&_GCSkillToTileOK6);
            }

            pPlayer->sendPacket(&_GCSkillToTileOK1);

            cList.push_back(pVampire);

            list<Creature*> watcherList = pZone->getWatcherList(myX, myY, pVampire);

            // Watchers that are not in cList and cannot see the caster (pVampire)
            // are sent OK4 and added to cList.
            for (list<Creature*>::const_iterator itr = watcherList.begin(); itr != watcherList.end(); itr++) {
                bool bBelong = false;
                for (list<Creature*>::const_iterator tItr = cList.begin(); tItr != cList.end(); tItr++)
                    if (*itr == *tItr)
                        bBelong = true;

                Creature* pWatcher = (*itr);
                if (bBelong == false && canSee(pWatcher, pVampire) == false) {
                    if (!pWatcher->isPC()) {
                        GCSkillFailed1 _GCSkillFailed1;
                        _GCSkillFailed1.setSkillType(getSkillType());
                        pVampire->getPlayer()->sendPacket(&_GCSkillFailed1);

                        return;
                    }
                    pWatcher->getPlayer()->sendPacket(&_GCSkillToTileOK4);
                    cList.push_back(*itr);
                }
            }

            cList = pZone->broadcastSkillPacket(myX, myY, X, Y, &_GCSkillToTileOK5, cList, false);

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
//////////////////////////////////////////////////////////////////////////////
void Darkness::execute(Monster* pMonster, Creature* pEnemy)

{
    Assert(pMonster != NULL);
    Assert(pEnemy != NULL);
    execute(pMonster, pEnemy->getX(), pEnemy->getY());
}

//////////////////////////////////////////////////////////////////////////////
// Monster self handler
//////////////////////////////////////////////////////////////////////////////
void Darkness::execute(Monster* pMonster, ZoneCoord_t X, ZoneCoord_t Y)

{
    Assert(pMonster != NULL);

    try {
        Zone* pZone = pMonster->getZone();

        Assert(pZone != NULL);

        GCSkillToTileOK2 _GCSkillToTileOK2;
        GCSkillToTileOK3 _GCSkillToTileOK3;
        GCSkillToTileOK4 _GCSkillToTileOK4;
        GCSkillToTileOK5 _GCSkillToTileOK5;
        GCSkillToTileOK6 _GCSkillToTileOK6;

        SkillType_t SkillType = SKILL_DARKNESS;
        SkillInfo* pSkillInfo = de::gameContext().skillInfos().getSkillInfo(SkillType);

        ZoneCoord_t myX = pMonster->getX();
        ZoneCoord_t myY = pMonster->getY();

        bool bRangeCheck = verifyDistance(pMonster, X, Y, pSkillInfo->getRange());
        bool bHitRoll = HitRoll::isSuccessMagic(pMonster, pSkillInfo);

        bool bTileCheck = false;
        VSRect rect(0, 0, pZone->getWidth() - 1, pZone->getHeight() - 1);
        if (rect.ptInRect(X, Y))
            bTileCheck = true;

        if (bRangeCheck && bHitRoll && bTileCheck) {
            // Compute the effect's duration.
            SkillInput input(pMonster);
            SkillOutput output;
            computeOutput(input, output);

            Range_t Range = 3;

            int oX, oY;

            list<Creature*> cList; // denier list

            for (oY = -1; oY <= 1; oY++)
                for (oX = -1; oX <= 1; oX++) {
                    int tileX = X + oX;
                    int tileY = Y + oY;
                    if (rect.ptInRect(tileX, tileY)) {
                        Tile& tile = pZone->getTile(tileX, tileY);

                        if (tile.hasItem()) {
                            Item* pItem = tile.getItem();
                            if (pItem != NULL && pItem->getItemClass() == Item::ITEM_CLASS_CORPSE &&
                                pItem->getItemType() == MONSTER_CORPSE) {
                                MonsterCorpse* pMonsterCorpse = dynamic_cast<MonsterCorpse*>(pItem);
                                if (de::gameContext().flags().isFlagPole(pMonsterCorpse))
                                    continue;
                            }
                        }

                        // If the effect can be added to this tile.
                        if (tile.canAddEffect()) {
                            // Cannot be added where Mercy Ground is present.
                            if (tile.getEffect(Effect::EFFECT_CLASS_MERCY_GROUND) != NULL)
                                continue;
                            if (tile.getEffect(Effect::EFFECT_CLASS_SUMMON_CLAY) != NULL)
                                continue;

                            // Delete the same effect if one is already present.
                            Effect* pOldEffect = tile.getEffect(Effect::EFFECT_CLASS_DARKNESS);
                            if (pOldEffect != NULL) {
                                ObjectID_t effectID = pOldEffect->getObjectID();
                                pZone->deleteEffect(effectID); // fix me
                            }

                            // Create the effect object.
                            EffectDarkness* pEffect = new EffectDarkness(pZone, tileX, tileY);
                            pEffect->setDeadline(output.Duration);
                            pEffect->setLevel(pMonster->getINT());
                            pEffect->setDuration(output.Duration);
                            pEffect->setStartTime();

                            // An effect attached to a tile must be assigned an object ID.
                            ObjectRegistry& objectregister = pZone->getObjectRegistry();
                            objectregister.registerObject(pEffect);

                            pZone->addEffect(pEffect);
                            tile.addEffect(pEffect);

                            const forward_list<Object*>& oList = tile.getObjectList();
                            for (forward_list<Object*>::const_iterator itr = oList.begin(); itr != oList.end(); itr++) {
                                Object* pTarget = *itr;
                                Creature* pTargetCreature = NULL;
                                if (pTarget->getObjectClass() == Object::OBJECT_CLASS_CREATURE &&
                                    ((pTargetCreature = dynamic_cast<Creature*>(pTarget))->isSlayer() ||
                                     pTargetCreature->isOusters())) {
                                    cList.push_back(pTargetCreature);
                                    _GCSkillToTileOK2.addCListElement(pTargetCreature->getObjectID());
                                    _GCSkillToTileOK4.addCListElement(pTargetCreature->getObjectID());
                                    _GCSkillToTileOK5.addCListElement(pTargetCreature->getObjectID());
                                }

                                pEffect->affectObject(pTarget, false);
                            }
                        }
                    }
                }

            _GCSkillToTileOK2.setObjectID(pMonster->getObjectID());
            _GCSkillToTileOK2.setSkillType(SkillType);
            _GCSkillToTileOK2.setX(X);
            _GCSkillToTileOK2.setY(Y);
            _GCSkillToTileOK2.setDuration(output.Duration);
            _GCSkillToTileOK2.setRange(Range);

            _GCSkillToTileOK3.setObjectID(pMonster->getObjectID());
            _GCSkillToTileOK3.setSkillType(SkillType);
            _GCSkillToTileOK3.setX(X);
            _GCSkillToTileOK3.setY(Y);

            _GCSkillToTileOK4.setSkillType(SkillType);
            _GCSkillToTileOK4.setX(X);
            _GCSkillToTileOK4.setY(Y);
            _GCSkillToTileOK4.setRange(Range);
            _GCSkillToTileOK4.setDuration(output.Duration);

            _GCSkillToTileOK5.setObjectID(pMonster->getObjectID());
            _GCSkillToTileOK5.setSkillType(SkillType);
            _GCSkillToTileOK5.setX(X);
            _GCSkillToTileOK5.setY(Y);
            _GCSkillToTileOK5.setRange(Range);
            _GCSkillToTileOK5.setDuration(output.Duration);

            _GCSkillToTileOK6.setOrgXY(myX, myY);
            _GCSkillToTileOK6.setSkillType(SkillType);
            _GCSkillToTileOK6.setX(X);
            _GCSkillToTileOK6.setY(Y);
            _GCSkillToTileOK6.setDuration(output.Duration);
            _GCSkillToTileOK6.setRange(Range);

            for (list<Creature*>::const_iterator itr = cList.begin(); itr != cList.end(); itr++) {
                Creature* pTargetCreature = *itr;
                if (canSee(pTargetCreature, pMonster))
                    pTargetCreature->getPlayer()->sendPacket(&_GCSkillToTileOK2);
                else
                    pTargetCreature->getPlayer()->sendPacket(&_GCSkillToTileOK6);
            }

            cList.push_back(pMonster);

            list<Creature*> watcherList = pZone->getWatcherList(myX, myY, pMonster);

            // Watchers that are not in cList and cannot see the caster (pMonster)
            // are sent OK4 and added to cList.
            for (list<Creature*>::const_iterator itr = watcherList.begin(); itr != watcherList.end(); itr++) {
                bool bBelong = false;
                for (list<Creature*>::const_iterator tItr = cList.begin(); tItr != cList.end(); tItr++)
                    if (*itr == *tItr)
                        bBelong = true;

                Creature* pWatcher = (*itr);
                if (bBelong == false && canSee(pWatcher, pMonster) == false) {
                    if (!pWatcher->isPC()) {
                        return;
                    }

                    pWatcher->getPlayer()->sendPacket(&_GCSkillToTileOK4);
                    cList.push_back(*itr);
                }
            }

            cList = pZone->broadcastSkillPacket(myX, myY, X, Y, &_GCSkillToTileOK5, cList, false);

            pZone->broadcastPacket(myX, myY, &_GCSkillToTileOK3, cList);

            pZone->broadcastPacket(X, Y, &_GCSkillToTileOK4, cList);
        } else {
            executeSkillFailNormal(pMonster, getSkillType(), NULL);
        }
    } catch (Throwable& t) {
        executeSkillFailException(pMonster, getSkillType());
    }
}

Darkness g_Darkness;
