//////////////////////////////////////////////////////////////////////////////
// Filename    : NPC.cpp
// Description :
// Originally there was no shop handling; deriving a shop NPC from this class
// was awkward, so the shop interface was put directly into NPC.
//////////////////////////////////////////////////////////////////////////////

#include "NPC.h"

#include "CastleInfoManager.h"
#include "GCNPCAskDynamic.h"
#include "GameContext.h"
#include "PlayerCreature.h"
#include "ShopRack.h"
#include "couple/PartnerWaitingManager.h"
#include "mission/QuestInfoManager.h"
#include "mission/RewardClassInfoManager.h"

//////////////////////////////////////////////////////////////////////////////
// class NPC member methods
//////////////////////////////////////////////////////////////////////////////

NPC::NPC()

{
    m_pInventory = NULL;

    // Set the turn at which it is processed next.
    getCurrentTime(m_NextTurn);
    m_NextTurn.tv_sec += rand() % 3;
    m_NextTurn.tv_usec += rand() % 1000000;
    if (m_NextTurn.tv_usec >= 1000000)
        m_NextTurn.tv_sec++;

    // Create the inventory object.
    // m_pInventory = new Inventory(5,5);

    // Create the shop rack object.
    m_pRack = new ShopRack[SHOP_RACK_TYPE_MAX];
    Assert(m_pRack != NULL);

    // Set the shop market variables to their defaults.
    m_MarketCondBuy = 25;
    m_MarketCondSell = 100;

    // A shop is a normal shop by default.
    m_ShopType = SHOPTYPE_NORMAL;

    m_ClanType = 0;

    // A meaningless default.
    // Guards against VisionInfo looking up Sight by mistake.
    // by sigi. 2002.9.6
    m_Sight = 5;

    // m_pQuestBoard = NULL;

    m_pCoupleRegisterManager = NULL;
    m_pCoupleUnregisterManager = NULL;

    m_pQuestInfoManager = NULL;
    m_pRewardClassInfoManager = NULL;

    m_TaxingCastleZoneID = 0;
}

NPC::NPC(const string& name)

{
    m_Name = name;
    m_pInventory = NULL;

    // Set the turn at which it is processed next.
    getCurrentTime(m_NextTurn);
    m_NextTurn.tv_sec += rand() % 3;
    m_NextTurn.tv_usec += rand() % 1000000;
    if (m_NextTurn.tv_usec >= 1000000)
        m_NextTurn.tv_sec++;

    // Create the inventory object.
    // m_pInventory = new Inventory(5,5);

    // Create the shop rack object.
    m_pRack = new ShopRack[SHOP_RACK_TYPE_MAX];
    Assert(m_pRack != NULL);

    // Set the shop market variables to their defaults.
    m_MarketCondBuy = 25;
    m_MarketCondSell = 100;

    m_pCoupleRegisterManager = NULL;
    m_pCoupleUnregisterManager = NULL;

    m_pQuestInfoManager = NULL;
    m_pRewardClassInfoManager = NULL;
}

NPC::~NPC()

{
    try {
        SAFE_DELETE(m_pCoupleUnregisterManager);
        SAFE_DELETE(m_pCoupleRegisterManager);

        SAFE_DELETE(m_pQuestInfoManager);
        SAFE_DELETE(m_pRewardClassInfoManager);

        SAFE_DELETE(m_pInventory);
        SAFE_DELETE_ARRAY(m_pRack);
    } catch (Throwable&) {
        // A Throwable from the try block is dropped here and goes no further.
    }
}

// registerObject()
// Allocates the ObjectIDs of the NPC and its owned items using the
// ObjectRegistry that belongs to the Zone.
// For now the only thing to register is the NPC's own OID.
void NPC::registerObject()

{
    Assert(getZone() != NULL);

    ObjectRegistry& OR = getZone()->getObjectRegistry();

    __ENTER_CRITICAL_SECTION(OR)

    OR.registerObject_NOLOCKED(this);

    __LEAVE_CRITICAL_SECTION(OR)
}

// load()
// Loads the data related to this NPC.
// Scripts, triggers and so on.
bool NPC::load()

{
    // Load the triggers.
    m_TriggerManager.load(m_Name);

    // If there is an AtFirst condition, run it and delete it.
    if (m_TriggerManager.hasCondition(Condition::CONDITION_AT_FIRST)) {
        list<Trigger*>& triggers = m_TriggerManager.getTriggers();
        list<Trigger*>::iterator itr = triggers.begin();

        for (; itr != triggers.end(); itr++) {
            Trigger* pTrigger = *itr;
            if (pTrigger->hasCondition(Condition::CONDITION_AT_FIRST)) {
                pTrigger->activate(this);
                triggers.erase(itr);
                break;
            }
        }
    }

    return true;
}

// init()
// Initializes the data and registers the NPC in the zone.
void NPC::init()

{
    load();
    // registerObject();
}

// act()
// Called once per main loop of the game server.
// This is the main function in which the AI code runs.
void NPC::act(const Timeval& currentTime)

