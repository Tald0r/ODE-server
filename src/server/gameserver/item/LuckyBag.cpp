//////////////////////////////////////////////////////////////////////////////
// Filename    : LuckyBag.cpp
// Written By  : excel96
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "LuckyBag.h"

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

ItemID_t LuckyBag::m_ItemIDRegistry = 0;
Mutex LuckyBag::m_Mutex;

//////////////////////////////////////////////////////////////////////////////
// class LuckyBag member methods
//////////////////////////////////////////////////////////////////////////////

LuckyBag::LuckyBag()

{
    m_ItemType = 0;
}

LuckyBag::LuckyBag(ItemType_t itemType, const list<OptionType_t>& optionType, ItemNum_t Num)

{
    m_ItemType = itemType;
    m_Num = Num;

    if (!de::gameContext().itemInfos().isPossibleItem(getItemClass(), m_ItemType, optionType)) {
        filelog("itembug.log", "LuckyBag::LuckyBag() : Invalid item type or option type");
        throw Error("LuckyBag::LuckyBag() : Invalid item type or optionType");
    }
}

void LuckyBag::create(const string& ownerID, Storage storage, StorageID_t storageID, BYTE x, BYTE y, ItemID_t itemID)

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

    defaultItemObjectRepository().insertNumItem(GEAR_LUCKY_BAG, m_ItemID, m_ObjectID, m_ItemType, ownerID, (int)storage,
                                                storageID, (int)x, (int)y, (int)m_Num, (int)m_CreateType);

    __END_CATCH
}

//--------------------------------------------------------------------------------
// save item
//--------------------------------------------------------------------------------
void LuckyBag::tinysave(const char* field) const

{
    __BEGIN_TRY

    defaultItemObjectRepository().tinysaveGear(GEAR_LUCKY_BAG, field, m_ItemID);

    __END_CATCH
}

void LuckyBag::save(const string& ownerID, Storage storage, StorageID_t storageID, BYTE x, BYTE y)

{
    __BEGIN_TRY

    defaultItemObjectRepository().updateNumItem(GEAR_LUCKY_BAG, m_ObjectID, m_ItemType, ownerID, (int)storage,
                                                storageID, (int)x, (int)y, (int)m_Num, m_ItemID);

    __END_CATCH
}

string LuckyBag::toString() const

{
    StringStream msg;

    msg << "LuckyBag(" << "ItemID:" << m_ItemID << ",ItemType:" << (int)m_ItemType << ",Num:" << (int)m_Num << ")";

    return msg.toString();
}

VolumeWidth_t LuckyBag::getVolumeWidth() const

{
    __BEGIN_TRY

    return de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_LUCKY_BAG, m_ItemType)->getVolumeWidth();

    __END_CATCH
}

VolumeHeight_t LuckyBag::getVolumeHeight() const

{
    __BEGIN_TRY

    return de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_LUCKY_BAG, m_ItemType)->getVolumeHeight();

    __END_CATCH
}

Weight_t LuckyBag::getWeight() const

{
    __BEGIN_TRY

    return de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_LUCKY_BAG, m_ItemType)->getWeight();

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
// class LuckyBagInfo member methods
//////////////////////////////////////////////////////////////////////////////

string LuckyBagInfo::toString() const

{
    StringStream msg;
    msg << "LuckyBagInfo(" << "ItemType:" << (int)m_ItemType << ",Name:" << m_Name << ",EName:" << m_EName
        << ",Price:" << (int)m_Price << ",VolumeType:" << Volume2String[m_VolumeType] << ",Weight:" << (int)m_Weight
        << ")";
    return msg.toString();
}

void LuckyBagInfoManager::load()

