//////////////////////////////////////////////////////////////////////////////
// Filename    : HolyWater.cpp
// Written By  : Elca
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "HolyWater.h"

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

ItemID_t HolyWater::m_ItemIDRegistry = 0;
Mutex HolyWater::m_Mutex;

//--------------------------------------------------------------------------------
// constructor
//--------------------------------------------------------------------------------
HolyWater::HolyWater()

{
    m_ItemType = 0;
    m_Num = 1;
}

HolyWater::HolyWater(ItemType_t itemType, const list<OptionType_t>& optionType)

{
    m_ItemType = itemType;
    m_Num = 1;

    if (!de::gameContext().itemInfos().isPossibleItem(getItemClass(), m_ItemType, optionType)) {
        filelog("itembug.log", "HolyWater::HolyWater() : Invalid item type or option type");
        throw Error("HolyWater::HolyWater() : Invalid item type or optionType");
    }
}


//--------------------------------------------------------------------------------
// create item
//--------------------------------------------------------------------------------
void HolyWater::create(const string& ownerID, Storage storage, StorageID_t storageID, BYTE x, BYTE y, ItemID_t itemID)

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

    defaultItemObjectRepository().insertNumOnlyItem(GEAR_HOLY_WATER, m_ItemID, m_ObjectID, m_ItemType, ownerID,
                                                    (int)storage, storageID, (int)x, (int)y, (int)m_Num);

    __END_CATCH
}


//--------------------------------------------------------------------------------
// save item
//--------------------------------------------------------------------------------
void HolyWater::tinysave(const char* field) const

{
    __BEGIN_TRY

    defaultItemObjectRepository().tinysaveGear(GEAR_HOLY_WATER, field, m_ItemID);

    __END_CATCH
}

//--------------------------------------------------------------------------------
// save item
//--------------------------------------------------------------------------------
void HolyWater::save(const string& ownerID, Storage storage, StorageID_t storageID, BYTE x, BYTE y)

{
    __BEGIN_TRY

    defaultItemObjectRepository().updateNumOnlyItem(GEAR_HOLY_WATER, m_ObjectID, m_ItemType, ownerID, (int)storage,
                                                    storageID, (int)x, (int)y, (int)m_Num, m_ItemID);

    __END_CATCH
}

Damage_t HolyWater::getMinDamage() const

{
    __BEGIN_TRY

    return de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_HOLYWATER, m_ItemType)->getMinDamage();

    __END_CATCH
}

Damage_t HolyWater::getMaxDamage() const

{
    __BEGIN_TRY

    return de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_HOLYWATER, m_ItemType)->getMaxDamage();

    __END_CATCH
}

//--------------------------------------------------------------------------------
// get debug string
//--------------------------------------------------------------------------------
string HolyWater::toString() const

{
    StringStream msg;
    msg << "HolyWater(" << "ItemID:" << m_ItemID << ",ItemType:" << m_ItemType << ",Num:" << (int)m_Num << ")";
    return msg.toString();
}


//--------------------------------------------------------------------------------
// get width
//--------------------------------------------------------------------------------
VolumeWidth_t HolyWater::getVolumeWidth() const

{
    __BEGIN_TRY

    return de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_HOLYWATER, m_ItemType)->getVolumeWidth();

    __END_CATCH
}


//--------------------------------------------------------------------------------
// get height
//--------------------------------------------------------------------------------
VolumeHeight_t HolyWater::getVolumeHeight() const

{
    __BEGIN_TRY

    return de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_HOLYWATER, m_ItemType)->getVolumeHeight();

    __END_CATCH
}


//--------------------------------------------------------------------------------
// get weight
//--------------------------------------------------------------------------------
Weight_t HolyWater::getWeight() const

{
    __BEGIN_TRY

    return de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_HOLYWATER, m_ItemType)->getWeight();

    __END_CATCH
}


//--------------------------------------------------------------------------------
// get debug string
//--------------------------------------------------------------------------------
string HolyWaterInfo::toString() const

{
    StringStream msg;

    msg << "HolyWaterInfo(" << "ItemType:" << m_ItemType << ",Name:" << m_Name << ",EName:" << m_EName
        << ",Price:" << m_Price << ",VolumeType:" << Volume2String[m_VolumeType] << ",Weight:" << m_Weight
        << ",Description:" << m_Description << ")";

    return msg.toString();
}


//--------------------------------------------------------------------------------
// load from DB
//--------------------------------------------------------------------------------
void HolyWaterInfoManager::load()

{
    __BEGIN_TRY

    m_InfoCount = defaultItemObjectRepository().loadMaxGearType(GEAR_HOLY_WATER);

    m_pItemInfos = new ItemInfo*[m_InfoCount + 1];

    for (uint i = 0; i <= m_InfoCount; i++)
        m_pItemInfos[i] = NULL;

    vector<DamageInfoRow> rows = defaultItemObjectRepository().loadDamageInfos(GEAR_HOLY_WATER);

    for (size_t r = 0; r < rows.size(); r++) {
        HolyWaterInfo* pHolyWaterInfo = new HolyWaterInfo();

        pHolyWaterInfo->setItemType(rows[r].basic.itemType);
        pHolyWaterInfo->setName(rows[r].basic.name);
        pHolyWaterInfo->setEName(rows[r].basic.ename);
        pHolyWaterInfo->setPrice(rows[r].basic.price);
        pHolyWaterInfo->setVolumeType(rows[r].basic.volume);
        pHolyWaterInfo->setWeight(rows[r].basic.weight);
        pHolyWaterInfo->setRatio(rows[r].basic.ratio);
        pHolyWaterInfo->setMinDamage(rows[r].minDamage);
        pHolyWaterInfo->setMaxDamage(rows[r].maxDamage);

        addItemInfo(pHolyWaterInfo);
    }

    __END_CATCH
}


