//////////////////////////////////////////////////////////////////////////////
// Filename    : OustersChakram.cpp
// Written By  :
// Description :
//////////////////////////////////////////////////////////////////////////////

// include files
#include "OustersChakram.h"

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

ItemID_t OustersChakram::m_ItemIDRegistry = 0;
Mutex OustersChakram::m_Mutex;

//--------------------------------------------------------------------------------
// constructor
//--------------------------------------------------------------------------------
OustersChakram::OustersChakram()

{
    setItemType(0);
    setDurability(0);
    setBonusDamage(0);
}

OustersChakram::OustersChakram(ItemType_t itemType, const list<OptionType_t>& optionType)

//: m_OptionType( optionType )
{
    setItemType(itemType);
    setOptionType(optionType);

    setDurability(computeMaxDurability(this));

    if (!de::gameContext().itemInfos().isPossibleItem(getItemClass(), getItemType(), getOptionTypeList())) {
        filelog("itembug.log", "OustersChakram::OustersChakram() : Invalid item type or option type");
        throw Error("OustersChakram::OustersChakram() : Invalid item type or optionType");
    }
}

//--------------------------------------------------------------------------------
// create item
//--------------------------------------------------------------------------------
void OustersChakram::create(const string& ownerID, Storage storage, StorageID_t storageID, BYTE x, BYTE y,
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

    defaultItemObjectRepository().insertGear(GEAR_OUSTERS_CHAKRAM, m_ItemID, m_ObjectID, getItemType(), ownerID,
                                             (int)storage, storageID, (int)x, (int)y, optionField, getDurability(),
                                             getGrade(), (int)m_CreateType);

    __END_CATCH
}

//--------------------------------------------------------------------------------
// save item
//--------------------------------------------------------------------------------
void OustersChakram::tinysave(const char* field) const

{
    __BEGIN_TRY

    defaultItemObjectRepository().tinysaveGear(GEAR_OUSTERS_CHAKRAM, field, m_ItemID);

    __END_CATCH
}

//--------------------------------------------------------------------------------
// save item
//--------------------------------------------------------------------------------
void OustersChakram::save(const string& ownerID, Storage storage, StorageID_t storageID, BYTE x, BYTE y)

{
    __BEGIN_TRY

    string optionField;
    setOptionTypeToField(getOptionTypeList(), optionField);

    defaultItemObjectRepository().updateGear(GEAR_OUSTERS_CHAKRAM, m_ObjectID, getItemType(), ownerID, (int)storage,
                                             storageID, (int)x, (int)y, optionField, getDurability(), getGrade(),
                                             (int)getEnchantLevel(), m_ItemID);

    __END_CATCH
}


//--------------------------------------------------------------------------------
// get debug string
//--------------------------------------------------------------------------------
string OustersChakram::toString() const

{
    StringStream msg;

    msg << "OustersChakram(" << "ItemID:" << m_ItemID << ",ItemType:" << (int)getItemType()
        << ",OptionType:" << getOptionTypeToString(getOptionTypeList()).c_str()
        << ",Durability:" << (int)getDurability() << ",EnchantLevel:" << (int)getEnchantLevel() << ")";

    return msg.toString();
}


//--------------------------------------------------------------------------------
// get debug string
//--------------------------------------------------------------------------------
string OustersChakramInfo::toString() const

{
    StringStream msg;

    msg << "OustersChakramInfo(" << "ItemType:" << m_ItemType << ",Name:" << m_Name << ",EName:" << m_EName
        << ",Price:" << m_Price << ",VolumeType:" << Volume2String[m_VolumeType] << ",Weight:" << m_Weight
        << ",Description:" << m_Description << ",Durability:" << m_Durability << ",minDamage:" << m_MinDamage
        << ",maxDamage:" << m_MaxDamage << ",ReqAbility:" << ")";

    return msg.toString();
}


//--------------------------------------------------------------------------------
// load from DB
//--------------------------------------------------------------------------------
void OustersChakramInfoManager::load()

{
    __BEGIN_TRY

    m_InfoCount = defaultItemObjectRepository().loadMaxGearType(GEAR_OUSTERS_CHAKRAM);

    m_pItemInfos = new ItemInfo*[m_InfoCount + 1];

    for (uint i = 0; i <= m_InfoCount; i++)
        m_pItemInfos[i] = NULL;

    vector<WeaponInfoRow> rows = defaultItemObjectRepository().loadWeaponInfos(GEAR_OUSTERS_CHAKRAM);

    for (size_t r = 0; r < rows.size(); r++) {
        OustersChakramInfo* pOustersChakramInfo = new OustersChakramInfo();

        pOustersChakramInfo->setItemType(rows[r].itemType);
        pOustersChakramInfo->setName(rows[r].name);
        pOustersChakramInfo->setEName(rows[r].ename);
        pOustersChakramInfo->setPrice(rows[r].price);
        pOustersChakramInfo->setVolumeType(rows[r].volume);
        pOustersChakramInfo->setWeight(rows[r].weight);
        pOustersChakramInfo->setRatio(rows[r].ratio);
        pOustersChakramInfo->setDurability(rows[r].durability);
        pOustersChakramInfo->setMinDamage(rows[r].minDamage);
        pOustersChakramInfo->setMaxDamage(rows[r].maxDamage);
        pOustersChakramInfo->setSpeed(rows[r].speed);
        pOustersChakramInfo->setReqAbility(rows[r].reqAbility);
        pOustersChakramInfo->setItemLevel(rows[r].itemLevel);
        pOustersChakramInfo->setCriticalBonus(rows[r].criticalBonus);
        pOustersChakramInfo->setDefaultOptions(rows[r].defaultOption);
        pOustersChakramInfo->setUpgradeRatio(rows[r].upgradeRatio);
        pOustersChakramInfo->setUpgradeCrashPercent(rows[r].upgradeCrashPercent);
        pOustersChakramInfo->setNextOptionRatio(rows[r].nextOptionRatio);
        pOustersChakramInfo->setNextItemType(rows[r].nextItemType);
        pOustersChakramInfo->setDowngradeRatio(rows[r].downgradeRatio);

        addItemInfo(pOustersChakramInfo);
    }

    __END_CATCH
}


