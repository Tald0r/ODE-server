//////////////////////////////////////////////////////////////////////////////
// Filename    : InstallMine.cpp
// Written by  : Elca
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "InstallMine.h"

#include "Assert.h"
#include "GCSkillToInventoryOK1.h"
#include "GameContext.h"
#include "ItemUtil.h"
#include "SkillInfo.h"
#include "item/Mine.h"
// #include "GCSkillToTileOK1.h"
// #include "GCSkillToTileOK5.h"
#include "GCDeleteEffectFromTile.h"
#include "GCDeleteObject.h"
#include "GCSkillFailed1.h"
#include "GCSkillFailed2.h"
#include "ItemInfoManager.h"

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void InstallMine::execute(Slayer* pSlayer, ObjectID_t, CoordInven_t X, CoordInven_t Y, CoordInven_t TargetX,
                          CoordInven_t TargetY, SkillSlot* pSkillSlot)

{
    Assert(pSlayer != NULL);
    Assert(pSkillSlot != NULL);

    try {
        Player* pPlayer = pSlayer->getPlayer();
        Zone* pZone = pSlayer->getZone();

        Assert(pPlayer != NULL);
        Assert(pZone != NULL);

        GCSkillToInventoryOK1 _GCSkillToInventoryOK1;

        SkillType_t SkillType = pSkillSlot->getSkillType();
        SkillInfo* pSkillInfo = de::gameContext().skillInfos().getSkillInfo(SkillType);
        SkillLevel_t SkillLevel = pSkillSlot->getExpLevel();

        int RequiredMP = (int)pSkillInfo->getConsumeMP();
        bool bManaCheck = hasEnoughMana(pSlayer, RequiredMP);
        bool bTimeCheck = verifyRunTime(pSkillSlot);
        bool bRangeCheck = checkZoneLevelToUseSkill(pSlayer);
        ZoneCoord_t slayerX = pSlayer->getX(), slayerY = pSlayer->getY();
        bool bInstallAction = false;

        Mine* pMine = NULL;

        Inventory* pInventory = pSlayer->getInventory();
        Assert(pInventory != NULL);

        if (bManaCheck && bTimeCheck && bRangeCheck) {
            // Finds the mine.
            Item* pItem = pInventory->getItem(X, Y);
            if (pItem != NULL && pItem->getItemClass() == Item::ITEM_CLASS_MINE) {
                bInstallAction = true;
                pMine = dynamic_cast<Mine*>(pItem);
            }
        }


        // Decides whether the skill succeeds.
        if (bInstallAction) {
            GCSkillToInventoryOK1 _GCSkillToInventoryOK1;

            ItemInfo* pItemInfo =
                de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_MINE, pMine->getItemType());

            Damage_t MinDamage = pItemInfo->getMinDamage();
            Damage_t MaxDamage = pItemInfo->getMaxDamage();

            Damage_t RealDamage = MinDamage + (max(0, ((int)MaxDamage * (int)SkillLevel / 100) - MinDamage));

            Mine* pInstallMine = new Mine();
            ObjectRegistry& OR = pZone->getObjectRegistry();
            OR.registerObject(pInstallMine);

            Assert(pInstallMine != NULL);
            pInstallMine->setItemType(pMine->getItemType());
            pInstallMine->setDir(TargetX);
            pInstallMine->setDamage(RealDamage);
            pInstallMine->setInstallerName(pSlayer->getName());
            pInstallMine->setInstallerPartyID(pSlayer->getPartyID());
            pInstallMine->setFlag(Effect::EFFECT_CLASS_INSTALL);

            // Items disappear after three minutes, which took the mine with them, so
            // the mine's lifetime is fixed at ten minutes.
            TPOINT pt = pZone->addItem(pInstallMine, slayerX, slayerY, true, 6000);

            // EXP up
            Exp_t Point = pSkillInfo->getPoint();

            shareAttrExp(pSlayer, 100, 1, 8, 1, _GCSkillToInventoryOK1);
            increaseDomainExp(pSlayer, SKILL_DOMAIN_GUN, Point, _GCSkillToInventoryOK1);
            increaseSkillExp(pSlayer, SKILL_DOMAIN_GUN, pSkillSlot, pSkillInfo, _GCSkillToInventoryOK1);

            decreaseMana(pSlayer, RequiredMP, _GCSkillToInventoryOK1);
            decreaseItemNum(pMine, pInventory, pSlayer->getName(), STORAGE_INVENTORY, 0, X, Y);


            _GCSkillToInventoryOK1.setObjectID(pInstallMine->getObjectID());
            _GCSkillToInventoryOK1.setSkillType(SkillType);
            _GCSkillToInventoryOK1.setCEffectID(0);
            _GCSkillToInventoryOK1.setX(X);
            _GCSkillToInventoryOK1.setY(Y);
            _GCSkillToInventoryOK1.setDuration(0);


            pPlayer->sendPacket(&_GCSkillToInventoryOK1);

            // Deletes the mine for those who can no longer see it.
            addInstalledMine(pZone, pInstallMine, pt.x, pt.y);


            // Set NextTime
            pSkillSlot->setRunTime();

        } else {
            GCSkillFailed1 _GCSkillFailed1;
            GCSkillFailed2 _GCSkillFailed2;

            executeSkillFailException(pSlayer, getSkillType());
        }

    } catch (Throwable& t) {
        executeSkillFailException(pSlayer, getSkillType());
    }
}


InstallMine g_InstallMine;
