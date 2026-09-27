//////////////////////////////////////////////////////////////////////////////
// Filename    : OustersSummonItem.cpp
// Written By  : excel96
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "OustersSummonItem.h"

#include "Belt.h"
#include "DB.h"
#include "GameContext.h"
#include "ItemInfoManager.h"
#include "ItemUtil.h"
#include "Motorcycle.h"
#include "Ousters.h"
#include "Slayer.h"
#include "Stash.h"
#include "Vampire.h"
#include "repository/ItemObjectRepository.h"

ItemID_t OustersSummonItem::m_ItemIDRegistry = 0;
Mutex OustersSummonItem::m_Mutex;

//////////////////////////////////////////////////////////////////////////////
// class OustersSummonItem member methods
//////////////////////////////////////////////////////////////////////////////

OustersSummonItem::OustersSummonItem()

{
    m_ItemType = 0;
    m_Charge = 0;
}

OustersSummonItem::OustersSummonItem(ItemType_t itemType, const list<OptionType_t>& optionType)

{
    m_ItemType = itemType;
    m_Charge = getMaxCharge();

    if (!de::gameContext().itemInfos().isPossibleItem(getItemClass(), m_ItemType, optionType)) {
        filelog("itembug.log", "OustersSummonItem::OustersSummonItem() : Invalid item type or option type");
        throw Error("OustersSummonItem::OustersSummonItem() : Invalid item type or optionType");
    }
}

void OustersSummonItem::create(const string& ownerID, Storage storage, StorageID_t storageID, BYTE x, BYTE y,
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

    defaultItemObjectRepository().insertChargeItem(GEAR_OUSTERS_SUMMON_ITEM, m_ItemID, m_ObjectID, m_ItemType, ownerID,
                                                   (int)storage, storageID, (int)x, (int)y, m_Charge);

    __END_CATCH
}

//--------------------------------------------------------------------------------
// save item
//--------------------------------------------------------------------------------
void OustersSummonItem::tinysave(const char* field) const

{
    __BEGIN_TRY

    defaultItemObjectRepository().tinysaveGear(GEAR_OUSTERS_SUMMON_ITEM, field, m_ItemID);

    __END_CATCH
}

void OustersSummonItem::save(const string& ownerID, Storage storage, StorageID_t storageID, BYTE x, BYTE y)

{
    __BEGIN_TRY

    defaultItemObjectRepository().updateChargeItem(GEAR_OUSTERS_SUMMON_ITEM, m_ObjectID, m_ItemType, ownerID,
                                                   (int)storage, storageID, (int)x, (int)y, m_Charge, m_ItemID);

    __END_CATCH
}

VolumeWidth_t OustersSummonItem::getVolumeWidth() const

{
    __BEGIN_TRY

    return de::gameContext()
        .itemInfos()
        .getItemInfo(Item::ITEM_CLASS_OUSTERS_SUMMON_ITEM, m_ItemType)
        ->getVolumeWidth();

    __END_CATCH
}

VolumeHeight_t OustersSummonItem::getVolumeHeight() const

{
    __BEGIN_TRY

    return de::gameContext()
        .itemInfos()
        .getItemInfo(Item::ITEM_CLASS_OUSTERS_SUMMON_ITEM, m_ItemType)
        ->getVolumeHeight();

    __END_CATCH
}

Weight_t OustersSummonItem::getWeight() const

{
    __BEGIN_TRY

    return de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_OUSTERS_SUMMON_ITEM, m_ItemType)->getWeight();

    __END_CATCH
}

string OustersSummonItem::toString() const

{
    __BEGIN_TRY

    StringStream msg;
    msg << "OustersSummonItem(" << "ItemID:" << m_ItemID << ",ItemType:" << (int)m_ItemType << ",Charge:" << m_Charge
        << ")";

    return msg.toString();

    __END_CATCH
}

int OustersSummonItem::getMaxCharge(void) const

{
    __BEGIN_TRY

    OustersSummonItemInfo* pInfo = dynamic_cast<OustersSummonItemInfo*>(
        de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_OUSTERS_SUMMON_ITEM, m_ItemType));
    Assert(pInfo != NULL);
    return pInfo->getMaxCharge();

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
// class OustersSummonItemInfo member methods
//////////////////////////////////////////////////////////////////////////////

void OustersSummonItemInfoManager::load()

{
    __BEGIN_TRY

    m_InfoCount = defaultItemObjectRepository().loadMaxGearType(GEAR_OUSTERS_SUMMON_ITEM);

    m_pItemInfos = new ItemInfo*[m_InfoCount + 1];

    for (uint i = 0; i <= m_InfoCount; i++)
        m_pItemInfos[i] = NULL;

    vector<SummonItemInfoRow> rows = defaultItemObjectRepository().loadSummonItemInfos(GEAR_OUSTERS_SUMMON_ITEM);

    for (size_t r = 0; r < rows.size(); r++) {
        OustersSummonItemInfo* pOustersSummonItemInfo = new OustersSummonItemInfo();

        pOustersSummonItemInfo->setItemType(rows[r].head.itemType);
        pOustersSummonItemInfo->setName(rows[r].head.name);
        pOustersSummonItemInfo->setEName(rows[r].head.ename);
        pOustersSummonItemInfo->setPrice(rows[r].head.price);
        pOustersSummonItemInfo->setVolumeType(rows[r].head.volume);
        pOustersSummonItemInfo->setWeight(rows[r].head.weight);
        pOustersSummonItemInfo->setMaxCharge(rows[r].maxCharge);
        pOustersSummonItemInfo->setEffectID(rows[r].effectID);

        addItemInfo(pOustersSummonItemInfo);
    }

    __END_CATCH
}

