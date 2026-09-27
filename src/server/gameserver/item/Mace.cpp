//////////////////////////////////////////////////////////////////////////////
// Filename    : Mace.cpp
// Written By  : Elca
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "Mace.h"

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

ItemID_t Mace::m_ItemIDRegistry = 0;
Mutex Mace::m_Mutex;

//--------------------------------------------------------------------------------
// constructor
//--------------------------------------------------------------------------------
Mace::Mace()

{
    setItemType(0);
    setDurability(0);
    setBonusDamage(0);
    setSilver(0);
}

Mace::Mace(ItemType_t itemType, const list<OptionType_t>& optionType)

//: m_OptionType( optionType )
{
    setItemType(itemType);
    setOptionType(optionType);
    setBonusDamage(0);
    setSilver(0);

    setDurability(computeMaxDurability(this));

    if (!de::gameContext().itemInfos().isPossibleItem(getItemClass(), getItemType(), getOptionTypeList())) {
        filelog("itembug.log", "Mace::Mace() : Invalid item type or option type");
        throw Error("Mace::Mace() : Invalid item type or optionType");
    }
}


//--------------------------------------------------------------------------------
// create item
//--------------------------------------------------------------------------------
void Mace::create(const string& ownerID, Storage storage, StorageID_t storageID, BYTE x, BYTE y, ItemID_t itemID)

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

    defaultItemObjectRepository().insertGear(GEAR_MACE, m_ItemID, m_ObjectID, getItemType(), ownerID, (int)storage,
                                             storageID, (int)x, (int)y, optionField, getDurability(), (int)getGrade(),
                                             (int)m_CreateType);

    __END_CATCH
}


//--------------------------------------------------------------------------------
// save item
//--------------------------------------------------------------------------------
void Mace::tinysave(const char* field) const

{
    __BEGIN_TRY

    defaultItemObjectRepository().tinysaveGear(GEAR_MACE, field, m_ItemID);

    __END_CATCH
}

//--------------------------------------------------------------------------------
// save item
//--------------------------------------------------------------------------------
void Mace::save(const string& ownerID, Storage storage, StorageID_t storageID, BYTE x, BYTE y)

{
    __BEGIN_TRY

    string optionField;
    setOptionTypeToField(getOptionTypeList(), optionField);

    defaultItemObjectRepository().updateSilverWeapon(
        GEAR_MACE, m_ObjectID, getItemType(), ownerID, (int)storage, storageID, (int)x, (int)y, optionField,
        getDurability(), (int)getEnchantLevel(), (int)getSilver(), (int)getGrade(), m_ItemID);

    __END_CATCH
}


//--------------------------------------------------------------------------------
// get debug string
//--------------------------------------------------------------------------------
string Mace::toString() const

{
    StringStream msg;

    msg << "Mace(" << "ItemID:" << m_ItemID << ",ItemType:" << (int)getItemType()
        << ",OptionType:" << getOptionTypeToString(getOptionTypeList()).c_str()
        << ",Durability:" << (int)getDurability() << ",Silver:" << (int)getSilver()
        << ",EnchantLevel:" << (int)getEnchantLevel() << ")";

    return msg.toString();
}


MP_t Mace::getMPBonus() const

{
    __BEGIN_TRY

    return de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_MACE, getItemType())->getMPBonus();

    __END_CATCH
}
//--------------------------------------------------------------------------------
// get debug string
//--------------------------------------------------------------------------------
string MaceInfo::toString() const

{
    StringStream msg;

    msg << "MaceInfo(" << "ItemType:" << m_ItemType << ",Name:" << m_Name << ",EName:" << m_EName
        << ",Price:" << m_Price << ",VolumeType:" << Volume2String[m_VolumeType] << ",Weight:" << m_Weight
        << ",Description:" << m_Description << ",Durability:" << m_Durability << ",minDamage:" << m_MinDamage
        << ",maxDamage:" << m_MaxDamage << ",MPBonus:" << m_MPBonus << ",ReqAbility:?" << ")";

    return msg.toString();
}


//--------------------------------------------------------------------------------
// load from DB
//--------------------------------------------------------------------------------
void MaceInfoManager::load()

{
    __BEGIN_TRY

    m_InfoCount = defaultItemObjectRepository().loadMaxGearType(GEAR_MACE);

    m_pItemInfos = new ItemInfo*[m_InfoCount + 1];

    for (uint i = 0; i <= m_InfoCount; i++)
        m_pItemInfos[i] = NULL;

    vector<SilverWeaponMPInfoRow> rows = defaultItemObjectRepository().loadSilverWeaponMPInfos(GEAR_MACE);

    for (size_t r = 0; r < rows.size(); r++) {
        MaceInfo* pMaceInfo = new MaceInfo();

        pMaceInfo->setItemType(rows[r].itemType);
        pMaceInfo->setName(rows[r].name);
        pMaceInfo->setEName(rows[r].ename);
        pMaceInfo->setPrice(rows[r].price);
        pMaceInfo->setVolumeType(rows[r].volume);
        pMaceInfo->setWeight(rows[r].weight);
        pMaceInfo->setRatio(rows[r].ratio);
        pMaceInfo->setDurability(rows[r].durability);
        pMaceInfo->setMinDamage(rows[r].minDamage);
        pMaceInfo->setMaxDamage(rows[r].maxDamage);
        pMaceInfo->setMPBonus(rows[r].mpBonus);
        pMaceInfo->setMaxSilver(rows[r].maxSilver);
        pMaceInfo->setSpeed(rows[r].speed);
        pMaceInfo->setReqAbility(rows[r].reqAbility);
        pMaceInfo->setItemLevel(rows[r].itemLevel);
        pMaceInfo->setCriticalBonus(rows[r].criticalBonus);
        pMaceInfo->setDefaultOptions(rows[r].defaultOption);
        pMaceInfo->setUpgradeRatio(rows[r].upgradeRatio);
        pMaceInfo->setUpgradeCrashPercent(rows[r].upgradeCrashPercent);
        pMaceInfo->setNextOptionRatio(rows[r].nextOptionRatio);
        pMaceInfo->setNextItemType(rows[r].nextItemType);
        pMaceInfo->setDowngradeRatio(rows[r].downgradeRatio);

        addItemInfo(pMaceInfo);
    }

    __END_CATCH
}


