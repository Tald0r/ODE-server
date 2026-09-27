//////////////////////////////////////////////////////////////////////////////
// Filename    : VampireNecklace.cpp
// Written By  : Elca
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "VampireNecklace.h"

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

ItemID_t VampireNecklace::m_ItemIDRegistry = 0;
Mutex VampireNecklace::m_Mutex;

//--------------------------------------------------------------------------------
// constructor
//--------------------------------------------------------------------------------
VampireNecklace::VampireNecklace()

//: m_ItemType(0), m_Durability(0)
{
    setItemType(0);
    setDurability(0);
}

VampireNecklace::VampireNecklace(ItemType_t itemType, const list<OptionType_t>& optionType)

//: m_ItemType(itemType), m_OptionType(optionType), m_Durability(0)
{
    setItemType(itemType);
    setOptionType(optionType);

    setDurability(computeMaxDurability(this));

    if (!de::gameContext().itemInfos().isPossibleItem(getItemClass(), getItemType(), getOptionTypeList())) {
        filelog("itembug.log", "VampireNecklace::VampireNecklace() : Invalid item type or option type");
        throw Error("VampireNecklace::VampireNecklace() : Invalid item type or optionType");
    }
}


//--------------------------------------------------------------------------------
// create item
//--------------------------------------------------------------------------------
void VampireNecklace::create(const string& ownerID, Storage storage, StorageID_t storageID, BYTE x, BYTE y,
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

    string optionField;
    setOptionTypeToField(getOptionTypeList(), optionField);

    defaultItemObjectRepository().insertGear(GEAR_VAMPIRE_NECKLACE, m_ItemID, m_ObjectID, getItemType(), ownerID,
                                             (int)storage, storageID, (int)x, (int)y, optionField, getDurability(),
                                             getGrade(), (int)m_CreateType);

    __END_CATCH
}


//--------------------------------------------------------------------------------
// save item
//--------------------------------------------------------------------------------
void VampireNecklace::tinysave(const char* field) const

{
    __BEGIN_TRY

    defaultItemObjectRepository().tinysaveGear(GEAR_VAMPIRE_NECKLACE, field, m_ItemID);

    __END_CATCH
}

//--------------------------------------------------------------------------------
// save item
//--------------------------------------------------------------------------------
void VampireNecklace::save(const string& ownerID, Storage storage, StorageID_t storageID, BYTE x, BYTE y)

{
    __BEGIN_TRY

    string optionField;
    setOptionTypeToField(getOptionTypeList(), optionField);

    defaultItemObjectRepository().updateGear(GEAR_VAMPIRE_NECKLACE, m_ObjectID, getItemType(), ownerID, (int)storage,
                                             storageID, (int)x, (int)y, optionField, getDurability(), getGrade(),
                                             (int)getEnchantLevel(), m_ItemID);

    __END_CATCH
}


//--------------------------------------------------------------------------------
// get debug string
//--------------------------------------------------------------------------------
string VampireNecklace::toString() const

{
    StringStream msg;

    msg << "VampireNecklace(" << "ItemID:" << m_ItemID << ",ItemType:" << (int)getItemType()
        << ",OptionType:" << getOptionTypeToString(getOptionTypeList()).c_str()
        << ",Durability:" << (int)getDurability() << ",EnchantLevel:" << (int)getEnchantLevel() << ")";

    return msg.toString();
}


//--------------------------------------------------------------------------------
// get debug string
//--------------------------------------------------------------------------------
string VampireNecklaceInfo::toString() const

{
    StringStream msg;

    msg << "VampireNecklaceInfo(" << "ItemType:" << m_ItemType << ",Name:" << m_Name << ",EName:" << m_EName
        << ",Price:" << m_Price << ",VolumeType:" << Volume2String[m_VolumeType] << ",Weight:" << m_Weight
        << ",Description:" << m_Description << ",Durability:" << m_Durability << ",DefenseBonus:" << m_DefenseBonus
        << ")";

    return msg.toString();
}


//--------------------------------------------------------------------------------
// load from DB
//--------------------------------------------------------------------------------
void VampireNecklaceInfoManager::load()

{
    __BEGIN_TRY

    m_InfoCount = defaultItemObjectRepository().loadMaxGearType(GEAR_VAMPIRE_NECKLACE);

    m_pItemInfos = new ItemInfo*[m_InfoCount + 1];

    for (uint i = 0; i <= m_InfoCount; i++)
        m_pItemInfos[i] = NULL;

    vector<GearInfoRow> rows = defaultItemObjectRepository().loadGearInfos(GEAR_VAMPIRE_NECKLACE);

    for (size_t r = 0; r < rows.size(); r++) {
        VampireNecklaceInfo* pVampireNecklaceInfo = new VampireNecklaceInfo();

        pVampireNecklaceInfo->setItemType(rows[r].itemType);
        pVampireNecklaceInfo->setName(rows[r].name);
        pVampireNecklaceInfo->setEName(rows[r].ename);
        pVampireNecklaceInfo->setPrice(rows[r].price);
        pVampireNecklaceInfo->setVolumeType(rows[r].volume);
        pVampireNecklaceInfo->setWeight(rows[r].weight);
        pVampireNecklaceInfo->setRatio(rows[r].ratio);
        pVampireNecklaceInfo->setDurability(rows[r].durability);
        pVampireNecklaceInfo->setDefenseBonus(rows[r].defense);
        pVampireNecklaceInfo->setProtectionBonus(rows[r].protection);
        pVampireNecklaceInfo->setReqAbility(rows[r].reqAbility);
        pVampireNecklaceInfo->setItemLevel(rows[r].itemLevel);
        pVampireNecklaceInfo->setDefaultOptions(rows[r].defaultOption);
        pVampireNecklaceInfo->setUpgradeRatio(rows[r].upgradeRatio);
        pVampireNecklaceInfo->setUpgradeCrashPercent(rows[r].upgradeCrashPercent);
        pVampireNecklaceInfo->setNextOptionRatio(rows[r].nextOptionRatio);
        pVampireNecklaceInfo->setNextItemType(rows[r].nextItemType);
        pVampireNecklaceInfo->setDowngradeRatio(rows[r].downgradeRatio);

        addItemInfo(pVampireNecklaceInfo);
    }

    __END_CATCH
}


