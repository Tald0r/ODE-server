//////////////////////////////////////////////////////////////////////////////
// Filename    : EventETC.cpp
// Written By  : excel96
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "EventETC.h"

#include "Belt.h"
#include "DB.h"
#include "GameContext.h"
#include "ItemInfoManager.h"
#include "ItemUtil.h"
#include "Motorcycle.h"
#include "Ousters.h"
#include "OustersArmsband.h"
#include "Slayer.h"
#include "Stash.h"
#include "Utility.h"
#include "Vampire.h"
#include "repository/ItemObjectRepository.h"

ItemID_t EventETC::m_ItemIDRegistry = 0;
Mutex EventETC::m_Mutex;

//////////////////////////////////////////////////////////////////////////////
// class EventETC member methods
//////////////////////////////////////////////////////////////////////////////

EventETC::EventETC()

{
    m_ItemType = 0;
}

EventETC::EventETC(ItemType_t itemType, const list<OptionType_t>& optionType, ItemNum_t Num)

{
    m_ItemType = itemType;
    m_Num = Num;

    if (!de::gameContext().itemInfos().isPossibleItem(getItemClass(), m_ItemType, optionType)) {
        filelog("itembug.log", "EventETC::EventETC() : Invalid item type or option type");
        throw Error("EventETC::EventETC() : Invalid item type or optionType");
    }
}

void EventETC::create(const string& ownerID, Storage storage, StorageID_t storageID, BYTE x, BYTE y, ItemID_t itemID)

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

    defaultItemObjectRepository().insertNumItem(GEAR_EVENT_ETC, m_ItemID, m_ObjectID, m_ItemType, ownerID, (int)storage,
                                                storageID, (int)x, (int)y, (int)m_Num, (int)m_CreateType);

    __END_CATCH
}

//--------------------------------------------------------------------------------
// save item
//--------------------------------------------------------------------------------
void EventETC::tinysave(const char* field) const

{
    __BEGIN_TRY

    defaultItemObjectRepository().tinysaveGear(GEAR_EVENT_ETC, field, m_ItemID);

    __END_CATCH
}

void EventETC::save(const string& ownerID, Storage storage, StorageID_t storageID, BYTE x, BYTE y)

{
    __BEGIN_TRY

    defaultItemObjectRepository().updateNumItem(GEAR_EVENT_ETC, m_ObjectID, m_ItemType, ownerID, (int)storage,
                                                storageID, (int)x, (int)y, (int)m_Num, m_ItemID);

    __END_CATCH
}

string EventETC::toString() const

{
    StringStream msg;

    msg << "EventETC(" << "ItemID:" << m_ItemID << ",ItemType:" << (int)m_ItemType << ",Num:" << (int)m_Num << ")";

    return msg.toString();
}

VolumeWidth_t EventETC::getVolumeWidth() const

{
    __BEGIN_TRY

    return de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_EVENT_ETC, m_ItemType)->getVolumeWidth();

    __END_CATCH
}

VolumeHeight_t EventETC::getVolumeHeight() const

{
    __BEGIN_TRY

    return de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_EVENT_ETC, m_ItemType)->getVolumeHeight();

    __END_CATCH
}

Weight_t EventETC::getWeight() const

{
    __BEGIN_TRY

    return de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_EVENT_ETC, m_ItemType)->getWeight();

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
// class EventETCInfo member methods
//////////////////////////////////////////////////////////////////////////////

string EventETCInfo::toString() const

{
    StringStream msg;
    msg << "EventETCInfo(" << "ItemType:" << (int)m_ItemType << ",Name:" << m_Name << ",EName:" << m_EName
        << ",Price:" << (int)m_Price << ",VolumeType:" << Volume2String[m_VolumeType] << ",Weight:" << (int)m_Weight
        << ",Description:" << m_Description << ")";
    return msg.toString();
}

void EventETCInfoManager::load()

