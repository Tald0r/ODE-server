//////////////////////////////////////////////////////////////////////////////
// Filename    : Necklace.cpp
// Written By  : Elca
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "Necklace.h"

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

ItemID_t Necklace::m_ItemIDRegistry = 0;
Mutex Necklace::m_Mutex;

//--------------------------------------------------------------------------------
// constructor
//--------------------------------------------------------------------------------
Necklace::Necklace()

//: m_ItemType(0), m_Durability(0)
{
    setItemType(0);
    setDurability(0);
}

Necklace::Necklace(ItemType_t itemType, const list<OptionType_t>& optionType)

//: m_ItemType(itemType), m_OptionType(optionType), m_Durability(0)
{
    setItemType(itemType);
    setOptionType(optionType);


    setDurability(computeMaxDurability(this));

    if (!de::gameContext().itemInfos().isPossibleItem(getItemClass(), getItemType(), getOptionTypeList())) {
        filelog("itembug.log", "Necklace::Necklace() : Invalid item type or option type");
        throw Error("Necklace::Necklace() : Invalid item type or optionType");
    }
}


//--------------------------------------------------------------------------------
// create item
//--------------------------------------------------------------------------------
void Necklace::create(const string& ownerID, Storage storage, StorageID_t storageID, BYTE x, BYTE y, ItemID_t itemID)

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

    defaultItemObjectRepository().insertGear(GEAR_NECKLACE, m_ItemID, m_ObjectID, getItemType(), ownerID, (int)storage,
                                             storageID, (int)x, (int)y, optionField, getDurability(), getGrade(),
                                             (int)m_CreateType);

    __END_CATCH
}


//--------------------------------------------------------------------------------
// save item
//--------------------------------------------------------------------------------
void Necklace::tinysave(const char* field) const

{
    __BEGIN_TRY

    defaultItemObjectRepository().tinysaveGear(GEAR_NECKLACE, field, m_ItemID);

    __END_CATCH
}

//--------------------------------------------------------------------------------
// save item
//--------------------------------------------------------------------------------
void Necklace::save(const string& ownerID, Storage storage, StorageID_t storageID, BYTE x, BYTE y)

{
    __BEGIN_TRY

    string optionField;
    setOptionTypeToField(getOptionTypeList(), optionField);

    defaultItemObjectRepository().updateGear(GEAR_NECKLACE, m_ObjectID, getItemType(), ownerID, (int)storage, storageID,
                                             (int)x, (int)y, optionField, getDurability(), getGrade(),
                                             (int)getEnchantLevel(), m_ItemID);

    __END_CATCH
}


//--------------------------------------------------------------------------------
// get debug string
//--------------------------------------------------------------------------------
string Necklace::toString() const

{
    StringStream msg;

    msg << "Necklace(" << "ItemID:" << m_ItemID << ",ItemType:" << (int)getItemType()
        << ",OptionType:" << getOptionTypeToString(getOptionTypeList()).c_str()
        << ",Durability:" << (int)getDurability() << ",EnchantLevel:" << (int)getEnchantLevel() << ")";

    return msg.toString();
}


//--------------------------------------------------------------------------------
// get debug string
//--------------------------------------------------------------------------------
string NecklaceInfo::toString() const

{
    StringStream msg;

    msg << "NecklaceInfo(" << "ItemType:" << m_ItemType << ",Name:" << m_Name << ",EName:" << m_EName
        << ",Price:" << m_Price << ",VolumeType:" << Volume2String[m_VolumeType] << ",Weight:" << m_Weight
        << ",Description:" << m_Description << ",Durability:" << m_Durability << ",DefenseBonus:" << m_DefenseBonus
        << ")";

    return msg.toString();
}


//--------------------------------------------------------------------------------
// load from DB
//--------------------------------------------------------------------------------
void NecklaceInfoManager::load()

{
    __BEGIN_TRY

    m_InfoCount = defaultItemObjectRepository().loadMaxGearType(GEAR_NECKLACE);

    m_pItemInfos = new ItemInfo*[m_InfoCount + 1];

    for (uint i = 0; i <= m_InfoCount; i++)
        m_pItemInfos[i] = NULL;

    vector<GearInfoRow> rows = defaultItemObjectRepository().loadGearInfos(GEAR_NECKLACE);

    for (size_t r = 0; r < rows.size(); r++) {
        NecklaceInfo* pNecklaceInfo = new NecklaceInfo();

        pNecklaceInfo->setItemType(rows[r].itemType);
        pNecklaceInfo->setName(rows[r].name);
        pNecklaceInfo->setEName(rows[r].ename);
        pNecklaceInfo->setPrice(rows[r].price);
        pNecklaceInfo->setVolumeType(rows[r].volume);
        pNecklaceInfo->setWeight(rows[r].weight);
        pNecklaceInfo->setRatio(rows[r].ratio);
        pNecklaceInfo->setDurability(rows[r].durability);
        pNecklaceInfo->setDefenseBonus(rows[r].defense);
        pNecklaceInfo->setProtectionBonus(rows[r].protection);
        pNecklaceInfo->setReqAbility(rows[r].reqAbility);
        pNecklaceInfo->setItemLevel(rows[r].itemLevel);
        pNecklaceInfo->setDefaultOptions(rows[r].defaultOption);
        pNecklaceInfo->setUpgradeRatio(rows[r].upgradeRatio);
        pNecklaceInfo->setUpgradeCrashPercent(rows[r].upgradeCrashPercent);
        pNecklaceInfo->setNextOptionRatio(rows[r].nextOptionRatio);
        pNecklaceInfo->setNextItemType(rows[r].nextItemType);
        pNecklaceInfo->setDowngradeRatio(rows[r].downgradeRatio);

        addItemInfo(pNecklaceInfo);
    }

    __END_CATCH
}


