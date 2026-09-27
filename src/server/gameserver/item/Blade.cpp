//////////////////////////////////////////////////////////////////////////////
// Filename    : Blade.cpp
// Written By  : Elca
// Description :
//////////////////////////////////////////////////////////////////////////////

// include files
#include "Blade.h"

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

ItemID_t Blade::m_ItemIDRegistry = 0;
Mutex Blade::m_Mutex;

//--------------------------------------------------------------------------------
// constructor
//--------------------------------------------------------------------------------
Blade::Blade()

{
    setItemType(0);
    setDurability(0);
    setBonusDamage(0);
    setSilver(0);
}

Blade::Blade(ItemType_t itemType, const list<OptionType_t>& optionType)

//: m_OptionType(optionType)
{
    setItemType(itemType);
    setOptionType(optionType);
    setBonusDamage(0);
    setSilver(0);

    setDurability(computeMaxDurability(this));

    if (!de::gameContext().itemInfos().isPossibleItem(getItemClass(), getItemType(), getOptionTypeList())) {
        filelog("itembug.log", "Blade::Blade() : Invalid item type or option type");
        throw Error("Blade::Blade() : Invalid item type or optionType");
    }
}

//--------------------------------------------------------------------------------
// create item
//--------------------------------------------------------------------------------
void Blade::create(const string& ownerID, Storage storage, StorageID_t storageID, BYTE x, BYTE y, ItemID_t itemID)

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

    defaultItemObjectRepository().insertGear(GEAR_BLADE, m_ItemID, m_ObjectID, getItemType(), ownerID, (int)storage,
                                             storageID, (int)x, (int)y, optionField, getDurability(), getGrade(),
                                             (int)m_CreateType);

    __END_CATCH
}

//--------------------------------------------------------------------------------
// save item
//--------------------------------------------------------------------------------
void Blade::tinysave(const char* field) const

{
    __BEGIN_TRY

    defaultItemObjectRepository().tinysaveGear(GEAR_BLADE, field, m_ItemID);

    __END_CATCH
}

//--------------------------------------------------------------------------------
// save item
//--------------------------------------------------------------------------------
void Blade::save(const string& ownerID, Storage storage, StorageID_t storageID, BYTE x, BYTE y)

{
    __BEGIN_TRY

    string optionField;
    setOptionTypeToField(getOptionTypeList(), optionField);

    defaultItemObjectRepository().updateSilverWeapon(
        GEAR_BLADE, m_ObjectID, getItemType(), ownerID, (int)storage, storageID, (int)x, (int)y, optionField,
        getDurability(), (int)getEnchantLevel(), (int)getSilver(), (int)getGrade(), m_ItemID);

    __END_CATCH
}


//--------------------------------------------------------------------------------
// get debug string
//--------------------------------------------------------------------------------
string Blade::toString() const

{
    StringStream msg;

    msg << "Blade(" << "ItemID:" << m_ItemID << ",ItemType:" << (int)getItemType()
        << ",OptionType:" << getOptionTypeToString(getOptionTypeList()).c_str()
        << ",Durability:" << (int)getDurability() << ",Silver:" << (int)getSilver()
        << ",EnchantLevel:" << (int)getEnchantLevel() << ")";

    return msg.toString();
}


//--------------------------------------------------------------------------------
// get debug string
//--------------------------------------------------------------------------------
string BladeInfo::toString() const

{
    StringStream msg;

    msg << "BladeInfo(" << "ItemType:" << m_ItemType << ",Name:" << m_Name << ",EName:" << m_EName
        << ",Price:" << m_Price << ",VolumeType:" << Volume2String[m_VolumeType] << ",Weight:" << m_Weight
        << ",Description:" << m_Description << ",Durability:" << m_Durability << ",minDamage:" << m_MinDamage
        << ",maxDamage:" << m_MaxDamage << ",ReqAbility:" << ")";

    return msg.toString();
}


//--------------------------------------------------------------------------------
// load from DB
//--------------------------------------------------------------------------------
void BladeInfoManager::load()

{
    __BEGIN_TRY

    m_InfoCount = defaultItemObjectRepository().loadMaxGearType(GEAR_BLADE);

    m_pItemInfos = new ItemInfo*[m_InfoCount + 1];

    for (uint i = 0; i <= m_InfoCount; i++)
        m_pItemInfos[i] = NULL;

    vector<SilverWeaponInfoRow> rows = defaultItemObjectRepository().loadSilverWeaponInfos(GEAR_BLADE);

    for (size_t r = 0; r < rows.size(); r++) {
        BladeInfo* pBladeInfo = new BladeInfo();

        pBladeInfo->setItemType(rows[r].itemType);
        pBladeInfo->setName(rows[r].name);
        pBladeInfo->setEName(rows[r].ename);
        pBladeInfo->setPrice(rows[r].price);
        pBladeInfo->setVolumeType(rows[r].volume);
        pBladeInfo->setWeight(rows[r].weight);
        pBladeInfo->setRatio(rows[r].ratio);
        pBladeInfo->setDurability(rows[r].durability);
        pBladeInfo->setMinDamage(rows[r].minDamage);
        pBladeInfo->setMaxDamage(rows[r].maxDamage);
        pBladeInfo->setMaxSilver(rows[r].maxSilver);
        pBladeInfo->setSpeed(rows[r].speed);
        pBladeInfo->setReqAbility(rows[r].reqAbility);
        pBladeInfo->setItemLevel(rows[r].itemLevel);
        pBladeInfo->setCriticalBonus(rows[r].criticalBonus);
        pBladeInfo->setDefaultOptions(rows[r].defaultOption);
        pBladeInfo->setUpgradeRatio(rows[r].upgradeRatio);
        pBladeInfo->setUpgradeCrashPercent(rows[r].upgradeCrashPercent);
        pBladeInfo->setNextOptionRatio(rows[r].nextOptionRatio);
        pBladeInfo->setNextItemType(rows[r].nextItemType);
        pBladeInfo->setDowngradeRatio(rows[r].downgradeRatio);

        addItemInfo(pBladeInfo);
    }

    __END_CATCH
}


