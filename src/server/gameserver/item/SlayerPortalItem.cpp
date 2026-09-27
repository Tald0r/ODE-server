//////////////////////////////////////////////////////////////////////////////
// Filename    : SlayerPortalItem.cpp
// Written By  : excel96
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "SlayerPortalItem.h"

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

ItemID_t SlayerPortalItem::m_ItemIDRegistry = 0;
Mutex SlayerPortalItem::m_Mutex;

//////////////////////////////////////////////////////////////////////////////
// class SlayerPortalItem member methods
//////////////////////////////////////////////////////////////////////////////

SlayerPortalItem::SlayerPortalItem()

{
    m_ItemType = 0;
    m_Charge = 0;
}

SlayerPortalItem::SlayerPortalItem(ItemType_t itemType, const list<OptionType_t>& optionType)

{
    m_ItemType = itemType;
    m_Charge = getMaxCharge();

    if (!de::gameContext().itemInfos().isPossibleItem(getItemClass(), m_ItemType, optionType)) {
        filelog("itembug.log", "SlayerPortalItem::SlayerPortalItem() : Invalid item type or option type");
        throw Error("SlayerPortalItem::SlayerPortalItem() : Invalid item type or optionType");
    }
}

void SlayerPortalItem::create(const string& ownerID, Storage storage, StorageID_t storageID, BYTE x, BYTE y,
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

    defaultItemObjectRepository().insertChargeItem(GEAR_SLAYER_PORTAL_ITEM, m_ItemID, m_ObjectID, m_ItemType, ownerID,
                                                   (int)storage, storageID, (int)x, (int)y, m_Charge);

    __END_CATCH
}

//--------------------------------------------------------------------------------
// save item
//--------------------------------------------------------------------------------
void SlayerPortalItem::tinysave(const char* field) const

{
    __BEGIN_TRY

    defaultItemObjectRepository().tinysaveGear(GEAR_SLAYER_PORTAL_ITEM, field, m_ItemID);

    __END_CATCH
}

void SlayerPortalItem::save(const string& ownerID, Storage storage, StorageID_t storageID, BYTE x, BYTE y)

{
    __BEGIN_TRY

    defaultItemObjectRepository().updateChargeItem(GEAR_SLAYER_PORTAL_ITEM, m_ObjectID, m_ItemType, ownerID,
                                                   (int)storage, storageID, (int)x, (int)y, m_Charge, m_ItemID);

    __END_CATCH
}

VolumeWidth_t SlayerPortalItem::getVolumeWidth() const

{
    __BEGIN_TRY

    return de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_SLAYER_PORTAL_ITEM, m_ItemType)->getVolumeWidth();

    __END_CATCH
}

VolumeHeight_t SlayerPortalItem::getVolumeHeight() const

{
    __BEGIN_TRY

    return de::gameContext()
        .itemInfos()
        .getItemInfo(Item::ITEM_CLASS_SLAYER_PORTAL_ITEM, m_ItemType)
        ->getVolumeHeight();

    __END_CATCH
}

Weight_t SlayerPortalItem::getWeight() const

{
    __BEGIN_TRY

    return de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_SLAYER_PORTAL_ITEM, m_ItemType)->getWeight();

    __END_CATCH
}

string SlayerPortalItem::toString() const

{
    __BEGIN_TRY

    StringStream msg;
    msg << "SlayerPortalItem(" << "ItemID:" << m_ItemID << ",ItemType:" << (int)m_ItemType << ",Charge:" << m_Charge
        << ")";

    return msg.toString();

    __END_CATCH
}

int SlayerPortalItem::getMaxCharge(void) const

{
    __BEGIN_TRY

    SlayerPortalItemInfo* pInfo = dynamic_cast<SlayerPortalItemInfo*>(
        de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_SLAYER_PORTAL_ITEM, m_ItemType));
    Assert(pInfo != NULL);
    return pInfo->getMaxCharge();

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
// class SlayerPortalItemInfo member methods
//////////////////////////////////////////////////////////////////////////////

void SlayerPortalItemInfoManager::load()

{
    __BEGIN_TRY

    m_InfoCount = defaultItemObjectRepository().loadMaxGearType(GEAR_SLAYER_PORTAL_ITEM);

    m_pItemInfos = new ItemInfo*[m_InfoCount + 1];

    for (uint i = 0; i <= m_InfoCount; i++)
        m_pItemInfos[i] = NULL;

    vector<LevelStringInfoRow> rows = defaultItemObjectRepository().loadLevelStringInfos(GEAR_SLAYER_PORTAL_ITEM);

    for (size_t r = 0; r < rows.size(); r++) {
        SlayerPortalItemInfo* pSlayerPortalItemInfo = new SlayerPortalItemInfo();

        pSlayerPortalItemInfo->setItemType(rows[r].basic.itemType);
        pSlayerPortalItemInfo->setName(rows[r].basic.name);
        pSlayerPortalItemInfo->setEName(rows[r].basic.ename);
        pSlayerPortalItemInfo->setPrice(rows[r].basic.price);
        pSlayerPortalItemInfo->setVolumeType(rows[r].basic.volume);
        pSlayerPortalItemInfo->setWeight(rows[r].basic.weight);
        pSlayerPortalItemInfo->setRatio(rows[r].basic.ratio);
        pSlayerPortalItemInfo->setMaxCharge(rows[r].itemLevel);
        pSlayerPortalItemInfo->setReqAbility(rows[r].value);

        addItemInfo(pSlayerPortalItemInfo);
    }

    __END_CATCH
}

