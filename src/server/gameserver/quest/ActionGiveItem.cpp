////////////////////////////////////////////////////////////////////////////////
// Filename    : ActionGiveItem.cpp
// Written By  :
// Description :
////////////////////////////////////////////////////////////////////////////////

#include "ActionGiveItem.h"

#include "Creature.h"
#include "GCCreateItem.h"
#include "GameContext.h"
#include "GamePlayer.h"
#include "Inventory.h"
#include "ItemFactoryManager.h"
#include "ItemUtil.h"
#include "PacketUtil.h"
#include "PlayerCreature.h"
#include "Zone.h"

////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////
void ActionGiveItem::read(PropertyBuffer& propertyBuffer)

{
    try {
        m_ItemClass = (Item::ItemClass)propertyBuffer.getPropertyInt("ItemClass");
        m_ItemType = (ItemType_t)propertyBuffer.getPropertyInt("ItemType");
    } catch (NoSuchElementException& nsee) {
        throw Error(nsee.toString());
    }
}


////////////////////////////////////////////////////////////////////////////////
// Execute the action.
////////////////////////////////////////////////////////////////////////////////
void ActionGiveItem::execute(Creature* pCreature1, Creature* pCreature2)

{
    Assert(pCreature2 != NULL);
    Assert(pCreature2->isPC());

    PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pCreature2);
    Assert(pPC != NULL);

    Player* pPlayer = pCreature2->getPlayer();
    Assert(pPlayer != NULL);

    list<OptionType_t> optionTypeList;

    Item* pItem = context().itemFactories().createItem(m_ItemClass, m_ItemType, optionTypeList);
    Assert(pItem != NULL);

    _TPOINT pt;

    Inventory* pInventory = pPC->getInventory();
    Assert(pInventory != NULL);

    if (!pInventory->getEmptySlot(pItem, pt)) {
        // This action must always be paired with the ConditionHasInvenSpace condition.
        throw Error("ActionGiveItem: no room in the inventory; pair this action with ConditionHasInvenSpace.");
    }

    CoordInven_t X = pt.x;
    CoordInven_t Y = pt.y;

    pPC->getZone()->getObjectRegistry().registerObject(pItem);
    pInventory->addItem(X, Y, pItem);
    pItem->create(pPC->getName(), STORAGE_INVENTORY, 0, X, Y);

    if (pItem != NULL && pItem->isTraceItem()) {
        remainTraceLog(pItem, "ActionGiveItem", pCreature2->getName(), ITEM_LOG_CREATE, DETAIL_EVENTNPC);
        remainTraceLogNew(pItem, pCreature2->getName(), ITL_GET, ITLD_EVENTNPC, pCreature2->getZone()->getZoneID());
    }

    // Tell the client the gift was added.
    GCCreateItem gcCreateItem;
    makeGCCreateItem(&gcCreateItem, pItem, X, Y);
    pPlayer->sendPacket(&gcCreateItem);
}


////////////////////////////////////////////////////////////////////////////////
// get debug string
////////////////////////////////////////////////////////////////////////////////
string ActionGiveItem::toString() const

{
    StringStream msg;
    msg << "ActionGiveItem(" << ")";

    return msg.toString();
}
