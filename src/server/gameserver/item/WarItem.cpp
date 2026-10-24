//////////////////////////////////////////////////////////////////////////////
// Filename    : WarItem.cpp
// Written By  : Changaya
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "WarItem.h"

#include <stdio.h>

#include "DB.h"
#include "GameContext.h"
#include "ItemInfoManager.h"
#include "RelicUtil.h"
#include "repository/ItemObjectRepository.h"

ItemID_t WarItem::m_ItemIDRegistry = 0;
Mutex WarItem::m_Mutex;

//--------------------------------------------------------------------------------
// constructor
//--------------------------------------------------------------------------------
WarItem::WarItem()

    : m_ItemType(0) {}

WarItem::WarItem(ItemType_t itemType, const list<OptionType_t>& optionType)

    : m_ItemType(itemType) {
    try {
        if (!de::gameContext().itemInfos().isPossibleItem(getItemClass(), m_ItemType, optionType)) {
            filelog("itembug.log", "WarItem::WarItem() : Invalid item type or option type");
            throw Error("WarItem::WarItem() : Invalid item type or optionType");
        }
    } catch (Throwable& t) {
        cout << t.toString().c_str() << endl;
        Assert(false);
    }
}


//--------------------------------------------------------------------------------
// create item
//--------------------------------------------------------------------------------
void WarItem::create(const string& ownerID, Storage storage, StorageID_t storageID, BYTE x, BYTE y, ItemID_t itemID)

{
    if (itemID == 0) {
        __ENTER_CRITICAL_SECTION(m_Mutex)

        m_ItemIDRegistry += de::gameContext().itemInfos().getItemIDSuccessor();
        m_ItemID = m_ItemIDRegistry;

        __LEAVE_CRITICAL_SECTION(m_Mutex)
    } else {
        m_ItemID = itemID;
    }

    logRelicStorage(this, storage, ownerID);

    const string sql = defaultItemObjectRepository().insertPlainItemLogged(
        GEAR_WAR_ITEM, m_ItemID, m_ObjectID, m_ItemType, ownerID, (int)storage, storageID, (int)x, (int)y);
    filelog("WarLog.txt", "%s", sql.c_str());
}


//--------------------------------------------------------------------------------
// save item
//--------------------------------------------------------------------------------
void WarItem::tinysave(const char* field) const

{
    char query[255];

    snprintf(query, sizeof(query), "UPDATE WarItemObject SET %s WHERE ItemID=%u", field, m_ItemID);
    defaultItemObjectRepository().tinysaveGear(GEAR_WAR_ITEM, field, m_ItemID);
    filelog("WarLog.txt", "%s", query);
}

//--------------------------------------------------------------------------------
// save item
//--------------------------------------------------------------------------------
void WarItem::save(const string& ownerID, Storage storage, StorageID_t storageID, BYTE x, BYTE y)

{
    logRelicStorage(this, storage, ownerID);

    defaultItemObjectRepository().updatePlainItem(GEAR_WAR_ITEM, m_ObjectID, m_ItemType, ownerID, (int)storage,
                                                  storageID, (int)x, (int)y, m_ItemID);
}


//--------------------------------------------------------------------------------
// get debug string
//--------------------------------------------------------------------------------
string WarItem::toString() const

{
    StringStream msg;

    msg << "WarItem(" << "ItemID:" << m_ItemID << ",ItemType:" << (int)m_ItemType << ")";

    return msg.toString();
}


//--------------------------------------------------------------------------------
// get width
//--------------------------------------------------------------------------------
VolumeWidth_t WarItem::getVolumeWidth() const

{
    return de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_WAR_ITEM, m_ItemType)->getVolumeWidth();
}


//--------------------------------------------------------------------------------
// get height
//--------------------------------------------------------------------------------
VolumeHeight_t WarItem::getVolumeHeight() const

{
    return de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_WAR_ITEM, m_ItemType)->getVolumeHeight();
}


//--------------------------------------------------------------------------------
// get weight
//--------------------------------------------------------------------------------
Weight_t WarItem::getWeight() const

{
    return de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_WAR_ITEM, m_ItemType)->getWeight();
}


//--------------------------------------------------------------------------------
// get debug string
//--------------------------------------------------------------------------------
string WarItemInfo::toString() const

{
    StringStream msg;

    msg << "WarItemInfo(" << "ItemType:" << m_ItemType << ",Name:" << m_Name << ",EName:" << m_EName
        << ",Price:" << m_Price << ",VolumeType:" << Volume2String[m_VolumeType] << ",Weight:" << m_Weight
        << ",Description:" << m_Description << ")";

    return msg.toString();
}


//--------------------------------------------------------------------------------
// load from DB
//--------------------------------------------------------------------------------
void WarItemInfoManager::load()

{
    m_InfoCount = defaultItemObjectRepository().loadMaxGearType(GEAR_WAR_ITEM);

    m_pItemInfos = new ItemInfo*[m_InfoCount + 1];

    for (uint i = 0; i <= m_InfoCount; i++)
        m_pItemInfos[i] = NULL;

    vector<BasicInfoRow> rows = defaultItemObjectRepository().loadBasicInfos(GEAR_WAR_ITEM);

    for (size_t r = 0; r < rows.size(); r++) {
        WarItemInfo* pWarItemInfo = new WarItemInfo();

        pWarItemInfo->setItemType(rows[r].itemType);
        pWarItemInfo->setName(rows[r].name);
        pWarItemInfo->setEName(rows[r].ename);
        pWarItemInfo->setPrice(rows[r].price);
        pWarItemInfo->setVolumeType(rows[r].volume);
        pWarItemInfo->setWeight(rows[r].weight);
        pWarItemInfo->setRatio(rows[r].ratio);

        addItemInfo(pWarItemInfo);
    }
}


//--------------------------------------------------------------------------------
// load to creature
//--------------------------------------------------------------------------------
void WarItemLoader::load(Creature* pCreature)

{
    Assert(pCreature != NULL);
}


//--------------------------------------------------------------------------------
// load to zone
//--------------------------------------------------------------------------------
void WarItemLoader::load(Zone* pZone)

{
    Assert(pZone != NULL);
}


//--------------------------------------------------------------------------------
// load to inventory
//--------------------------------------------------------------------------------
void WarItemLoader::load(StorageID_t storageID, Inventory* pInventory)

{}
