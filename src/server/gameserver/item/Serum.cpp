//////////////////////////////////////////////////////////////////////////////
// Filename    : Serum.cpp
// Written By  : excel96
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "Serum.h"

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

ItemID_t Serum::m_ItemIDRegistry = 0;
Mutex Serum::m_Mutex;

//////////////////////////////////////////////////////////////////////////////
// constructor
//////////////////////////////////////////////////////////////////////////////
Serum::Serum()

{
    m_ItemType = 0;
    m_Num = 1;
}

Serum::Serum(ItemType_t itemType, const list<OptionType_t>& optionType)

{
    m_ItemType = itemType;
    m_Num = 1;

    if (!de::gameContext().itemInfos().isPossibleItem(getItemClass(), m_ItemType, optionType)) {
        filelog("itembug.log", "Serum::Serum() : Invalid item type or option type");
        throw Error("Serum::Serum() : Invalid item type or optionType");
    }
}


//////////////////////////////////////////////////////////////////////////////
// create item
//////////////////////////////////////////////////////////////////////////////
void Serum::create(const string& ownerID, Storage storage, StorageID_t storageID, BYTE x, BYTE y, ItemID_t itemID)

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

    defaultItemObjectRepository().insertNumOnlyItem(GEAR_SERUM, m_ItemID, m_ObjectID, m_ItemType, ownerID, (int)storage,
                                                    storageID, (int)x, (int)y, (int)m_Num);

    __END_CATCH
}


//--------------------------------------------------------------------------------
// save item
//--------------------------------------------------------------------------------
void Serum::tinysave(const char* field) const

{
    __BEGIN_TRY

    defaultItemObjectRepository().tinysaveGear(GEAR_SERUM, field, m_ItemID);

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
// save item
//////////////////////////////////////////////////////////////////////////////
void Serum::save(const string& ownerID, Storage storage, StorageID_t storageID, BYTE x, BYTE y)

{
    __BEGIN_TRY

    defaultItemObjectRepository().updateNumOnlyItem(GEAR_SERUM, m_ObjectID, m_ItemType, ownerID, (int)storage,
                                                    storageID, (int)x, (int)y, (int)m_Num, m_ItemID);

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////////////
// get debug string
//////////////////////////////////////////////////////////////////////////////
string Serum::toString() const

{
    StringStream msg;
    msg << "Serum(" << "ItemID:" << m_ItemID << ",ItemType:" << (int)m_ItemType << ",Num:" << (int)m_Num << ")";
    return msg.toString();
}


//////////////////////////////////////////////////////////////////////////////
// get width
//////////////////////////////////////////////////////////////////////////////
VolumeWidth_t Serum::getVolumeWidth() const

{
    __BEGIN_TRY

    return de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_SERUM, m_ItemType)->getVolumeWidth();

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////////////
// get height
//////////////////////////////////////////////////////////////////////////////
VolumeHeight_t Serum::getVolumeHeight() const

{
    __BEGIN_TRY

    return de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_SERUM, m_ItemType)->getVolumeHeight();

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////////////
// get weight
//////////////////////////////////////////////////////////////////////////////
Weight_t Serum::getWeight() const

{
    __BEGIN_TRY

    return de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_SERUM, m_ItemType)->getWeight();

    __END_CATCH
}

int Serum::getHPAmount(void) const

{
    SerumInfo* pInfo =
        dynamic_cast<SerumInfo*>(de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_SERUM, m_ItemType));
    return pInfo->getHPAmount();
}

int Serum::getPeriod(void) const

{
    SerumInfo* pInfo =
        dynamic_cast<SerumInfo*>(de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_SERUM, m_ItemType));
    return pInfo->getPeriod();
}

int Serum::getCount(void) const

{
    SerumInfo* pInfo =
        dynamic_cast<SerumInfo*>(de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_SERUM, m_ItemType));
    return pInfo->getCount();
}


//////////////////////////////////////////////////////////////////////////////
// parse effect string
//////////////////////////////////////////////////////////////////////////////
void SerumInfo::parseEffect(const string& effect)

