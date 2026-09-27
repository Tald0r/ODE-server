
//////////////////////////////////////////////////////////////////////////////
// Filename    : ResurrectItem.cpp
// Written By  : excel96
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "ResurrectItem.h"

#include "Belt.h"
#include "DB.h"
#include "GameContext.h"
#include "ItemInfoManager.h"
#include "ItemUtil.h"
#include "Motorcycle.h"
#include "Ousters.h"
#include "Slayer.h"
#include "Stash.h"
#include "Utility.h"
#include "Vampire.h"
#include "repository/ItemObjectRepository.h"

ItemID_t ResurrectItem::m_ItemIDRegistry = 0;
Mutex ResurrectItem::m_Mutex;

//////////////////////////////////////////////////////////////////////////////
// class ResurrectItem member methods
//////////////////////////////////////////////////////////////////////////////

ResurrectItem::ResurrectItem()

{
    m_ItemType = 0;
}

ResurrectItem::ResurrectItem(ItemType_t itemType, const list<OptionType_t>& optionType, ItemNum_t Num)

{
    m_ItemType = itemType;
    m_Num = Num;

    if (!de::gameContext().itemInfos().isPossibleItem(getItemClass(), m_ItemType, optionType)) {
        filelog("itembug.log", "ResurrectItem::ResurrectItem() : Invalid item type or option type");
        throw Error("ResurrectItem::ResurrectItem() : Invalid item type or optionType");
    }
}

void ResurrectItem::create(const string& ownerID, Storage storage, StorageID_t storageID, BYTE x, BYTE y,
                           ItemID_t itemID)

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

    defaultItemObjectRepository().insertNumItem(GEAR_RESURRECT_ITEM, m_ItemID, m_ObjectID, m_ItemType, ownerID,
                                                (int)storage, storageID, (int)x, (int)y, (int)m_Num, (int)m_CreateType);

    __END_CATCH
}

//--------------------------------------------------------------------------------
// save item
//--------------------------------------------------------------------------------
void ResurrectItem::tinysave(const char* field) const

{
    __BEGIN_TRY

    defaultItemObjectRepository().tinysaveGear(GEAR_RESURRECT_ITEM, field, m_ItemID);

    __END_CATCH
}

void ResurrectItem::save(const string& ownerID, Storage storage, StorageID_t storageID, BYTE x, BYTE y)

{
    __BEGIN_TRY

    defaultItemObjectRepository().updateNumItem(GEAR_RESURRECT_ITEM, m_ObjectID, m_ItemType, ownerID, (int)storage,
                                                storageID, (int)x, (int)y, (int)m_Num, m_ItemID);

    __END_CATCH
}

string ResurrectItem::toString() const

{
    StringStream msg;

    msg << "ResurrectItem(" << "ItemID:" << m_ItemID << ",ItemType:" << (int)m_ItemType << ",Num:" << (int)m_Num << ")";

    return msg.toString();
}

VolumeWidth_t ResurrectItem::getVolumeWidth() const

{
    __BEGIN_TRY

    return de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_RESURRECT_ITEM, m_ItemType)->getVolumeWidth();

    __END_CATCH
}

VolumeHeight_t ResurrectItem::getVolumeHeight() const

{
    __BEGIN_TRY

    return de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_RESURRECT_ITEM, m_ItemType)->getVolumeHeight();

    __END_CATCH
}

Weight_t ResurrectItem::getWeight() const

{
    __BEGIN_TRY

    return de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_RESURRECT_ITEM, m_ItemType)->getWeight();

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
// class ResurrectItemInfo member methods
//////////////////////////////////////////////////////////////////////////////

string ResurrectItemInfo::toString() const

{
    StringStream msg;
    msg << "ResurrectItemInfo(" << "ItemType:" << (int)m_ItemType << ",Name:" << m_Name << ",EName:" << m_EName
        << ",Price:" << (int)m_Price << ",VolumeType:" << Volume2String[m_VolumeType] << ",Weight:" << (int)m_Weight
        << ",Description:" << m_Description << ")";
    return msg.toString();
}

void ResurrectItemInfoManager::load()

