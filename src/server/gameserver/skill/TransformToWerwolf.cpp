//////////////////////////////////////////////////////////////////////////////
// Filename    : TransformToWerwolf.cpp
// Written by  : elca@ewestsoft.com
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "TransformToWerwolf.h"

#include "EffectTransformToWerwolf.h"
#include "GCAddWolf.h"
#include "GCDeleteObject.h"
#include "GCSkillToInventoryOK1.h"
#include "GCSkillToSelfOK1.h"
#include "GCSkillToSelfOK3.h"
#include "GameContext.h"
#include "ItemUtil.h"
#include "PKZoneInfoManager.h"
#include "PacketUtil.h"
#include "RankBonus.h"

//////////////////////////////////////////////////////////////////////////////
// Vampire inventory handler
//////////////////////////////////////////////////////////////////////////////
void TransformToWerwolf::execute(Vampire* pVampire, ObjectID_t InvenObjectID, CoordInven_t X, CoordInven_t Y,
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
        if (pItem->getItemClass() != Item::ITEM_CLASS_SKULL || pItem->getItemType() != 39 || pVampire->hasRelicItem() ||
            de::gameContext().pkZoneInfos().isPKZone(pZone->getZoneID()) ||
            pVampire->isFlag(Effect::EFFECT_CLASS_REFINIUM_TICKET)) {
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
        bool bEffected = pVampire->isFlag(Effect::EFFECT_CLASS_TRANSFORM_TO_WERWOLF) ||
                         pVampire->isFlag(Effect::EFFECT_CLASS_HAS_FLAG) ||
                         pVampire->isFlag(Effect::EFFECT_CLASS_HAS_SWEEPER);

        if (bManaCheck && bTimeCheck && bRangeCheck && bHitRoll && bMoveModeCheck && !bEffected) {
            decreaseMana(pVampire, RequiredMP, _GCSkillToInventoryOK1);

            SkillInput input(pVampire);
            SkillOutput output;
            computeOutput(input, output);

            // Create the effect class and attach it.
            EffectTransformToWerwolf* pEffectTTW = new EffectTransformToWerwolf(pVampire);
            pEffectTTW->setDeadline(999999999);
            pVampire->addEffect(pEffectTTW);
            pVampire->setFlag(Effect::EFFECT_CLASS_TRANSFORM_TO_WERWOLF);

            // Recompute the stats that change as a result.
            VAMPIRE_RECORD prev;
            pVampire->getVampireRecord(prev);
            pVampire->initAllStat();
            pVampire->addModifyInfo(prev, _GCSkillToInventoryOK1);

            _GCSkillToInventoryOK1.setSkillType(SkillType);
            _GCSkillToInventoryOK1.setCEffectID(0);
            _GCSkillToInventoryOK1.setDuration(0);

            pPlayer->sendPacket(&_GCSkillToInventoryOK1);

            DWORD dummyFlag;
            Color_t color[PCVampireInfo::VAMPIRE_COLOR_MAX];
            pVampire->getShapeInfo(dummyFlag, color);

            // Tells clients to add a werewolf in place of the Vampire.
            GCAddWolf gcAddWerwolf;
            gcAddWerwolf.setObjectID(pVampire->getObjectID());
            gcAddWerwolf.setName(pVampire->getName());
            gcAddWerwolf.setXYDir(x, y, pVampire->getDir());
            gcAddWerwolf.setMainColor(color[PCVampireInfo::VAMPIRE_COLOR_COAT]);
            gcAddWerwolf.setItemType(pItem->getItemType());
            gcAddWerwolf.setCurrentHP(pVampire->getHP());
            gcAddWerwolf.setMaxHP(pVampire->getHP(ATTR_MAX));
            gcAddWerwolf.setGuildID(pVampire->getGuildID());
            pZone->broadcastPacket(x, y, &gcAddWerwolf, pVampire);


            decreaseItemNum(pItem, pInventory, pVampire->getName(), STORAGE_INVENTORY, 0, X, Y);

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
void TransformToWerwolf::execute(Monster* pMonster)

{
    Assert(pMonster != NULL);

    try {
        Zone* pZone = pMonster->getZone();
        Assert(pZone != NULL);

        SkillType_t SkillType = SKILL_TRANSFORM_TO_WERWOLF;
        SkillInfo* pSkillInfo = de::gameContext().skillInfos().getSkillInfo(SkillType);

        ZoneCoord_t x = pMonster->getX();
        ZoneCoord_t y = pMonster->getY();

        bool bRangeCheck = checkZoneLevelToUseSkill(pMonster);
        bool bHitRoll = HitRoll::isSuccessMagic(pMonster, pSkillInfo);
        bool bMoveModeCheck = pMonster->isWalking();
        bool bEffected = pMonster->isFlag(Effect::EFFECT_CLASS_TRANSFORM_TO_WERWOLF);

        if (bRangeCheck && bHitRoll && bMoveModeCheck && !bEffected) {
            SkillInput input(pMonster);
            SkillOutput output;
            computeOutput(input, output);

            // Create the effect class and attach it.
            EffectTransformToWerwolf* pEffectTTW = new EffectTransformToWerwolf(pMonster);
            pEffectTTW->setDeadline(999999999);
            pMonster->addEffect(pEffectTTW);
            pMonster->setFlag(Effect::EFFECT_CLASS_TRANSFORM_TO_WERWOLF);

            // Recompute the stats that change as a result.
            pMonster->initAllStat();

            // Tells clients to add a werewolf in place of the Vampire.
            GCAddWolf gcAddWerwolf;
            gcAddWerwolf.setObjectID(pMonster->getObjectID());
            gcAddWerwolf.setName(pMonster->getName());
            gcAddWerwolf.setXYDir(x, y, pMonster->getDir());
            gcAddWerwolf.setItemType(3);
            gcAddWerwolf.setCurrentHP(pMonster->getHP());
            gcAddWerwolf.setMaxHP(pMonster->getHP(ATTR_MAX));
            gcAddWerwolf.setGuildID(1);
            pZone->broadcastPacket(x, y, &gcAddWerwolf, pMonster);
        } else {
            executeSkillFailNormal(pMonster, getSkillType(), NULL);
        }
    } catch (Throwable& t) {
        executeSkillFailException(pMonster, getSkillType());
    }
}

TransformToWerwolf g_TransformToWerwolf;
