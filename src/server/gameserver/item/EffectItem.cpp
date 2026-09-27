//////////////////////////////////////////////////////////////////////////////
// Filename    : EffectItem.cpp
// Written By  : excel96
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "EffectItem.h"

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

ItemID_t EffectItem::m_ItemIDRegistry = 0;
Mutex EffectItem::m_Mutex;

//////////////////////////////////////////////////////////////////////////////
// class EffectItem member methods
//////////////////////////////////////////////////////////////////////////////

EffectItem::EffectItem()

{
    setItemType(0);
}

EffectItem::EffectItem(ItemType_t itemType, const list<OptionType_t>& optionType, ItemNum_t Num)

{
    setItemType(itemType);
    setNum(Num);

    if (!de::gameContext().itemInfos().isPossibleItem(getItemClass(), getItemType(), getOptionTypeList())) {
        filelog("itembug.log", "EffectItem::EffectItem() : Invalid item type or option type");
        throw Error("EffectItem::EffectItem() : Invalid item type or optionType");
    }
}

void EffectItem::create(const string& ownerID, Storage storage, StorageID_t storageID, BYTE x, BYTE y, ItemID_t itemID)

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

    defaultItemObjectRepository().insertNumItem(GEAR_EFFECT_ITEM, m_ItemID, m_ObjectID, getItemType(), ownerID,
                                                (int)storage, storageID, (int)x, (int)y, (int)getNum(),
                                                (int)m_CreateType);

    __END_CATCH
}

//--------------------------------------------------------------------------------
// save item
//--------------------------------------------------------------------------------
void EffectItem::tinysave(const char* field) const

{
    __BEGIN_TRY

    defaultItemObjectRepository().tinysaveGear(GEAR_EFFECT_ITEM, field, m_ItemID);

    __END_CATCH
}

void EffectItem::save(const string& ownerID, Storage storage, StorageID_t storageID, BYTE x, BYTE y)

{
    __BEGIN_TRY

    defaultItemObjectRepository().updateNumItem(GEAR_EFFECT_ITEM, m_ObjectID, getItemType(), ownerID, (int)storage,
                                                storageID, (int)x, (int)y, (int)getNum(), m_ItemID);

    __END_CATCH
}

string EffectItem::toString() const

{
    StringStream msg;

    msg << "EffectItem(" << "ItemID:" << m_ItemID << ",ItemType:" << (int)getItemType() << ",Num:" << (int)getNum()
        << ")";

    return msg.toString();
}

//////////////////////////////////////////////////////////////////////////////
// class EffectItemInfo member methods
//////////////////////////////////////////////////////////////////////////////

string EffectItemInfo::toString() const

{
    StringStream msg;
    msg << "EffectItemInfo(" << "ItemType:" << (int)m_ItemType << ",Name:" << m_Name << ",EName:" << m_EName
        << ",Price:" << (int)m_Price << ",VolumeType:" << Volume2String[m_VolumeType] << ",Weight:" << (int)m_Weight
        << ",Description:" << m_Description << ")";
    return msg.toString();
}

void EffectItemInfoManager::load()

