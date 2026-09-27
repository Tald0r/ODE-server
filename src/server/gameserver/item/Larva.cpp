//////////////////////////////////////////////////////////////////////////////
// Filename    : Larva.cpp
// Written By  : Elca
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "Larva.h"

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
#include "ZoneGroupManager.h"
#include "repository/ItemObjectRepository.h"

ItemID_t Larva::m_ItemIDRegistry = 0;
Mutex Larva::m_Mutex;

//--------------------------------------------------------------------------------
// constructor
//--------------------------------------------------------------------------------
Larva::Larva()

    : m_ItemType(0) {}

Larva::Larva(ItemType_t itemType, const list<OptionType_t>& optionType, ItemNum_t Num)

    : m_ItemType(itemType), m_Num(Num) {
    if (!de::gameContext().itemInfos().isPossibleItem(getItemClass(), m_ItemType, optionType)) {
        filelog("itembug.log", "Larva::Larva() : Invalid item type or option type");
        throw Error("Larva::Larva() : Invalid item type or optionType");
    }
}


//--------------------------------------------------------------------------------
// create item
//--------------------------------------------------------------------------------
void Larva::create(const string& ownerID, Storage storage, StorageID_t storageID, BYTE x, BYTE y, ItemID_t itemID)

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

    defaultItemObjectRepository().insertNumOnlyItem(GEAR_LARVA, m_ItemID, m_ObjectID, m_ItemType, ownerID, (int)storage,
                                                    storageID, x, y, (int)m_Num);

    __END_CATCH
}

//--------------------------------------------------------------------------------
// destroy
//--------------------------------------------------------------------------------
bool Larva::destroy()

{
    __BEGIN_TRY

    if (!defaultItemObjectRepository().destroyItemObject(GEAR_LARVA, getObjectTableName(), m_ItemID))
        return false;

    __END_CATCH

    return true;
}


//--------------------------------------------------------------------------------
// save item
//--------------------------------------------------------------------------------
void Larva::tinysave(const char* field) const

{
    __BEGIN_TRY

    defaultItemObjectRepository().tinysaveGear(GEAR_LARVA, field, m_ItemID);

    __END_CATCH
}

//--------------------------------------------------------------------------------
// save item
//--------------------------------------------------------------------------------
void Larva::save(const string& ownerID, Storage storage, StorageID_t storageID, BYTE x, BYTE y)

{
    __BEGIN_TRY

    defaultItemObjectRepository().updateNumOnlyItem(GEAR_LARVA, m_ObjectID, m_ItemType, ownerID, (int)storage,
                                                    storageID, (int)x, (int)y, (int)m_Num, m_ItemID);

    __END_CATCH
}


//--------------------------------------------------------------------------------
// get debug string
//--------------------------------------------------------------------------------
string Larva::toString() const

{
    StringStream msg;

    msg << "Larva(" << "ItemID:" << m_ItemID << ",ItemType:" << (int)m_ItemType << ",Num:" << (int)m_Num << ")";

    return msg.toString();
}


//--------------------------------------------------------------------------------
// get width
//--------------------------------------------------------------------------------
VolumeWidth_t Larva::getVolumeWidth() const

{
    __BEGIN_TRY

    return de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_LARVA, m_ItemType)->getVolumeWidth();

    __END_CATCH
}


//--------------------------------------------------------------------------------
// get height
//--------------------------------------------------------------------------------
VolumeHeight_t Larva::getVolumeHeight() const

{
    __BEGIN_TRY

    return de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_LARVA, m_ItemType)->getVolumeHeight();

    __END_CATCH
}


//--------------------------------------------------------------------------------
// get weight
//--------------------------------------------------------------------------------
Weight_t Larva::getWeight() const

{
    __BEGIN_TRY

    return de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_LARVA, m_ItemType)->getWeight();

    __END_CATCH
}

int Larva::getHPAmount(void) const

{
    __BEGIN_TRY

    LarvaInfo* pInfo =
        dynamic_cast<LarvaInfo*>(de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_LARVA, m_ItemType));
    return pInfo->getHPAmount();

    __END_CATCH
}

int Larva::getMPAmount(void) const

{
    __BEGIN_TRY

    LarvaInfo* pInfo =
        dynamic_cast<LarvaInfo*>(de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_LARVA, m_ItemType));
    return pInfo->getMPAmount();

    __END_CATCH
}

int Larva::getHPDelay(void) const

{
    __BEGIN_TRY

    LarvaInfo* pInfo =
        dynamic_cast<LarvaInfo*>(de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_LARVA, m_ItemType));
    return pInfo->getHPDelay();

    __END_CATCH
}

int Larva::getMPDelay(void) const

{
    __BEGIN_TRY

    LarvaInfo* pInfo =
        dynamic_cast<LarvaInfo*>(de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_LARVA, m_ItemType));
    return pInfo->getMPDelay();

    __END_CATCH
}

int Larva::getHPQuantity(void) const