//--------------------------------------------------------------------------------
// load to creature
//--------------------------------------------------------------------------------
void HolyWaterLoader::load(Creature* pCreature)

{
    __BEGIN_TRY

    Assert(pCreature != NULL);

    vector<NumOnlyObjectRow> rows =
        defaultItemObjectRepository().loadNumOnlyItemOfOwner(GEAR_HOLY_WATER, pCreature->getName());

    for (size_t r = 0; r < rows.size(); r++) {
        try {
            HolyWater* pHolyWater = new HolyWater();

            pHolyWater->setItemID(rows[r].itemID);
            pHolyWater->setObjectID(rows[r].objectID);
            pHolyWater->setItemType(rows[r].itemType);

            Storage storage = (Storage)rows[r].storage;
            BYTE x = rows[r].x;
            BYTE y = rows[r].y;

            pHolyWater->setNum(rows[r].num);

            Inventory* pInventory = NULL;
            Slayer* pSlayer = NULL;
            Vampire* pVampire = NULL;
            Stash* pStash = NULL;

            if (pCreature->isSlayer()) {
                pSlayer = dynamic_cast<Slayer*>(pCreature);
                pInventory = pSlayer->getInventory();
                pStash = pSlayer->getStash();
            } else if (pCreature->isVampire()) {
                pVampire = dynamic_cast<Vampire*>(pCreature);
                pInventory = pVampire->getInventory();
                pStash = pVampire->getStash();
            } else
                throw UnsupportedError("Saving Monster/NPC inventories is not supported.");

            switch (storage) {
            case STORAGE_INVENTORY:
                if (pInventory->canAddingEx(x, y, pHolyWater)) {
                    pInventory->addItemEx(x, y, pHolyWater);
                } else {
                    processItemBugEx(pCreature, pHolyWater);
                }
                break;

            case STORAGE_GEAR:
                processItemBugEx(pCreature, pHolyWater);
                break;

            case STORAGE_BELT:
                processItemBugEx(pCreature, pHolyWater);
                break;

            case STORAGE_EXTRASLOT:
                if (pCreature->isSlayer())
                    pSlayer->addItemToExtraInventorySlot(pHolyWater);
                else if (pCreature->isVampire())
                    pVampire->addItemToExtraInventorySlot(pHolyWater);
                break;

            case STORAGE_MOTORCYCLE:
                processItemBugEx(pCreature, pHolyWater);
                break;

            case STORAGE_STASH:
                if (pStash->isExist(x, y)) {
                    processItemBugEx(pCreature, pHolyWater);
                } else
                    pStash->insert(x, y, pHolyWater);
                break;

            case STORAGE_GARBAGE:
                processItemBug(pCreature, pHolyWater);
                break;

            default:
                throw Error("invalid storage or OwnerID must be NULL");
            }

        } catch (Error& error) {
            filelog("itemLoadError.txt", "[%s] %s", getItemClassName().c_str(), error.toString().c_str());
            throw;
        } catch (Throwable& t) {
            filelog("itemLoadError.txt", "[%s] %s", getItemClassName().c_str(), t.toString().c_str());
        }
    }

    __END_CATCH
}


//--------------------------------------------------------------------------------
// load to zone
//--------------------------------------------------------------------------------
void HolyWaterLoader::load(Zone* pZone)

{
    __BEGIN_TRY

    Assert(pZone != NULL);

    vector<NumOnlyZoneObjectRow> rows =
        defaultItemObjectRepository().loadNumOnlyItemInZone(GEAR_HOLY_WATER, (int)STORAGE_ZONE, pZone->getZoneID());

    for (size_t r = 0; r < rows.size(); r++) {
        HolyWater* pHolyWater = new HolyWater();

        pHolyWater->setItemID(rows[r].itemID);
        pHolyWater->setObjectID(rows[r].objectID);
        pHolyWater->setItemType(rows[r].itemType);

        Storage storage = (Storage)rows[r].storage;
        BYTE x = rows[r].x;
        BYTE y = rows[r].y;

        pHolyWater->setNum(rows[r].num);

        switch (storage) {
        case STORAGE_ZONE: {
            Tile& pTile = pZone->getTile(x, y);
            Assert(!pTile.hasItem());
            pTile.addItem(pHolyWater);
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
void HolyWaterLoader::load(StorageID_t storageID, Inventory* pInventory)

{
    __BEGIN_TRY

    Statement* pStmt;

    BEGIN_DB {}
    END_DB(pStmt)

    __END_CATCH
}
