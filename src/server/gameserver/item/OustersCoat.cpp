//////////////////////////////////////////////////////////////////////////////
// Filename    : OustersCoat.cpp
// Written By  : Elca
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "OustersCoat.h"

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

ItemID_t OustersCoat::m_ItemIDRegistry = 0;
Mutex OustersCoat::m_Mutex;

//--------------------------------------------------------------------------------
// constructor
//--------------------------------------------------------------------------------
OustersCoat::OustersCoat()

//: m_ItemType(0), m_Durability(0)
{
    setItemType(0);
    setDurability(0);
}

OustersCoat::OustersCoat(ItemType_t itemType, const list<OptionType_t>& optionType)

//: m_ItemType(itemType), m_OptionType(optionType), m_Durability(0)
{
    setItemType(itemType);
    setOptionType(optionType);

    setDurability(computeMaxDurability(this));

    if (!de::gameContext().itemInfos().isPossibleItem(getItemClass(), getItemType(), getOptionTypeList())) {
        filelog("itembug.log", "OustersCoat::OustersCoat() : Invalid item type or option type");
        throw Error("OustersCoat::OustersCoat() : Invalid item type or optionType");
    }
}


//--------------------------------------------------------------------------------
// create item
//--------------------------------------------------------------------------------
void OustersCoat::create(const string& ownerID, Storage storage, StorageID_t storageID, BYTE x, BYTE y, ItemID_t itemID)

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

    defaultItemObjectRepository().insertGear(GEAR_OUSTERS_COAT, m_ItemID, m_ObjectID, getItemType(), ownerID,
                                             (int)storage, storageID, (int)x, (int)y, optionField, getDurability(),
                                             getGrade(), (int)m_CreateType);

    __END_CATCH
}


//--------------------------------------------------------------------------------
// save item
//--------------------------------------------------------------------------------
void OustersCoat::tinysave(const char* field) const

{
    __BEGIN_TRY

    defaultItemObjectRepository().tinysaveGear(GEAR_OUSTERS_COAT, field, m_ItemID);

    __END_CATCH
}

//--------------------------------------------------------------------------------
// save item
//--------------------------------------------------------------------------------
void OustersCoat::save(const string& ownerID, Storage storage, StorageID_t storageID, BYTE x, BYTE y)

{
    __BEGIN_TRY

    string optionField;
    setOptionTypeToField(getOptionTypeList(), optionField);

    defaultItemObjectRepository().updateGear(GEAR_OUSTERS_COAT, m_ObjectID, getItemType(), ownerID, (int)storage,
                                             storageID, (int)x, (int)y, optionField, getDurability(), getGrade(),
                                             (int)getEnchantLevel(), m_ItemID);

    __END_CATCH
}


//--------------------------------------------------------------------------------
// get debug string
//--------------------------------------------------------------------------------
string OustersCoat::toString() const

{
    StringStream msg;

    msg << "OustersCoat(" << "ItemID:" << m_ItemID << ",ItemType:" << (int)getItemType()
        << ",OptionType:" << getOptionTypeToString(getOptionTypeList()).c_str()
        << ",Durability:" << (int)getDurability() << ",EnchantLevel:" << (int)getEnchantLevel() << ")";

    return msg.toString();
}


//--------------------------------------------------------------------------------
// get debug string
//--------------------------------------------------------------------------------
string OustersCoatInfo::toString() const

{
    StringStream msg;

    msg << "OustersCoatInfo(" << "ItemType:" << m_ItemType << ",Name:" << m_Name << ",EName:" << m_EName
        << ",Price:" << m_Price << ",VolumeType:" << Volume2String[m_VolumeType] << ",Weight:" << m_Weight
        << ",Description:" << m_Description << ",Durability:" << m_Durability << ",DefenseBonus:" << m_DefenseBonus
        << ")";

    return msg.toString();
}


//--------------------------------------------------------------------------------
// load from DB
//--------------------------------------------------------------------------------
void OustersCoatInfoManager::load()

{
    __BEGIN_TRY

    m_InfoCount = defaultItemObjectRepository().loadMaxGearType(GEAR_OUSTERS_COAT);

    m_pItemInfos = new ItemInfo*[m_InfoCount + 1];

    for (uint i = 0; i <= m_InfoCount; i++)
        m_pItemInfos[i] = NULL;

    vector<GearInfoRow> rows = defaultItemObjectRepository().loadGearInfos(GEAR_OUSTERS_COAT);

    for (size_t r = 0; r < rows.size(); r++) {
        OustersCoatInfo* pOustersCoatInfo = new OustersCoatInfo();

        pOustersCoatInfo->setItemType(rows[r].itemType);
        pOustersCoatInfo->setName(rows[r].name);
        pOustersCoatInfo->setEName(rows[r].ename);
        pOustersCoatInfo->setPrice(rows[r].price);
        pOustersCoatInfo->setVolumeType(rows[r].volume);
        pOustersCoatInfo->setWeight(rows[r].weight);
        pOustersCoatInfo->setRatio(rows[r].ratio);
        pOustersCoatInfo->setDurability(rows[r].durability);
        pOustersCoatInfo->setDefenseBonus(rows[r].defense);
        pOustersCoatInfo->setProtectionBonus(rows[r].protection);
        pOustersCoatInfo->setReqAbility(rows[r].reqAbility);
        pOustersCoatInfo->setItemLevel(rows[r].itemLevel);
        pOustersCoatInfo->setDefaultOptions(rows[r].defaultOption);
        pOustersCoatInfo->setUpgradeRatio(rows[r].upgradeRatio);
        pOustersCoatInfo->setUpgradeCrashPercent(rows[r].upgradeCrashPercent);
        pOustersCoatInfo->setNextOptionRatio(rows[r].nextOptionRatio);
        pOustersCoatInfo->setNextItemType(rows[r].nextItemType);
        pOustersCoatInfo->setDowngradeRatio(rows[r].downgradeRatio);

        addItemInfo(pOustersCoatInfo);
    }

    __END_CATCH
}


