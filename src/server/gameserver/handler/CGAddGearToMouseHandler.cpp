//////////////////////////////////////////////////////////////////////////////
// Filename    : CGAddGearToMouseHandler.cc
// Written By  : elca@ewestsoft.com
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "CGAddGearToMouse.h"

#ifdef __GAME_SERVER__
#include "GCCannotAdd.h"
#include "GamePlayer.h"
#include "Inventory.h"
#include "Item.h"
#include "ItemInfo.h"
#include "ItemInfoManager.h"
#include "Ousters.h"
#include "Slayer.h"
#include "Vampire.h"
#include "Zone.h"
#endif

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void CGAddGearToMouseHandler::execute(CGAddGearToMouse* pPacket, Player* pPlayer)

{
    __BEGIN_DEBUG_EX

#ifdef __GAME_SERVER__

    Assert(pPacket != NULL);
    Assert(pPlayer != NULL);

    try {
        GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(pPlayer);
        Creature* pCreature = pGamePlayer->getCreature();
        bool bSuccess = false;
        SlotID_t SlotID = pPacket->getSlotID();

        Assert(pCreature != NULL);

        if (pCreature->isSlayer()) {
            Slayer* pSlayer = dynamic_cast<Slayer*>(pCreature);
            Assert(pSlayer != NULL);

            // Nothing may be held on the mouse,
            // and the part being uncovered must have an item worn on it.
            if (pSlayer->getExtraInventorySlotItem() == NULL && pSlayer->isWear((Slayer::WearPart)SlotID)) {
                // Get the Item that is worn.
                Item* pItem = pSlayer->getWearItem((Slayer::WearPart)SlotID);

                // Check that the item exists and that its ObjectID matches.
                if (pItem != NULL && pItem->getObjectID() == pPacket->getObjectID()) {
                    // Erase the item from the gear window and move it to the Mouse.
                    // Take the gear off, lowering the stats, and move the removed item to the Mouse.
                    pSlayer->takeOffItem((Slayer::WearPart)SlotID, true, true);
                    bSuccess = true;
                }
            }
        } else if (pCreature->isVampire()) {
            Vampire* pVampire = dynamic_cast<Vampire*>(pCreature);

            Assert(pVampire != NULL);

            // Nothing may be held on the mouse,
            // and the part being uncovered must have an item worn on it.
            if (pVampire->getExtraInventorySlotItem() == NULL && pVampire->isWear((Vampire::WearPart)SlotID)) {
                // Get the Item that is worn.
                Item* pItem = pVampire->getWearItem((Vampire::WearPart)SlotID);

                // Check that the item exists and that its ObjectID matches.
                if (pItem != NULL && pItem->getObjectID() == pPacket->getObjectID()) {
                    // Erase the item from the gear window and move it to the Mouse.
                    // Take the gear off, lowering the stats, and move the removed item to the Mouse.
                    pVampire->takeOffItem((Vampire::WearPart)SlotID, true, true);
                    bSuccess = true;
                }
            }
        } else if (pCreature->isOusters()) {
            Ousters* pOusters = dynamic_cast<Ousters*>(pCreature);

            Assert(pOusters != NULL);

            // Nothing may be held on the mouse,
            // and the part being uncovered must have an item worn on it.
            if (pOusters->getExtraInventorySlotItem() == NULL && pOusters->isWear((Ousters::WearPart)SlotID)) {
                // Get the Item that is worn.
                Item* pItem = pOusters->getWearItem((Ousters::WearPart)SlotID);

                // Check that the item exists and that its ObjectID matches.
                if (pItem != NULL && pItem->getObjectID() == pPacket->getObjectID()) {
                    // Erase the item from the gear window and move it to the Mouse.
                    // Take the gear off, lowering the stats, and move the removed item to the Mouse.
                    pOusters->takeOffItem((Ousters::WearPart)SlotID, true, true);
                    bSuccess = true;
                }
            }
        }

        if (!bSuccess) {
            GCCannotAdd _GCCannotAdd;
            _GCCannotAdd.setObjectID(pPacket->getObjectID());
            pPlayer->sendPacket(&_GCCannotAdd);
        }
    } catch (Throwable& t) {
    }

#endif // __GAME_SERVER__

    __END_DEBUG_EX
}
