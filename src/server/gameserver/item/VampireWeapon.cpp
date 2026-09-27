//////////////////////////////////////////////////////////////////////////////
// Filename    : VampireWeapon.cpp
// Written By  : Elca
// Description :
//////////////////////////////////////////////////////////////////////////////

// include files
#include "VampireWeapon.h"

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

ItemID_t VampireWeapon::m_ItemIDRegistry = 0;
Mutex VampireWeapon::m_Mutex;

//--------------------------------------------------------------------------------
// constructor
//--------------------------------------------------------------------------------
VampireWeapon::VampireWeapon()

{
    setItemType(0);
    setDurability(0);
    setBonusDamage(0);
}

VampireWeapon::VampireWeapon(ItemType_t itemType, const list<OptionType_t>& optionType)

//: m_OptionType( optionType )
{
    setItemType(itemType);
    setOptionType(optionType);
    setBonusDamage(0);

    setDurability(computeMaxDurability(this));

    if (!de::gameContext().itemInfos().isPossibleItem(getItemClass(), getItemType(), getOptionTypeList())) {
        filelog("itembug.log", "VampireWeapon::VampireWeapon() : Invalid item type or option type");
        throw Error("VampireWeapon::VampireWeapon() : Invalid item type or optionType");
    }
}

//--------------------------------------------------------------------------------
// create item
//--------------------------------------------------------------------------------
void VampireWeapon::create(const string& ownerID, Storage storage, StorageID_t storageID, BYTE x, BYTE y,
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

    defaultItemObjectRepository().insertGear(GEAR_VAMPIRE_WEAPON, m_ItemID, m_ObjectID, getItemType(), ownerID,
                                             (int)storage, storageID, (int)x, (int)y, optionField, getDurability(),
                                             getGrade(), (int)m_CreateType);

    __END_CATCH
}

//--------------------------------------------------------------------------------
// save item
//--------------------------------------------------------------------------------
void VampireWeapon::tinysave(const char* field) const

{
    __BEGIN_TRY

    defaultItemObjectRepository().tinysaveGear(GEAR_VAMPIRE_WEAPON, field, m_ItemID);

    __END_CATCH
}

//--------------------------------------------------------------------------------
// save item
//--------------------------------------------------------------------------------
void VampireWeapon::save(const string& ownerID, Storage storage, StorageID_t storageID, BYTE x, BYTE y)

{
    __BEGIN_TRY

    string optionField;
    setOptionTypeToField(getOptionTypeList(), optionField);

    defaultItemObjectRepository().updateGear(GEAR_VAMPIRE_WEAPON, m_ObjectID, getItemType(), ownerID, (int)storage,
                                             storageID, (int)x, (int)y, optionField, getDurability(), getGrade(),
                                             (int)getEnchantLevel(), m_ItemID);

    __END_CATCH
}


//--------------------------------------------------------------------------------
// get debug string
//--------------------------------------------------------------------------------
string VampireWeapon::toString() const

{
    StringStream msg;

    msg << "VampireWeapon(" << "ItemID:" << m_ItemID << ",ItemType:" << (int)getItemType()
        << ",OptionType:" << getOptionTypeToString(getOptionTypeList()).c_str()
        << ",Durability:" << (int)getDurability() << ",EnchantLevel:" << (int)getEnchantLevel() << ")";

    return msg.toString();
}


//--------------------------------------------------------------------------------
// get debug string
//--------------------------------------------------------------------------------
string VampireWeaponInfo::toString() const

{
    StringStream msg;

    msg << "VampireWeaponInfo(" << "ItemType:" << m_ItemType << ",Name:" << m_Name << ",EName:" << m_EName
        << ",Price:" << m_Price << ",VolumeType:" << Volume2String[m_VolumeType] << ",Weight:" << m_Weight
        << ",Description:" << m_Description << ",Durability:" << m_Durability << ",minDamage:" << m_MinDamage
        << ",maxDamage:" << m_MaxDamage << ",ReqAbility:" << ")";

    return msg.toString();
}


//--------------------------------------------------------------------------------
// load from DB
//--------------------------------------------------------------------------------
void VampireWeaponInfoManager::load()

{
    __BEGIN_TRY

    m_InfoCount = defaultItemObjectRepository().loadMaxGearType(GEAR_VAMPIRE_WEAPON);

    m_pItemInfos = new ItemInfo*[m_InfoCount + 1];

    for (uint i = 0; i <= m_InfoCount; i++)
        m_pItemInfos[i] = NULL;

    vector<WeaponInfoRow> rows = defaultItemObjectRepository().loadWeaponInfos(GEAR_VAMPIRE_WEAPON);

    for (size_t r = 0; r < rows.size(); r++) {
        VampireWeaponInfo* pVampireWeaponInfo = new VampireWeaponInfo();

        pVampireWeaponInfo->setItemType(rows[r].itemType);
        pVampireWeaponInfo->setName(rows[r].name);
        pVampireWeaponInfo->setEName(rows[r].ename);
        pVampireWeaponInfo->setPrice(rows[r].price);
        pVampireWeaponInfo->setVolumeType(rows[r].volume);
        pVampireWeaponInfo->setWeight(rows[r].weight);
        pVampireWeaponInfo->setRatio(rows[r].ratio);
        pVampireWeaponInfo->setDurability(rows[r].durability);
        pVampireWeaponInfo->setMinDamage(rows[r].minDamage);
        pVampireWeaponInfo->setMaxDamage(rows[r].maxDamage);
        pVampireWeaponInfo->setSpeed(rows[r].speed);
        pVampireWeaponInfo->setReqAbility(rows[r].reqAbility);
        pVampireWeaponInfo->setItemLevel(rows[r].itemLevel);
        pVampireWeaponInfo->setCriticalBonus(rows[r].criticalBonus);
        pVampireWeaponInfo->setDefaultOptions(rows[r].defaultOption);
        pVampireWeaponInfo->setUpgradeRatio(rows[r].upgradeRatio);
        pVampireWeaponInfo->setUpgradeCrashPercent(rows[r].upgradeCrashPercent);
        pVampireWeaponInfo->setNextOptionRatio(rows[r].nextOptionRatio);
        pVampireWeaponInfo->setNextItemType(rows[r].nextItemType);
        pVampireWeaponInfo->setDowngradeRatio(rows[r].downgradeRatio);

        addItemInfo(pVampireWeaponInfo);
    }

    __END_CATCH
}


