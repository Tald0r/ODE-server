//////////////////////////////////////////////////////////////////////////////
// Filename    : ComposMei.cpp
// Written By  : Elca
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "ComposMei.h"

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

ItemID_t ComposMei::m_ItemIDRegistry = 0;
Mutex ComposMei::m_Mutex;

//--------------------------------------------------------------------------------
// constructor
//--------------------------------------------------------------------------------
ComposMei::ComposMei()

//: m_ItemType(0)
{
    setItemType(0);
}

ComposMei::ComposMei(ItemType_t itemType, const list<OptionType_t>& optionType, ItemNum_t Num)

//: m_ItemType(itemType), m_Num(Num)
{
    setItemType(itemType);
    setNum(Num);
    if (!de::gameContext().itemInfos().isPossibleItem(getItemClass(), getItemType(), getOptionTypeList())) {
        filelog("itembug.log", "ComposMei::ComposMei() : Invalid item type or option type");
        throw Error("ComposMei::ComposMei() : Invalid item type or optionType");
    }
}


//--------------------------------------------------------------------------------
// create item
//--------------------------------------------------------------------------------
void ComposMei::create(const string& ownerID, Storage storage, StorageID_t storageID, BYTE x, BYTE y, ItemID_t itemID)

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

    defaultItemObjectRepository().insertNumOnlyItem(GEAR_COMPOS_MEI, m_ItemID, m_ObjectID, getItemType(), ownerID,
                                                    (int)storage, storageID, x, y, (int)getNum());

    __END_CATCH
}

//--------------------------------------------------------------------------------
// destroy
//--------------------------------------------------------------------------------
bool ComposMei::destroy()

{
    __BEGIN_TRY

    if (!defaultItemObjectRepository().destroyItemObject(GEAR_COMPOS_MEI, getObjectTableName(), m_ItemID))
        return false;

    __END_CATCH

    return true;
}


//--------------------------------------------------------------------------------
// save item
//--------------------------------------------------------------------------------
void ComposMei::tinysave(const char* field) const

{
    __BEGIN_TRY

    defaultItemObjectRepository().tinysaveGear(GEAR_COMPOS_MEI, field, m_ItemID);

    __END_CATCH
}

//--------------------------------------------------------------------------------
// save item
//--------------------------------------------------------------------------------
void ComposMei::save(const string& ownerID, Storage storage, StorageID_t storageID, BYTE x, BYTE y)

{
    __BEGIN_TRY

    defaultItemObjectRepository().updateNumOnlyItem(GEAR_COMPOS_MEI, m_ObjectID, getItemType(), ownerID, (int)storage,
                                                    storageID, (int)x, (int)y, (int)getNum(), m_ItemID);

    __END_CATCH
}


//--------------------------------------------------------------------------------
// get debug string
//--------------------------------------------------------------------------------
string ComposMei::toString() const

{
    StringStream msg;

    msg << "ComposMei(" << "ItemID:" << m_ItemID << ",ItemType:" << (int)getItemType() << ",Num:" << (int)getNum()
        << ")";

    return msg.toString();
}


int ComposMei::getHPAmount(void) const

{
    __BEGIN_TRY

    ComposMeiInfo* pInfo = dynamic_cast<ComposMeiInfo*>(
        de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_COMPOS_MEI, getItemType()));
    return pInfo->getHPAmount();

    __END_CATCH
}

int ComposMei::getMPAmount(void) const

{
    __BEGIN_TRY

    ComposMeiInfo* pInfo = dynamic_cast<ComposMeiInfo*>(
        de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_COMPOS_MEI, getItemType()));
    return pInfo->getMPAmount();

    __END_CATCH
}

int ComposMei::getHPDelay(void) const

{
    __BEGIN_TRY

    ComposMeiInfo* pInfo = dynamic_cast<ComposMeiInfo*>(
        de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_COMPOS_MEI, getItemType()));
    return pInfo->getHPDelay();

    __END_CATCH
}

int ComposMei::getMPDelay(void) const

{
    __BEGIN_TRY

    ComposMeiInfo* pInfo = dynamic_cast<ComposMeiInfo*>(
        de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_COMPOS_MEI, getItemType()));
    return pInfo->getMPDelay();

    __END_CATCH
}

int ComposMei::getHPQuantity(void) const