{
    __BEGIN_TRY

    LarvaInfo* pInfo =
        dynamic_cast<LarvaInfo*>(de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_LARVA, m_ItemType));
    return pInfo->getHPQuantity();

    __END_CATCH
}

int Larva::getMPQuantity(void) const

{
    __BEGIN_TRY

    LarvaInfo* pInfo =
        dynamic_cast<LarvaInfo*>(de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_LARVA, m_ItemType));
    return pInfo->getMPQuantity();

    __END_CATCH
}

int Larva::getHPRecoveryUnit(void) const

{
    __BEGIN_TRY

    LarvaInfo* pInfo =
        dynamic_cast<LarvaInfo*>(de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_LARVA, m_ItemType));
    return pInfo->getHPRecoveryUnit();

    __END_CATCH
}

int Larva::getMPRecoveryUnit(void) const

{
    __BEGIN_TRY

    LarvaInfo* pInfo =
        dynamic_cast<LarvaInfo*>(de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_LARVA, m_ItemType));
    return pInfo->getMPRecoveryUnit();

    __END_CATCH
}

//--------------------------------------------------------------------------------
// parse effect string
//--------------------------------------------------------------------------------
void LarvaInfo::parseEffect(const string& effect)

{
    __BEGIN_TRY

    m_HPAmount = 0;
    m_HPDelay = 0;
    m_HPRecoveryUnit = 0;
    m_MPAmount = 0;
    m_MPDelay = 0;
    m_MPRecoveryUnit = 0;

    if (effect.size() < 5)
        return;

    size_t a = 0, b = 0, c = 0, d = 0, e = 0;

    while (e < effect.size() - 1) {
        ////////////////////////////////////////////////////////////
        //(HP,+50,2,1)(MP+10)
        // a  b   ca
        ////////////////////////////////////////////////////////////
        a = effect.find_first_of('(', e);
        b = effect.find_first_of(',', a + 1);
        c = effect.find_first_of(',', b + 1);
        d = effect.find_first_of(',', c + 1);
        e = effect.find_first_of(')', d + 1);

        if (a > b || b > c || c > d || d > e)
            break;

        string recover = trim(effect.substr(a + 1, b - a - 1));
        uint amount = atoi(effect.substr(b + 1, c - b - 1).c_str());
        uint delay = atoi(effect.substr(c + 1, d - c - 1).c_str());
        uint unit = atoi(effect.substr(d + 1, e - d - 1).c_str());

        if (recover == "HP") {
            m_HPAmount = (int)amount;
            m_HPDelay = (int)delay;
            m_HPRecoveryUnit = (int)unit;
        } else if (recover == "MP") {
            m_MPAmount = (int)amount;
            m_MPDelay = (int)delay;
            m_MPRecoveryUnit = (int)unit;
        }
    }

    __END_CATCH
}

//--------------------------------------------------------------------------------
// get debug string
//--------------------------------------------------------------------------------
string LarvaInfo::toString() const

{
    StringStream msg;
    msg << "LarvaInfo(" << "ItemType:" << (int)m_ItemType << ",Name:" << m_Name << ",EName:" << m_EName
        << ",Price:" << (int)m_Price << ",VolumeType:" << Volume2String[m_VolumeType] << ",Weight:" << (int)m_Weight
        << ",Description:" << m_Description << ",HPAmount:" << (int)m_HPAmount << ",MPAmount:" << (int)m_MPAmount
        << ")";
    return msg.toString();
}


//--------------------------------------------------------------------------------
// load from DB
//--------------------------------------------------------------------------------
void LarvaInfoManager::load()

{
    __BEGIN_TRY

    m_InfoCount = defaultItemObjectRepository().loadMaxGearType(GEAR_LARVA);

    m_pItemInfos = new ItemInfo*[m_InfoCount + 1];

    for (uint i = 0; i <= m_InfoCount; i++)
        m_pItemInfos[i] = NULL;

    vector<StringInfoRow> rows = defaultItemObjectRepository().loadStringInfos(GEAR_LARVA);

    for (size_t r = 0; r < rows.size(); r++) {
        LarvaInfo* pLarvaInfo = new LarvaInfo();

        pLarvaInfo->setItemType(rows[r].basic.itemType);
        pLarvaInfo->setName(rows[r].basic.name);
        pLarvaInfo->setEName(rows[r].basic.ename);
        pLarvaInfo->setPrice(rows[r].basic.price);
        pLarvaInfo->setVolumeType(rows[r].basic.volume);
        pLarvaInfo->setWeight(rows[r].basic.weight);
        pLarvaInfo->setRatio(rows[r].basic.ratio);
        pLarvaInfo->parseEffect(rows[r].value);

        addItemInfo(pLarvaInfo);
    }

    __END_CATCH
}


//--------------------------------------------------------------------------------
// load to creature
//--------------------------------------------------------------------------------
void LarvaLoader::load(Creature* pCreature)

