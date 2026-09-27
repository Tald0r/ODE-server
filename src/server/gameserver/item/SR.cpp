//////////////////////////////////////////////////////////////////////////////
// Filename    : SR.cpp
// Written By  : Elca
// Description :
//////////////////////////////////////////////////////////////////////////////

// include files
#include "SR.h"

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

ItemID_t SR::m_ItemIDRegistry = 0;
Mutex SR::m_Mutex;

//--------------------------------------------------------------------------------
// constructor
//--------------------------------------------------------------------------------
SR::SR()

{
    setItemType(0);
    setDurability(0);
    setEnchantLevel(0);
    setBulletCount(0);
    setBonusDamage(0);
    setSilver(0);
}

SR::SR(ItemType_t itemType, const list<OptionType_t>& optionType)

//: Gun(itemType, optionType)
{
    setItemType(itemType);
    setOptionType(optionType);
    setDurability(computeMaxDurability(this));

    if (!de::gameContext().itemInfos().isPossibleItem(getItemClass(), getItemType(), getOptionTypeList())) {
        filelog("itembug.log", "SR::SR() : Invalid item type or option type");
        throw Error("SR::SR() : Invalid item type or optionType");
    }
}


//--------------------------------------------------------------------------------
// destructor
//--------------------------------------------------------------------------------
SR::~SR()

{}


//--------------------------------------------------------------------------------
// create item
//--------------------------------------------------------------------------------
void SR::create(const string& ownerID, Storage storage, StorageID_t storageID, BYTE x, BYTE y, ItemID_t itemID)

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

    defaultItemObjectRepository().insertGun(GEAR_SR, m_ItemID, m_ObjectID, getItemType(), ownerID, (int)storage,
                                            storageID, (int)x, (int)y, optionField, getDurability(),
                                            (int)getBulletCount(), (int)getGrade(), (int)m_CreateType);

    __END_CATCH
}


//--------------------------------------------------------------------------------
// save item
//--------------------------------------------------------------------------------
void SR::tinysave(const char* field) const

{
    __BEGIN_TRY

    defaultItemObjectRepository().tinysaveGun(GEAR_SR, field, (int)getBulletCount(), m_ItemID);

    __END_CATCH
}

//--------------------------------------------------------------------------------
// save item
//--------------------------------------------------------------------------------
void SR::save(const string& ownerID, Storage storage, StorageID_t storageID, BYTE x, BYTE y)

{
    __BEGIN_TRY

    string optionField;
    setOptionTypeToField(getOptionTypeList(), optionField);

    defaultItemObjectRepository().updateGun(GEAR_SR, m_ObjectID, getItemType(), ownerID, (int)storage, storageID,
                                            (int)x, (int)y, optionField, getDurability(), (int)getEnchantLevel(),
                                            (int)getBulletCount(), (int)getSilver(), (int)getGrade(), m_ItemID);

    __END_CATCH
}

//--------------------------------------------------------------------------------
// save item
//--------------------------------------------------------------------------------
void SR::saveBullet() {
    __BEGIN_TRY

    defaultItemObjectRepository().saveGunBullet(GEAR_SR, getBulletCount(), m_ItemID);

    __END_CATCH
}

void SR::makePCItemInfo(PCItemInfo& result) const {
    Item::makePCItemInfo(result);
    result.setItemNum(getBulletCount());
}

//--------------------------------------------------------------------------------
// get debug string
//--------------------------------------------------------------------------------
string SR::toString() const

{
    StringStream msg;

    msg << "SR(" << "ItemID:" << m_ItemID << ",ItemType:" << (int)getItemType()
        << ",OptionType:" << getOptionTypeToString(getOptionTypeList()).c_str()
        << ",Durability:" << (int)getDurability() << ",BulletCount:" << (int)getBulletCount()
        << ",Silver:" << (int)getSilver() << ",EnchantLevel:" << (int)getEnchantLevel() << ")";

    return msg.toString();
}


//--------------------------------------------------------------------------------
// get debug string
//--------------------------------------------------------------------------------
string SRInfo::toString() const

{
    StringStream msg;

    msg << "SRInfo(" << "ItemType:" << m_ItemType << ",Name:" << m_Name << ",EName:" << m_EName << ",Price:" << m_Price
        << ",VolumeType:" << Volume2String[m_VolumeType] << ",Weight:" << m_Weight << ",Description:" << m_Description
        << ",Durability:" << m_Durability << ",minDamage:" << m_MinDamage << ",maxDamage:" << m_MaxDamage
        << ",ToHitBonus:" << m_ToHitBonus << ",Range:" << (int)m_Range << ",ReqAbility:?" << ")";

    return msg.toString();
}


//--------------------------------------------------------------------------------
// load from DB
//--------------------------------------------------------------------------------
void SRInfoManager::load()

