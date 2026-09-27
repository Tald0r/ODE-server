//////////////////////////////////////////////////////////////////////////////
// Filename    : Persona.cpp
// Written By  : Elca
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "Persona.h"

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

ItemID_t Persona::m_ItemIDRegistry = 0;
Mutex Persona::m_Mutex;

//--------------------------------------------------------------------------------
// constructor
//--------------------------------------------------------------------------------
Persona::Persona()

{
    setItemType(0);
    setDurability(0);
}

Persona::Persona(ItemType_t itemType, const list<OptionType_t>& optionType)

{
    setItemType(itemType);
    setOptionType(optionType);

    setDurability(computeMaxDurability(this));

    if (!de::gameContext().itemInfos().isPossibleItem(getItemClass(), getItemType(), getOptionTypeList())) {
        filelog("itembug.log", "Persona::Persona() : Invalid item type or option type");
        throw Error("Persona::Persona() : Invalid item type or optionType");
    }
}


//--------------------------------------------------------------------------------
// create item
//--------------------------------------------------------------------------------
void Persona::create(const string& ownerID, Storage storage, StorageID_t storageID, BYTE x, BYTE y, ItemID_t itemID)

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

    defaultItemObjectRepository().insertGear(GEAR_PERSONA, m_ItemID, m_ObjectID, getItemType(), ownerID, (int)storage,
                                             storageID, (int)x, (int)y, optionField, getDurability(), getGrade(),
                                             (int)m_CreateType);

    __END_CATCH
}


//--------------------------------------------------------------------------------
// save item
//--------------------------------------------------------------------------------
void Persona::tinysave(const char* field) const

{
    __BEGIN_TRY

    defaultItemObjectRepository().tinysaveGear(GEAR_PERSONA, field, m_ItemID);

    __END_CATCH
}

//--------------------------------------------------------------------------------
// save item
//--------------------------------------------------------------------------------
void Persona::save(const string& ownerID, Storage storage, StorageID_t storageID, BYTE x, BYTE y)

{
    __BEGIN_TRY

    string optionField;
    setOptionTypeToField(getOptionTypeList(), optionField);

    defaultItemObjectRepository().updateGear(GEAR_PERSONA, m_ObjectID, getItemType(), ownerID, (int)storage, storageID,
                                             (int)x, (int)y, optionField, getDurability(), getGrade(),
                                             (int)getEnchantLevel(), m_ItemID);

    __END_CATCH
}


//--------------------------------------------------------------------------------
// get debug string
//--------------------------------------------------------------------------------
string Persona::toString() const

{
    StringStream msg;

    msg << "Persona(" << "ItemID:" << m_ItemID << ",ItemType:" << (int)getItemType()
        << ",OptionType:" << getOptionTypeToString(getOptionTypeList()).c_str()
        << ",Durability:" << (int)getDurability() << ",EnchantLevel:" << (int)getEnchantLevel() << ")";

    return msg.toString();
}

//--------------------------------------------------------------------------------
// get debug string
//--------------------------------------------------------------------------------
string PersonaInfo::toString() const

{
    StringStream msg;

    msg << "PersonaInfo(" << "ItemType:" << m_ItemType << ",Name:" << m_Name << ",EName:" << m_EName
        << ",Price:" << m_Price << ",VolumeType:" << Volume2String[m_VolumeType] << ",Weight:" << m_Weight
        << ",Description:" << m_Description << ",Durability:" << m_Durability << ",DefenseBonus:" << m_DefenseBonus
        << ")";

    return msg.toString();
}


//--------------------------------------------------------------------------------
// load from DB
//--------------------------------------------------------------------------------
void PersonaInfoManager::load()

