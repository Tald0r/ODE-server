//////////////////////////////////////////////////////////////////////////////
// Filename    : Trouser.cpp
// Written By  : Elca
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "Trouser.h"

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

ItemID_t Trouser::m_ItemIDRegistry = 0;
Mutex Trouser::m_Mutex;

//--------------------------------------------------------------------------------
// constructor
//--------------------------------------------------------------------------------
Trouser::Trouser()

//: m_ItemType(0), m_Durability(0)
{
    setItemType(0);
    setDurability(0);
}

Trouser::Trouser(ItemType_t itemType, const list<OptionType_t>& optionType)

//: m_ItemType(itemType), m_OptionType(optionType), m_Durability(0)
{
    setItemType(itemType);
    setOptionType(optionType);

    setDurability(computeMaxDurability(this));

    if (!de::gameContext().itemInfos().isPossibleItem(getItemClass(), getItemType(), getOptionTypeList())) {
        filelog("itembug.log", "Trouser::Trouser() : Invalid item type or option type");
        throw Error("Trouser::Trouser() : Invalid item type or optionType");
    }
}


//--------------------------------------------------------------------------------
// create item
//--------------------------------------------------------------------------------
void Trouser::create(const string& ownerID, Storage storage, StorageID_t storageID, BYTE x, BYTE y, ItemID_t itemID)

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

    defaultItemObjectRepository().insertGear(GEAR_TROUSER, m_ItemID, m_ObjectID, getItemType(), ownerID, (int)storage,
                                             storageID, (int)x, (int)y, optionField, getDurability(), (int)getGrade(),
                                             (int)m_CreateType);

    __END_CATCH
}


//--------------------------------------------------------------------------------
// save item
//--------------------------------------------------------------------------------
void Trouser::tinysave(const char* field) const

{
    __BEGIN_TRY

    defaultItemObjectRepository().tinysaveGear(GEAR_TROUSER, field, m_ItemID);

    __END_CATCH
}

//--------------------------------------------------------------------------------
// save item
//--------------------------------------------------------------------------------
void Trouser::save(const string& ownerID, Storage storage, StorageID_t storageID, BYTE x, BYTE y)

{
    __BEGIN_TRY

    string optionField;
    setOptionTypeToField(getOptionTypeList(), optionField);

    defaultItemObjectRepository().updateGear(GEAR_TROUSER, m_ObjectID, getItemType(), ownerID, (int)storage, storageID,
                                             (int)x, (int)y, optionField, getDurability(), (int)getGrade(),
                                             (int)getEnchantLevel(), m_ItemID);

    __END_CATCH
}


//--------------------------------------------------------------------------------
// get debug string
//--------------------------------------------------------------------------------
string Trouser::toString() const

{
    StringStream msg;

    msg << "Trouser(" << "ItemID:" << m_ItemID << ",ItemType:" << (int)getItemType()
        << ",OptionType:" << getOptionTypeToString(getOptionTypeList()).c_str()
        << ",Durability:" << (int)getDurability() << ",EnchantLevel:" << (int)getEnchantLevel() << ")";

    return msg.toString();
}


//--------------------------------------------------------------------------------
// get debug string
//--------------------------------------------------------------------------------
string TrouserInfo::toString() const

{
    StringStream msg;

    msg << "TrouserInfo(" << "ItemType:" << m_ItemType << ",Name:" << m_Name << ",EName:" << m_EName
        << ",Price:" << m_Price << ",VolumeType:" << Volume2String[m_VolumeType] << ",Weight:" << m_Weight
        << ",Description:" << m_Description << ",Durability:" << m_Durability << ",DefenseBonus:" << m_DefenseBonus
        << ",ProtectionBonus:" << m_ProtectionBonus << ")";

    return msg.toString();
}


//--------------------------------------------------------------------------------
// load from DB
//--------------------------------------------------------------------------------
void TrouserInfoManager::load()

{
    __BEGIN_TRY

    m_InfoCount = defaultItemObjectRepository().loadMaxGearType(GEAR_TROUSER);

    m_pItemInfos = new ItemInfo*[m_InfoCount + 1];

    for (uint i = 0; i <= m_InfoCount; i++)
        m_pItemInfos[i] = NULL;

    vector<GearInfoRow> rows = defaultItemObjectRepository().loadGearInfos(GEAR_TROUSER);

    for (size_t r = 0; r < rows.size(); r++) {
        TrouserInfo* pTrouserInfo = new TrouserInfo();

        pTrouserInfo->setItemType(rows[r].itemType);
        pTrouserInfo->setName(rows[r].name);
        pTrouserInfo->setEName(rows[r].ename);
        pTrouserInfo->setPrice(rows[r].price);
        pTrouserInfo->setVolumeType(rows[r].volume);
        pTrouserInfo->setWeight(rows[r].weight);
        pTrouserInfo->setRatio(rows[r].ratio);
        pTrouserInfo->setDurability(rows[r].durability);
        pTrouserInfo->setDefenseBonus(rows[r].defense);
        pTrouserInfo->setProtectionBonus(rows[r].protection);
        pTrouserInfo->setReqAbility(rows[r].reqAbility);
        pTrouserInfo->setItemLevel(rows[r].itemLevel);
        pTrouserInfo->setDefaultOptions(rows[r].defaultOption);
        pTrouserInfo->setUpgradeRatio(rows[r].upgradeRatio);
        pTrouserInfo->setUpgradeCrashPercent(rows[r].upgradeCrashPercent);
        pTrouserInfo->setNextOptionRatio(rows[r].nextOptionRatio);
        pTrouserInfo->setNextItemType(rows[r].nextItemType);
        pTrouserInfo->setDowngradeRatio(rows[r].downgradeRatio);

        addItemInfo(pTrouserInfo);
    }

    __END_CATCH
}


