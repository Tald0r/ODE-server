//////////////////////////////////////////////////////////////////////////////
// Filename    : Sweeper.cpp
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "Sweeper.h"

#include <stdio.h>

#include "Belt.h"
#include "DB.h"
#include "GameContext.h"
#include "ItemInfoManager.h"
#include "ItemUtil.h"
#include "Motorcycle.h"
#include "Slayer.h"
#include "Stash.h"
#include "Vampire.h"
#include "repository/ItemObjectRepository.h"

ItemID_t Sweeper::m_ItemIDRegistry = 0;
Mutex Sweeper::m_Mutex;

//--------------------------------------------------------------------------------
// constructor
//--------------------------------------------------------------------------------
Sweeper::Sweeper()

    : m_ItemType(0), m_Durability(0) {
    m_EnchantLevel = 0;
}

Sweeper::Sweeper(ItemType_t itemType, const list<OptionType_t>& optionType)

    : m_ItemType(itemType), m_Durability(0) {
    try {
        m_EnchantLevel = 0;

        m_Durability = computeMaxDurability(this);

        if (!de::gameContext().itemInfos().isPossibleItem(getItemClass(), m_ItemType, optionType)) {
            filelog("itembug.log", "Sweeper::Sweeper() : Invalid item type or option type");
            throw Error("Sweeper::Sweeper() : Invalid item type or optionType");
        }
    } catch (Throwable& t) {
        cout << t.toString().c_str() << endl;
        Assert(false);
    }
}


//--------------------------------------------------------------------------------
// create item
//--------------------------------------------------------------------------------
void Sweeper::create(const string& ownerID, Storage storage, StorageID_t storageID, BYTE x, BYTE y, ItemID_t itemID)

{
    __BEGIN_TRY

    if (itemID == 0) {
        __ENTER_CRITICAL_SECTION(m_Mutex)

        m_ItemIDRegistry += de::gameContext().itemInfos().getItemIDSuccessor();
        m_ItemID = m_ItemIDRegistry;

        __LEAVE_CRITICAL_SECTION(m_Mutex)
    } else {
        m_ItemID = itemID;
    }

    const string sql = defaultItemObjectRepository().insertWarItem(
        GEAR_SWEEPER, m_ItemID, m_ObjectID, m_ItemType, ownerID, (int)storage, storageID, (int)x, (int)y, m_Durability);
    filelog("WarLog.txt", "%s", sql.c_str());

    __END_CATCH
}


//--------------------------------------------------------------------------------
// save item
//--------------------------------------------------------------------------------
void Sweeper::tinysave(const char* field) const

{
    __BEGIN_TRY

    char query[255];

    snprintf(query, sizeof(query), "UPDATE SweeperObject SET %s WHERE ItemID=%u", field, m_ItemID);
    defaultItemObjectRepository().tinysaveGear(GEAR_SWEEPER, field, m_ItemID);
    filelog("WarLog.txt", "%s", query);

    __END_CATCH
}

//--------------------------------------------------------------------------------
// save item
//--------------------------------------------------------------------------------
void Sweeper::save(const string& ownerID, Storage storage, StorageID_t storageID, BYTE x, BYTE y)

{
    __BEGIN_TRY

    defaultItemObjectRepository().updateWarItem(GEAR_SWEEPER, m_ObjectID, m_ItemType, ownerID, (int)storage, storageID,
                                                (int)x, (int)y, m_Durability, (int)m_EnchantLevel, m_ItemID);

    __END_CATCH
}


//--------------------------------------------------------------------------------
// get debug string
//--------------------------------------------------------------------------------
string Sweeper::toString() const

{
    StringStream msg;

    msg << "Sweeper(" << "ItemID:" << m_ItemID << ",ItemType:" << (int)m_ItemType << ",Durability:" << (int)m_Durability
        << ",EnchantLevel:" << (int)m_EnchantLevel << ")";

    return msg.toString();
}


//--------------------------------------------------------------------------------
// get width
//--------------------------------------------------------------------------------
VolumeWidth_t Sweeper::getVolumeWidth() const

{
    __BEGIN_TRY

    return de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_SWEEPER, m_ItemType)->getVolumeWidth();

    __END_CATCH
}


//--------------------------------------------------------------------------------
// get height
//--------------------------------------------------------------------------------
VolumeHeight_t Sweeper::getVolumeHeight() const

{
    __BEGIN_TRY

    return de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_SWEEPER, m_ItemType)->getVolumeHeight();

    __END_CATCH
}


//--------------------------------------------------------------------------------
// get weight
//--------------------------------------------------------------------------------
Weight_t Sweeper::getWeight() const

{
    __BEGIN_TRY

    return de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_SWEEPER, m_ItemType)->getWeight();

    __END_CATCH
}


