//////////////////////////////////////////////////////////////////////////////
// Filename    : CoreZap.cpp
// Written By  : Elca
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "CoreZap.h"

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

ItemID_t CoreZap::m_ItemIDRegistry = 0;
Mutex CoreZap::m_Mutex;

//--------------------------------------------------------------------------------
// constructor
//--------------------------------------------------------------------------------
CoreZap::CoreZap()

//: m_ItemType(0), m_Durability(0)
{
    setItemType(0);
}

CoreZap::CoreZap(ItemType_t itemType, const list<OptionType_t>& optionType)

{
    setItemType(itemType);
    setOptionType(optionType);

    if (!de::gameContext().itemInfos().isPossibleItem(getItemClass(), getItemType(), getOptionTypeList())) {
        filelog("itembug.log", "CoreZap::CoreZap() : Invalid item type or option type");
        throw Error("CoreZap::CoreZap() : Invalid item type or optionType");
    }
}


//--------------------------------------------------------------------------------
// create item
//--------------------------------------------------------------------------------
void CoreZap::create(const string& ownerID, Storage storage, StorageID_t storageID, BYTE x, BYTE y, ItemID_t itemID)

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

    string optionField;
    setOptionTypeToField(getOptionTypeList(), optionField);

    defaultItemObjectRepository().insertOptionGradeItem(GEAR_CORE_ZAP, m_ItemID, m_ObjectID, getItemType(), ownerID,
                                                        (int)storage, storageID, (int)x, (int)y, optionField,
                                                        getGrade(), (int)m_CreateType);

    __END_CATCH
}


//--------------------------------------------------------------------------------
// save item
//--------------------------------------------------------------------------------
void CoreZap::tinysave(const char* field) const

{
    __BEGIN_TRY

    defaultItemObjectRepository().tinysaveGear(GEAR_CORE_ZAP, field, m_ItemID);

    __END_CATCH
}

//--------------------------------------------------------------------------------
// save item
//--------------------------------------------------------------------------------
void CoreZap::save(const string& ownerID, Storage storage, StorageID_t storageID, BYTE x, BYTE y)

{
    __BEGIN_TRY

    string optionField;
    setOptionTypeToField(getOptionTypeList(), optionField);

    defaultItemObjectRepository().updateCoreZap(GEAR_CORE_ZAP, m_ObjectID, getItemType(), ownerID, (int)storage,
                                                storageID, (int)x, (int)y, optionField, getGrade(), m_ItemID);

    __END_CATCH
}


//--------------------------------------------------------------------------------
// get debug string
//--------------------------------------------------------------------------------
string CoreZap::toString() const

{
    StringStream msg;

    msg << "CoreZap(" << "ItemID:" << m_ItemID << ",ItemType:" << (int)getItemType()
        << ",OptionType:" << getOptionTypeToString(getOptionTypeList()).c_str() << ")";

    return msg.toString();
}


//--------------------------------------------------------------------------------
// get debug string
//--------------------------------------------------------------------------------
string CoreZapInfo::toString() const

{
    StringStream msg;

    msg << "CoreZapInfo(" << "ItemType:" << m_ItemType << ",Name:" << m_Name << ",EName:" << m_EName
        << ",Price:" << m_Price << ",VolumeType:" << Volume2String[m_VolumeType] << ",Weight:" << m_Weight
        << ",Description:" << m_Description << ")";

    return msg.toString();
}


//--------------------------------------------------------------------------------
// load from DB
//--------------------------------------------------------------------------------
void CoreZapInfoManager::load()

{
    __BEGIN_TRY

    m_InfoCount = defaultItemObjectRepository().loadMaxGearType(GEAR_CORE_ZAP);

    m_pItemInfos = new ItemInfo*[m_InfoCount + 1];

    for (uint i = 0; i <= m_InfoCount; i++)
        m_pItemInfos[i] = NULL;

    vector<IntInfoRow> rows = defaultItemObjectRepository().loadIntInfos(GEAR_CORE_ZAP);

    for (size_t r = 0; r < rows.size(); r++) {
        CoreZapInfo* pCoreZapInfo = new CoreZapInfo();

        pCoreZapInfo->setItemType(rows[r].basic.itemType);
        pCoreZapInfo->setName(rows[r].basic.name);
        pCoreZapInfo->setEName(rows[r].basic.ename);
        pCoreZapInfo->setPrice(rows[r].basic.price);
        pCoreZapInfo->setVolumeType(rows[r].basic.volume);
        pCoreZapInfo->setWeight(rows[r].basic.weight);
        pCoreZapInfo->setRatio(rows[r].basic.ratio);
        pCoreZapInfo->setOptionClass((OptionClass)rows[r].value);

        addItemInfo(pCoreZapInfo);
    }

    __END_CATCH
}


