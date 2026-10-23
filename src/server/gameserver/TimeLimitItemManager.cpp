#include "TimeLimitItemManager.h"

#include "GCTimeLimitItemInfo.h"
#include "Item.h"
#include "PlayerCreature.h"
#include "repository/ItemRepository.h"

TimeLimitItemManager::~TimeLimitItemManager() {
    TableRecordList::iterator itr = m_TableRecords.begin();

    for (; itr != m_TableRecords.end(); ++itr) {
        SAFE_DELETE(*itr);
    }

    m_TableRecords.clear();
    m_ItemTimeLimits.clear();
    m_loaded = false;
}

void TimeLimitItemManager::load()

{
    vector<TimeLimitItemRow> rows = defaultItemRepository().loadTimeLimitItems(m_pOwnerPC->getName(), (uint)VALID);

    for (size_t r = 0; r < rows.size(); r++) {
        TableRecord* pTableRecord = new TableRecord;

        pTableRecord->m_ItemClass = (Item::ItemClass)rows[r].itemClass;
        pTableRecord->m_ItemID = rows[r].itemID;

        const string limitDateTime = rows[r].limitDateTime;
        pTableRecord->m_TimeLimit = VSDateTime(limitDateTime);

        m_TableRecords.push_back(pTableRecord);
    }

    m_loaded = true;
}

// Compares the item against the table and registers its object id.
// When true is returned the object id has been registered.
// When false is returned it has not, so m_ItemTimeLimits[pItem->ObjectID()]
// throws.
bool TimeLimitItemManager::registerItem(Item* pItem)

{
    if (m_ItemTimeLimits.find(pItem->getObjectID()) != m_ItemTimeLimits.end())
        return false;

    TableRecordList::iterator itr = m_TableRecords.begin();
    TableRecordList::iterator endItr = m_TableRecords.end();

    for (; itr != endItr; ++itr) {
        TableRecord* pTableRecord = *itr;
        if (pTableRecord != NULL) {
            if (pTableRecord->m_ItemClass == pItem->getItemClass() && pTableRecord->m_ItemID == pItem->getItemID()) {
                m_ItemTimeLimits[pItem->getObjectID()] = pTableRecord->m_TimeLimit;
                pItem->setTimeLimitItem();
                return true;
            }
        }
    }

    return false;
}

bool TimeLimitItemManager::checkTimeLimit(Item* pItem)

{
    Assert(m_loaded);

    ObjectID_t objectID = pItem->getObjectID();

    ItemTimeLimitMap::iterator itr = m_ItemTimeLimits.find(objectID);

    if (itr == m_ItemTimeLimits.end()) {
        if (!registerItem(pItem)) {
            // Not a time-limited item,
            // so its lifetime is unlimited.
            return true;
        }
    }

    VSDateTime currentTime = VSDateTime::currentDateTime();
    if (currentTime > m_ItemTimeLimits[objectID]) {
        cout << pItem->toString() << " time limit exceeded : " << currentTime.toString() << " > "
             << m_ItemTimeLimits[objectID].toString() << endl;
        // The time limit has passed.
        return false;
    }

    return true;
}

bool TimeLimitItemManager::wasteIfTimeOver(Item* pItem)

{
    if (checkTimeLimit(pItem))
        return false;

    if (!defaultItemRepository().updateTimeLimitItemStatus((uint)EXPIRED, m_pOwnerPC->getName(),
                                                           (uint)pItem->getItemClass(), (uint)pItem->getItemID())) {
        return false;
    }

    return true;
}

bool TimeLimitItemManager::makeTimeLimitItemInfo(GCTimeLimitItemInfo& gcTLII) const

{
    Assert(m_loaded);

    if (m_ItemTimeLimits.empty())
        return false;

    //	gcTLII.clear();

    VSDateTime currentTime = VSDateTime::currentDateTime();
    ItemTimeLimitMap::const_iterator itr = m_ItemTimeLimits.begin();
    ItemTimeLimitMap::const_iterator endItr = m_ItemTimeLimits.end();

    for (; itr != endItr; ++itr) {
        int diffSecs = currentTime.secsTo(itr->second);

        if (diffSecs < 0)
            diffSecs = 0;

        gcTLII.addTimeLimit(itr->first, diffSecs);
    }

    return true;
}

bool TimeLimitItemManager::updateItemTimeLimit(Item* pItem, DWORD time) {
    if (pItem->getCreateType() != Item::CREATE_TYPE_TIME_EXTENSION)
        return false;
    if (!changeStatus(pItem, EXTENDED))
        return false;
    addTimeLimitItem(pItem, time);
    return true;
}

// Must be called only after the item has been registered in the zone.
void TimeLimitItemManager::addTimeLimitItem(Item* pItem, DWORD time)

{
    Assert(pItem != NULL);
    Assert(time != 0);

    VSDateTime timeLimit = VSDateTime::currentDateTime().addSecs(time);

    defaultItemRepository().insertTimeLimitItem(m_pOwnerPC->getName(), (uint)pItem->getItemClass(),
                                                (uint)pItem->getItemID(), timeLimit.toDateTime());

    TableRecord* pTableRecord = new TableRecord;

    pTableRecord->m_ItemClass = pItem->getItemClass();
    pTableRecord->m_ItemID = pItem->getItemID();
    pTableRecord->m_TimeLimit = timeLimit;

    m_TableRecords.push_back(pTableRecord);

    registerItem(pItem);
}

// An entry still in memory is one whose time has not run out yet.
bool TimeLimitItemManager::changeStatus(Item* pItem, TimeLimitStatus status) {
    Assert(status != VALID);

    if (!defaultItemRepository().updateTimeLimitItemStatus((uint)status, m_pOwnerPC->getName(),
                                                           (uint)pItem->getItemClass(), (uint)pItem->getItemID())) {
        return false;
    }

    TableRecordList::iterator itr = m_TableRecords.begin();
    TableRecordList::iterator endItr = m_TableRecords.end();

    bool erased = false;

    for (; itr != endItr; ++itr) {
        if ((*itr)->m_ItemClass == pItem->getItemClass() && (*itr)->m_ItemID == pItem->getItemID()) {
            m_TableRecords.erase(itr);
            erased = true;
            break;
        }
    }

    if (!erased)
        filelog("QuestItem.log", "[%u,%u] : Deleted a time-limited item from the table, but it is not in memory.",
                (uint)pItem->getItemClass(), (uint)pItem->getItemID());

    // An ObjectID of 0 breaks this. Selling an item only happens inside a zone, so it cannot be 0 here.
    ItemTimeLimitMap::iterator itr2 = m_ItemTimeLimits.find(pItem->getObjectID());

    if (itr2 != m_ItemTimeLimits.end()) {
        m_ItemTimeLimits.erase(itr2);
    } else {
        filelog("QuestItem.log", "[%u,%u] : Not in the Item Time Limit Map either.", (uint)pItem->getItemClass(),
                (uint)pItem->getItemID());
    }

    return true;
}
