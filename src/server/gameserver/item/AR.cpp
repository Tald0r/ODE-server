//////////////////////////////////////////////////////////////////////////////
// Filename    : AR.cpp
// Written By  : Elca
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "AR.h"

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

ItemID_t AR::m_ItemIDRegistry = 0;
Mutex AR::m_Mutex;

//--------------------------------------------------------------------------------
// constructor
//--------------------------------------------------------------------------------
AR::AR()

{
    setItemType(0);
    setDurability(0);
    setEnchantLevel(0);
    setBulletCount(0);
    setBonusDamage(0);
    setSilver(0);
}

AR::AR(ItemType_t itemType, const list<OptionType_t>& optionType)

//: Gun(itemType, optionType)
{
    setItemType(itemType);
    setOptionType(optionType);
    setDurability(computeMaxDurability(this));

    if (!de::gameContext().itemInfos().isPossibleItem(getItemClass(), getItemType(), getOptionTypeList())) {
        filelog("itembug.log", "AR::AR() : Invalid item type or option type");
        throw Error("AR::AR() : Invalid item type or optionType");
    }
}


//--------------------------------------------------------------------------------
// destructor
//--------------------------------------------------------------------------------
AR::~AR()

{}


//--------------------------------------------------------------------------------
// create item
//--------------------------------------------------------------------------------
void AR::create(const string& ownerID, Storage storage, StorageID_t storageID, BYTE x, BYTE y, ItemID_t itemID)

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

    defaultItemObjectRepository().insertGun(GEAR_AR, m_ItemID, m_ObjectID, getItemType(), ownerID, (int)storage,
                                            storageID, (int)x, (int)y, optionField, getDurability(),
                                            (int)getBulletCount(), (int)getGrade(), (int)m_CreateType);

    __END_CATCH
}


//--------------------------------------------------------------------------------
// save item
//--------------------------------------------------------------------------------
void AR::tinysave(const char* field) const

{
    __BEGIN_TRY

    defaultItemObjectRepository().tinysaveGear(GEAR_AR, field, m_ItemID);

    __END_CATCH
}

//--------------------------------------------------------------------------------
// save item
//--------------------------------------------------------------------------------
void AR::save(const string& ownerID, Storage storage, StorageID_t storageID, BYTE x, BYTE y)

{
    __BEGIN_TRY

    string optionField;
    setOptionTypeToField(getOptionTypeList(), optionField);

    defaultItemObjectRepository().updateGun(GEAR_AR, m_ObjectID, getItemType(), ownerID, (int)storage, storageID,
                                            (int)x, (int)y, optionField, getDurability(), (int)getEnchantLevel(),
                                            (int)getBulletCount(), (int)getSilver(), (int)getGrade(), m_ItemID);

    __END_CATCH
}

//--------------------------------------------------------------------------------
// save item
//--------------------------------------------------------------------------------
void AR::saveBullet() {
    __BEGIN_TRY

    defaultItemObjectRepository().saveGunBullet(GEAR_AR, getBulletCount(), m_ItemID);

    __END_CATCH
}

void AR::makePCItemInfo(PCItemInfo& result) const {
    Item::makePCItemInfo(result);
    result.setItemNum(getBulletCount());
}

//--------------------------------------------------------------------------------
// get debug string
//--------------------------------------------------------------------------------
string AR::toString() const

{
    StringStream msg;
    msg << "AR(" << "ItemID:" << m_ItemID << ",ItemType:" << (int)getItemType()
        << ",OptionType:" << getOptionTypeToString(getOptionTypeList()).c_str()
        << ",Durability:" << (int)getDurability() << ",BulletCount:" << (int)getBulletCount()
        << ",Silver:" << (int)getSilver() << ",EnchantLevel:" << (int)getEnchantLevel() << ")";
    return msg.toString();
}


//--------------------------------------------------------------------------------
// get width
//--------------------------------------------------------------------------------

//--------------------------------------------------------------------------------
// get debug string
//--------------------------------------------------------------------------------
string ARInfo::toString() const

{
    StringStream msg;
    msg << "ARInfo(" << "ItemType:" << m_ItemType << ",Name:" << m_Name << ",EName:" << m_EName << ",Price:" << m_Price
        << ",VolumeType:" << Volume2String[m_VolumeType] << ",Weight:" << m_Weight << ",Description:" << m_Description
        << ",Durability:" << m_Durability << ",minDamage:" << m_MinDamage << ",maxDamage:" << m_MaxDamage
        << ",ToHitBonus:" << m_ToHitBonus << ",Range:" << (int)m_Range << ")";
    return msg.toString();
}


//--------------------------------------------------------------------------------
// load from DB
//--------------------------------------------------------------------------------
void ARInfoManager::load()

