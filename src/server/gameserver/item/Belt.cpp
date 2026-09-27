//////////////////////////////////////////////////////////////////////////////
// Filename    : Belt.cpp
// Written By  : Elca
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "Belt.h"

#include "DB.h"
#include "GameContext.h"
#include "ItemInfoManager.h"
#include "ItemUtil.h"
#include "Motorcycle.h"
#include "PCItemInfo.h"
#include "Slayer.h"
#include "Stash.h"
#include "Vampire.h"
#include "repository/ItemObjectRepository.h"

ItemID_t Belt::m_ItemIDRegistry = 0;
Mutex Belt::m_Mutex;

//--------------------------------------------------------------------------------
// constructor
//--------------------------------------------------------------------------------
Belt::Belt()

//: m_ItemType(0), m_Durability(0), m_pInventory(NULL)
{
    setItemType(0);
    setDurability(0);
    m_pInventory = NULL;
}

Belt::Belt(ItemType_t itemType, const list<OptionType_t>& optionType)

//: m_ItemType(itemType), m_OptionType(optionType), m_Durability(0), m_pInventory(NULL)
{
    __BEGIN_TRY

    setItemType(itemType);
    setOptionType(optionType);

    BeltInfo* pBeltInfo =
        dynamic_cast<BeltInfo*>(de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_BELT, getItemType()));

    m_pInventory = new Inventory(pBeltInfo->getPocketCount(), 1);


    setDurability(computeMaxDurability(this));

    if (!de::gameContext().itemInfos().isPossibleItem(getItemClass(), getItemType(), getOptionTypeList())) {
        filelog("itembug.log", "Belt::Belt() : Invalid item type or option type");
        throw Error("Belt::Belt() : Invalid item type or optionType");
    }

    __END_CATCH
}


//--------------------------------------------------------------------------------
// destructor
//--------------------------------------------------------------------------------
Belt::~Belt()

{
    SAFE_DELETE(m_pInventory);
}

//--------------------------------------------------------------------------------
// create item
//--------------------------------------------------------------------------------
void Belt::create(const string& ownerID, Storage storage, StorageID_t storageID, BYTE x, BYTE y, ItemID_t itemID)

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

    defaultItemObjectRepository().insertGear(GEAR_BELT, m_ItemID, m_ObjectID, getItemType(), ownerID, (int)storage,
                                             storageID, (int)x, (int)y, optionField, getDurability(), getGrade(),
                                             (int)m_CreateType);

    __END_CATCH
}


//--------------------------------------------------------------------------------
// destroy item
//--------------------------------------------------------------------------------
bool Belt::destroy()

{
    __BEGIN_TRY

    //-------------------------------------------------------
    // If any items are left in the belt, the items inside would have to be
    // destroyed as well.
    // For a belt the items are already deleted by the delete above, so they
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

    if (!defaultItemObjectRepository().destroyGearObject(GEAR_BELT, m_ItemID))
        return false;

    __END_CATCH

    return true;
}


//--------------------------------------------------------------------------------
// save item
//--------------------------------------------------------------------------------
void Belt::tinysave(const char* field) const

{
    __BEGIN_TRY

    defaultItemObjectRepository().tinysaveGear(GEAR_BELT, field, m_ItemID);

    __END_CATCH
}

//--------------------------------------------------------------------------------
// save item
//--------------------------------------------------------------------------------
void Belt::save(const string& ownerID, Storage storage, StorageID_t storageID, BYTE x, BYTE y)

