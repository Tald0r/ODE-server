////////////////////////////////////////////////////////////////////////////////
// Filename    : ActionTradeGiftBox.cpp
// Description :
////////////////////////////////////////////////////////////////////////////////
#include "ActionTradeGiftBox.h"

#include <list>

#include "DB.h"
#include "FlagSet.h"
#include "GCCreateItem.h"
#include "GCDeleteInventoryItem.h"
#include "GCNPCResponse.h"
#include "GameContext.h"
#include "GamePlayer.h"
#include "Inventory.h"
#include "Item.h"
#include "ItemFactoryManager.h"
#include "ItemUtil.h"
#include "OptionInfo.h"
#include "PacketUtil.h"
#include "PlayerCreature.h"
#include "PriceManager.h"
#include "Slayer.h"
#include "Thread.h"
#include "Vampire.h"
#include "Zone.h"

////////////////////////////////////////////////////////////////////////////////
// ActionTradeGiftBox
////////////////////////////////////////////////////////////////////////////////
ActionTradeGiftBox::ActionTradeGiftBox() : m_pLuaState(NULL), m_pLuaSlayerItem(NULL), m_pLuaVampireItem(NULL) {
    m_pLuaState = new LuaState;
    m_pLuaState->init();

    m_pLuaSlayerItem = new LuaTradeEventSlayerItem(m_pLuaState);
    m_pLuaVampireItem = new LuaTradeEventVampireItem(m_pLuaState);
}

////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////
ActionTradeGiftBox::~ActionTradeGiftBox() {
    __BEGIN_TRY

    SAFE_DELETE(m_pLuaState);
    SAFE_DELETE(m_pLuaSlayerItem);
    SAFE_DELETE(m_pLuaVampireItem);

    __END_CATCH_NO_RETHROW
}

////////////////////////////////////////////////////////////////////////////////
//  load
////////////////////////////////////////////////////////////////////////////////
void ActionTradeGiftBox::load()

{
    Assert(m_pLuaState != NULL);

    int result = m_pLuaState->dofile(m_CommonFilename);
    LuaState::logError(result);
}

////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////
void ActionTradeGiftBox::read(PropertyBuffer& propertyBuffer)

{
    try {
        // read script id
        m_CommonFilename = propertyBuffer.getProperty("CommonFilename");
        m_SlayerFilename = propertyBuffer.getProperty("SlayerFilename");
        m_VampireFilename = propertyBuffer.getProperty("VampireFilename");

        load();
    } catch (NoSuchElementException& nsee) {
        throw Error(nsee.toString());
    }
}

////////////////////////////////////////////////////////////////////////////////
// Execute the action.
////////////////////////////////////////////////////////////////////////////////
void ActionTradeGiftBox::execute(Creature* pCreature1, Creature* pCreature2)

