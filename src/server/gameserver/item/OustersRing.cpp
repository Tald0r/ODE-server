//////////////////////////////////////////////////////////////////////////////
// Filename    : OustersRing.cpp
// Written By  : Elca
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "OustersRing.h"

#include "Belt.h"
#include "DB.h"
#include "GameContext.h"
#include "ItemInfoManager.h"
#include "ItemUtil.h"
#include "Motorcycle.h"
#include "Ousters.h"
#include "Slayer.h"
#include "Stash.h"
#include "Vampire.h"
#include "repository/ItemObjectRepository.h"

ItemID_t OustersRing::m_ItemIDRegistry = 0;
Mutex OustersRing::m_Mutex;

//--------------------------------------------------------------------------------
// constructor
//--------------------------------------------------------------------------------
OustersRing::OustersRing()

//: m_ItemType(0), m_Durability(0)
{
    setItemType(0);
    setDurability(0);
}

OustersRing::OustersRing(ItemType_t itemType, const list<OptionType_t>& optionType)

//: m_ItemType(itemType), m_OptionType(optionType), m_Durability(0)
{
    setItemType(itemType);
    setOptionType(optionType);

    setDurability(computeMaxDurability(this));

    if (!de::gameContext().itemInfos().isPossibleItem(getItemClass(), getItemType(), getOptionTypeList())) {
        filelog("itembug.log", "OustersRing::OustersRing() : Invalid item type or option type");
        throw Error("OustersRing::OustersRing() : Invalid item type or optionType");
    }
}


//--------------------------------------------------------------------------------
// create item
//--------------------------------------------------------------------------------
void OustersRing::create(const string& ownerID, Storage storage, StorageID_t storageID, BYTE x, BYTE y, ItemID_t itemID)

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

    defaultItemObjectRepository().insertGear(GEAR_OUSTERS_RING, m_ItemID, m_ObjectID, getItemType(), ownerID,
                                             (int)storage, storageID, (int)x, (int)y, optionField, getDurability(),
                                             getGrade(), (int)m_CreateType);

    __END_CATCH
}


//--------------------------------------------------------------------------------
// save item
//--------------------------------------------------------------------------------
void OustersRing::tinysave(const char* field) const

{
    __BEGIN_TRY

    defaultItemObjectRepository().tinysaveGear(GEAR_OUSTERS_RING, field, m_ItemID);

    __END_CATCH
}

//--------------------------------------------------------------------------------
// save item
//--------------------------------------------------------------------------------
void OustersRing::save(const string& ownerID, Storage storage, StorageID_t storageID, BYTE x, BYTE y)

{
    __BEGIN_TRY

    string optionField;
    setOptionTypeToField(getOptionTypeList(), optionField);

    defaultItemObjectRepository().updateGear(GEAR_OUSTERS_RING, m_ObjectID, getItemType(), ownerID, (int)storage,
                                             storageID, (int)x, (int)y, optionField, getDurability(), getGrade(),
                                             (int)getEnchantLevel(), m_ItemID);

    __END_CATCH
}


//--------------------------------------------------------------------------------
// get debug string
//--------------------------------------------------------------------------------
string OustersRing::toString() const

{
    StringStream msg;

    msg << "OustersRing(" << "ItemID:" << m_ItemID << ",ItemType:" << (int)getItemType()
        << ",OptionType:" << getOptionTypeToString(getOptionTypeList()).c_str()
        << ",Durability:" << (int)getDurability() << ",EnchantLevel:" << (int)getEnchantLevel() << ")";

    return msg.toString();
}


//--------------------------------------------------------------------------------
// get debug string
//--------------------------------------------------------------------------------
string OustersRingInfo::toString() const

{
    StringStream msg;

    msg << "OustersRingInfo(" << "ItemType:" << m_ItemType << ",Name:" << m_Name << ",EName:" << m_EName
        << ",Price:" << m_Price << ",VolumeType:" << Volume2String[m_VolumeType] << ",Weight:" << m_Weight
        << ",Description:" << m_Description << ",Durability:" << m_Durability << ",DefenseBonus:" << m_DefenseBonus
        << ")";

    return msg.toString();
}


