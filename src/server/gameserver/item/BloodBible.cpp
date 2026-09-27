//////////////////////////////////////////////////////////////////////////////
// Filename    : BloodBible.cpp
// Written By  : Changaya
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "BloodBible.h"

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

ItemID_t BloodBible::m_ItemIDRegistry = 0;
Mutex BloodBible::m_Mutex;

//--------------------------------------------------------------------------------
// constructor
//--------------------------------------------------------------------------------
BloodBible::BloodBible()

    : m_ItemType(0), m_Durability(0) {
    m_EnchantLevel = 0;
}

BloodBible::BloodBible(ItemType_t itemType, const list<OptionType_t>& optionType)

    : m_ItemType(itemType), m_Durability(0) {
    try {
        m_EnchantLevel = 0;

        m_Durability = computeMaxDurability(this);

        if (!de::gameContext().itemInfos().isPossibleItem(getItemClass(), m_ItemType, optionType)) {
            filelog("itembug.log", "BloodBible::BloodBible() : Invalid item type or option type");
            throw Error("BloodBible::BloodBible() : Invalid item type or optionType");
        }
    } catch (Throwable& t) {
        cout << t.toString().c_str() << endl;
        Assert(false);
    }
}


//--------------------------------------------------------------------------------
// create item
//--------------------------------------------------------------------------------
void BloodBible::create(const string& ownerID, Storage storage, StorageID_t storageID, BYTE x, BYTE y, ItemID_t itemID)

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

    logRelicStorage(this, storage, ownerID);

    const string sql =
        defaultItemObjectRepository().insertWarItem(GEAR_BLOOD_BIBLE, m_ItemID, m_ObjectID, m_ItemType, ownerID,
                                                    (int)storage, storageID, (int)x, (int)y, m_Durability);
    filelog("WarLog.txt", "%s", sql.c_str());

    __END_CATCH
}


//--------------------------------------------------------------------------------
// save item
//--------------------------------------------------------------------------------
void BloodBible::tinysave(const char* field) const

{
    __BEGIN_TRY

    char query[255];

    snprintf(query, sizeof(query), "UPDATE BloodBibleObject SET %s WHERE ItemID=%u", field, m_ItemID);
    defaultItemObjectRepository().tinysaveGear(GEAR_BLOOD_BIBLE, field, m_ItemID);
    filelog("WarLog.txt", "%s", query);

    __END_CATCH
}

//--------------------------------------------------------------------------------
// save item
//--------------------------------------------------------------------------------
void BloodBible::save(const string& ownerID, Storage storage, StorageID_t storageID, BYTE x, BYTE y)

{
    __BEGIN_TRY

    logRelicStorage(this, storage, ownerID);

    defaultItemObjectRepository().updateWarItem(GEAR_BLOOD_BIBLE, m_ObjectID, m_ItemType, ownerID, (int)storage,
                                                storageID, (int)x, (int)y, m_Durability, (int)getEnchantLevel(),
                                                m_ItemID);

    __END_CATCH
}


//--------------------------------------------------------------------------------
// get debug string
//--------------------------------------------------------------------------------
string BloodBible::toString() const

{
    StringStream msg;

    msg << "BloodBible(" << "ItemID:" << m_ItemID << ",ItemType:" << (int)m_ItemType
        << ",Durability:" << (int)m_Durability << ",EnchantLevel:" << (int)getEnchantLevel() << ")";

    return msg.toString();
}


//--------------------------------------------------------------------------------
// get width
//--------------------------------------------------------------------------------
VolumeWidth_t BloodBible::getVolumeWidth() const

{
    __BEGIN_TRY

    return de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_BLOOD_BIBLE, m_ItemType)->getVolumeWidth();

    __END_CATCH
}


//--------------------------------------------------------------------------------
// get height
//--------------------------------------------------------------------------------
VolumeHeight_t BloodBible::getVolumeHeight() const

{
    __BEGIN_TRY

    return de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_BLOOD_BIBLE, m_ItemType)->getVolumeHeight();

    __END_CATCH
}


//--------------------------------------------------------------------------------
// get weight
//--------------------------------------------------------------------------------
Weight_t BloodBible::getWeight() const

{
    __BEGIN_TRY

    return de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_BLOOD_BIBLE, m_ItemType)->getWeight();

    __END_CATCH
}


//--------------------------------------------------------------------------------
// get/set armor's Defense Bonus
//--------------------------------------------------------------------------------
Defense_t BloodBible::getDefenseBonus() const

