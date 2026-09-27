//////////////////////////////////////////////////////////////////////////////
// Filename    : Sword.cpp
// Written By  : Elca
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "Sword.h"

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

ItemID_t Sword::m_ItemIDRegistry = 0;
Mutex Sword::m_Mutex;

//--------------------------------------------------------------------------------
// constructor
//--------------------------------------------------------------------------------
Sword::Sword()

{
    setBonusDamage(0);
}

Sword::Sword(ItemType_t itemType, const list<OptionType_t>& optionType)

//: m_OptionType( optionType )
{
    setItemType(itemType);
    setOptionType(optionType);
    setBonusDamage(0);
    setSilver(0);

    setDurability(computeMaxDurability(this));

    if (!de::gameContext().itemInfos().isPossibleItem(getItemClass(), getItemType(), getOptionTypeList())) {
        filelog("itembug.log", "Sword::Sword() : Invalid item type or option type");
        throw Error("Sword::Sword() : Invalid item type or optionType");
    }
}


//--------------------------------------------------------------------------------
// create item
//--------------------------------------------------------------------------------
void Sword::create(const string& ownerID, Storage storage, StorageID_t storageID, BYTE x, BYTE y, ItemID_t itemID)

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

    defaultItemObjectRepository().insertGear(GEAR_SWORD, m_ItemID, m_ObjectID, getItemType(), ownerID, (int)storage,
                                             storageID, (int)x, (int)y, optionField, getDurability(), getGrade(),
                                             (int)m_CreateType);

    __END_CATCH
}


//--------------------------------------------------------------------------------
// save item
//--------------------------------------------------------------------------------
void Sword::tinysave(const char* field) const

{
    __BEGIN_TRY

    defaultItemObjectRepository().tinysaveGear(GEAR_SWORD, field, m_ItemID);

    __END_CATCH
}

//--------------------------------------------------------------------------------
// save item
//--------------------------------------------------------------------------------
void Sword::save(const string& ownerID, Storage storage, StorageID_t storageID, BYTE x, BYTE y)

{
    __BEGIN_TRY

    string optionField;
    setOptionTypeToField(getOptionTypeList(), optionField);

    defaultItemObjectRepository().updateSilverWeapon(
        GEAR_SWORD, m_ObjectID, getItemType(), ownerID, (int)storage, storageID, (int)x, (int)y, optionField,
        getDurability(), (int)getEnchantLevel(), (int)getSilver(), (int)getGrade(), m_ItemID);

    __END_CATCH
}


//--------------------------------------------------------------------------------
// get debug string
//--------------------------------------------------------------------------------
string Sword::toString() const

{
    StringStream msg;

    msg << "Sword(" << "ItemID:" << m_ItemID << ",ItemType:" << (int)getItemType()
        << ",OptionType:" << getOptionTypeToString(getOptionTypeList()).c_str()
        << ",Durability:" << (int)getDurability() << ",Silver:" << (int)getSilver()
        << ",EnchantLevel:" << (int)getEnchantLevel() << ")";

    return msg.toString();
}


//--------------------------------------------------------------------------------
// get/set weapon's minDamage
//--------------------------------------------------------------------------------


//--------------------------------------------------------------------------------
// get debug string
//--------------------------------------------------------------------------------
string SwordInfo::toString() const

{
    StringStream msg;

    msg << "SwordInfo(" << "ItemType:" << m_ItemType << ",Name:" << m_Name << ",EName:" << m_EName
        << ",Price:" << m_Price << ",VolumeType:" << Volume2String[m_VolumeType] << ",Weight:" << m_Weight
        << ",Description:" << m_Description << ",Durability:" << m_Durability << ",minDamage:" << m_MinDamage
        << ",maxDamage:" << m_MaxDamage << ",ReqAbility:?" << ")";

    return msg.toString();
}


//--------------------------------------------------------------------------------
// load from DB
//--------------------------------------------------------------------------------
void SwordInfoManager::load()

{
    __BEGIN_TRY

    m_InfoCount = defaultItemObjectRepository().loadMaxGearType(GEAR_SWORD);

    m_pItemInfos = new ItemInfo*[m_InfoCount + 1];

    for (uint i = 0; i <= m_InfoCount; i++)
        m_pItemInfos[i] = NULL;

    vector<SilverWeaponInfoRow> rows = defaultItemObjectRepository().loadSilverWeaponInfos(GEAR_SWORD);

    for (size_t r = 0; r < rows.size(); r++) {
        SwordInfo* pSwordInfo = new SwordInfo();

        pSwordInfo->setItemType(rows[r].itemType);
        pSwordInfo->setName(rows[r].name);
        pSwordInfo->setEName(rows[r].ename);
        pSwordInfo->setPrice(rows[r].price);
        pSwordInfo->setVolumeType(rows[r].volume);
        pSwordInfo->setWeight(rows[r].weight);
        pSwordInfo->setRatio(rows[r].ratio);
        pSwordInfo->setDurability(rows[r].durability);
        pSwordInfo->setMinDamage(rows[r].minDamage);
        pSwordInfo->setMaxDamage(rows[r].maxDamage);
        pSwordInfo->setMaxSilver(rows[r].maxSilver);
        pSwordInfo->setSpeed(rows[r].speed);
        pSwordInfo->setReqAbility(rows[r].reqAbility);
        pSwordInfo->setItemLevel(rows[r].itemLevel);
        pSwordInfo->setCriticalBonus(rows[r].criticalBonus);
        pSwordInfo->setDefaultOptions(rows[r].defaultOption);
        pSwordInfo->setUpgradeRatio(rows[r].upgradeRatio);
        pSwordInfo->setUpgradeCrashPercent(rows[r].upgradeCrashPercent);
        pSwordInfo->setNextOptionRatio(rows[r].nextOptionRatio);
        pSwordInfo->setNextItemType(rows[r].nextItemType);
        pSwordInfo->setDowngradeRatio(rows[r].downgradeRatio);

        addItemInfo(pSwordInfo);
    }

    __END_CATCH
}


