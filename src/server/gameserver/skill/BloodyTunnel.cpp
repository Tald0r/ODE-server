//////////////////////////////////////////////////////////////////////////////
// Filename    : BloodyTunnel.cpp
// Written by  : elca@ewestsoft.com
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "BloodyTunnel.h"

#include "EffectSchedule.h"
#include "EffectVampirePortal.h"
#include "GCSkillToInventoryOK1.h"
#include "GCSkillToSelfOK1.h"
#include "GCSkillToSelfOK3.h"
#include "GameContext.h"
#include "ZoneUtil.h"
#include "item/VampirePortalItem.h"

//////////////////////////////////////////////////////////////////////////////
// Vampire inventory handler
//////////////////////////////////////////////////////////////////////////////
void BloodyTunnel::execute(Vampire* pVampire, ObjectID_t InvenObjectID, CoordInven_t X, CoordInven_t Y,
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

        // BloodyTunnel cannot be used in a no-portal zone, a master lair,
        // a castle or a holy land.
        if (pZone->isNoPortalZone() || pZone->isMasterLair() || pZone->isCastle() || pZone->isHolyLand()) {
            executeSkillFailException(pVampire, getSkillType());
            return;
        }


        Item* pItem = pInventory->getItem(X, Y);
        // The skill fails if the item is missing, is not a vampire portal item, or the OID does not match.
        if (pItem == NULL || pItem->getItemClass() != Item::ITEM_CLASS_VAMPIRE_PORTAL_ITEM ||
            pItem->getObjectID() != InvenObjectID) {
            executeSkillFailException(pVampire, getSkillType());
            return;
        }

        VampirePortalItem* pVampirePortalItem = dynamic_cast<VampirePortalItem*>(pItem);
        Assert(pVampirePortalItem != NULL);

        // Fails when the vampire portal item has no recorded location.
        ZoneID_t zoneid = pVampirePortalItem->getZoneID();
        ZoneCoord_t tx = pVampirePortalItem->getX();
        ZoneCoord_t ty = pVampirePortalItem->getY();
        if (zoneid == 0) {
            executeSkillFailException(pVampire, getSkillType());
            return;
        }

        // Validate the target zone and coordinates that were read.
        Zone* pTargetZone = getZoneByZoneID(zoneid);

        // Adam's holy land does not connect to other zones.
        if (pZone->isHolyLand() != pTargetZone->isHolyLand()) {
            executeSkillFailException(pVampire, getSkillType());
            return;
        }


        VSRect* pRect = pTargetZone->getOuterRect();

        if (!pRect->ptInRect(tx, ty)) {
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
        bool bHitRoll = true;

        if (bManaCheck && bTimeCheck && bRangeCheck && bHitRoll) {
            decreaseMana(pVampire, RequiredMP, _GCSkillToInventoryOK1);

            SkillInput input(pVampire);
            SkillOutput output;
            computeOutput(input, output);

            // Add a portal to each of the two zones.
            ZONE_COORD s_coord;
            ZONE_COORD t_coord;

            s_coord.id = pZone->getZoneID();
            s_coord.x = pVampire->getX();
            s_coord.y = pVampire->getY();

            t_coord.id = pTargetZone->getZoneID();
            t_coord.x = tx;
            t_coord.y = ty;

            pZone->addVampirePortal(s_coord.x, s_coord.y, pVampire, t_coord);
            pTargetZone->addVampirePortal(t_coord.x, t_coord.y, pVampire, s_coord);

            _GCSkillToInventoryOK1.setSkillType(SkillType);
            _GCSkillToInventoryOK1.setObjectID(InvenObjectID);
            _GCSkillToInventoryOK1.setCEffectID(0);
            _GCSkillToInventoryOK1.setDuration(0);

            pPlayer->sendPacket(&_GCSkillToInventoryOK1);

            // Reduce the charge count.
            pVampirePortalItem->setCharge(pVampirePortalItem->getCharge() - 1);

            if (pVampirePortalItem->getCharge() > 0) {
                // Keep the item while charges remain.
                pVampirePortalItem->save(pVampire->getName(), STORAGE_INVENTORY, 0, X, Y);
            } else {
                // Delete the portal item once its charges are used up.
                pInventory->deleteItem(X, Y);
                pVampirePortalItem->destroy();

                // Once the item pointer itself is deleted and set to NULL,
                // the item pointer inside the effect becomes NULL as well.
                SAFE_DELETE(pVampirePortalItem);
            }

            pSkillSlot->setRunTime(output.Delay);
        } else {
            executeSkillFailNormal(pVampire, getSkillType(), NULL);
        }
    } catch (Throwable& t) {
        executeSkillFailException(pVampire, getSkillType());
    }
}

BloodyTunnel g_BloodyTunnel;
