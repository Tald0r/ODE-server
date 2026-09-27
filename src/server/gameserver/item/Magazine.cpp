//////////////////////////////////////////////////////////////////////////////
// Filename    : Magazine.cpp
// Written By  : Elca
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "Magazine.h"

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

ItemID_t Magazine::m_ItemIDRegistry = 0;
Mutex Magazine::m_Mutex;

//--------------------------------------------------------------------------------
// constructor
//--------------------------------------------------------------------------------
Magazine::Magazine()

    : m_ItemType(0) {}

Magazine::Magazine(ItemType_t itemType, const list<OptionType_t>& optionType, ItemNum_t Num)

    : m_ItemType(itemType), m_Num(Num) {
    if (!de::gameContext().itemInfos().isPossibleItem(getItemClass(), m_ItemType, optionType)) {
        filelog("itembug.log", "Magazine::Magazine() : Invalid item type or option type");
        throw Error("Magazine::Magazine() : Invalid item type or optionType");
    }
}


//--------------------------------------------------------------------------------
// create item
//--------------------------------------------------------------------------------
void Magazine::create(const string& ownerID, Storage storage, StorageID_t storageID, BYTE x, BYTE y, ItemID_t itemID)

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

    defaultItemObjectRepository().insertNumOnlyItem(GEAR_MAGAZINE, m_ItemID, m_ObjectID, m_ItemType, ownerID,
                                                    (int)storage, storageID, (int)x, (int)y, (int)m_Num);

    __END_CATCH
}


//--------------------------------------------------------------------------------
// save item
//--------------------------------------------------------------------------------
void Magazine::tinysave(const char* field) const

{
    __BEGIN_TRY

    defaultItemObjectRepository().tinysaveGear(GEAR_MAGAZINE, field, m_ItemID);

    __END_CATCH
}

//--------------------------------------------------------------------------------
// save item
//--------------------------------------------------------------------------------
void Magazine::save(const string& ownerID, Storage storage, StorageID_t storageID, BYTE x, BYTE y)

{
    __BEGIN_TRY

    defaultItemObjectRepository().updateNumOnlyItem(GEAR_MAGAZINE, m_ObjectID, m_ItemType, ownerID, (int)storage,
                                                    storageID, (int)x, (int)y, (int)m_Num, m_ItemID);

    __END_CATCH
}


//--------------------------------------------------------------------------------
// get debug string
//--------------------------------------------------------------------------------
string Magazine::toString() const

{
    StringStream msg;

    msg << "Magazine(" << "ItemID:" << m_ItemID << ",ItemType:" << (int)m_ItemType << ",Num:" << (int)m_Num << ")";

    return msg.toString();
}


//--------------------------------------------------------------------------------
// get width
//--------------------------------------------------------------------------------
VolumeWidth_t Magazine::getVolumeWidth() const

{
    __BEGIN_TRY

    return de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_MAGAZINE, m_ItemType)->getVolumeWidth();

    __END_CATCH
}


//--------------------------------------------------------------------------------
// get height
//--------------------------------------------------------------------------------
VolumeHeight_t Magazine::getVolumeHeight() const

{
    __BEGIN_TRY

    return de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_MAGAZINE, m_ItemType)->getVolumeHeight();

    __END_CATCH
}


//--------------------------------------------------------------------------------
// get weight
//--------------------------------------------------------------------------------
Weight_t Magazine::getWeight() const

{
    __BEGIN_TRY

    return de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_MAGAZINE, m_ItemType)->getWeight();

    __END_CATCH
}


//--------------------------------------------------------------------------------
// get debug string
//--------------------------------------------------------------------------------
string MagazineInfo::toString() const

{
    StringStream msg;

    msg << "MagazineInfo(" << "ItemType:" << m_ItemType << ",Name:" << m_Name << ",EName:" << m_EName
        << ",Price:" << m_Price << ",VolumeType:" << Volume2String[m_VolumeType] << ",Weight:" << m_Weight
        << ",Description:" << m_Description << ",maxBullets:" << m_MaxBullets << ")";

    return msg.toString();
}


//--------------------------------------------------------------------------------
// load from DB
//--------------------------------------------------------------------------------
void MagazineInfoManager::load()

{
    __BEGIN_TRY

    m_InfoCount = defaultItemObjectRepository().loadMaxGearType(GEAR_MAGAZINE);

    m_pItemInfos = new ItemInfo*[m_InfoCount + 1];

    for (uint i = 0; i <= m_InfoCount; i++)
        m_pItemInfos[i] = NULL;

    vector<MagazineInfoRow> rows = defaultItemObjectRepository().loadMagazineInfos(GEAR_MAGAZINE);

    for (size_t r = 0; r < rows.size(); r++) {
        MagazineInfo* pMagazineInfo = new MagazineInfo();

        pMagazineInfo->setItemType(rows[r].basic.itemType);
        pMagazineInfo->setName(rows[r].basic.name);
        pMagazineInfo->setEName(rows[r].basic.ename);
        pMagazineInfo->setPrice(rows[r].basic.price);
        pMagazineInfo->setVolumeType(rows[r].basic.volume);
        pMagazineInfo->setWeight(rows[r].basic.weight);
        pMagazineInfo->setRatio(rows[r].basic.ratio);
        pMagazineInfo->setItemLevel(rows[r].itemLevel);
        pMagazineInfo->setMaxBullets(rows[r].maxBullets);
        pMagazineInfo->setMaxSilver(rows[r].maxSilverBullets);
        pMagazineInfo->setVivid(rows[r].vivid != 0);
        pMagazineInfo->setGunType((MagazineInfo::GunType)rows[r].gunType);

        addItemInfo(pMagazineInfo);
    }

    __END_CATCH
}


