//////////////////////////////////////////////////////////////////////////////
// Filename    : CGAddMouseToInventoryHandler.cc
// Written By  : elca@ewestsoft.com
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "CGAddMouseToInventory.h"

#ifdef __GAME_SERVER__
#include <stdio.h>

#include "GCCannotAdd.h"
#include "GCCreateItem.h"
#include "GCDeleteInventoryItem.h"
#include "GCNoticeEvent.h"
#include "GCTradeVerify.h"
#include "GameContext.h"
#include "GamePlayer.h"
#include "Inventory.h"
#include "Item.h"
#include "ItemFactoryManager.h"
#include "ItemInfo.h"
#include "ItemInfoManager.h"
#include "ItemUtil.h"
#include "PacketUtil.h"
#include "Slayer.h"
#include "TradeManager.h"
#include "Vampire.h"
#include "VariableManager.h"
#include "Zone.h"
#endif

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void CGAddMouseToInventoryHandler::execute(CGAddMouseToInventory* pPacket, Player* pPlayer) {
    __BEGIN_TRY __BEGIN_DEBUG_EX

#ifdef __GAME_SERVER__

        Assert(pPacket != NULL);
    Assert(pPlayer != NULL);


    try {
        GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(pPlayer);
        Assert(pGamePlayer != NULL);

        Creature* pCreature = pGamePlayer->getCreature();
        Assert(pCreature != NULL);
        Assert(pCreature->isPC());

        PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pCreature);
        Assert(pPC != NULL);

        Zone* pZone = pPC->getZone();
        Assert(pZone != NULL);

        Inventory* pInventory = pPC->getInventory();
        Assert(pInventory != NULL);

        Item* pItem = pPC->getExtraInventorySlot()->getItem();
        bool Success = false;
        ItemFactoryManager& itemFactories = de::gameContext().itemFactories();

        if (pItem == NULL) {
            GCCannotAdd _GCCannotAdd;
            _GCCannotAdd.setObjectID(pPacket->getObjectID());
            pPlayer->sendPacket(&_GCCannotAdd);
            return;
        }

        ObjectID_t itemObjectID = pItem->getObjectID();
        CoordInven_t InvenX = pPacket->getInvenX();
        CoordInven_t InvenY = pPacket->getInvenY();

        if (InvenX >= 10 || InvenY >= 6) {
            GCCannotAdd _GCCannotAdd;
            _GCCannotAdd.setObjectID(pPacket->getObjectID());
            pPlayer->sendPacket(&_GCCannotAdd);
            return;
        }

        // Check that the item's ObjectID matches.
        if (itemObjectID != pPacket->getObjectID()) {
            GCCannotAdd _GCCannotAdd;
            _GCCannotAdd.setObjectID(pPacket->getObjectID());
            pPlayer->sendPacket(&_GCCannotAdd);
            return;
        }

        // Check whether it can go into the inventory.
        if (!pInventory->canAdding(InvenX, InvenY, pItem)) {
            GCCannotAdd _GCCannotAdd;
            _GCCannotAdd.setObjectID(pPacket->getObjectID());
            pPlayer->sendPacket(&_GCCannotAdd);
            return;
        }

        TPOINT pt;
        pt.x = 99;
        pt.y = 99;


        // Get the Item of the target Inventory Slot.
        Item* pPrevItem = pInventory->searchItem(InvenX, InvenY, pItem, pt);

        // If there is an item in that place
        if (pPrevItem != NULL) {
            bool bisSame = true;
            // When the item class is the same, raise the count and remove the one on the mouse.
            if (canStack(pItem, pPrevItem)) {
                // Keep time-limited items from being put together
                if (pItem->isTimeLimitItem() || pPrevItem->isTimeLimitItem()) {
                    GCCannotAdd _GCCannotAdd;
                    _GCCannotAdd.setObjectID(pPacket->getObjectID());
                    pPlayer->sendPacket(&_GCCannotAdd);
                    return;
                }
                int MaxStack = ItemMaxStack[pItem->getItemClass()];

                VolumeWidth_t ItemWidth = pItem->getVolumeWidth();
                VolumeHeight_t ItemHeight = pItem->getVolumeHeight();
                VolumeWidth_t InvenWidth = pInventory->getWidth();
                VolumeWidth_t InvenHeight = pInventory->getHeight();


                if ((InvenX + ItemWidth <= InvenWidth) && (InvenY + ItemHeight <= InvenHeight)) {
                    for (int x = InvenX; x < (InvenX + ItemWidth); x++) {
                        for (int y = InvenY; y < (InvenY + ItemHeight); y++) {
                            if (pInventory->hasItem(x, y)) {
                                if (pInventory->getItem(x, y) != pPrevItem) {
                                    bisSame = false;
                                    break;
                                }
                            } else {
                                bisSame = false;
                                break;
                            }
                        }
                    }
                }

                // What if the incoming item and the item already there have exactly the same coordinates?
                if (bisSame) {
                    // If the count goes past the stack limit, add only up to it and leave the rest on the mouse.
                    if (pItem->getNum() + pPrevItem->getNum() > MaxStack) {
                        ItemNum_t CurrentNum = pPrevItem->getNum();
                        ItemNum_t AddNum = pItem->getNum();
                        ItemNum_t NewNum = AddNum + CurrentNum - MaxStack;

                        pPrevItem->setNum(MaxStack);
                        pItem->setNum(NewNum);
                        pInventory->increaseNum(MaxStack - CurrentNum);
                        pInventory->increaseWeight(pItem->getWeight() * (MaxStack - CurrentNum));
                        char pField[80];
                        snprintf(pField, sizeof(pField), "Num=%d, Storage=%d, X=%d, Y=%d", MaxStack, STORAGE_INVENTORY,
                                 InvenX, InvenY);
                        pPrevItem->tinysave(pField);

                        snprintf(pField, sizeof(pField), "Num=%d, Storage=%d", NewNum, STORAGE_EXTRASLOT);
                        pItem->tinysave(pField);

                        Success = true;
                    } else {
                        pPC->deleteItemFromExtraInventorySlot();
                        pPrevItem->setNum(pPrevItem->getNum() + pItem->getNum());
                        pInventory->increaseNum(pItem->getNum());
                        pInventory->increaseWeight(pItem->getWeight() * pItem->getNum());
                        char pField[80];
                        snprintf(pField, sizeof(pField), "Num=%d, Storage=%d, X=%d, Y=%d", pPrevItem->getNum(),
                                 STORAGE_INVENTORY, InvenX, InvenY);
                        pPrevItem->tinysave(pField);

                        pItem->destroy();
                        SAFE_DELETE(pItem);
                        Success = true;
                    }

                    if (de::gameContext().variables().getVariable(NETMARBLE_CARD_EVENT) != 0 &&
                        pPrevItem->getItemClass() == Item::ITEM_CLASS_MOON_CARD && pPrevItem->getItemType() == 2 &&
                        pPrevItem->getNum() == 99) {
                        GCNoticeEvent gcNE;
                        gcNE.setCode(NOTICE_EVENT_NETMARBLE_CARD_FULL);
                        pGamePlayer->sendPacket(&gcNE);
                        cout << "gcNE sent" << endl;
                    }

                    if (pPrevItem->getItemClass() == Item::ITEM_CLASS_LUCKY_BAG && pPrevItem->getItemType() == 3 &&
                        pPrevItem->getNum() == 50) {
                        GCDeleteInventoryItem gcDI;
                        gcDI.setObjectID(pPrevItem->getObjectID());
                        pGamePlayer->sendPacket(&gcDI);

                        pInventory->deleteItem(pPrevItem->getObjectID());
                        pPrevItem->destroy();
                        SAFE_DELETE(pPrevItem);

                        Item* pNewItem =
                            itemFactories.createItem(Item::ITEM_CLASS_EVENT_ITEM, 28, list<OptionType_t>());
                        pZone->registerObject(pNewItem);

                        if (!pInventory->addItem(pt.x, pt.y, pNewItem)) {
                            cout << "-_-;" << endl;
                            return;
                        }

                        pNewItem->create(pPC->getName(), STORAGE_INVENTORY, 0, pt.x, pt.y);

                        if (pNewItem != NULL) {
                            GCCreateItem gcCI;
                            makeGCCreateItem(&gcCI, pNewItem, pt.x, pt.y);
                            pGamePlayer->sendPacket(&gcCI);
                        }
                    }
                } else {
                    pInventory->deleteItem(pPrevItem->getObjectID());

                    pPC->deleteItemFromExtraInventorySlot();

                    pPC->addItemToExtraInventorySlot(pPrevItem);

                    pInventory->addItem(InvenX, InvenY, pItem);

                    char pField[80];
                    snprintf(pField, sizeof(pField), "Storage=%d", STORAGE_EXTRASLOT);
                    pPrevItem->tinysave(pField);

                    snprintf(pField, sizeof(pField), "Storage=%d, X=%d, Y=%d", STORAGE_INVENTORY, InvenX, InvenY);
                    pItem->tinysave(pField);


                    Success = true;
                }
            } else // If the item class differs, or the item does not stack.
            {
                pInventory->deleteItem(pPrevItem->getObjectID());

                pPC->deleteItemFromExtraInventorySlot();

                pPC->addItemToExtraInventorySlot(pPrevItem);

                pInventory->addItem(InvenX, InvenY, pItem);

                char pField[80];
                snprintf(pField, sizeof(pField), "Storage=%d", STORAGE_EXTRASLOT);
                pPrevItem->tinysave(pField);

                snprintf(pField, sizeof(pField), "Storage=%d, X=%d, Y=%d", STORAGE_INVENTORY, InvenX, InvenY);
                pItem->tinysave(pField);


                Success = true;
            }
        } else // If there is no item in that place.
        {
            // Put a given item into the Inventory.
            pInventory->addItem(InvenX, InvenY, pItem);

            // On a successful add, remove the item hanging on the mouse.
            pPC->deleteItemFromExtraInventorySlot();

            char pField[80];
            snprintf(pField, sizeof(pField), "Storage=%d, X=%d, Y=%d", STORAGE_INVENTORY, InvenX, InvenY);
            pItem->tinysave(pField);


            Success = true;
        }


        if (Success) {
            TradeManager* pTradeManager = pZone->getTradeManager();
            TradeInfo* pInfo = pTradeManager->getTradeInfo(pCreature->getName());
            if (pInfo != NULL && pInfo->getStatus() == TRADE_FINISH) {
                GCTradeVerify gcTradeVerify;
                gcTradeVerify.setCode(GC_TRADE_VERIFY_CODE_MOUSE_TO_INVENTORY_OK);
                pPlayer->sendPacket(&gcTradeVerify);
            }

            // When it is a tree piece
            if (pItem != NULL && pItem->getItemClass() == Item::ITEM_CLASS_EVENT_TREE) {
                ItemType_t itemtype = pItem->getItemType();

                // When it is a Christmas tree piece
                if (itemtype <= 11) {
                    // See whether the Christmas tree pieces fit together.
                    TPOINT pt = checkEventPuzzle(pPC, InvenX, InvenY, 0);
                    if (pt.x != -1 && pt.y != -1) {
                        // Erase the tree pieces that fit together.
                        deleteInventoryItem(pInventory, pt.x, pt.y, pt.x + 2, pt.y + 3);

                        // Create the tree.
                        list<OptionType_t> optionType;
                        Item* pTreeItem = itemFactories.createItem(Item::ITEM_CLASS_EVENT_TREE, 12, optionType);
                        pZone->getObjectRegistry().registerObject(pTreeItem);
                        pInventory->addItem(pt.x, pt.y, pTreeItem);
                        pTreeItem->create(pPC->getName(), STORAGE_INVENTORY, 0, pt.x, pt.y);

                        // Tell the client that the tree was made.
                        GCCreateItem gcCreateItem;
                        makeGCCreateItem(&gcCreateItem, pTreeItem, pt.x, pt.y);
                        pGamePlayer->sendPacket(&gcCreateItem);
                    }
                }
                // When it is a Christmas tree piece
                else if (itemtype > 12 && itemtype <= 24) {
                    // See whether the Christmas tree pieces fit together.
                    TPOINT pt = checkEventPuzzle(pPC, InvenX, InvenY, 13);
                    if (pt.x != -1 && pt.y != -1) {
                        // Erase the tree pieces that fit together.
                        deleteInventoryItem(pInventory, pt.x, pt.y, pt.x + 2, pt.y + 3);

                        // Create the tree.
                        list<OptionType_t> optionType;
                        Item* pTreeItem = itemFactories.createItem(Item::ITEM_CLASS_EVENT_TREE, 25, optionType);
                        pTreeItem->setQuestItem();
                        pZone->getObjectRegistry().registerObject(pTreeItem);
                        pInventory->addItem(pt.x, pt.y, pTreeItem);
                        pTreeItem->create(pPC->getName(), STORAGE_INVENTORY, 0, pt.x, pt.y);

                        // Tell the client that the tree was made.
                        GCCreateItem gcCreateItem;
                        makeGCCreateItem(&gcCreateItem, pTreeItem, pt.x, pt.y);
                        pGamePlayer->sendPacket(&gcCreateItem);
                    }
                }
                // When it is a Christmas tree piece
                else if (itemtype > 28 && itemtype <= 40) {
                    // See whether the Christmas tree pieces fit together.
                    TPOINT pt = checkEventPuzzle(pPC, InvenX, InvenY, 29);
                    if (pt.x != -1 && pt.y != -1) {
                        // Erase the tree pieces that fit together.
                        deleteInventoryItem(pInventory, pt.x, pt.y, pt.x + 2, pt.y + 3);

                        // Create the tree.
                        list<OptionType_t> optionType;
                        Item* pTreeItem = itemFactories.createItem(Item::ITEM_CLASS_EVENT_TREE, 41, optionType);
                        pTreeItem->setQuestItem();
                        pZone->getObjectRegistry().registerObject(pTreeItem);
                        pInventory->addItem(pt.x, pt.y, pTreeItem);
                        pTreeItem->create(pPC->getName(), STORAGE_INVENTORY, 0, pt.x, pt.y);

                        // Tell the client that the tree was made.
                        GCCreateItem gcCreateItem;
                        makeGCCreateItem(&gcCreateItem, pTreeItem, pt.x, pt.y);
                        pGamePlayer->sendPacket(&gcCreateItem);
                    }
                }
            }
        } else {
            GCCannotAdd _GCCannotAdd;
            _GCCannotAdd.setObjectID(pPacket->getObjectID());
            pPlayer->sendPacket(&_GCCannotAdd);
        }
    } catch (Throwable& t) {
    }


#endif // __GAME_SERVER__

    __END_DEBUG_EX __END_CATCH
}
