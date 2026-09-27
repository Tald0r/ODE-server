//////////////////////////////////////////////////////////////////////////////
// Filename    : CGUseMessageItemFromInventoryHandler.cpp
// Written By  : elca@ewestsoft.com
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "CGUseMessageItemFromInventory.h"


#ifdef __GAME_SERVER__
#include "CastleInfoManager.h"
#include "CreatureUtil.h"
#include "DB.h"
#include "GCCannotUse.h"
#include "GCSystemMessage.h"
#include "GCUseOK.h"
#include "GameContext.h"
#include "GamePlayer.h"
#include "GameWorldInfoManager.h"
#include "Inventory.h"
#include "Item.h"
#include "ItemInfo.h"
#include "ItemInfoManager.h"
#include "ItemUtil.h"
#include "MonsterCorpse.h"
#include "PlayerCreature.h"
#include "Slayer.h"
#include "Store.h"
#include "Vampire.h"
#include "VariableManager.h"
#include "Zone.h"
#include "ZoneGroupManager.h"
#include "ZoneInfoManager.h"
#include "ZonePlayerManager.h"
#include "ZoneUtil.h"
#endif


//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void CGUseMessageItemFromInventoryHandler::execute(CGUseMessageItemFromInventory* pPacket, Player* pPlayer) {
    __BEGIN_TRY __BEGIN_DEBUG_EX

#ifdef __GAME_SERVER__

        Assert(pPacket != NULL);
    Assert(pPlayer != NULL);

    GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(pPlayer);
    Assert(pGamePlayer != NULL);

    Creature* pCreature = pGamePlayer->getCreature();
    Assert(pCreature != NULL);
    Assert(pCreature->isPC());

    PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pCreature);
    Assert(pPC != NULL);

    Inventory* pInventory = pPC->getInventory();
    Zone* pZone = pPC->getZone();

    Assert(pInventory != NULL);
    Assert(pZone != NULL);

    CoordInven_t InvenX = pPacket->getX();
    CoordInven_t InvenY = pPacket->getY();

    // An area beyond the inventory coordinates is not allowed.
    if (InvenX >= pInventory->getWidth() || InvenY >= pInventory->getHeight()) {
        GCCannotUse _GCCannotUse;
        _GCCannotUse.setObjectID(pPacket->getObjectID());
        pGamePlayer->sendPacket(&_GCCannotUse);
        return;
    }

    // It is an error when the inventory holds no such item.
    Item* pItem = pInventory->getItem(InvenX, InvenY);
    if (pItem == NULL) {
        GCCannotUse _GCCannotUse;
        _GCCannotUse.setObjectID(pPacket->getObjectID());
        pGamePlayer->sendPacket(&_GCCannotUse);
        return;
    }

    // Get the Object of the item in the inventory.
    ObjectID_t ItemObjectID = pItem->getObjectID();

    // A mismatched OID, or an item that cannot be used, is an error.
    if (ItemObjectID != pPacket->getObjectID() || !isUsableItem(pItem, pCreature)) {
        GCCannotUse _GCCannotUse;
        _GCCannotUse.setObjectID(pPacket->getObjectID());
        pGamePlayer->sendPacket(&_GCCannotUse);
        return;
    }

    if (pPC->getStore()->getItemIndex(pItem) != 0xff) {
        GCCannotUse _GCCannotUse;
        _GCCannotUse.setObjectID(pPacket->getObjectID());
        pGamePlayer->sendPacket(&_GCCannotUse);
        return;
    }

    // Branch to the handling function by item kind.
    switch (pItem->getItemClass()) {
    case Item::ITEM_CLASS_EVENT_TREE:
        executeEventTree(pPacket, pPlayer);
        break;
    // add by Coffee 2007-8-5
    case Item::ITEM_CLASS_EFFECT_ITEM:
        executeEventFromMessage(pPacket, pPlayer);
        break;
    default: {
        //  by sigi. 2002.12.25
        filelog("useItemError.txt", "[CGUseMessageItemFromInventory] No Such ItemClassHandler=%s, owner=%s",
                ItemClass2ShortString[pItem->getItemClass()].c_str(), pCreature->getName().c_str());

        GCCannotUse _GCCannotUse;
        _GCCannotUse.setObjectID(pPacket->getObjectID());
        pGamePlayer->sendPacket(&_GCCannotUse);
    }
        return;
    }

#endif

    __END_DEBUG_EX __END_CATCH
}


