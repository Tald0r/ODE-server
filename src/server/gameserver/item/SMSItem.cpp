//////////////////////////////////////////////////////////////////////////////
// Filename    : SMSItem.cpp
// Written By  : excel96
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "SMSItem.h"

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

ItemID_t SMSItem::m_ItemIDRegistry = 0;
Mutex SMSItem::m_Mutex;

//////////////////////////////////////////////////////////////////////////////
// class SMSItem member methods
//////////////////////////////////////////////////////////////////////////////

SMSItem::SMSItem()

{
    setItemType(0);
}

SMSItem::SMSItem(ItemType_t itemType, const list<OptionType_t>& optionType)

{
    setItemType(itemType);

    if (!de::gameContext().itemInfos().isPossibleItem(getItemClass(), getItemType(), getOptionTypeList())) {
        filelog("itembug.log", "SMSItem::SMSItem() : Invalid item type or option type");
        throw Error("SMSItem::SMSItem() : Invalid item type or optionType");
    }
}

void SMSItem::create(const string& ownerID, Storage storage, StorageID_t storageID, BYTE x, BYTE y, ItemID_t itemID)

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

    defaultItemObjectRepository().insertFlagItem(GEAR_SMSITEM, m_ItemID, m_ObjectID, getItemType(), ownerID,
                                                 (int)storage, storageID, (int)x, (int)y, (int)m_CreateType);

    __END_CATCH
}

//--------------------------------------------------------------------------------
// save item
//--------------------------------------------------------------------------------
void SMSItem::tinysave(const char* field) const

{
    __BEGIN_TRY

    defaultItemObjectRepository().tinysaveGear(GEAR_SMSITEM, field, m_ItemID);

    __END_CATCH
}

void SMSItem::save(const string& ownerID, Storage storage, StorageID_t storageID, BYTE x, BYTE y)

{
    __BEGIN_TRY

    defaultItemObjectRepository().updatePlainItem(GEAR_SMSITEM, m_ObjectID, getItemType(), ownerID, (int)storage,
                                                  storageID, (int)x, (int)y, m_ItemID);

    __END_CATCH
}

string SMSItem::toString() const

{
    StringStream msg;

    msg << "SMSItem(" << "ItemID:" << m_ItemID << ",ItemType:" << (int)getItemType() << ")";

    return msg.toString();
}

//////////////////////////////////////////////////////////////////////////////
// class SMSItemInfo member methods
//////////////////////////////////////////////////////////////////////////////

string SMSItemInfo::toString() const

{
    StringStream msg;
    msg << "SMSItemInfo(" << "ItemType:" << (int)m_ItemType << ",Name:" << m_Name << ",EName:" << m_EName
        << ",Price:" << (int)m_Price << ",VolumeType:" << Volume2String[m_VolumeType] << ",Weight:" << (int)m_Weight
        << ",Description:" << m_Description << ")";
    return msg.toString();
}

void SMSItemInfoManager::load()

{
    __BEGIN_TRY

    m_InfoCount = defaultItemObjectRepository().loadMaxGearType(GEAR_SMSITEM);

    m_pItemInfos = new ItemInfo*[m_InfoCount + 1];

    for (uint i = 0; i <= m_InfoCount; i++)
        m_pItemInfos[i] = NULL;

    vector<IntInfoRow> rows = defaultItemObjectRepository().loadIntInfos(GEAR_SMSITEM);

    for (size_t r = 0; r < rows.size(); r++) {
        SMSItemInfo* pSMSItemInfo = new SMSItemInfo();

        pSMSItemInfo->setItemType(rows[r].basic.itemType);
        pSMSItemInfo->setName(rows[r].basic.name);
        pSMSItemInfo->setEName(rows[r].basic.ename);
        pSMSItemInfo->setPrice(rows[r].basic.price);
        pSMSItemInfo->setVolumeType(rows[r].basic.volume);
        pSMSItemInfo->setWeight(rows[r].basic.weight);
        pSMSItemInfo->setRatio(rows[r].basic.ratio);
        pSMSItemInfo->setCharge(rows[r].value);

        addItemInfo(pSMSItemInfo);
    }

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
// class SMSItemLoader member methods
//////////////////////////////////////////////////////////////////////////////

void SMSItemLoader::load(Creature* pCreature)

{
    __BEGIN_TRY

    Assert(pCreature != NULL);

    vector<FlagObjectRow> rows = defaultItemObjectRepository().loadFlagItemOfOwner(GEAR_SMSITEM, pCreature->getName());

    for (size_t r = 0; r < rows.size(); r++) {
        try {
            SMSItem* pSMSItem = new SMSItem();

            pSMSItem->setItemID(rows[r].itemID);
            pSMSItem->setObjectID(rows[r].objectID);
            pSMSItem->setItemType(rows[r].itemType);

            Storage storage = (Storage)rows[r].storage;
            BYTE x = rows[r].x;
            BYTE y = rows[r].y;

            pSMSItem->setCreateType((Item::CreateType)rows[r].createType);

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
                if (pInventory->canAddingEx(x, y, pSMSItem)) {
                    pInventory->addItemEx(x, y, pSMSItem);
                } else {
                    processItemBugEx(pCreature, pSMSItem);
                }
                break;

            case STORAGE_GEAR:
                processItemBugEx(pCreature, pSMSItem);
                break;

            case STORAGE_BELT:
                processItemBugEx(pCreature, pSMSItem);
                break;

            case STORAGE_EXTRASLOT:
                if (pCreature->isSlayer())
                    pSlayer->addItemToExtraInventorySlot(pSMSItem);
                else if (pCreature->isVampire())
                    pVampire->addItemToExtraInventorySlot(pSMSItem);
                else if (pCreature->isOusters())
                    pOusters->addItemToExtraInventorySlot(pSMSItem);
                break;

            case STORAGE_MOTORCYCLE:
                processItemBugEx(pCreature, pSMSItem);
                break;

            case STORAGE_STASH:
                if (pStash->isExist(x, y)) {
                    processItemBugEx(pCreature, pSMSItem);
                } else
                    pStash->insert(x, y, pSMSItem);
                break;

            case STORAGE_GARBAGE:
                processItemBug(pCreature, pSMSItem);
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

void SMSItemLoader::load(Zone* pZone)

{
    __BEGIN_TRY

    Assert(pZone != NULL);

    vector<FlagZoneObjectRow> rows =
        defaultItemObjectRepository().loadFlagItemInZone(GEAR_SMSITEM, (int)STORAGE_ZONE, pZone->getZoneID());

    for (size_t r = 0; r < rows.size(); r++) {
        SMSItem* pSMSItem = new SMSItem();

        pSMSItem->setItemID(rows[r].itemID);
        pSMSItem->setObjectID(rows[r].objectID);
        pSMSItem->setItemType(rows[r].itemType);

        Storage storage = (Storage)rows[r].storage;
        BYTE x = rows[r].x;
        BYTE y = rows[r].y;

        pSMSItem->setCreateType((Item::CreateType)rows[r].createType);

        switch (storage) {
        case STORAGE_ZONE: {
            Tile& pTile = pZone->getTile(x, y);
            Assert(!pTile.hasItem());
            pTile.addItem(pSMSItem);
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

void SMSItemLoader::load(StorageID_t storageID, Inventory* pInventory)

{
    __BEGIN_TRY

    Statement* pStmt;

    BEGIN_DB {}
    END_DB(pStmt)

    __END_CATCH
}
