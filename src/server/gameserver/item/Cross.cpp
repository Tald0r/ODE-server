//////////////////////////////////////////////////////////////////////////////
// Filename    : Cross.cpp
// Written By  : Elca
// Description :
//////////////////////////////////////////////////////////////////////////////

// include files
#include "Cross.h"

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

ItemID_t Cross::m_ItemIDRegistry = 0;
Mutex Cross::m_Mutex;

//--------------------------------------------------------------------------------
// constructor
//--------------------------------------------------------------------------------
Cross::Cross()

{
    setItemType(0);
    setDurability(0);
    setBonusDamage(0);
    setSilver(0);
}

Cross::Cross(ItemType_t itemType, const list<OptionType_t>& optionType)

//: m_OptionType( optionType )
{
    setItemType(itemType);
    setOptionType(optionType);
    setBonusDamage(0);
    setSilver(0);

    setDurability(computeMaxDurability(this));

    if (!de::gameContext().itemInfos().isPossibleItem(getItemClass(), getItemType(), getOptionTypeList())) {
        filelog("itembug.log", "Cross::Cross() : Invalid item type or option type");
        throw Error("Cross::Cross() : Invalid item type or optionType");
    }
}


//--------------------------------------------------------------------------------
// create item
//--------------------------------------------------------------------------------
void Cross::create(const string& ownerID, Storage storage, StorageID_t storageID, BYTE x, BYTE y, ItemID_t itemID)

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

    defaultItemObjectRepository().insertGear(GEAR_CROSS, m_ItemID, m_ObjectID, getItemType(), ownerID, (int)storage,
                                             storageID, (int)x, (int)y, optionField, getDurability(), (int)getGrade(),
                                             (int)m_CreateType);

    __END_CATCH
}


//--------------------------------------------------------------------------------
// save item
//--------------------------------------------------------------------------------
void Cross::tinysave(const char* field) const

{
    __BEGIN_TRY

    defaultItemObjectRepository().tinysaveGear(GEAR_CROSS, field, m_ItemID);

    __END_CATCH
}

//--------------------------------------------------------------------------------
// save item
//--------------------------------------------------------------------------------
void Cross::save(const string& ownerID, Storage storage, StorageID_t storageID, BYTE x, BYTE y)

{
    __BEGIN_TRY

    string optionField;
    setOptionTypeToField(getOptionTypeList(), optionField);

    defaultItemObjectRepository().updateSilverWeapon(
        GEAR_CROSS, m_ObjectID, getItemType(), ownerID, (int)storage, storageID, (int)x, (int)y, optionField,
        getDurability(), (int)getEnchantLevel(), (int)getSilver(), (int)getGrade(), m_ItemID);

    __END_CATCH
}


//--------------------------------------------------------------------------------
// get debug string
//--------------------------------------------------------------------------------
string Cross::toString() const

{
    StringStream msg;

    msg << "Cross(" << "ItemID:" << m_ItemID << ",ItemType:" << (int)getItemType()
        << ",OptionType:" << getOptionTypeToString(getOptionTypeList()).c_str()
        << ",Durability:" << (int)getDurability() << ",Silver:" << (int)getSilver()
        << ",EnchantLevel:" << (int)getEnchantLevel() << ")";

    return msg.toString();
}


MP_t Cross::getMPBonus() const

{
    __BEGIN_TRY

    return de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_CROSS, getItemType())->getMPBonus();

    __END_CATCH
}

//--------------------------------------------------------------------------------
// get debug string
//--------------------------------------------------------------------------------
string CrossInfo::toString() const

{
    StringStream msg;

    msg << "CrossInfo(" << "ItemType:" << m_ItemType << ",Name:" << m_Name << ",EName:" << m_EName
        << ",Price:" << m_Price << ",VolumeType:" << Volume2String[m_VolumeType] << ",Weight:" << m_Weight
        << ",Description:" << m_Description << ",Durability:" << m_Durability << ",minDamage:" << m_MinDamage
        << ",maxDamage:" << m_MaxDamage << ",MPBonus:" << m_MPBonus << ",ReqAbility:?" << ")";

    return msg.toString();
}


//--------------------------------------------------------------------------------
// load from DB
//--------------------------------------------------------------------------------
void CrossInfoManager::load()

{
    __BEGIN_TRY

    m_InfoCount = defaultItemObjectRepository().loadMaxGearType(GEAR_CROSS);

    m_pItemInfos = new ItemInfo*[m_InfoCount + 1];

    for (uint i = 0; i <= m_InfoCount; i++)
        m_pItemInfos[i] = NULL;

    vector<SilverWeaponMPInfoRow> rows = defaultItemObjectRepository().loadSilverWeaponMPInfos(GEAR_CROSS);

    for (size_t r = 0; r < rows.size(); r++) {
        CrossInfo* pCrossInfo = new CrossInfo();

        pCrossInfo->setItemType(rows[r].itemType);
        pCrossInfo->setName(rows[r].name);
        pCrossInfo->setEName(rows[r].ename);
        pCrossInfo->setPrice(rows[r].price);
        pCrossInfo->setVolumeType(rows[r].volume);
        pCrossInfo->setWeight(rows[r].weight);
        pCrossInfo->setRatio(rows[r].ratio);
        pCrossInfo->setDurability(rows[r].durability);
        pCrossInfo->setMinDamage(rows[r].minDamage);
        pCrossInfo->setMaxDamage(rows[r].maxDamage);
        pCrossInfo->setMPBonus(rows[r].mpBonus);
        pCrossInfo->setMaxSilver(rows[r].maxSilver);
        pCrossInfo->setSpeed(rows[r].speed);
        pCrossInfo->setReqAbility(rows[r].reqAbility);
        pCrossInfo->setItemLevel(rows[r].itemLevel);
        pCrossInfo->setCriticalBonus(rows[r].criticalBonus);
        pCrossInfo->setDefaultOptions(rows[r].defaultOption);
        pCrossInfo->setUpgradeRatio(rows[r].upgradeRatio);
        pCrossInfo->setUpgradeCrashPercent(rows[r].upgradeCrashPercent);
        pCrossInfo->setNextOptionRatio(rows[r].nextOptionRatio);
        pCrossInfo->setNextItemType(rows[r].nextItemType);
        pCrossInfo->setDowngradeRatio(rows[r].downgradeRatio);

        addItemInfo(pCrossInfo);
    }

    __END_CATCH
}