{
    __BEGIN_TRY

    m_InfoCount = defaultItemObjectRepository().loadMaxGearType(GEAR_RESURRECT_ITEM);

    m_pItemInfos = new ItemInfo*[m_InfoCount + 1];

    for (uint i = 0; i <= m_InfoCount; i++)
        m_pItemInfos[i] = NULL;

    vector<ResurrectInfoRow> rows = defaultItemObjectRepository().loadResurrectInfos(GEAR_RESURRECT_ITEM);

    for (size_t r = 0; r < rows.size(); r++) {
        ResurrectItemInfo* pResurrectItemInfo = new ResurrectItemInfo();

        pResurrectItemInfo->setItemType(rows[r].basic.itemType);
        pResurrectItemInfo->setName(rows[r].basic.name);
        pResurrectItemInfo->setEName(rows[r].basic.ename);
        pResurrectItemInfo->setPrice(rows[r].basic.price);
        pResurrectItemInfo->setVolumeType(rows[r].basic.volume);
        pResurrectItemInfo->setWeight(rows[r].basic.weight);
        pResurrectItemInfo->setRatio(rows[r].basic.ratio);
        pResurrectItemInfo->setResurrectType((ResurrectItemInfo::ResurrectType)rows[r].resurrectType);

        addItemInfo(pResurrectItemInfo);
    }

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
// class ResurrectItemLoader member methods
//////////////////////////////////////////////////////////////////////////////

void ResurrectItemLoader::load(Creature* pCreature)

{
    __BEGIN_TRY

    Assert(pCreature != NULL);

    vector<NumObjectRow> rows =
        defaultItemObjectRepository().loadNumItemOfOwner(GEAR_RESURRECT_ITEM, pCreature->getName());

    for (size_t r = 0; r < rows.size(); r++) {
        try {
            ResurrectItem* pResurrectItem = new ResurrectItem();

            pResurrectItem->setItemID(rows[r].itemID);
            pResurrectItem->setObjectID(rows[r].objectID);
            pResurrectItem->setItemType(rows[r].itemType);

            Storage storage = (Storage)rows[r].storage;
            BYTE x = rows[r].x;
            BYTE y = rows[r].y;

            pResurrectItem->setNum(rows[r].num);
            pResurrectItem->setCreateType((Item::CreateType)rows[r].createType);

            Inventory* pInventory = NULL;
            Slayer* pSlayer = NULL;
            Vampire* pVampire = NULL;
            Ousters* pOusters = NULL;
            Stash* pStash = NULL;

            if (pCreature->isSlayer()) {
                pSlayer = dynamic_cast<Slayer*>(pCreature);
                pInventory = pSlayer->getInventory();
                pStash = pSlayer->getStash();
            } else if (pCreature->isVampire()) {
                pVampire = dynamic_cast<Vampire*>(pCreature);
                pInventory = pVampire->getInventory();
                pStash = pVampire->getStash();
            } else if (pCreature->isOusters()) {
                pOusters = dynamic_cast<Ousters*>(pCreature);
                pInventory = pOusters->getInventory();
                pStash = pOusters->getStash();
            } else
                throw UnsupportedError("Saving Monster/NPC inventories is not supported.");

            switch (storage) {
            case STORAGE_INVENTORY:
                if (pInventory->canAddingEx(x, y, pResurrectItem)) {
                    pInventory->addItemEx(x, y, pResurrectItem);
                } else {
                    processItemBugEx(pCreature, pResurrectItem);
                }
                break;

            case STORAGE_GEAR:
                processItemBugEx(pCreature, pResurrectItem);
                break;

            case STORAGE_BELT:
                processItemBugEx(pCreature, pResurrectItem);
                break;

            case STORAGE_EXTRASLOT:
                if (pCreature->isSlayer())
                    pSlayer->addItemToExtraInventorySlot(pResurrectItem);
                else if (pCreature->isVampire())
                    pVampire->addItemToExtraInventorySlot(pResurrectItem);
                else if (pCreature->isOusters())
                    pOusters->addItemToExtraInventorySlot(pResurrectItem);
                break;

            case STORAGE_MOTORCYCLE:
                processItemBugEx(pCreature, pResurrectItem);
                break;

            case STORAGE_STASH:
                if (pStash->isExist(x, y)) {
                    processItemBugEx(pCreature, pResurrectItem);
                } else
                    pStash->insert(x, y, pResurrectItem);
                break;

            case STORAGE_GARBAGE:
                processItemBug(pCreature, pResurrectItem);
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

void ResurrectItemLoader::load(Zone* pZone)

{
    __BEGIN_TRY

    Assert(pZone != NULL);

    vector<NumZoneObjectRow> rows =
        defaultItemObjectRepository().loadNumItemInZone(GEAR_RESURRECT_ITEM, (int)STORAGE_ZONE, pZone->getZoneID());

    for (size_t r = 0; r < rows.size(); r++) {
        ResurrectItem* pResurrectItem = new ResurrectItem();

        pResurrectItem->setItemID(rows[r].itemID);
        pResurrectItem->setObjectID(rows[r].objectID);
        pResurrectItem->setItemType(rows[r].itemType);

        Storage storage = (Storage)rows[r].storage;
        BYTE x = rows[r].x;
        BYTE y = rows[r].y;

        pResurrectItem->setNum(rows[r].num);
        pResurrectItem->setCreateType((Item::CreateType)rows[r].createType);

        switch (storage) {
        case STORAGE_ZONE: {
            Tile& pTile = pZone->getTile(x, y);
            Assert(!pTile.hasItem());
            pTile.addItem(pResurrectItem);
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

void ResurrectItemLoader::load(StorageID_t storageID, Inventory* pInventory)

{
    __BEGIN_TRY

    Statement* pStmt;

    BEGIN_DB {}
    END_DB(pStmt)

    __END_CATCH
}
