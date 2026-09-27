//////////////////////////////////////////////////////////////////////////////
// Filename    : Pupa.cpp
// Written By  : Elca
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "Pupa.h"

#include "Belt.h"
#include "DB.h"
#include "GameContext.h"
#include "ItemInfoManager.h"
#include "ItemUtil.h"
#include "Motorcycle.h"
#include "Ousters.h"
#include "OustersArmsband.h"
#include "Slayer.h"
#include "Stash.h"
#include "Utility.h"
#include "Vampire.h"
#include "ZoneGroupManager.h"
#include "repository/ItemObjectRepository.h"

ItemID_t Pupa::m_ItemIDRegistry = 0;
Mutex Pupa::m_Mutex;

//--------------------------------------------------------------------------------
// constructor
//--------------------------------------------------------------------------------
Pupa::Pupa()

    : m_ItemType(0) {}

Pupa::Pupa(ItemType_t itemType, const list<OptionType_t>& optionType, ItemNum_t Num)

    : m_ItemType(itemType), m_Num(Num) {
    if (!de::gameContext().itemInfos().isPossibleItem(getItemClass(), m_ItemType, optionType)) {
        filelog("itembug.log", "Pupa::Pupa() : Invalid item type or option type");
        throw Error("Pupa::Pupa() : Invalid item type or optionType");
    }
}


//--------------------------------------------------------------------------------
// create item
//--------------------------------------------------------------------------------
void Pupa::create(const string& ownerID, Storage storage, StorageID_t storageID, BYTE x, BYTE y, ItemID_t itemID)

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

    defaultItemObjectRepository().insertNumOnlyItem(GEAR_PUPA, m_ItemID, m_ObjectID, m_ItemType, ownerID, (int)storage,
                                                    storageID, x, y, (int)m_Num);

    __END_CATCH
}

//--------------------------------------------------------------------------------
// destroy
//--------------------------------------------------------------------------------
bool Pupa::destroy()

{
    __BEGIN_TRY

    if (!defaultItemObjectRepository().destroyItemObject(GEAR_PUPA, getObjectTableName(), m_ItemID))
        return false;

    __END_CATCH

    return true;
}


//--------------------------------------------------------------------------------
// save item
//--------------------------------------------------------------------------------
void Pupa::tinysave(const char* field) const

{
    __BEGIN_TRY

    defaultItemObjectRepository().tinysaveGear(GEAR_PUPA, field, m_ItemID);

    __END_CATCH
}

//--------------------------------------------------------------------------------
// save item
//--------------------------------------------------------------------------------
void Pupa::save(const string& ownerID, Storage storage, StorageID_t storageID, BYTE x, BYTE y)

{
    __BEGIN_TRY

    defaultItemObjectRepository().updateNumOnlyItem(GEAR_PUPA, m_ObjectID, m_ItemType, ownerID, (int)storage, storageID,
                                                    (int)x, (int)y, (int)m_Num, m_ItemID);

    __END_CATCH
}


//--------------------------------------------------------------------------------
// get debug string
//--------------------------------------------------------------------------------
string Pupa::toString() const

{
    StringStream msg;

    msg << "Pupa(" << "ItemID:" << m_ItemID << ",ItemType:" << (int)m_ItemType << ",Num:" << (int)m_Num << ")";

    return msg.toString();
}


//--------------------------------------------------------------------------------
// get width
//--------------------------------------------------------------------------------
VolumeWidth_t Pupa::getVolumeWidth() const

{
    __BEGIN_TRY

    return de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_PUPA, m_ItemType)->getVolumeWidth();

    __END_CATCH
}


//--------------------------------------------------------------------------------
// get height
//--------------------------------------------------------------------------------
VolumeHeight_t Pupa::getVolumeHeight() const

{
    __BEGIN_TRY

    return de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_PUPA, m_ItemType)->getVolumeHeight();

    __END_CATCH
}


//--------------------------------------------------------------------------------
// get weight
//--------------------------------------------------------------------------------
Weight_t Pupa::getWeight() const

{
    __BEGIN_TRY

    return de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_PUPA, m_ItemType)->getWeight();

    __END_CATCH
}

int Pupa::getHPAmount(void) const

{
    __BEGIN_TRY

    PupaInfo* pInfo =
        dynamic_cast<PupaInfo*>(de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_PUPA, m_ItemType));
    return pInfo->getHPAmount();

    __END_CATCH
}

int Pupa::getMPAmount(void) const

{
    __BEGIN_TRY

    PupaInfo* pInfo =
        dynamic_cast<PupaInfo*>(de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_PUPA, m_ItemType));
    return pInfo->getMPAmount();

    __END_CATCH
}