//--------------------------------------------------------------------------------
// load to creature
//--------------------------------------------------------------------------------
void MagazineLoader::load(Creature* pCreature)

{
    __BEGIN_TRY

    Assert(pCreature != NULL);

    vector<NumOnlyObjectRow> rows =
        defaultItemObjectRepository().loadNumOnlyItemOfOwner(GEAR_MAGAZINE, pCreature->getName());

    for (size_t r = 0; r < rows.size(); r++) {
        try {
            Magazine* pMagazine = new Magazine();

            pMagazine->setItemID(rows[r].itemID);
            pMagazine->setObjectID(rows[r].objectID);
            pMagazine->setItemType(rows[r].itemType);

            Storage storage = (Storage)rows[r].storage;
            StorageID_t storageID = rows[r].storageID;
            BYTE x = rows[r].x;
            BYTE y = rows[r].y;

            pMagazine->setNum(rows[r].num);

            Inventory* pInventory = NULL;
            Slayer* pSlayer = NULL;
            Vampire* pVampire = NULL;
            Item* pItem = NULL;
            Stash* pStash = NULL;
            Belt* pBelt = NULL;
            Inventory* pBeltInventory = NULL;

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
                if (pInventory->canAddingEx(x, y, pMagazine)) {
                    pInventory->addItemEx(x, y, pMagazine);
                } else {
                    processItemBugEx(pCreature, pMagazine);
                }
                break;

            case STORAGE_GEAR:
                processItemBugEx(pCreature, pMagazine);
                break;

            case STORAGE_BELT:
                if (pCreature->isSlayer()) {
                    pItem = pSlayer->findBeltIID(storageID);
                    if (pItem != NULL && pItem->getItemClass() == Item::ITEM_CLASS_BELT) {
                        pBelt = dynamic_cast<Belt*>(pItem);
                        pBeltInventory = pBelt->getInventory();
                        if (pBeltInventory->canAddingEx(x, 0, pMagazine)) {
                            pBeltInventory->addItem(x, 0, pMagazine);
                        } else {
                            processItemBugEx(pCreature, pMagazine);
                        }
                    } else {
                        processItemBugEx(pCreature, pMagazine);
                    }
                } else if (pCreature->isVampire()) {
                    pItem = pVampire->findBeltIID(storageID);
                    if (pItem != NULL && pItem->getItemClass() == Item::ITEM_CLASS_BELT) {
                        pBelt = dynamic_cast<Belt*>(pItem);
                        pBeltInventory = pBelt->getInventory();
                        if (pBeltInventory->canAddingEx(x, 0, pMagazine)) {
                            pBeltInventory->addItemEx(x, 0, pMagazine);
                        } else {
                            processItemBugEx(pCreature, pMagazine);
                        }
                    } else {
                        processItemBugEx(pCreature, pMagazine);
                    }
                }
                break;

            case STORAGE_EXTRASLOT:
                if (pCreature->isSlayer())
                    pSlayer->addItemToExtraInventorySlot(pMagazine);
                else if (pCreature->isVampire())
                    pVampire->addItemToExtraInventorySlot(pMagazine);
                break;

            case STORAGE_MOTORCYCLE:
                processItemBugEx(pCreature, pMagazine);
                break;

            case STORAGE_STASH:
                if (pStash->isExist(x, y)) {
                    processItemBugEx(pCreature, pMagazine);
                } else
                    pStash->insert(x, y, pMagazine);
                break;

            case STORAGE_GARBAGE:
                processItemBug(pCreature, pMagazine);
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
void MagazineLoader::load(Zone* pZone)

{
    __BEGIN_TRY

    Assert(pZone != NULL);

    vector<NumOnlyZoneObjectRow> rows =
        defaultItemObjectRepository().loadNumOnlyItemInZone(GEAR_MAGAZINE, (int)STORAGE_ZONE, pZone->getZoneID());

    for (size_t r = 0; r < rows.size(); r++) {
        Magazine* pMagazine = new Magazine();

        pMagazine->setItemID(rows[r].itemID);
        pMagazine->setObjectID(rows[r].objectID);
        pMagazine->setItemType(rows[r].itemType);

        Storage storage = (Storage)rows[r].storage;
        BYTE x = rows[r].x;
        BYTE y = rows[r].y;

        pMagazine->setNum(rows[r].num);

        switch (storage) {
        case STORAGE_ZONE: {
            Tile& pTile = pZone->getTile(x, y);
            Assert(!pTile.hasItem());
            pTile.addItem(pMagazine);
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
void MagazineLoader::load(StorageID_t storageID, Inventory* pInventory)

{
    __BEGIN_TRY

    Statement* pStmt;

    BEGIN_DB {}
    END_DB(pStmt)

    __END_CATCH
}
