//////////////////////////////////////////////////////////////////////////////
// Filename    : QuestItem.cpp
// Written By  : excel96
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "QuestItem.h"

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

ItemID_t QuestItem::m_ItemIDRegistry = 0;
Mutex QuestItem::m_Mutex;

//////////////////////////////////////////////////////////////////////////////
// class QuestItem member methods
//////////////////////////////////////////////////////////////////////////////

QuestItem::QuestItem()

{
    m_ItemType = 0;
}

QuestItem::QuestItem(ItemType_t itemType, const list<OptionType_t>& optionType)

{
    m_ItemType = itemType;

    if (!de::gameContext().itemInfos().isPossibleItem(getItemClass(), m_ItemType, optionType)) {
        filelog("itembug.log", "QuestItem::QuestItem() : Invalid item type or option type");
        throw Error("QuestItem::QuestItem() : Invalid item type or optionType");
    }
}

void QuestItem::create(const string& ownerID, Storage storage, StorageID_t storageID, BYTE x, BYTE y, ItemID_t itemID)

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

    defaultItemObjectRepository().insertFlagItem(GEAR_QUEST_ITEM, m_ItemID, m_ObjectID, m_ItemType, ownerID,
                                                 (int)storage, storageID, (int)x, (int)y, (int)m_CreateType);

    __END_CATCH
}

//--------------------------------------------------------------------------------
// save item
//--------------------------------------------------------------------------------
void QuestItem::tinysave(const char* field) const

{
    __BEGIN_TRY

    defaultItemObjectRepository().tinysaveGear(GEAR_QUEST_ITEM, field, m_ItemID);

    __END_CATCH
}

void QuestItem::save(const string& ownerID, Storage storage, StorageID_t storageID, BYTE x, BYTE y)

{
    __BEGIN_TRY

    defaultItemObjectRepository().updatePlainItem(GEAR_QUEST_ITEM, m_ObjectID, m_ItemType, ownerID, (int)storage,
                                                  storageID, (int)x, (int)y, m_ItemID);

    __END_CATCH
}

string QuestItem::toString() const

{
    StringStream msg;

    msg << "QuestItem(" << "ItemID:" << m_ItemID << ",ItemType:" << (int)m_ItemType << ")";

    return msg.toString();
}

VolumeWidth_t QuestItem::getVolumeWidth() const

{
    __BEGIN_TRY

    return de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_QUEST_ITEM, m_ItemType)->getVolumeWidth();

    __END_CATCH
}

VolumeHeight_t QuestItem::getVolumeHeight() const

{
    __BEGIN_TRY

    return de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_QUEST_ITEM, m_ItemType)->getVolumeHeight();

    __END_CATCH
}

Weight_t QuestItem::getWeight() const

{
    __BEGIN_TRY

    return de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_QUEST_ITEM, m_ItemType)->getWeight();

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
// class QuestItemInfo member methods
//////////////////////////////////////////////////////////////////////////////

string QuestItemInfo::toString() const

{
    StringStream msg;
    msg << "QuestItemInfo(" << "ItemType:" << (int)m_ItemType << ",Name:" << m_Name << ",EName:" << m_EName
        << ",Price:" << (int)m_Price << ",VolumeType:" << Volume2String[m_VolumeType] << ",Weight:" << (int)m_Weight
        << ",BonusRatio:" << (int)m_BonusRatio << ",Description:" << m_Description << ")";
    return msg.toString();
}

void QuestItemInfoManager::load()