//--------------------------------------------------------------------------------
// load to creature
//--------------------------------------------------------------------------------
void CoreZapLoader::load(Creature* pCreature)

{
    __BEGIN_TRY

    Assert(pCreature != NULL);

    vector<CoreZapObjectRow> rows =
        defaultItemObjectRepository().loadCoreZapOfOwner(GEAR_CORE_ZAP, pCreature->getName());

    for (size_t r = 0; r < rows.size(); r++) {
        try {
            CoreZap* pCoreZap = new CoreZap();

            pCoreZap->setItemID(rows[r].itemID);
            pCoreZap->setObjectID(rows[r].objectID);
            pCoreZap->setItemType(rows[r].itemType);

            if (de::gameContext()
                    .itemInfos()
                    .getItemInfo(Item::ITEM_CLASS_CORE_ZAP, pCoreZap->getItemType())
                    ->isUnique())
                pCoreZap->setUnique();

            Storage storage = (Storage)rows[r].storage;
            BYTE x = rows[r].x;
            BYTE y = rows[r].y;

            string optionField = rows[r].optionField;
            list<OptionType_t> optionTypes;
            setOptionTypeFromField(optionTypes, optionField);
            pCoreZap->setOptionType(optionTypes);

            pCoreZap->setGrade(rows[r].grade);
            pCoreZap->setCreateType((Item::CreateType)rows[r].createType);

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
                if (pInventory->canAddingEx(x, y, pCoreZap)) {
                    pInventory->addItemEx(x, y, pCoreZap);
                } else {
                    processItemBugEx(pCreature, pCoreZap);
                }
                break;

            case STORAGE_GEAR:
                if (pCreature->isSlayer()) {
                    if (!pSlayer->isWear((Slayer::WearPart)x)) {
                        pSlayer->wearItem((Slayer::WearPart)x, pCoreZap);
                    } else {
                        processItemBugEx(pCreature, pCoreZap);
                    }
                } else if (pCreature->isVampire()) {
                    if (!pVampire->isWear((Vampire::WearPart)x)) {
                        pVampire->wearItem((Vampire::WearPart)x, pCoreZap);
                    } else {
                        processItemBugEx(pCreature, pCoreZap);
                    }
                } else if (pCreature->isOusters()) {
                    if (!pOusters->isWear((Ousters::WearPart)x)) {
                        pOusters->wearItem((Ousters::WearPart)x, pCoreZap);
                    } else {
                        processItemBugEx(pCreature, pCoreZap);
                    }
                }
                break;

            case STORAGE_BELT:
                processItemBugEx(pCreature, pCoreZap);
                break;

            case STORAGE_EXTRASLOT:
                if (pCreature->isSlayer())
                    pSlayer->addItemToExtraInventorySlot(pCoreZap);
                else if (pCreature->isVampire())
                    pVampire->addItemToExtraInventorySlot(pCoreZap);
                else if (pCreature->isOusters())
                    pOusters->addItemToExtraInventorySlot(pCoreZap);
                break;

            case STORAGE_MOTORCYCLE:
                processItemBugEx(pCreature, pCoreZap);
                break;

            case STORAGE_STASH:
                if (pStash->isExist(x, y)) {
                    processItemBugEx(pCreature, pCoreZap);
                } else
                    pStash->insert(x, y, pCoreZap);
                break;

            case STORAGE_GARBAGE:
                processItemBug(pCreature, pCoreZap);
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
void CoreZapLoader::load(Zone* pZone)

{
    __BEGIN_TRY

    Assert(pZone != NULL);

    vector<CoreZapZoneObjectRow> rows =
        defaultItemObjectRepository().loadCoreZapInZone(GEAR_CORE_ZAP, (int)STORAGE_ZONE, pZone->getZoneID());

    for (size_t r = 0; r < rows.size(); r++) {
        CoreZap* pCoreZap = new CoreZap();

        pCoreZap->setItemID(rows[r].itemID);
        pCoreZap->setObjectID(rows[r].objectID);
        pCoreZap->setItemType(rows[r].itemType);

        Storage storage = (Storage)rows[r].storage;
        BYTE x = rows[r].x;
        BYTE y = rows[r].y;

        string optionField = rows[r].optionField;
        list<OptionType_t> optionTypes;
        setOptionTypeFromField(optionTypes, optionField);
        pCoreZap->setOptionType(optionTypes);

        pCoreZap->setCreateType((Item::CreateType)rows[r].createType);

        switch (storage) {
        case STORAGE_ZONE: {
            Tile& pTile = pZone->getTile(x, y);
            Assert(!pTile.hasItem());
            pTile.addItem(pCoreZap);
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
void CoreZapLoader::load(StorageID_t storageID, Inventory* pInventory)

{
    __BEGIN_TRY

    Statement* pStmt;

    BEGIN_DB {}
    END_DB(pStmt)

    __END_CATCH
}
