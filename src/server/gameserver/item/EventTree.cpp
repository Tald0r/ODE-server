//////////////////////////////////////////////////////////////////////////////
// Filename    : EventTree.cpp
// Written By  : excel96
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "EventTree.h"

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

ItemID_t EventTree::m_ItemIDRegistry = 0;
Mutex EventTree::m_Mutex;

//////////////////////////////////////////////////////////////////////////////
// class EventTree member methods
//////////////////////////////////////////////////////////////////////////////

EventTree::EventTree()

{
    m_ItemType = 0;
}

EventTree::EventTree(ItemType_t itemType, const list<OptionType_t>& optionType, ItemNum_t Num)

{
    m_ItemType = itemType;
    m_Num = Num;

    if (!de::gameContext().itemInfos().isPossibleItem(getItemClass(), m_ItemType, optionType)) {
        filelog("itembug.log", "EventTree::EventTree() : Invalid item type or option type");
        throw Error("EventTree::EventTree() : Invalid item type or optionType");
    }
}

void EventTree::create(const string& ownerID, Storage storage, StorageID_t storageID, BYTE x, BYTE y, ItemID_t itemID)

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

    defaultItemObjectRepository().insertNumItem(GEAR_EVENT_TREE, m_ItemID, m_ObjectID, m_ItemType, ownerID,
                                                (int)storage, storageID, (int)x, (int)y, (int)m_Num, (int)m_CreateType);

    __END_CATCH
}

//--------------------------------------------------------------------------------
// save item
//--------------------------------------------------------------------------------
void EventTree::tinysave(const char* field) const

{
    __BEGIN_TRY

    defaultItemObjectRepository().tinysaveGear(GEAR_EVENT_TREE, field, m_ItemID);

    __END_CATCH
}

void EventTree::save(const string& ownerID, Storage storage, StorageID_t storageID, BYTE x, BYTE y)

{
    __BEGIN_TRY

    defaultItemObjectRepository().updateNumItem(GEAR_EVENT_TREE, m_ObjectID, m_ItemType, ownerID, (int)storage,
                                                storageID, (int)x, (int)y, (int)m_Num, m_ItemID);

    __END_CATCH
}

string EventTree::toString() const

{
    StringStream msg;

    msg << "EventTree(" << "ItemID:" << m_ItemID << ",ItemType:" << (int)m_ItemType << ",Num:" << (int)m_Num << ")";

    return msg.toString();
}

VolumeWidth_t EventTree::getVolumeWidth() const

{
    __BEGIN_TRY

    return de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_EVENT_TREE, m_ItemType)->getVolumeWidth();

    __END_CATCH
}

VolumeHeight_t EventTree::getVolumeHeight() const

{
    __BEGIN_TRY

    return de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_EVENT_TREE, m_ItemType)->getVolumeHeight();

    __END_CATCH
}

Weight_t EventTree::getWeight() const

{
    __BEGIN_TRY

    return de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_EVENT_TREE, m_ItemType)->getWeight();

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
// class EventTreeInfo member methods
//////////////////////////////////////////////////////////////////////////////

string EventTreeInfo::toString() const

{
    StringStream msg;
    msg << "EventTreeInfo(" << "ItemType:" << (int)m_ItemType << ",Name:" << m_Name << ",EName:" << m_EName
        << ",Price:" << (int)m_Price << ",VolumeType:" << Volume2String[m_VolumeType] << ",Weight:" << (int)m_Weight
        << ",Description:" << m_Description << ")";
    return msg.toString();
}

void EventTreeInfoManager::load()

