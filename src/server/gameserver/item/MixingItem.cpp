//////////////////////////////////////////////////////////////////////////////
// Filename    : MixingItem.cpp
// Written By  : excel96
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "MixingItem.h"

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

ItemID_t MixingItem::m_ItemIDRegistry = 0;
Mutex MixingItem::m_Mutex;

//////////////////////////////////////////////////////////////////////////////
// class MixingItem member methods
//////////////////////////////////////////////////////////////////////////////

MixingItem::MixingItem()

{
    m_ItemType = 0;
}

MixingItem::MixingItem(ItemType_t itemType, const list<OptionType_t>& optionType, ItemNum_t Num)

{
    m_ItemType = itemType;
    m_Num = Num;

    if (!de::gameContext().itemInfos().isPossibleItem(getItemClass(), m_ItemType, optionType)) {
        filelog("itembug.log", "MixingItem::MixingItem() : Invalid item type or option type");
        throw Error("MixingItem::MixingItem() : Invalid item type or optionType");
    }
}

void MixingItem::create(const string& ownerID, Storage storage, StorageID_t storageID, BYTE x, BYTE y, ItemID_t itemID)

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

    defaultItemObjectRepository().insertNumItem(GEAR_MIXING_ITEM, m_ItemID, m_ObjectID, m_ItemType, ownerID,
                                                (int)storage, storageID, (int)x, (int)y, (int)m_Num, (int)m_CreateType);

    __END_CATCH
}

//--------------------------------------------------------------------------------
// save item
//--------------------------------------------------------------------------------
void MixingItem::tinysave(const char* field) const

{
    __BEGIN_TRY

    defaultItemObjectRepository().tinysaveGear(GEAR_MIXING_ITEM, field, m_ItemID);

    __END_CATCH
}

void MixingItem::save(const string& ownerID, Storage storage, StorageID_t storageID, BYTE x, BYTE y)

{
    __BEGIN_TRY

    defaultItemObjectRepository().updateNumItem(GEAR_MIXING_ITEM, m_ObjectID, m_ItemType, ownerID, (int)storage,
                                                storageID, (int)x, (int)y, (int)m_Num, m_ItemID);

    __END_CATCH
}

string MixingItem::toString() const

{
    StringStream msg;

    msg << "MixingItem(" << "ItemID:" << m_ItemID << ",ItemType:" << (int)m_ItemType << ",Num:" << (int)m_Num << ")";

    return msg.toString();
}

VolumeWidth_t MixingItem::getVolumeWidth() const

{
    __BEGIN_TRY

    return de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_MIXING_ITEM, m_ItemType)->getVolumeWidth();

    __END_CATCH
}

VolumeHeight_t MixingItem::getVolumeHeight() const

{
    __BEGIN_TRY

    return de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_MIXING_ITEM, m_ItemType)->getVolumeHeight();

    __END_CATCH
}

Weight_t MixingItem::getWeight() const

{
    __BEGIN_TRY

    return de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_MIXING_ITEM, m_ItemType)->getWeight();

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
// class MixingItemInfo member methods
//////////////////////////////////////////////////////////////////////////////

string MixingItemInfo::toString() const

{
    StringStream msg;
    msg << "MixingItemInfo(" << "ItemType:" << (int)m_ItemType << ",Name:" << m_Name << ",EName:" << m_EName
        << ",Price:" << (int)m_Price << ",VolumeType:" << Volume2String[m_VolumeType] << ",Weight:" << (int)m_Weight
        << ",Description:" << m_Description << ")";
    return msg.toString();
}

void MixingItemInfoManager::load()

