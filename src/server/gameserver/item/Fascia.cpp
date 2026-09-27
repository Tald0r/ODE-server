//////////////////////////////////////////////////////////////////////////////
// Filename    : Fascia.cpp
// Written By  : Elca
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "Fascia.h"

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

ItemID_t Fascia::m_ItemIDRegistry = 0;
Mutex Fascia::m_Mutex;

//--------------------------------------------------------------------------------
// constructor
//--------------------------------------------------------------------------------
Fascia::Fascia()

{
    setItemType(0);
}

Fascia::Fascia(ItemType_t itemType, const list<OptionType_t>& optionType)

{
    setItemType(itemType);
    setOptionType(optionType);

    if (!de::gameContext().itemInfos().isPossibleItem(getItemClass(), getItemType(), getOptionTypeList())) {
        filelog("itembug.log", "Fascia::Fascia() : Invalid item type or option type");
        throw Error("Fascia::Fascia() : Invalid item type or optionType");
    }
}


//--------------------------------------------------------------------------------
// create item
//--------------------------------------------------------------------------------
void Fascia::create(const string& ownerID, Storage storage, StorageID_t storageID, BYTE x, BYTE y, ItemID_t itemID)

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

    defaultItemObjectRepository().insertOptionGradeItem(GEAR_FASCIA, m_ItemID, m_ObjectID, getItemType(), ownerID,
                                                        (int)storage, storageID, (int)x, (int)y, optionField,
                                                        (int)getGrade(), (int)m_CreateType);

    __END_CATCH
}


//--------------------------------------------------------------------------------
// save item
//--------------------------------------------------------------------------------
void Fascia::tinysave(const char* field) const

{
    __BEGIN_TRY

    defaultItemObjectRepository().tinysaveGear(GEAR_FASCIA, field, m_ItemID);

    __END_CATCH
}

//--------------------------------------------------------------------------------
// save item
//--------------------------------------------------------------------------------
void Fascia::save(const string& ownerID, Storage storage, StorageID_t storageID, BYTE x, BYTE y)

{
    __BEGIN_TRY

    string optionField;
    setOptionTypeToField(getOptionTypeList(), optionField);

    defaultItemObjectRepository().updateAmulet(GEAR_FASCIA, m_ObjectID, getItemType(), ownerID, (int)storage, storageID,
                                               (int)x, (int)y, optionField, getGrade(), (int)getEnchantLevel(),
                                               m_ItemID);

    __END_CATCH
}


//--------------------------------------------------------------------------------
// get debug string
//--------------------------------------------------------------------------------
string Fascia::toString() const

{
    StringStream msg;

    msg << "Fascia(" << "ItemID:" << m_ItemID << ",ItemType:" << (int)getItemType()
        << ",OptionType:" << getOptionTypeToString(getOptionTypeList()).c_str()
        << ",EnchantLevel:" << (int)getEnchantLevel() << ")";

    return msg.toString();
}

//--------------------------------------------------------------------------------
// get debug string
//--------------------------------------------------------------------------------
string FasciaInfo::toString() const

{
    StringStream msg;

    msg << "FasciaInfo(" << "ItemType:" << m_ItemType << ",Name:" << m_Name << ",EName:" << m_EName
        << ",Price:" << m_Price << ",VolumeType:" << Volume2String[m_VolumeType] << ",Weight:" << m_Weight
        << ",Description:" << m_Description << ",DefenseBonus:" << m_DefenseBonus << ")";

    return msg.toString();
}


//--------------------------------------------------------------------------------
// load from DB
//--------------------------------------------------------------------------------
void FasciaInfoManager::load()

