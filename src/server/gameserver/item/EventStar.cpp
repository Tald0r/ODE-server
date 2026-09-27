//////////////////////////////////////////////////////////////////////////////
// Filename    : EventStar.cpp
// Written By  : excel96
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "EventStar.h"

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

ItemID_t EventStar::m_ItemIDRegistry = 0;
Mutex EventStar::m_Mutex;

//////////////////////////////////////////////////////////////////////////////
// class EventStar member methods
//////////////////////////////////////////////////////////////////////////////

EventStar::EventStar()

{
    m_ItemType = 0;
}

EventStar::EventStar(ItemType_t itemType, const list<OptionType_t>& optionType, ItemNum_t Num)

{
    m_ItemType = itemType;
    m_Num = Num;

    if (!de::gameContext().itemInfos().isPossibleItem(getItemClass(), m_ItemType, optionType)) {
        filelog("itembug.log", "EventStar::EventStar() : Invalid item type or option type");
        throw Error("EventStar::EventStar() : Invalid item type or optionType");
    }
}

void EventStar::create(const string& ownerID, Storage storage, StorageID_t storageID, BYTE x, BYTE y, ItemID_t itemID)

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

    defaultItemObjectRepository().insertNumItem(GEAR_EVENT_STAR, m_ItemID, m_ObjectID, m_ItemType, ownerID,
                                                (int)storage, storageID, (int)x, (int)y, (int)m_Num, (int)m_CreateType);

    __END_CATCH
}

//--------------------------------------------------------------------------------
// save item
//--------------------------------------------------------------------------------
void EventStar::tinysave(const char* field) const

{
    __BEGIN_TRY

    defaultItemObjectRepository().tinysaveGear(GEAR_EVENT_STAR, field, m_ItemID);

    __END_CATCH
}

void EventStar::save(const string& ownerID, Storage storage, StorageID_t storageID, BYTE x, BYTE y)

{
    __BEGIN_TRY

    defaultItemObjectRepository().updateNumItem(GEAR_EVENT_STAR, m_ObjectID, m_ItemType, ownerID, (int)storage,
                                                storageID, (int)x, (int)y, (int)m_Num, m_ItemID);

    __END_CATCH
}

string EventStar::toString() const

{
    StringStream msg;

    msg << "EventStar(" << "ItemID:" << m_ItemID << ",ItemType:" << (int)m_ItemType << ",Num:" << (int)m_Num << ")";

    return msg.toString();
}

VolumeWidth_t EventStar::getVolumeWidth() const

{
    __BEGIN_TRY

    return de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_EVENT_STAR, m_ItemType)->getVolumeWidth();

    __END_CATCH
}

VolumeHeight_t EventStar::getVolumeHeight() const

{
    __BEGIN_TRY

    return de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_EVENT_STAR, m_ItemType)->getVolumeHeight();

    __END_CATCH
}

Weight_t EventStar::getWeight() const

{
    __BEGIN_TRY

    return de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_EVENT_STAR, m_ItemType)->getWeight();

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
// class EventStarInfo member methods
//////////////////////////////////////////////////////////////////////////////

string EventStarInfo::toString() const

{
    StringStream msg;
    msg << "EventStarInfo(" << "ItemType:" << (int)m_ItemType << ",Name:" << m_Name << ",EName:" << m_EName
        << ",Price:" << (int)m_Price << ",VolumeType:" << Volume2String[m_VolumeType] << ",Weight:" << (int)m_Weight
        << ",Function:" << (int)m_fFunction << ",FunctionValue:" << (int)m_FunctionValue
        << ",Description:" << m_Description << ")";
    return msg.toString();
}

void EventStarInfoManager::load()