{
    __BEGIN_TRY

    m_InfoCount = defaultItemObjectRepository().loadMaxGearType(GEAR_LUCKY_BAG);

    m_pItemInfos = new ItemInfo*[m_InfoCount + 1];

    for (uint i = 0; i <= m_InfoCount; i++)
        m_pItemInfos[i] = NULL;

    vector<BasicInfoRow> rows = defaultItemObjectRepository().loadBasicInfos(GEAR_LUCKY_BAG);

    for (size_t r = 0; r < rows.size(); r++) {
        LuckyBagInfo* pLuckyBagInfo = new LuckyBagInfo();

        pLuckyBagInfo->setItemType(rows[r].itemType);
        pLuckyBagInfo->setName(rows[r].name);
        pLuckyBagInfo->setEName(rows[r].ename);
        pLuckyBagInfo->setPrice(rows[r].price);
        pLuckyBagInfo->setVolumeType(rows[r].volume);
        pLuckyBagInfo->setWeight(rows[r].weight);
        pLuckyBagInfo->setRatio(rows[r].ratio);

        addItemInfo(pLuckyBagInfo);
    }

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
// class LuckyBagLoader member methods
//////////////////////////////////////////////////////////////////////////////

void LuckyBagLoader::load(Creature* pCreature)

{
    __BEGIN_TRY

    Assert(pCreature != NULL);

    vector<NumObjectRow> rows = defaultItemObjectRepository().loadNumItemOfOwner(GEAR_LUCKY_BAG, pCreature->getName());

    for (size_t r = 0; r < rows.size(); r++) {
        try {
            LuckyBag* pLuckyBag = new LuckyBag();

            pLuckyBag->setItemID(rows[r].itemID);
            pLuckyBag->setObjectID(rows[r].objectID);
            pLuckyBag->setItemType(rows[r].itemType);

            Storage storage = (Storage)rows[r].storage;
            BYTE x = rows[r].x;
            BYTE y = rows[r].y;

            pLuckyBag->setNum(rows[r].num);
            pLuckyBag->setCreateType((Item::CreateType)rows[r].createType);

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
                if (pInventory->canAddingEx(x, y, pLuckyBag)) {
                    pInventory->addItemEx(x, y, pLuckyBag);
                } else {
                    processItemBugEx(pCreature, pLuckyBag);
                }
                break;

            case STORAGE_GEAR:
                processItemBugEx(pCreature, pLuckyBag);
                break;

            case STORAGE_BELT:
                processItemBugEx(pCreature, pLuckyBag);
                break;

            case STORAGE_EXTRASLOT:
                if (pCreature->isSlayer())
                    pSlayer->addItemToExtraInventorySlot(pLuckyBag);
                else if (pCreature->isVampire())
                    pVampire->addItemToExtraInventorySlot(pLuckyBag);
                else if (pCreature->isOusters())
                    pOusters->addItemToExtraInventorySlot(pLuckyBag);
                break;

            case STORAGE_MOTORCYCLE:
                processItemBugEx(pCreature, pLuckyBag);
                break;

            case STORAGE_STASH:
                if (pStash->isExist(x, y)) {
                    processItemBugEx(pCreature, pLuckyBag);
                } else
                    pStash->insert(x, y, pLuckyBag);
                break;

            case STORAGE_GARBAGE:
                processItemBug(pCreature, pLuckyBag);
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

void LuckyBagLoader::load(Zone* pZone)

{
    __BEGIN_TRY

    Assert(pZone != NULL);

    vector<NumZoneObjectRow> rows =
        defaultItemObjectRepository().loadNumItemInZone(GEAR_LUCKY_BAG, (int)STORAGE_ZONE, pZone->getZoneID());

    for (size_t r = 0; r < rows.size(); r++) {
        LuckyBag* pLuckyBag = new LuckyBag();

        pLuckyBag->setItemID(rows[r].itemID);
        pLuckyBag->setObjectID(rows[r].objectID);
        pLuckyBag->setItemType(rows[r].itemType);

        Storage storage = (Storage)rows[r].storage;
        BYTE x = rows[r].x;
        BYTE y = rows[r].y;

        pLuckyBag->setNum(rows[r].num);
        pLuckyBag->setCreateType((Item::CreateType)rows[r].createType);

        switch (storage) {
        case STORAGE_ZONE: {
            Tile& pTile = pZone->getTile(x, y);
            Assert(!pTile.hasItem());
            pTile.addItem(pLuckyBag);
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

void LuckyBagLoader::load(StorageID_t storageID, Inventory* pInventory)

{
    __BEGIN_TRY

    Statement* pStmt;

    BEGIN_DB {}
    END_DB(pStmt)

    __END_CATCH
}
