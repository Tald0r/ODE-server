//////////////////////////////////////////////////////////////////////////////
// Filename    : TrapItem.cpp
// Written By  : excel96
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "TrapItem.h"

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

ItemID_t TrapItem::m_ItemIDRegistry = 0;
Mutex TrapItem::m_Mutex;

//////////////////////////////////////////////////////////////////////////////
// class TrapItem member methods
//////////////////////////////////////////////////////////////////////////////

TrapItem::TrapItem()

{
    m_ItemType = 0;
}

TrapItem::TrapItem(ItemType_t itemType, const list<OptionType_t>& optionType)

{
    m_ItemType = itemType;

    if (!de::gameContext().itemInfos().isPossibleItem(getItemClass(), m_ItemType, optionType)) {
        filelog("itembug.log", "TrapItem::TrapItem() : Invalid item type or option type");
        throw Error("TrapItem::TrapItem() : Invalid item type or optionType");
    }
}

void TrapItem::create(const string& ownerID, Storage storage, StorageID_t storageID, BYTE x, BYTE y, ItemID_t itemID)

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

    defaultItemObjectRepository().insertFlagItem(GEAR_TRAP_ITEM, m_ItemID, m_ObjectID, m_ItemType, ownerID,
                                                 (int)storage, storageID, (int)x, (int)y, (int)m_CreateType);

    __END_CATCH
}

//--------------------------------------------------------------------------------
// save item
//--------------------------------------------------------------------------------
void TrapItem::tinysave(const char* field) const

{
    __BEGIN_TRY

    defaultItemObjectRepository().tinysaveGear(GEAR_TRAP_ITEM, field, m_ItemID);

    __END_CATCH
}

void TrapItem::save(const string& ownerID, Storage storage, StorageID_t storageID, BYTE x, BYTE y)

{
    __BEGIN_TRY

    defaultItemObjectRepository().updatePlainItem(GEAR_TRAP_ITEM, m_ObjectID, m_ItemType, ownerID, (int)storage,
                                                  storageID, (int)x, (int)y, m_ItemID);

    __END_CATCH
}

string TrapItem::toString() const

{
    StringStream msg;

    msg << "TrapItem(" << "ItemID:" << m_ItemID << ",ItemType:" << (int)m_ItemType << ")";

    return msg.toString();
}

VolumeWidth_t TrapItem::getVolumeWidth() const

{
    __BEGIN_TRY

    return de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_TRAP_ITEM, m_ItemType)->getVolumeWidth();

    __END_CATCH
}

VolumeHeight_t TrapItem::getVolumeHeight() const

{
    __BEGIN_TRY

    return de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_TRAP_ITEM, m_ItemType)->getVolumeHeight();

    __END_CATCH
}

Weight_t TrapItem::getWeight() const

{
    __BEGIN_TRY

    return de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_TRAP_ITEM, m_ItemType)->getWeight();

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
// class TrapItemInfo member methods
//////////////////////////////////////////////////////////////////////////////

string TrapItemInfo::toString() const

{
    StringStream msg;
    msg << "TrapItemInfo(" << "ItemType:" << (int)m_ItemType << ",Name:" << m_Name << ",EName:" << m_EName
        << ",Price:" << (int)m_Price << ",VolumeType:" << Volume2String[m_VolumeType] << ",Weight:" << (int)m_Weight
        << ",Description:" << m_Description << ")";
    return msg.toString();
}

void TrapItemInfoManager::load()