//--------------------------------------------------------------------------------
// load from DB
//--------------------------------------------------------------------------------
void OustersRingInfoManager::load()

{
    __BEGIN_TRY

    m_InfoCount = defaultItemObjectRepository().loadMaxGearType(GEAR_OUSTERS_RING);

    m_pItemInfos = new ItemInfo*[m_InfoCount + 1];

    for (uint i = 0; i <= m_InfoCount; i++)
        m_pItemInfos[i] = NULL;

    vector<GearInfoRow> rows = defaultItemObjectRepository().loadGearInfos(GEAR_OUSTERS_RING);

    for (size_t r = 0; r < rows.size(); r++) {
        OustersRingInfo* pOustersRingInfo = new OustersRingInfo();

        pOustersRingInfo->setItemType(rows[r].itemType);
        pOustersRingInfo->setName(rows[r].name);
        pOustersRingInfo->setEName(rows[r].ename);
        pOustersRingInfo->setPrice(rows[r].price);
        pOustersRingInfo->setVolumeType(rows[r].volume);
        pOustersRingInfo->setWeight(rows[r].weight);
        pOustersRingInfo->setRatio(rows[r].ratio);
        pOustersRingInfo->setDurability(rows[r].durability);
        pOustersRingInfo->setDefenseBonus(rows[r].defense);
        pOustersRingInfo->setProtectionBonus(rows[r].protection);
        pOustersRingInfo->setReqAbility(rows[r].reqAbility);
        pOustersRingInfo->setItemLevel(rows[r].itemLevel);
        pOustersRingInfo->setDefaultOptions(rows[r].defaultOption);
        pOustersRingInfo->setUpgradeRatio(rows[r].upgradeRatio);
        pOustersRingInfo->setUpgradeCrashPercent(rows[r].upgradeCrashPercent);
        pOustersRingInfo->setNextOptionRatio(rows[r].nextOptionRatio);
        pOustersRingInfo->setNextItemType(rows[r].nextItemType);
        pOustersRingInfo->setDowngradeRatio(rows[r].downgradeRatio);

        addItemInfo(pOustersRingInfo);
    }

    __END_CATCH
}


//--------------------------------------------------------------------------------
// load to creature
//--------------------------------------------------------------------------------
void OustersRingLoader::load(Creature* pCreature)

