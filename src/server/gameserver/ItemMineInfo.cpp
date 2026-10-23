//////////////////////////////////////////////////////////////////////////////
// Filename    : ItemMineInfo.h
// Written By  : bezz, sequoia, dew
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "ItemMineInfo.h"

#include "GameContext.h"
#include "ItemFactoryManager.h"
#include "ItemUtil.h"
#include "Treasure.h"
#include "Utility.h"
#include "repository/GameInfoRepository.h"

ItemMineInfo::ItemMineInfo() : m_ID(0) {}

ItemMineInfo::~ItemMineInfo(){__BEGIN_TRY __END_CATCH_NO_RETHROW}

Item* ItemMineInfo::getItem() {
    Item* pItem =
        de::gameContext().itemFactories().createItem((Item::ItemClass)getItemClass(), getItemType(), getItemOptions());

    return pItem;
}


string ItemMineInfo::toString() const {
    StringStream msg;
    msg << "ItemMineInfo(" << "m_ID : " << m_ID << "m_ItemClass : " << ItemClass2String[(int)m_ItemClass]
        << "m_ItemType : " << (int)m_ItemType << "m_ItemOptions : " << getOptionTypeToString(m_ItemOptions) << ")";
    return msg.toString();
}

ItemMineInfoManager::ItemMineInfoManager() {}

ItemMineInfoManager::~ItemMineInfoManager() {
    __BEGIN_TRY

    HashMapItemMineInfoItor itr = m_ItemMineInfos.begin();
    HashMapItemMineInfoItor endItr = m_ItemMineInfos.end();

    for (; itr != endItr; itr++) {
        ItemMineInfo* pInfo = itr->second;
        SAFE_DELETE(pInfo);
    }

    m_ItemMineInfos.clear();

    __END_CATCH_NO_RETHROW
}

void ItemMineInfoManager::load()

{
    vector<ItemMineRow> rows = defaultGameInfoRepository().loadItemMines();

    for (size_t r = 0; r < rows.size(); r++) {
        ItemMineInfo* pItemMineInfo = new ItemMineInfo();

        int id = rows[r].id;
        string itemClass = rows[r].itemClass;
        int itemType = rows[r].itemType;
        string itemOptions = rows[r].itemOption;

        list<OptionType_t> oList;
        makeOptionList(itemOptions, oList);

        pItemMineInfo->setID(id);
        pItemMineInfo->setItemClass(TreasureItemClass::getItemClassFromString(itemClass));
        pItemMineInfo->setItemType(itemType);
        pItemMineInfo->setItemOptions(oList);

        addItemMineInfo(pItemMineInfo);
    }
}

ItemMineInfo* ItemMineInfoManager::getItemMineInfo(int id) const {
    HashMapItemMineInfoConstItor itr = m_ItemMineInfos.find(id);

    if (itr == m_ItemMineInfos.end())
        return NULL;

    return itr->second;
}

void ItemMineInfoManager::addItemMineInfo(ItemMineInfo* pItemMineInfo)

{
    HashMapItemMineInfoItor itr = m_ItemMineInfos.find(pItemMineInfo->getID());

    if (itr != m_ItemMineInfos.end())
        throw DuplicatedException();

    m_ItemMineInfos[pItemMineInfo->getID()] = pItemMineInfo;
}

string ItemMineInfoManager::toString() const {
    StringStream msg;

    if (m_ItemMineInfos.empty())
        msg << "EMPTY";
    else {
        HashMapItemMineInfoConstItor endItr = m_ItemMineInfos.end();
        for (HashMapItemMineInfoConstItor itr = m_ItemMineInfos.begin(); itr != endItr; itr++) {
            msg << itr->second->toString();
        }
    }

    msg << ")";

    return msg.toString();
}

Item* ItemMineInfoManager::getRandomItem(int minID, int maxID) {
    ItemMineInfo* pItemMineInfo = getItemMineInfo(Random(minID, maxID));

    // A gap in the rows is a data fault; the caller refuses a null item.
    if (pItemMineInfo == NULL)
        return NULL;

    return pItemMineInfo->getItem();
}
