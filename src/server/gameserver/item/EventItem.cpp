//////////////////////////////////////////////////////////////////////////////
// Filename    : EventItem.cpp
// Written By  : excel96
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "EventItem.h"

#include "Belt.h"
#include "DB.h"
#include "GameContext.h"
#include "ItemInfoManager.h"
#include "ItemUtil.h"
#include "Motorcycle.h"
#include "Ousters.h"
#include "Party.h"
#include "Slayer.h"
#include "Stash.h"
#include "Utility.h"
#include "Vampire.h"
#include "repository/ItemObjectRepository.h"

ItemID_t EventItem::m_ItemIDRegistry = 0;
Mutex EventItem::m_Mutex;

//////////////////////////////////////////////////////////////////////////////
// class EventItem member methods
//////////////////////////////////////////////////////////////////////////////

EventItem::EventItem()

{
    m_ItemType = 0;
}

EventItem::EventItem(ItemType_t itemType, const list<OptionType_t>& optionType, ItemNum_t Num)

{
    m_ItemType = itemType;
    m_Num = Num;

    if (!de::gameContext().itemInfos().isPossibleItem(getItemClass(), m_ItemType, optionType)) {
        filelog("itembug.log", "EventItem::EventItem() : Invalid item type or option type");
        throw Error("EventItem::EventItem() : Invalid item type or optionType");
    }
}

void EventItem::create(const string& ownerID, Storage storage, StorageID_t storageID, BYTE x, BYTE y, ItemID_t itemID)

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

    defaultItemObjectRepository().insertNumItem(GEAR_EVENT_ITEM, m_ItemID, m_ObjectID, m_ItemType, ownerID,
                                                (int)storage, storageID, (int)x, (int)y, (int)m_Num, (int)m_CreateType);

    __END_CATCH
}

//--------------------------------------------------------------------------------
// save item
//--------------------------------------------------------------------------------
void EventItem::tinysave(const char* field) const

{
    __BEGIN_TRY

    defaultItemObjectRepository().tinysaveGear(GEAR_EVENT_ITEM, field, m_ItemID);

    __END_CATCH
}

void EventItem::save(const string& ownerID, Storage storage, StorageID_t storageID, BYTE x, BYTE y)

{
    __BEGIN_TRY

    defaultItemObjectRepository().updateNumItem(GEAR_EVENT_ITEM, m_ObjectID, m_ItemType, ownerID, (int)storage,
                                                storageID, (int)x, (int)y, (int)m_Num, m_ItemID);

    __END_CATCH
}

string EventItem::toString() const

{
    StringStream msg;

    msg << "EventItem(" << "ItemID:" << m_ItemID << ",ItemType:" << (int)m_ItemType << ",Num:" << (int)m_Num << ")";

    return msg.toString();
}

VolumeWidth_t EventItem::getVolumeWidth() const

{
    __BEGIN_TRY

    return de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_EVENT_ITEM, m_ItemType)->getVolumeWidth();

    __END_CATCH
}

VolumeHeight_t EventItem::getVolumeHeight() const

{
    __BEGIN_TRY

    return de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_EVENT_ITEM, m_ItemType)->getVolumeHeight();

    __END_CATCH
}

Weight_t EventItem::getWeight() const

{
    __BEGIN_TRY

    return de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_EVENT_ITEM, m_ItemType)->getWeight();

    __END_CATCH
}

void EventItem::whenPCTake(PlayerCreature* pPC) {}

//////////////////////////////////////////////////////////////////////////////
// class EventItemInfo member methods
//////////////////////////////////////////////////////////////////////////////

string EventItemInfo::toString() const

{
    StringStream msg;
    msg << "EventItemInfo(" << "ItemType:" << (int)m_ItemType << ",Name:" << m_Name << ",EName:" << m_EName
        << ",Price:" << (int)m_Price << ",VolumeType:" << Volume2String[m_VolumeType] << ",Weight:" << (int)m_Weight
        << ",Description:" << m_Description << ")";
    return msg.toString();
}

void EventItemInfoManager::load()

