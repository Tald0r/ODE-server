////////////////////////////////////////////////////////////////////////////////
// Filename    : ActionRedeemMotorcycle.cpp
// Written By  :
// Description :
////////////////////////////////////////////////////////////////////////////////

#include "ActionRedeemMotorcycle.h"

#include "Belt.h"
#include "Creature.h"
#include "GCNPCResponse.h"
#include "GameContext.h"
#include "GamePlayer.h"
#include "Item.h"
#include "ItemFactoryManager.h"
#include "ItemUtil.h"
#include "ParkingCenter.h"
#include "Slayer.h"
#include "Vampire.h"
#include "Zone.h"
#include "item/Key.h"
#include "repository/ItemObjectRepository.h"

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
void ActionRedeemMotorcycle::read(PropertyBuffer& propertyBuffer)

{
    __BEGIN_TRY
    __END_CATCH
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
void ActionRedeemMotorcycle::execute(Creature* pCreature1, Creature* pCreature2)

{
    __BEGIN_TRY

    Assert(pCreature1 != NULL);
    Assert(pCreature2 != NULL);
    Assert(pCreature1->isNPC());
    Assert(pCreature2->isPC());

    Player* pPlayer = pCreature2->getPlayer();
    Assert(pPlayer != NULL);

    // Send an OK packet to the client first.
    GCNPCResponse answerOKpkt;
    pPlayer->sendPacket(&answerOKpkt);

    // Check whether the player is a Slayer.
    if (pCreature2->isSlayer()) {
        Slayer* pSlayer = dynamic_cast<Slayer*>(pCreature2);
        Zone* pZone = pSlayer->getZone();
        Inventory* pInventory = pSlayer->getInventory();
        uint InvenWidth = pInventory->getWidth();
        uint InvenHeight = pInventory->getHeight();
        Item* pItem = NULL;

        Inventory* pBeltInventory = NULL;
        uint BeltInvenWidth = 0;
        uint BeltInvenHeight = 0;
        Item* pBelt = NULL;

        pBelt = pSlayer->getWearItem(Slayer::WEAR_BELT);
        if (pBelt != NULL) {
            pBeltInventory = ((Belt*)pBelt)->getInventory();

            BeltInvenWidth = pBeltInventory->getWidth();
            BeltInvenHeight = pBeltInventory->getHeight();
        }

        // Search the inventory.
        for (uint y = 0; y < InvenHeight; y++) {
            for (uint x = 0; x < InvenWidth; x++) {
                // If there is an item at x, y...
                if (pInventory->hasItem(x, y)) {
                    pItem = pInventory->getItem(x, y);
                    if (load(pItem, pSlayer, pZone, pSlayer->getX(), pSlayer->getY())) {
                        return;
                    }
                }
            }
        }

        if (pBelt != NULL) {
            // Search the belt.
            for (uint y = 0; y < BeltInvenHeight; y++) {
                for (uint x = 0; x < BeltInvenWidth; x++) {
                    if (pBeltInventory->hasItem(x, y)) {
                        pItem = pBeltInventory->getItem(x, y);
                        if (load(pItem, pSlayer, pZone, pSlayer->getX(), pSlayer->getY())) {
                            return;
                        }
                    }
                }
            }
        }
    } else // A Vampire has no motorcycle to redeem.
    {
    }

    __END_CATCH
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
bool ActionRedeemMotorcycle::load(Item* pItem, Slayer* pSlayer, Zone* pZone, ZoneCoord_t x, ZoneCoord_t y) const

{
    bool bFound = false;

    __BEGIN_TRY

    // Return false if the item is not a key.
    if (pItem->getItemClass() != Item::ITEM_CLASS_KEY)
        return false;

    Key* pKey = dynamic_cast<Key*>(pItem);
    ItemID_t targetID = pKey->getTarget();

    try {
        // If it is a key, get the item ID of the item the key targets.
        // A targetID of 0 means the motorcycle object's ItemID was never set.
        // In that case the targetID is set to the key's own ItemID.
        // Because the targetID is used as the motorcycle's itemID,
        // broadcasting and the like crashed on an Assert().
        // by sigi. 2002.12.25 x-mas T_T;
        if (targetID == 0) {
            targetID = pKey->setNewMotorcycle(pSlayer);
        } else {
            // The motorcycle linked to the key can be deleted behind its back.
            // Check that it still exists in the database, and create a new one if not.
            if (!defaultItemObjectRepository().motorcycleExists(targetID)) {
                Key* pKey = dynamic_cast<Key*>(pItem);
                Assert(pKey != NULL);

                targetID = pKey->setNewMotorcycle(pSlayer);
            }
        }


        // Defensive guard.
        if (targetID == 0) {
            diagnosticFilelog("errorLog.txt", "[ActionRedeemMotorcycle] itemID=%d, motorItemID=%d",
                              (int)pItem->getItemID(), (int)targetID);
            return false;
        }

        // Check that the object has not been created already before querying the database.
        if (context().parking().hasMotorcycleBox(targetID)) {
            // A box for this motorcycle already exists, so it must not be
            // created a second time.

            return false;
        }

        ItemID_t itemID;
        ItemType_t itemType;
        string optionType;
        Durability_t durability;

        {
            MotorcycleRedeemRow redeemRow;
            bool bRowFound = defaultItemObjectRepository().loadMotorcycleForRedeem(REDEEM_SPELLING_QUEST_ACTION,
                                                                                   targetID, redeemRow);

            // by sigi. 2002.10.14
            // No row means there is no motorcycle.
            if (!bRowFound) {
                bFound = false;

                itemID = targetID;
                itemType = 0;
                optionType = "";
                durability = 300;
            } else {
                bFound = true;

                itemID = redeemRow.itemID;
                itemType = redeemRow.itemType;
                optionType = redeemRow.optionField;
                durability = redeemRow.durability;
            }


            // Create the motorcycle object.
            list<OptionType_t> optionTypes;
            setOptionTypeFromField(optionTypes, optionType);
            Motorcycle* pMotorcycle = new Motorcycle(itemType, optionTypes);

            Assert(pMotorcycle != NULL);

            pMotorcycle->setItemID(itemID);
            pMotorcycle->setDurability(durability);

            // Assign a new object ID before attaching it to the zone.
            (pZone->getObjectRegistry()).registerObject(pMotorcycle);

            // Attach the created motorcycle to the zone.
            TPOINT pt = pZone->addItem(pMotorcycle, x, y, false);
            if (pt.x == -1) {
                // The motorcycle could not be added to the zone.
                filelog(
                    "motorError.txt",
                    "ActionRedeemMotorcycle::load() : cannot add the motorcycle to the zone. zoneID=%d, xy=(%d, %d)",
                    (int)pZone->getZoneID(), (int)x, (int)y);
                throw Error("ActionRedeemMotorcycle::load() : cannot add the motorcycle to the zone");
            }

            // by sigi. 2002.10.14
            if (!bFound) {
                defaultItemObjectRepository().insertRedeemedMotorcycle(
                    REDEEM_SPELLING_QUEST_ACTION, itemID, pMotorcycle->getObjectID(), itemType, STORAGE_ZONE,
                    pZone->getZoneID(), pt.x, pt.y, durability);
            }

            // Register the motorcycle with the parking center.
            MotorcycleBox* pBox = new MotorcycleBox(pMotorcycle, pZone, pt.x, pt.y);
            Assert(pBox != NULL);

            try {
                context().parking().addMotorcycleBox(pBox);
            } catch (DuplicatedException& de) { // by sigi. 2002.12.24
                filelog("motorError.txt", "%s - itemID=%d, motorid=%d", de.toString().c_str(), itemID,
                        pMotorcycle->getObjectID());
            }

            bFound = true;
            //}
        }

    } catch (Throwable& t) { // by sigi. 2002.12.25
        filelog("motorError.txt", "%s - itemID=%d, motorItemID=%d", t.toString().c_str(), (int)pItem->getItemID(),
                (int)targetID);
        // Swallowed so the failure does not bring the server down.
    }

    __END_CATCH

    return bFound;
}

////////////////////////////////////////////////////////////////////////////////
// get debug string
////////////////////////////////////////////////////////////////////////////////
string ActionRedeemMotorcycle::toString() const

{
    __BEGIN_TRY

    StringStream msg;
    msg << "ActionRedeemMotorcycle(" << ")";
    return msg.toString();

    __END_CATCH
}
