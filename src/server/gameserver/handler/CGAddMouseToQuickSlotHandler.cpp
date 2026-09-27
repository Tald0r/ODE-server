//////////////////////////////////////////////////////////////////////////////
// Filename    : CGAddMouseToQuickSlotHandler.cc
// Written By  : elca@ewestsoft.com
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "CGAddMouseToQuickSlot.h"

#ifdef __GAME_SERVER__
#include <stdio.h>

#include "GCCannotAdd.h"
#include "GamePlayer.h"
#include "Inventory.h"
#include "Item.h"
#include "ItemUtil.h"
#include "Ousters.h"
#include "Slayer.h"
#include "Zone.h"
#include "item/Belt.h"
#include "item/OustersArmsband.h"
#endif

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void CGAddMouseToQuickSlotHandler::execute(CGAddMouseToQuickSlot* pPacket, Player* pPlayer)

{
    __BEGIN_TRY __BEGIN_DEBUG_EX

#ifdef __GAME_SERVER__

        Assert(pPacket != NULL);
    Assert(pPlayer != NULL);

    GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(pPlayer);
    Creature* pCreature = pGamePlayer->getCreature();
    bool Success = false;

    if (pCreature->isSlayer()) {
        Slayer* pSlayer = dynamic_cast<Slayer*>(pCreature);
        InventorySlot* pExtraSlot = pSlayer->getExtraInventorySlot();
        Item* pItem = pExtraSlot->getItem();

        if (pItem == NULL) {
            GCCannotAdd _GCCannotAdd;
            _GCCannotAdd.setObjectID(pPacket->getObjectID());
            pPlayer->sendPacket(&_GCCannotAdd);
            return;
        }

        ObjectID_t ItemObjectID = pItem->getObjectID();
        SlotID_t SlotID = pPacket->getSlotID();
        Item::ItemClass IClass = pItem->getItemClass();

        // Check that the item's ObjectID matches.
        if (ItemObjectID != pPacket->getObjectID()) {
            GCCannotAdd _GCCannotAdd;
            _GCCannotAdd.setObjectID(pPacket->getObjectID());
            pPlayer->sendPacket(&_GCCannotAdd);
            return;
        }

        // Without a belt worn, no item can be added to the belt.
        if (!pSlayer->isWear(Slayer::WEAR_BELT)) {
            GCCannotAdd _GCCannotAdd;
            _GCCannotAdd.setObjectID(pPacket->getObjectID());
            pPlayer->sendPacket(&_GCCannotAdd);
            return;
        }

        // Anything that is not a potion or a magazine cannot be added.
        if (IClass != Item::ITEM_CLASS_POTION && IClass != Item::ITEM_CLASS_MAGAZINE &&
            IClass != Item::ITEM_CLASS_EVENT_ETC && IClass != Item::ITEM_CLASS_KEY) {
            GCCannotAdd _GCCannotAdd;
            _GCCannotAdd.setObjectID(pPacket->getObjectID());
            pPlayer->sendPacket(&_GCCannotAdd);
            return;
        }
        if (IClass == Item::ITEM_CLASS_EVENT_ETC && pItem->getItemType() < 14) {
            GCCannotAdd _GCCannotAdd;
            _GCCannotAdd.setObjectID(pPacket->getObjectID());
            pPlayer->sendPacket(&_GCCannotAdd);
            return;
        }

        Item* pBelt = pSlayer->getWearItem(Slayer::WEAR_BELT);
        Inventory* pBeltInventory = ((Belt*)pBelt)->getInventory();

        if (pBeltInventory->canAdding(SlotID, 0, pItem)) {
            // Get the Item currently in the belt.
            Item* pPrevItem = pBeltInventory->getItem(SlotID, 0);

            // If there is an item in the given place...
            if (pPrevItem != NULL) {
                // If the item is exactly the same item...
                if (isStackable(pItem) && isSameItem(pItem, pPrevItem)) {
                    int MaxStack = ItemMaxStack[pItem->getItemClass()];

                    if (pItem->getNum() + pPrevItem->getNum() > MaxStack) {
                        ItemNum_t CurrentNum = pPrevItem->getNum();
                        ItemNum_t AddNum = pItem->getNum();
                        ItemNum_t NewNum = AddNum + CurrentNum - MaxStack;

                        pPrevItem->setNum(MaxStack);
                        pItem->setNum(NewNum);
                        pBeltInventory->increaseNum(MaxStack - CurrentNum);
                        pBeltInventory->increaseWeight(pItem->getWeight() * (MaxStack - CurrentNum));
                        char pField[80];
                        snprintf(pField, sizeof(pField), "Num=%d, Storage=%d, StorageID=%u, X=%d", MaxStack,
                                 STORAGE_BELT, pBelt->getItemID(), SlotID);
                        pPrevItem->tinysave(pField);

                        snprintf(pField, sizeof(pField), "Num=%d, Storage=%d", NewNum, STORAGE_EXTRASLOT);
                        pItem->tinysave(pField);


                        Success = true;
                    } else // When the count does not go past the stack limit.
                    {
                        pSlayer->deleteItemFromExtraInventorySlot();
                        pPrevItem->setNum(pPrevItem->getNum() + pItem->getNum());
                        pBeltInventory->increaseNum(pItem->getNum());
                        pBeltInventory->increaseWeight(pItem->getWeight() * pItem->getNum());
                        char pField[80];
                        snprintf(pField, sizeof(pField), "Num=%d, Storage=%d, StorageID=%u, X=%d", pPrevItem->getNum(),
                                 STORAGE_BELT, pBelt->getItemID(), SlotID);
                        pPrevItem->tinysave(pField);

                        pItem->destroy();
                        SAFE_DELETE(pItem);
                        Success = true;
                    }
                } else // When the class and the type are not the same
                {
                    // Remove the item hanging on the mouse and the item in the belt.
                    pSlayer->deleteItemFromExtraInventorySlot();
                    pBeltInventory->deleteItem(pPrevItem->getObjectID());

                    // Swap the two positions.
                    pSlayer->addItemToExtraInventorySlot(pPrevItem);
                    pBeltInventory->addItem(SlotID, 0, pItem);

                    // Save to the DB.
                    char pField[80];
                    snprintf(pField, sizeof(pField), "Storage=%d", STORAGE_EXTRASLOT);
                    pPrevItem->tinysave(pField);

                    snprintf(pField, sizeof(pField), "Storage=%d, StorageID=%u, X=%d", STORAGE_BELT, pBelt->getItemID(),
                             SlotID);
                    pItem->tinysave(pField);


                    Success = true;
                }
            } else // When the slot holds no existing item.
            {
                // Put a given item into the Inventory.
                pBeltInventory->addItem(SlotID, 0, pItem);

                // On a successful add, remove the item hanging on the mouse.
                pSlayer->deleteItemFromExtraInventorySlot();
                char pField[80];
                snprintf(pField, sizeof(pField), "Storage=%d, StorageID=%u, X=%d", STORAGE_BELT, pBelt->getItemID(),
                         SlotID);
                pItem->tinysave(pField);

                Success = true;
            }
        } // end of if (pBeltInventory->canAdding(SlotID, 0,  pItem))
    } // if (pCreature->isSlayer())
    else if (pCreature->isOusters()) {
        Ousters* pOusters = dynamic_cast<Ousters*>(pCreature);
        InventorySlot* pExtraSlot = pOusters->getExtraInventorySlot();
        Item* pItem = pExtraSlot->getItem();

        if (pItem == NULL) {
            GCCannotAdd _GCCannotAdd;
            _GCCannotAdd.setObjectID(pPacket->getObjectID());
            pPlayer->sendPacket(&_GCCannotAdd);
            return;
        }

        ObjectID_t ItemObjectID = pItem->getObjectID();
        SlotID_t SlotID = pPacket->getSlotID();
        Item::ItemClass IClass = pItem->getItemClass();

        // Check that the item's ObjectID matches.
        if (ItemObjectID != pPacket->getObjectID()) {
            GCCannotAdd _GCCannotAdd;
            _GCCannotAdd.setObjectID(pPacket->getObjectID());
            pPlayer->sendPacket(&_GCCannotAdd);
            return;
        }

        Ousters::WearPart part = (SlotID > 2 ? Ousters::WEAR_ARMSBAND2 : Ousters::WEAR_ARMSBAND1);
        if (SlotID > 2)
            SlotID -= 3;

        // The matching armsband is not worn
        if (!pOusters->isWear(part)) {
            GCCannotAdd _GCCannotAdd;
            _GCCannotAdd.setObjectID(pPacket->getObjectID());
            pPlayer->sendPacket(&_GCCannotAdd);
            return;
        }

        // Anything that is not a pupa or a compos mei cannot be added.
        if (IClass != Item::ITEM_CLASS_PUPA && IClass != Item::ITEM_CLASS_COMPOS_MEI &&
            IClass != Item::ITEM_CLASS_EVENT_ETC) {
            GCCannotAdd _GCCannotAdd;
            _GCCannotAdd.setObjectID(pPacket->getObjectID());
            pPlayer->sendPacket(&_GCCannotAdd);
            return;
        }

        if (IClass == Item::ITEM_CLASS_EVENT_ETC && pItem->getItemType() < 14) {
            GCCannotAdd _GCCannotAdd;
            _GCCannotAdd.setObjectID(pPacket->getObjectID());
            pPlayer->sendPacket(&_GCCannotAdd);
            return;
        }

        Item* pArmsband = pOusters->getWearItem(part);
        Inventory* pArmsbandInventory = ((OustersArmsband*)pArmsband)->getInventory();

        if (pArmsbandInventory->canAdding(SlotID, 0, pItem)) {
            // Get the Item currently in the belt.
            Item* pPrevItem = pArmsbandInventory->getItem(SlotID, 0);

            // If there is an item in the given place...
            if (pPrevItem != NULL) {
                // If the item is exactly the same item...
                if (isSameItem(pItem, pPrevItem)) {
                    int MaxStack = ItemMaxStack[pItem->getItemClass()];

                    if (pItem->getNum() + pPrevItem->getNum() > MaxStack) {
                        ItemNum_t CurrentNum = pPrevItem->getNum();
                        ItemNum_t AddNum = pItem->getNum();
                        ItemNum_t NewNum = AddNum + CurrentNum - MaxStack;

                        pPrevItem->setNum(MaxStack);
                        pItem->setNum(NewNum);
                        pArmsbandInventory->increaseNum(MaxStack - CurrentNum);
                        pArmsbandInventory->increaseWeight(pItem->getWeight() * (MaxStack - CurrentNum));
                        char pField[80];
                        snprintf(pField, sizeof(pField), "Num=%d, Storage=%d, StorageID=%u, X=%d", MaxStack,
                                 STORAGE_BELT, pArmsband->getItemID(), SlotID);
                        pPrevItem->tinysave(pField);

                        snprintf(pField, sizeof(pField), "Num=%d, Storage=%d", NewNum, STORAGE_EXTRASLOT);
                        pItem->tinysave(pField);


                        Success = true;
                    } else // When the count does not go past 9.
                    {
                        pOusters->deleteItemFromExtraInventorySlot();
                        pPrevItem->setNum(pPrevItem->getNum() + pItem->getNum());
                        pArmsbandInventory->increaseNum(pItem->getNum());
                        pArmsbandInventory->increaseWeight(pItem->getWeight() * pItem->getNum());
                        char pField[80];
                        snprintf(pField, sizeof(pField), "Num=%d, Storage=%d, StorageID=%u, X=%d", pPrevItem->getNum(),
                                 STORAGE_BELT, pArmsband->getItemID(), SlotID);
                        pPrevItem->tinysave(pField);

                        pItem->destroy();
                        SAFE_DELETE(pItem);
                        Success = true;
                    }
                } else // When the class and the type are not the same
                {
                    // Remove the item hanging on the mouse and the item in the belt.
                    pOusters->deleteItemFromExtraInventorySlot();
                    pArmsbandInventory->deleteItem(pPrevItem->getObjectID());

                    // Swap the two positions.
                    pOusters->addItemToExtraInventorySlot(pPrevItem);
                    pArmsbandInventory->addItem(SlotID, 0, pItem);

                    // Save to the DB.
                    char pField[80];
                    snprintf(pField, sizeof(pField), "Storage=%d", STORAGE_EXTRASLOT);
                    pPrevItem->tinysave(pField);

                    snprintf(pField, sizeof(pField), "Storage=%d, StorageID=%u, X=%d", STORAGE_BELT,
                             pArmsband->getItemID(), SlotID);
                    pItem->tinysave(pField);


                    Success = true;
                }
            } else // When the slot holds no existing item.
            {
                // Put a given item into the Inventory.
                pArmsbandInventory->addItem(SlotID, 0, pItem);

                // On a successful add, remove the item hanging on the mouse.
                pOusters->deleteItemFromExtraInventorySlot();
                char pField[80];
                snprintf(pField, sizeof(pField), "Storage=%d, StorageID=%u, X=%d", STORAGE_BELT, pArmsband->getItemID(),
                         SlotID);
                pItem->tinysave(pField);

                Success = true;
            }
        } // end of if (pArmsbandInventory->canAdding(SlotID, 0,  pItem))
    } // if (pCreature->isOusters())

    // Send a failure packet when adding to the QuickSlot failed.
    if (!Success) {
        GCCannotAdd _GCCannotAdd;
        _GCCannotAdd.setObjectID(pPacket->getObjectID());
        pPlayer->sendPacket(&_GCCannotAdd);
    }

#endif // __GAME_SERVER__

    __END_DEBUG_EX __END_CATCH
}
