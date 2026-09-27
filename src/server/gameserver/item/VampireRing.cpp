//////////////////////////////////////////////////////////////////////////////
// Filename    : VampireRing.cpp
// Written By  : Elca
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "VampireRing.h"

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

ItemID_t VampireRing::m_ItemIDRegistry = 0;
Mutex VampireRing::m_Mutex;

//--------------------------------------------------------------------------------
// constructor
//--------------------------------------------------------------------------------
VampireRing::VampireRing()

//: m_ItemType(0), m_Durability(0)
{
    setItemType(0);
    setDurability(0);
}

VampireRing::VampireRing(ItemType_t itemType, const list<OptionType_t>& optionType)

//: m_ItemType(itemType), m_OptionType(optionType), m_Durability(0)
{
    setItemType(itemType);
    setOptionType(optionType);

    setDurability(computeMaxDurability(this));

    if (!de::gameContext().itemInfos().isPossibleItem(getItemClass(), getItemType(), getOptionTypeList())) {
        filelog("itembug.log", "VampireRing::VampireRing() : Invalid item type or option type");
        throw Error("VampireRing::VampireRing() : Invalid item type or optionType");
    }
}


//--------------------------------------------------------------------------------
// create item
//--------------------------------------------------------------------------------
void VampireRing::create(const string& ownerID, Storage storage, StorageID_t storageID, BYTE x, BYTE y, ItemID_t itemID)

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

    defaultItemObjectRepository().insertGear(GEAR_VAMPIRE_RING, m_ItemID, m_ObjectID, getItemType(), ownerID,
                                             (int)storage, storageID, (int)x, (int)y, optionField, getDurability(),
                                             getGrade(), (int)m_CreateType);

    __END_CATCH
}


//--------------------------------------------------------------------------------
// save item
//--------------------------------------------------------------------------------
void VampireRing::tinysave(const char* field) const

{
    __BEGIN_TRY

    defaultItemObjectRepository().tinysaveGear(GEAR_VAMPIRE_RING, field, m_ItemID);

    __END_CATCH
}

//--------------------------------------------------------------------------------
// save item
//--------------------------------------------------------------------------------
void VampireRing::save(const string& ownerID, Storage storage, StorageID_t storageID, BYTE x, BYTE y)

{
    __BEGIN_TRY

    string optionField;
    setOptionTypeToField(getOptionTypeList(), optionField);

    defaultItemObjectRepository().updateGear(GEAR_VAMPIRE_RING, m_ObjectID, getItemType(), ownerID, (int)storage,
                                             storageID, (int)x, (int)y, optionField, getDurability(), getGrade(),
                                             (int)getEnchantLevel(), m_ItemID);

    __END_CATCH
}


//--------------------------------------------------------------------------------
// get debug string
//--------------------------------------------------------------------------------
string VampireRing::toString() const

{
    StringStream msg;

    msg << "VampireRing(" << "ItemID:" << m_ItemID << ",ItemType:" << (int)getItemType()
        << ",OptionType:" << getOptionTypeToString(getOptionTypeList()).c_str()
        << ",Durability:" << (int)getDurability() << ",EnchantLevel:" << (int)getEnchantLevel() << ")";

    return msg.toString();
}


//--------------------------------------------------------------------------------
// get debug string
//--------------------------------------------------------------------------------
string VampireRingInfo::toString() const

{
    StringStream msg;

    msg << "VampireRingInfo(" << "ItemType:" << m_ItemType << ",Name:" << m_Name << ",EName:" << m_EName
        << ",Price:" << m_Price << ",VolumeType:" << Volume2String[m_VolumeType] << ",Weight:" << m_Weight
        << ",Description:" << m_Description << ",Durability:" << m_Durability << ",DefenseBonus:" << m_DefenseBonus
        << ")";

    return msg.toString();
}


//--------------------------------------------------------------------------------
// load from DB
//--------------------------------------------------------------------------------
void VampireRingInfoManager::load()

{
    __BEGIN_TRY

    m_InfoCount = defaultItemObjectRepository().loadMaxGearType(GEAR_VAMPIRE_RING);

    m_pItemInfos = new ItemInfo*[m_InfoCount + 1];

    for (uint i = 0; i <= m_InfoCount; i++)
        m_pItemInfos[i] = NULL;

    vector<GearInfoRow> rows = defaultItemObjectRepository().loadGearInfos(GEAR_VAMPIRE_RING);

    for (size_t r = 0; r < rows.size(); r++) {
        VampireRingInfo* pVampireRingInfo = new VampireRingInfo();

        pVampireRingInfo->setItemType(rows[r].itemType);
        pVampireRingInfo->setName(rows[r].name);
        pVampireRingInfo->setEName(rows[r].ename);
        pVampireRingInfo->setPrice(rows[r].price);
        pVampireRingInfo->setVolumeType(rows[r].volume);
        pVampireRingInfo->setWeight(rows[r].weight);
        pVampireRingInfo->setRatio(rows[r].ratio);
        pVampireRingInfo->setDurability(rows[r].durability);
        pVampireRingInfo->setDefenseBonus(rows[r].defense);
        pVampireRingInfo->setProtectionBonus(rows[r].protection);
        pVampireRingInfo->setReqAbility(rows[r].reqAbility);
        pVampireRingInfo->setItemLevel(rows[r].itemLevel);
        pVampireRingInfo->setDefaultOptions(rows[r].defaultOption);
        pVampireRingInfo->setUpgradeRatio(rows[r].upgradeRatio);
        pVampireRingInfo->setUpgradeCrashPercent(rows[r].upgradeCrashPercent);
        pVampireRingInfo->setNextOptionRatio(rows[r].nextOptionRatio);
        pVampireRingInfo->setNextItemType(rows[r].nextItemType);
        pVampireRingInfo->setDowngradeRatio(rows[r].downgradeRatio);

        addItemInfo(pVampireRingInfo);
    }

    __END_CATCH
}


