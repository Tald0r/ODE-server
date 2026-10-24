////////////////////////////////////////////////////////////////////////////////
// Filename    : TradeManager.cpp
// Description :
////////////////////////////////////////////////////////////////////////////////

#include "TradeManager.h"

#include "Creature.h"
#include "EventItemUtil.h"
#include "FlagSet.h"
#include "GCTradeFinish.h"
#include "GameContext.h"
#include "GamePlayer.h"
#include "Inventory.h"
#include "Item.h"
#include "ItemMap.h"
#include "ItemUtil.h"
#include "Ousters.h"
#include "Player.h"
#include "Slayer.h"
#include "VSDateTime.h"
#include "Vampire.h"
#include "VariableManager.h"
#include "repository/PlayRecordRepository.h"

////////////////////////////////////////////////////////////////////////////////
// class TradeInfo member methods
////////////////////////////////////////////////////////////////////////////////

TradeInfo::TradeInfo()

{
    m_Gold = 0;
    m_Status = TRADE_TRADING;

    Timeval currentTime;
    getCurrentTime(currentTime);
    m_LastOKTime = currentTime;
}

TradeInfo::~TradeInfo()

{
    m_ItemList.clear();
}

bool TradeInfo::hasItem(Item* pItem)

{
    list<Item*>::const_iterator itr = m_ItemList.begin();
    for (; itr != m_ItemList.end(); itr++) {
        if (pItem == (*itr))
            return true;
    }
    return false;
}

bool TradeInfo::addItem(Item* pItem)

{
    if (hasItem(pItem))
        return false;
    m_ItemList.push_back(pItem);
    return true;
}

bool TradeInfo::removeItem(Item* pItem)

{
    list<Item*>::iterator itr = m_ItemList.begin();
    for (; itr != m_ItemList.end(); itr++) {
        if (pItem == (*itr)) {
            m_ItemList.erase(itr);
            return true;
        }
    }
    return false;
}

void TradeInfo::clearAll(void)

{
    m_ItemList.clear();
    m_Gold = 0;
    m_Status = TRADE_TRADING;
}

void TradeInfo::setNextTime(const Timeval& currentTime) {
    m_LastOKTime.tv_sec = (currentTime.tv_sec + 4);
    m_LastOKTime.tv_usec = currentTime.tv_usec;
}

bool TradeInfo::isValidOKTime(const Timeval& currentTime) {
    if (m_LastOKTime < currentTime)
        return true;
    return false;
}

////////////////////////////////////////////////////////////////////////////////
// class TradeManager member methods
////////////////////////////////////////////////////////////////////////////////

TradeManager::TradeManager()

{}

TradeManager::~TradeManager()

{
    unordered_map<string, TradeInfo*>::iterator itr = m_InfoMap.begin();
    for (; itr != m_InfoMap.end(); itr++) {
        TradeInfo* pInfo = itr->second;
        SAFE_DELETE(pInfo);
    }

    m_InfoMap.clear();
}

void TradeManager::init()

{}

bool TradeManager::hasTradeInfo(const string& Name)

{
    unordered_map<string, TradeInfo*>::iterator itr = m_InfoMap.find(Name);
    if (itr == m_InfoMap.end())
        return false;
    return true;
}

TradeInfo* TradeManager::getTradeInfo(const string& Name)

{
    unordered_map<string, TradeInfo*>::iterator itr = m_InfoMap.find(Name);
    if (itr == m_InfoMap.end())
        return NULL;
    return itr->second;
}

void TradeManager::addTradeInfo(TradeInfo* pInfo) {
    Assert(pInfo != NULL);
    unordered_map<string, TradeInfo*>::iterator itr = m_InfoMap.find(pInfo->getMainName());
    if (itr != m_InfoMap.end())
        throw DuplicatedException();
    m_InfoMap[pInfo->getMainName()] = pInfo;
}

