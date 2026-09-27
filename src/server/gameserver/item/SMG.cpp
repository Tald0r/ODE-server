//////////////////////////////////////////////////////////////////////////////
// Filename    : SMG.cpp
// Written By  : Elca
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "SMG.h"

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

ItemID_t SMG::m_ItemIDRegistry = 0;
Mutex SMG::m_Mutex;

//--------------------------------------------------------------------------------
// constructor
//--------------------------------------------------------------------------------
SMG::SMG()

{
    setItemType(0);
    setDurability(0);
    setEnchantLevel(0);
    setBulletCount(0);
    setBonusDamage(0);
    setSilver(0);
}

SMG::SMG(ItemType_t itemType, const list<OptionType_t>& optionType)

//: Gun(itemType, optionType)
{
    setItemType(itemType);
    setOptionType(optionType);
    setDurability(computeMaxDurability(this));

    if (!de::gameContext().itemInfos().isPossibleItem(getItemClass(), getItemType(), getOptionTypeList())) {
        filelog("itembug.log", "SMG::SMG() : Invalid item type or option type");
        throw Error("SMG::SMG() : Invalid item type or optionType");
    }
}


//--------------------------------------------------------------------------------
// destructor
//--------------------------------------------------------------------------------
SMG::~SMG()

{}


//--------------------------------------------------------------------------------
// create item
//--------------------------------------------------------------------------------
void SMG::create(const string& ownerID, Storage storage, StorageID_t storageID, BYTE x, BYTE y, ItemID_t itemID)

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

    defaultItemObjectRepository().insertGun(GEAR_SMG, m_ItemID, m_ObjectID, getItemType(), ownerID, (int)storage,
                                            storageID, (int)x, (int)y, optionField, getDurability(),
                                            (int)getBulletCount(), (int)getGrade(), (int)m_CreateType);

    __END_CATCH
}


//--------------------------------------------------------------------------------
// save item
//--------------------------------------------------------------------------------
void SMG::tinysave(const char* field) const

{
    __BEGIN_TRY

    defaultItemObjectRepository().tinysaveGun(GEAR_SMG, field, (int)getBulletCount(), m_ItemID);

    __END_CATCH
}

//--------------------------------------------------------------------------------
// save item
//--------------------------------------------------------------------------------
void SMG::save(const string& ownerID, Storage storage, StorageID_t storageID, BYTE x, BYTE y)

{
    __BEGIN_TRY

    string optionField;
    setOptionTypeToField(getOptionTypeList(), optionField);

    defaultItemObjectRepository().updateGun(GEAR_SMG, m_ObjectID, getItemType(), ownerID, (int)storage, storageID,
                                            (int)x, (int)y, optionField, getDurability(), (int)getEnchantLevel(),
                                            (int)getBulletCount(), (int)getSilver(), (int)getGrade(), m_ItemID);

    __END_CATCH
}

//--------------------------------------------------------------------------------
// save item
//--------------------------------------------------------------------------------
void SMG::saveBullet() {
    __BEGIN_TRY

    defaultItemObjectRepository().saveGunBullet(GEAR_SMG, getBulletCount(), m_ItemID);

    __END_CATCH
}

void SMG::makePCItemInfo(PCItemInfo& result) const {
    Item::makePCItemInfo(result);
    result.setItemNum(getBulletCount());
}

//--------------------------------------------------------------------------------
// get debug string
//--------------------------------------------------------------------------------
string SMG::toString() const

{
    StringStream msg;

    msg << "SMG(" << "ItemID:" << m_ItemID << ",ItemType:" << (int)getItemType()
        << ",OptionType:" << getOptionTypeToString(getOptionTypeList()).c_str()
        << ",Durability:" << (int)getDurability() << ",BulletCount:" << (int)getBulletCount()
        << ",Silver:" << (int)getSilver() << ",EnchantLevel:" << (int)getEnchantLevel() << ")";

    return msg.toString();
}


//--------------------------------------------------------------------------------
// get debug string
//--------------------------------------------------------------------------------
string SMGInfo::toString() const

{
    StringStream msg;

    msg << "SMGInfo(" << "ItemType:" << m_ItemType << ",Name:" << m_Name << ",EName:" << m_EName << ",Price:" << m_Price
        << ",VolumeType:" << Volume2String[m_VolumeType] << ",Weight:" << m_Weight << ",Description:" << m_Description
        << ",Durability:" << m_Durability << ",minDamage:" << m_MinDamage << ",maxDamage:" << m_MaxDamage
        << ",ToHitBonus:" << m_ToHitBonus << ",Range:" << (int)m_Range << ",ReqAbility:?" << ")";

    return msg.toString();
}


//--------------------------------------------------------------------------------
// load from DB
//--------------------------------------------------------------------------------
void SMGInfoManager::load()

