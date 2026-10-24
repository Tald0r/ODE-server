////////////////////////////////////////////////////////////////////////////////
// Filename    : ActionGiveTestServerReward.cpp
// Description :
////////////////////////////////////////////////////////////////////////////////
#include "ActionGiveTestServerReward.h"

#include <list>

#include "DB.h"
#include "GCCreateItem.h"
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
#include "Slayer.h"
#include "SpecialEventFlag.h"
#include "Thread.h"
#include "Vampire.h"
#include "Zone.h"


////////////////////////////////////////////////////////////////////////////////
// ActionGiveTestServerReward
////////////////////////////////////////////////////////////////////////////////
ActionGiveTestServerReward::ActionGiveTestServerReward()
    : m_pLuaState(NULL), m_pLuaSlayerItem(NULL), m_pLuaVampireItem(NULL) {
    m_pLuaState = new LuaState;
    m_pLuaState->init();

    m_pLuaSlayerItem = new LuaTradeEventSlayerItem(m_pLuaState);
    m_pLuaVampireItem = new LuaTradeEventVampireItem(m_pLuaState);
}

////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////
ActionGiveTestServerReward::~ActionGiveTestServerReward() {
    try {
        SAFE_DELETE(m_pLuaState);
        SAFE_DELETE(m_pLuaSlayerItem);
        SAFE_DELETE(m_pLuaVampireItem);
    } catch (Throwable&) {
        // A Throwable from the try block is dropped here and goes no further.
    }
}

////////////////////////////////////////////////////////////////////////////////
//  load
////////////////////////////////////////////////////////////////////////////////
void ActionGiveTestServerReward::load()

{
    Assert(m_pLuaState != NULL);

    int result = m_pLuaState->dofile(m_CommonFilename);
    LuaState::logError(result);
}

////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////
void ActionGiveTestServerReward::read(PropertyBuffer& propertyBuffer)

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
void ActionGiveTestServerReward::execute(Creature* pCreature1, Creature* pCreature2)

{
    Assert(pCreature1 != NULL);
    Assert(pCreature2 != NULL);
    Assert(pCreature1->isNPC());
    Assert(pCreature2->isPC());

    PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pCreature2);
    Assert(pPC != NULL);

    Player* pPlayer = pPC->getPlayer();
    Assert(pPlayer != NULL);

    GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(pPlayer);
    Assert(pGamePlayer != NULL);

    Inventory* pInventory = pPC->getInventory();
    Assert(pInventory != NULL);

    Zone* pZone = pPC->getZone();
    Assert(pZone != NULL);

    Item::ItemClass ItemClass;
    ItemType_t ItemType;
    OptionType_t OptionType;

    Item* pItem;

    // Check whether the reward can be received.
    if (!(pGamePlayer->getSpecialEventCount() & SPECIAL_EVENT_TEST_SERVER_REWARD)) {
        GCNPCResponse response;
        response.setCode(NPC_RESPONSE_REWARD_FAIL);
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
        filelog("TestServerRewardError.txt", "[ No item selector ] : %s", pPC->getName().c_str());

        GCNPCResponse quit;
        quit.setCode(NPC_RESPONSE_QUIT_DIALOGUE);
        pPlayer->sendPacket(&quit);

        return;
    }

    // Create the item from the result Lua computes.
    pLuaSelectItem->prepare();

    int result = pLuaSelectItem->executeFile(luaFileName);
    LuaState::logError(result);

    ItemClass = pLuaSelectItem->getItemClass();
    ItemType = pLuaSelectItem->getItemType();
    OptionType = pLuaSelectItem->getOptionType();

    pLuaSelectItem->clear();

    if (ItemClass >= Item::ITEM_CLASS_MAX) {
        filelog("TestServerRewardError.txt", "[ ItemInfo Error ] : ItemClass = %d , ItemType = %d , OptionType = %d",
                ItemClass, ItemType, OptionType);

        GCNPCResponse quit;
        quit.setCode(NPC_RESPONSE_QUIT_DIALOGUE);
        pPlayer->sendPacket(&quit);

        return;
    }

    // Build the gift item.
    list<OptionType_t> optionTypeList;
    if (OptionType != 0)
        optionTypeList.push_back(OptionType);

    pItem = context().itemFactories().createItem(ItemClass, ItemType, optionTypeList);
    Assert(pItem != NULL);

    // Get an empty inventory slot for the item.
    TPOINT p;

    if (!pInventory->getEmptySlot(pItem, p)) {
        SAFE_DELETE(pItem);

        GCNPCResponse response;
        response.setCode(NPC_RESPONSE_NO_EMPTY_SLOT);
        pGamePlayer->sendPacket(&response);

        GCNPCResponse quit;
        quit.setCode(NPC_RESPONSE_QUIT_DIALOGUE);
        pPlayer->sendPacket(&quit);

        return;
    }

    // Add the gift to the inventory.
    pZone->getObjectRegistry().registerObject(pItem);
    pInventory->addItem(p.x, p.y, pItem);
    pItem->create(pPC->getName(), STORAGE_INVENTORY, 0, p.x, p.y);

    // Leave an ItemTraceLog.
    if (pItem != NULL && pItem->isTraceItem()) {
        remainTraceLog(pItem, pCreature1->getName(), pCreature2->getName(), ITEM_LOG_CREATE, DETAIL_EVENTNPC);
    }

    // Tell the client the gift was added.
    GCCreateItem gcCreateItem;
    makeGCCreateItem(&gcCreateItem, pItem, p.x, p.y);
    pPlayer->sendPacket(&gcCreateItem);

    // Turn off the flag now that the gift has been received.
    pGamePlayer->setSpecialEventCount(pGamePlayer->getSpecialEventCount() & ~(SPECIAL_EVENT_TEST_SERVER_REWARD));
    // Save the flag.
    pGamePlayer->saveSpecialEventCount();

    // Tell the client the reward was received.
    GCNPCResponse response;
    response.setCode(NPC_RESPONSE_REWARD_OK);
    pPlayer->sendPacket(&response);

    GCNPCResponse quit;
    quit.setCode(NPC_RESPONSE_QUIT_DIALOGUE);
    pPlayer->sendPacket(&quit);
}


////////////////////////////////////////////////////////////////////////////////
// get debug string
////////////////////////////////////////////////////////////////////////////////
string ActionGiveTestServerReward::toString() const

{
    StringStream msg;
    msg << "ActionGiveTestServerReward(" << ")";
    return msg.toString();
}