void TradeManager::removeTradeInfo(const string& Name) {
    unordered_map<string, TradeInfo*>::iterator itr = m_InfoMap.find(Name);
    if (itr == m_InfoMap.end()) {
        cerr << "TradeManager::removeTradeInfo() : NoSuchElementException" << endl;
        // Ignored, as it carries no meaning.
        // by sigi. 2002.8.31
        // throw NoSuchElementException();
        return;
    }
    TradeInfo* pInfo = itr->second;
    m_InfoMap.erase(itr);
    SAFE_DELETE(pInfo);
}

void TradeManager::initTrade(Creature* pCreature1, Creature* pCreature2)

{
    // Neither creature may already have trade info.
    if (hasTradeInfo(pCreature1->getName()) || hasTradeInfo(pCreature2->getName())) {
        throw Error("TradeManager::initTrade() : Trade info already exist!");
    }

    TradeInfo* pInfo1 = new TradeInfo();
    pInfo1->setMainName(pCreature1->getName());
    pInfo1->setTargetName(pCreature2->getName());
    pInfo1->setStatus(TRADE_TRADING);
    addTradeInfo(pInfo1);

    TradeInfo* pInfo2 = new TradeInfo();
    pInfo2->setMainName(pCreature2->getName());
    pInfo2->setTargetName(pCreature1->getName());
    pInfo2->setStatus(TRADE_TRADING);
    addTradeInfo(pInfo2);
}

int TradeManager::canTrade(Creature* pCreature1, Creature* pCreature2)

