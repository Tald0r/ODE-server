//////////////////////////////////////////////////////////////////////////////
// Filename    : Mitten.cpp
// Written By  : Elca
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "Mitten.h"

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

ItemID_t Mitten::m_ItemIDRegistry = 0;
Mutex Mitten::m_Mutex;

//--------------------------------------------------------------------------------
// constructor
//--------------------------------------------------------------------------------
Mitten::Mitten()

{
    setItemType(0);
    setDurability(0);
}

Mitten::Mitten(ItemType_t itemType, const list<OptionType_t>& optionType)

{
    setItemType(itemType);
    setOptionType(optionType);

    setDurability(computeMaxDurability(this));

    if (!de::gameContext().itemInfos().isPossibleItem(getItemClass(), getItemType(), getOptionTypeList())) {
        filelog("itembug.log", "Mitten::Mitten() : Invalid item type or option type");
        throw Error("Mitten::Mitten() : Invalid item type or optionType");
    }
}


//--------------------------------------------------------------------------------
// create item
//--------------------------------------------------------------------------------
void Mitten::create(const string& ownerID, Storage storage, StorageID_t storageID, BYTE x, BYTE y, ItemID_t itemID)

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

    defaultItemObjectRepository().insertGear(GEAR_MITTEN, m_ItemID, m_ObjectID, getItemType(), ownerID, (int)storage,
                                             storageID, (int)x, (int)y, optionField, getDurability(), getGrade(),
                                             (int)m_CreateType);

    __END_CATCH
}


//--------------------------------------------------------------------------------
// save item
//--------------------------------------------------------------------------------
void Mitten::tinysave(const char* field) const

{
    __BEGIN_TRY

    defaultItemObjectRepository().tinysaveGear(GEAR_MITTEN, field, m_ItemID);

    __END_CATCH
}

//--------------------------------------------------------------------------------
// save item
//--------------------------------------------------------------------------------
void Mitten::save(const string& ownerID, Storage storage, StorageID_t storageID, BYTE x, BYTE y)

{
    __BEGIN_TRY

    string optionField;
    setOptionTypeToField(getOptionTypeList(), optionField);

    defaultItemObjectRepository().updateGear(GEAR_MITTEN, m_ObjectID, getItemType(), ownerID, (int)storage, storageID,
                                             (int)x, (int)y, optionField, getDurability(), getGrade(),
                                             (int)getEnchantLevel(), m_ItemID);

    __END_CATCH
}


//--------------------------------------------------------------------------------
// get debug string
//--------------------------------------------------------------------------------
string Mitten::toString() const

{
    StringStream msg;

    msg << "Mitten(" << "ItemID:" << m_ItemID << ",ItemType:" << (int)getItemType()
        << ",OptionType:" << getOptionTypeToString(getOptionTypeList()).c_str()
        << ",Durability:" << (int)getDurability() << ",EnchantLevel:" << (int)getEnchantLevel() << ")";

    return msg.toString();
}

//--------------------------------------------------------------------------------
// get debug string
//--------------------------------------------------------------------------------
string MittenInfo::toString() const

{
    StringStream msg;

    msg << "MittenInfo(" << "ItemType:" << m_ItemType << ",Name:" << m_Name << ",EName:" << m_EName
        << ",Price:" << m_Price << ",VolumeType:" << Volume2String[m_VolumeType] << ",Weight:" << m_Weight
        << ",Description:" << m_Description << ",Durability:" << m_Durability << ",DefenseBonus:" << m_DefenseBonus
        << ")";

    return msg.toString();
}


//--------------------------------------------------------------------------------
// load from DB
//--------------------------------------------------------------------------------
void MittenInfoManager::load()