{
    __BEGIN_DEBUG

    // Do nothing until the next turn time has been reached.
    if (currentTime < m_NextTurn)
        return;

    // Set the next turn.
    // NPCs currently move only one tile per second.
    Timeval delay;
    delay.tv_sec = 0;
    delay.tv_usec = 750000 + rand() % 200000;
    m_NextTurn = m_NextTurn + delay;

    if (m_pCoupleRegisterManager != NULL)
        m_pCoupleRegisterManager->heartbeat();
    if (m_pCoupleUnregisterManager != NULL)
        m_pCoupleUnregisterManager->heartbeat();

    // When it has triggers, check whether the conditions of each trigger are met.
    // If every condition of a particular trigger is met, run the action for that
    // trigger and return, because a trigger is an action too and an action runs
    // only once per turn.
    list<Trigger*>& triggers = m_TriggerManager.getTriggers();
    list<Trigger*>::iterator itr = triggers.begin();

    for (; itr != triggers.end(); itr++) {
        Trigger* pTrigger = *itr;
        if (pTrigger->isAllSatisfied(Trigger::ACTIVE_TRIGGER, this)) {
            pTrigger->activate(this);
            return;
        }
    }

    __END_DEBUG
}

// getShopVersion()
// Returns the version of the shop of the given type.
ShopVersion_t NPC::getShopVersion(ShopRackType_t type) const {
    Assert(type < SHOP_RACK_TYPE_MAX);
    return m_pRack[type].getVersion();
}

// setShopVersion()
// Sets the version of the shop of the given type.
void NPC::setShopVersion(ShopRackType_t type, ShopVersion_t ver) {
    Assert(type < SHOP_RACK_TYPE_MAX);
    m_pRack[type].setVersion(ver);
}

// increaseShopVersion()
// Raises the version of the shop of the given type.
void NPC::increaseShopVersion(ShopRackType_t type) {
    Assert(type < SHOP_RACK_TYPE_MAX);
    m_pRack[type].increaseVersion();
}

// isExistShopItem()
// Returns whether the item of the given type and index exists in the shop rack.
bool NPC::isExistShopItem(ShopRackType_t type, BYTE index) const {
    Assert(type < SHOP_RACK_TYPE_MAX);
    return m_pRack[type].isExist(index);
}

// insertShopItem()
// Puts an item into the rack of the given type and index.
void NPC::insertShopItem(ShopRackType_t type, BYTE index, Item* pItem) {
    Assert(type < SHOP_RACK_TYPE_MAX);
    m_pRack[type].insert(index, pItem);
}

// removeShopItem()
// Removes the item from the rack of the given type and index.
void NPC::removeShopItem(ShopRackType_t type, BYTE index) {
    Assert(type < SHOP_RACK_TYPE_MAX);
    return m_pRack[type].remove(index);
}

// getShopItem()
// Returns the pointer to the item in the rack of the given type and index.
Item* NPC::getShopItem(ShopRackType_t type, BYTE index) const {
    Assert(type < SHOP_RACK_TYPE_MAX);
    return m_pRack[type].get(index);
}

// clearShopItem()
// Clears the rack.
void NPC::clearShopItem(void) {
    for (int i = 0; i < SHOP_RACK_TYPE_MAX; i++)
        m_pRack[i].clear();
}

// getFirstEmptySlot()
// Finds the frontmost empty slot in the rack of the given type.
BYTE NPC::getFirstEmptySlot(ShopRackType_t type) const {
    Assert(type < SHOP_RACK_TYPE_MAX);
    return m_pRack[type].getFirstEmptySlot();
}

// getLastEmptySlot()
// Finds the rearmost empty slot in the rack of the given type.
BYTE NPC::getLastEmptySlot(ShopRackType_t type) const {
    Assert(type < SHOP_RACK_TYPE_MAX);
    return m_pRack[type].getLastEmptySlot();
}

// isFull()
// Returns whether the rack of the given type is currently full.
bool NPC::isFull(ShopRackType_t type) const {
    Assert(type < SHOP_RACK_TYPE_MAX);
    return m_pRack[type].isFull();
}

// isEmpty
// Returns whether the rack of the given type is currently empty.
bool NPC::isEmpty(ShopRackType_t type) const {
    Assert(type < SHOP_RACK_TYPE_MAX);
    return m_pRack[type].isEmpty();
}

int NPC::getTaxRatio(PlayerCreature* pPC) const {
    return de::gameContext().castleInfos().getItemTaxRatio(pPC, this);
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
string NPC::toString() const

{
    StringStream msg;
    msg << "NPC(" << "ObjectID:" << (int)m_ObjectID << ",SpriteType:" << (int)m_SpriteType << ",Name:" << m_Name
        << ",MainColor:" << (int)m_MainColor << ",SubColor:" << (int)m_SubColor << ",X:" << (int)m_X
        << ",Y:" << (int)m_Y << ",Sight:" << (int)m_Sight << m_TriggerManager.toString() << ")";
    return msg.toString();
}