{
    try {
        // The pointers must not be null.
        if (pCreature1 == NULL || pCreature2 == NULL)
            return 0;

        // Only PCs may trade.
        if (!pCreature1->isPC() || !pCreature2->isPC())
            return 0;

        // Different races cannot trade.
        if (!isSameRace(pCreature1, pCreature2))
            return 0;

        // Without trade info there is nothing to trade.
        if (!isTrading(pCreature1, pCreature2))
            return 0;

        TradeInfo* pInfo1 = getTradeInfo(pCreature1->getName());
        TradeInfo* pInfo2 = getTradeInfo(pCreature2->getName());

        if (pInfo1 == NULL || pInfo2 == NULL) // by sigi. 2002.12.25
            return 0;

        // Both sides must have confirmed the trade.
        if (pInfo1->getStatus() != TRADE_FINISH || pInfo2->getStatus() != TRADE_FINISH)
            return 0;

        // Prepare the variables that are needed.
        list<Item*> tradeList1 = pInfo1->getItemList();
        list<Item*> tradeList2 = pInfo2->getItemList();
        ItemMap itemMap1;
        ItemMap itemMap2;
        Inventory* pInventory1 = NULL;
        Inventory* pInventory2 = NULL;

        bool bTradeGiftBox = false;
        int EventGiftBoxCount = 0;

        if (pCreature1->isSlayer() && pCreature2->isSlayer()) {
            Slayer* pSlayer1 = dynamic_cast<Slayer*>(pCreature1);
            Slayer* pSlayer2 = dynamic_cast<Slayer*>(pCreature2);

            pInventory1 = new Inventory(pSlayer1->getInventory());
            pInventory2 = new Inventory(pSlayer2->getInventory());
            pInventory1->setDeleteAllFlag(false);
            pInventory2->setDeleteAllFlag(false);
        } else if (pCreature1->isVampire() && pCreature2->isVampire()) {
            Vampire* pVampire1 = dynamic_cast<Vampire*>(pCreature1);
            Vampire* pVampire2 = dynamic_cast<Vampire*>(pCreature2);

            pInventory1 = new Inventory(pVampire1->getInventory());
            pInventory2 = new Inventory(pVampire2->getInventory());
            pInventory1->setDeleteAllFlag(false);
            pInventory2->setDeleteAllFlag(false);
        } else if (pCreature1->isOusters() && pCreature2->isOusters()) {
            Ousters* pOusters1 = dynamic_cast<Ousters*>(pCreature1);
            Ousters* pOusters2 = dynamic_cast<Ousters*>(pCreature2);

            pInventory1 = new Inventory(pOusters1->getInventory());
            pInventory2 = new Inventory(pOusters2->getInventory());
            pInventory1->setDeleteAllFlag(false);
            pInventory2->setDeleteAllFlag(false);
        } else
            throw Error("TradeManager::canTrade() : the races differ!");

        // First remove the items to be traded from each inventory.
        for (list<Item*>::iterator itr = tradeList1.begin(); itr != tradeList1.end(); itr++) {
            Item* pItem = (*itr);
            if (pInventory1->hasItem(pItem->getObjectID())) {
                // Gift box trade event: a gift box cannot be traded alongside other items -- cancel.
                if (pItem->getItemClass() == Item::ITEM_CLASS_EVENT_GIFT_BOX && pItem->getItemType() > 1 &&
                    pItem->getItemType() < 6) {
                    bTradeGiftBox = true;
                }

                if (pItem->getItemClass() == Item::ITEM_CLASS_EVENT_GIFT_BOX && pItem->getItemType() >= 16 &&
                    pItem->getItemType() <= 18) {
                    EventGiftBoxCount++;
                }

                pInventory1->deleteItem(pItem->getObjectID());
                itemMap1.addItem(pItem);
            } else
                goto ErrorCode;
        }

        for (list<Item*>::iterator itr = tradeList2.begin(); itr != tradeList2.end(); itr++) {
            Item* pItem = (*itr);
            if (pInventory2->hasItem(pItem->getObjectID())) {
                // Gift box trade event: a gift box cannot be traded alongside other items -- cancel.
                if (pItem->getItemClass() == Item::ITEM_CLASS_EVENT_GIFT_BOX && pItem->getItemType() > 1 &&
                    pItem->getItemType() < 6) {
                    // The trade cannot go ahead unless the other side also offered a GiftBox.
                    if (!bTradeGiftBox) {
                        SAFE_DELETE(pInventory1);
                        SAFE_DELETE(pInventory2);
                        return 2;
                    }

                    bTradeGiftBox = false;
                }

                if (pItem->getItemClass() == Item::ITEM_CLASS_EVENT_GIFT_BOX && pItem->getItemType() >= 16 &&
                    pItem->getItemType() <= 18) {
                    EventGiftBoxCount--;
                }

                pInventory2->deleteItem(pItem->getObjectID());
                itemMap2.addItem(pItem);
            } else
                goto ErrorCode;
        }

        // If only one side offered a GiftBox, bTradeGiftBox is still true here.
        if (bTradeGiftBox || EventGiftBoxCount != 0) {
            SAFE_DELETE(pInventory1);
            SAFE_DELETE(pInventory2);
            return 2;
        }

        // Now try adding the items to be traded into the other inventory.
        for (ItemMap::iterator itr = itemMap1.begin(); itr != itemMap1.end(); itr++) {
            Item* pItem = itr->second;
            // If any one item cannot be added, return false.
            if (!pInventory2->addItem(pItem))
                goto ErrorCode;
        }

        for (ItemMap::iterator itr = itemMap2.begin(); itr != itemMap2.end(); itr++) {
            Item* pItem = itr->second;
            // If any one item cannot be added, return false.
            if (!pInventory1->addItem(pItem))
                goto ErrorCode;
        }

        SAFE_DELETE(pInventory1);
        SAFE_DELETE(pInventory2);

        // If every item could be added, return true.
        return 1;

    ErrorCode:
        SAFE_DELETE(pInventory1);
        SAFE_DELETE(pInventory2);
        return 0;

    } catch (Throwable& t) { // by sigi. 2002.12.25

        filelog("tradeError.txt", "C1=%s, C2=%s, %s", pCreature1->getName().c_str(), pCreature2->getName().c_str(),
                t.toString().c_str());

        // It is enough to report that the trade is not possible.
        return 0;
    }
}

void TradeManager::processTrade(Creature* pCreature1, Creature* pCreature2)