{
    __BEGIN_TRY

    m_InfoCount = defaultItemObjectRepository().loadMaxGearType(GEAR_FASCIA);

    m_pItemInfos = new ItemInfo*[m_InfoCount + 1];

    for (uint i = 0; i <= m_InfoCount; i++)
        m_pItemInfos[i] = NULL;

    vector<GearInfoNoDurabilityRow> rows = defaultItemObjectRepository().loadGearInfosNoDurability(GEAR_FASCIA);

    for (size_t r = 0; r < rows.size(); r++) {
        FasciaInfo* pFasciaInfo = new FasciaInfo();

        pFasciaInfo->setItemType(rows[r].itemType);
        pFasciaInfo->setName(rows[r].name);
        pFasciaInfo->setEName(rows[r].ename);
        pFasciaInfo->setPrice(rows[r].price);
        pFasciaInfo->setVolumeType(rows[r].volume);
        pFasciaInfo->setWeight(rows[r].weight);
        pFasciaInfo->setRatio(rows[r].ratio);
        pFasciaInfo->setDefenseBonus(rows[r].defense);
        pFasciaInfo->setProtectionBonus(rows[r].protection);
        pFasciaInfo->setReqAbility(rows[r].reqAbility);
        pFasciaInfo->setItemLevel(rows[r].itemLevel);
        pFasciaInfo->setDefaultOptions(rows[r].defaultOption);
        pFasciaInfo->setUpgradeRatio(rows[r].upgradeRatio);
        pFasciaInfo->setUpgradeCrashPercent(rows[r].upgradeCrashPercent);
        pFasciaInfo->setNextOptionRatio(rows[r].nextOptionRatio);
        pFasciaInfo->setNextItemType(rows[r].nextItemType);
        pFasciaInfo->setDowngradeRatio(rows[r].downgradeRatio);

        addItemInfo(pFasciaInfo);
    }

    __END_CATCH
}


//--------------------------------------------------------------------------------
// load to creature
//--------------------------------------------------------------------------------
void FasciaLoader::load(Creature* pCreature)

{
    __BEGIN_TRY

    Assert(pCreature != NULL);

    vector<OptionGradeObjectRow> rows =
        defaultItemObjectRepository().loadOptionGradeOfOwner(GEAR_FASCIA, pCreature->getName());

    for (size_t r = 0; r < rows.size(); r++) {
        try {
            Fascia* pFascia = new Fascia();

            pFascia->setItemID(rows[r].itemID);
            pFascia->setObjectID(rows[r].objectID);
            pFascia->setItemType(rows[r].itemType);

            if (de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_FASCIA, pFascia->getItemType())->isUnique())
                pFascia->setUnique();

            Storage storage = (Storage)rows[r].storage;
            BYTE x = rows[r].x;
            BYTE y = rows[r].y;

            string optionField = rows[r].optionField;
            list<OptionType_t> optionTypes;
            setOptionTypeFromField(optionTypes, optionField);
            pFascia->setOptionType(optionTypes);

            pFascia->setGrade(rows[r].grade);
            pFascia->setEnchantLevel(rows[r].enchantLevel);
            pFascia->setCreateType((Item::CreateType)rows[r].createType);

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
                if (pInventory->canAddingEx(x, y, pFascia)) {
                    pInventory->addItemEx(x, y, pFascia);
                } else {
                    processItemBugEx(pCreature, pFascia);
                }
                break;

            case STORAGE_GEAR:
                if (pCreature->isSlayer() || pCreature->isVampire()) {
                    processItemBugEx(pCreature, pFascia);
                } else if (pCreature->isOusters()) {
                    if (!pOusters->isWear((Ousters::WearPart)x)) {
                        pOusters->wearItem((Ousters::WearPart)x, pFascia);
                    } else {
                        processItemBugEx(pCreature, pFascia);
                    }
                }
                break;

            case STORAGE_BELT:
                processItemBugEx(pCreature, pFascia);
                break;

            case STORAGE_EXTRASLOT:
                if (pCreature->isSlayer())
                    pSlayer->addItemToExtraInventorySlot(pFascia);
                else if (pCreature->isVampire())
                    pVampire->addItemToExtraInventorySlot(pFascia);
                else if (pCreature->isOusters())
                    pOusters->addItemToExtraInventorySlot(pFascia);
                break;

            case STORAGE_MOTORCYCLE:
                processItemBugEx(pCreature, pFascia);
                break;

            case STORAGE_STASH:
                if (pStash->isExist(x, y)) {
                    processItemBugEx(pCreature, pFascia);
                } else
                    pStash->insert(x, y, pFascia);
                break;

            case STORAGE_GARBAGE:
                processItemBug(pCreature, pFascia);
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
void FasciaLoader::load(Zone* pZone)

{
    __BEGIN_TRY

    __END_CATCH
}


//--------------------------------------------------------------------------------
// load to inventory
//--------------------------------------------------------------------------------
void FasciaLoader::load(StorageID_t storageID, Inventory* pInventory)

{
    __BEGIN_TRY

    __END_CATCH
}