//--------------------------------------------------------------------------------
// load to creature
//--------------------------------------------------------------------------------
void OustersCoatLoader::load(Creature* pCreature)

{
    __BEGIN_TRY

    Assert(pCreature != NULL);

    vector<GearObjectRow> rows = defaultItemObjectRepository().loadGearOfOwner(GEAR_OUSTERS_COAT, pCreature->getName());

    for (size_t r = 0; r < rows.size(); r++) {
        try {
            OustersCoat* pOustersCoat = new OustersCoat();

            pOustersCoat->setItemID(rows[r].itemID);
            pOustersCoat->setObjectID(rows[r].objectID);
            pOustersCoat->setItemType(rows[r].itemType);

            if (de::gameContext()
                    .itemInfos()
                    .getItemInfo(Item::ITEM_CLASS_OUSTERS_COAT, pOustersCoat->getItemType())
                    ->isUnique())
                pOustersCoat->setUnique();

            Storage storage = (Storage)rows[r].storage;
            BYTE x = rows[r].x;
            BYTE y = rows[r].y;

            string optionField = rows[r].optionField;
            list<OptionType_t> optionTypes;
            setOptionTypeFromField(optionTypes, optionField);
            pOustersCoat->setOptionType(optionTypes);

            pOustersCoat->setDurability(rows[r].durability);
            pOustersCoat->setGrade(rows[r].grade);
            pOustersCoat->setEnchantLevel(rows[r].enchantLevel);
            pOustersCoat->setCreateType((Item::CreateType)rows[r].createType);

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
                if (pInventory->canAddingEx(x, y, pOustersCoat)) {
                    pInventory->addItemEx(x, y, pOustersCoat);
                } else {
                    processItemBugEx(pCreature, pOustersCoat);
                }
                break;

            case STORAGE_GEAR:
                if (pCreature->isSlayer() || pCreature->isVampire()) {
                    processItemBugEx(pCreature, pOustersCoat);
                } else if (pCreature->isOusters()) {
                    if (!pOusters->isWear((Ousters::WearPart)x)) {
                        pOusters->wearItem((Ousters::WearPart)x, pOustersCoat);
                    } else {
                        processItemBugEx(pCreature, pOustersCoat);
                    }
                }
                break;

            case STORAGE_BELT:
                processItemBugEx(pCreature, pOustersCoat);
                break;

            case STORAGE_EXTRASLOT:
                if (pCreature->isSlayer())
                    pSlayer->addItemToExtraInventorySlot(pOustersCoat);
                else if (pCreature->isVampire())
                    pVampire->addItemToExtraInventorySlot(pOustersCoat);
                else if (pCreature->isOusters())
                    pOusters->addItemToExtraInventorySlot(pOustersCoat);
                break;

            case STORAGE_MOTORCYCLE:
                processItemBugEx(pCreature, pOustersCoat);
                break;

            case STORAGE_STASH:
                if (pStash->isExist(x, y)) {
                    processItemBugEx(pCreature, pOustersCoat);
                } else
                    pStash->insert(x, y, pOustersCoat);
                break;

            case STORAGE_GARBAGE:
                processItemBug(pCreature, pOustersCoat);
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
void OustersCoatLoader::load(Zone* pZone)

{
    __BEGIN_TRY

    Assert(pZone != NULL);

    vector<GearZoneObjectRow> rows =
        defaultItemObjectRepository().loadGearInZone(GEAR_OUSTERS_COAT, (int)STORAGE_ZONE, pZone->getZoneID());

    for (size_t r = 0; r < rows.size(); r++) {
        OustersCoat* pOustersCoat = new OustersCoat();

        pOustersCoat->setItemID(rows[r].itemID);
        pOustersCoat->setObjectID(rows[r].objectID);
        pOustersCoat->setItemType(rows[r].itemType);

        Storage storage = (Storage)rows[r].storage;
        BYTE x = rows[r].x;
        BYTE y = rows[r].y;

        string optionField = rows[r].optionField;
        list<OptionType_t> optionTypes;
        setOptionTypeFromField(optionTypes, optionField);
        pOustersCoat->setOptionType(optionTypes);

        pOustersCoat->setDurability(rows[r].durability);
        pOustersCoat->setEnchantLevel(rows[r].enchantLevel);
        pOustersCoat->setCreateType((Item::CreateType)rows[r].createType);

        switch (storage) {
        case STORAGE_ZONE: {
            Tile& pTile = pZone->getTile(x, y);
            Assert(!pTile.hasItem());
            pTile.addItem(pOustersCoat);
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
void OustersCoatLoader::load(StorageID_t storageID, Inventory* pInventory)

{
    __BEGIN_TRY

    Statement* pStmt;

    BEGIN_DB {}
    END_DB(pStmt)

    __END_CATCH
}