{
    __BEGIN_TRY

    Assert(pCreature != NULL);

    vector<NumOnlyObjectRow> rows =
        defaultItemObjectRepository().loadNumOnlyItemOfOwner(GEAR_LARVA, pCreature->getName());

    for (size_t r = 0; r < rows.size(); r++) {
        try {
            Larva* pLarva = new Larva();

            pLarva->setItemID(rows[r].itemID);
            pLarva->setObjectID(rows[r].objectID);
            pLarva->setItemType(rows[r].itemType);

            Storage storage = (Storage)rows[r].storage;
            StorageID_t storageID = rows[r].storageID;
            BYTE x = rows[r].x;
            BYTE y = rows[r].y;

            pLarva->setNum(rows[r].num);

            Inventory* pInventory = NULL;
            Slayer* pSlayer = NULL;
            Vampire* pVampire = NULL;
            Ousters* pOusters = NULL;
            Item* pItem = NULL;
            Stash* pStash = NULL;
            Belt* pBelt = NULL;
            Inventory* pBeltInventory = NULL;

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
                if (pInventory->canAddingEx(x, y, pLarva)) {
                    pInventory->addItemEx(x, y, pLarva);
                } else {
                    processItemBugEx(pCreature, pLarva);
                }
                break;

            case STORAGE_GEAR:
                processItemBugEx(pCreature, pLarva);
                break;

            case STORAGE_BELT:
                if (pCreature->isSlayer()) {
                    pItem = pSlayer->findBeltIID(storageID);
                    if (pItem != NULL && pItem->getItemClass() == Item::ITEM_CLASS_BELT) {
                        pBelt = dynamic_cast<Belt*>(pItem);
                        pBeltInventory = pBelt->getInventory();
                        if (pBeltInventory->canAddingEx(x, 0, pLarva)) {
                            pBeltInventory->addItem(x, 0, pLarva);
                        } else {
                            processItemBugEx(pCreature, pLarva);
                        }
                    } else {
                        processItemBugEx(pCreature, pLarva);
                    }
                } else if (pCreature->isVampire()) {
                    pItem = pVampire->findBeltIID(storageID);
                    if (pItem != NULL && pItem->getItemClass() == Item::ITEM_CLASS_BELT) {
                        pBelt = dynamic_cast<Belt*>(pItem);
                        pBeltInventory = pBelt->getInventory();
                        if (pBeltInventory->canAddingEx(x, 0, pLarva)) {
                            pBeltInventory->addItemEx(x, 0, pLarva);
                        } else {
                            processItemBugEx(pCreature, pLarva);
                        }
                    } else {
                        processItemBugEx(pCreature, pLarva);
                    }
                } else if (pCreature->isOusters()) {
                    pItem = pOusters->findBeltIID(storageID);
                    if (pItem != NULL && pItem->getItemClass() == Item::ITEM_CLASS_BELT) {
                        pBelt = dynamic_cast<Belt*>(pItem);
                        pBeltInventory = pBelt->getInventory();
                        if (pBeltInventory->canAddingEx(x, 0, pLarva)) {
                            pBeltInventory->addItemEx(x, 0, pLarva);
                        } else {
                            processItemBugEx(pCreature, pLarva);
                        }
                    } else {
                        processItemBugEx(pCreature, pLarva);
                    }
                }
                break;

            case STORAGE_EXTRASLOT:
                if (pCreature->isSlayer())
                    pSlayer->addItemToExtraInventorySlot(pLarva);
                else if (pCreature->isVampire())
                    pVampire->addItemToExtraInventorySlot(pLarva);
                else if (pCreature->isOusters())
                    pOusters->addItemToExtraInventorySlot(pLarva);
                break;

            case STORAGE_MOTORCYCLE:
                processItemBugEx(pCreature, pLarva);
                break;

            case STORAGE_STASH:
                if (pStash->isExist(x, y)) {
                    processItemBugEx(pCreature, pLarva);
                } else
                    pStash->insert(x, y, pLarva);
                break;

            case STORAGE_GARBAGE:
                processItemBug(pCreature, pLarva);
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
void LarvaLoader::load(Zone* pZone)

{
    __BEGIN_TRY

    Assert(pZone != NULL);

    vector<NumOnlyZoneObjectRow> rows =
        defaultItemObjectRepository().loadNumOnlyItemInZone(GEAR_LARVA, (int)STORAGE_ZONE, pZone->getZoneID());

    for (size_t r = 0; r < rows.size(); r++) {
        Larva* pLarva = new Larva();

        pLarva->setItemID(rows[r].itemID);
        pLarva->setObjectID(rows[r].objectID);
        pLarva->setItemType(rows[r].itemType);

        Storage storage = (Storage)rows[r].storage;
        BYTE x = rows[r].x;
        BYTE y = rows[r].y;

        pLarva->setNum(rows[r].num);

        switch (storage) {
        case STORAGE_ZONE: {
            Tile& pTile = pZone->getTile(x, y);
            Assert(!pTile.hasItem());
            pTile.addItem(pLarva);
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
void LarvaLoader::load(StorageID_t storageID, Inventory* pInventory)

{
    __BEGIN_TRY

    Statement* pStmt;

    BEGIN_DB {}
    END_DB(pStmt)

    __END_CATCH
}
