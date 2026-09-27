//////////////////////////////////////////////////////////////////////////////
// Filename    : LearningItem.cpp
// Written By  : Elca
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "LearningItem.h"

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

ItemID_t LearningItem::m_ItemIDRegistry = 0;
Mutex LearningItem::m_Mutex;

//--------------------------------------------------------------------------------
// constructor
//--------------------------------------------------------------------------------
LearningItem::LearningItem()

    : m_ItemType(0) {}

LearningItem::LearningItem(ItemType_t itemType, const list<OptionType_t>& optionType)

    : m_ItemType(itemType) {
    if (!de::gameContext().itemInfos().isPossibleItem(getItemClass(), m_ItemType, optionType)) {
        filelog("itembug.log", "LearningItem::LearningItem() : Invalid item type or option type");
        throw Error("LearningItem::LearningItem() : Invalid item type or optionType");
    }
}


//--------------------------------------------------------------------------------
// create item
//--------------------------------------------------------------------------------
void LearningItem::create(const string& ownerID, Storage storage, StorageID_t storageID, BYTE x, BYTE y,
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

    defaultItemObjectRepository().insertPlainItem(GEAR_LEARNING_ITEM, m_ItemID, m_ObjectID, m_ItemType, ownerID,
                                                  (int)storage, storageID, (int)x, (int)y);

    __END_CATCH
}


//--------------------------------------------------------------------------------
// save item
//--------------------------------------------------------------------------------
void LearningItem::tinysave(const char* field) const

{
    __BEGIN_TRY

    defaultItemObjectRepository().tinysaveGear(GEAR_LEARNING_ITEM, field, m_ItemID);

    __END_CATCH
}

//--------------------------------------------------------------------------------
// save item
//--------------------------------------------------------------------------------
void LearningItem::save(const string& ownerID, Storage storage, StorageID_t storageID, BYTE x, BYTE y)

{
    __BEGIN_TRY

    defaultItemObjectRepository().updatePlainItem(GEAR_LEARNING_ITEM, m_ObjectID, m_ItemType, ownerID, (int)storage,
                                                  storageID, (int)x, (int)y, m_ItemID);

    __END_CATCH
}


//--------------------------------------------------------------------------------
// get debug string
//--------------------------------------------------------------------------------
string LearningItem::toString() const

{
    StringStream msg;

    msg << "LearningItem(" << "ItemID:" << m_ItemID << ",ItemType:" << (int)m_ItemType << ")";

    return msg.toString();
}


//--------------------------------------------------------------------------------
// get width
//--------------------------------------------------------------------------------
VolumeWidth_t LearningItem::getVolumeWidth() const

{
    __BEGIN_TRY

    return de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_LEARNINGITEM, m_ItemType)->getVolumeWidth();

    __END_CATCH
}


//--------------------------------------------------------------------------------
// get height
//--------------------------------------------------------------------------------
VolumeHeight_t LearningItem::getVolumeHeight() const

{
    __BEGIN_TRY

    return de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_LEARNINGITEM, m_ItemType)->getVolumeHeight();

    __END_CATCH
}


//--------------------------------------------------------------------------------
// get weight
//--------------------------------------------------------------------------------
Weight_t LearningItem::getWeight() const

{
    __BEGIN_TRY

    return de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_LEARNINGITEM, m_ItemType)->getWeight();

    __END_CATCH
}


//--------------------------------------------------------------------------------
// get debug string
//--------------------------------------------------------------------------------
string LearningItemInfo::toString() const

{
    StringStream msg;

    msg << "LearningItemInfo(" << "ItemType:" << m_ItemType << ",Name:" << m_Name << ",EName:" << m_EName
        << ",Price:" << m_Price << ",VolumeType:" << Volume2String[m_VolumeType] << ",Weight:" << m_Weight
        << ",Description:" << m_Description << ",SkillType:" << m_SkillType << ")";

    return msg.toString();
}


//--------------------------------------------------------------------------------
// load from DB
//--------------------------------------------------------------------------------
void LearningItemInfoManager::load()

