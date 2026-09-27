//////////////////////////////////////////////////////////////////////////////
// Filename    : Helm.cpp
// Written By  : Elca
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "Helm.h"

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

ItemID_t Helm::m_ItemIDRegistry = 0;
Mutex Helm::m_Mutex;

//--------------------------------------------------------------------------------
// constructor
//--------------------------------------------------------------------------------
Helm::Helm()

//: m_ItemType(0), m_Durability(0)
{
    setItemType(0);
    setDurability(0);
}

Helm::Helm(ItemType_t itemType, const list<OptionType_t>& optionType)

//: m_ItemType(itemType), m_OptionType(optionType), m_Durability(0)
{
    setItemType(itemType);
    setOptionType(optionType);

    setDurability(computeMaxDurability(this));

    if (!de::gameContext().itemInfos().isPossibleItem(getItemClass(), getItemType(), getOptionTypeList())) {
        filelog("itembug.log", "Helm::Helm() : Invalid item type or option type");
        throw Error("Helm::Helm() : Invalid item type or optionType");
    }
}


//--------------------------------------------------------------------------------
// create item
//--------------------------------------------------------------------------------
void Helm::create(const string& ownerID, Storage storage, StorageID_t storageID, BYTE x, BYTE y, ItemID_t itemID)

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

    defaultItemObjectRepository().insertGear(GEAR_HELM, m_ItemID, m_ObjectID, getItemType(), ownerID, (int)storage,
                                             storageID, (int)x, (int)y, optionField, getDurability(), (int)getGrade(),
                                             (int)m_CreateType);

    __END_CATCH
}


//--------------------------------------------------------------------------------
// save item
//--------------------------------------------------------------------------------
void Helm::tinysave(const char* field) const

{
    __BEGIN_TRY

    defaultItemObjectRepository().tinysaveGear(GEAR_HELM, field, m_ItemID);

    __END_CATCH
}

//--------------------------------------------------------------------------------
// save item
//--------------------------------------------------------------------------------
void Helm::save(const string& ownerID, Storage storage, StorageID_t storageID, BYTE x, BYTE y)

{
    __BEGIN_TRY

    string optionField;
    setOptionTypeToField(getOptionTypeList(), optionField);

    defaultItemObjectRepository().updateGear(GEAR_HELM, m_ObjectID, getItemType(), ownerID, (int)storage, storageID,
                                             (int)x, (int)y, optionField, getDurability(), (int)getGrade(),
                                             (int)getEnchantLevel(), m_ItemID);

    __END_CATCH
}


//--------------------------------------------------------------------------------
// get debug string
//--------------------------------------------------------------------------------
string Helm::toString() const

{
    StringStream msg;

    msg << "Helm(" << "ItemID:" << m_ItemID << ",ItemType:" << (int)getItemType()
        << ",OptionType:" << getOptionTypeToString(getOptionTypeList()).c_str()
        << ",Durability:" << (int)getDurability() << ",EnchantLevel:" << (int)getEnchantLevel() << ")";

    return msg.toString();
}


//--------------------------------------------------------------------------------
// get debug string
//--------------------------------------------------------------------------------
string HelmInfo::toString() const

{
    StringStream msg;

    msg << "HelmInfo(" << "ItemType:" << m_ItemType << ",Name:" << m_Name << ",EName:" << m_EName
        << ",Price:" << m_Price << ",VolumeType:" << Volume2String[m_VolumeType] << ",Weight:" << m_Weight
        << ",Description:" << m_Description << ",Durability:" << m_Durability << ",DefenseBonus:" << m_DefenseBonus
        << ")";

    return msg.toString();
}


//--------------------------------------------------------------------------------
// load from DB
//--------------------------------------------------------------------------------
void HelmInfoManager::load()

{
    __BEGIN_TRY

    m_InfoCount = defaultItemObjectRepository().loadMaxGearType(GEAR_HELM);

    m_pItemInfos = new ItemInfo*[m_InfoCount + 1];

    for (uint i = 0; i <= m_InfoCount; i++)
        m_pItemInfos[i] = NULL;

    vector<GearInfoRow> rows = defaultItemObjectRepository().loadGearInfos(GEAR_HELM);

    for (size_t r = 0; r < rows.size(); r++) {
        HelmInfo* pHelmInfo = new HelmInfo();

        pHelmInfo->setItemType(rows[r].itemType);
        pHelmInfo->setName(rows[r].name);
        pHelmInfo->setEName(rows[r].ename);
        pHelmInfo->setPrice(rows[r].price);
        pHelmInfo->setVolumeType(rows[r].volume);
        pHelmInfo->setWeight(rows[r].weight);
        pHelmInfo->setRatio(rows[r].ratio);
        pHelmInfo->setDurability(rows[r].durability);
        pHelmInfo->setDefenseBonus(rows[r].defense);
        pHelmInfo->setProtectionBonus(rows[r].protection);
        pHelmInfo->setReqAbility(rows[r].reqAbility);
        pHelmInfo->setItemLevel(rows[r].itemLevel);
        pHelmInfo->setDefaultOptions(rows[r].defaultOption);
        pHelmInfo->setUpgradeRatio(rows[r].upgradeRatio);
        pHelmInfo->setUpgradeCrashPercent(rows[r].upgradeCrashPercent);
        pHelmInfo->setNextOptionRatio(rows[r].nextOptionRatio);
        pHelmInfo->setNextItemType(rows[r].nextItemType);
        pHelmInfo->setDowngradeRatio(rows[r].downgradeRatio);

        addItemInfo(pHelmInfo);
    }

    __END_CATCH
}


