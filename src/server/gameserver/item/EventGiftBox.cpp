//////////////////////////////////////////////////////////////////////////////
// Filename    : EventGiftBox.cpp
// Written By  : excel96
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "EventGiftBox.h"

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

ItemID_t EventGiftBox::m_ItemIDRegistry = 0;
Mutex EventGiftBox::m_Mutex;

//////////////////////////////////////////////////////////////////////////////
// class EventGiftBox member methods
//////////////////////////////////////////////////////////////////////////////

EventGiftBox::EventGiftBox()

{
    m_ItemType = 0;
}

EventGiftBox::EventGiftBox(ItemType_t itemType, const list<OptionType_t>& optionType)

    : m_ItemType(itemType) {
    if (!de::gameContext().itemInfos().isPossibleItem(getItemClass(), m_ItemType, optionType)) {
        filelog("itembug.log", "EventGiftBox::EventGiftBox() : Invalid item type or option type");
        throw Error("EventGiftBox::EventGiftBox() : Invalid item type or optionType");
    }
}

void EventGiftBox::create(const string& ownerID, Storage storage, StorageID_t storageID, BYTE x, BYTE y,
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

    defaultItemObjectRepository().insertPlainItem(GEAR_EVENT_GIFT_BOX, m_ItemID, m_ObjectID, m_ItemType, ownerID,
                                                  (int)storage, storageID, (int)x, (int)y);

    __END_CATCH
}

//--------------------------------------------------------------------------------
// save item
//--------------------------------------------------------------------------------
void EventGiftBox::tinysave(const char* field) const

{
    __BEGIN_TRY

    defaultItemObjectRepository().tinysaveGear(GEAR_EVENT_GIFT_BOX, field, m_ItemID);

    __END_CATCH
}

void EventGiftBox::save(const string& ownerID, Storage storage, StorageID_t storageID, BYTE x, BYTE y)

{
    __BEGIN_TRY

    defaultItemObjectRepository().updatePlainItem(GEAR_EVENT_GIFT_BOX, m_ObjectID, m_ItemType, ownerID, (int)storage,
                                                  storageID, (int)x, (int)y, m_ItemID);

    __END_CATCH
}

string EventGiftBox::toString() const

{
    StringStream msg;
    msg << "EventGiftBox(" << "ItemID:" << m_ItemID << ",ItemType:" << m_ItemType << ")";
    return msg.toString();
}

VolumeWidth_t EventGiftBox::getVolumeWidth() const

{
    __BEGIN_TRY

    return de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_EVENT_GIFT_BOX, m_ItemType)->getVolumeWidth();

    __END_CATCH
}

VolumeHeight_t EventGiftBox::getVolumeHeight() const

{
    __BEGIN_TRY

    return de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_EVENT_GIFT_BOX, m_ItemType)->getVolumeHeight();

    __END_CATCH
}

Weight_t EventGiftBox::getWeight() const

{
    __BEGIN_TRY

    return de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_EVENT_GIFT_BOX, m_ItemType)->getWeight();

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
// class EventGiftBoxInfo member methods
//////////////////////////////////////////////////////////////////////////////

string EventGiftBoxInfo::toString() const

{
    StringStream msg;
    msg << "EventGiftBoxInfo(" << "ItemType:" << m_ItemType << ",Name:" << m_Name << ",EName:" << m_EName
        << ",Price:" << m_Price << ",VolumeType:" << Volume2String[m_VolumeType] << ",Weight:" << m_Weight
        << ",Description:" << m_Description << ")";
    return msg.toString();
}

void EventGiftBoxInfoManager::load()