{
    __BEGIN_TRY

    m_InfoCount = defaultItemObjectRepository().loadMaxGearType(GEAR_LEARNING_ITEM);

    m_pItemInfos = new ItemInfo*[m_InfoCount + 1];

    for (uint i = 0; i <= m_InfoCount; i++)
        m_pItemInfos[i] = NULL;

    vector<IntInfoRow> rows = defaultItemObjectRepository().loadIntInfos(GEAR_LEARNING_ITEM);

    for (size_t r = 0; r < rows.size(); r++) {
        LearningItemInfo* pLearningItemInfo = new LearningItemInfo();

        pLearningItemInfo->setItemType(rows[r].basic.itemType);
        pLearningItemInfo->setName(rows[r].basic.name);
        pLearningItemInfo->setEName(rows[r].basic.ename);
        pLearningItemInfo->setPrice(rows[r].basic.price);
        pLearningItemInfo->setVolumeType(rows[r].basic.volume);
        pLearningItemInfo->setWeight(rows[r].basic.weight);
        pLearningItemInfo->setRatio(rows[r].basic.ratio);
        pLearningItemInfo->setSkillType(rows[r].value);

        addItemInfo(pLearningItemInfo);
    }

    __END_CATCH
}


//--------------------------------------------------------------------------------
// load to creature
//--------------------------------------------------------------------------------
void LearningItemLoader::load(Creature* pCreature)

{
    __BEGIN_TRY

    Assert(pCreature != NULL);

    vector<PlainObjectRow> rows =
        defaultItemObjectRepository().loadPlainItemOfOwner(GEAR_LEARNING_ITEM, pCreature->getName());

    for (size_t r = 0; r < rows.size(); r++) {
        try {
            LearningItem* pLearningItem = new LearningItem();

            pLearningItem->setItemID(rows[r].itemID);
            pLearningItem->setObjectID(rows[r].objectID);
            pLearningItem->setItemType(rows[r].itemType);

            Storage storage = (Storage)rows[r].storage;
            BYTE x = rows[r].x;
            BYTE y = rows[r].y;

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

            switch (storage) {
            case STORAGE_INVENTORY:
                if (pInventory->canAddingEx(x, y, pLearningItem)) {
                    pInventory->addItemEx(x, y, pLearningItem);
                } else {
                    processItemBugEx(pCreature, pLearningItem);
                }
                break;

            case STORAGE_GEAR:
                processItemBugEx(pCreature, pLearningItem);
                break;

            case STORAGE_BELT:
                processItemBugEx(pCreature, pLearningItem);
                break;

            case STORAGE_EXTRASLOT:
                if (pCreature->isSlayer())
                    pSlayer->addItemToExtraInventorySlot(pLearningItem);
                else if (pCreature->isVampire())
                    pVampire->addItemToExtraInventorySlot(pLearningItem);
                break;

            case STORAGE_MOTORCYCLE:
                processItemBugEx(pCreature, pLearningItem);
                break;

            case STORAGE_STASH:
                if (pStash->isExist(x, y)) {
                    processItemBugEx(pCreature, pLearningItem);
                } else
                    pStash->insert(x, y, pLearningItem);
                break;

            case STORAGE_GARBAGE:
                processItemBug(pCreature, pLearningItem);
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
void LearningItemLoader::load(Zone* pZone)

{
    __BEGIN_TRY

    Assert(pZone != NULL);

    vector<PlainZoneObjectRow> rows =
        defaultItemObjectRepository().loadPlainItemInZone(GEAR_LEARNING_ITEM, (int)STORAGE_ZONE, pZone->getZoneID());

    for (size_t r = 0; r < rows.size(); r++) {
        LearningItem* pLearningItem = new LearningItem();

        pLearningItem->setItemID(rows[r].itemID);
        pLearningItem->setObjectID(rows[r].objectID);
        pLearningItem->setItemType(rows[r].itemType);

        Storage storage = (Storage)rows[r].storage;
        BYTE x = rows[r].x;
        BYTE y = rows[r].y;

        switch (storage) {
        case STORAGE_ZONE: {
            Tile& pTile = pZone->getTile(x, y);
            Assert(!pTile.hasItem());
            pTile.addItem(pLearningItem);
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
void LearningItemLoader::load(StorageID_t storageID, Inventory* pInventory)

{
    __BEGIN_TRY

    Statement* pStmt;

    BEGIN_DB {}
    END_DB(pStmt)

    __END_CATCH
}
