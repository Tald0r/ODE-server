//////////////////////////////////////////////////////////////////////////////
// Filename    : TransformToBat.cpp
// Written by  : elca@ewestsoft.com
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "TransformToBat.h"

#include "CreatureUtil.h"
#include "DynamicZone.h"
#include "EffectTransformToBat.h"
#include "GCAddBat.h"
#include "GCDeleteObject.h"
#include "GCSkillToInventoryOK1.h"
#include "GCSkillToSelfOK1.h"
#include "GCSkillToSelfOK3.h"
#include "GDRLairManager.h"
#include "GQuestManager.h"
#include "GameContext.h"
#include "GamePlayer.h"
#include "ItemUtil.h"
#include "PKZoneInfoManager.h"
#include "RankBonus.h"
#include "SiegeManager.h"
#include "ZoneUtil.h"

//////////////////////////////////////////////////////////////////////////////
// Vampire inventory handler
//////////////////////////////////////////////////////////////////////////////
void TransformToBat::execute(Vampire* pVampire, ObjectID_t InvenObjectID, CoordInven_t X, CoordInven_t Y,
                             CoordInven_t TargetX, CoordInven_t TargetY, VampireSkillSlot* pSkillSlot)

{
    Assert(pVampire != NULL);
    Assert(pSkillSlot != NULL);

    try {
        Player* pPlayer = pVampire->getPlayer();
        Zone* pZone = pVampire->getZone();
        Inventory* pInventory = pVampire->getInventory();

        Assert(pPlayer != NULL);
        Assert(pInventory != NULL);
        Assert(pZone != NULL);

        Item* pItem = pInventory->getItem(X, Y);
        // Transformation requires a suitable item.
        // It is also blocked in a PK zone.
        if (pItem == NULL || pItem->getItemClass() != Item::ITEM_CLASS_VAMPIRE_ETC || pItem->getItemType() != 1 ||
            pVampire->hasRelicItem() || de::gameContext().pkZoneInfos().isPKZone(pZone->getZoneID()) ||
            pVampire->isFlag(Effect::EFFECT_CLASS_REFINIUM_TICKET) ||
            GDRLairManager::Instance().isGDRLairZone(pZone->getZoneID()) ||
            SiegeManager::Instance().isSiegeZone(pZone->getZoneID()) ||
            (pZone->isDynamicZone() && pZone->getDynamicZone()->getTemplateZoneID() == 4002) ||
            pZone->isNoPortalZone()) {
            executeSkillFailException(pVampire, getSkillType());
            return;
        }

        GCSkillToInventoryOK1 _GCSkillToInventoryOK1;

        SkillType_t SkillType = pSkillSlot->getSkillType();
        SkillInfo* pSkillInfo = de::gameContext().skillInfos().getSkillInfo(SkillType);

        ZoneCoord_t x = pVampire->getX();
        ZoneCoord_t y = pVampire->getY();
        Tile& tile = pZone->getTile(x, y);

        // Knowledge of Innate gives a hit bonus of 10.
        int HitBonus = 0;
        if (pVampire->hasRankBonus(RankBonus::RANK_BONUS_KNOWLEDGE_OF_INNATE)) {
            RankBonus* pRankBonus = pVampire->getRankBonus(RankBonus::RANK_BONUS_KNOWLEDGE_OF_INNATE);
            Assert(pRankBonus != NULL);

            HitBonus = pRankBonus->getPoint();
        }

        int RequiredMP = decreaseConsumeMP(pVampire, pSkillInfo);
        bool bManaCheck = hasEnoughMana(pVampire, RequiredMP);
        bool bTimeCheck = verifyRunTime(pSkillSlot);
        bool bRangeCheck = checkZoneLevelToUseSkill(pVampire);
        bool bHitRoll = HitRoll::isSuccessMagic(pVampire, pSkillInfo, pSkillSlot, HitBonus);
        bool bMoveModeCheck = pVampire->isWalking();
        bool bTileCheck =
            (canAddCreature(pZone, x, y, Creature::MOVE_MODE_FLYING) && tile.hasFlyingCreature() == false);
        bool bEffected = pVampire->isFlag(Effect::EFFECT_CLASS_TRANSFORM_TO_BAT) ||
                         pVampire->isFlag(Effect::EFFECT_CLASS_HAS_FLAG) ||
                         pVampire->isFlag(Effect::EFFECT_CLASS_HAS_SWEEPER);

        if (bManaCheck && bTimeCheck && bRangeCheck && bHitRoll && bMoveModeCheck && bTileCheck && !bEffected) {
            TPOINT pt = findSuitablePosition(pZone, x, y, Creature::MOVE_MODE_FLYING);

            if (pt.x != -1) // Check that a destination coordinate was found.
            {
                decreaseMana(pVampire, RequiredMP, _GCSkillToInventoryOK1);

                SkillInput input(pVampire);
                SkillOutput output;
                computeOutput(input, output);

                // Create the effect and attach it.
                EffectTransformToBat* pEffectTTW = new EffectTransformToBat(pVampire);
                pEffectTTW->setDeadline(99999999);
                pVampire->addEffect(pEffectTTW);
                pVampire->setFlag(Effect::EFFECT_CLASS_TRANSFORM_TO_BAT);

                // Send the stats that change as a result.
                VAMPIRE_RECORD prev;
                pVampire->getVampireRecord(prev);
                pVampire->initAllStat();
                pVampire->addModifyInfo(prev, _GCSkillToInventoryOK1);

                _GCSkillToInventoryOK1.setSkillType(SkillType);
                _GCSkillToInventoryOK1.setCEffectID(0);
                _GCSkillToInventoryOK1.setDuration(0);

                pPlayer->sendPacket(&_GCSkillToInventoryOK1);

                // Tell the clients to add a bat in place of the Vampire.
                GCAddBat gcAddBat;
                gcAddBat.setObjectID(pVampire->getObjectID());
                gcAddBat.setName(pVampire->getName());
                gcAddBat.setXYDir(x, y, pVampire->getDir());
                gcAddBat.setItemType(pItem->getItemType());
                gcAddBat.setCurrentHP(pVampire->getHP());
                gcAddBat.setMaxHP(pVampire->getHP(ATTR_MAX));
                gcAddBat.setGuildID(pVampire->getGuildID());
                gcAddBat.setColor(pVampire->getBatColor());
                pZone->broadcastPacket(x, y, &gcAddBat, pVampire);

                // A tile files a creature under its move mode, so the mode is changed by
                // taking the creature off its tile and adding it again.
                pZone->deleteCreatureFromTile(pVampire, x, y);

                pVampire->setMoveMode(Creature::MOVE_MODE_FLYING);
                pZone->addCreatureToTile(pVampire, pt.x, pt.y);
                pVampire->setXYDir(pt.x, pt.y, pVampire->getDir());

                decreaseItemNum(pItem, pInventory, pVampire->getName(), STORAGE_INVENTORY, 0, X, Y);

                pSkillSlot->setRunTime(output.Delay);

                if (pVampire->getPetInfo() != NULL) {
                    pVampire->setPetInfo(NULL);
                    sendPetInfo(dynamic_cast<GamePlayer*>(pVampire->getPlayer()), true);
                }

                pVampire->getGQuestManager()->rideMotorcycle();
            }
        } else {
            executeSkillFailNormal(pVampire, getSkillType(), NULL);
        }
    } catch (Throwable& t) {
        executeSkillFailException(pVampire, getSkillType());
    }
}