//--------------------------------------------------------------------------------
// load to creature
//--------------------------------------------------------------------------------
void SwordLoader::load(Creature* pCreature)

{
    __BEGIN_TRY

    Assert(pCreature != NULL);

    vector<SilverWeaponObjectRow> rows =
        defaultItemObjectRepository().loadSilverWeaponOfOwner(GEAR_SWORD, pCreature->getName());

    for (size_t r = 0; r < rows.size(); r++) {
        try {
            Sword* pSword = new Sword();

            pSword->setItemID(rows[r].itemID);
            pSword->setObjectID(rows[r].objectID);
            pSword->setItemType(rows[r].itemType);

            if (de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_SWORD, pSword->getItemType())->isUnique())
                pSword->setUnique();

            Storage storage = (Storage)rows[r].storage;
            BYTE x = rows[r].x;
            BYTE y = rows[r].y;

            string optionField = rows[r].optionField;
            list<OptionType_t> optionTypes;
            setOptionTypeFromField(optionTypes, optionField);
            pSword->setOptionType(optionTypes);

            pSword->setDurability(rows[r].durability);
            pSword->setEnchantLevel(rows[r].enchantLevel);
            pSword->setSilver(rows[r].silver);
            pSword->setGrade(rows[r].grade);
            pSword->setCreateType((Item::CreateType)rows[r].createType);

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
                if (pInventory->canAddingEx(x, y, pSword)) {
                    pInventory->addItemEx(x, y, pSword);
                } else {
                    processItemBugEx(pCreature, pSword);
                }
                break;

            case STORAGE_GEAR:
                if (pCreature->isSlayer()) {
                    if (!pSlayer->isWear((Slayer::WearPart)x)) {
                        pSlayer->wearItem((Slayer::WearPart)x, pSword);
                    } else {
                        processItemBugEx(pCreature, pSword);
                    }
                } else if (pCreature->isVampire()) {
                    processItemBugEx(pCreature, pSword);
                }
                break;

            case STORAGE_BELT:
                processItemBugEx(pCreature, pSword);
                break;

            case STORAGE_EXTRASLOT:
                if (pCreature->isSlayer())
                    pSlayer->addItemToExtraInventorySlot(pSword);
                else if (pCreature->isVampire())
                    pVampire->addItemToExtraInventorySlot(pSword);
                break;

            case STORAGE_MOTORCYCLE:
                processItemBugEx(pCreature, pSword);
                break;

            case STORAGE_STASH:
                if (pStash->isExist(x, y)) {
                    processItemBugEx(pCreature, pSword);
                } else
                    pStash->insert(x, y, pSword);
                break;

            case STORAGE_GARBAGE:
                processItemBug(pCreature, pSword);
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
void SwordLoader::load(Zone* pZone)

{
    __BEGIN_TRY

    Assert(pZone != NULL);

    vector<SilverWeaponZoneObjectRow> rows =
        defaultItemObjectRepository().loadSilverWeaponInZone(GEAR_SWORD, (int)STORAGE_ZONE, pZone->getZoneID());

    for (size_t r = 0; r < rows.size(); r++) {
        Sword* pSword = new Sword();

        pSword->setItemID(rows[r].itemID);
        pSword->setObjectID(rows[r].objectID);
        pSword->setItemType(rows[r].itemType);

        Storage storage = (Storage)rows[r].storage;
        BYTE x = rows[r].x;
        BYTE y = rows[r].y;

        string optionField = rows[r].optionField;
        list<OptionType_t> optionTypes;
        setOptionTypeFromField(optionTypes, optionField);
        pSword->setOptionType(optionTypes);

        pSword->setDurability(rows[r].durability);
        pSword->setEnchantLevel(rows[r].enchantLevel);
        pSword->setSilver(rows[r].silver);
        pSword->setCreateType((Item::CreateType)rows[r].createType);

        switch (storage) {
        case STORAGE_ZONE: {
            Tile& pTile = pZone->getTile(x, y);
            Assert(!pTile.hasItem());
            pTile.addItem(pSword);
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
void SwordLoader::load(StorageID_t storageID, Inventory* pInventory)

{
    __BEGIN_TRY

    Statement* pStmt;

    BEGIN_DB {}
    END_DB(pStmt)

    __END_CATCH
}