{
    __BEGIN_TRY

    m_InfoCount = defaultItemObjectRepository().loadMaxGearType(GEAR_SMG);

    m_pItemInfos = new ItemInfo*[m_InfoCount + 1];

    for (uint i = 0; i <= m_InfoCount; i++)
        m_pItemInfos[i] = NULL;

    vector<GunInfoRow> rows = defaultItemObjectRepository().loadGunInfos(GEAR_SMG);

    for (size_t r = 0; r < rows.size(); r++) {
        SMGInfo* pSMGInfo = new SMGInfo();

        pSMGInfo->setItemType(rows[r].itemType);
        pSMGInfo->setName(rows[r].name);
        pSMGInfo->setEName(rows[r].ename);
        pSMGInfo->setPrice(rows[r].price);
        pSMGInfo->setVolumeType(rows[r].volume);
        pSMGInfo->setWeight(rows[r].weight);
        pSMGInfo->setRatio(rows[r].ratio);
        pSMGInfo->setDurability(rows[r].durability);
        pSMGInfo->setMinDamage(rows[r].minDamage);
        pSMGInfo->setMaxDamage(rows[r].maxDamage);
        pSMGInfo->setToHitBonus(rows[r].toHitBonus);
        pSMGInfo->setRange(rows[r].range);
        pSMGInfo->setSpeed(rows[r].speed);
        pSMGInfo->setReqAbility(rows[r].reqAbility);
        pSMGInfo->setItemLevel(rows[r].itemLevel);
        pSMGInfo->setCriticalBonus(rows[r].criticalBonus);
        pSMGInfo->setDefaultOptions(rows[r].defaultOption);
        pSMGInfo->setUpgradeRatio(rows[r].upgradeRatio);
        pSMGInfo->setUpgradeCrashPercent(rows[r].upgradeCrashPercent);
        pSMGInfo->setNextOptionRatio(rows[r].nextOptionRatio);
        pSMGInfo->setNextItemType(rows[r].nextItemType);
        pSMGInfo->setDowngradeRatio(rows[r].downgradeRatio);

        addItemInfo(pSMGInfo);
    }

    __END_CATCH
}


//--------------------------------------------------------------------------------
// load to creature
//--------------------------------------------------------------------------------
void SMGLoader::load(Creature* pCreature)

{
    __BEGIN_TRY

    Assert(pCreature != NULL);

    vector<GunObjectRow> rows = defaultItemObjectRepository().loadGunOfOwner(GEAR_SMG, pCreature->getName());

    for (size_t r = 0; r < rows.size(); r++) {
        try {
            SMG* pSMG = new SMG();

            pSMG->setItemID(rows[r].itemID);
            pSMG->setObjectID(rows[r].objectID);
            pSMG->setItemType(rows[r].itemType);

            if (de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_SMG, pSMG->getItemType())->isUnique())
                pSMG->setUnique();

            Storage storage = (Storage)rows[r].storage;
            BYTE x = rows[r].x;
            BYTE y = rows[r].y;

            string optionField = rows[r].optionField;
            list<OptionType_t> optionTypes;
            setOptionTypeFromField(optionTypes, optionField);
            pSMG->setOptionType(optionTypes);

            pSMG->setDurability(rows[r].durability);
            pSMG->setEnchantLevel(rows[r].enchantLevel);
            pSMG->setBulletCount(rows[r].bulletCount);
            pSMG->setSilver(rows[r].silver);
            pSMG->setGrade(rows[r].grade);
            pSMG->setCreateType((Item::CreateType)rows[r].createType);

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
                if (pInventory->canAddingEx(x, y, pSMG)) {
                    pInventory->addItemEx(x, y, pSMG);
                } else {
                    processItemBugEx(pCreature, pSMG);
                }
                break;

            case STORAGE_GEAR:
                if (pCreature->isSlayer()) {
                    if (!pSlayer->isWear((Slayer::WearPart)x)) {
                        pSlayer->wearItem((Slayer::WearPart)x, pSMG);
                    } else {
                        processItemBugEx(pCreature, pSMG);
                    }
                } else if (pCreature->isVampire()) {
                    processItemBugEx(pCreature, pSMG);
                }
                break;

            case STORAGE_BELT:
                processItemBugEx(pCreature, pSMG);
                break;

            case STORAGE_EXTRASLOT:
                if (pCreature->isSlayer())
                    pSlayer->addItemToExtraInventorySlot(pSMG);
                else if (pCreature->isVampire())
                    pVampire->addItemToExtraInventorySlot(pSMG);
                break;

            case STORAGE_MOTORCYCLE:
                processItemBugEx(pCreature, pSMG);
                break;

            case STORAGE_STASH:
                if (pStash->isExist(x, y)) {
                    processItemBugEx(pCreature, pSMG);
                } else
                    pStash->insert(x, y, pSMG);
                break;

            case STORAGE_GARBAGE:
                processItemBug(pCreature, pSMG);
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
void SMGLoader::load(Zone* pZone)

{
    __BEGIN_TRY

    Assert(pZone != NULL);

    vector<GunZoneObjectRow> rows =
        defaultItemObjectRepository().loadGunInZone(GEAR_SMG, (int)STORAGE_ZONE, pZone->getZoneID());

    for (size_t r = 0; r < rows.size(); r++) {
        SMG* pSMG = new SMG();

        pSMG->setItemID(rows[r].itemID);
        pSMG->setObjectID(rows[r].objectID);
        pSMG->setItemType(rows[r].itemType);

        Storage storage = (Storage)rows[r].storage;
        BYTE x = rows[r].x;
        BYTE y = rows[r].y;

        string optionField = rows[r].optionField;
        list<OptionType_t> optionTypes;
        setOptionTypeFromField(optionTypes, optionField);
        pSMG->setOptionType(optionTypes);

        pSMG->setDurability(rows[r].durability);
        pSMG->setEnchantLevel(rows[r].enchantLevel);
        pSMG->setBulletCount(rows[r].bulletCount);
        pSMG->setSilver(rows[r].silver);
        pSMG->setCreateType((Item::CreateType)rows[r].createType);

        switch (storage) {
        case STORAGE_ZONE: {
            Tile& pTile = pZone->getTile(x, y);
            Assert(!pTile.hasItem());
            pTile.addItem(pSMG);
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
void SMGLoader::load(StorageID_t storageID, Inventory* pInventory)

{
    __BEGIN_TRY

    Statement* pStmt;

    BEGIN_DB {}
    END_DB(pStmt)

    __END_CATCH
}