{
    __BEGIN_TRY

    return de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_BLOOD_BIBLE, m_ItemType)->getDefenseBonus();

    __END_CATCH
}
Protection_t BloodBible::getProtectionBonus() const

{
    __BEGIN_TRY

    return de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_BLOOD_BIBLE, m_ItemType)->getProtectionBonus();

    __END_CATCH
}


//--------------------------------------------------------------------------------
// get debug string
//--------------------------------------------------------------------------------
string BloodBibleInfo::toString() const

{
    StringStream msg;

    msg << "BloodBibleInfo(" << "ItemType:" << m_ItemType << ",Name:" << m_Name << ",EName:" << m_EName
        << ",Price:" << m_Price << ",VolumeType:" << Volume2String[m_VolumeType] << ",Weight:" << m_Weight
        << ",Description:" << m_Description << ",Durability:" << m_Durability << ",DefenseBonus:" << m_DefenseBonus
        << ")";

    return msg.toString();
}


//--------------------------------------------------------------------------------
// load from DB
//--------------------------------------------------------------------------------
void BloodBibleInfoManager::load()

{
    __BEGIN_TRY

    m_InfoCount = defaultItemObjectRepository().loadMaxGearType(GEAR_BLOOD_BIBLE);

    m_pItemInfos = new ItemInfo*[m_InfoCount + 1];

    for (uint i = 0; i <= m_InfoCount; i++)
        m_pItemInfos[i] = NULL;

    vector<WarInfoRow> rows = defaultItemObjectRepository().loadWarInfos(GEAR_BLOOD_BIBLE);

    for (size_t r = 0; r < rows.size(); r++) {
        BloodBibleInfo* pBloodBibleInfo = new BloodBibleInfo();

        pBloodBibleInfo->setItemType(rows[r].itemType);
        pBloodBibleInfo->setName(rows[r].name);
        pBloodBibleInfo->setEName(rows[r].ename);
        pBloodBibleInfo->setPrice(rows[r].price);
        pBloodBibleInfo->setVolumeType(rows[r].volume);
        pBloodBibleInfo->setWeight(rows[r].weight);
        pBloodBibleInfo->setRatio(rows[r].ratio);
        pBloodBibleInfo->setDurability(rows[r].durability);
        pBloodBibleInfo->setDefenseBonus(rows[r].defense);
        pBloodBibleInfo->setProtectionBonus(rows[r].protection);
        pBloodBibleInfo->setReqAbility(rows[r].reqAbility);
        pBloodBibleInfo->setItemLevel(rows[r].itemLevel);

        addItemInfo(pBloodBibleInfo);
    }

    __END_CATCH
}


//--------------------------------------------------------------------------------
// load to creature
//--------------------------------------------------------------------------------
void BloodBibleLoader::load(Creature* pCreature)

{
    __BEGIN_TRY

    Assert(pCreature != NULL);

    // Anything left to load for a BloodBible means, as things stand,
    // that the server went down earlier.
    // So it is deleted.
    defaultItemObjectRepository().deleteWarItemsOfOwner(GEAR_BLOOD_BIBLE, pCreature->getName());

    __END_CATCH
}


//--------------------------------------------------------------------------------
// load to zone
//--------------------------------------------------------------------------------
void BloodBibleLoader::load(Zone* pZone)

{
    __BEGIN_TRY

    Assert(pZone != NULL);

    vector<WarItemZoneObjectRow> rows =
        defaultItemObjectRepository().loadWarItemInZone(GEAR_BLOOD_BIBLE, (int)STORAGE_ZONE, pZone->getZoneID());

    for (size_t r = 0; r < rows.size(); r++) {
        BloodBible* pBloodBible = new BloodBible();

        pBloodBible->setItemID(rows[r].itemID);
        pBloodBible->setObjectID(rows[r].objectID);
        pBloodBible->setItemType(rows[r].itemType);

        Storage storage = (Storage)rows[r].storage;
        BYTE x = rows[r].x;
        BYTE y = rows[r].y;

        pBloodBible->setDurability(rows[r].durability);
        pBloodBible->setEnchantLevel(rows[r].enchantLevel);

        switch (storage) {
        case STORAGE_ZONE: {
            Tile& pTile = pZone->getTile(x, y);
            Assert(!pTile.hasItem());
            pTile.addItem(pBloodBible);
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
void BloodBibleLoader::load(StorageID_t storageID, Inventory* pInventory)

{
    __BEGIN_TRY

    Statement* pStmt;

    BEGIN_DB {}
    END_DB(pStmt)

    __END_CATCH
}