int Pupa::getHPDelay(void) const

{
    __BEGIN_TRY

    PupaInfo* pInfo =
        dynamic_cast<PupaInfo*>(de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_PUPA, m_ItemType));
    return pInfo->getHPDelay();

    __END_CATCH
}

int Pupa::getMPDelay(void) const

{
    __BEGIN_TRY

    PupaInfo* pInfo =
        dynamic_cast<PupaInfo*>(de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_PUPA, m_ItemType));
    return pInfo->getMPDelay();

    __END_CATCH
}

int Pupa::getHPQuantity(void) const

{
    __BEGIN_TRY

    PupaInfo* pInfo =
        dynamic_cast<PupaInfo*>(de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_PUPA, m_ItemType));
    return pInfo->getHPQuantity();

    __END_CATCH
}

int Pupa::getMPQuantity(void) const

{
    __BEGIN_TRY

    PupaInfo* pInfo =
        dynamic_cast<PupaInfo*>(de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_PUPA, m_ItemType));
    return pInfo->getMPQuantity();

    __END_CATCH
}

int Pupa::getHPRecoveryUnit(void) const

{
    __BEGIN_TRY

    PupaInfo* pInfo =
        dynamic_cast<PupaInfo*>(de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_PUPA, m_ItemType));
    return pInfo->getHPRecoveryUnit();

    __END_CATCH
}

int Pupa::getMPRecoveryUnit(void) const

{
    __BEGIN_TRY

    PupaInfo* pInfo =
        dynamic_cast<PupaInfo*>(de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_PUPA, m_ItemType));
    return pInfo->getMPRecoveryUnit();

    __END_CATCH
}

//--------------------------------------------------------------------------------
// parse effect string
//--------------------------------------------------------------------------------
void PupaInfo::parseEffect(const string& effect)

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
string PupaInfo::toString() const

{
    StringStream msg;
    msg << "PupaInfo(" << "ItemType:" << (int)m_ItemType << ",Name:" << m_Name << ",EName:" << m_EName
        << ",Price:" << (int)m_Price << ",VolumeType:" << Volume2String[m_VolumeType] << ",Weight:" << (int)m_Weight
        << ",Description:" << m_Description << ",HPAmount:" << (int)m_HPAmount << ",MPAmount:" << (int)m_MPAmount
        << ")";
    return msg.toString();
}


//--------------------------------------------------------------------------------
// load from DB
//--------------------------------------------------------------------------------
void PupaInfoManager::load()

{
    __BEGIN_TRY

    m_InfoCount = defaultItemObjectRepository().loadMaxGearType(GEAR_PUPA);

    m_pItemInfos = new ItemInfo*[m_InfoCount + 1];

    for (uint i = 0; i <= m_InfoCount; i++)
        m_pItemInfos[i] = NULL;

    vector<StringInfoRow> rows = defaultItemObjectRepository().loadStringInfos(GEAR_PUPA);

    for (size_t r = 0; r < rows.size(); r++) {
        PupaInfo* pPupaInfo = new PupaInfo();

        pPupaInfo->setItemType(rows[r].basic.itemType);
        pPupaInfo->setName(rows[r].basic.name);
        pPupaInfo->setEName(rows[r].basic.ename);
        pPupaInfo->setPrice(rows[r].basic.price);
        pPupaInfo->setVolumeType(rows[r].basic.volume);
        pPupaInfo->setWeight(rows[r].basic.weight);
        pPupaInfo->setRatio(rows[r].basic.ratio);
        pPupaInfo->parseEffect(rows[r].value);

        addItemInfo(pPupaInfo);
    }

    __END_CATCH
}


//--------------------------------------------------------------------------------
// load to creature
//--------------------------------------------------------------------------------
void PupaLoader::load(Creature* pCreature)