//--------------------------------------------------------------------------------
// load to creature
//--------------------------------------------------------------------------------
void NecklaceLoader::load(Creature* pCreature)

{
    __BEGIN_TRY

    Assert(pCreature != NULL);

    vector<GearObjectRow> rows = defaultItemObjectRepository().loadGearOfOwner(GEAR_NECKLACE, pCreature->getName());

    for (size_t r = 0; r < rows.size(); r++) {
        try {
            Necklace* pNecklace = new Necklace();

            pNecklace->setItemID(rows[r].itemID);
            pNecklace->setObjectID(rows[r].objectID);
            pNecklace->setItemType(rows[r].itemType);

            if (de::gameContext()
                    .itemInfos()
                    .getItemInfo(Item::ITEM_CLASS_NECKLACE, pNecklace->getItemType())
                    ->isUnique())
                pNecklace->setUnique();

            Storage storage = (Storage)rows[r].storage;
            BYTE x = rows[r].x;
            BYTE y = rows[r].y;

            string optionField = rows[r].optionField;
            list<OptionType_t> optionTypes;
            setOptionTypeFromField(optionTypes, optionField);
            pNecklace->setOptionType(optionTypes);

            pNecklace->setDurability(rows[r].durability);
            pNecklace->setGrade(rows[r].grade);
            pNecklace->setEnchantLevel(rows[r].enchantLevel);
            pNecklace->setCreateType((Item::CreateType)rows[r].createType);

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
                if (pInventory->canAddingEx(x, y, pNecklace)) {
                    pInventory->addItemEx(x, y, pNecklace);
                } else {
                    processItemBugEx(pCreature, pNecklace);
                }
                break;

            case STORAGE_GEAR:
                if (pCreature->isSlayer()) {
                    if (!pSlayer->isWear((Slayer::WearPart)x)) {
                        pSlayer->wearItem((Slayer::WearPart)x, pNecklace);
                    } else {
                        processItemBugEx(pCreature, pNecklace);
                    }
                } else if (pCreature->isVampire()) {
                    processItemBugEx(pCreature, pNecklace);
                }
                break;

            case STORAGE_BELT:
                processItemBugEx(pCreature, pNecklace);
                break;

            case STORAGE_EXTRASLOT:
                if (pCreature->isSlayer())
                    pSlayer->addItemToExtraInventorySlot(pNecklace);
                else if (pCreature->isVampire())
                    pVampire->addItemToExtraInventorySlot(pNecklace);
                break;

            case STORAGE_MOTORCYCLE:
                processItemBugEx(pCreature, pNecklace);
                break;

            case STORAGE_STASH:
                if (pStash->isExist(x, y)) {
                    processItemBugEx(pCreature, pNecklace);
                } else
                    pStash->insert(x, y, pNecklace);
                break;

            case STORAGE_GARBAGE:
                processItemBug(pCreature, pNecklace);
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
void NecklaceLoader::load(Zone* pZone)

{
    __BEGIN_TRY

    Assert(pZone != NULL);

    vector<GearZoneObjectRow> rows =
        defaultItemObjectRepository().loadGearInZone(GEAR_NECKLACE, (int)STORAGE_ZONE, pZone->getZoneID());

    for (size_t r = 0; r < rows.size(); r++) {
        Necklace* pNecklace = new Necklace();

        pNecklace->setItemID(rows[r].itemID);
        pNecklace->setObjectID(rows[r].objectID);
        pNecklace->setItemType(rows[r].itemType);

        Storage storage = (Storage)rows[r].storage;
        BYTE x = rows[r].x;
        BYTE y = rows[r].y;

        string optionField = rows[r].optionField;
        list<OptionType_t> optionTypes;
        setOptionTypeFromField(optionTypes, optionField);
        pNecklace->setOptionType(optionTypes);

        pNecklace->setDurability(rows[r].durability);
        pNecklace->setEnchantLevel(rows[r].enchantLevel);
        pNecklace->setCreateType((Item::CreateType)rows[r].createType);

        switch (storage) {
        case STORAGE_ZONE: {
            Tile& pTile = pZone->getTile(x, y);
            Assert(!pTile.hasItem());
            pTile.addItem(pNecklace);
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
void NecklaceLoader::load(StorageID_t storageID, Inventory* pInventory)

{
    __BEGIN_TRY

    Statement* pStmt;

    BEGIN_DB {}
    END_DB(pStmt)

    __END_CATCH
}