{
    if (!canTrade(pCreature1, pCreature2)) {
        throw Error("TradeManager::processTrade() : the two cannot trade at all");
    }

    // Prepare the variables that are needed.
    TradeInfo* pInfo1 = getTradeInfo(pCreature1->getName());
    TradeInfo* pInfo2 = getTradeInfo(pCreature2->getName());
    list<Item*> tradeList1 = pInfo1->getItemList();
    list<Item*> tradeList2 = pInfo2->getItemList();
    ItemMap itemMap1;
    ItemMap itemMap2;
    Gold_t tradeGold1 = pInfo1->getGold();
    Gold_t tradeGold2 = pInfo2->getGold();
    Inventory* pInventory1 = NULL;
    Inventory* pInventory2 = NULL;
    Slayer* pSlayer1 = NULL;
    Slayer* pSlayer2 = NULL;
    Vampire* pVampire1 = NULL;
    Vampire* pVampire2 = NULL;
    Ousters* pOusters1 = NULL;
    Ousters* pOusters2 = NULL;
    PlayerCreature* pPlayerCreature1 = dynamic_cast<PlayerCreature*>(pCreature1);
    PlayerCreature* pPlayerCreature2 = dynamic_cast<PlayerCreature*>(pCreature2);

    bool check1 = pPlayerCreature1->checkDBGold(pPlayerCreature1->getGold() + tradeGold1);
    bool check2 = pPlayerCreature2->checkDBGold(pPlayerCreature2->getGold() + tradeGold2);

    if (!check1) {
        filelog("GoldBug.log", "TradeManager::processTrade : gold does not match. [%s:%s]",
                pPlayerCreature1->getName().c_str(), pPlayerCreature1->getPlayer()->getID().c_str());
    }

    if (!check2) {
        filelog("GoldBug.log", "TradeManager::processTrade : gold does not match. [%s:%s]",
                pPlayerCreature2->getName().c_str(), pPlayerCreature2->getPlayer()->getID().c_str());
    }

    if (!check1 || !check2) {
        GamePlayer* pGamePlayer1 = dynamic_cast<GamePlayer*>(pPlayerCreature1->getPlayer());
        GamePlayer* pGamePlayer2 = dynamic_cast<GamePlayer*>(pPlayerCreature2->getPlayer());

        if (pGamePlayer1 != NULL) {
            pGamePlayer1->setPenaltyFlag(PENALTY_TYPE_KICKED);
            pGamePlayer1->setItemRatioBonusPoint(1);
        }

        if (pGamePlayer2 != NULL) {
            pGamePlayer2->setPenaltyFlag(PENALTY_TYPE_KICKED);
            pGamePlayer2->setItemRatioBonusPoint(1);
        }

        return;
    }


    bool bTradeGiftBox = false;
    int giftBoxType1 = 0;
    int giftBoxType2 = 0;

    if (pCreature1->isSlayer() && pCreature2->isSlayer()) {
        pSlayer1 = dynamic_cast<Slayer*>(pCreature1);
        pSlayer2 = dynamic_cast<Slayer*>(pCreature2);

        pInventory1 = pSlayer1->getInventory();
        pInventory2 = pSlayer2->getInventory();
    } else if (pCreature1->isVampire() && pCreature2->isVampire()) {
        pVampire1 = dynamic_cast<Vampire*>(pCreature1);
        pVampire2 = dynamic_cast<Vampire*>(pCreature2);

        pInventory1 = pVampire1->getInventory();
        pInventory2 = pVampire2->getInventory();
    } else if (pCreature1->isOusters() && pCreature2->isOusters()) {
        pOusters1 = dynamic_cast<Ousters*>(pCreature1);
        pOusters2 = dynamic_cast<Ousters*>(pCreature2);

        pInventory1 = pOusters1->getInventory();
        pInventory2 = pOusters2->getInventory();
    } else
        throw Error("TradeManager::processTrade() : the two races cannot trade with each other");

    // First remove the items to be traded from each inventory.
    for (list<Item*>::iterator itr = tradeList1.begin(); itr != tradeList1.end(); itr++) {
        Item* pItem = (*itr);
        if (pInventory1->hasItem(pItem->getObjectID())) {
            // Gift box trade event: a gift box cannot be traded alongside other items -- cancel.
            if (pItem->getItemClass() == Item::ITEM_CLASS_EVENT_GIFT_BOX && pItem->getItemType() > 1 &&
                pItem->getItemType() < 6) {
                bTradeGiftBox = true;

                giftBoxType1 = pItem->getItemType();
            }

            pInventory1->deleteItem(pItem->getObjectID());
            itemMap1.addItem(pItem);
            pItem->whenPCLost(pPlayerCreature1);
        } else
            throw Error("TradeManager::processTrade() : there is no item on the table");
    }
    for (list<Item*>::iterator itr = tradeList2.begin(); itr != tradeList2.end(); itr++) {
        Item* pItem = (*itr);
        if (pInventory2->hasItem(pItem->getObjectID())) {
            // Gift box trade event: a gift box cannot be traded alongside other items -- cancel.
            if (pItem->getItemClass() == Item::ITEM_CLASS_EVENT_GIFT_BOX && pItem->getItemType() > 1 &&
                pItem->getItemType() < 6) {
                // The trade cannot go ahead unless the other side also offered a GiftBox.
                if (!bTradeGiftBox)
                    throw Error("TradeManager::processTrade() : a gift box trades only against another gift box");

                giftBoxType2 = pItem->getItemType();
            }

            pInventory2->deleteItem(pItem->getObjectID());
            itemMap2.addItem(pItem);
            pItem->whenPCLost(pPlayerCreature2);
        } else
            throw Error("TradeManager::processTrade() : there is no item on the table");
    }

    // Now try adding the items to be traded into the other inventory.
    for (ItemMap::iterator itr = itemMap1.begin(); itr != itemMap1.end(); itr++) {
        Item* pItem = itr->second;
        // If any one item cannot be added, the trade cannot complete.
        if (!pInventory2->addItem(pItem)) {
            throw Error("TradeManager::processTrade() : the trade failed halfway through");
        }

        pItem->whenPCTake(pPlayerCreature2);

        // Christmas event.
        // A green gift box becomes a red one.
        if (pItem->getItemClass() == Item::ITEM_CLASS_EVENT_GIFT_BOX && pItem->getItemType() == 0) {
            pItem->setItemType(1);

            // Set the flag for having received a green gift box.
            FlagSet* pFlagSet = pPlayerCreature2->getFlagSet();
            Assert(pFlagSet != NULL);
            pFlagSet->turnOn(FLAGSET_RECEIVE_GREEN_GIFT_BOX);
            pFlagSet->save(pCreature2->getName());
        }

        if (pItem->getItemClass() == Item::ITEM_CLASS_EVENT_GIFT_BOX && pItem->getItemType() > 1 &&
            pItem->getItemType() < 6) {
            int blackGiftBoxType = getBlackGiftBoxType(giftBoxType1, giftBoxType2);

            if (blackGiftBoxType != 0)
                pItem->setItemType(blackGiftBoxType);
            else
                throw Error("TradeManager::processTrade() : the trade failed halfway through");
        }

        if (pItem->getItemClass() == Item::ITEM_CLASS_EVENT_GIFT_BOX && pItem->getItemType() >= 16 &&
            pItem->getItemType() <= 18) {
            pItem->setItemType(pItem->getItemType() + 3);
        }

        // Leave an ItemTraceLog.
        if (pItem != NULL && pItem->isTraceItem()) {
            remainTraceLog(pItem, pCreature1->getName(), pCreature2->getName(), ITEM_LOG_TRADE, DETAIL_TRADE);
            remainTraceLogNew(pItem, pCreature1->getName(), ITL_DROP, ITLD_TRADE);
            remainTraceLogNew(pItem, pCreature2->getName(), ITL_GET, ITLD_TRADE);
        }
    }
    for (ItemMap::iterator itr = itemMap2.begin(); itr != itemMap2.end(); itr++) {
        Item* pItem = itr->second;
        // If any one item cannot be added, the trade cannot complete.
        if (!pInventory1->addItem(pItem)) {
            throw Error("TradeManager::processTrade() : the trade failed halfway through");
        }

        pItem->whenPCTake(pPlayerCreature1);

        // Christmas event.
        // A green gift box becomes a red one.
        if (pItem->getItemClass() == Item::ITEM_CLASS_EVENT_GIFT_BOX && pItem->getItemType() == 0) {
            pItem->setItemType(1);

            // Set the flag for having received a green gift box.
            FlagSet* pFlagSet = pPlayerCreature1->getFlagSet();
            Assert(pFlagSet != NULL);
            pFlagSet->turnOn(FLAGSET_RECEIVE_GREEN_GIFT_BOX);
            pFlagSet->save(pCreature1->getName());
        }

        if (pItem->getItemClass() == Item::ITEM_CLASS_EVENT_GIFT_BOX && pItem->getItemType() > 1 &&
            pItem->getItemType() < 6) {
            int blackGiftBoxType = getBlackGiftBoxType(giftBoxType1, giftBoxType2);

            if (blackGiftBoxType != 0)
                pItem->setItemType(blackGiftBoxType);
            else
                throw Error("TradeManager::processTrade() : the trade failed halfway through");
        }

        if (pItem->getItemClass() == Item::ITEM_CLASS_EVENT_GIFT_BOX && pItem->getItemType() >= 16 &&
            pItem->getItemType() <= 18) {
            pItem->setItemType(pItem->getItemType() + 3);
        }

        // Leave an ItemTraceLog.
        if (pItem != NULL && pItem->isTraceItem()) {
            remainTraceLog(pItem, pCreature2->getName(), pCreature1->getName(), ITEM_LOG_TRADE, DETAIL_TRADE);
            remainTraceLogNew(pItem, pCreature2->getName(), ITL_DROP, ITLD_TRADE);
            remainTraceLogNew(pItem, pCreature1->getName(), ITL_GET, ITLD_TRADE);
        }
    }

    // Save the moved items to the database.
    pInventory1->save(pCreature1->getName());
    pInventory2->save(pCreature2->getName());

    // Save the moved gold.
    if (pCreature1->isSlayer()) {
        pSlayer1->setGoldEx(pSlayer1->getGold() + tradeGold2);
        pSlayer2->setGoldEx(pSlayer2->getGold() + tradeGold1);
    } else if (pCreature2->isVampire()) {
        pVampire1->setGoldEx(pVampire1->getGold() + tradeGold2);
        pVampire2->setGoldEx(pVampire2->getGold() + tradeGold1);
    } else if (pCreature2->isOusters()) {
        pOusters1->setGoldEx(pOusters1->getGold() + tradeGold2);
        pOusters2->setGoldEx(pOusters2->getGold() + tradeGold1);
    }

    // Leave a log before the trade info is removed.
    string ip1 = pCreature1->getPlayer()->getSocket()->getHost();
    string ip2 = pCreature2->getPlayer()->getSocket()->getHost();
    StringStream msg;
    msg << "[" << pCreature1->getName() << "(" << pPlayerCreature1->getPlayer()->getID() << ")] " << "\n";
    msg << "GOLD:" << tradeGold1 << "\n";
    for (ItemMap::iterator itr = itemMap1.begin(); itr != itemMap1.end(); itr++)
        msg << itr->second->toString() << "\n";
    msg << "----" << "\n";
    msg << "[" << pCreature2->getName() << "(" << pPlayerCreature2->getPlayer()->getID() << ")] " << "\n";
    msg << "GOLD:" << tradeGold2 << "\n";
    for (ItemMap::iterator itr = itemMap2.begin(); itr != itemMap2.end(); itr++)
        msg << itr->second->toString() << "\n";

    // Leave a money log.
    if (tradeGold1 >= de::gameContext().variables().getMoneyTraceLogLimit()) {
        remainMoneyTraceLog(pCreature1->getName(), pCreature2->getName(), ITEM_LOG_TRADE, DETAIL_TRADE, tradeGold1);
    }

    if (tradeGold2 >= de::gameContext().variables().getMoneyTraceLogLimit()) {
        remainMoneyTraceLog(pCreature2->getName(), pCreature1->getName(), ITEM_LOG_TRADE, DETAIL_TRADE, tradeGold2);
    }

    defaultPlayRecordRepository().logPlayerTrade(VSDateTime::currentDateTime().toString(), pCreature1->getName(), ip1,
                                                 pCreature2->getName(), ip2, msg.toString());

    // The trade is over, so the trade info would be removed here.
    // *** Left out because the client side wants the trade
    // window to stay open after the trade finishes.
    // removeTradeInfo(pCreature1->getObjectID());
    // removeTradeInfo(pCreature2->getObjectID());
    pInfo1->clearAll();
    pInfo2->clearAll();
}

