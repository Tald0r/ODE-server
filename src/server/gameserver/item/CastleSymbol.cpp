//////////////////////////////////////////////////////////////////////////////
// Filename    : CastleSymbol.cpp
// Written By  : Changaya
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "CastleSymbol.h"

#include <stdio.h>

#include "Belt.h"
#include "DB.h"
#include "GameContext.h"
#include "ItemInfoManager.h"
#include "ItemUtil.h"
#include "Motorcycle.h"
#include "RelicUtil.h"
#include "Slayer.h"
#include "Stash.h"
#include "Vampire.h"
#include "repository/ItemObjectRepository.h"

ItemID_t CastleSymbol::m_ItemIDRegistry = 0;
Mutex CastleSymbol::m_Mutex;

//--------------------------------------------------------------------------------
// constructor
//--------------------------------------------------------------------------------
CastleSymbol::CastleSymbol()

    : m_ItemType(0), m_Durability(0) {
    m_EnchantLevel = 0;
}

CastleSymbol::CastleSymbol(ItemType_t itemType, const list<OptionType_t>& optionType)

    : m_ItemType(itemType), m_Durability(0) {
    try {
        m_EnchantLevel = 0;

        m_Durability = computeMaxDurability(this);

        if (!de::gameContext().itemInfos().isPossibleItem(getItemClass(), m_ItemType, optionType)) {
            filelog("itembug.log", "CastleSymbol::CastleSymbol() : Invalid item type or option type");
            throw Error("CastleSymbol::CastleSymbol() : Invalid item type or optionType");
        }
    } catch (Throwable& t) {
        cout << t.toString().c_str() << endl;
        Assert(false);
    }
}


//--------------------------------------------------------------------------------
// create item
//--------------------------------------------------------------------------------
void CastleSymbol::create(const string& ownerID, Storage storage, StorageID_t storageID, BYTE x, BYTE y,
                          ItemID_t itemID)

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

    const string sql =
        defaultItemObjectRepository().insertWarItem(GEAR_CASTLE_SYMBOL, m_ItemID, m_ObjectID, m_ItemType, ownerID,
                                                    (int)storage, storageID, (int)x, (int)y, m_Durability);
    filelog("WarLog.txt", "%s", sql.c_str());
}


//--------------------------------------------------------------------------------
// save item
//--------------------------------------------------------------------------------
void CastleSymbol::tinysave(const char* field) const

{
    char query[255];

    snprintf(query, sizeof(query), "UPDATE CastleSymbolObject SET %s WHERE ItemID=%u", field, m_ItemID);
    defaultItemObjectRepository().tinysaveGear(GEAR_CASTLE_SYMBOL, field, m_ItemID);
    filelog("WarLog.txt", "%s", query);
}

//--------------------------------------------------------------------------------
// save item
//--------------------------------------------------------------------------------
void CastleSymbol::save(const string& ownerID, Storage storage, StorageID_t storageID, BYTE x, BYTE y)

{
    logRelicStorage(this, storage, ownerID);

    defaultItemObjectRepository().updateWarItem(GEAR_CASTLE_SYMBOL, m_ObjectID, m_ItemType, ownerID, (int)storage,
                                                storageID, (int)x, (int)y, m_Durability, (int)m_EnchantLevel, m_ItemID);
}


//--------------------------------------------------------------------------------
// get debug string
//--------------------------------------------------------------------------------
string CastleSymbol::toString() const

{
    StringStream msg;

    msg << "CastleSymbol(" << "ItemID:" << m_ItemID << ",ItemType:" << (int)m_ItemType
        << ",Durability:" << (int)m_Durability << ",EnchantLevel:" << (int)m_EnchantLevel << ")";

    return msg.toString();
}


//--------------------------------------------------------------------------------
// get width
//--------------------------------------------------------------------------------
VolumeWidth_t CastleSymbol::getVolumeWidth() const

{
    return de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_CASTLE_SYMBOL, m_ItemType)->getVolumeWidth();
}


//--------------------------------------------------------------------------------
// get height
//--------------------------------------------------------------------------------
VolumeHeight_t CastleSymbol::getVolumeHeight() const

{
    return de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_CASTLE_SYMBOL, m_ItemType)->getVolumeHeight();
}


//--------------------------------------------------------------------------------
// get weight
//--------------------------------------------------------------------------------
Weight_t CastleSymbol::getWeight() const

{
    return de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_CASTLE_SYMBOL, m_ItemType)->getWeight();
}


//--------------------------------------------------------------------------------
// get/set armor's Defense Bonus
//--------------------------------------------------------------------------------
Defense_t CastleSymbol::getDefenseBonus() const

