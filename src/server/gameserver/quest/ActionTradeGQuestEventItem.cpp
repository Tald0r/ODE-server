////////////////////////////////////////////////////////////////////////////////
// Filename    : ActionTradeGQuestEventItem.cpp
// Description :
////////////////////////////////////////////////////////////////////////////////
#include "ActionTradeGQuestEventItem.h"

#include <stdio.h>

#include <list>

#include "DB.h"
#include "GCCreateItem.h"
#include "GCNPCResponse.h"
#include "GCSystemMessage.h"
#include "GQuestInventory.h"
#include "GQuestManager.h"
#include "GameContext.h"
#include "GamePlayer.h"
#include "Inventory.h"
#include "Item.h"
#include "ItemFactoryManager.h"
#include "ItemUtil.h"
#include "PacketUtil.h"
#include "PlayerCreature.h"
#include "PriceManager.h"
#include "Slayer.h"
#include "StringPool.h"
#include "Thread.h"
#include "Vampire.h"
#include "Zone.h"
#include "item/Key.h"

////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////
void ActionTradeGQuestEventItem::read(PropertyBuffer& propertyBuffer)

{
    try {
    } catch (NoSuchElementException& nsee) {
        throw Error(nsee.toString());
    }
}

////////////////////////////////////////////////////////////////////////////////
// Execute the action.
////////////////////////////////////////////////////////////////////////////////
void ActionTradeGQuestEventItem::execute(Creature* pCreature1, Creature* pCreature2)

{
    Assert(pCreature1 != NULL);
    Assert(pCreature2 != NULL);
    Assert(pCreature1->isNPC());
    Assert(pCreature2->isPC());

    PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pCreature2);
    Assert(pPC != NULL);

    Player* pPlayer = pPC->getPlayer();
    Assert(pPlayer != NULL);

    GQuestInventory& inven = pPC->getGQuestManager()->getGQuestInventory();

    // Send GCNPCResponse to the client first.
    GCNPCResponse okpkt;
    pPlayer->sendPacket(&okpkt);

    list<ItemType_t>::iterator itr = inven.getItems().begin();
    list<ItemType_t>::iterator endItr = inven.getItems().end();

    int count[3] = {0, 0, 0};

    for (; itr != endItr; ++itr) {
        if ((*itr) < 4 || (*itr) > 12)
            continue;
        int level = (*itr) % 3;
        count[level]++;
    }

    if (count[0] < 5 || count[1] < 5 || count[2] < 5) {
        GCSystemMessage gcSM;
        gcSM.setMessage("You need 15 quest items.");
        pPC->getPlayer()->sendPacket(&gcSM);
        return;
    }

    _TPOINT tp;
    if (!pPC->getInventory()->getEmptySlot(1, 1, tp)) {
        GCSystemMessage gcSM;
        gcSM.setMessage("Not enough inventory space.");
        pPC->getPlayer()->sendPacket(&gcSM);
        return;
    }

    for (itr = inven.getItems().begin(); itr != endItr;) {
        if ((*itr) < 4 || (*itr) > 12)
            continue;
        inven.removeOne(pPC->getName(), *itr);
        inven.getItems().erase(itr++);
    }

    pPC->getPlayer()->sendPacket(inven.getInventoryPacket());

    Item::ItemClass iClass;
    ItemType_t iType;
    list<OptionType_t> optionList;

    if (pPC->isSlayer())
        iClass = Item::ITEM_CLASS_RING;
    else if (pPC->isVampire())
        iClass = Item::ITEM_CLASS_VAMPIRE_RING;
    else
        iClass = Item::ITEM_CLASS_OUSTERS_RING;

    if (pPC->getLevel() <= 50) {
        iType = 5;
    } else if (pPC->getLevel() <= 90) {
        iType = 6;
    } else
        iType = 7;

    int value = rand() % 6;
    string option;

    switch (value) {
    case 0:
        option = "DAM+4,MPSTL+3";
        break;
    case 1:
        option = "ASPD+4,ATTR+2";
        break;
    case 2:
        option = "STR+4,LUCK+2";
        break;
    case 3:
        option = "DEX+4,LUCK+2";
        break;
    case 4:
        option = "INT+4,LUCK+2";
        break;
    case 5:
        option = "RES+4,HP+2";
        break;
    default:
        break;
    }

    makeOptionList(option, optionList);
    if (optionList.size() != 2) {
        filelog("GQuestEventBug.log", "Wrong option. : %d/%s", value, option.c_str());
    }

    Item* pItem = context().itemFactories().createItem(iClass, iType, optionList);
    pItem->setGrade(4);

    pPC->getZone()->registerObject(pItem);
    Assert(pPC->getInventory()->addItem(pItem, tp));
    Assert(tp.x != -1);

    pItem->create(pPC->getName(), STORAGE_INVENTORY, 0, tp.x, tp.y);

    GCCreateItem gcCI;
    makeGCCreateItem(&gcCI, pItem, tp.x, tp.y);
    pPC->getPlayer()->sendPacket(&gcCI);

    filelog("GQuestEvent.log", "Gave the event quest item. : [%s]%u/%s/%u", pPC->getName().c_str(), iType,
            option.c_str(), pItem->getItemID());

    GCNPCResponse response;
    response.setCode(NPC_RESPONSE_QUIT_DIALOGUE);
    pPlayer->sendPacket(&response);

    GCSystemMessage gcSM;
    gcSM.setMessage("The prize has been handed out.");
    pPC->getPlayer()->sendPacket(&gcSM);
}


////////////////////////////////////////////////////////////////////////////////
// get debug string
////////////////////////////////////////////////////////////////////////////////
string ActionTradeGQuestEventItem::toString() const

{
    StringStream msg;
    msg << "ActionTradeGQuestEventItem(" << ")";
    return msg.toString();
}
