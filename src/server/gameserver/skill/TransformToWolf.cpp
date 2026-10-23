//////////////////////////////////////////////////////////////////////////////
// Filename    : TransformToWolf.cpp
// Written by  : elca@ewestsoft.com
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "TransformToWolf.h"

#include "CreatureUtil.h"
#include "EffectTransformToWolf.h"
#include "GCAddWolf.h"
#include "GCDeleteObject.h"
#include "GCSkillToInventoryOK1.h"
#include "GCSkillToSelfOK1.h"
#include "GCSkillToSelfOK3.h"
#include "GDRLairManager.h"
#include "GameContext.h"
#include "GamePlayer.h"
#include "ItemUtil.h"
#include "PKZoneInfoManager.h"
#include "RankBonus.h"

//////////////////////////////////////////////////////////////////////////////
// Vampire inventory handler
//////////////////////////////////////////////////////////////////////////////
void TransformToWolf::execute(Vampire* pVampire, ObjectID_t InvenObjectID, CoordInven_t X, CoordInven_t Y,
                              CoordInven_t TargetX, CoordInven_t TargetY, VampireSkillSlot* pSkillSlot)

{
    Assert(pVampire != NULL);
    Assert(pSkillSlot != NULL);

    try {
        Player* pPlayer = pVampire->getPlayer();
        Zone* pZone = pVampire->getZone();
        Inventory* pInventory = pVampire->getInventory();

        Assert(pPlayer != NULL);
        Assert(pZone != NULL);
        Assert(pInventory != NULL);

        Item* pItem = pInventory->getItem(X, Y);
        Assert(pItem != NULL);

        // Transformation requires a suitable item.
        // Transformation is not possible in a PK zone.
        if (pItem->getItemClass() != Item::ITEM_CLASS_VAMPIRE_ETC || pItem->getItemType() != 0 ||
            pVampire->hasRelicItem() || de::gameContext().pkZoneInfos().isPKZone(pZone->getZoneID()) ||
            pVampire->isFlag(Effect::EFFECT_CLASS_REFINIUM_TICKET) ||
            GDRLairManager::Instance().isGDRLairZone(pZone->getZoneID())) {
            executeSkillFailException(pVampire, getSkillType());
            return;
        }

        GCSkillToInventoryOK1 _GCSkillToInventoryOK1;

        SkillType_t SkillType = pSkillSlot->getSkillType();
        SkillInfo* pSkillInfo = de::gameContext().skillInfos().getSkillInfo(SkillType);

        ZoneCoord_t x = pVampire->getX();
        ZoneCoord_t y = pVampire->getY();

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
        bool bEffected = pVampire->isFlag(Effect::EFFECT_CLASS_TRANSFORM_TO_WOLF) ||
                         pVampire->isFlag(Effect::EFFECT_CLASS_HAS_FLAG) ||
                         pVampire->isFlag(Effect::EFFECT_CLASS_HAS_SWEEPER)
                         // Werewolf form blocks the transformation as well.
                         || pVampire->isFlag(Effect::EFFECT_CLASS_TRANSFORM_TO_WERWOLF);
        // end 2006-12-29

        if (bManaCheck && bTimeCheck && bRangeCheck && bHitRoll && bMoveModeCheck && !bEffected) {
            decreaseMana(pVampire, RequiredMP, _GCSkillToInventoryOK1);

            SkillInput input(pVampire);
            SkillOutput output;
            computeOutput(input, output);

            // Create the effect class and attach it.
            EffectTransformToWolf* pEffectTTW = new EffectTransformToWolf(pVampire);
            pEffectTTW->setDeadline(999999999);
            pVampire->addEffect(pEffectTTW);
            pVampire->setFlag(Effect::EFFECT_CLASS_TRANSFORM_TO_WOLF);

            // Recompute the stats that change as a result.
            VAMPIRE_RECORD prev;
            pVampire->getVampireRecord(prev);
            pVampire->initAllStat();
            pVampire->addModifyInfo(prev, _GCSkillToInventoryOK1);

            _GCSkillToInventoryOK1.setSkillType(SkillType);
            _GCSkillToInventoryOK1.setCEffectID(0);
            _GCSkillToInventoryOK1.setDuration(0);

            pPlayer->sendPacket(&_GCSkillToInventoryOK1);

            // Tells clients to add a werewolf in place of the Vampire.
            GCAddWolf gcAddWolf;
            gcAddWolf.setObjectID(pVampire->getObjectID());
            gcAddWolf.setName(pVampire->getName());
            gcAddWolf.setXYDir(x, y, pVampire->getDir());
            gcAddWolf.setItemType(pItem->getItemType());
            gcAddWolf.setCurrentHP(pVampire->getHP());
            gcAddWolf.setMaxHP(pVampire->getHP(ATTR_MAX));
            gcAddWolf.setGuildID(pVampire->getGuildID());
            pZone->broadcastPacket(x, y, &gcAddWolf, pVampire);

            decreaseItemNum(pItem, pInventory, pVampire->getName(), STORAGE_INVENTORY, 0, X, Y);

            if (pVampire->getPetInfo() != NULL) {
                pVampire->setPetInfo(NULL);
                sendPetInfo(dynamic_cast<GamePlayer*>(pVampire->getPlayer()), true);
            }

            pSkillSlot->setRunTime(output.Delay);
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
void TransformToWolf::execute(Monster* pMonster)

{
    Assert(pMonster != NULL);

    try {
        Zone* pZone = pMonster->getZone();
        Assert(pZone != NULL);

        SkillType_t SkillType = SKILL_TRANSFORM_TO_WOLF;
        SkillInfo* pSkillInfo = de::gameContext().skillInfos().getSkillInfo(SkillType);

        ZoneCoord_t x = pMonster->getX();
        ZoneCoord_t y = pMonster->getY();

        bool bRangeCheck = checkZoneLevelToUseSkill(pMonster);
        bool bHitRoll = HitRoll::isSuccessMagic(pMonster, pSkillInfo);
        bool bMoveModeCheck = pMonster->isWalking();
        bool bEffected = pMonster->isFlag(Effect::EFFECT_CLASS_TRANSFORM_TO_WOLF);

        if (bRangeCheck && bHitRoll && bMoveModeCheck && !bEffected) {
            SkillInput input(pMonster);
            SkillOutput output;
            computeOutput(input, output);

            // Create the effect class and attach it.
            EffectTransformToWolf* pEffectTTW = new EffectTransformToWolf(pMonster);
            pEffectTTW->setDeadline(999999999);
            pMonster->addEffect(pEffectTTW);
            pMonster->setFlag(Effect::EFFECT_CLASS_TRANSFORM_TO_WOLF);

            // Recompute the stats that change as a result.
            pMonster->initAllStat();

            // Tells clients to add a werewolf in place of the Vampire.
            GCAddWolf gcAddWolf;
            gcAddWolf.setObjectID(pMonster->getObjectID());
            gcAddWolf.setName(pMonster->getName());
            gcAddWolf.setXYDir(x, y, pMonster->getDir());
            gcAddWolf.setItemType(0);
            gcAddWolf.setCurrentHP(pMonster->getHP());
            gcAddWolf.setMaxHP(pMonster->getHP(ATTR_MAX));
            gcAddWolf.setGuildID(1);
            pZone->broadcastPacket(x, y, &gcAddWolf, pMonster);
        } else {
            executeSkillFailNormal(pMonster, getSkillType(), NULL);
        }
    } catch (Throwable& t) {
        executeSkillFailException(pMonster, getSkillType());
    }
}

TransformToWolf g_TransformToWolf;