{
    return de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_CASTLE_SYMBOL, m_ItemType)->getDefenseBonus();
}
Protection_t CastleSymbol::getProtectionBonus() const

{
    return de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_CASTLE_SYMBOL, m_ItemType)->getProtectionBonus();
}


//--------------------------------------------------------------------------------
// get debug string
//--------------------------------------------------------------------------------
string CastleSymbolInfo::toString() const

{
    StringStream msg;

    msg << "CastleSymbolInfo(" << "ItemType:" << m_ItemType << ",Name:" << m_Name << ",EName:" << m_EName
        << ",Price:" << m_Price << ",VolumeType:" << Volume2String[m_VolumeType] << ",Weight:" << m_Weight
        << ",Description:" << m_Description << ",Durability:" << m_Durability << ",DefenseBonus:" << m_DefenseBonus
        << ")";

    return msg.toString();
}


//--------------------------------------------------------------------------------
// load from DB
//--------------------------------------------------------------------------------
void CastleSymbolInfoManager::load()

{
    m_InfoCount = defaultItemObjectRepository().loadMaxGearType(GEAR_CASTLE_SYMBOL);

    m_pItemInfos = new ItemInfo*[m_InfoCount + 1];

    for (uint i = 0; i <= m_InfoCount; i++)
        m_pItemInfos[i] = NULL;

    vector<WarInfoRow> rows = defaultItemObjectRepository().loadWarInfos(GEAR_CASTLE_SYMBOL);

    for (size_t r = 0; r < rows.size(); r++) {
        CastleSymbolInfo* pCastleSymbolInfo = new CastleSymbolInfo();

        pCastleSymbolInfo->setItemType(rows[r].itemType);
        pCastleSymbolInfo->setName(rows[r].name);
        pCastleSymbolInfo->setEName(rows[r].ename);
        pCastleSymbolInfo->setPrice(rows[r].price);
        pCastleSymbolInfo->setVolumeType(rows[r].volume);
        pCastleSymbolInfo->setWeight(rows[r].weight);
        pCastleSymbolInfo->setRatio(rows[r].ratio);
        pCastleSymbolInfo->setDurability(rows[r].durability);
        pCastleSymbolInfo->setDefenseBonus(rows[r].defense);
        pCastleSymbolInfo->setProtectionBonus(rows[r].protection);
        pCastleSymbolInfo->setReqAbility(rows[r].reqAbility);
        pCastleSymbolInfo->setItemLevel(rows[r].itemLevel);

        addItemInfo(pCastleSymbolInfo);
    }
}


//--------------------------------------------------------------------------------
// load to creature
//--------------------------------------------------------------------------------
void CastleSymbolLoader::load(Creature* pCreature)

{
    Assert(pCreature != NULL);

    // Anything left to load for a CastleSymbol means, as things stand,
    // that the server went down earlier.
    // So it is deleted.
    defaultItemObjectRepository().deleteWarItemsOfOwner(GEAR_CASTLE_SYMBOL, pCreature->getName());
}


//--------------------------------------------------------------------------------
// load to zone
//--------------------------------------------------------------------------------
void CastleSymbolLoader::load(Zone* pZone)

{
    Assert(pZone != NULL);

    vector<WarItemZoneObjectRow> rows =
        defaultItemObjectRepository().loadWarItemInZone(GEAR_CASTLE_SYMBOL, (int)STORAGE_ZONE, pZone->getZoneID());

    for (size_t r = 0; r < rows.size(); r++) {
        CastleSymbol* pCastleSymbol = new CastleSymbol();

        pCastleSymbol->setItemID(rows[r].itemID);
        pCastleSymbol->setObjectID(rows[r].objectID);
        pCastleSymbol->setItemType(rows[r].itemType);

        Storage storage = (Storage)rows[r].storage;
        BYTE x = rows[r].x;
        BYTE y = rows[r].y;

        pCastleSymbol->setDurability(rows[r].durability);
        pCastleSymbol->setEnchantLevel(rows[r].enchantLevel);

        switch (storage) {
        case STORAGE_ZONE: {
            Tile& pTile = pZone->getTile(x, y);
            Assert(!pTile.hasItem());
            pTile.addItem(pCastleSymbol);
        } break;

        case STORAGE_STASH:
        case STORAGE_CORPSE:
            throw UnsupportedError("Saving items inside boxes or corpses is not supported.");

        default:
            throw Error("Storage must be STORAGE_ZONE");
        }
    }
}


//--------------------------------------------------------------------------------
// load to inventory
//--------------------------------------------------------------------------------
void CastleSymbolLoader::load(StorageID_t storageID, Inventory* pInventory)

{
    Statement* pStmt;

    BEGIN_DB {}
    END_DB(pStmt)
}