//////////////////////////////////////////////////////////////////////////////
// Monster self handler
//////////////////////////////////////////////////////////////////////////////
void TransformToBat::execute(Monster* pMonster)

{
    Assert(pMonster != NULL);

    try {
        Zone* pZone = pMonster->getZone();
        Assert(pZone != NULL);

        SkillType_t SkillType = SKILL_TRANSFORM_TO_BAT;
        SkillInfo* pSkillInfo = de::gameContext().skillInfos().getSkillInfo(SkillType);

        ZoneCoord_t x = pMonster->getX();
        ZoneCoord_t y = pMonster->getY();
        Tile& tile = pZone->getTile(x, y);

        bool bRangeCheck = checkZoneLevelToUseSkill(pMonster);
        bool bHitRoll = HitRoll::isSuccessMagic(pMonster, pSkillInfo);
        bool bMoveModeCheck = pMonster->isWalking();
        bool bTileCheck =
            (canAddCreature(pZone, x, y, Creature::MOVE_MODE_FLYING) && tile.hasFlyingCreature() == false);
        bool bEffected = pMonster->isFlag(Effect::EFFECT_CLASS_TRANSFORM_TO_BAT);

        if (bRangeCheck && bHitRoll && bMoveModeCheck && bTileCheck && !bEffected) {
            SkillInput input(pMonster);
            SkillOutput output;
            computeOutput(input, output);

            // Create the effect class and attach it.
            EffectTransformToBat* pEffectTTW = new EffectTransformToBat(pMonster);
            pEffectTTW->setDeadline(99999999);
            pMonster->addEffect(pEffectTTW);
            pMonster->setFlag(Effect::EFFECT_CLASS_TRANSFORM_TO_BAT);

            // Recompute the stats that change as a result.
            pMonster->initAllStat();

            // Tell the clients to add a bat in place of the monster.
            GCAddBat gcAddBat;
            gcAddBat.setObjectID(pMonster->getObjectID());
            gcAddBat.setName(pMonster->getName());
            gcAddBat.setXYDir(x, y, pMonster->getDir());
            gcAddBat.setItemType(1);
            gcAddBat.setCurrentHP(pMonster->getHP());
            gcAddBat.setMaxHP(pMonster->getHP(ATTR_MAX));
            gcAddBat.setGuildID(1);
            gcAddBat.setColor(0);
            pZone->broadcastPacket(x, y, &gcAddBat, pMonster);


            // A tile files a creature under its move mode, so the mode is changed by
            // taking the creature off its tile and adding it again.
            pZone->deleteCreatureFromTile(pMonster, x, y);

            TPOINT pt = findSuitablePosition(pZone, x, y, Creature::MOVE_MODE_FLYING);
            pMonster->setMoveMode(Creature::MOVE_MODE_FLYING);
            pZone->addCreatureToTile(pMonster, pt.x, pt.y);
            pMonster->setXYDir(pt.x, pt.y, pMonster->getDir());

        } else {
            executeSkillFailNormal(pMonster, getSkillType(), NULL);
        }
    } catch (Throwable& t) {
        executeSkillFailException(pMonster, getSkillType());
    }
}


TransformToBat g_TransformToBat;