string OustersSummonItemInfo::toString() const

{
    __BEGIN_TRY

    StringStream msg;
    msg << "OustersSummonItemInfo(" << "ItemType:" << m_ItemType << ",Name:" << m_Name << ",EName:" << m_EName
        << ",Price:" << m_Price << ",VolumeType:" << Volume2String[m_VolumeType] << ",Weight:" << m_Weight
        << ",MaxCharge:" << m_MaxCharge << ",EffectID:" << m_EffectID << ")";
    return msg.toString();

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////////////
// class OustersSummonItemLoader member methods
//////////////////////////////////////////////////////////////////////////////

void OustersSummonItemLoader::load(Creature* pCreature)

{
    __BEGIN_TRY

    Assert(pCreature != NULL);

    vector<ChargeObjectRow> rows =
        defaultItemObjectRepository().loadChargeItemOfOwner(GEAR_OUSTERS_SUMMON_ITEM, pCreature->getName());

    for (size_t r = 0; r < rows.size(); r++) {
        try {
            ItemID_t itemID = rows[r].itemID;
            ObjectID_t objectID = rows[r].objectID;
            ItemType_t itemType = rows[r].itemType;
            Storage storage = (Storage)rows[r].storage;
            BYTE x = rows[r].x;
            BYTE y = rows[r].y;
            int charge = rows[r].charge;

            OustersSummonItem* pOustersSummonItem = new OustersSummonItem();
            pOustersSummonItem->setItemID(itemID);
            pOustersSummonItem->setObjectID(objectID);
            pOustersSummonItem->setItemType(itemType);
            pOustersSummonItem->setCharge(charge);

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

            if (storage == STORAGE_INVENTORY) {
                if (pInventory->canAddingEx(x, y, pOustersSummonItem)) {
                    pInventory->addItemEx(x, y, pOustersSummonItem);
                } else {
                    processItemBugEx(pCreature, pOustersSummonItem);
                }
            } else if (storage == STORAGE_GEAR) {
                processItemBugEx(pCreature, pOustersSummonItem);
            } else if (storage == STORAGE_BELT) {
                processItemBugEx(pCreature, pOustersSummonItem);
            } else if (storage == STORAGE_EXTRASLOT) {
                if (pCreature->isSlayer())
                    pSlayer->addItemToExtraInventorySlot(pOustersSummonItem);
                else if (pCreature->isVampire())
                    pVampire->addItemToExtraInventorySlot(pOustersSummonItem);
                else if (pCreature->isOusters())
                    pOusters->addItemToExtraInventorySlot(pOustersSummonItem);
            } else if (storage == STORAGE_MOTORCYCLE) {
                processItemBugEx(pCreature, pOustersSummonItem);
            } else if (storage == STORAGE_STASH) {
                if (pStash->isExist(x, y)) {
                    processItemBugEx(pCreature, pOustersSummonItem);
                } else
                    pStash->insert(x, y, pOustersSummonItem);
            } else if (storage == STORAGE_GARBAGE) {
                processItemBug(pCreature, pOustersSummonItem);
            } else {
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

void OustersSummonItemLoader::load(Zone* pZone)

{
    __BEGIN_TRY

    Assert(pZone != NULL);

    vector<ChargeObjectRow> rows = defaultItemObjectRepository().loadChargeItemInZone(
        GEAR_OUSTERS_SUMMON_ITEM, (int)STORAGE_ZONE, pZone->getZoneID());

    for (size_t r = 0; r < rows.size(); r++) {
        ItemID_t itemID = rows[r].itemID;
        ObjectID_t objectID = rows[r].objectID;
        ItemType_t itemType = rows[r].itemType;
        Storage storage = (Storage)rows[r].storage;
        BYTE x = rows[r].x;
        BYTE y = rows[r].y;
        int charge = rows[r].charge;

        OustersSummonItem* pOustersSummonItem = new OustersSummonItem();
        pOustersSummonItem->setItemID(itemID);
        pOustersSummonItem->setObjectID(objectID);
        pOustersSummonItem->setItemType(itemType);
        pOustersSummonItem->setCharge(charge);

        if (storage == STORAGE_ZONE) {
            Tile& pTile = pZone->getTile(x, y);
            Assert(!pTile.hasItem());
            pTile.addItem(pOustersSummonItem);
        } else {
            throw Error("Storage must be STORAGE_ZONE");
        }
    }

    __END_CATCH
}

void OustersSummonItemLoader::load(StorageID_t storageID, Inventory* pInventory)

{
    __BEGIN_TRY
    __END_CATCH
}