{
    __BEGIN_TRY

    Assert(pCreature != NULL);

    vector<GearObjectRow> rows = defaultItemObjectRepository().loadGearOfOwner(GEAR_OUSTERS_RING, pCreature->getName());

    for (size_t r = 0; r < rows.size(); r++) {
        try {
            OustersRing* pOustersRing = new OustersRing();

            pOustersRing->setItemID(rows[r].itemID);
            pOustersRing->setObjectID(rows[r].objectID);
            pOustersRing->setItemType(rows[r].itemType);

            if (de::gameContext()
                    .itemInfos()
                    .getItemInfo(Item::ITEM_CLASS_OUSTERS_RING, pOustersRing->getItemType())
                    ->isUnique())
                pOustersRing->setUnique();

            Storage storage = (Storage)rows[r].storage;
            BYTE x = rows[r].x;
            BYTE y = rows[r].y;

            string optionField = rows[r].optionField;
            list<OptionType_t> optionTypes;
            setOptionTypeFromField(optionTypes, optionField);
            pOustersRing->setOptionType(optionTypes);

            pOustersRing->setDurability(rows[r].durability);
            pOustersRing->setGrade(rows[r].grade);
            pOustersRing->setEnchantLevel(rows[r].enchantLevel);
            pOustersRing->setCreateType((Item::CreateType)rows[r].createType);

            Inventory* pInventory = NULL;
            Slayer* pSlayer = NULL;
            Vampire* pVampire = NULL;
            Ousters* pOusters = NULL;
            Stash* pStash = NULL;

            if (pCreature->isSlayer()) {
                pSlayer = dynamic_cast<Slayer*>(pCreature);
                pInventory = pSlayer->getInventory();
                pStash = pSlayer->getStash();
            } else if (pCreature->isVampire()) {
                pVampire = dynamic_cast<Vampire*>(pCreature);
                pInventory = pVampire->getInventory();
                pStash = pVampire->getStash();
            } else if (pCreature->isOusters()) {
                pOusters = dynamic_cast<Ousters*>(pCreature);
                pInventory = pOusters->getInventory();
                pStash = pOusters->getStash();
            } else
                throw UnsupportedError("Saving Monster/NPC inventories is not supported.");

            switch (storage) {
            case STORAGE_INVENTORY:
                if (pInventory->canAddingEx(x, y, pOustersRing)) {
                    pInventory->addItemEx(x, y, pOustersRing);
                } else {
                    processItemBugEx(pCreature, pOustersRing);
                }
                break;

            case STORAGE_GEAR:
                if (pCreature->isSlayer() || pCreature->isVampire()) {
                    processItemBugEx(pCreature, pOustersRing);
                } else if (pCreature->isOusters()) {
                    if (!pOusters->isWear((Ousters::WearPart)x)) {
                        pOusters->wearItem((Ousters::WearPart)x, pOustersRing);
                    } else {
                        processItemBugEx(pCreature, pOustersRing);
                    }
                }
                break;

            case STORAGE_BELT:
                processItemBugEx(pCreature, pOustersRing);
                break;

            case STORAGE_EXTRASLOT:
                if (pCreature->isSlayer())
                    pSlayer->addItemToExtraInventorySlot(pOustersRing);
                else if (pCreature->isVampire())
                    pVampire->addItemToExtraInventorySlot(pOustersRing);
                else if (pCreature->isOusters())
                    pOusters->addItemToExtraInventorySlot(pOustersRing);
                break;

            case STORAGE_MOTORCYCLE:
                processItemBugEx(pCreature, pOustersRing);
                break;

            case STORAGE_STASH:
                if (pStash->isExist(x, y)) {
                    processItemBugEx(pCreature, pOustersRing);
                } else
                    pStash->insert(x, y, pOustersRing);
                break;

            case STORAGE_GARBAGE:
                processItemBug(pCreature, pOustersRing);
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
void OustersRingLoader::load(Zone* pZone)

{
    __BEGIN_TRY

    Assert(pZone != NULL);

    vector<GearZoneObjectRow> rows =
        defaultItemObjectRepository().loadGearInZone(GEAR_OUSTERS_RING, (int)STORAGE_ZONE, pZone->getZoneID());

    for (size_t r = 0; r < rows.size(); r++) {
        OustersRing* pOustersRing = new OustersRing();

        pOustersRing->setItemID(rows[r].itemID);
        pOustersRing->setObjectID(rows[r].objectID);
        pOustersRing->setItemType(rows[r].itemType);

        Storage storage = (Storage)rows[r].storage;
        BYTE x = rows[r].x;
        BYTE y = rows[r].y;

        string optionField = rows[r].optionField;
        list<OptionType_t> optionTypes;
        setOptionTypeFromField(optionTypes, optionField);
        pOustersRing->setOptionType(optionTypes);

        pOustersRing->setDurability(rows[r].durability);
        pOustersRing->setEnchantLevel(rows[r].enchantLevel);
        pOustersRing->setCreateType((Item::CreateType)rows[r].createType);

        switch (storage) {
        case STORAGE_ZONE: {
            Tile& pTile = pZone->getTile(x, y);
            Assert(!pTile.hasItem());
            pTile.addItem(pOustersRing);
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
void OustersRingLoader::load(StorageID_t storageID, Inventory* pInventory)

{
    __BEGIN_TRY

    Statement* pStmt;

    BEGIN_DB {}
    END_DB(pStmt)

    __END_CATCH
}