{
    __BEGIN_TRY

    m_InfoCount = defaultItemObjectRepository().loadMaxGearType(GEAR_TRAP_ITEM);

    m_pItemInfos = new ItemInfo*[m_InfoCount + 1];

    for (uint i = 0; i <= m_InfoCount; i++)
        m_pItemInfos[i] = NULL;

    vector<IntPairInfoRow> rows = defaultItemObjectRepository().loadIntPairInfos(GEAR_TRAP_ITEM);

    for (size_t r = 0; r < rows.size(); r++) {
        TrapItemInfo* pTrapItemInfo = new TrapItemInfo();

        pTrapItemInfo->setItemType(rows[r].basic.itemType);
        pTrapItemInfo->setName(rows[r].basic.name);
        pTrapItemInfo->setEName(rows[r].basic.ename);
        pTrapItemInfo->setPrice(rows[r].basic.price);
        pTrapItemInfo->setVolumeType(rows[r].basic.volume);
        pTrapItemInfo->setWeight(rows[r].basic.weight);
        pTrapItemInfo->setRatio(rows[r].basic.ratio);
        pTrapItemInfo->setFunction(rows[r].first);
        pTrapItemInfo->setParameter(rows[r].second);

        addItemInfo(pTrapItemInfo);
    }

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
// class TrapItemLoader member methods
//////////////////////////////////////////////////////////////////////////////

void TrapItemLoader::load(Creature* pCreature)

{
    __BEGIN_TRY

    Assert(pCreature != NULL);

    vector<FlagObjectRow> rows =
        defaultItemObjectRepository().loadFlagItemOfOwner(GEAR_TRAP_ITEM, pCreature->getName());

    for (size_t r = 0; r < rows.size(); r++) {
        try {
            TrapItem* pTrapItem = new TrapItem();

            pTrapItem->setItemID(rows[r].itemID);
            pTrapItem->setObjectID(rows[r].objectID);
            pTrapItem->setItemType(rows[r].itemType);

            Storage storage = (Storage)rows[r].storage;
            BYTE x = rows[r].x;
            BYTE y = rows[r].y;

            pTrapItem->setCreateType((Item::CreateType)rows[r].createType);

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
                if (pInventory->canAddingEx(x, y, pTrapItem)) {
                    pInventory->addItemEx(x, y, pTrapItem);
                } else {
                    processItemBugEx(pCreature, pTrapItem);
                }
                break;

            case STORAGE_GEAR:
                processItemBugEx(pCreature, pTrapItem);
                break;

            case STORAGE_BELT:
                processItemBugEx(pCreature, pTrapItem);
                break;

            case STORAGE_EXTRASLOT:
                if (pCreature->isSlayer())
                    pSlayer->addItemToExtraInventorySlot(pTrapItem);
                else if (pCreature->isVampire())
                    pVampire->addItemToExtraInventorySlot(pTrapItem);
                else if (pCreature->isOusters())
                    pOusters->addItemToExtraInventorySlot(pTrapItem);
                break;

            case STORAGE_MOTORCYCLE:
                processItemBugEx(pCreature, pTrapItem);
                break;

            case STORAGE_STASH:
                if (pStash->isExist(x, y)) {
                    processItemBugEx(pCreature, pTrapItem);
                } else
                    pStash->insert(x, y, pTrapItem);
                break;

            case STORAGE_GARBAGE:
                processItemBug(pCreature, pTrapItem);
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

void TrapItemLoader::load(Zone* pZone)

{
    __BEGIN_TRY

    Assert(pZone != NULL);

    vector<FlagZoneObjectRow> rows =
        defaultItemObjectRepository().loadFlagItemInZone(GEAR_TRAP_ITEM, (int)STORAGE_ZONE, pZone->getZoneID());

    for (size_t r = 0; r < rows.size(); r++) {
        TrapItem* pTrapItem = new TrapItem();

        pTrapItem->setItemID(rows[r].itemID);
        pTrapItem->setObjectID(rows[r].objectID);
        pTrapItem->setItemType(rows[r].itemType);

        Storage storage = (Storage)rows[r].storage;
        BYTE x = rows[r].x;
        BYTE y = rows[r].y;

        pTrapItem->setCreateType((Item::CreateType)rows[r].createType);

        switch (storage) {
        case STORAGE_ZONE: {
            Tile& pTile = pZone->getTile(x, y);
            Assert(!pTile.hasItem());
            pTile.addItem(pTrapItem);
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

void TrapItemLoader::load(StorageID_t storageID, Inventory* pInventory)

{
    __BEGIN_TRY

    Statement* pStmt;

    BEGIN_DB {}
    END_DB(pStmt)

    __END_CATCH
}
