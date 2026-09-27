//////////////////////////////////////////////////////////////////////////////
// Filename    : VampireAmulet.cpp
// Written By  : Elca
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "VampireAmulet.h"

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

ItemID_t VampireAmulet::m_ItemIDRegistry = 0;
Mutex VampireAmulet::m_Mutex;

//--------------------------------------------------------------------------------
// constructor
//--------------------------------------------------------------------------------
VampireAmulet::VampireAmulet()

//: m_ItemType(0), m_Durability(0)
{
    setItemType(0);
}

VampireAmulet::VampireAmulet(ItemType_t itemType, const list<OptionType_t>& optionType)

//: m_ItemType(itemType), m_OptionType(optionType), m_Durability(0)
{
    setItemType(itemType);
    setOptionType(optionType);


    if (!de::gameContext().itemInfos().isPossibleItem(getItemClass(), getItemType(), getOptionTypeList())) {
        filelog("itembug.log", "VampireAmulet::VampireAmulet() : Invalid item type or option type");
        throw Error("VampireAmulet::VampireAmulet() : Invalid item type or optionType");
    }
}


//--------------------------------------------------------------------------------
// create item
//--------------------------------------------------------------------------------
void VampireAmulet::create(const string& ownerID, Storage storage, StorageID_t storageID, BYTE x, BYTE y,
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

    defaultItemObjectRepository().insertOptionGradeItem(GEAR_VAMPIRE_AMULET, m_ItemID, m_ObjectID, getItemType(),
                                                        ownerID, (int)storage, storageID, (int)x, (int)y, optionField,
                                                        getGrade(), (int)m_CreateType);

    __END_CATCH
}


//--------------------------------------------------------------------------------
// save item
//--------------------------------------------------------------------------------
void VampireAmulet::tinysave(const char* field) const

{
    __BEGIN_TRY

    defaultItemObjectRepository().tinysaveGear(GEAR_VAMPIRE_AMULET, field, m_ItemID);

    __END_CATCH
}

//--------------------------------------------------------------------------------
// save item
//--------------------------------------------------------------------------------
void VampireAmulet::save(const string& ownerID, Storage storage, StorageID_t storageID, BYTE x, BYTE y)

{
    __BEGIN_TRY

    string optionField;
    setOptionTypeToField(getOptionTypeList(), optionField);

    defaultItemObjectRepository().updateAmulet(GEAR_VAMPIRE_AMULET, m_ObjectID, getItemType(), ownerID, (int)storage,
                                               storageID, (int)x, (int)y, optionField, getGrade(),
                                               (int)getEnchantLevel(), m_ItemID);

    __END_CATCH
}


//--------------------------------------------------------------------------------
// get debug string
//--------------------------------------------------------------------------------
string VampireAmulet::toString() const

{
    StringStream msg;

    msg << "VampireAmulet(" << "ItemID:" << m_ItemID << ",ItemType:" << (int)getItemType()
        << ",OptionType:" << getOptionTypeToString(getOptionTypeList()).c_str()
        << ",EnchantLevel:" << (int)getEnchantLevel() << ")";

    return msg.toString();
}


//--------------------------------------------------------------------------------
// get debug string
//--------------------------------------------------------------------------------
string VampireAmuletInfo::toString() const

{
    StringStream msg;

    msg << "VampireAmuletInfo(" << "ItemType:" << m_ItemType << ",Name:" << m_Name << ",EName:" << m_EName
        << ",Price:" << m_Price << ",VolumeType:" << Volume2String[m_VolumeType] << ",Weight:" << m_Weight
        << ",Description:" << m_Description << ",Durability:" << m_Durability << ",DefenseBonus:" << m_DefenseBonus
        << ")";

    return msg.toString();
}


//--------------------------------------------------------------------------------
// load from DB
//--------------------------------------------------------------------------------
void VampireAmuletInfoManager::load()

{
    __BEGIN_TRY

    m_InfoCount = defaultItemObjectRepository().loadMaxGearType(GEAR_VAMPIRE_AMULET);

    m_pItemInfos = new ItemInfo*[m_InfoCount + 1];

    for (uint i = 0; i <= m_InfoCount; i++)
        m_pItemInfos[i] = NULL;

    vector<GearInfoRow> rows = defaultItemObjectRepository().loadGearInfos(GEAR_VAMPIRE_AMULET);

    for (size_t r = 0; r < rows.size(); r++) {
        VampireAmuletInfo* pVampireAmuletInfo = new VampireAmuletInfo();

        pVampireAmuletInfo->setItemType(rows[r].itemType);
        pVampireAmuletInfo->setName(rows[r].name);
        pVampireAmuletInfo->setEName(rows[r].ename);
        pVampireAmuletInfo->setPrice(rows[r].price);
        pVampireAmuletInfo->setVolumeType(rows[r].volume);
        pVampireAmuletInfo->setWeight(rows[r].weight);
        pVampireAmuletInfo->setRatio(rows[r].ratio);
        pVampireAmuletInfo->setDurability(rows[r].durability);
        pVampireAmuletInfo->setDefenseBonus(rows[r].defense);
        pVampireAmuletInfo->setProtectionBonus(rows[r].protection);
        pVampireAmuletInfo->setReqAbility(rows[r].reqAbility);
        pVampireAmuletInfo->setItemLevel(rows[r].itemLevel);
        pVampireAmuletInfo->setDefaultOptions(rows[r].defaultOption);
        pVampireAmuletInfo->setUpgradeRatio(rows[r].upgradeRatio);
        pVampireAmuletInfo->setUpgradeCrashPercent(rows[r].upgradeCrashPercent);
        pVampireAmuletInfo->setNextOptionRatio(rows[r].nextOptionRatio);
        pVampireAmuletInfo->setNextItemType(rows[r].nextItemType);
        pVampireAmuletInfo->setDowngradeRatio(rows[r].downgradeRatio);

        addItemInfo(pVampireAmuletInfo);
    }

    __END_CATCH
}


