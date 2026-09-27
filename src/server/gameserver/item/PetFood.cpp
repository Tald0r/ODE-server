//////////////////////////////////////////////////////////////////////////////
// Filename    : PetFood.cpp
// Written By  : excel96
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "PetFood.h"

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

ItemID_t PetFood::m_ItemIDRegistry = 0;
Mutex PetFood::m_Mutex;

//////////////////////////////////////////////////////////////////////////////
// class PetFood member methods
//////////////////////////////////////////////////////////////////////////////

PetFood::PetFood()

{
    m_ItemType = 0;
    m_Num = 1;
}

PetFood::PetFood(ItemType_t itemType, const list<OptionType_t>& optionType, ItemNum_t Num)

{
    m_ItemType = itemType;
    m_Num = Num;

    if (!de::gameContext().itemInfos().isPossibleItem(getItemClass(), m_ItemType, optionType)) {
        filelog("itembug.log", "PetFood::PetFood() : Invalid item type or option type");
        throw Error("PetFood::PetFood() : Invalid item type or optionType");
    }
}

void PetFood::create(const string& ownerID, Storage storage, StorageID_t storageID, BYTE x, BYTE y, ItemID_t itemID)

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

    defaultItemObjectRepository().insertNumItem(GEAR_PET_FOOD, m_ItemID, m_ObjectID, m_ItemType, ownerID, (int)storage,
                                                storageID, (int)x, (int)y, (int)m_Num, (int)m_CreateType);

    __END_CATCH
}

//--------------------------------------------------------------------------------
// save item
//--------------------------------------------------------------------------------
void PetFood::tinysave(const char* field) const

{
    __BEGIN_TRY

    defaultItemObjectRepository().tinysaveGear(GEAR_PET_FOOD, field, m_ItemID);

    __END_CATCH
}

void PetFood::save(const string& ownerID, Storage storage, StorageID_t storageID, BYTE x, BYTE y)

{
    __BEGIN_TRY

    defaultItemObjectRepository().updateNumItem(GEAR_PET_FOOD, m_ObjectID, m_ItemType, ownerID, (int)storage, storageID,
                                                (int)x, (int)y, m_Num, m_ItemID);

    __END_CATCH
}

string PetFood::toString() const

{
    StringStream msg;

    msg << "PetFood(" << "ItemID:" << m_ItemID << ",ItemType:" << (int)m_ItemType << ")";

    return msg.toString();
}

VolumeWidth_t PetFood::getVolumeWidth() const

{
    __BEGIN_TRY

    return de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_PET_FOOD, m_ItemType)->getVolumeWidth();

    __END_CATCH
}

VolumeHeight_t PetFood::getVolumeHeight() const

{
    __BEGIN_TRY

    return de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_PET_FOOD, m_ItemType)->getVolumeHeight();

    __END_CATCH
}

Weight_t PetFood::getWeight() const

{
    __BEGIN_TRY

    return de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_PET_FOOD, m_ItemType)->getWeight();

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
// class PetFoodInfo member methods
//////////////////////////////////////////////////////////////////////////////

string PetFoodInfo::toString() const

{
    StringStream msg;
    msg << "PetFoodInfo(" << "ItemType:" << (int)m_ItemType << ",Name:" << m_Name << ",EName:" << m_EName
        << ",Price:" << (int)m_Price << ",VolumeType:" << Volume2String[m_VolumeType] << ",Weight:" << (int)m_Weight
        << ",Description:" << m_Description << ")";
    return msg.toString();
}

void PetFoodInfoManager::load()