//--------------------------------------------------------------------------------
// get/set armor's Defense Bonus
//--------------------------------------------------------------------------------
Defense_t Sweeper::getDefenseBonus() const

{
    __BEGIN_TRY

    return de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_SWEEPER, m_ItemType)->getDefenseBonus();

    __END_CATCH
}
Protection_t Sweeper::getProtectionBonus() const

{
    __BEGIN_TRY

    return de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_SWEEPER, m_ItemType)->getProtectionBonus();

    __END_CATCH
}


//--------------------------------------------------------------------------------
// get debug string
//--------------------------------------------------------------------------------
string SweeperInfo::toString() const

{
    StringStream msg;

    msg << "SweeperInfo(" << "ItemType:" << m_ItemType << ",Name:" << m_Name << ",EName:" << m_EName
        << ",Price:" << m_Price << ",VolumeType:" << Volume2String[m_VolumeType] << ",Weight:" << m_Weight
        << ",Description:" << m_Description << ",Durability:" << m_Durability << ",DefenseBonus:" << m_DefenseBonus
        << ")";

    return msg.toString();
}


//--------------------------------------------------------------------------------
// load from DB
//--------------------------------------------------------------------------------
void SweeperInfoManager::load()

{
    __BEGIN_TRY

    m_InfoCount = defaultItemObjectRepository().loadMaxGearType(GEAR_SWEEPER);

    m_pItemInfos = new ItemInfo*[m_InfoCount + 1];

    for (uint i = 0; i <= m_InfoCount; i++)
        m_pItemInfos[i] = NULL;

    vector<WarInfoRow> rows = defaultItemObjectRepository().loadWarInfos(GEAR_SWEEPER);

    for (size_t r = 0; r < rows.size(); r++) {
        SweeperInfo* pSweeperInfo = new SweeperInfo();

        pSweeperInfo->setItemType(rows[r].itemType);
        pSweeperInfo->setName(rows[r].name);
        pSweeperInfo->setEName(rows[r].ename);
        pSweeperInfo->setPrice(rows[r].price);
        pSweeperInfo->setVolumeType(rows[r].volume);
        pSweeperInfo->setWeight(rows[r].weight);
        pSweeperInfo->setRatio(rows[r].ratio);
        pSweeperInfo->setDurability(rows[r].durability);
        pSweeperInfo->setDefenseBonus(rows[r].defense);
        pSweeperInfo->setProtectionBonus(rows[r].protection);
        pSweeperInfo->setReqAbility(rows[r].reqAbility);
        pSweeperInfo->setItemLevel(rows[r].itemLevel);

        addItemInfo(pSweeperInfo);
    }

    __END_CATCH
}


//--------------------------------------------------------------------------------
// load to creature
//--------------------------------------------------------------------------------
void SweeperLoader::load(Creature* pCreature)

{
    __BEGIN_TRY

    Assert(pCreature != NULL);

    // Anything left to load for a Sweeper means, as things stand,
    // that the server went down earlier.
    // So it is deleted.
    defaultItemObjectRepository().deleteWarItemsOfOwner(GEAR_SWEEPER, pCreature->getName());

    __END_CATCH
}


//--------------------------------------------------------------------------------
// load to zone
//--------------------------------------------------------------------------------
void SweeperLoader::load(Zone* pZone)

{
    __BEGIN_TRY

    Assert(pZone != NULL);

    vector<WarItemZoneObjectRow> rows =
        defaultItemObjectRepository().loadWarItemInZone(GEAR_SWEEPER, (int)STORAGE_ZONE, pZone->getZoneID());

    for (size_t r = 0; r < rows.size(); r++) {
        Sweeper* pSweeper = new Sweeper();

        pSweeper->setItemID(rows[r].itemID);
        pSweeper->setObjectID(rows[r].objectID);
        pSweeper->setItemType(rows[r].itemType);

        Storage storage = (Storage)rows[r].storage;
        BYTE x = rows[r].x;
        BYTE y = rows[r].y;

        pSweeper->setDurability(rows[r].durability);
        pSweeper->setEnchantLevel(rows[r].enchantLevel);

        switch (storage) {
        case STORAGE_ZONE: {
            Tile& pTile = pZone->getTile(x, y);
            Assert(!pTile.hasItem());
            pTile.addItem(pSweeper);
        } break;

        case STORAGE_STASH:
        case STORAGE_CORPSE:
            throw UnsupportedError("Saving items inside boxes or corpses is not supported.");

        default:
            throw Error("Storage must be STORAGE_ZONE");
        }
    }

    __END_CATCH
}


//--------------------------------------------------------------------------------
// load to inventory
//--------------------------------------------------------------------------------
void SweeperLoader::load(StorageID_t storageID, Inventory* pInventory)

{
    __BEGIN_TRY

    Statement* pStmt;

    BEGIN_DB {}
    END_DB(pStmt)

    __END_CATCH
}