{
    __BEGIN_TRY

    m_InfoCount = defaultItemObjectRepository().loadMaxGearType(GEAR_EVENT_GIFT_BOX);

    m_pItemInfos = new ItemInfo*[m_InfoCount + 1];

    for (uint i = 0; i <= m_InfoCount; i++)
        m_pItemInfos[i] = NULL;

    vector<BasicInfoRow> rows = defaultItemObjectRepository().loadBasicInfos(GEAR_EVENT_GIFT_BOX);

    for (size_t r = 0; r < rows.size(); r++) {
        EventGiftBoxInfo* pEventGiftBoxInfo = new EventGiftBoxInfo();

        pEventGiftBoxInfo->setItemType(rows[r].itemType);
        pEventGiftBoxInfo->setName(rows[r].name);
        pEventGiftBoxInfo->setEName(rows[r].ename);
        pEventGiftBoxInfo->setPrice(rows[r].price);
        pEventGiftBoxInfo->setVolumeType(rows[r].volume);
        pEventGiftBoxInfo->setWeight(rows[r].weight);
        pEventGiftBoxInfo->setRatio(rows[r].ratio);

        addItemInfo(pEventGiftBoxInfo);
    }

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
// class EventGiftBoxLoader member methods
//////////////////////////////////////////////////////////////////////////////

void EventGiftBoxLoader::load(Creature* pCreature)

{
    __BEGIN_TRY

    Assert(pCreature != NULL);

    vector<PlainObjectRow> rows =
        defaultItemObjectRepository().loadPlainItemOfOwner(GEAR_EVENT_GIFT_BOX, pCreature->getName());

    for (size_t r = 0; r < rows.size(); r++) {
        try {
            EventGiftBox* pEventGiftBox = new EventGiftBox();

            pEventGiftBox->setItemID(rows[r].itemID);
            pEventGiftBox->setObjectID(rows[r].objectID);
            pEventGiftBox->setItemType(rows[r].itemType);

            Storage storage = (Storage)rows[r].storage;
            BYTE x = rows[r].x;
            BYTE y = rows[r].y;

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
                if (pInventory->canAddingEx(x, y, pEventGiftBox)) {
                    pInventory->addItemEx(x, y, pEventGiftBox);
                } else {
                    processItemBugEx(pCreature, pEventGiftBox);
                }
                break;

            case STORAGE_GEAR:
                processItemBugEx(pCreature, pEventGiftBox);
                break;

            case STORAGE_BELT:
                processItemBugEx(pCreature, pEventGiftBox);
                break;

            case STORAGE_EXTRASLOT:
                if (pCreature->isSlayer())
                    pSlayer->addItemToExtraInventorySlot(pEventGiftBox);
                else if (pCreature->isVampire())
                    pVampire->addItemToExtraInventorySlot(pEventGiftBox);
                else if (pCreature->isOusters())
                    pOusters->addItemToExtraInventorySlot(pEventGiftBox);
                break;

            case STORAGE_MOTORCYCLE:
                processItemBugEx(pCreature, pEventGiftBox);
                break;

            case STORAGE_STASH:
                if (pStash->isExist(x, y)) {
                    processItemBugEx(pCreature, pEventGiftBox);
                } else
                    pStash->insert(x, y, pEventGiftBox);
                break;

            case STORAGE_GARBAGE:
                processItemBug(pCreature, pEventGiftBox);
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

void EventGiftBoxLoader::load(Zone* pZone)

{
    __BEGIN_TRY

    Assert(pZone != NULL);

    vector<PlainZoneObjectRow> rows =
        defaultItemObjectRepository().loadPlainItemInZone(GEAR_EVENT_GIFT_BOX, (int)STORAGE_ZONE, pZone->getZoneID());

    for (size_t r = 0; r < rows.size(); r++) {
        EventGiftBox* pEventGiftBox = new EventGiftBox();

        pEventGiftBox->setItemID(rows[r].itemID);
        pEventGiftBox->setObjectID(rows[r].objectID);
        pEventGiftBox->setItemType(rows[r].itemType);

        Storage storage = (Storage)rows[r].storage;
        BYTE x = rows[r].x;
        BYTE y = rows[r].y;

        switch (storage) {
        case STORAGE_ZONE: {
            Tile& pTile = pZone->getTile(x, y);
            Assert(!pTile.hasItem());
            pTile.addItem(pEventGiftBox);
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

void EventGiftBoxLoader::load(StorageID_t storageID, Inventory* pInventory)

{
    __BEGIN_TRY

    Statement* pStmt;

    BEGIN_DB {}
    END_DB(pStmt)

    __END_CATCH
}
