//////////////////////////////////////////////////////////////////////////////
// Filename    : BloodyMarker.cpp
// Written by  : excel96
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "BloodyMarker.h"

#include "GCDeleteObject.h"
#include "GCSkillToInventoryOK1.h"
#include "GCSkillToSelfOK1.h"
#include "GCSkillToSelfOK3.h"
#include "GameContext.h"
#include "ZoneUtil.h"
#include "item/VampirePortalItem.h"

//////////////////////////////////////////////////////////////////////////////
// Vampire inventory handler
//////////////////////////////////////////////////////////////////////////////
void BloodyMarker::execute(Vampire* pVampire, ObjectID_t InvenObjectID, CoordInven_t X, CoordInven_t Y,
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

        // BloodyMark cannot be used in a war zone.
        // The check goes by ZoneID for now; it should move into ZoneInfo.
        ///*

        // Block the event arena and the OX quiz zone.
        if (pZone->isNoPortalZone() ||
            pZone->isMasterLair()
            // Block castles too.
            || pZone->isCastle() || pZone->isHolyLand()) {
            executeSkillFailException(pVampire, getSkillType());
            return;
        }
        //*/

        Item* pItem = pInventory->getItem(X, Y);
        // Fails if the item is NULL, is not a portal item, or has the wrong object ID.
        if (pItem == NULL || pItem->getItemClass() != Item::ITEM_CLASS_VAMPIRE_PORTAL_ITEM ||
            pItem->getObjectID() != InvenObjectID) {
            executeSkillFailException(pVampire, getSkillType());
            return;
        }

        VampirePortalItem* pVampirePortalItem = dynamic_cast<VampirePortalItem*>(pItem);
        Assert(pVampirePortalItem != NULL);

        // Fails too if the Vampire portal item already records a position.
        if (pVampirePortalItem->getZoneID() != 0 || pVampirePortalItem->getX() != 0 ||
            pVampirePortalItem->getY() != 0) {
            executeSkillFailException(pVampire, getSkillType());
            return;
        }

        GCSkillToInventoryOK1 _GCSkillToInventoryOK1;

        SkillType_t SkillType = pSkillSlot->getSkillType();
        SkillInfo* pSkillInfo = de::gameContext().skillInfos().getSkillInfo(SkillType);

        int RequiredMP = decreaseConsumeMP(pVampire, pSkillInfo);
        bool bManaCheck = hasEnoughMana(pVampire, RequiredMP);
        bool bTimeCheck = verifyRunTime(pSkillSlot);
        bool bRangeCheck = checkZoneLevelToUseSkill(pVampire);

        if (bManaCheck && bTimeCheck && bRangeCheck) {
            // Consume mana.
            decreaseMana(pVampire, RequiredMP, _GCSkillToInventoryOK1);

            SkillInput input(pVampire);
            SkillOutput output;
            computeOutput(input, output);

            // Record the current position on the item and save it.
            pVampirePortalItem->setZoneID(pZone->getZoneID());
            pVampirePortalItem->setX(pVampire->getX());
            pVampirePortalItem->setY(pVampire->getY());
            pVampirePortalItem->save(pVampire->getName(), STORAGE_INVENTORY, 0, X, Y);

            _GCSkillToInventoryOK1.setSkillType(SkillType);
            _GCSkillToInventoryOK1.setObjectID(InvenObjectID);
            _GCSkillToInventoryOK1.setCEffectID(0);
            _GCSkillToInventoryOK1.setDuration(0);

            pPlayer->sendPacket(&_GCSkillToInventoryOK1);

            pSkillSlot->setRunTime(output.Delay);
        } else {
            executeSkillFailNormal(pVampire, getSkillType(), NULL);
        }
    } catch (Throwable& t) {
        executeSkillFailException(pVampire, getSkillType());
    }
}

BloodyMarker g_BloodyMarker;