string SlayerPortalItemInfo::toString() const

{
    __BEGIN_TRY

    StringStream msg;
    msg << "SlayerPortalItemInfo(" << "ItemType:" << m_ItemType << ",Name:" << m_Name << ",EName:" << m_EName
        << ",Price:" << m_Price << ",VolumeType:" << Volume2String[m_VolumeType] << ",Weight:" << m_Weight
        << ",MaxCharge:" << m_MaxCharge << ")";
    return msg.toString();

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////////////
// class SlayerPortalItemLoader member methods
//////////////////////////////////////////////////////////////////////////////

void SlayerPortalItemLoader::load(Creature* pCreature)

{
    __BEGIN_TRY

    Assert(pCreature != NULL);

    vector<ChargeObjectRow> rows =
        defaultItemObjectRepository().loadChargeItemOfOwner(GEAR_SLAYER_PORTAL_ITEM, pCreature->getName());

    for (size_t r = 0; r < rows.size(); r++) {
        try {
            ItemID_t itemID = rows[r].itemID;
            ObjectID_t objectID = rows[r].objectID;
            ItemType_t itemType = rows[r].itemType;
            Storage storage = (Storage)rows[r].storage;
            BYTE x = rows[r].x;
            BYTE y = rows[r].y;
            int charge = rows[r].charge;

            SlayerPortalItem* pSlayerPortalItem = new SlayerPortalItem();
            pSlayerPortalItem->setItemID(itemID);
            pSlayerPortalItem->setObjectID(objectID);
            pSlayerPortalItem->setItemType(itemType);
            pSlayerPortalItem->setCharge(charge);

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

            if (storage == STORAGE_INVENTORY) {
                if (pInventory->canAddingEx(x, y, pSlayerPortalItem)) {
                    pInventory->addItemEx(x, y, pSlayerPortalItem);
                } else {
                    processItemBugEx(pCreature, pSlayerPortalItem);
                }
            } else if (storage == STORAGE_GEAR) {
                processItemBugEx(pCreature, pSlayerPortalItem);
            } else if (storage == STORAGE_BELT) {
                processItemBugEx(pCreature, pSlayerPortalItem);
            } else if (storage == STORAGE_EXTRASLOT) {
                if (pCreature->isSlayer())
                    pSlayer->addItemToExtraInventorySlot(pSlayerPortalItem);
                else if (pCreature->isVampire())
                    pVampire->addItemToExtraInventorySlot(pSlayerPortalItem);
            } else if (storage == STORAGE_MOTORCYCLE) {
                processItemBugEx(pCreature, pSlayerPortalItem);
            } else if (storage == STORAGE_STASH) {
                if (pStash->isExist(x, y)) {
                    processItemBugEx(pCreature, pSlayerPortalItem);
                } else
                    pStash->insert(x, y, pSlayerPortalItem);
            } else if (storage == STORAGE_GARBAGE) {
                processItemBug(pCreature, pSlayerPortalItem);
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

void SlayerPortalItemLoader::load(Zone* pZone)

{
    __BEGIN_TRY

    Assert(pZone != NULL);

    vector<ChargeObjectRow> rows = defaultItemObjectRepository().loadChargeItemInZone(
        GEAR_SLAYER_PORTAL_ITEM, (int)STORAGE_ZONE, pZone->getZoneID());

    for (size_t r = 0; r < rows.size(); r++) {
        ItemID_t itemID = rows[r].itemID;
        ObjectID_t objectID = rows[r].objectID;
        ItemType_t itemType = rows[r].itemType;
        Storage storage = (Storage)rows[r].storage;
        BYTE x = rows[r].x;
        BYTE y = rows[r].y;
        int charge = rows[r].charge;

        SlayerPortalItem* pSlayerPortalItem = new SlayerPortalItem();
        pSlayerPortalItem->setItemID(itemID);
        pSlayerPortalItem->setObjectID(objectID);
        pSlayerPortalItem->setItemType(itemType);
        pSlayerPortalItem->setCharge(charge);

        if (storage == STORAGE_ZONE) {
            Tile& pTile = pZone->getTile(x, y);
            Assert(!pTile.hasItem());
            pTile.addItem(pSlayerPortalItem);
        } else {
            throw Error("Storage must be STORAGE_ZONE");
        }
    }

    __END_CATCH
}

void SlayerPortalItemLoader::load(StorageID_t storageID, Inventory* pInventory)

{
    __BEGIN_TRY
    __END_CATCH
}