void CGUseMessageItemFromInventoryHandler::executeEventTree(CGUseMessageItemFromInventory* pPacket, Player* pPlayer) {
    __BEGIN_TRY __BEGIN_DEBUG_EX

#ifdef __GAME_SERVER__

        Assert(pPacket != NULL);
    Assert(pPlayer != NULL);

    // The enclosing function checked plenty of errors, so
    // the error checking here is cut right down.
    GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(pPlayer);
    Creature* pCreature = pGamePlayer->getCreature();
    PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pCreature);
    Inventory* pInventory = pPC->getInventory();
    Zone* pZone = pPC->getZone();
    CoordInven_t InvenX = pPacket->getX();
    CoordInven_t InvenY = pPacket->getY();
    Item* pItem = pInventory->getItem(InvenX, InvenY);
    ObjectID_t ItemObjectID = pItem->getObjectID();
    MonsterType_t MType = 0;

    int time = 0;

    switch (pItem->getItemType()) {
    case 12:
        MType = 482;
        time = de::gameContext().variables().getVariable(CHRISTMAS_TREE_DECAY_TIME) / 10;
        break;
    case 26:
        MType = 650;
        time = 21600;
        break;
    case 27:
        MType = 650;
        time = 43200;
        break;
    case 28:
        MType = 650;
        time = 86400;
        break;
    default: {
        filelog("EventTree.log", "Used an unexpected item. : %s used %d", pPC->getName().c_str(), pItem->getItemType());
        GCCannotUse _GCCannotUse;
        _GCCannotUse.setObjectID(pPacket->getObjectID());
        pGamePlayer->sendPacket(&_GCCannotUse);
    }
        return;
    }

    // Take it for a skill used on a tile and check whether it can be used.
    // It cannot be used in a safe zone.
    // Check that the item type can be used. Only ItemType 12 can be used.
    // It cannot be used when another tree stands nearby (within 5x5 tiles around the player).
    if (!isAbleToUseTileSkill(pCreature) || pZone->isMasterLair() || ItemObjectID != pPacket->getObjectID() ||
        checkCorpse(pZone, MType, pPC->getX() - 2, pPC->getY() - 2, pPC->getX() + 2, pPC->getY() + 2)) {
        GCCannotUse _GCCannotUse;
        _GCCannotUse.setObjectID(pPacket->getObjectID());
        pGamePlayer->sendPacket(&_GCCannotUse);
        return;
    }

    // In a castle only the owning guild's members can use it.
    if (!pPC->isGOD()) {
        if (pZone->isCastle()) {
            if (!de::gameContext().castleInfos().isCastleMember(pZone->getZoneID(), pPC)) {
                GCCannotUse _GCCannotUse;
                _GCCannotUse.setObjectID(pPacket->getObjectID());
                pGamePlayer->sendPacket(&_GCCannotUse);
                return;
            }
        }
        // It can never be used in a safe zone outside a castle.
        else if (pZone->getZoneLevel(pCreature->getX(), pCreature->getY()) & SAFE_ZONE) {
            GCCannotUse _GCCannotUse;
            _GCCannotUse.setObjectID(pPacket->getObjectID());
            pGamePlayer->sendPacket(&_GCCannotUse);
            return;
        }
    }

    if (!createBulletinBoard(pZone, pPC->getX(), pPC->getY(), MType, pPacket->getMessage(),
                             VSDateTime::currentDateTime().addSecs(time))) {
        GCCannotUse _GCCannotUse;
        _GCCannotUse.setObjectID(pPacket->getObjectID());
        pGamePlayer->sendPacket(&_GCCannotUse);

        return;
    }

    // The item was used, so it is erased.
    pInventory->deleteItem(InvenX, InvenY);
    pItem->destroy();
    SAFE_DELETE(pItem);

    // Tell the client that the item was used.
    GCUseOK gcUseOK;
    pGamePlayer->sendPacket(&gcUseOK);


#endif
    __END_DEBUG_EX __END_CATCH
}

void CGUseMessageItemFromInventoryHandler::executeEventFromMessage(CGUseMessageItemFromInventory* pPacket,
                                                                   Player* pPlayer) {
    __BEGIN_TRY __BEGIN_DEBUG_EX

#ifdef __GAME_SERVER__

        Assert(pPacket != NULL);
    Assert(pPlayer != NULL);


    GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(pPlayer);
    Creature* pCreature = pGamePlayer->getCreature();
    PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pCreature);
    Inventory* pInventory = pPC->getInventory();
    CoordInven_t InvenX = pPacket->getX();
    CoordInven_t InvenY = pPacket->getY();
    Item* pItem = pInventory->getItem(InvenX, InvenY);

    // Check the item
    // Set the color  green = 0  blue = 1  yellow = 2
    string color = "0";
    switch (pItem->getItemType()) {
    case 10:
        color = "0";
        break;
    case 11:
        color = "1";
        break;
    case 12:
        color = "2";
        break;
    default: {
        filelog("EventTree.log", "Failed to use the notice board. Character: %s  item type %d", pPC->getName().c_str(),
                pItem->getItemType());
        GCCannotUse _GCCannotUse;
        _GCCannotUse.setObjectID(pPacket->getObjectID());
        pGamePlayer->sendPacket(&_GCCannotUse);
    }
        return;
    }


    decreaseItemNum(pItem, pInventory, pCreature->getName(), STORAGE_INVENTORY, 0, InvenX, InvenY);


    GCSystemMessage _GCSystemMessage;
    string message = pPacket->getMessage().c_str();
    message += color.c_str();
    _GCSystemMessage.setMessage(message);
    _GCSystemMessage.setType(SYSTEM_MESSAGE_PLAYER);
    de::gameContext().zoneGroups().broadcast(&_GCSystemMessage);
    GCUseOK gcUseOK;
    pGamePlayer->sendPacket(&gcUseOK);


#endif
    __END_DEBUG_EX __END_CATCH
}