{
    __BEGIN_TRY

    m_InfoCount = defaultItemObjectRepository().loadMaxGearType(GEAR_QUEST_ITEM);

    m_pItemInfos = new ItemInfo*[m_InfoCount + 1];

    for (uint i = 0; i <= m_InfoCount; i++)
        m_pItemInfos[i] = NULL;

    vector<IntInfoRow> rows = defaultItemObjectRepository().loadIntInfos(GEAR_QUEST_ITEM);

    for (size_t r = 0; r < rows.size(); r++) {
        QuestItemInfo* pQuestItemInfo = new QuestItemInfo();

        pQuestItemInfo->setItemType(rows[r].basic.itemType);
        pQuestItemInfo->setName(rows[r].basic.name);
        pQuestItemInfo->setEName(rows[r].basic.ename);
        pQuestItemInfo->setPrice(rows[r].basic.price);
        pQuestItemInfo->setVolumeType(rows[r].basic.volume);
        pQuestItemInfo->setWeight(rows[r].basic.weight);
        pQuestItemInfo->setRatio(rows[r].basic.ratio);
        pQuestItemInfo->setBonusRatio(rows[r].value);

        addItemInfo(pQuestItemInfo);
    }

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
// class QuestItemLoader member methods
//////////////////////////////////////////////////////////////////////////////

void QuestItemLoader::load(Creature* pCreature)

{
    __BEGIN_TRY

    Assert(pCreature != NULL);

    vector<FlagObjectRow> rows =
        defaultItemObjectRepository().loadFlagItemOfOwner(GEAR_QUEST_ITEM, pCreature->getName());

    for (size_t r = 0; r < rows.size(); r++) {
        try {
            QuestItem* pQuestItem = new QuestItem();

            pQuestItem->setItemID(rows[r].itemID);
            pQuestItem->setObjectID(rows[r].objectID);
            pQuestItem->setItemType(rows[r].itemType);

            Storage storage = (Storage)rows[r].storage;
            BYTE x = rows[r].x;
            BYTE y = rows[r].y;

            pQuestItem->setCreateType((Item::CreateType)rows[r].createType);

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
                if (pInventory->canAddingEx(x, y, pQuestItem)) {
                    pInventory->addItemEx(x, y, pQuestItem);
                } else {
                    processItemBugEx(pCreature, pQuestItem);
                }
                break;

            case STORAGE_GEAR:
                processItemBugEx(pCreature, pQuestItem);
                break;

            case STORAGE_BELT:
                processItemBugEx(pCreature, pQuestItem);
                break;

            case STORAGE_EXTRASLOT:
                if (pCreature->isSlayer())
                    pSlayer->addItemToExtraInventorySlot(pQuestItem);
                else if (pCreature->isVampire())
                    pVampire->addItemToExtraInventorySlot(pQuestItem);
                else if (pCreature->isOusters())
                    pOusters->addItemToExtraInventorySlot(pQuestItem);
                break;

            case STORAGE_MOTORCYCLE:
                processItemBugEx(pCreature, pQuestItem);
                break;

            case STORAGE_STASH:
                if (pStash->isExist(x, y)) {
                    processItemBugEx(pCreature, pQuestItem);
                } else
                    pStash->insert(x, y, pQuestItem);
                break;

            case STORAGE_GARBAGE:
                processItemBug(pCreature, pQuestItem);
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

void QuestItemLoader::load(Zone* pZone)

{
    __BEGIN_TRY

    Assert(pZone != NULL);

    vector<FlagZoneObjectRow> rows =
        defaultItemObjectRepository().loadFlagItemInZone(GEAR_QUEST_ITEM, (int)STORAGE_ZONE, pZone->getZoneID());

    for (size_t r = 0; r < rows.size(); r++) {
        QuestItem* pQuestItem = new QuestItem();

        pQuestItem->setItemID(rows[r].itemID);
        pQuestItem->setObjectID(rows[r].objectID);
        pQuestItem->setItemType(rows[r].itemType);

        Storage storage = (Storage)rows[r].storage;
        BYTE x = rows[r].x;
        BYTE y = rows[r].y;

        pQuestItem->setCreateType((Item::CreateType)rows[r].createType);

        switch (storage) {
        case STORAGE_ZONE: {
            Tile& pTile = pZone->getTile(x, y);
            Assert(!pTile.hasItem());
            pTile.addItem(pQuestItem);
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

void QuestItemLoader::load(StorageID_t storageID, Inventory* pInventory)

{
    __BEGIN_TRY

    Statement* pStmt;

    BEGIN_DB {}
    END_DB(pStmt)

    __END_CATCH
}
