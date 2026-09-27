//////////////////////////////////////////////////////////////////////////////
// Filename    : OustersArmsband.cpp
// Written By  : Elca
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "OustersArmsband.h"

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

ItemID_t OustersArmsband::m_ItemIDRegistry = 0;
Mutex OustersArmsband::m_Mutex;

//--------------------------------------------------------------------------------
// constructor
//--------------------------------------------------------------------------------
OustersArmsband::OustersArmsband()

//: m_ItemType(0), m_Durability(0), m_pInventory(NULL)
{
    setItemType(0);
    setDurability(0);
    m_pInventory = NULL;
}

OustersArmsband::OustersArmsband(ItemType_t itemType, const list<OptionType_t>& optionType)

//: m_ItemType(itemType), m_OptionType(optionType), m_Durability(0), m_pInventory(NULL)
{
    setItemType(itemType);
    setOptionType(optionType);
    OustersArmsbandInfo* pOustersArmsbandInfo = dynamic_cast<OustersArmsbandInfo*>(
        de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_OUSTERS_ARMSBAND, getItemType()));

    m_pInventory = new Inventory(pOustersArmsbandInfo->getPocketCount(), 1);


    setDurability(computeMaxDurability(this));

    if (!de::gameContext().itemInfos().isPossibleItem(getItemClass(), getItemType(), getOptionTypeList())) {
        filelog("itembug.log", "OustersArmsband::OustersArmsband() : Invalid item type or option type");
        throw Error("OustersArmsband::OustersArmsband() : Invalid item type or optionType");
    }
}

//--------------------------------------------------------------------------------
// destructor
//--------------------------------------------------------------------------------
OustersArmsband::~OustersArmsband()

{
    SAFE_DELETE(m_pInventory);
}

//--------------------------------------------------------------------------------
// create item
//--------------------------------------------------------------------------------
void OustersArmsband::create(const string& ownerID, Storage storage, StorageID_t storageID, BYTE x, BYTE y,
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

    defaultItemObjectRepository().insertGear(GEAR_OUSTERS_ARMSBAND, m_ItemID, m_ObjectID, getItemType(), ownerID,
                                             (int)storage, storageID, (int)x, (int)y, optionField, getDurability(),
                                             getGrade(), (int)m_CreateType);

    __END_CATCH
}


//--------------------------------------------------------------------------------
// destroy item
//--------------------------------------------------------------------------------
bool OustersArmsband::destroy()

{
    __BEGIN_TRY

    //-------------------------------------------------------
    // If any items are left in the armsband, the items inside would have to be
    // destroyed as well.
    // For an armsband the items are already deleted by the delete above, so they
    // are not deleted here.
    // If the belt is no longer needed it has to be deleted above.
    //-------------------------------------------------------
    for (int i = 0; i < m_pInventory->getHeight(); i++) {
        for (int j = 0; j < m_pInventory->getWidth(); j++) {
            Item* pItem = m_pInventory->getItem(j, i);
            if (pItem != NULL) {
                pItem->destroy();
            }
        }
    }

    if (!defaultItemObjectRepository().destroyGearObject(GEAR_OUSTERS_ARMSBAND, m_ItemID))
        return false;

    __END_CATCH

    return true;
}


//--------------------------------------------------------------------------------
// save item
//--------------------------------------------------------------------------------
void OustersArmsband::tinysave(const char* field) const

{
    __BEGIN_TRY

    defaultItemObjectRepository().tinysaveGear(GEAR_OUSTERS_ARMSBAND, field, m_ItemID);

    __END_CATCH
}

//--------------------------------------------------------------------------------
// save item
//--------------------------------------------------------------------------------
void OustersArmsband::save(const string& ownerID, Storage storage, StorageID_t storageID, BYTE x, BYTE y)

{
    __BEGIN_TRY

    string optionField;
    setOptionTypeToField(getOptionTypeList(), optionField);

    defaultItemObjectRepository().updateGear(GEAR_OUSTERS_ARMSBAND, m_ObjectID, getItemType(), ownerID, (int)storage,
                                             storageID, (int)x, (int)y, optionField, getDurability(), getGrade(),
                                             (int)getEnchantLevel(), m_ItemID);

    // Take the items out one by one and UPDATE each of them right away.
    for (int i = 0; i < m_pInventory->getHeight(); i++) {
        for (int j = 0; j < m_pInventory->getWidth(); j++) {
            Item* pItem = m_pInventory->getItem(j, 0);
            if (pItem != NULL) {
                pItem->save(ownerID, STORAGE_BELT, m_ItemID, j, 0);
            }
        }
    }

    __END_CATCH
}