{
    __BEGIN_TRY

    m_InfoCount = defaultItemObjectRepository().loadMaxGearType(GEAR_EVENT_ITEM);

    m_pItemInfos = new ItemInfo*[m_InfoCount + 1];

    for (uint i = 0; i <= m_InfoCount; i++)
        m_pItemInfos[i] = NULL;

    vector<BasicInfoRow> rows = defaultItemObjectRepository().loadBasicInfos(GEAR_EVENT_ITEM);

    for (size_t r = 0; r < rows.size(); r++) {
        EventItemInfo* pEventItemInfo = new EventItemInfo();

        pEventItemInfo->setItemType(rows[r].itemType);
        pEventItemInfo->setName(rows[r].name);
        pEventItemInfo->setEName(rows[r].ename);
        pEventItemInfo->setPrice(rows[r].price);
        pEventItemInfo->setVolumeType(rows[r].volume);
        pEventItemInfo->setWeight(rows[r].weight);
        pEventItemInfo->setRatio(rows[r].ratio);

        addItemInfo(pEventItemInfo);
    }

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
// class EventItemLoader member methods
//////////////////////////////////////////////////////////////////////////////

void EventItemLoader::load(Creature* pCreature)

{
    __BEGIN_TRY

    Assert(pCreature != NULL);

    vector<NumObjectRow> rows = defaultItemObjectRepository().loadNumItemOfOwner(GEAR_EVENT_ITEM, pCreature->getName());

    for (size_t r = 0; r < rows.size(); r++) {
        try {
            EventItem* pEventItem = new EventItem();
            pEventItem->setQuestItem();

            pEventItem->setItemID(rows[r].itemID);
            pEventItem->setObjectID(rows[r].objectID);
            pEventItem->setItemType(rows[r].itemType);

            Storage storage = (Storage)rows[r].storage;
            BYTE x = rows[r].x;
            BYTE y = rows[r].y;

            pEventItem->setNum(rows[r].num);
            pEventItem->setCreateType((Item::CreateType)rows[r].createType);

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

            PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pCreature);

            if (pEventItem->getItemType() == 27) {
                // A flag must not turn up here.
                processItemBug(pCreature, pEventItem);
            } else
                switch (storage) {
                case STORAGE_INVENTORY:
                    if (pInventory->canAddingEx(x, y, pEventItem)) {
                        pInventory->addItemEx(x, y, pEventItem);
                        if (pEventItem->getItemType() == 30)
                            pPC->setBaseLuck(10);
                    } else {
                        processItemBugEx(pCreature, pEventItem);
                    }
                    break;

                case STORAGE_GEAR:
                    processItemBugEx(pCreature, pEventItem);
                    break;

                case STORAGE_BELT:
                    processItemBugEx(pCreature, pEventItem);
                    break;

                case STORAGE_EXTRASLOT:
                    if (pCreature->isSlayer())
                        pSlayer->addItemToExtraInventorySlot(pEventItem);
                    else if (pCreature->isVampire())
                        pVampire->addItemToExtraInventorySlot(pEventItem);
                    else if (pCreature->isOusters())
                        pOusters->addItemToExtraInventorySlot(pEventItem);
                    if (pEventItem->getItemType() == 30)
                        pPC->setBaseLuck(10);
                    break;

                case STORAGE_MOTORCYCLE:
                    processItemBugEx(pCreature, pEventItem);
                    break;

                case STORAGE_STASH:
                    if (pStash->isExist(x, y)) {
                        processItemBugEx(pCreature, pEventItem);
                    } else {
                        pStash->insert(x, y, pEventItem);
                        if (pEventItem->getItemType() == 30)
                            pPC->setBaseLuck(10);
                    }
                    break;

                case STORAGE_GARBAGE:
                    processItemBug(pCreature, pEventItem);
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

void EventItemLoader::load(Zone* pZone)

{
    __BEGIN_TRY

    Assert(pZone != NULL);

    vector<NumZoneObjectRow> rows =
        defaultItemObjectRepository().loadNumItemInZone(GEAR_EVENT_ITEM, (int)STORAGE_ZONE, pZone->getZoneID());

    for (size_t r = 0; r < rows.size(); r++) {
        EventItem* pEventItem = new EventItem();

        pEventItem->setItemID(rows[r].itemID);
        pEventItem->setObjectID(rows[r].objectID);
        pEventItem->setItemType(rows[r].itemType);

        Storage storage = (Storage)rows[r].storage;
        BYTE x = rows[r].x;
        BYTE y = rows[r].y;

        pEventItem->setNum(rows[r].num);
        pEventItem->setCreateType((Item::CreateType)rows[r].createType);

        switch (storage) {
        case STORAGE_ZONE: {
            Tile& pTile = pZone->getTile(x, y);
            Assert(!pTile.hasItem());
            pTile.addItem(pEventItem);
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

void EventItemLoader::load(StorageID_t storageID, Inventory* pInventory)

{
    __BEGIN_TRY

    Statement* pStmt;

    BEGIN_DB {}
    END_DB(pStmt)

    __END_CATCH
}