{
    __BEGIN_TRY

    m_InfoCount = defaultItemObjectRepository().loadMaxGearType(GEAR_EVENT_STAR);

    m_pItemInfos = new ItemInfo*[m_InfoCount + 1];

    for (uint i = 0; i <= m_InfoCount; i++)
        m_pItemInfos[i] = NULL;

    vector<FunctionValueInfoRow> rows = defaultItemObjectRepository().loadFunctionValueInfos(GEAR_EVENT_STAR);

    for (size_t r = 0; r < rows.size(); r++) {
        EventStarInfo* pEventStarInfo = new EventStarInfo();

        pEventStarInfo->setItemType(rows[r].basic.itemType);
        pEventStarInfo->setName(rows[r].basic.name);
        pEventStarInfo->setEName(rows[r].basic.ename);
        pEventStarInfo->setPrice(rows[r].basic.price);
        pEventStarInfo->setVolumeType(rows[r].basic.volume);
        pEventStarInfo->setWeight(rows[r].basic.weight);
        pEventStarInfo->setRatio(rows[r].basic.ratio);
        pEventStarInfo->setFunctionFlag(rows[r].functionFlag);
        pEventStarInfo->setFunctionValue(rows[r].functionValue);

        addItemInfo(pEventStarInfo);
    }

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
// class EventStarLoader member methods
//////////////////////////////////////////////////////////////////////////////

void EventStarLoader::load(Creature* pCreature)

{
    __BEGIN_TRY

    Assert(pCreature != NULL);

    vector<NumObjectRow> rows = defaultItemObjectRepository().loadNumItemOfOwner(GEAR_EVENT_STAR, pCreature->getName());

    for (size_t r = 0; r < rows.size(); r++) {
        try {
            EventStar* pEventStar = new EventStar();

            pEventStar->setItemID(rows[r].itemID);
            pEventStar->setObjectID(rows[r].objectID);
            pEventStar->setItemType(rows[r].itemType);

            Storage storage = (Storage)rows[r].storage;
            BYTE x = rows[r].x;
            BYTE y = rows[r].y;

            pEventStar->setNum(rows[r].num);
            pEventStar->setCreateType((Item::CreateType)rows[r].createType);

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
                if (pInventory->canAddingEx(x, y, pEventStar)) {
                    pInventory->addItemEx(x, y, pEventStar);
                } else {
                    processItemBugEx(pCreature, pEventStar);
                }
                break;

            case STORAGE_GEAR:
                processItemBugEx(pCreature, pEventStar);
                break;

            case STORAGE_BELT:
                processItemBugEx(pCreature, pEventStar);
                break;

            case STORAGE_EXTRASLOT:
                if (pCreature->isSlayer())
                    pSlayer->addItemToExtraInventorySlot(pEventStar);
                else if (pCreature->isVampire())
                    pVampire->addItemToExtraInventorySlot(pEventStar);
                else if (pCreature->isOusters())
                    pOusters->addItemToExtraInventorySlot(pEventStar);
                break;

            case STORAGE_MOTORCYCLE:
                processItemBugEx(pCreature, pEventStar);
                break;

            case STORAGE_STASH:
                if (pStash->isExist(x, y)) {
                    processItemBugEx(pCreature, pEventStar);
                } else
                    pStash->insert(x, y, pEventStar);
                break;

            case STORAGE_GARBAGE:
                processItemBug(pCreature, pEventStar);
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

void EventStarLoader::load(Zone* pZone)

{
    __BEGIN_TRY

    Assert(pZone != NULL);

    vector<NumZoneObjectRow> rows =
        defaultItemObjectRepository().loadNumItemInZone(GEAR_EVENT_STAR, (int)STORAGE_ZONE, pZone->getZoneID());

    for (size_t r = 0; r < rows.size(); r++) {
        EventStar* pEventStar = new EventStar();

        pEventStar->setItemID(rows[r].itemID);
        pEventStar->setObjectID(rows[r].objectID);
        pEventStar->setItemType(rows[r].itemType);

        Storage storage = (Storage)rows[r].storage;
        BYTE x = rows[r].x;
        BYTE y = rows[r].y;

        pEventStar->setNum(rows[r].num);
        pEventStar->setCreateType((Item::CreateType)rows[r].createType);

        switch (storage) {
        case STORAGE_ZONE: {
            Tile& pTile = pZone->getTile(x, y);
            Assert(!pTile.hasItem());
            pTile.addItem(pEventStar);
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

void EventStarLoader::load(StorageID_t storageID, Inventory* pInventory)

{
    __BEGIN_TRY

    Statement* pStmt;

    BEGIN_DB {}
    END_DB(pStmt)

    __END_CATCH
}