//--------------------------------------------------------------------------------
// load to creature
//--------------------------------------------------------------------------------
void HelmLoader::load(Creature* pCreature)

{
    __BEGIN_TRY

    Assert(pCreature != NULL);

    vector<GearObjectRow> rows = defaultItemObjectRepository().loadGearOfOwner(GEAR_HELM, pCreature->getName());

    for (size_t r = 0; r < rows.size(); r++) {
        try {
            Helm* pHelm = new Helm();

            pHelm->setItemID(rows[r].itemID);
            pHelm->setObjectID(rows[r].objectID);
            pHelm->setItemType(rows[r].itemType);

            if (de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_HELM, pHelm->getItemType())->isUnique())
                pHelm->setUnique();

            Storage storage = (Storage)rows[r].storage;
            BYTE x = rows[r].x;
            BYTE y = rows[r].y;

            string optionField = rows[r].optionField;
            list<OptionType_t> optionTypes;
            setOptionTypeFromField(optionTypes, optionField);
            pHelm->setOptionType(optionTypes);

            pHelm->setDurability(rows[r].durability);
            pHelm->setGrade(rows[r].grade);
            pHelm->setEnchantLevel(rows[r].enchantLevel);
            pHelm->setCreateType((Item::CreateType)rows[r].createType);

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
                if (pInventory->canAddingEx(x, y, pHelm)) {
                    pInventory->addItemEx(x, y, pHelm);
                } else {
                    processItemBugEx(pCreature, pHelm);
                }
                break;

            case STORAGE_GEAR:
                if (pCreature->isSlayer()) {
                    if (!pSlayer->isWear((Slayer::WearPart)x)) {
                        pSlayer->wearItem((Slayer::WearPart)x, pHelm);
                    } else {
                        processItemBugEx(pCreature, pHelm);
                    }
                } else if (pCreature->isVampire()) {
                    processItemBugEx(pCreature, pHelm);
                }
                break;

            case STORAGE_BELT:
                processItemBugEx(pCreature, pHelm);
                break;

            case STORAGE_EXTRASLOT:
                if (pCreature->isSlayer())
                    pSlayer->addItemToExtraInventorySlot(pHelm);
                else if (pCreature->isVampire())
                    pVampire->addItemToExtraInventorySlot(pHelm);
                break;

            case STORAGE_MOTORCYCLE:
                processItemBugEx(pCreature, pHelm);
                break;

            case STORAGE_STASH:
                if (pStash->isExist(x, y)) {
                    processItemBugEx(pCreature, pHelm);
                } else
                    pStash->insert(x, y, pHelm);
                break;

            case STORAGE_GARBAGE:
                processItemBug(pCreature, pHelm);
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
void HelmLoader::load(Zone* pZone)

{
    __BEGIN_TRY

    Assert(pZone != NULL);

    vector<GearZoneObjectRow> rows =
        defaultItemObjectRepository().loadGearInZone(GEAR_HELM, (int)STORAGE_ZONE, pZone->getZoneID());

    for (size_t r = 0; r < rows.size(); r++) {
        Helm* pHelm = new Helm();

        pHelm->setItemID(rows[r].itemID);
        pHelm->setObjectID(rows[r].objectID);
        pHelm->setItemType(rows[r].itemType);

        Storage storage = (Storage)rows[r].storage;
        BYTE x = rows[r].x;
        BYTE y = rows[r].y;

        string optionField = rows[r].optionField;
        list<OptionType_t> optionTypes;
        setOptionTypeFromField(optionTypes, optionField);
        pHelm->setOptionType(optionTypes);

        pHelm->setDurability(rows[r].durability);
        pHelm->setEnchantLevel(rows[r].enchantLevel);
        pHelm->setCreateType((Item::CreateType)rows[r].createType);


        switch (storage) {
        case STORAGE_ZONE: {
            Tile& pTile = pZone->getTile(x, y);
            Assert(!pTile.hasItem());
            pTile.addItem(pHelm);
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
void HelmLoader::load(StorageID_t storageID, Inventory* pInventory)

{
    __BEGIN_TRY

    Statement* pStmt;

    BEGIN_DB {}
    END_DB(pStmt)

    __END_CATCH
}