////////////////////////////////////////////////////////////////////////////////
// Cancel the trade and put back the items that were marked for it.
// In practice only the gold has to be put back.
////////////////////////////////////////////////////////////////////////////////
void TradeManager::cancelTrade(Creature* pCreature1, Creature* pCreature2)

{
    try {
        int nCondition = 0;

        // The pointers must not be null.
        if (pCreature1 == NULL || pCreature2 == NULL)
            nCondition = 1;

        // Only PCs may trade.
        if (!pCreature1->isPC() || !pCreature2->isPC())
            nCondition = 2;

        // Different races cannot trade.
        if (!isSameRace(pCreature1, pCreature2))
            nCondition = 3;

        // Without trade info there is nothing to trade.
        if (!isTrading(pCreature1, pCreature2))
            nCondition = 4;

        if (nCondition != 0) {
            StringStream msg;
            msg << "TradeManager::cancelTrade() error... CODE(" << nCondition << ")";
            filelog("tradeError.txt", "[1] %s", msg.toString().c_str());
            // throw "TradeManager::cancelTrade() : what is this now?";

            return;
        }

        TradeInfo* pInfo1 = getTradeInfo(pCreature1->getName());
        TradeInfo* pInfo2 = getTradeInfo(pCreature2->getName());

        // Remove the trade info belonging to the first creature.
        if (pCreature1->isSlayer()) {
            Slayer* pSlayer1 = dynamic_cast<Slayer*>(pCreature1);
            if (pInfo1)
                pSlayer1->setGold(pSlayer1->getGold() + pInfo1->getGold());
        } else if (pCreature1->isVampire()) {
            Vampire* pVampire1 = dynamic_cast<Vampire*>(pCreature1);
            if (pInfo1)
                pVampire1->setGold(pVampire1->getGold() + pInfo1->getGold());
        } else if (pCreature1->isOusters()) {
            Ousters* pOusters1 = dynamic_cast<Ousters*>(pCreature1);
            if (pInfo1)
                pOusters1->setGold(pOusters1->getGold() + pInfo1->getGold());
        }

        // Remove the trade info belonging to the second creature.
        if (pCreature2->isSlayer()) {
            Slayer* pSlayer2 = dynamic_cast<Slayer*>(pCreature2);
            if (pInfo2)
                pSlayer2->setGold(pSlayer2->getGold() + pInfo2->getGold());
        } else if (pCreature2->isVampire()) {
            Vampire* pVampire2 = dynamic_cast<Vampire*>(pCreature2);
            if (pInfo2)
                pVampire2->setGold(pVampire2->getGold() + pInfo2->getGold());
        } else if (pCreature2->isOusters()) {
            Ousters* pOusters2 = dynamic_cast<Ousters*>(pCreature2);
            if (pInfo2)
                pOusters2->setGold(pOusters2->getGold() + pInfo2->getGold());
        }

        // Remove the trade info.
        if (pInfo1)
            removeTradeInfo(pCreature1->getName());
        if (pInfo2)
            removeTradeInfo(pCreature2->getName());

    } catch (Throwable& t) {
        filelog("tradeError.txt", "[1] %s", t.toString().c_str());
        // throw Error(t.toString().c_str());
    }
}