{
    __BEGIN_TRY

    Assert(pCreature != NULL);

    vector<NumOnlyObjectRow> rows =
        defaultItemObjectRepository().loadNumOnlyItemOfOwner(GEAR_PUPA, pCreature->getName());

    for (size_t r = 0; r < rows.size(); r++) {
        try {
            Pupa* pPupa = new Pupa();

            pPupa->setItemID(rows[r].itemID);
            pPupa->setObjectID(rows[r].objectID);
            pPupa->setItemType(rows[r].itemType);

            Storage storage = (Storage)rows[r].storage;
            StorageID_t storageID = rows[r].storageID;
            BYTE x = rows[r].x;
            BYTE y = rows[r].y;

            pPupa->setNum(rows[r].num);

            Inventory* pInventory = NULL;
            Slayer* pSlayer = NULL;
            Vampire* pVampire = NULL;
            Ousters* pOusters = NULL;
            Item* pItem = NULL;
            Stash* pStash = NULL;
            Belt* pBelt = NULL;
            Inventory* pBeltInventory = NULL;

            OustersArmsband* pOustersArmsband = NULL;
            Inventory* pArmsbandInventory = NULL;

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
                if (pInventory->canAddingEx(x, y, pPupa)) {
                    pInventory->addItemEx(x, y, pPupa);
                } else {
                    processItemBugEx(pCreature, pPupa);
                }
                break;

            case STORAGE_GEAR:
                processItemBugEx(pCreature, pPupa);
                break;

            case STORAGE_BELT:
                if (pCreature->isSlayer()) {
                    pItem = pSlayer->findBeltIID(storageID);
                    if (pItem != NULL && pItem->getItemClass() == Item::ITEM_CLASS_BELT) {
                        pBelt = dynamic_cast<Belt*>(pItem);
                        pBeltInventory = pBelt->getInventory();
                        if (pBeltInventory->canAddingEx(x, 0, pPupa)) {
                            pBeltInventory->addItem(x, 0, pPupa);
                        } else {
                            processItemBugEx(pCreature, pPupa);
                        }
                    } else {
                        processItemBugEx(pCreature, pPupa);
                    }
                } else if (pCreature->isVampire()) {
                    pItem = pVampire->findBeltIID(storageID);
                    if (pItem != NULL && pItem->getItemClass() == Item::ITEM_CLASS_BELT) {
                        pBelt = dynamic_cast<Belt*>(pItem);
                        pBeltInventory = pBelt->getInventory();
                        if (pBeltInventory->canAddingEx(x, 0, pPupa)) {
                            pBeltInventory->addItemEx(x, 0, pPupa);
                        } else {
                            processItemBugEx(pCreature, pPupa);
                        }
                    } else {
                        processItemBugEx(pCreature, pPupa);
                    }
                } else if (pCreature->isOusters()) {
                    pItem = findItemIID(pOusters, storageID, Item::ITEM_CLASS_OUSTERS_ARMSBAND);
                    if (pItem != NULL && pItem->getItemClass() == Item::ITEM_CLASS_OUSTERS_ARMSBAND) {
                        pOustersArmsband = dynamic_cast<OustersArmsband*>(pItem);
                        pArmsbandInventory = pOustersArmsband->getInventory();
                        if (pArmsbandInventory->canAddingEx(x, 0, pPupa)) {
                            pArmsbandInventory->addItemEx(x, 0, pPupa);
                        } else {
                            processItemBugEx(pCreature, pPupa);
                        }
                    } else {
                        processItemBugEx(pCreature, pPupa);
                    }
                }
                break;

            case STORAGE_EXTRASLOT:
                if (pCreature->isSlayer())
                    pSlayer->addItemToExtraInventorySlot(pPupa);
                else if (pCreature->isVampire())
                    pVampire->addItemToExtraInventorySlot(pPupa);
                else if (pCreature->isOusters())
                    pOusters->addItemToExtraInventorySlot(pPupa);
                break;

            case STORAGE_MOTORCYCLE:
                processItemBugEx(pCreature, pPupa);
                break;

            case STORAGE_STASH:
                if (pStash->isExist(x, y)) {
                    processItemBugEx(pCreature, pPupa);
                } else
                    pStash->insert(x, y, pPupa);
                break;

            case STORAGE_GARBAGE:
                processItemBug(pCreature, pPupa);
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
void PupaLoader::load(Zone* pZone)

{
    __BEGIN_TRY

    Assert(pZone != NULL);

    vector<NumOnlyZoneObjectRow> rows =
        defaultItemObjectRepository().loadNumOnlyItemInZone(GEAR_PUPA, (int)STORAGE_ZONE, pZone->getZoneID());

    for (size_t r = 0; r < rows.size(); r++) {
        Pupa* pPupa = new Pupa();

        pPupa->setItemID(rows[r].itemID);
        pPupa->setObjectID(rows[r].objectID);
        pPupa->setItemType(rows[r].itemType);

        Storage storage = (Storage)rows[r].storage;
        BYTE x = rows[r].x;
        BYTE y = rows[r].y;

        pPupa->setNum(rows[r].num);

        switch (storage) {
        case STORAGE_ZONE: {
            Tile& pTile = pZone->getTile(x, y);
            Assert(!pTile.hasItem());
            pTile.addItem(pPupa);
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
void PupaLoader::load(StorageID_t storageID, Inventory* pInventory)

{
    __BEGIN_TRY

    Statement* pStmt;

    BEGIN_DB {}
    END_DB(pStmt)

    __END_CATCH
}