{
    __BEGIN_TRY

    m_InfoCount = defaultItemObjectRepository().loadMaxGearType(GEAR_MITTEN);

    m_pItemInfos = new ItemInfo*[m_InfoCount + 1];

    for (uint i = 0; i <= m_InfoCount; i++)
        m_pItemInfos[i] = NULL;

    vector<GearInfoRow> rows = defaultItemObjectRepository().loadGearInfos(GEAR_MITTEN);

    for (size_t r = 0; r < rows.size(); r++) {
        MittenInfo* pMittenInfo = new MittenInfo();

        pMittenInfo->setItemType(rows[r].itemType);
        pMittenInfo->setName(rows[r].name);
        pMittenInfo->setEName(rows[r].ename);
        pMittenInfo->setPrice(rows[r].price);
        pMittenInfo->setVolumeType(rows[r].volume);
        pMittenInfo->setWeight(rows[r].weight);
        pMittenInfo->setRatio(rows[r].ratio);
        pMittenInfo->setDurability(rows[r].durability);
        pMittenInfo->setDefenseBonus(rows[r].defense);
        pMittenInfo->setProtectionBonus(rows[r].protection);
        pMittenInfo->setReqAbility(rows[r].reqAbility);
        pMittenInfo->setItemLevel(rows[r].itemLevel);
        pMittenInfo->setDefaultOptions(rows[r].defaultOption);
        pMittenInfo->setUpgradeRatio(rows[r].upgradeRatio);
        pMittenInfo->setUpgradeCrashPercent(rows[r].upgradeCrashPercent);
        pMittenInfo->setNextOptionRatio(rows[r].nextOptionRatio);
        pMittenInfo->setNextItemType(rows[r].nextItemType);
        pMittenInfo->setDowngradeRatio(rows[r].downgradeRatio);

        addItemInfo(pMittenInfo);
    }

    __END_CATCH
}


//--------------------------------------------------------------------------------
// load to creature
//--------------------------------------------------------------------------------
void MittenLoader::load(Creature* pCreature)

{
    __BEGIN_TRY

    Assert(pCreature != NULL);

    vector<GearObjectRow> rows = defaultItemObjectRepository().loadGearOfOwner(GEAR_MITTEN, pCreature->getName());

    for (size_t r = 0; r < rows.size(); r++) {
        try {
            Mitten* pMitten = new Mitten();

            pMitten->setItemID(rows[r].itemID);
            pMitten->setObjectID(rows[r].objectID);
            pMitten->setItemType(rows[r].itemType);

            if (de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_MITTEN, pMitten->getItemType())->isUnique())
                pMitten->setUnique();

            Storage storage = (Storage)rows[r].storage;
            BYTE x = rows[r].x;
            BYTE y = rows[r].y;

            string optionField = rows[r].optionField;
            list<OptionType_t> optionTypes;
            setOptionTypeFromField(optionTypes, optionField);
            pMitten->setOptionType(optionTypes);

            pMitten->setDurability(rows[r].durability);
            pMitten->setGrade(rows[r].grade);
            pMitten->setEnchantLevel(rows[r].enchantLevel);
            pMitten->setCreateType((Item::CreateType)rows[r].createType);

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
                if (pInventory->canAddingEx(x, y, pMitten)) {
                    pInventory->addItemEx(x, y, pMitten);
                } else {
                    processItemBugEx(pCreature, pMitten);
                }
                break;

            case STORAGE_GEAR:
                if (pCreature->isSlayer() || pCreature->isVampire()) {
                    processItemBugEx(pCreature, pMitten);
                } else if (pCreature->isOusters()) {
                    if (!pOusters->isWear((Ousters::WearPart)x)) {
                        pOusters->wearItem((Ousters::WearPart)x, pMitten);
                    } else {
                        processItemBugEx(pCreature, pMitten);
                    }
                }
                break;

            case STORAGE_BELT:
                processItemBugEx(pCreature, pMitten);
                break;

            case STORAGE_EXTRASLOT:
                if (pCreature->isSlayer())
                    pSlayer->addItemToExtraInventorySlot(pMitten);
                else if (pCreature->isVampire())
                    pVampire->addItemToExtraInventorySlot(pMitten);
                else if (pCreature->isOusters())
                    pOusters->addItemToExtraInventorySlot(pMitten);
                break;

            case STORAGE_MOTORCYCLE:
                processItemBugEx(pCreature, pMitten);
                break;

            case STORAGE_STASH:
                if (pStash->isExist(x, y)) {
                    processItemBugEx(pCreature, pMitten);
                } else
                    pStash->insert(x, y, pMitten);
                break;

            case STORAGE_GARBAGE:
                processItemBug(pCreature, pMitten);
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
void MittenLoader::load(Zone* pZone)

{
    __BEGIN_TRY

    __END_CATCH
}


//--------------------------------------------------------------------------------
// load to inventory
//--------------------------------------------------------------------------------
void MittenLoader::load(StorageID_t storageID, Inventory* pInventory)

{
    __BEGIN_TRY

    __END_CATCH
}