{
    __BEGIN_TRY

    m_InfoCount = defaultItemObjectRepository().loadMaxGearType(GEAR_SR);

    m_pItemInfos = new ItemInfo*[m_InfoCount + 1];

    for (uint i = 0; i <= m_InfoCount; i++)
        m_pItemInfos[i] = NULL;

    vector<GunInfoRow> rows = defaultItemObjectRepository().loadGunInfos(GEAR_SR);

    for (size_t r = 0; r < rows.size(); r++) {
        SRInfo* pSRInfo = new SRInfo();

        pSRInfo->setItemType(rows[r].itemType);
        pSRInfo->setName(rows[r].name);
        pSRInfo->setEName(rows[r].ename);
        pSRInfo->setPrice(rows[r].price);
        pSRInfo->setVolumeType(rows[r].volume);
        pSRInfo->setWeight(rows[r].weight);
        pSRInfo->setRatio(rows[r].ratio);
        pSRInfo->setDurability(rows[r].durability);
        pSRInfo->setMinDamage(rows[r].minDamage);
        pSRInfo->setMaxDamage(rows[r].maxDamage);
        pSRInfo->setToHitBonus(rows[r].toHitBonus);
        pSRInfo->setRange(rows[r].range);
        pSRInfo->setSpeed(rows[r].speed);
        pSRInfo->setReqAbility(rows[r].reqAbility);
        pSRInfo->setItemLevel(rows[r].itemLevel);
        pSRInfo->setCriticalBonus(rows[r].criticalBonus);
        pSRInfo->setDefaultOptions(rows[r].defaultOption);
        pSRInfo->setUpgradeRatio(rows[r].upgradeRatio);
        pSRInfo->setUpgradeCrashPercent(rows[r].upgradeCrashPercent);
        pSRInfo->setNextOptionRatio(rows[r].nextOptionRatio);
        pSRInfo->setNextItemType(rows[r].nextItemType);
        pSRInfo->setDowngradeRatio(rows[r].downgradeRatio);

        addItemInfo(pSRInfo);
    }

    __END_CATCH
}


//--------------------------------------------------------------------------------
// load to creature
//--------------------------------------------------------------------------------
void SRLoader::load(Creature* pCreature)

{
    __BEGIN_TRY

    Assert(pCreature != NULL);

    vector<GunObjectRow> rows = defaultItemObjectRepository().loadGunOfOwner(GEAR_SR, pCreature->getName());

    for (size_t r = 0; r < rows.size(); r++) {
        try {
            SR* pSR = new SR();

            pSR->setItemID(rows[r].itemID);
            pSR->setObjectID(rows[r].objectID);
            pSR->setItemType(rows[r].itemType);

            if (de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_SR, pSR->getItemType())->isUnique())
                pSR->setUnique();

            Storage storage = (Storage)rows[r].storage;
            BYTE x = rows[r].x;
            BYTE y = rows[r].y;

            string optionField = rows[r].optionField;
            list<OptionType_t> optionTypes;
            setOptionTypeFromField(optionTypes, optionField);
            pSR->setOptionType(optionTypes);

            pSR->setDurability(rows[r].durability);
            pSR->setEnchantLevel(rows[r].enchantLevel);
            pSR->setBulletCount(rows[r].bulletCount);
            pSR->setSilver(rows[r].silver);
            pSR->setGrade(rows[r].grade);
            pSR->setCreateType((Item::CreateType)rows[r].createType);

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
                if (pInventory->canAddingEx(x, y, pSR)) {
                    pInventory->addItemEx(x, y, pSR);
                } else {
                    processItemBugEx(pCreature, pSR);
                }
                break;

            case STORAGE_GEAR:
                if (pCreature->isSlayer()) {
                    if (!pSlayer->isWear((Slayer::WearPart)x)) {
                        pSlayer->wearItem((Slayer::WearPart)x, pSR);
                    } else {
                        processItemBugEx(pCreature, pSR);
                    }
                } else if (pCreature->isVampire()) {
                    processItemBugEx(pCreature, pSR);
                }
                break;

            case STORAGE_BELT:
                processItemBugEx(pCreature, pSR);
                break;

            case STORAGE_EXTRASLOT:
                if (pCreature->isSlayer())
                    pSlayer->addItemToExtraInventorySlot(pSR);
                else if (pCreature->isVampire())
                    pVampire->addItemToExtraInventorySlot(pSR);
                break;

            case STORAGE_MOTORCYCLE:
                processItemBugEx(pCreature, pSR);
                break;

            case STORAGE_STASH:
                if (pStash->isExist(x, y)) {
                    processItemBugEx(pCreature, pSR);
                } else
                    pStash->insert(x, y, pSR);
                break;

            case STORAGE_GARBAGE:
                processItemBug(pCreature, pSR);
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
void SRLoader::load(Zone* pZone)

{
    __BEGIN_TRY

    Assert(pZone != NULL);

    vector<GunZoneObjectRow> rows =
        defaultItemObjectRepository().loadGunInZone(GEAR_SR, (int)STORAGE_ZONE, pZone->getZoneID());

    for (size_t r = 0; r < rows.size(); r++) {
        SR* pSR = new SR();

        pSR->setItemID(rows[r].itemID);
        pSR->setObjectID(rows[r].objectID);
        pSR->setItemType(rows[r].itemType);

        Storage storage = (Storage)rows[r].storage;
        BYTE x = rows[r].x;
        BYTE y = rows[r].y;

        string optionField = rows[r].optionField;
        list<OptionType_t> optionTypes;
        setOptionTypeFromField(optionTypes, optionField);
        pSR->setOptionType(optionTypes);

        pSR->setDurability(rows[r].durability);
        pSR->setEnchantLevel(rows[r].enchantLevel);
        pSR->setBulletCount(rows[r].bulletCount);
        pSR->setSilver(rows[r].silver);
        pSR->setCreateType((Item::CreateType)rows[r].createType);

        switch (storage) {
        case STORAGE_ZONE: {
            Tile& pTile = pZone->getTile(x, y);
            Assert(!pTile.hasItem());
            pTile.addItem(pSR);
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
void SRLoader::load(StorageID_t storageID, Inventory* pInventory)

{
    __BEGIN_TRY

    Statement* pStmt;

    BEGIN_DB {}
    END_DB(pStmt)

    __END_CATCH
}