{
    __BEGIN_TRY

    m_InfoCount = defaultItemObjectRepository().loadMaxGearType(GEAR_PET_FOOD);

    m_pItemInfos = new ItemInfo*[m_InfoCount + 1];

    for (uint i = 0; i <= m_InfoCount; i++)
        m_pItemInfos[i] = NULL;

    vector<IntTripleInfoRow> rows = defaultItemObjectRepository().loadIntTripleInfos(GEAR_PET_FOOD);

    for (size_t r = 0; r < rows.size(); r++) {
        PetFoodInfo* pPetFoodInfo = new PetFoodInfo();

        pPetFoodInfo->setItemType(rows[r].basic.itemType);
        pPetFoodInfo->setName(rows[r].basic.name);
        pPetFoodInfo->setEName(rows[r].basic.ename);
        pPetFoodInfo->setPrice(rows[r].basic.price);
        pPetFoodInfo->setVolumeType(rows[r].basic.volume);
        pPetFoodInfo->setWeight(rows[r].basic.weight);
        pPetFoodInfo->setRatio(rows[r].basic.ratio);
        pPetFoodInfo->setTarget(rows[r].first);
        pPetFoodInfo->setPetHP(rows[r].second);
        pPetFoodInfo->setTameRatio(rows[r].third);

        addItemInfo(pPetFoodInfo);
    }

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
// class PetFoodLoader member methods
//////////////////////////////////////////////////////////////////////////////

void PetFoodLoader::load(Creature* pCreature)

{
    __BEGIN_TRY

    Assert(pCreature != NULL);

    vector<NumIntObjectRow> rows =
        defaultItemObjectRepository().loadNumIntItemOfOwner(GEAR_PET_FOOD, pCreature->getName());

    for (size_t r = 0; r < rows.size(); r++) {
        try {
            PetFood* pPetFood = new PetFood();

            pPetFood->setItemID(rows[r].itemID);
            pPetFood->setObjectID(rows[r].objectID);
            pPetFood->setItemType(rows[r].itemType);

            Storage storage = (Storage)rows[r].storage;
            BYTE x = rows[r].x;
            BYTE y = rows[r].y;

            pPetFood->setNum(rows[r].num);
            pPetFood->setCreateType((Item::CreateType)rows[r].createType);

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
                if (pInventory->canAddingEx(x, y, pPetFood)) {
                    pInventory->addItemEx(x, y, pPetFood);
                } else {
                    processItemBugEx(pCreature, pPetFood);
                }
                break;

            case STORAGE_GEAR:
                processItemBugEx(pCreature, pPetFood);
                break;

            case STORAGE_BELT:
                processItemBugEx(pCreature, pPetFood);
                break;

            case STORAGE_EXTRASLOT:
                if (pCreature->isSlayer())
                    pSlayer->addItemToExtraInventorySlot(pPetFood);
                else if (pCreature->isVampire())
                    pVampire->addItemToExtraInventorySlot(pPetFood);
                else if (pCreature->isOusters())
                    pOusters->addItemToExtraInventorySlot(pPetFood);
                break;

            case STORAGE_MOTORCYCLE:
                processItemBugEx(pCreature, pPetFood);
                break;

            case STORAGE_STASH:
                if (pStash->isExist(x, y)) {
                    processItemBugEx(pCreature, pPetFood);
                } else
                    pStash->insert(x, y, pPetFood);
                break;

            case STORAGE_GARBAGE:
                processItemBug(pCreature, pPetFood);
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

void PetFoodLoader::load(Zone* pZone)

{
    __BEGIN_TRY

    Assert(pZone != NULL);

    vector<FlagZoneObjectRow> rows =
        defaultItemObjectRepository().loadFlagItemInZone(GEAR_PET_FOOD, (int)STORAGE_ZONE, pZone->getZoneID());

    for (size_t r = 0; r < rows.size(); r++) {
        PetFood* pPetFood = new PetFood();

        pPetFood->setItemID(rows[r].itemID);
        pPetFood->setObjectID(rows[r].objectID);
        pPetFood->setItemType(rows[r].itemType);

        Storage storage = (Storage)rows[r].storage;
        BYTE x = rows[r].x;
        BYTE y = rows[r].y;

        pPetFood->setCreateType((Item::CreateType)rows[r].createType);

        switch (storage) {
        case STORAGE_ZONE: {
            Tile& pTile = pZone->getTile(x, y);
            Assert(!pTile.hasItem());
            pTile.addItem(pPetFood);
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

void PetFoodLoader::load(StorageID_t storageID, Inventory* pInventory)

{
    __BEGIN_TRY

    Statement* pStmt;

    BEGIN_DB {}
    END_DB(pStmt)

    __END_CATCH
}