//--------------------------------------------------------------------------------
// load to creature
//--------------------------------------------------------------------------------
void MaceLoader::load(Creature* pCreature)

{
    __BEGIN_TRY

    Assert(pCreature != NULL);

    vector<SilverWeaponObjectRow> rows =
        defaultItemObjectRepository().loadSilverWeaponOfOwner(GEAR_MACE, pCreature->getName());

    for (size_t r = 0; r < rows.size(); r++) {
        try {
            Mace* pMace = new Mace();

            pMace->setItemID(rows[r].itemID);
            pMace->setObjectID(rows[r].objectID);
            pMace->setItemType(rows[r].itemType);

            if (de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_MACE, pMace->getItemType())->isUnique())
                pMace->setUnique();

            Storage storage = (Storage)rows[r].storage;
            BYTE x = rows[r].x;
            BYTE y = rows[r].y;

            string optionField = rows[r].optionField;
            list<OptionType_t> optionTypes;
            setOptionTypeFromField(optionTypes, optionField);
            pMace->setOptionType(optionTypes);

            pMace->setDurability(rows[r].durability);
            pMace->setEnchantLevel(rows[r].enchantLevel);
            pMace->setSilver(rows[r].silver);
            pMace->setGrade(rows[r].grade);
            pMace->setCreateType((Item::CreateType)rows[r].createType);

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
                if (pInventory->canAddingEx(x, y, pMace)) {
                    pInventory->addItemEx(x, y, pMace);
                } else {
                    processItemBugEx(pCreature, pMace);
                }
                break;

            case STORAGE_GEAR:
                if (pCreature->isSlayer()) {
                    if (!pSlayer->isWear((Slayer::WearPart)x)) {
                        pSlayer->wearItem((Slayer::WearPart)x, pMace);
                    } else {
                        processItemBugEx(pCreature, pMace);
                    }
                } else if (pCreature->isVampire()) {
                    processItemBugEx(pCreature, pMace);
                }
                break;

            case STORAGE_BELT:
                processItemBugEx(pCreature, pMace);
                break;

            case STORAGE_EXTRASLOT:
                if (pCreature->isSlayer())
                    pSlayer->addItemToExtraInventorySlot(pMace);
                else if (pCreature->isVampire())
                    pVampire->addItemToExtraInventorySlot(pMace);
                break;

            case STORAGE_MOTORCYCLE:
                processItemBugEx(pCreature, pMace);
                break;

            case STORAGE_STASH:
                if (pStash->isExist(x, y)) {
                    processItemBugEx(pCreature, pMace);
                } else
                    pStash->insert(x, y, pMace);
                break;

            case STORAGE_GARBAGE:
                processItemBug(pCreature, pMace);
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
void MaceLoader::load(Zone* pZone)

{
    __BEGIN_TRY

    Assert(pZone != NULL);

    vector<SilverWeaponZoneObjectRow> rows =
        defaultItemObjectRepository().loadSilverWeaponInZone(GEAR_MACE, (int)STORAGE_ZONE, pZone->getZoneID());

    for (size_t r = 0; r < rows.size(); r++) {
        Mace* pMace = new Mace();

        pMace->setItemID(rows[r].itemID);
        pMace->setObjectID(rows[r].objectID);
        pMace->setItemType(rows[r].itemType);

        Storage storage = (Storage)rows[r].storage;
        BYTE x = rows[r].x;
        BYTE y = rows[r].y;

        string optionField = rows[r].optionField;
        list<OptionType_t> optionTypes;
        setOptionTypeFromField(optionTypes, optionField);
        pMace->setOptionType(optionTypes);

        pMace->setDurability(rows[r].durability);
        pMace->setEnchantLevel(rows[r].enchantLevel);
        pMace->setSilver(rows[r].silver);
        pMace->setCreateType((Item::CreateType)rows[r].createType);

        switch (storage) {
        case STORAGE_ZONE: {
            Tile& pTile = pZone->getTile(x, y);
            Assert(!pTile.hasItem());
            pTile.addItem(pMace);
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
void MaceLoader::load(StorageID_t storageID, Inventory* pInventory)

{
    __BEGIN_TRY

    Statement* pStmt;

    BEGIN_DB {}
    END_DB(pStmt)

    __END_CATCH
}