{
    __BEGIN_TRY

    m_HPAmount = 0;
    m_Period = 0;
    m_Count = 0;

    size_t a = 0, b = 0, c = 0, d = 0;

    while (d < effect.size() - 1) {
        a = effect.find_first_of('(', d);
        b = effect.find_first_of(',', a + 1);
        c = effect.find_first_of(',', b + 1);
        d = effect.find_first_of(')', c + 1);

        if (a > b || b > c || c > d)
            break;

        string hpamount = trim(effect.substr(a + 1, b - a - 1));
        string period = trim(effect.substr(b + 1, c - b - 1));
        string count = trim(effect.substr(c + 1, d - c - 1));

        m_HPAmount = atoi(hpamount.c_str());
        m_Period = atoi(period.c_str());
        m_Count = atoi(count.c_str());
    }

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////////////
// get debug string
//////////////////////////////////////////////////////////////////////////////
string SerumInfo::toString() const

{
    StringStream msg;
    msg << "SerumInfo(" << "ItemType:" << m_ItemType << ",Name:" << m_Name << ",EName:" << m_EName
        << ",Price:" << m_Price << ",VolumeType:" << Volume2String[m_VolumeType] << ",Weight:" << m_Weight
        << ",Description:" << m_Description << ",HPAmount:" << m_HPAmount << ",Period:" << m_Period
        << ",Count:" << m_Count << ")";
    return msg.toString();
}


//////////////////////////////////////////////////////////////////////////////
// load from DB
//////////////////////////////////////////////////////////////////////////////
void SerumInfoManager::load()

{
    __BEGIN_TRY

    m_InfoCount = defaultItemObjectRepository().loadMaxGearType(GEAR_SERUM);

    m_pItemInfos = new ItemInfo*[m_InfoCount + 1];

    for (uint i = 0; i <= m_InfoCount; i++)
        m_pItemInfos[i] = NULL;

    vector<StringInfoRow> rows = defaultItemObjectRepository().loadStringInfos(GEAR_SERUM);

    for (size_t r = 0; r < rows.size(); r++) {
        SerumInfo* pSerumInfo = new SerumInfo();

        pSerumInfo->setItemType(rows[r].basic.itemType);
        pSerumInfo->setName(rows[r].basic.name);
        pSerumInfo->setEName(rows[r].basic.ename);
        pSerumInfo->setPrice(rows[r].basic.price);
        pSerumInfo->setVolumeType(rows[r].basic.volume);
        pSerumInfo->setWeight(rows[r].basic.weight);
        pSerumInfo->setRatio(rows[r].basic.ratio);
        pSerumInfo->parseEffect(rows[r].value);

        addItemInfo(pSerumInfo);
    }

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////////////
// load to creature
//////////////////////////////////////////////////////////////////////////////
void SerumLoader::load(Creature* pCreature)

{
    __BEGIN_TRY

    Assert(pCreature != NULL);

    vector<NumOnlyObjectRow> rows =
        defaultItemObjectRepository().loadNumOnlyItemOfOwner(GEAR_SERUM, pCreature->getName());

    for (size_t r = 0; r < rows.size(); r++) {
        try {
            Serum* pSerum = new Serum();

            pSerum->setItemID(rows[r].itemID);
            pSerum->setObjectID(rows[r].objectID);
            pSerum->setItemType(rows[r].itemType);

            Storage storage = (Storage)rows[r].storage;
            BYTE x = rows[r].x;
            BYTE y = rows[r].y;

            pSerum->setNum(rows[r].num);

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
                if (pInventory->canAddingEx(x, y, pSerum)) {
                    pInventory->addItemEx(x, y, pSerum);
                } else {
                    processItemBugEx(pCreature, pSerum);
                }
                break;

            case STORAGE_GEAR:
                processItemBugEx(pCreature, pSerum);
                break;

            case STORAGE_BELT:
                processItemBugEx(pCreature, pSerum);
                break;

            case STORAGE_EXTRASLOT:
                if (pCreature->isSlayer())
                    pSlayer->addItemToExtraInventorySlot(pSerum);
                else if (pCreature->isVampire())
                    pVampire->addItemToExtraInventorySlot(pSerum);
                break;

            case STORAGE_MOTORCYCLE:
                processItemBugEx(pCreature, pSerum);
                break;

            case STORAGE_STASH:
                if (pStash->isExist(x, y)) {
                    processItemBugEx(pCreature, pSerum);
                } else
                    pStash->insert(x, y, pSerum);
                break;

            case STORAGE_GARBAGE:
                processItemBug(pCreature, pSerum);
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


//////////////////////////////////////////////////////////////////////////////
// load to zone
//////////////////////////////////////////////////////////////////////////////
void SerumLoader::load(Zone* pZone)

{
    __BEGIN_TRY

    Assert(pZone != NULL);

    vector<NumOnlyZoneObjectRow> rows =
        defaultItemObjectRepository().loadNumOnlyItemInZone(GEAR_SERUM, (int)STORAGE_ZONE, pZone->getZoneID());

    for (size_t r = 0; r < rows.size(); r++) {
        Serum* pSerum = new Serum();

        pSerum->setItemID(rows[r].itemID);
        pSerum->setObjectID(rows[r].objectID);
        pSerum->setItemType(rows[r].itemType);

        Storage storage = (Storage)rows[r].storage;
        BYTE x = rows[r].x;
        BYTE y = rows[r].y;

        pSerum->setNum(rows[r].num);

        switch (storage) {
        case STORAGE_ZONE: {
            Tile& pTile = pZone->getTile(x, y);
            Assert(!pTile.hasItem());
            pTile.addItem(pSerum);
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


//////////////////////////////////////////////////////////////////////////////
// load to inventory
//////////////////////////////////////////////////////////////////////////////
void SerumLoader::load(StorageID_t storageID, Inventory* pInventory)

{
    __BEGIN_TRY

    Statement* pStmt;

    BEGIN_DB {}
    END_DB(pStmt)

    __END_CATCH
}