{
    __BEGIN_TRY

    m_InfoCount = defaultItemObjectRepository().loadMaxGearType(GEAR_PERSONA);

    m_pItemInfos = new ItemInfo*[m_InfoCount + 1];

    for (uint i = 0; i <= m_InfoCount; i++)
        m_pItemInfos[i] = NULL;

    vector<GearInfoNoRatioRow> rows = defaultItemObjectRepository().loadGearInfosNoRatio(GEAR_PERSONA);

    for (size_t r = 0; r < rows.size(); r++) {
        PersonaInfo* pPersonaInfo = new PersonaInfo();

        pPersonaInfo->setItemType(rows[r].itemType);
        pPersonaInfo->setName(rows[r].name);
        pPersonaInfo->setEName(rows[r].ename);
        pPersonaInfo->setPrice(rows[r].price);
        pPersonaInfo->setVolumeType(rows[r].volume);
        pPersonaInfo->setWeight(rows[r].weight);
        pPersonaInfo->setRatio(rows[r].ratio);
        pPersonaInfo->setDurability(rows[r].durability);
        pPersonaInfo->setDefenseBonus(rows[r].defense);
        pPersonaInfo->setProtectionBonus(rows[r].protection);
        pPersonaInfo->setReqAbility(rows[r].reqAbility);
        pPersonaInfo->setItemLevel(rows[r].itemLevel);
        pPersonaInfo->setDefaultOptions(rows[r].defaultOption);
        pPersonaInfo->setUpgradeCrashPercent(rows[r].upgradeCrashPercent);
        pPersonaInfo->setNextOptionRatio(rows[r].nextOptionRatio);
        pPersonaInfo->setNextItemType(rows[r].nextItemType);

        addItemInfo(pPersonaInfo);
    }

    __END_CATCH
}


//--------------------------------------------------------------------------------
// load to creature
//--------------------------------------------------------------------------------
void PersonaLoader::load(Creature* pCreature)

{
    __BEGIN_TRY

    Assert(pCreature != NULL);

    vector<GearObjectRow> rows = defaultItemObjectRepository().loadGearOfOwner(GEAR_PERSONA, pCreature->getName());

    for (size_t r = 0; r < rows.size(); r++) {
        try {
            Persona* pPersona = new Persona();

            pPersona->setItemID(rows[r].itemID);
            pPersona->setObjectID(rows[r].objectID);
            pPersona->setItemType(rows[r].itemType);

            if (de::gameContext()
                    .itemInfos()
                    .getItemInfo(Item::ITEM_CLASS_PERSONA, pPersona->getItemType())
                    ->isUnique())
                pPersona->setUnique();

            Storage storage = (Storage)rows[r].storage;
            BYTE x = rows[r].x;
            BYTE y = rows[r].y;


            string optionField = rows[r].optionField;
            list<OptionType_t> optionTypes;
            setOptionTypeFromField(optionTypes, optionField);
            pPersona->setOptionType(optionTypes);

            pPersona->setDurability(rows[r].durability);
            pPersona->setGrade(rows[r].grade);
            pPersona->setEnchantLevel(rows[r].enchantLevel);
            pPersona->setCreateType((Item::CreateType)rows[r].createType);

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
                if (pInventory->canAddingEx(x, y, pPersona)) {
                    pInventory->addItemEx(x, y, pPersona);
                } else {
                    processItemBugEx(pCreature, pPersona);
                }
                break;

            case STORAGE_GEAR:
                if (pCreature->isSlayer()) {
                    processItemBugEx(pCreature, pPersona);
                } else if (pCreature->isVampire()) {
                    if (!pVampire->isWear((Vampire::WearPart)x)) {
                        pVampire->wearItem((Vampire::WearPart)x, pPersona);
                    } else {
                        processItemBugEx(pCreature, pPersona);
                    }
                }
                break;

            case STORAGE_BELT:
                processItemBugEx(pCreature, pPersona);
                break;

            case STORAGE_EXTRASLOT:
                if (pCreature->isSlayer())
                    pSlayer->addItemToExtraInventorySlot(pPersona);
                else if (pCreature->isVampire())
                    pVampire->addItemToExtraInventorySlot(pPersona);
                break;

            case STORAGE_MOTORCYCLE:
                processItemBugEx(pCreature, pPersona);
                break;

            case STORAGE_STASH:
                if (pStash->isExist(x, y)) {
                    processItemBugEx(pCreature, pPersona);
                } else
                    pStash->insert(x, y, pPersona);
                break;

            case STORAGE_GARBAGE:
                processItemBug(pCreature, pPersona);
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
void PersonaLoader::load(Zone* pZone)

{
    __BEGIN_TRY

    __END_CATCH
}


//--------------------------------------------------------------------------------
// load to inventory
//--------------------------------------------------------------------------------
void PersonaLoader::load(StorageID_t storageID, Inventory* pInventory)

{
    __BEGIN_TRY


    __END_CATCH
}