{
    __BEGIN_TRY

    m_InfoCount = defaultItemObjectRepository().loadMaxGearType(GEAR_EVENT_TREE);

    m_pItemInfos = new ItemInfo*[m_InfoCount + 1];

    for (uint i = 0; i <= m_InfoCount; i++)
        m_pItemInfos[i] = NULL;

    vector<BasicInfoRow> rows = defaultItemObjectRepository().loadBasicInfos(GEAR_EVENT_TREE);

    for (size_t r = 0; r < rows.size(); r++) {
        EventTreeInfo* pEventTreeInfo = new EventTreeInfo();

        pEventTreeInfo->setItemType(rows[r].itemType);
        pEventTreeInfo->setName(rows[r].name);
        pEventTreeInfo->setEName(rows[r].ename);
        pEventTreeInfo->setPrice(rows[r].price);
        pEventTreeInfo->setVolumeType(rows[r].volume);
        pEventTreeInfo->setWeight(rows[r].weight);
        pEventTreeInfo->setRatio(rows[r].ratio);

        addItemInfo(pEventTreeInfo);
    }

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
// class EventTreeLoader member methods
//////////////////////////////////////////////////////////////////////////////

void EventTreeLoader::load(Creature* pCreature)

{
    __BEGIN_TRY

    Assert(pCreature != NULL);

    vector<NumObjectRow> rows = defaultItemObjectRepository().loadNumItemOfOwner(GEAR_EVENT_TREE, pCreature->getName());

    for (size_t r = 0; r < rows.size(); r++) {
        try {
            EventTree* pEventTree = new EventTree();

            pEventTree->setItemID(rows[r].itemID);
            pEventTree->setObjectID(rows[r].objectID);
            pEventTree->setItemType(rows[r].itemType);

            if (pEventTree->getItemType() > 12)
                pEventTree->setQuestItem();

            Storage storage = (Storage)rows[r].storage;
            BYTE x = rows[r].x;
            BYTE y = rows[r].y;

            pEventTree->setNum(rows[r].num);
            pEventTree->setCreateType((Item::CreateType)rows[r].createType);

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
                if (pInventory->canAddingEx(x, y, pEventTree)) {
                    pInventory->addItemEx(x, y, pEventTree);
                } else {
                    processItemBugEx(pCreature, pEventTree);
                }
                break;

            case STORAGE_GEAR:
                processItemBugEx(pCreature, pEventTree);
                break;

            case STORAGE_BELT:
                processItemBugEx(pCreature, pEventTree);
                break;

            case STORAGE_EXTRASLOT:
                if (pCreature->isSlayer())
                    pSlayer->addItemToExtraInventorySlot(pEventTree);
                else if (pCreature->isVampire())
                    pVampire->addItemToExtraInventorySlot(pEventTree);
                else if (pCreature->isOusters())
                    pOusters->addItemToExtraInventorySlot(pEventTree);
                break;

            case STORAGE_MOTORCYCLE:
                processItemBugEx(pCreature, pEventTree);
                break;

            case STORAGE_STASH:
                if (pStash->isExist(x, y)) {
                    processItemBugEx(pCreature, pEventTree);
                } else
                    pStash->insert(x, y, pEventTree);
                break;

            case STORAGE_GARBAGE:
                processItemBug(pCreature, pEventTree);
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

void EventTreeLoader::load(Zone* pZone)

{
    __BEGIN_TRY

    Assert(pZone != NULL);

    vector<NumZoneObjectRow> rows =
        defaultItemObjectRepository().loadNumItemInZone(GEAR_EVENT_TREE, (int)STORAGE_ZONE, pZone->getZoneID());

    for (size_t r = 0; r < rows.size(); r++) {
        EventTree* pEventTree = new EventTree();

        pEventTree->setItemID(rows[r].itemID);
        pEventTree->setObjectID(rows[r].objectID);
        pEventTree->setItemType(rows[r].itemType);

        Storage storage = (Storage)rows[r].storage;
        BYTE x = rows[r].x;
        BYTE y = rows[r].y;

        pEventTree->setNum(rows[r].num);
        pEventTree->setCreateType((Item::CreateType)rows[r].createType);

        switch (storage) {
        case STORAGE_ZONE: {
            Tile& pTile = pZone->getTile(x, y);
            Assert(!pTile.hasItem());
            pTile.addItem(pEventTree);
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

void EventTreeLoader::load(StorageID_t storageID, Inventory* pInventory)

{
    __BEGIN_TRY

    Statement* pStmt;

    BEGIN_DB {}
    END_DB(pStmt)

    __END_CATCH
}