{
    __BEGIN_TRY

    ComposMeiInfo* pInfo = dynamic_cast<ComposMeiInfo*>(
        de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_COMPOS_MEI, getItemType()));
    return pInfo->getHPQuantity();

    __END_CATCH
}

int ComposMei::getMPQuantity(void) const

{
    __BEGIN_TRY

    ComposMeiInfo* pInfo = dynamic_cast<ComposMeiInfo*>(
        de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_COMPOS_MEI, getItemType()));
    return pInfo->getMPQuantity();

    __END_CATCH
}

int ComposMei::getHPRecoveryUnit(void) const

{
    __BEGIN_TRY

    ComposMeiInfo* pInfo = dynamic_cast<ComposMeiInfo*>(
        de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_COMPOS_MEI, getItemType()));
    return pInfo->getHPRecoveryUnit();

    __END_CATCH
}

int ComposMei::getMPRecoveryUnit(void) const

{
    __BEGIN_TRY

    ComposMeiInfo* pInfo = dynamic_cast<ComposMeiInfo*>(
        de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_COMPOS_MEI, getItemType()));
    return pInfo->getMPRecoveryUnit();

    __END_CATCH
}

//--------------------------------------------------------------------------------
// parse effect string
//--------------------------------------------------------------------------------
void ComposMeiInfo::parseEffect(const string& effect)

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
string ComposMeiInfo::toString() const

{
    StringStream msg;
    msg << "ComposMeiInfo(" << "ItemType:" << (int)m_ItemType << ",Name:" << m_Name << ",EName:" << m_EName
        << ",Price:" << (int)m_Price << ",VolumeType:" << Volume2String[m_VolumeType] << ",Weight:" << (int)m_Weight
        << ",Description:" << m_Description << ",HPAmount:" << (int)m_HPAmount << ",MPAmount:" << (int)m_MPAmount
        << ")";
    return msg.toString();
}


//--------------------------------------------------------------------------------
// load from DB
//--------------------------------------------------------------------------------
void ComposMeiInfoManager::load()

{
    __BEGIN_TRY

    m_InfoCount = defaultItemObjectRepository().loadMaxGearType(GEAR_COMPOS_MEI);

    m_pItemInfos = new ItemInfo*[m_InfoCount + 1];

    for (uint i = 0; i <= m_InfoCount; i++)
        m_pItemInfos[i] = NULL;

    vector<StringInfoRow> rows = defaultItemObjectRepository().loadStringInfos(GEAR_COMPOS_MEI);

    for (size_t r = 0; r < rows.size(); r++) {
        ComposMeiInfo* pComposMeiInfo = new ComposMeiInfo();

        pComposMeiInfo->setItemType(rows[r].basic.itemType);
        pComposMeiInfo->setName(rows[r].basic.name);
        pComposMeiInfo->setEName(rows[r].basic.ename);
        pComposMeiInfo->setPrice(rows[r].basic.price);
        pComposMeiInfo->setVolumeType(rows[r].basic.volume);
        pComposMeiInfo->setWeight(rows[r].basic.weight);
        pComposMeiInfo->setRatio(rows[r].basic.ratio);
        pComposMeiInfo->parseEffect(rows[r].value);

        addItemInfo(pComposMeiInfo);
    }

    __END_CATCH
}


//--------------------------------------------------------------------------------
// load to creature
//--------------------------------------------------------------------------------
void ComposMeiLoader::load(Creature* pCreature)