{
    __BEGIN_TRY
    __BEGIN_DEBUG

    m_InfoCount = defaultItemObjectRepository().loadMaxGearType(GEAR_AR);

    m_pItemInfos = new ItemInfo*[m_InfoCount + 1];

    for (uint i = 0; i <= m_InfoCount; i++)
        m_pItemInfos[i] = NULL;

    vector<GunInfoRow> rows = defaultItemObjectRepository().loadGunInfos(GEAR_AR);

    for (size_t r = 0; r < rows.size(); r++) {
        ARInfo* pARInfo = new ARInfo();

        pARInfo->setItemType(rows[r].itemType);
        pARInfo->setName(rows[r].name);
        pARInfo->setEName(rows[r].ename);
        pARInfo->setPrice(rows[r].price);
        pARInfo->setVolumeType(rows[r].volume);
        pARInfo->setWeight(rows[r].weight);
        pARInfo->setRatio(rows[r].ratio);
        pARInfo->setDurability(rows[r].durability);
        pARInfo->setMinDamage(rows[r].minDamage);
        pARInfo->setMaxDamage(rows[r].maxDamage);
        pARInfo->setToHitBonus(rows[r].toHitBonus);
        pARInfo->setRange(rows[r].range);
        pARInfo->setSpeed(rows[r].speed);
        pARInfo->setReqAbility(rows[r].reqAbility);
        pARInfo->setItemLevel(rows[r].itemLevel);
        pARInfo->setCriticalBonus(rows[r].criticalBonus);
        pARInfo->setDefaultOptions(rows[r].defaultOption);
        pARInfo->setUpgradeRatio(rows[r].upgradeRatio);
        pARInfo->setUpgradeCrashPercent(rows[r].upgradeCrashPercent);
        pARInfo->setNextOptionRatio(rows[r].nextOptionRatio);
        pARInfo->setNextItemType(rows[r].nextItemType);
        pARInfo->setDowngradeRatio(rows[r].downgradeRatio);

        addItemInfo(pARInfo);
    }

    __END_DEBUG
    __END_CATCH
}


//--------------------------------------------------------------------------------
// load to creature
//--------------------------------------------------------------------------------
void ARLoader::load(Creature* pCreature)

{
    __BEGIN_TRY

    Assert(pCreature != NULL);

    vector<GunObjectRow> rows = defaultItemObjectRepository().loadGunOfOwner(GEAR_AR, pCreature->getName());

    for (size_t r = 0; r < rows.size(); r++) {
        try {
            AR* pAR = new AR();

            pAR->setItemID(rows[r].itemID);
            pAR->setObjectID(rows[r].objectID);
            pAR->setItemType(rows[r].itemType);

            if (de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_AR, pAR->getItemType())->isUnique())
                pAR->setUnique();

            Storage storage = (Storage)rows[r].storage;
            BYTE x = rows[r].x;
            BYTE y = rows[r].y;

            string optionField = rows[r].optionField;
            list<OptionType_t> optionTypes;
            setOptionTypeFromField(optionTypes, optionField);
            pAR->setOptionType(optionTypes);

            pAR->setDurability(rows[r].durability);
            pAR->setBulletCount(rows[r].bulletCount);
            pAR->setSilver(rows[r].silver);
            pAR->setEnchantLevel(rows[r].enchantLevel);
            pAR->setGrade(rows[r].grade);
            pAR->setCreateType((Item::CreateType)rows[r].createType);

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
                if (pInventory->canAddingEx(x, y, pAR)) {
                    pInventory->addItemEx(x, y, pAR);
                } else {
                    processItemBugEx(pCreature, pAR);
                }
                break;

            case STORAGE_GEAR:
                if (pCreature->isSlayer()) {
                    if (!pSlayer->isWear((Slayer::WearPart)x)) {
                        pSlayer->wearItem((Slayer::WearPart)x, pAR);
                    } else {
                        processItemBugEx(pCreature, pAR);
                    }
                } else if (pCreature->isVampire()) {
                    processItemBugEx(pCreature, pAR);
                }
                break;

            case STORAGE_BELT:
                processItemBugEx(pCreature, pAR);
                break;

            case STORAGE_EXTRASLOT:
                if (pCreature->isSlayer())
                    pSlayer->addItemToExtraInventorySlot(pAR);
                else if (pCreature->isVampire())
                    pVampire->addItemToExtraInventorySlot(pAR);
                break;

            case STORAGE_MOTORCYCLE:
                processItemBugEx(pCreature, pAR);
                break;

            case STORAGE_STASH:
                if (pStash->isExist(x, y)) {
                    processItemBugEx(pCreature, pAR);
                } else
                    pStash->insert(x, y, pAR);
                break;

            case STORAGE_GARBAGE:
                processItemBug(pCreature, pAR);
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
void ARLoader::load(Zone* pZone)

{
    __BEGIN_TRY

    Assert(pZone != NULL);

    vector<GunZoneObjectRow> rows =
        defaultItemObjectRepository().loadGunInZone(GEAR_AR, (int)STORAGE_ZONE, pZone->getZoneID());

    for (size_t r = 0; r < rows.size(); r++) {
        AR* pAR = new AR();

        pAR->setItemID(rows[r].itemID);
        pAR->setObjectID(rows[r].objectID);
        pAR->setItemType(rows[r].itemType);

        Storage storage = (Storage)rows[r].storage;
        BYTE x = rows[r].x;
        BYTE y = rows[r].y;

        string optionField = rows[r].optionField;
        list<OptionType_t> optionTypes;
        setOptionTypeFromField(optionTypes, optionField);
        pAR->setOptionType(optionTypes);

        pAR->setDurability(rows[r].durability);
        pAR->setBulletCount(rows[r].bulletCount);
        pAR->setSilver(rows[r].silver);
        pAR->setEnchantLevel(rows[r].enchantLevel);
        pAR->setCreateType((Item::CreateType)rows[r].createType);

        switch (storage) {
        case STORAGE_ZONE: {
            Tile& pTile = pZone->getTile(x, y);
            Assert(!pTile.hasItem());
            pTile.addItem(pAR);
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
void ARLoader::load(StorageID_t storageID, Inventory* pInventory)

{
    __BEGIN_TRY

    Statement* pStmt;

    BEGIN_DB {}
    END_DB(pStmt);

    __END_CATCH
}