//--------------------------------------------------------------------------------
// load to creature
//--------------------------------------------------------------------------------
void VampireRingLoader::load(Creature* pCreature)

{
    __BEGIN_TRY

    Assert(pCreature != NULL);

    vector<GearObjectRow> rows = defaultItemObjectRepository().loadGearOfOwner(GEAR_VAMPIRE_RING, pCreature->getName());

    for (size_t r = 0; r < rows.size(); r++) {
        try {
            VampireRing* pVampireRing = new VampireRing();

            pVampireRing->setItemID(rows[r].itemID);
            pVampireRing->setObjectID(rows[r].objectID);
            pVampireRing->setItemType(rows[r].itemType);

            if (de::gameContext()
                    .itemInfos()
                    .getItemInfo(Item::ITEM_CLASS_VAMPIRE_RING, pVampireRing->getItemType())
                    ->isUnique())
                pVampireRing->setUnique();

            Storage storage = (Storage)rows[r].storage;
            BYTE x = rows[r].x;
            BYTE y = rows[r].y;

            string optionField = rows[r].optionField;
            list<OptionType_t> optionTypes;
            setOptionTypeFromField(optionTypes, optionField);
            pVampireRing->setOptionType(optionTypes);

            pVampireRing->setDurability(rows[r].durability);
            pVampireRing->setGrade(rows[r].grade);
            pVampireRing->setEnchantLevel(rows[r].enchantLevel);
            pVampireRing->setCreateType((Item::CreateType)rows[r].createType);

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
                if (pInventory->canAddingEx(x, y, pVampireRing)) {
                    pInventory->addItemEx(x, y, pVampireRing);
                } else {
                    processItemBugEx(pCreature, pVampireRing);
                }
                break;

            case STORAGE_GEAR:
                if (pCreature->isSlayer()) {
                    processItemBugEx(pCreature, pVampireRing);
                } else if (pCreature->isVampire()) {
                    if (!pVampire->isWear((Vampire::WearPart)x)) {
                        pVampire->wearItem((Vampire::WearPart)x, pVampireRing);
                    } else {
                        processItemBugEx(pCreature, pVampireRing);
                    }
                }
                break;

            case STORAGE_BELT:
                processItemBugEx(pCreature, pVampireRing);
                break;

            case STORAGE_EXTRASLOT:
                if (pCreature->isSlayer())
                    pSlayer->addItemToExtraInventorySlot(pVampireRing);
                else if (pCreature->isVampire())
                    pVampire->addItemToExtraInventorySlot(pVampireRing);
                break;

            case STORAGE_MOTORCYCLE:
                processItemBugEx(pCreature, pVampireRing);
                break;

            case STORAGE_STASH:
                if (pStash->isExist(x, y)) {
                    processItemBugEx(pCreature, pVampireRing);
                } else
                    pStash->insert(x, y, pVampireRing);
                break;

            case STORAGE_GARBAGE:
                processItemBug(pCreature, pVampireRing);
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
void VampireRingLoader::load(Zone* pZone)

{
    __BEGIN_TRY

    Assert(pZone != NULL);

    vector<GearZoneObjectRow> rows =
        defaultItemObjectRepository().loadGearInZone(GEAR_VAMPIRE_RING, (int)STORAGE_ZONE, pZone->getZoneID());

    for (size_t r = 0; r < rows.size(); r++) {
        VampireRing* pVampireRing = new VampireRing();

        pVampireRing->setItemID(rows[r].itemID);
        pVampireRing->setObjectID(rows[r].objectID);
        pVampireRing->setItemType(rows[r].itemType);

        Storage storage = (Storage)rows[r].storage;
        BYTE x = rows[r].x;
        BYTE y = rows[r].y;

        string optionField = rows[r].optionField;
        list<OptionType_t> optionTypes;
        setOptionTypeFromField(optionTypes, optionField);
        pVampireRing->setOptionType(optionTypes);

        pVampireRing->setDurability(rows[r].durability);
        pVampireRing->setEnchantLevel(rows[r].enchantLevel);
        pVampireRing->setCreateType((Item::CreateType)rows[r].createType);

        switch (storage) {
        case STORAGE_ZONE: {
            Tile& pTile = pZone->getTile(x, y);
            Assert(!pTile.hasItem());
            pTile.addItem(pVampireRing);
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
void VampireRingLoader::load(StorageID_t storageID, Inventory* pInventory)

{
    __BEGIN_TRY

    Statement* pStmt;

    BEGIN_DB {}
    END_DB(pStmt)

    __END_CATCH
}