void OustersArmsband::makePCItemInfo(PCItemInfo& result) const {
    Item::makePCItemInfo(result);

    BYTE SubItemCount = 0;

    // Read as many item infos as the belt has pockets.
    for (int i = 0; i < getPocketCount(); i++) {
        Item* pOustersArmsbandItem = getInventory()->getItem(i, 0);

        if (pOustersArmsbandItem != NULL) {
            SubItemInfo* pSubItemInfo = new SubItemInfo();
            pSubItemInfo->setObjectID(pOustersArmsbandItem->getObjectID());
            pSubItemInfo->setItemClass(pOustersArmsbandItem->getItemClass());
            pSubItemInfo->setItemType(pOustersArmsbandItem->getItemType());
            pSubItemInfo->setItemNum(pOustersArmsbandItem->getNum());
            pSubItemInfo->setSlotID(i);

            result.addListElement(pSubItemInfo);

            SubItemCount++;
        }
    }

    result.setListNum(SubItemCount);
}

//--------------------------------------------------------------------------------
// get debug string
//--------------------------------------------------------------------------------
string OustersArmsband::toString() const

{
    StringStream msg;

    msg << "OustersArmsband(" << "ItemID:" << m_ItemID << ",ItemType:" << (int)getItemType()
        << ",OptionType:" << getOptionTypeToString(getOptionTypeList()).c_str()
        << ",Durability:" << (int)getDurability() << ",EnchantLevel:" << (int)getEnchantLevel() << ")";

    return msg.toString();
}


//--------------------------------------------------------------------------------
// get pocket count
//--------------------------------------------------------------------------------
PocketNum_t OustersArmsband::getPocketCount(void) const

{
    __BEGIN_TRY

    OustersArmsbandInfo* pOustersArmsbandInfo = dynamic_cast<OustersArmsbandInfo*>(
        de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_OUSTERS_ARMSBAND, getItemType()));
    Assert(pOustersArmsbandInfo != NULL);
    return pOustersArmsbandInfo->getPocketCount();

    __END_CATCH
}
//--------------------------------------------------------------------------------
// get debug string
//--------------------------------------------------------------------------------
string OustersArmsbandInfo::toString() const

{
    StringStream msg;

    msg << "OustersArmsbandInfo(" << "ItemType:" << m_ItemType << ",Name:" << m_Name << ",EName:" << m_EName
        << ",Price:" << m_Price << ",VolumeType:" << Volume2String[m_VolumeType] << ",Weight:" << m_Weight
        << ",Description:" << m_Description << ",Durability:" << m_Durability << ",DefenseBonus:" << m_DefenseBonus
        << ")";

    return msg.toString();
}


//--------------------------------------------------------------------------------
// load from DB
//--------------------------------------------------------------------------------
void OustersArmsbandInfoManager::load()

{
    __BEGIN_TRY

    m_InfoCount = defaultItemObjectRepository().loadMaxGearType(GEAR_OUSTERS_ARMSBAND);

    m_pItemInfos = new ItemInfo*[m_InfoCount + 1];

    for (uint i = 0; i <= m_InfoCount; i++)
        m_pItemInfos[i] = NULL;

    vector<PocketInfoRow> rows = defaultItemObjectRepository().loadPocketInfos(GEAR_OUSTERS_ARMSBAND);

    for (size_t r = 0; r < rows.size(); r++) {
        OustersArmsbandInfo* pOustersArmsbandInfo = new OustersArmsbandInfo();

        pOustersArmsbandInfo->setItemType(rows[r].itemType);
        pOustersArmsbandInfo->setName(rows[r].name);
        pOustersArmsbandInfo->setEName(rows[r].ename);
        pOustersArmsbandInfo->setPrice(rows[r].price);
        pOustersArmsbandInfo->setVolumeType(rows[r].volume);
        pOustersArmsbandInfo->setWeight(rows[r].weight);
        pOustersArmsbandInfo->setRatio(rows[r].ratio);
        pOustersArmsbandInfo->setDurability(rows[r].durability);
        pOustersArmsbandInfo->setDefenseBonus(rows[r].defense);
        pOustersArmsbandInfo->setProtectionBonus(rows[r].protection);
        pOustersArmsbandInfo->setPocketCount(rows[r].pocketCount);
        pOustersArmsbandInfo->setReqAbility(rows[r].reqAbility);
        pOustersArmsbandInfo->setItemLevel(rows[r].itemLevel);
        pOustersArmsbandInfo->setDefaultOptions(rows[r].defaultOption);
        pOustersArmsbandInfo->setUpgradeRatio(rows[r].upgradeRatio);
        pOustersArmsbandInfo->setUpgradeCrashPercent(rows[r].upgradeCrashPercent);
        pOustersArmsbandInfo->setNextOptionRatio(rows[r].nextOptionRatio);
        pOustersArmsbandInfo->setNextItemType(rows[r].nextItemType);
        pOustersArmsbandInfo->setDowngradeRatio(rows[r].downgradeRatio);

        addItemInfo(pOustersArmsbandInfo);
    }

    __END_CATCH
}


//--------------------------------------------------------------------------------
// load to creature
//--------------------------------------------------------------------------------
void OustersArmsbandLoader::load(Creature* pCreature)