//--------------------------------------------------------------------------------
// load to creature
//--------------------------------------------------------------------------------
void CrossLoader::load(Creature* pCreature)

{
    __BEGIN_TRY

    Assert(pCreature != NULL);

    vector<SilverWeaponObjectRow> rows =
        defaultItemObjectRepository().loadSilverWeaponOfOwner(GEAR_CROSS, pCreature->getName());

    for (size_t r = 0; r < rows.size(); r++) {
        try {
            Cross* pCross = new Cross();

            pCross->setItemID(rows[r].itemID);
            pCross->setObjectID(rows[r].objectID);
            pCross->setItemType(rows[r].itemType);

            if (de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_CROSS, pCross->getItemType())->isUnique())
                pCross->setUnique();

            Storage storage = (Storage)rows[r].storage;
            BYTE x = rows[r].x;
            BYTE y = rows[r].y;

            string optionField = rows[r].optionField;
            list<OptionType_t> optionTypes;
            setOptionTypeFromField(optionTypes, optionField);
            pCross->setOptionType(optionTypes);

            pCross->setDurability(rows[r].durability);
            pCross->setEnchantLevel(rows[r].enchantLevel);
            pCross->setSilver(rows[r].silver);
            pCross->setGrade(rows[r].grade);
            pCross->setCreateType((Item::CreateType)rows[r].createType);

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
                if (pInventory->canAddingEx(x, y, pCross)) {
                    pInventory->addItemEx(x, y, pCross);
                } else {
                    processItemBugEx(pCreature, pCross);
                }
                break;

            case STORAGE_GEAR:
                if (pCreature->isSlayer()) {
                    if (!pSlayer->isWear((Slayer::WearPart)x)) {
                        pSlayer->wearItem((Slayer::WearPart)x, pCross);
                    } else {
                        processItemBugEx(pCreature, pCross);
                    }
                } else if (pCreature->isVampire()) {
                    processItemBugEx(pCreature, pCross);
                }
                break;

            case STORAGE_BELT:
                processItemBugEx(pCreature, pCross);
                break;

            case STORAGE_EXTRASLOT:
                if (pCreature->isSlayer())
                    pSlayer->addItemToExtraInventorySlot(pCross);
                else if (pCreature->isVampire())
                    pVampire->addItemToExtraInventorySlot(pCross);
                break;

            case STORAGE_MOTORCYCLE:
                processItemBugEx(pCreature, pCross);
                break;

            case STORAGE_STASH:
                if (pStash->isExist(x, y)) {
                    processItemBugEx(pCreature, pCross);
                } else
                    pStash->insert(x, y, pCross);
                break;

            case STORAGE_GARBAGE:
                processItemBug(pCreature, pCross);
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
void CrossLoader::load(Zone* pZone)

{
    __BEGIN_TRY

    Assert(pZone != NULL);

    vector<SilverWeaponZoneObjectRow> rows =
        defaultItemObjectRepository().loadSilverWeaponInZone(GEAR_CROSS, (int)STORAGE_ZONE, pZone->getZoneID());

    for (size_t r = 0; r < rows.size(); r++) {
        Cross* pCross = new Cross();

        pCross->setItemID(rows[r].itemID);
        pCross->setObjectID(rows[r].objectID);
        pCross->setItemType(rows[r].itemType);

        Storage storage = (Storage)rows[r].storage;
        BYTE x = rows[r].x;
        BYTE y = rows[r].y;

        string optionField = rows[r].optionField;
        list<OptionType_t> optionTypes;
        setOptionTypeFromField(optionTypes, optionField);
        pCross->setOptionType(optionTypes);

        pCross->setDurability(rows[r].durability);
        pCross->setEnchantLevel(rows[r].enchantLevel);
        pCross->setSilver(rows[r].silver);
        pCross->setCreateType((Item::CreateType)rows[r].createType);

        switch (storage) {
        case STORAGE_ZONE: {
            Tile& pTile = pZone->getTile(x, y);
            Assert(!pTile.hasItem());
            pTile.addItem(pCross);
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
void CrossLoader::load(StorageID_t storageID, Inventory* pInventory)

{
    __BEGIN_TRY

    Statement* pStmt;

    BEGIN_DB {}
    END_DB(pStmt)

    __END_CATCH
}