{
    __BEGIN_TRY

    m_InfoCount = defaultItemObjectRepository().loadMaxGearType(GEAR_EFFECT_ITEM);

    m_pItemInfos = new ItemInfo*[m_InfoCount + 1];

    for (uint i = 0; i <= m_InfoCount; i++)
        m_pItemInfos[i] = NULL;

    vector<EffectInfoRow> rows = defaultItemObjectRepository().loadEffectInfos(GEAR_EFFECT_ITEM);

    for (size_t r = 0; r < rows.size(); r++) {
        EffectItemInfo* pEffectItemInfo = new EffectItemInfo();

        pEffectItemInfo->setItemType(rows[r].basic.itemType);
        pEffectItemInfo->setName(rows[r].basic.name);
        pEffectItemInfo->setEName(rows[r].basic.ename);
        pEffectItemInfo->setPrice(rows[r].basic.price);
        pEffectItemInfo->setVolumeType(rows[r].basic.volume);
        pEffectItemInfo->setWeight(rows[r].basic.weight);
        pEffectItemInfo->setRatio(rows[r].basic.ratio);
        pEffectItemInfo->setEffectClass((Effect::EffectClass)rows[r].effectClass);
        pEffectItemInfo->setDuration(rows[r].timeSec);

        addItemInfo(pEffectItemInfo);
    }

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
// class EffectItemLoader member methods
//////////////////////////////////////////////////////////////////////////////

void EffectItemLoader::load(Creature* pCreature)

{
    __BEGIN_TRY

    Assert(pCreature != NULL);

    vector<NumObjectRow> rows =
        defaultItemObjectRepository().loadNumItemOfOwner(GEAR_EFFECT_ITEM, pCreature->getName());

    for (size_t r = 0; r < rows.size(); r++) {
        try {
            EffectItem* pEffectItem = new EffectItem();

            pEffectItem->setItemID(rows[r].itemID);
            pEffectItem->setObjectID(rows[r].objectID);
            pEffectItem->setItemType(rows[r].itemType);

            Storage storage = (Storage)rows[r].storage;
            BYTE x = rows[r].x;
            BYTE y = rows[r].y;

            pEffectItem->setNum(rows[r].num);
            pEffectItem->setCreateType((Item::CreateType)rows[r].createType);

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
                if (pInventory->canAddingEx(x, y, pEffectItem)) {
                    pInventory->addItemEx(x, y, pEffectItem);
                } else {
                    processItemBugEx(pCreature, pEffectItem);
                }
                break;

            case STORAGE_GEAR:
                processItemBugEx(pCreature, pEffectItem);
                break;

            case STORAGE_BELT:
                processItemBugEx(pCreature, pEffectItem);
                break;

            case STORAGE_EXTRASLOT:
                if (pCreature->isSlayer())
                    pSlayer->addItemToExtraInventorySlot(pEffectItem);
                else if (pCreature->isVampire())
                    pVampire->addItemToExtraInventorySlot(pEffectItem);
                else if (pCreature->isOusters())
                    pOusters->addItemToExtraInventorySlot(pEffectItem);
                break;

            case STORAGE_MOTORCYCLE:
                processItemBugEx(pCreature, pEffectItem);
                break;

            case STORAGE_STASH:
                if (pStash->isExist(x, y)) {
                    processItemBugEx(pCreature, pEffectItem);
                } else
                    pStash->insert(x, y, pEffectItem);
                break;

            case STORAGE_GARBAGE:
                processItemBug(pCreature, pEffectItem);
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

void EffectItemLoader::load(Zone* pZone)

{
    __BEGIN_TRY

    Assert(pZone != NULL);

    vector<NumZoneObjectRow> rows =
        defaultItemObjectRepository().loadNumItemInZone(GEAR_EFFECT_ITEM, (int)STORAGE_ZONE, pZone->getZoneID());

    for (size_t r = 0; r < rows.size(); r++) {
        EffectItem* pEffectItem = new EffectItem();

        pEffectItem->setItemID(rows[r].itemID);
        pEffectItem->setObjectID(rows[r].objectID);
        pEffectItem->setItemType(rows[r].itemType);

        Storage storage = (Storage)rows[r].storage;
        BYTE x = rows[r].x;
        BYTE y = rows[r].y;

        pEffectItem->setNum(rows[r].num);
        pEffectItem->setCreateType((Item::CreateType)rows[r].createType);

        switch (storage) {
        case STORAGE_ZONE: {
            Tile& pTile = pZone->getTile(x, y);
            Assert(!pTile.hasItem());
            pTile.addItem(pEffectItem);
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

void EffectItemLoader::load(StorageID_t storageID, Inventory* pInventory)

{
    __BEGIN_TRY

    Statement* pStmt;

    BEGIN_DB {}
    END_DB(pStmt)

    __END_CATCH
}