//--------------------------------------------------------------------------------
// load to creature
//--------------------------------------------------------------------------------
void BladeLoader::load(Creature* pCreature)

{
    __BEGIN_TRY

    Assert(pCreature != NULL);

    vector<SilverWeaponObjectRow> rows =
        defaultItemObjectRepository().loadSilverWeaponOfOwner(GEAR_BLADE, pCreature->getName());

    for (size_t r = 0; r < rows.size(); r++) {
        try {
            Blade* pBlade = new Blade();

            pBlade->setItemID(rows[r].itemID);
            pBlade->setObjectID(rows[r].objectID);
            pBlade->setItemType(rows[r].itemType);

            if (de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_BLADE, pBlade->getItemType())->isUnique())
                pBlade->setUnique();

            Storage storage = (Storage)rows[r].storage;
            BYTE x = rows[r].x;
            BYTE y = rows[r].y;

            string optionField = rows[r].optionField;
            list<OptionType_t> optionTypes;
            setOptionTypeFromField(optionTypes, optionField);
            pBlade->setOptionType(optionTypes);

            pBlade->setDurability(rows[r].durability);
            pBlade->setEnchantLevel(rows[r].enchantLevel);
            pBlade->setSilver(rows[r].silver);
            pBlade->setGrade(rows[r].grade);
            pBlade->setCreateType((Item::CreateType)rows[r].createType);

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
                if (pInventory->canAddingEx(x, y, pBlade)) {
                    pInventory->addItemEx(x, y, pBlade);
                } else {
                    processItemBugEx(pCreature, pBlade);
                }
                break;

            case STORAGE_GEAR:
                if (pCreature->isSlayer()) {
                    if (!pSlayer->isWear((Slayer::WearPart)x)) {
                        pSlayer->wearItem((Slayer::WearPart)x, pBlade);
                    } else {
                        processItemBugEx(pCreature, pBlade);
                    }
                } else if (pCreature->isVampire()) {
                    processItemBugEx(pCreature, pBlade);
                }
                break;

            case STORAGE_BELT:
                processItemBugEx(pCreature, pBlade);
                break;

            case STORAGE_EXTRASLOT:
                if (pCreature->isSlayer())
                    pSlayer->addItemToExtraInventorySlot(pBlade);
                else if (pCreature->isVampire())
                    pVampire->addItemToExtraInventorySlot(pBlade);
                break;

            case STORAGE_MOTORCYCLE:
                processItemBugEx(pCreature, pBlade);
                break;

            case STORAGE_STASH:
                if (pStash->isExist(x, y)) {
                    processItemBugEx(pCreature, pBlade);
                } else {
                    pStash->insert(x, y, pBlade);
                }
                break;

            case STORAGE_GARBAGE:
                processItemBug(pCreature, pBlade);
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
void BladeLoader::load(Zone* pZone)

{
    __BEGIN_TRY

    Assert(pZone != NULL);

    vector<SilverWeaponZoneObjectRow> rows =
        defaultItemObjectRepository().loadSilverWeaponInZone(GEAR_BLADE, (int)STORAGE_ZONE, pZone->getZoneID());

    for (size_t r = 0; r < rows.size(); r++) {
        Blade* pBlade = new Blade();

        pBlade->setItemID(rows[r].itemID);
        pBlade->setObjectID(rows[r].objectID);
        pBlade->setItemType(rows[r].itemType);

        Storage storage = (Storage)rows[r].storage;
        BYTE x = rows[r].x;
        BYTE y = rows[r].y;

        string optionField = rows[r].optionField;
        list<OptionType_t> optionTypes;
        setOptionTypeFromField(optionTypes, optionField);
        pBlade->setOptionType(optionTypes);

        pBlade->setDurability(rows[r].durability);
        pBlade->setEnchantLevel(rows[r].enchantLevel);
        pBlade->setSilver(rows[r].silver);
        pBlade->setCreateType((Item::CreateType)rows[r].createType);

        switch (storage) {
        case STORAGE_ZONE: {
            Tile& pTile = pZone->getTile(x, y);
            Assert(!pTile.hasItem());
            pTile.addItem(pBlade);
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
void BladeLoader::load(StorageID_t storageID, Inventory* pInventory)

{
    __BEGIN_TRY

    Statement* pStmt;

    BEGIN_DB {}
    END_DB(pStmt)

    __END_CATCH
}