{
    __BEGIN_TRY

    Assert(pCreature != NULL);

    vector<GearObjectRow> rows =
        defaultItemObjectRepository().loadGearOfOwner(GEAR_OUSTERS_ARMSBAND, pCreature->getName());

    for (size_t r = 0; r < rows.size(); r++) {
        try {
            OustersArmsband* pOustersArmsband = new OustersArmsband();

            pOustersArmsband->setItemID(rows[r].itemID);
            pOustersArmsband->setObjectID(rows[r].objectID);
            pOustersArmsband->setItemType(rows[r].itemType);

            if (de::gameContext()
                    .itemInfos()
                    .getItemInfo(Item::ITEM_CLASS_OUSTERS_ARMSBAND, pOustersArmsband->getItemType())
                    ->isUnique())
                pOustersArmsband->setUnique();

            OustersArmsbandInfo* pOustersArmsbandInfo =
                dynamic_cast<OustersArmsbandInfo*>(de::gameContext().itemInfos().getItemInfo(
                    Item::ITEM_CLASS_OUSTERS_ARMSBAND, pOustersArmsband->getItemType()));
            Inventory* pOustersArmsbandInventory = new Inventory(pOustersArmsbandInfo->getPocketCount(), 1);

            pOustersArmsband->setInventory(pOustersArmsbandInventory);

            Storage storage = (Storage)rows[r].storage;
            BYTE x = rows[r].x;
            BYTE y = rows[r].y;

            string optionField = rows[r].optionField;
            list<OptionType_t> optionTypes;
            setOptionTypeFromField(optionTypes, optionField);
            pOustersArmsband->setOptionType(optionTypes);

            pOustersArmsband->setDurability(rows[r].durability);
            pOustersArmsband->setGrade(rows[r].grade);
            pOustersArmsband->setEnchantLevel(rows[r].enchantLevel);
            pOustersArmsband->setCreateType((Item::CreateType)rows[r].createType);

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
                if (pInventory->canAddingEx(x, y, pOustersArmsband)) {
                    pInventory->addItemEx(x, y, pOustersArmsband);
                } else {
                    processItemBugEx(pCreature, pOustersArmsband);
                }
                break;

            case STORAGE_GEAR:
                if (pCreature->isSlayer() || pCreature->isVampire()) {
                    processItemBugEx(pCreature, pOustersArmsband);
                } else if (pCreature->isOusters()) {
                    if (!pOusters->isWear((Ousters::WearPart)x)) {
                        pOusters->wearItem((Ousters::WearPart)x, pOustersArmsband);
                    } else {
                        processItemBugEx(pCreature, pOustersArmsband);
                    }
                }
                break;

            case STORAGE_BELT:
                processItemBugEx(pCreature, pOustersArmsband);
                break;

            case STORAGE_EXTRASLOT:
                if (pCreature->isSlayer())
                    pSlayer->addItemToExtraInventorySlot(pOustersArmsband);
                else if (pCreature->isVampire())
                    pVampire->addItemToExtraInventorySlot(pOustersArmsband);
                else if (pCreature->isOusters())
                    pOusters->addItemToExtraInventorySlot(pOustersArmsband);
                break;

            case STORAGE_MOTORCYCLE:
                processItemBugEx(pCreature, pOustersArmsband);
                break;

            case STORAGE_STASH:
                if (pStash->isExist(x, y)) {
                    processItemBugEx(pCreature, pOustersArmsband);
                } else
                    pStash->insert(x, y, pOustersArmsband);
                break;

            case STORAGE_GARBAGE:
                processItemBug(pCreature, pOustersArmsband);
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
void OustersArmsbandLoader::load(Zone* pZone)

{
    __BEGIN_TRY

    Assert(pZone != NULL);

    vector<GearZoneObjectRow> rows =
        defaultItemObjectRepository().loadGearInZone(GEAR_OUSTERS_ARMSBAND, (int)STORAGE_ZONE, pZone->getZoneID());

    for (size_t r = 0; r < rows.size(); r++) {
        OustersArmsband* pOustersArmsband = new OustersArmsband();

        pOustersArmsband->setItemID(rows[r].itemID);
        pOustersArmsband->setObjectID(rows[r].objectID);
        pOustersArmsband->setItemType(rows[r].itemType);

        Storage storage = (Storage)rows[r].storage;
        BYTE x = rows[r].x;
        BYTE y = rows[r].y;

        string optionField = rows[r].optionField;
        list<OptionType_t> optionTypes;
        setOptionTypeFromField(optionTypes, optionField);
        pOustersArmsband->setOptionType(optionTypes);

        pOustersArmsband->setDurability(rows[r].durability);
        pOustersArmsband->setEnchantLevel(rows[r].enchantLevel);
        pOustersArmsband->setCreateType((Item::CreateType)rows[r].createType);

        switch (storage) {
        case STORAGE_ZONE: {
            Tile& pTile = pZone->getTile(x, y);
            Assert(!pTile.hasItem());
            pTile.addItem(pOustersArmsband);
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
void OustersArmsbandLoader::load(StorageID_t storageID, Inventory* pInventory)

{
    __BEGIN_TRY

    Statement* pStmt;

    BEGIN_DB {}
    END_DB(pStmt)

    __END_CATCH
}