//--------------------------------------------------------------------------------
// load to creature
//--------------------------------------------------------------------------------
void VampireWeaponLoader::load(Creature* pCreature)

{
    __BEGIN_TRY

    Assert(pCreature != NULL);

    vector<GearObjectRow> rows =
        defaultItemObjectRepository().loadGearOfOwner(GEAR_VAMPIRE_WEAPON, pCreature->getName());

    for (size_t r = 0; r < rows.size(); r++) {
        try {
            VampireWeapon* pVampireWeapon = new VampireWeapon();

            pVampireWeapon->setItemID(rows[r].itemID);
            pVampireWeapon->setObjectID(rows[r].objectID);
            pVampireWeapon->setItemType(rows[r].itemType);

            if (de::gameContext()
                    .itemInfos()
                    .getItemInfo(Item::ITEM_CLASS_VAMPIRE_WEAPON, pVampireWeapon->getItemType())
                    ->isUnique())
                pVampireWeapon->setUnique();

            Storage storage = (Storage)rows[r].storage;
            BYTE x = rows[r].x;
            BYTE y = rows[r].y;

            string optionField = rows[r].optionField;
            list<OptionType_t> optionTypes;
            setOptionTypeFromField(optionTypes, optionField);
            pVampireWeapon->setOptionType(optionTypes);

            pVampireWeapon->setDurability(rows[r].durability);
            pVampireWeapon->setGrade(rows[r].grade);
            pVampireWeapon->setEnchantLevel(rows[r].enchantLevel);
            pVampireWeapon->setCreateType((Item::CreateType)rows[r].createType);

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
                if (pInventory->canAddingEx(x, y, pVampireWeapon)) {
                    pInventory->addItemEx(x, y, pVampireWeapon);
                } else {
                    processItemBugEx(pCreature, pVampireWeapon);
                }
                break;

            case STORAGE_GEAR:
                if (pCreature->isVampire()) {
                    if (!pVampire->isWear((Vampire::WearPart)x)) {
                        pVampire->wearItem((Vampire::WearPart)x, pVampireWeapon);
                    } else {
                        processItemBugEx(pCreature, pVampireWeapon);
                    }
                } else if (pCreature->isSlayer()) {
                    processItemBugEx(pCreature, pVampireWeapon);
                }
                break;

            case STORAGE_BELT:
                processItemBugEx(pCreature, pVampireWeapon);
                break;

            case STORAGE_EXTRASLOT:
                if (pCreature->isSlayer())
                    pSlayer->addItemToExtraInventorySlot(pVampireWeapon);
                else if (pCreature->isVampire())
                    pVampire->addItemToExtraInventorySlot(pVampireWeapon);
                break;

            case STORAGE_MOTORCYCLE:
                processItemBugEx(pCreature, pVampireWeapon);
                break;

            case STORAGE_STASH:
                if (pStash->isExist(x, y)) {
                    processItemBugEx(pCreature, pVampireWeapon);
                } else {
                    pStash->insert(x, y, pVampireWeapon);
                }
                break;

            case STORAGE_GARBAGE:
                processItemBug(pCreature, pVampireWeapon);
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
void VampireWeaponLoader::load(Zone* pZone)

{
    __BEGIN_TRY

    Assert(pZone != NULL);

    vector<GearZoneObjectRow> rows =
        defaultItemObjectRepository().loadGearInZone(GEAR_VAMPIRE_WEAPON, (int)STORAGE_ZONE, pZone->getZoneID());

    for (size_t r = 0; r < rows.size(); r++) {
        VampireWeapon* pVampireWeapon = new VampireWeapon();

        pVampireWeapon->setItemID(rows[r].itemID);
        pVampireWeapon->setObjectID(rows[r].objectID);
        pVampireWeapon->setItemType(rows[r].itemType);

        Storage storage = (Storage)rows[r].storage;
        BYTE x = rows[r].x;
        BYTE y = rows[r].y;

        string optionField = rows[r].optionField;
        list<OptionType_t> optionTypes;
        setOptionTypeFromField(optionTypes, optionField);
        pVampireWeapon->setOptionType(optionTypes);

        pVampireWeapon->setDurability(rows[r].durability);
        pVampireWeapon->setEnchantLevel(rows[r].enchantLevel);
        pVampireWeapon->setCreateType((Item::CreateType)rows[r].createType);

        switch (storage) {
        case STORAGE_ZONE: {
            Tile& pTile = pZone->getTile(x, y);
            Assert(!pTile.hasItem());
            pTile.addItem(pVampireWeapon);
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
void VampireWeaponLoader::load(StorageID_t storageID, Inventory* pInventory)

{
    __BEGIN_TRY

    Statement* pStmt;

    BEGIN_DB {}
    END_DB(pStmt)

    __END_CATCH
}