{
    __BEGIN_TRY

    m_InfoCount = defaultItemObjectRepository().loadMaxGearType(GEAR_MIXING_ITEM);

    m_pItemInfos = new ItemInfo*[m_InfoCount + 1];

    for (uint i = 0; i <= m_InfoCount; i++)
        m_pItemInfos[i] = NULL;

    vector<MixingItemInfoRow> rows = defaultItemObjectRepository().loadMixingItemInfos(GEAR_MIXING_ITEM);

    for (size_t r = 0; r < rows.size(); r++) {
        MixingItemInfo* pMixingItemInfo = new MixingItemInfo();

        pMixingItemInfo->setItemType(rows[r].head.itemType);
        pMixingItemInfo->setName(rows[r].head.name);
        pMixingItemInfo->setEName(rows[r].head.ename);
        pMixingItemInfo->setPrice(rows[r].head.price);
        pMixingItemInfo->setVolumeType(rows[r].head.volume);
        pMixingItemInfo->setWeight(rows[r].head.weight);
        pMixingItemInfo->setTarget((MixingItemInfo::Target)rows[r].target);
        pMixingItemInfo->setType((MixingItemInfo::Type)rows[r].type);
        pMixingItemInfo->setSlayerLevel(rows[r].slayerLevel);
        pMixingItemInfo->setVampireLevel(rows[r].vampireLevel);
        pMixingItemInfo->setOustersLevel(rows[r].oustersLevel);

        addItemInfo(pMixingItemInfo);
    }

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
// class MixingItemLoader member methods
//////////////////////////////////////////////////////////////////////////////

void MixingItemLoader::load(Creature* pCreature)

{
    __BEGIN_TRY

    Assert(pCreature != NULL);

    vector<NumIntObjectRow> rows =
        defaultItemObjectRepository().loadNumIntItemOfOwner(GEAR_MIXING_ITEM, pCreature->getName());

    for (size_t r = 0; r < rows.size(); r++) {
        try {
            MixingItem* pMixingItem = new MixingItem();

            pMixingItem->setItemID(rows[r].itemID);
            pMixingItem->setObjectID(rows[r].objectID);
            pMixingItem->setItemType(rows[r].itemType);

            Storage storage = (Storage)rows[r].storage;
            BYTE x = rows[r].x;
            BYTE y = rows[r].y;

            pMixingItem->setNum(rows[r].num);
            pMixingItem->setCreateType((Item::CreateType)rows[r].createType);

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
                if (pInventory->canAddingEx(x, y, pMixingItem)) {
                    pInventory->addItemEx(x, y, pMixingItem);
                } else {
                    processItemBugEx(pCreature, pMixingItem);
                }
                break;

            case STORAGE_GEAR:
                processItemBugEx(pCreature, pMixingItem);
                break;

            case STORAGE_BELT:
                processItemBugEx(pCreature, pMixingItem);
                break;

            case STORAGE_EXTRASLOT:
                if (pCreature->isSlayer())
                    pSlayer->addItemToExtraInventorySlot(pMixingItem);
                else if (pCreature->isVampire())
                    pVampire->addItemToExtraInventorySlot(pMixingItem);
                else if (pCreature->isOusters())
                    pOusters->addItemToExtraInventorySlot(pMixingItem);
                break;

            case STORAGE_MOTORCYCLE:
                processItemBugEx(pCreature, pMixingItem);
                break;

            case STORAGE_STASH:
                if (pStash->isExist(x, y)) {
                    processItemBugEx(pCreature, pMixingItem);
                } else
                    pStash->insert(x, y, pMixingItem);
                break;

            case STORAGE_GARBAGE:
                processItemBug(pCreature, pMixingItem);
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

void MixingItemLoader::load(Zone* pZone)

{
    __BEGIN_TRY

    Assert(pZone != NULL);

    vector<NumIntZoneObjectRow> rows =
        defaultItemObjectRepository().loadNumIntItemInZone(GEAR_MIXING_ITEM, (int)STORAGE_ZONE, pZone->getZoneID());

    for (size_t r = 0; r < rows.size(); r++) {
        MixingItem* pMixingItem = new MixingItem();

        pMixingItem->setItemID(rows[r].itemID);
        pMixingItem->setObjectID(rows[r].objectID);
        pMixingItem->setItemType(rows[r].itemType);

        Storage storage = (Storage)rows[r].storage;
        BYTE x = rows[r].x;
        BYTE y = rows[r].y;

        pMixingItem->setNum(rows[r].num);
        pMixingItem->setCreateType((Item::CreateType)rows[r].createType);

        switch (storage) {
        case STORAGE_ZONE: {
            Tile& pTile = pZone->getTile(x, y);
            Assert(!pTile.hasItem());
            pTile.addItem(pMixingItem);
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

void MixingItemLoader::load(StorageID_t storageID, Inventory* pInventory)

{
    __BEGIN_TRY

    Statement* pStmt;

    BEGIN_DB {}
    END_DB(pStmt)

    __END_CATCH
}