//--------------------------------------------------------------------------------
// load to creature
//--------------------------------------------------------------------------------
void TrouserLoader::load(Creature* pCreature)

{
    __BEGIN_TRY

    Assert(pCreature != NULL);

    vector<GearObjectRow> rows = defaultItemObjectRepository().loadGearOfOwner(GEAR_TROUSER, pCreature->getName());

    for (size_t r = 0; r < rows.size(); r++) {
        try {
            Trouser* pTrouser = new Trouser();

            pTrouser->setItemID(rows[r].itemID);
            pTrouser->setObjectID(rows[r].objectID);
            pTrouser->setItemType(rows[r].itemType);

            if (de::gameContext()
                    .itemInfos()
                    .getItemInfo(Item::ITEM_CLASS_TROUSER, pTrouser->getItemType())
                    ->isUnique())
                pTrouser->setUnique();

            Storage storage = (Storage)rows[r].storage;
            BYTE x = rows[r].x;
            BYTE y = rows[r].y;

            string optionField = rows[r].optionField;
            list<OptionType_t> optionTypes;
            setOptionTypeFromField(optionTypes, optionField);
            pTrouser->setOptionType(optionTypes);

            pTrouser->setDurability(rows[r].durability);
            pTrouser->setGrade(rows[r].grade);
            pTrouser->setEnchantLevel(rows[r].enchantLevel);
            pTrouser->setCreateType((Item::CreateType)rows[r].createType);

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
                if (pInventory->canAddingEx(x, y, pTrouser)) {
                    pInventory->addItemEx(x, y, pTrouser);
                } else {
                    processItemBugEx(pCreature, pTrouser);
                }
                break;

            case STORAGE_GEAR:
                if (pCreature->isSlayer()) {
                    if (!pSlayer->isWear((Slayer::WearPart)x)) {
                        pSlayer->wearItem((Slayer::WearPart)x, pTrouser);
                    } else {
                        processItemBugEx(pCreature, pTrouser);
                    }
                } else if (pCreature->isVampire()) {
                    processItemBugEx(pCreature, pTrouser);
                }
                break;

            case STORAGE_BELT:
                processItemBugEx(pCreature, pTrouser);
                break;

            case STORAGE_EXTRASLOT:
                if (pCreature->isSlayer())
                    pSlayer->addItemToExtraInventorySlot(pTrouser);
                else if (pCreature->isVampire())
                    pVampire->addItemToExtraInventorySlot(pTrouser);
                break;

            case STORAGE_MOTORCYCLE:
                processItemBugEx(pCreature, pTrouser);
                break;

            case STORAGE_STASH:
                if (pStash->isExist(x, y)) {
                    processItemBugEx(pCreature, pTrouser);
                } else
                    pStash->insert(x, y, pTrouser);
                break;

            case STORAGE_GARBAGE:
                processItemBug(pCreature, pTrouser);
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
void TrouserLoader::load(Zone* pZone)

{
    __BEGIN_TRY

    Assert(pZone != NULL);

    vector<GearZoneObjectRow> rows =
        defaultItemObjectRepository().loadGearInZone(GEAR_TROUSER, (int)STORAGE_ZONE, pZone->getZoneID());

    for (size_t r = 0; r < rows.size(); r++) {
        Trouser* pTrouser = new Trouser();

        pTrouser->setItemID(rows[r].itemID);
        pTrouser->setObjectID(rows[r].objectID);
        pTrouser->setItemType(rows[r].itemType);

        Storage storage = (Storage)rows[r].storage;
        BYTE x = rows[r].x;
        BYTE y = rows[r].y;

        string optionField = rows[r].optionField;
        list<OptionType_t> optionTypes;
        setOptionTypeFromField(optionTypes, optionField);
        pTrouser->setOptionType(optionTypes);

        pTrouser->setDurability(rows[r].durability);
        pTrouser->setEnchantLevel(rows[r].enchantLevel);
        pTrouser->setCreateType((Item::CreateType)rows[r].createType);

        switch (storage) {
        case STORAGE_ZONE: {
            Tile& pTile = pZone->getTile(x, y);
            Assert(!pTile.hasItem());
            pTile.addItem(pTrouser);
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
void TrouserLoader::load(StorageID_t storageID, Inventory* pInventory)

{
    __BEGIN_TRY

    Statement* pStmt;

    BEGIN_DB {}
    END_DB(pStmt)

    __END_CATCH
}