//--------------------------------------------------------------------------------
// load to creature
//--------------------------------------------------------------------------------
void OustersChakramLoader::load(Creature* pCreature)

{
    __BEGIN_TRY

    Assert(pCreature != NULL);

    vector<GearObjectRow> rows =
        defaultItemObjectRepository().loadGearOfOwner(GEAR_OUSTERS_CHAKRAM, pCreature->getName());

    for (size_t r = 0; r < rows.size(); r++) {
        try {
            OustersChakram* pOustersChakram = new OustersChakram();

            pOustersChakram->setItemID(rows[r].itemID);
            pOustersChakram->setObjectID(rows[r].objectID);
            pOustersChakram->setItemType(rows[r].itemType);

            if (de::gameContext()
                    .itemInfos()
                    .getItemInfo(Item::ITEM_CLASS_OUSTERS_CHAKRAM, pOustersChakram->getItemType())
                    ->isUnique())
                pOustersChakram->setUnique();

            Storage storage = (Storage)rows[r].storage;
            BYTE x = rows[r].x;
            BYTE y = rows[r].y;

            string optionField = rows[r].optionField;
            list<OptionType_t> optionTypes;
            setOptionTypeFromField(optionTypes, optionField);
            pOustersChakram->setOptionType(optionTypes);

            pOustersChakram->setDurability(rows[r].durability);
            pOustersChakram->setGrade(rows[r].grade);
            pOustersChakram->setEnchantLevel(rows[r].enchantLevel);
            pOustersChakram->setCreateType((Item::CreateType)rows[r].createType);

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
                if (pInventory->canAddingEx(x, y, pOustersChakram)) {
                    pInventory->addItemEx(x, y, pOustersChakram);
                } else {
                    processItemBugEx(pCreature, pOustersChakram);
                }
                break;

            case STORAGE_GEAR:
                if (pCreature->isSlayer() || pCreature->isVampire()) {
                    processItemBugEx(pCreature, pOustersChakram);
                } else if (pCreature->isOusters()) {
                    if (!pOusters->isWear((Ousters::WearPart)x)) {
                        pOusters->wearItem((Ousters::WearPart)x, pOustersChakram);
                    } else {
                        processItemBugEx(pCreature, pOustersChakram);
                    }
                }
                break;

            case STORAGE_BELT:
                processItemBugEx(pCreature, pOustersChakram);
                break;

            case STORAGE_EXTRASLOT:
                if (pCreature->isSlayer())
                    pSlayer->addItemToExtraInventorySlot(pOustersChakram);
                else if (pCreature->isVampire())
                    pVampire->addItemToExtraInventorySlot(pOustersChakram);
                else if (pCreature->isOusters())
                    pOusters->addItemToExtraInventorySlot(pOustersChakram);
                break;

            case STORAGE_MOTORCYCLE:
                processItemBugEx(pCreature, pOustersChakram);
                break;

            case STORAGE_STASH:
                if (pStash->isExist(x, y)) {
                    processItemBugEx(pCreature, pOustersChakram);
                } else {
                    pStash->insert(x, y, pOustersChakram);
                }
                break;

            case STORAGE_GARBAGE:
                processItemBug(pCreature, pOustersChakram);
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
void OustersChakramLoader::load(Zone* pZone)

{
    __BEGIN_TRY

    Assert(pZone != NULL);

    vector<GearZoneObjectRow> rows =
        defaultItemObjectRepository().loadGearInZone(GEAR_OUSTERS_CHAKRAM, (int)STORAGE_ZONE, pZone->getZoneID());

    for (size_t r = 0; r < rows.size(); r++) {
        OustersChakram* pOustersChakram = new OustersChakram();

        pOustersChakram->setItemID(rows[r].itemID);
        pOustersChakram->setObjectID(rows[r].objectID);
        pOustersChakram->setItemType(rows[r].itemType);

        Storage storage = (Storage)rows[r].storage;
        BYTE x = rows[r].x;
        BYTE y = rows[r].y;

        string optionField = rows[r].optionField;
        list<OptionType_t> optionTypes;
        setOptionTypeFromField(optionTypes, optionField);
        pOustersChakram->setOptionType(optionTypes);

        pOustersChakram->setDurability(rows[r].durability);
        pOustersChakram->setEnchantLevel(rows[r].enchantLevel);
        pOustersChakram->setCreateType((Item::CreateType)rows[r].createType);

        switch (storage) {
        case STORAGE_ZONE: {
            Tile& pTile = pZone->getTile(x, y);
            Assert(!pTile.hasItem());
            pTile.addItem(pOustersChakram);
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
void OustersChakramLoader::load(StorageID_t storageID, Inventory* pInventory)

{
    __BEGIN_TRY

    Statement* pStmt;

    BEGIN_DB {}
    END_DB(pStmt)

    __END_CATCH
}