{
    __BEGIN_TRY

    m_InfoCount = defaultItemObjectRepository().loadMaxGearType(GEAR_EVENT_ETC);

    m_pItemInfos = new ItemInfo*[m_InfoCount + 1];

    for (uint i = 0; i <= m_InfoCount; i++)
        m_pItemInfos[i] = NULL;

    vector<FunctionInfoRow> rows = defaultItemObjectRepository().loadFunctionInfos(GEAR_EVENT_ETC);

    for (size_t r = 0; r < rows.size(); r++) {
        EventETCInfo* pEventETCInfo = new EventETCInfo();

        pEventETCInfo->setItemType(rows[r].basic.itemType);
        pEventETCInfo->setName(rows[r].basic.name);
        pEventETCInfo->setEName(rows[r].basic.ename);
        pEventETCInfo->setPrice(rows[r].basic.price);
        pEventETCInfo->setVolumeType(rows[r].basic.volume);
        pEventETCInfo->setWeight(rows[r].basic.weight);
        pEventETCInfo->setRatio(rows[r].basic.ratio);
        pEventETCInfo->setFunction(rows[r].function);

        addItemInfo(pEventETCInfo);
    }

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
// class EventETCLoader member methods
//////////////////////////////////////////////////////////////////////////////

void EventETCLoader::load(Creature* pCreature)

{
    __BEGIN_TRY

    Assert(pCreature != NULL);

    vector<NumObjectRow> rows = defaultItemObjectRepository().loadNumItemOfOwner(GEAR_EVENT_ETC, pCreature->getName());

    for (size_t r = 0; r < rows.size(); r++) {
        try {
            EventETC* pEventETC = new EventETC();

            pEventETC->setItemID(rows[r].itemID);
            pEventETC->setObjectID(rows[r].objectID);
            pEventETC->setItemType(rows[r].itemType);

            Storage storage = (Storage)rows[r].storage;
            StorageID_t storageID = rows[r].storageID;
            BYTE x = rows[r].x;
            BYTE y = rows[r].y;

            pEventETC->setNum(rows[r].num);
            pEventETC->setCreateType((Item::CreateType)rows[r].createType);

            Inventory* pInventory = NULL;
            Slayer* pSlayer = NULL;
            Vampire* pVampire = NULL;
            Ousters* pOusters = NULL;
            Item* pItem = NULL;
            Stash* pStash = NULL;
            Belt* pBelt = NULL;
            OustersArmsband* pOustersArmsband = NULL;
            Inventory* pBeltInventory = NULL;
            Inventory* pArmsbandInventory = NULL;

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
                if (pInventory->canAddingEx(x, y, pEventETC)) {
                    pInventory->addItemEx(x, y, pEventETC);
                } else {
                    processItemBugEx(pCreature, pEventETC);
                }
                break;

            case STORAGE_GEAR:
                processItemBugEx(pCreature, pEventETC);
                break;

            case STORAGE_BELT:
                if (pCreature->isSlayer()) {
                    pItem = pSlayer->findBeltIID(storageID);
                    if (pItem != NULL && pItem->getItemClass() == Item::ITEM_CLASS_BELT) {
                        pBelt = dynamic_cast<Belt*>(pItem);
                        pBeltInventory = pBelt->getInventory();
                        if (pBeltInventory->canAddingEx(x, 0, pEventETC)) {
                            pBeltInventory->addItem(x, 0, pEventETC);
                        } else {
                            processItemBugEx(pCreature, pEventETC);
                        }
                    } else {
                        processItemBugEx(pCreature, pEventETC);
                    }
                } else if (pCreature->isVampire()) {
                    pItem = pVampire->findBeltIID(storageID);
                    if (pItem != NULL && pItem->getItemClass() == Item::ITEM_CLASS_BELT) {
                        pBelt = dynamic_cast<Belt*>(pItem);
                        pBeltInventory = pBelt->getInventory();
                        if (pBeltInventory->canAddingEx(x, 0, pEventETC)) {
                            pBeltInventory->addItemEx(x, 0, pEventETC);
                        } else {
                            processItemBugEx(pCreature, pEventETC);
                        }
                    } else {
                        processItemBugEx(pCreature, pEventETC);
                    }
                } else if (pCreature->isOusters()) {
                    pItem = findItemIID(pOusters, storageID, Item::ITEM_CLASS_OUSTERS_ARMSBAND);
                    if (pItem != NULL && pItem->getItemClass() == Item::ITEM_CLASS_OUSTERS_ARMSBAND) {
                        pOustersArmsband = dynamic_cast<OustersArmsband*>(pItem);
                        pArmsbandInventory = pOustersArmsband->getInventory();
                        if (pArmsbandInventory->canAddingEx(x, 0, pEventETC)) {
                            pArmsbandInventory->addItemEx(x, 0, pEventETC);
                        } else {
                            processItemBugEx(pCreature, pEventETC);
                        }
                    } else {
                        processItemBugEx(pCreature, pEventETC);
                    }
                } else {
                    processItemBugEx(pCreature, pEventETC);
                }
                break;

            case STORAGE_EXTRASLOT:
                if (pCreature->isSlayer())
                    pSlayer->addItemToExtraInventorySlot(pEventETC);
                else if (pCreature->isVampire())
                    pVampire->addItemToExtraInventorySlot(pEventETC);
                else if (pCreature->isOusters())
                    pOusters->addItemToExtraInventorySlot(pEventETC);
                break;

            case STORAGE_MOTORCYCLE:
                processItemBugEx(pCreature, pEventETC);
                break;

            case STORAGE_STASH:
                if (pStash->isExist(x, y)) {
                    processItemBugEx(pCreature, pEventETC);
                } else
                    pStash->insert(x, y, pEventETC);
                break;

            case STORAGE_GARBAGE:
                processItemBug(pCreature, pEventETC);
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

void EventETCLoader::load(Zone* pZone)

{
    __BEGIN_TRY

    Assert(pZone != NULL);

    vector<NumZoneObjectRow> rows =
        defaultItemObjectRepository().loadNumItemInZone(GEAR_EVENT_ETC, (int)STORAGE_ZONE, pZone->getZoneID());

    for (size_t r = 0; r < rows.size(); r++) {
        EventETC* pEventETC = new EventETC();

        pEventETC->setItemID(rows[r].itemID);
        pEventETC->setObjectID(rows[r].objectID);
        pEventETC->setItemType(rows[r].itemType);

        Storage storage = (Storage)rows[r].storage;
        BYTE x = rows[r].x;
        BYTE y = rows[r].y;

        pEventETC->setNum(rows[r].num);
        pEventETC->setCreateType((Item::CreateType)rows[r].createType);

        switch (storage) {
        case STORAGE_ZONE: {
            Tile& pTile = pZone->getTile(x, y);
            Assert(!pTile.hasItem());
            pTile.addItem(pEventETC);
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

void EventETCLoader::load(StorageID_t storageID, Inventory* pInventory)

{
    __BEGIN_TRY

    Statement* pStmt;

    BEGIN_DB {}
    END_DB(pStmt)

    __END_CATCH
}
