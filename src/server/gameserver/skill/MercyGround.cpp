//////////////////////////////////////////////////////////////////////////////
// Filename    : MercyGround.cpp
// Written by  :
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "MercyGround.h"

#include "EffectMercyGround.h"
#include "GCAddEffectToTile.h"
#include "GCSkillFailed1.h"
#include "GCSkillToTileOK1.h"
#include "GCSkillToTileOK2.h"
#include "GCSkillToTileOK3.h"
#include "GCSkillToTileOK4.h"
#include "GCSkillToTileOK5.h"
#include "GCSkillToTileOK6.h"
#include "GameContext.h"

//////////////////////////////////////////////////////////////////////////////
// Slayer object handler
//////////////////////////////////////////////////////////////////////////////
void MercyGround::execute(Slayer* pSlayer, ObjectID_t TargetObjectID, SkillSlot* pSkillSlot, CEffectID_t CEffectID)

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
void MercyGround::execute(Slayer* pSlayer, ZoneCoord_t X, ZoneCoord_t Y, SkillSlot* pSkillSlot, CEffectID_t CEffectID)

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
        GCSkillToTileOK6 _GCSkillToTileOK6;

        SkillType_t SkillType = pSkillSlot->getSkillType();
        SkillInfo* pSkillInfo = de::gameContext().skillInfos().getSkillInfo(SkillType);

        ZoneCoord_t myX = pSlayer->getX();
        ZoneCoord_t myY = pSlayer->getY();

        int RequiredMP = (int)pSkillInfo->getConsumeMP();
        bool bManaCheck = hasEnoughMana(pSlayer, RequiredMP);
        bool bTimeCheck = verifyRunTime(pSkillSlot);
        bool bRangeCheck = verifyDistance(pSlayer, X, Y, pSkillInfo->getRange()) && checkZoneLevelToUseSkill(pSlayer);
        bool bHitRoll = HitRoll::isSuccessMagic(pSlayer, pSkillInfo, pSkillSlot);

        bool bTileCheck = false;
        VSRect rect(0, 0, pZone->getWidth() - 1, pZone->getHeight() - 1);
        if (rect.ptInRect(X, Y))
            bTileCheck = true;

        if (bManaCheck && bTimeCheck && bRangeCheck && bHitRoll && bTileCheck) {
            // Compute the effect's duration.
            SkillInput input(pSlayer, pSkillSlot);
            SkillOutput output;
            computeOutput(input, output);

            Range_t Range = 3;

            int oX, oY;

            list<Creature*> cList;

            for (oY = -1; oY <= 1; oY++)
                for (oX = -1; oX <= 1; oX++) {
                    int tileX = X + oX;
                    int tileY = Y + oY;
                    if (rect.ptInRect(tileX, tileY)) {
                        Tile& tile = pZone->getTile(tileX, tileY);

                        if (tile.canAddEffect()) {
                            if (tile.getEffect(Effect::EFFECT_CLASS_DARKNESS) != NULL ||
                                tile.getEffect(Effect::EFFECT_CLASS_PROMINENCE) != NULL ||
                                tile.getEffect(Effect::EFFECT_CLASS_ACID_SWAMP) != NULL ||
                                tile.getEffect(Effect::EFFECT_CLASS_MERCY_GROUND) != NULL ||
                                tile.getEffect(Effect::EFFECT_CLASS_HEAVEN_GROUND) != NULL ||
                                tile.getEffect(Effect::EFFECT_CLASS_SUMMON_CLAY) != NULL) {
                                executeSkillFailNormal(pSlayer, getSkillType(), NULL);

                                return;
                            }
                        } else {
                            executeSkillFailNormal(pSlayer, getSkillType(), NULL);
                            return;
                        }
                    }
                }

            // Mana must not be consumed on failure, so it is decreased here.
            decreaseMana(pSlayer, RequiredMP, _GCSkillToTileOK1);

            for (oY = -1; oY <= 1; oY++)
                for (oX = -1; oX <= 1; oX++) {
                    int tileX = X + oX;
                    int tileY = Y + oY;
                    if (rect.ptInRect(tileX, tileY)) {
                        Tile& tile = pZone->getTile(tileX, tileY);

                        // If the effect can be added to this tile.
                        {
                            // Create the effect object.
                            EffectMercyGround* pEffect = new EffectMercyGround(pZone, tileX, tileY);
                            pEffect->setDeadline(output.Duration);

                            // An effect attached to a tile must be assigned an object ID.
                            pZone->registerObject(pEffect);
                            pZone->addEffect(pEffect);
                            tile.addEffect(pEffect);

                            if (oX == 0 && oY == 0) {
                                GCAddEffectToTile gcAddEffectToTile;
                                gcAddEffectToTile.setEffectID(pEffect->getEffectClass());
                                gcAddEffectToTile.setObjectID(pEffect->getObjectID());
                                gcAddEffectToTile.setXY(X, Y);
                                gcAddEffectToTile.setDuration(output.Duration);

                                pZone->broadcastPacket(X, Y, &gcAddEffectToTile, pSlayer);

                                pEffect->setBroadcastingEffect(true);
                            } else {
                                pEffect->setBroadcastingEffect(false);
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

            _GCSkillToTileOK2.setObjectID(pSlayer->getObjectID());
            _GCSkillToTileOK2.setSkillType(SkillType);
            _GCSkillToTileOK2.setX(X);
            _GCSkillToTileOK2.setY(Y);
            _GCSkillToTileOK2.setDuration(output.Duration);
            _GCSkillToTileOK2.setRange(Range);

            _GCSkillToTileOK3.setObjectID(pSlayer->getObjectID());
            _GCSkillToTileOK3.setSkillType(SkillType);
            _GCSkillToTileOK3.setX(X);
            _GCSkillToTileOK3.setY(Y);

            _GCSkillToTileOK4.setSkillType(SkillType);
            _GCSkillToTileOK4.setX(X);
            _GCSkillToTileOK4.setY(Y);
            _GCSkillToTileOK4.setRange(Range);
            _GCSkillToTileOK4.setDuration(output.Duration);

            _GCSkillToTileOK5.setObjectID(pSlayer->getObjectID());
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

            // EXP UP!
            SkillDomainType_t DomainType = pSkillInfo->getDomainType();
            SkillGrade Grade =
                de::gameContext().skillInfos().getGradeByDomainLevel(pSlayer->getSkillDomainLevel(DomainType));
            Exp_t ExpUp = 10 * (Grade + 1);

            shareAttrExp(pSlayer, ExpUp, 1, 1, 8, _GCSkillToTileOK1);
            increaseDomainExp(pSlayer, DomainType, pSkillInfo->getPoint(), _GCSkillToTileOK1);
            increaseSkillExp(pSlayer, DomainType, pSkillSlot, pSkillInfo, _GCSkillToTileOK1);

            for (list<Creature*>::const_iterator itr = cList.begin(); itr != cList.end(); itr++) {
                Creature* pTargetCreature = *itr;
                if (canSee(pTargetCreature, pSlayer))
                    pTargetCreature->getPlayer()->sendPacket(&_GCSkillToTileOK2);
                else
                    pTargetCreature->getPlayer()->sendPacket(&_GCSkillToTileOK6);
            }

            pPlayer->sendPacket(&_GCSkillToTileOK1);

            cList.push_back(pSlayer);

            list<Creature*> watcherList = pZone->getWatcherList(myX, myY, pSlayer);

            // Watchers that are not in cList and cannot see the caster (pSlayer)
            // are sent OK4 and added to cList.
            for (list<Creature*>::const_iterator itr = watcherList.begin(); itr != watcherList.end(); itr++) {
                bool bBelong = false;
                for (list<Creature*>::const_iterator tItr = cList.begin(); tItr != cList.end(); tItr++)
                    if (*itr == *tItr)
                        bBelong = true;

                Creature* pWatcher = (*itr);
                if (bBelong == false && canSee(pWatcher, pSlayer) == false) {
                    if (!pWatcher->isPC()) {
                        GCSkillFailed1 _GCSkillFailed1;
                        _GCSkillFailed1.setSkillType(getSkillType());
                        pSlayer->getPlayer()->sendPacket(&_GCSkillFailed1);

                        return;
                    }
                    pWatcher->getPlayer()->sendPacket(&_GCSkillToTileOK4);
                    cList.push_back(*itr);
                }
            }

            cList = pZone->broadcastSkillPacket(myX, myY, X, Y, &_GCSkillToTileOK5, cList, false);


            pZone->broadcastPacket(myX, myY, &_GCSkillToTileOK3, cList);

            pZone->broadcastPacket(X, Y, &_GCSkillToTileOK4, cList);

            pSkillSlot->setRunTime(output.Delay);
        } else {
            executeSkillFailNormal(pSlayer, getSkillType(), NULL);
        }
    } catch (Throwable& t) {
        executeSkillFailException(pSlayer, getSkillType());
    }
}

MercyGround g_MercyGround;
