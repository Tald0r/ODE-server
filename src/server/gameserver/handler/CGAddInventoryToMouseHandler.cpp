//////////////////////////////////////////////////////////////////////////////
// Filename    : CGAddInventoryToMouseHandler.cc
// Written By  : elca@ewestsoft.com
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "CGAddInventoryToMouse.h"

#ifdef __GAME_SERVER__
#include <stdio.h>

#include "GCCannotAdd.h"
#include "GCCreateItem.h"
#include "GCTradeVerify.h"
#include "GameContext.h"
#include "GamePlayer.h"
#include "Inventory.h"
#include "Item.h"
#include "ItemFactoryManager.h"
#include "ItemUtil.h"
#include "ObjectRegistry.h"
#include "PlayerCreature.h"
#include "Store.h"
#include "TradeManager.h"
#include "Zone.h"
#endif

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void CGAddInventoryToMouseHandler::execute(CGAddInventoryToMouse* pPacket, Player* pPlayer) {
    __BEGIN_TRY __BEGIN_DEBUG_EX

#ifdef __GAME_SERVER__

        GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(pPlayer);
    Assert(pGamePlayer != NULL);

    Creature* pCreature = pGamePlayer->getCreature();
    Assert(pCreature != NULL);

    PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pCreature);
    Assert(pPC != NULL);

    Zone* pZone = pPC->getZone();
    Assert(pZone != NULL);

    if (pPC->getStore()->isOpen()) {
        GCCannotAdd _GCCannotAdd;
        _GCCannotAdd.setObjectID(pPacket->getObjectID());
        pPlayer->sendPacket(&_GCCannotAdd);
        return;
    }

    CoordInven_t InvenX = pPacket->getX();
    CoordInven_t InvenY = pPacket->getY();
    ObjectID_t ItemOID = pPacket->getObjectID();

    Inventory* pInventory = pPC->getInventory();
    Assert(pInventory != NULL);

    // The inventory coordinates must not be exceeded.
    if (InvenX >= pInventory->getWidth() || InvenY >= pInventory->getHeight()) {
        GCCannotAdd _GCCannotAdd;
        _GCCannotAdd.setObjectID(pPacket->getObjectID());
        pPlayer->sendPacket(&_GCCannotAdd);
        return;
    }

    Item* pItem = pInventory->getItem(InvenX, InvenY);
    Item* pExtraSlotItem = pPC->getExtraInventorySlotItem();

    if (pPC->getStore()->hasItem(pItem)) {
        GCCannotAdd _GCCannotAdd;
        _GCCannotAdd.setObjectID(pPacket->getObjectID());
        pPlayer->sendPacket(&_GCCannotAdd);
        return;
    }

    // If there is no item to add, or something is already on the mouse,
    // it cannot be picked up.
    if (pItem == NULL || pExtraSlotItem != NULL) {
        GCCannotAdd _GCCannotAdd;
        _GCCannotAdd.setObjectID(pPacket->getObjectID());
        pPlayer->sendPacket(&_GCCannotAdd);
        return;
    }

    // Ordinary item-to-mouse routine
    if (ItemOID != 0) {
        // The OID must match.
        if (pItem->getObjectID() != ItemOID) {
            GCCannotAdd _GCCannotAdd;
            _GCCannotAdd.setObjectID(pPacket->getObjectID());
            pPlayer->sendPacket(&_GCCannotAdd);
            return;
        }

        pInventory->deleteItem(pItem->getObjectID());
        pPC->addItemToExtraInventorySlot(pItem);
        char pField[80];
        snprintf(pField, sizeof(pField), "Storage=%d, StorageID=0", STORAGE_EXTRASLOT);
        pItem->tinysave(pField);

        TradeManager* pTradeManager = pZone->getTradeManager();
        TradeInfo* pInfo = pTradeManager->getTradeInfo(pCreature->getName());
        if (pInfo != NULL && pInfo->getStatus() == TRADE_FINISH) {
            GCTradeVerify gcTradeVerify;
            gcTradeVerify.setCode(GC_TRADE_VERIFY_CODE_INVENTORY_TO_MOUSE_OK);
            pPlayer->sendPacket(&gcTradeVerify);
        }
    }
    // Routine that splits a stacked item
    else {
        // A non-stackable item, or a count below 2, cannot be split.
        if (!isStackable(pItem) || pItem->getNum() < 2 ||
            (pItem->getItemClass() == Item::ITEM_CLASS_MOON_CARD && pItem->getItemType() == 2 &&
             pItem->getNum() == 99) ||
            pPC->getStore()->hasItem(pItem)) {
            GCCannotAdd _GCCannotAdd;
            _GCCannotAdd.setObjectID(pPacket->getObjectID());
            pPlayer->sendPacket(&_GCCannotAdd);
            return;
        }

        // Create the same item from the existing one.
        Item::ItemClass IClass = pItem->getItemClass();
        ItemType_t IType = pItem->getItemType();
        const list<OptionType_t>& OType = pItem->getOptionTypeList();

        Item* pNewItem = de::gameContext().itemFactories().createItem(IClass, IType, OType);
        Assert(pNewItem != NULL);

        // The item added to the mouse keeps the existing OID,
        // and the item left in the inventory must get a new OID.
        Zone* pZone = pPC->getZone();
        Assert(pZone != NULL);

        ObjectRegistry& OR = pZone->getObjectRegistry();
        OR.registerObject(pNewItem);

        // The number left in the inventory is the original number minus one.
        // The existing item moved to the mouse, so its number becomes 1.
        // Delete the item moved from the inventory to the mouse,
        // and add the newly created item.
        pInventory->deleteItem(pItem->getObjectID());
        pPC->addItemToExtraInventorySlot(pItem);

        int NewNum = pItem->getNum() - 1;
        pNewItem->setNum(NewNum);
        pItem->setNum(1);

        pInventory->addItem(InvenX, InvenY, pNewItem);

        // Save the changed position information.
        char pField[80];
        snprintf(pField, sizeof(pField), "Num=%d, Storage=%d, StorageID=0", 1, STORAGE_EXTRASLOT);
        pItem->tinysave(pField);

        pNewItem->create(pPC->getName(), STORAGE_INVENTORY, 0, InvenX, InvenY);
        snprintf(pField, sizeof(pField), "Num=%d, Storage=%d, StorageID=0", NewNum, STORAGE_INVENTORY);
        pNewItem->tinysave(pField);


        // Using the GCCreateItem packet, send the client
        // the information about the item newly created in the inventory.
        GCCreateItem gcCreateItem;
        gcCreateItem.setObjectID(pNewItem->getObjectID());
        gcCreateItem.setItemClass((BYTE)pNewItem->getItemClass());
        gcCreateItem.setItemType(pNewItem->getItemType());
        gcCreateItem.setOptionType(pNewItem->getOptionTypeList());
        gcCreateItem.setDurability(pNewItem->getDurability());
        gcCreateItem.setSilver(pNewItem->getSilver());
        gcCreateItem.setEnchantLevel(pNewItem->getEnchantLevel());
        gcCreateItem.setItemNum(pNewItem->getNum());
        gcCreateItem.setInvenX(InvenX);
        gcCreateItem.setInvenY(InvenY);
        pPlayer->sendPacket(&gcCreateItem);
    }

#endif // __GAME_SERVER__

    __END_DEBUG_EX __END_CATCH
}