{
    Assert(pCreature1 != NULL);
    Assert(pCreature2 != NULL);
    Assert(pCreature1->isNPC());
    Assert(pCreature2->isPC());

    PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pCreature2);
    Assert(pPC != NULL);

    Player* pPlayer = pPC->getPlayer();
    Assert(pPlayer != NULL);

    Inventory* pInventory = pPC->getInventory();
    Assert(pInventory != NULL);

    Zone* pZone = pPC->getZone();
    Assert(pZone != NULL);

    FlagSet* pFlagSet = pPC->getFlagSet();

    Item::ItemClass ItemClass;
    ItemType_t ItemType;
    OptionType_t OptionType;

    Item* pItem;
    Item* pGiftBoxItem;

    // The gift has already been exchanged.
    if (pFlagSet->isOn(FLAGSET_TRADE_GIFT_BOX_2002_12)) {
        GCNPCResponse response;
        response.setCode(NPC_RESPONSE_TRADE_GIFT_BOX_ALREADY_TRADE);
        pPlayer->sendPacket(&response);

        GCNPCResponse quit;
        quit.setCode(NPC_RESPONSE_QUIT_DIALOGUE);
        pPlayer->sendPacket(&quit);

        return;
    }

    // Check whether a red gift box is there.
    CoordInven_t X, Y;
    pGiftBoxItem = pInventory->findItem(Item::ITEM_CLASS_EVENT_GIFT_BOX, 1, X, Y);
    if (pGiftBoxItem == NULL) {
        GCNPCResponse response;
        response.setCode(NPC_RESPONSE_TRADE_GIFT_BOX_NO_ITEM);
        pPlayer->sendPacket(&response);

        GCNPCResponse quit;
        quit.setCode(NPC_RESPONSE_QUIT_DIALOGUE);
        pPlayer->sendPacket(&quit);

        return;
    }

    LuaSelectItem* pLuaSelectItem = NULL;
    string luaFileName;

    if (pPC->isSlayer()) {
        // Set the sum of the Slayer's stats in Lua.
        Slayer* pSlayer = dynamic_cast<Slayer*>(pPC);
        Assert(pSlayer != NULL);

        Attr_t sum = pSlayer->getSTR(ATTR_BASIC) + pSlayer->getDEX(ATTR_BASIC) + pSlayer->getINT(ATTR_BASIC);

        m_pLuaSlayerItem->setSum(sum);
        pLuaSelectItem = m_pLuaSlayerItem;
        luaFileName = m_SlayerFilename;

    } else if (pPC->isVampire()) {
        // Set the Vampire's level in Lua.
        Vampire* pVampire = dynamic_cast<Vampire*>(pPC);
        Assert(pVampire != NULL);

        int level = pVampire->getLevel();
        m_pLuaVampireItem->setLevel(level);
        pLuaSelectItem = m_pLuaVampireItem;
        luaFileName = m_VampireFilename;
    }

    // There is a selector script for a Slayer and one for a Vampire, and
    // none for an Ousters; an Ousters leaves the dialogue with nothing
    // rather than being served by no selector at all.
    if (pLuaSelectItem == NULL) {
        filelog("XMasEventError.txt", "[ No item selector ] : %s", pPC->getName().c_str());

        GCNPCResponse quit;
        quit.setCode(NPC_RESPONSE_QUIT_DIALOGUE);
        pPlayer->sendPacket(&quit);

        return;
    }

    //--------------------------------------------------------
    // Code that ran this 1000 times to check the speed.
    // The result was about 0.07 seconds.
    //--------------------------------------------------------

    // Create the item from the result Lua computes.
    pLuaSelectItem->prepare();

    int result = pLuaSelectItem->executeFile(luaFileName);
    LuaState::logError(result);

    ItemClass = pLuaSelectItem->getItemClass();
    ItemType = pLuaSelectItem->getItemType();
    OptionType = pLuaSelectItem->getOptionType();

    pLuaSelectItem->clear();

    if (ItemClass >= Item::ITEM_CLASS_MAX)
    //||  ItemType  >= ITEM_TYPE_MAX || ItemType  < 0
    //	|| OptionType == 0)
    {
        filelog("XMasEventError.txt", "[ ItemInfo Error ] : ItemClass = %d , ItemType = %d , OptionType = %d",
                ItemClass, ItemType, OptionType);

        GCNPCResponse quit;
        quit.setCode(NPC_RESPONSE_QUIT_DIALOGUE);
        pPlayer->sendPacket(&quit);

        return;
    }

    // Tell the client to remove the gift box.
    GCDeleteInventoryItem gcDeleteInventoryItem;
    gcDeleteInventoryItem.setObjectID(pGiftBoxItem->getObjectID());
    pPlayer->sendPacket(&gcDeleteInventoryItem);

    // Remove the gift box.
    pInventory->deleteItem(X, Y);
    // Leave an ItemTraceLog.
    if (pGiftBoxItem != NULL && pGiftBoxItem->isTraceItem()) {
        remainTraceLog(pGiftBoxItem, pCreature2->getName(), pCreature1->getName(), ITEM_LOG_DELETE, DETAIL_EVENTNPC);
    }
    pGiftBoxItem->destroy();
    SAFE_DELETE(pGiftBoxItem);


    // Build the gift item.
    list<OptionType_t> optionTypeList;
    if (OptionType != 0)
        optionTypeList.push_back(OptionType);

    pItem = context().itemFactories().createItem(ItemClass, ItemType, optionTypeList);
    Assert(pItem != NULL);


    // Add the gift to the inventory.
    pZone->getObjectRegistry().registerObject(pItem);
    pInventory->addItem(X, Y, pItem);
    pItem->create(pPC->getName(), STORAGE_INVENTORY, 0, X, Y);

    // Leave an ItemTraceLog.
    if (pItem != NULL && pItem->isTraceItem()) {
        remainTraceLog(pItem, pCreature1->getName(), pCreature2->getName(), ITEM_LOG_CREATE, DETAIL_EVENTNPC);
    }

    // Tell the client the gift was added.
    GCCreateItem gcCreateItem;
    makeGCCreateItem(&gcCreateItem, pItem, X, Y);
    pPlayer->sendPacket(&gcCreateItem);

    // Turn the flag on.
    pFlagSet->turnOn(FLAGSET_TRADE_GIFT_BOX_2002_12);

    // Save the flag.
    pFlagSet->save(pPC->getName());

    // Tell the client the item exchange took place.
    GCNPCResponse response;
    response.setCode(NPC_RESPONSE_TRADE_GIFT_BOX_OK);
    pPlayer->sendPacket(&response);

    GCNPCResponse quit;
    quit.setCode(NPC_RESPONSE_QUIT_DIALOGUE);
    pPlayer->sendPacket(&quit);
}


////////////////////////////////////////////////////////////////////////////////
// get debug string
////////////////////////////////////////////////////////////////////////////////
string ActionTradeGiftBox::toString() const

{
    StringStream msg;
    msg << "ActionTradeGiftBox(" << ")";
    return msg.toString();
}