{
    __BEGIN_TRY

    Assert(pCreature != NULL);

    vector<NumOnlyObjectRow> rows =
        defaultItemObjectRepository().loadNumOnlyItemOfOwner(GEAR_COMPOS_MEI, pCreature->getName());

    for (size_t r = 0; r < rows.size(); r++) {
        try {
            ComposMei* pComposMei = new ComposMei();

            pComposMei->setItemID(rows[r].itemID);
            pComposMei->setObjectID(rows[r].objectID);
            pComposMei->setItemType(rows[r].itemType);

            Storage storage = (Storage)rows[r].storage;
            StorageID_t storageID = rows[r].storageID;
            BYTE x = rows[r].x;
            BYTE y = rows[r].y;

            pComposMei->setNum(rows[r].num);

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
                if (pInventory->canAddingEx(x, y, pComposMei)) {
                    pInventory->addItemEx(x, y, pComposMei);
                } else {
                    processItemBugEx(pCreature, pComposMei);
                }
                break;

            case STORAGE_GEAR:
                processItemBugEx(pCreature, pComposMei);
                break;

            case STORAGE_BELT:
                if (pCreature->isSlayer()) {
                    pItem = pSlayer->findBeltIID(storageID);
                    if (pItem != NULL && pItem->getItemClass() == Item::ITEM_CLASS_BELT) {
                        pBelt = dynamic_cast<Belt*>(pItem);
                        pBeltInventory = pBelt->getInventory();
                        if (pBeltInventory->canAddingEx(x, 0, pComposMei)) {
                            pBeltInventory->addItem(x, 0, pComposMei);
                        } else {
                            processItemBugEx(pCreature, pComposMei);
                        }
                    } else {
                        processItemBugEx(pCreature, pComposMei);
                    }
                } else if (pCreature->isVampire()) {
                    pItem = pVampire->findBeltIID(storageID);
                    if (pItem != NULL && pItem->getItemClass() == Item::ITEM_CLASS_BELT) {
                        pBelt = dynamic_cast<Belt*>(pItem);
                        pBeltInventory = pBelt->getInventory();
                        if (pBeltInventory->canAddingEx(x, 0, pComposMei)) {
                            pBeltInventory->addItemEx(x, 0, pComposMei);
                        } else {
                            processItemBugEx(pCreature, pComposMei);
                        }
                    } else {
                        processItemBugEx(pCreature, pComposMei);
                    }
                } else if (pCreature->isOusters()) {
                    pItem = findItemIID(pOusters, storageID, Item::ITEM_CLASS_OUSTERS_ARMSBAND);
                    if (pItem != NULL && pItem->getItemClass() == Item::ITEM_CLASS_OUSTERS_ARMSBAND) {
                        pOustersArmsband = dynamic_cast<OustersArmsband*>(pItem);
                        pArmsbandInventory = pOustersArmsband->getInventory();
                        if (pArmsbandInventory->canAddingEx(x, 0, pComposMei)) {
                            pArmsbandInventory->addItemEx(x, 0, pComposMei);
                        } else {
                            processItemBugEx(pCreature, pComposMei);
                        }
                    } else {
                        processItemBugEx(pCreature, pComposMei);
                    }
                }
                break;

            case STORAGE_EXTRASLOT:
                if (pCreature->isSlayer())
                    pSlayer->addItemToExtraInventorySlot(pComposMei);
                else if (pCreature->isVampire())
                    pVampire->addItemToExtraInventorySlot(pComposMei);
                else if (pCreature->isOusters())
                    pOusters->addItemToExtraInventorySlot(pComposMei);
                break;

            case STORAGE_MOTORCYCLE:
                processItemBugEx(pCreature, pComposMei);
                break;

            case STORAGE_STASH:
                if (pStash->isExist(x, y)) {
                    processItemBugEx(pCreature, pComposMei);
                } else
                    pStash->insert(x, y, pComposMei);
                break;

            case STORAGE_GARBAGE:
                processItemBug(pCreature, pComposMei);
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
void ComposMeiLoader::load(Zone* pZone)

{
    __BEGIN_TRY

    Assert(pZone != NULL);

    vector<NumOnlyZoneObjectRow> rows =
        defaultItemObjectRepository().loadNumOnlyItemInZone(GEAR_COMPOS_MEI, (int)STORAGE_ZONE, pZone->getZoneID());

    for (size_t r = 0; r < rows.size(); r++) {
        ComposMei* pComposMei = new ComposMei();

        pComposMei->setItemID(rows[r].itemID);
        pComposMei->setObjectID(rows[r].objectID);
        pComposMei->setItemType(rows[r].itemType);

        Storage storage = (Storage)rows[r].storage;
        BYTE x = rows[r].x;
        BYTE y = rows[r].y;

        pComposMei->setNum(rows[r].num);

        switch (storage) {
        case STORAGE_ZONE: {
            Tile& pTile = pZone->getTile(x, y);
            Assert(!pTile.hasItem());
            pTile.addItem(pComposMei);
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
void ComposMeiLoader::load(StorageID_t storageID, Inventory* pInventory)

{
    __BEGIN_TRY

    Statement* pStmt;

    BEGIN_DB {}
    END_DB(pStmt)

    __END_CATCH
}