//--------------------------------------------------------------------------------
// load to creature
//--------------------------------------------------------------------------------
void VampireAmuletLoader::load(Creature* pCreature)

{
    __BEGIN_TRY

    Assert(pCreature != NULL);

    vector<GearObjectRow> rows =
        defaultItemObjectRepository().loadGearOfOwner(GEAR_VAMPIRE_AMULET, pCreature->getName());

    for (size_t r = 0; r < rows.size(); r++) {
        try {
            VampireAmulet* pVampireAmulet = new VampireAmulet();

            pVampireAmulet->setItemID(rows[r].itemID);
            pVampireAmulet->setObjectID(rows[r].objectID);
            pVampireAmulet->setItemType(rows[r].itemType);

            if (de::gameContext()
                    .itemInfos()
                    .getItemInfo(Item::ITEM_CLASS_VAMPIRE_AMULET, pVampireAmulet->getItemType())
                    ->isUnique())
                pVampireAmulet->setUnique();

            Storage storage = (Storage)rows[r].storage;
            BYTE x = rows[r].x;
            BYTE y = rows[r].y;

            string optionField = rows[r].optionField;
            list<OptionType_t> optionTypes;
            setOptionTypeFromField(optionTypes, optionField);
            pVampireAmulet->setOptionType(optionTypes);

            pVampireAmulet->setDurability(rows[r].durability);
            pVampireAmulet->setGrade(rows[r].grade);
            pVampireAmulet->setEnchantLevel(rows[r].enchantLevel);
            pVampireAmulet->setCreateType((Item::CreateType)rows[r].createType);

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
                if (pInventory->canAddingEx(x, y, pVampireAmulet)) {
                    pInventory->addItemEx(x, y, pVampireAmulet);
                } else {
                    processItemBugEx(pCreature, pVampireAmulet);
                }
                break;

            case STORAGE_GEAR:
                if (pCreature->isSlayer()) {
                    processItemBugEx(pCreature, pVampireAmulet);
                } else if (pCreature->isVampire()) {
                    if (!pVampire->isWear((Vampire::WearPart)x)) {
                        pVampire->wearItem((Vampire::WearPart)x, pVampireAmulet);
                    } else {
                        processItemBugEx(pCreature, pVampireAmulet);
                    }
                }
                break;

            case STORAGE_BELT:
                processItemBugEx(pCreature, pVampireAmulet);
                break;

            case STORAGE_EXTRASLOT:
                if (pCreature->isSlayer())
                    pSlayer->addItemToExtraInventorySlot(pVampireAmulet);
                else if (pCreature->isVampire())
                    pVampire->addItemToExtraInventorySlot(pVampireAmulet);
                break;

            case STORAGE_MOTORCYCLE:
                processItemBugEx(pCreature, pVampireAmulet);
                break;

            case STORAGE_STASH:
                if (pStash->isExist(x, y)) {
                    processItemBugEx(pCreature, pVampireAmulet);
                } else
                    pStash->insert(x, y, pVampireAmulet);
                break;

            case STORAGE_GARBAGE:
                processItemBug(pCreature, pVampireAmulet);
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
void VampireAmuletLoader::load(Zone* pZone)

{
    __BEGIN_TRY

    Assert(pZone != NULL);

    vector<GearZoneObjectRow> rows =
        defaultItemObjectRepository().loadGearInZone(GEAR_VAMPIRE_AMULET, (int)STORAGE_ZONE, pZone->getZoneID());

    for (size_t r = 0; r < rows.size(); r++) {
        VampireAmulet* pVampireAmulet = new VampireAmulet();

        pVampireAmulet->setItemID(rows[r].itemID);
        pVampireAmulet->setObjectID(rows[r].objectID);
        pVampireAmulet->setItemType(rows[r].itemType);

        Storage storage = (Storage)rows[r].storage;
        BYTE x = rows[r].x;
        BYTE y = rows[r].y;

        string optionField = rows[r].optionField;
        list<OptionType_t> optionTypes;
        setOptionTypeFromField(optionTypes, optionField);
        pVampireAmulet->setOptionType(optionTypes);

        pVampireAmulet->setDurability(rows[r].durability);
        pVampireAmulet->setEnchantLevel(rows[r].enchantLevel);
        pVampireAmulet->setCreateType((Item::CreateType)rows[r].createType);

        switch (storage) {
        case STORAGE_ZONE: {
            Tile& pTile = pZone->getTile(x, y);
            Assert(!pTile.hasItem());
            pTile.addItem(pVampireAmulet);
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
void VampireAmuletLoader::load(StorageID_t storageID, Inventory* pInventory)

{
    __BEGIN_TRY

    Statement* pStmt;

    BEGIN_DB {}
    END_DB(pStmt)

    __END_CATCH
}
