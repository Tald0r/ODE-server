//////////////////////////////////////////////////////////////////////////////
// Filename    : CGAddQuickSlotToMouseHandler.cc
// Written By  : elca@ewestsoft.com
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "CGAddQuickSlotToMouse.h"

#ifdef __GAME_SERVER__
#include <stdio.h>

#include "GCCannotAdd.h"
#include "GamePlayer.h"
#include "Inventory.h"
#include "Item.h"
#include "Ousters.h"
#include "Slayer.h"
#include "Zone.h"
#include "item/Belt.h"
#include "item/OustersArmsband.h"
#endif

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void CGAddQuickSlotToMouseHandler::execute(CGAddQuickSlotToMouse* pPacket, Player* pPlayer)

{
    __BEGIN_TRY __BEGIN_DEBUG_EX

#ifdef __GAME_SERVER__

        Assert(pPacket != NULL);
    Assert(pPlayer != NULL);

    GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(pPlayer);
    Creature* pCreature = pGamePlayer->getCreature();

    try {
        if (pCreature->isSlayer()) {
            Slayer* pSlayer = dynamic_cast<Slayer*>(pCreature);
            SlotID_t SlotID = pPacket->getSlotID();

            if (!pSlayer->isWear(Slayer::WEAR_BELT)) {
                GCCannotAdd _GCCannotAdd;
                _GCCannotAdd.setObjectID(pPacket->getObjectID());
                pPlayer->sendPacket(&_GCCannotAdd);
                return;
            }

            Item* pBeltItem = pSlayer->getWearItem(Slayer::WEAR_BELT);
            Inventory* pBeltInventory = ((Belt*)pBeltItem)->getInventory();

            if (SlotID >= pBeltInventory->getWidth()) {
                GCCannotAdd _GCCannotAdd;
                _GCCannotAdd.setObjectID(pPacket->getObjectID());
                pPlayer->sendPacket(&_GCCannotAdd);
                return;
            }

            Item* pSlotItem = pBeltInventory->getItem(SlotID, 0);

            // With no item in the belt, or something held on the mouse, nothing can be added.
            if (pSlotItem == NULL || pSlayer->getExtraInventorySlotItem() != NULL) {
                GCCannotAdd _GCCannotAdd;
                _GCCannotAdd.setObjectID(pPacket->getObjectID());
                pPlayer->sendPacket(&_GCCannotAdd);
                return;
            }

            // Erase the item from the belt inventory and move it to the Mouse.
            pBeltInventory->deleteItem(SlotID, 0);
            pSlayer->addItemToExtraInventorySlot(pSlotItem);
            char pField[80];
            snprintf(pField, sizeof(pField), "Storage=%d", STORAGE_EXTRASLOT);
            pSlotItem->tinysave(pField);
        }

        if (pCreature->isOusters()) {
            Ousters* pOusters = dynamic_cast<Ousters*>(pCreature);
            SlotID_t SlotID = pPacket->getSlotID();

            Ousters::WearPart part = (SlotID > 2 ? Ousters::WEAR_ARMSBAND2 : Ousters::WEAR_ARMSBAND1);
            if (SlotID > 2)
                SlotID -= 3;

            if (!pOusters->isWear(part)) {
                GCCannotAdd _GCCannotAdd;
                _GCCannotAdd.setObjectID(pPacket->getObjectID());
                pPlayer->sendPacket(&_GCCannotAdd);
                return;
            }

            Item* pOustersArmsbandItem = pOusters->getWearItem(part);
            Inventory* pOustersArmsbandInventory = ((OustersArmsband*)pOustersArmsbandItem)->getInventory();

            if (SlotID >= pOustersArmsbandInventory->getWidth()) {
                GCCannotAdd _GCCannotAdd;
                _GCCannotAdd.setObjectID(pPacket->getObjectID());
                pPlayer->sendPacket(&_GCCannotAdd);
                return;
            }

            Item* pSlotItem = pOustersArmsbandInventory->getItem(SlotID, 0);

            // With no item in the belt, or something held on the mouse, nothing can be added.
            if (pSlotItem == NULL || pOusters->getExtraInventorySlotItem() != NULL) {
                GCCannotAdd _GCCannotAdd;
                _GCCannotAdd.setObjectID(pPacket->getObjectID());
                pPlayer->sendPacket(&_GCCannotAdd);
                return;
            }

            // Erase the item from the belt inventory and move it to the Mouse.
            pOustersArmsbandInventory->deleteItem(SlotID, 0);
            pOusters->addItemToExtraInventorySlot(pSlotItem);
            char pField[80];
            snprintf(pField, sizeof(pField), "Storage=%d", STORAGE_EXTRASLOT);
            pSlotItem->tinysave(pField);
        }
    } catch (Throwable& t) {
    }

#endif // __GAME_SERVER__

    __END_DEBUG_EX __END_CATCH
}