//--------------------------------------------------------------------------------
// load to creature
//--------------------------------------------------------------------------------
void VampireNecklaceLoader::load(Creature* pCreature)

{
    __BEGIN_TRY

    Assert(pCreature != NULL);

    vector<GearObjectRow> rows =
        defaultItemObjectRepository().loadGearOfOwner(GEAR_VAMPIRE_NECKLACE, pCreature->getName());

    for (size_t r = 0; r < rows.size(); r++) {
        try {
            VampireNecklace* pVampireNecklace = new VampireNecklace();

            pVampireNecklace->setItemID(rows[r].itemID);
            pVampireNecklace->setObjectID(rows[r].objectID);
            pVampireNecklace->setItemType(rows[r].itemType);

            if (de::gameContext()
                    .itemInfos()
                    .getItemInfo(Item::ITEM_CLASS_VAMPIRE_NECKLACE, pVampireNecklace->getItemType())
                    ->isUnique())
                pVampireNecklace->setUnique();

            Storage storage = (Storage)rows[r].storage;
            BYTE x = rows[r].x;
            BYTE y = rows[r].y;

            string optionField = rows[r].optionField;
            list<OptionType_t> optionTypes;
            setOptionTypeFromField(optionTypes, optionField);
            pVampireNecklace->setOptionType(optionTypes);

            pVampireNecklace->setDurability(rows[r].durability);
            pVampireNecklace->setGrade(rows[r].grade);
            pVampireNecklace->setEnchantLevel(rows[r].enchantLevel);
            pVampireNecklace->setCreateType((Item::CreateType)rows[r].createType);

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
                if (pInventory->canAddingEx(x, y, pVampireNecklace)) {
                    pInventory->addItemEx(x, y, pVampireNecklace);
                } else {
                    processItemBugEx(pCreature, pVampireNecklace);
                }
                break;

            case STORAGE_GEAR:
                if (pCreature->isSlayer()) {
                    processItemBugEx(pCreature, pVampireNecklace);
                } else if (pCreature->isVampire()) {
                    if (!pVampire->isWear((Vampire::WearPart)x)) {
                        pVampire->wearItem((Vampire::WearPart)x, pVampireNecklace);
                    } else {
                        processItemBugEx(pCreature, pVampireNecklace);
                    }
                }
                break;

            case STORAGE_BELT:
                processItemBugEx(pCreature, pVampireNecklace);
                break;

            case STORAGE_EXTRASLOT:
                if (pCreature->isSlayer())
                    pSlayer->addItemToExtraInventorySlot(pVampireNecklace);
                else if (pCreature->isVampire())
                    pVampire->addItemToExtraInventorySlot(pVampireNecklace);
                break;

            case STORAGE_MOTORCYCLE:
                processItemBugEx(pCreature, pVampireNecklace);
                break;

            case STORAGE_STASH:
                if (pStash->isExist(x, y)) {
                    processItemBugEx(pCreature, pVampireNecklace);
                } else
                    pStash->insert(x, y, pVampireNecklace);
                break;

            case STORAGE_GARBAGE:
                processItemBug(pCreature, pVampireNecklace);
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
void VampireNecklaceLoader::load(Zone* pZone)

{
    __BEGIN_TRY

    Assert(pZone != NULL);

    vector<GearZoneObjectRow> rows =
        defaultItemObjectRepository().loadGearInZone(GEAR_VAMPIRE_NECKLACE, (int)STORAGE_ZONE, pZone->getZoneID());

    for (size_t r = 0; r < rows.size(); r++) {
        VampireNecklace* pVampireNecklace = new VampireNecklace();

        pVampireNecklace->setItemID(rows[r].itemID);
        pVampireNecklace->setObjectID(rows[r].objectID);
        pVampireNecklace->setItemType(rows[r].itemType);

        Storage storage = (Storage)rows[r].storage;
        BYTE x = rows[r].x;
        BYTE y = rows[r].y;

        string optionField = rows[r].optionField;
        list<OptionType_t> optionTypes;
        setOptionTypeFromField(optionTypes, optionField);
        pVampireNecklace->setOptionType(optionTypes);

        pVampireNecklace->setDurability(rows[r].durability);
        pVampireNecklace->setEnchantLevel(rows[r].enchantLevel);
        pVampireNecklace->setCreateType((Item::CreateType)rows[r].createType);


        switch (storage) {
        case STORAGE_ZONE: {
            Tile& pTile = pZone->getTile(x, y);
            Assert(!pTile.hasItem());
            pTile.addItem(pVampireNecklace);
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
void VampireNecklaceLoader::load(StorageID_t storageID, Inventory* pInventory)

{
    __BEGIN_TRY

    Statement* pStmt;

    BEGIN_DB {}
    END_DB(pStmt)

    __END_CATCH
}
