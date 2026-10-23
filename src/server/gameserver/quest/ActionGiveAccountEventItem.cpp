////////////////////////////////////////////////////////////////////////////////
// Filename    : ActionGiveAccountEventItem.cpp
// Written By  :
// Description :
////////////////////////////////////////////////////////////////////////////////
#include "ActionGiveAccountEventItem.h"

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
#include "VariableManager.h"
#include "Zone.h"


////////////////////////////////////////////////////////////////////////////////
// ActionGiveAccountEventItem
////////////////////////////////////////////////////////////////////////////////
ActionGiveAccountEventItem::ActionGiveAccountEventItem()
    : m_pLuaState(NULL), m_pLuaSlayerItem(NULL), m_pLuaVampireItem(NULL) {
    m_pLuaState = new LuaState;
    m_pLuaState->init();

    m_pLuaSlayerItem = new LuaTradeEventSlayerItem(m_pLuaState);
    m_pLuaVampireItem = new LuaTradeEventVampireItem(m_pLuaState);
}

////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////
ActionGiveAccountEventItem::~ActionGiveAccountEventItem() {
    __BEGIN_TRY

    SAFE_DELETE(m_pLuaState);
    SAFE_DELETE(m_pLuaSlayerItem);
    SAFE_DELETE(m_pLuaVampireItem);

    __END_CATCH_NO_RETHROW
}

////////////////////////////////////////////////////////////////////////////////
//  load
////////////////////////////////////////////////////////////////////////////////
void ActionGiveAccountEventItem::load()

{
    Assert(m_pLuaState != NULL);

    int result = m_pLuaState->dofile(m_CommonFilename);
    LuaState::logError(result);
}

////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////
void ActionGiveAccountEventItem::read(PropertyBuffer& propertyBuffer)

{
    try {
        // read script id
        m_SpecialEventFlag = (SpecialEventFlag)propertyBuffer.getPropertyInt("SpecialEventFlag");
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
void ActionGiveAccountEventItem::execute(Creature* pCreature1, Creature* pCreature2)

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
    OptionType_t OptionType2;

    Item* pItem;

    // The event is not running.
    if (!context().variables().isActiveGiveEventItem()) {
        GCNPCResponse response;
        response.setCode(NPC_RESPONSE_GIVE_EVENT_ITEM_FAIL_NOW);
        pPlayer->sendPacket(&response);

        GCNPCResponse quit;
        quit.setCode(NPC_RESPONSE_QUIT_DIALOGUE);
        pPlayer->sendPacket(&quit);

        return;
    }

    // Check whether the item has already been received.
    if (pGamePlayer->getSpecialEventCount() & m_SpecialEventFlag) {
        GCNPCResponse response;
        response.setCode(NPC_RESPONSE_GIVE_EVENT_ITEM_FAIL);
        pPlayer->sendPacket(&response);

        GCNPCResponse quit;
        quit.setCode(NPC_RESPONSE_QUIT_DIALOGUE);
        pPlayer->sendPacket(&quit);

        return;
    }

    // Check whether the account is a paying user.
    if (!PaySystem::isPayPlayingPeriodPersonal(pGamePlayer->getID())) {
        GCNPCResponse response;
        response.setCode(NPC_RESPONSE_GIVE_PREMIUM_USER_ONLY);
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
        filelog("AccountEventItemError.txt", "[ No item selector ] : %s", pPC->getName().c_str());

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
    OptionType2 = pLuaSelectItem->getOptionType2();

    pLuaSelectItem->clear();

    if (ItemClass >= Item::ITEM_CLASS_MAX) {
        filelog("AccountEventItemError.txt", "[ ItemInfo Error ] : ItemClass = %d , ItemType = %d , OptionType = %d",
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
    if (OptionType2 != 0)
        optionTypeList.push_back(OptionType2);

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

        remainTraceLogNew(pItem, pCreature2->getName(), ITL_GET, ITLD_EVENTNPC, pCreature1->getZone()->getZoneID(),
                          pCreature1->getX(), pCreature1->getY());
    }

    // Tell the client the gift was added.
    GCCreateItem gcCreateItem;
    makeGCCreateItem(&gcCreateItem, pItem, p.x, p.y);
    pPlayer->sendPacket(&gcCreateItem);

    // Turn on the flag marking the gift as received.
    pGamePlayer->setSpecialEventCount(pGamePlayer->getSpecialEventCount() | m_SpecialEventFlag);
    // Save the flag.
    pGamePlayer->saveSpecialEventCount();

    // Tell the client the reward was received.
    GCNPCResponse response;
    response.setCode(NPC_RESPONSE_GIVE_EVENT_ITEM_OK);
    pPlayer->sendPacket(&response);

    GCNPCResponse quit;
    quit.setCode(NPC_RESPONSE_QUIT_DIALOGUE);
    pPlayer->sendPacket(&quit);
}


////////////////////////////////////////////////////////////////////////////////
// get debug string
////////////////////////////////////////////////////////////////////////////////
string ActionGiveAccountEventItem::toString() const

{
    StringStream msg;
    msg << "ActionGiveAccountEventItem(" << ")";
    return msg.toString();
}