{
    __BEGIN_TRY

    string optionField;
    setOptionTypeToField(getOptionTypeList(), optionField);

    defaultItemObjectRepository().updateGear(GEAR_BELT, m_ObjectID, getItemType(), ownerID, (int)storage, storageID,
                                             (int)x, (int)y, optionField, getDurability(), getGrade(),
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

void Belt::makePCItemInfo(PCItemInfo& result) const {
    Item::makePCItemInfo(result);

    BYTE SubItemCount = 0;

    // Read as many item infos as the belt has pockets.
    for (int i = 0; i < getPocketCount(); i++) {
        Item* pBeltItem = getInventory()->getItem(i, 0);
        if (pBeltItem != NULL) {
            SubItemInfo* pSubItemInfo = new SubItemInfo();
            pSubItemInfo->setObjectID(pBeltItem->getObjectID());
            pSubItemInfo->setItemClass(pBeltItem->getItemClass());
            pSubItemInfo->setItemType(pBeltItem->getItemType());
            pSubItemInfo->setItemNum(pBeltItem->getNum());
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
string Belt::toString() const

{
    StringStream msg;

    msg << "Belt(" << "ItemID:" << m_ItemID << ",ItemType:" << (int)getItemType()
        << ",OptionType:" << getOptionTypeToString(getOptionTypeList()).c_str()
        << ",Durability:" << (int)getDurability() << ",EnchantLevel:" << (int)getEnchantLevel() << ")";

    return msg.toString();
}


//--------------------------------------------------------------------------------
// get pocket count
//--------------------------------------------------------------------------------
PocketNum_t Belt::getPocketCount(void) const

{
    __BEGIN_TRY

    BeltInfo* pBeltInfo =
        dynamic_cast<BeltInfo*>(de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_BELT, getItemType()));
    Assert(pBeltInfo != NULL);
    return pBeltInfo->getPocketCount();

    __END_CATCH
}


//--------------------------------------------------------------------------------
// get debug string
//--------------------------------------------------------------------------------
string BeltInfo::toString() const

{
    StringStream msg;

    msg << "BeltInfo(" << "ItemType:" << m_ItemType << ",Name:" << m_Name << ",EName:" << m_EName
        << ",Price:" << m_Price << ",VolumeType:" << Volume2String[m_VolumeType] << ",Weight:" << m_Weight
        << ",Description:" << m_Description << ",Durability:" << m_Durability << ")";

    return msg.toString();
}


//--------------------------------------------------------------------------------
// load from DB
//--------------------------------------------------------------------------------
void BeltInfoManager::load()

{
    __BEGIN_TRY
    __BEGIN_DEBUG

    m_InfoCount = defaultItemObjectRepository().loadMaxGearType(GEAR_BELT);

    m_pItemInfos = new ItemInfo*[m_InfoCount + 1];

    for (uint i = 0; i <= m_InfoCount; i++)
        m_pItemInfos[i] = NULL;

    vector<PocketInfoRow> rows = defaultItemObjectRepository().loadPocketInfos(GEAR_BELT);

    for (size_t r = 0; r < rows.size(); r++) {
        BeltInfo* pBeltInfo = new BeltInfo();

        pBeltInfo->setItemType(rows[r].itemType);
        pBeltInfo->setName(rows[r].name);
        pBeltInfo->setEName(rows[r].ename);
        pBeltInfo->setPrice(rows[r].price);
        pBeltInfo->setVolumeType(rows[r].volume);
        pBeltInfo->setWeight(rows[r].weight);
        pBeltInfo->setRatio(rows[r].ratio);
        pBeltInfo->setDurability(rows[r].durability);
        pBeltInfo->setDefenseBonus(rows[r].defense);
        pBeltInfo->setProtectionBonus(rows[r].protection);
        pBeltInfo->setPocketCount(rows[r].pocketCount);
        pBeltInfo->setReqAbility(rows[r].reqAbility);
        pBeltInfo->setItemLevel(rows[r].itemLevel);
        pBeltInfo->setDefaultOptions(rows[r].defaultOption);
        pBeltInfo->setUpgradeRatio(rows[r].upgradeRatio);
        pBeltInfo->setUpgradeCrashPercent(rows[r].upgradeCrashPercent);
        pBeltInfo->setNextOptionRatio(rows[r].nextOptionRatio);
        pBeltInfo->setNextItemType(rows[r].nextItemType);
        pBeltInfo->setDowngradeRatio(rows[r].downgradeRatio);

        addItemInfo(pBeltInfo);
    }

    __END_DEBUG
    __END_CATCH
}


//--------------------------------------------------------------------------------
// load to creature
//--------------------------------------------------------------------------------
void BeltLoader::load(Creature* pCreature)

{
    __BEGIN_TRY

    Assert(pCreature != NULL);

    vector<GearObjectRow> rows = defaultItemObjectRepository().loadGearOfOwner(GEAR_BELT, pCreature->getName());

    for (size_t r = 0; r < rows.size(); r++) {
        try {
            Belt* pBelt = new Belt();

            pBelt->setItemID(rows[r].itemID);
            pBelt->setObjectID(rows[r].objectID);
            pBelt->setItemType(rows[r].itemType);

            if (de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_BELT, pBelt->getItemType())->isUnique())
                pBelt->setUnique();

            BeltInfo* pBeltInfo = dynamic_cast<BeltInfo*>(
                de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_BELT, pBelt->getItemType()));
            Inventory* pBeltInventory = new Inventory(pBeltInfo->getPocketCount(), 1);

            pBelt->setInventory(pBeltInventory);

            Storage storage = (Storage)rows[r].storage;
            BYTE x = rows[r].x;
            BYTE y = rows[r].y;

            string optionField = rows[r].optionField;
            list<OptionType_t> optionTypes;
            setOptionTypeFromField(optionTypes, optionField);
            pBelt->setOptionType(optionTypes);

            pBelt->setDurability(rows[r].durability);
            pBelt->setGrade(rows[r].grade);
            pBelt->setEnchantLevel(rows[r].enchantLevel);
            pBelt->setCreateType((Item::CreateType)rows[r].createType);

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
                if (pInventory->canAddingEx(x, y, pBelt)) {
                    pInventory->addItemEx(x, y, pBelt);
                } else {
                    processItemBugEx(pCreature, pBelt);
                }
                break;

            case STORAGE_GEAR:
                if (pCreature->isSlayer()) {
                    if (!pSlayer->isWear((Slayer::WearPart)x)) {
                        pSlayer->wearItem((Slayer::WearPart)x, pBelt);
                    } else {
                        processItemBugEx(pCreature, pBelt);
                    }
                } else if (pCreature->isVampire()) {
                    processItemBugEx(pCreature, pBelt);
                }
                break;

            case STORAGE_BELT:
                processItemBugEx(pCreature, pBelt);
                break;

            case STORAGE_EXTRASLOT:
                if (pCreature->isSlayer())
                    pSlayer->addItemToExtraInventorySlot(pBelt);
                else if (pCreature->isVampire())
                    pVampire->addItemToExtraInventorySlot(pBelt);
                break;

            case STORAGE_MOTORCYCLE:
                processItemBugEx(pCreature, pBelt);
                break;

            case STORAGE_STASH:
                if (pStash->isExist(x, y)) {
                    processItemBugEx(pCreature, pBelt);
                } else {
                    pStash->insert(x, y, pBelt);
                }
                break;

            case STORAGE_GARBAGE:
                processItemBug(pCreature, pBelt);
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
void BeltLoader::load(Zone* pZone)

{
    __BEGIN_TRY

    Assert(pZone != NULL);

    vector<GearZoneObjectRow> rows =
        defaultItemObjectRepository().loadGearInZone(GEAR_BELT, (int)STORAGE_ZONE, pZone->getZoneID());

    for (size_t r = 0; r < rows.size(); r++) {
        Belt* pBelt = new Belt();

        pBelt->setItemID(rows[r].itemID);
        pBelt->setObjectID(rows[r].objectID);
        pBelt->setItemType(rows[r].itemType);

        Storage storage = (Storage)rows[r].storage;
        BYTE x = rows[r].x;
        BYTE y = rows[r].y;

        string optionField = rows[r].optionField;
        list<OptionType_t> optionTypes;
        setOptionTypeFromField(optionTypes, optionField);
        pBelt->setOptionType(optionTypes);

        pBelt->setDurability(rows[r].durability);
        pBelt->setEnchantLevel(rows[r].enchantLevel);
        pBelt->setCreateType((Item::CreateType)rows[r].createType);

        switch (storage) {
        case STORAGE_ZONE: {
            Tile& pTile = pZone->getTile(x, y);
            Assert(!pTile.hasItem());
            pTile.addItem(pBelt);
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
void BeltLoader::load(StorageID_t storageID, Inventory* pInventory)

{
    __BEGIN_TRY

    Statement* pStmt;

    BEGIN_DB {}
    END_DB(pStmt)

    __END_CATCH
}