////////////////////////////////////////////////////////////////////////////////
// Cancel the trade and put back the items that were marked for it.
// In practice only the gold has to be put back.
////////////////////////////////////////////////////////////////////////////////
void TradeManager::cancelTrade(Creature* pPC)

{
    try {
        // The pointer must not be null.
        if (pPC == NULL)
            return;

        TradeInfo* pInfo1 = getTradeInfo(pPC->getName());
        TradeInfo* pInfo2 = NULL;
        string TargetName;
        Creature* pTargetPC = NULL;
        Zone* pZone = pPC->getZone();

        if (pInfo1 != NULL) {
            pInfo2 = getTradeInfo(pInfo1->getTargetName());
            TargetName = pInfo1->getTargetName();

            // Remove the trade info belonging to the first creature.
            if (pPC->isSlayer()) {
                Slayer* pSlayer1 = dynamic_cast<Slayer*>(pPC);
                pSlayer1->setGold(pSlayer1->getGold() + pInfo1->getGold());
            } else if (pPC->isVampire()) {
                Vampire* pVampire1 = dynamic_cast<Vampire*>(pPC);
                pVampire1->setGold(pVampire1->getGold() + pInfo1->getGold());
            } else if (pPC->isOusters()) {
                Ousters* pOusters1 = dynamic_cast<Ousters*>(pPC);
                pOusters1->setGold(pOusters1->getGold() + pInfo1->getGold());
            }

            pTargetPC = pZone->getCreature(TargetName);

            // If the trade partner is in the same zone, send them a packet
            // saying the trade was refused and remove
            // the trade info.
            if (pTargetPC != NULL && pTargetPC->isPC() && pInfo2 != NULL) {
                // Remove the trade info belonging to the second creature.
                if (pTargetPC->isSlayer()) {
                    Slayer* pSlayer2 = dynamic_cast<Slayer*>(pTargetPC);
                    pSlayer2->setGold(pSlayer2->getGold() + pInfo2->getGold());
                } else if (pTargetPC->isVampire()) {
                    Vampire* pVampire2 = dynamic_cast<Vampire*>(pTargetPC);
                    pVampire2->setGold(pVampire2->getGold() + pInfo2->getGold());
                } else if (pTargetPC->isOusters()) {
                    Ousters* pOusters2 = dynamic_cast<Ousters*>(pTargetPC);
                    pOusters2->setGold(pOusters2->getGold() + pInfo2->getGold());
                }

                // Send the trade-refused packet to the target.
                GCTradeFinish gcTradeFinish;
                gcTradeFinish.setTargetObjectID(pPC->getObjectID());
                gcTradeFinish.setCode(GC_TRADE_FINISH_REJECT);

                Player* pTargetPlayer = pTargetPC->getPlayer();
                pTargetPlayer->sendPacket(&gcTradeFinish);
            }

            // Remove the trade info.
            removeTradeInfo(pPC->getName());
            removeTradeInfo(TargetName);
        }

    } catch (Throwable& t) {
        filelog("tradeError.txt", "[2] %s", t.toString().c_str());
        // throw Error(t.toString().c_str());
    }
}

////////////////////////////////////////////////////////////////////////////////
// Check whether the two creatures are trading with each other.
////////////////////////////////////////////////////////////////////////////////
bool TradeManager::isTrading(Creature* pCreature1, Creature* pCreature2)

{
    TradeInfo* pInfo1 = getTradeInfo(pCreature1->getName());
    TradeInfo* pInfo2 = getTradeInfo(pCreature2->getName());

    if (pInfo1 == NULL || pInfo2 == NULL)
        return false;

    if ((pInfo1->getMainName() == pInfo2->getTargetName()) && (pInfo2->getMainName() == pInfo1->getTargetName()))
        return true;

    return false;
}
