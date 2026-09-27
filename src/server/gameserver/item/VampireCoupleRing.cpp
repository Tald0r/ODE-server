//////////////////////////////////////////////////////////////////////////////
// Filename    : VampireCoupleRing.cpp
// Written By  : excel96
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "VampireCoupleRing.h"

#include <stdio.h>

#include "Belt.h"
#include "DB.h"
#include "FlagSet.h"
#include "GameContext.h"
#include "ItemInfoManager.h"
#include "ItemUtil.h"
#include "Motorcycle.h"
#include "PlayerCreature.h"
#include "Slayer.h"
#include "Stash.h"
#include "Vampire.h"
#include "couple/CoupleManager.h"
#include "repository/ItemObjectRepository.h"

ItemID_t VampireCoupleRing::m_ItemIDRegistry = 0;
Mutex VampireCoupleRing::m_Mutex;

//////////////////////////////////////////////////////////////////////////////
// class VampireCoupleRing member methods
//////////////////////////////////////////////////////////////////////////////

VampireCoupleRing::VampireCoupleRing()

{
    m_ItemType = 0;
}

VampireCoupleRing::VampireCoupleRing(ItemType_t itemType, const list<OptionType_t>& optionType)

    : m_ItemType(itemType), m_OptionType(optionType) {
    __BEGIN_TRY

    if (!de::gameContext().itemInfos().isPossibleItem(getItemClass(), m_ItemType, optionType)) {
        filelog("itembug.log", "VampireCoupleRing::VampireCoupleRing() : Invalid item type or option type");
        throw Error("VampireCoupleRing::VampireCoupleRing() : Invalid item type or optionType");
    }

    __END_CATCH
}

void VampireCoupleRing::create(const string& ownerID, Storage storage, StorageID_t storageID, BYTE x, BYTE y,
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
    setOptionTypeToField(m_OptionType, optionField);

    defaultItemObjectRepository().insertCoupleRing(GEAR_VAMPIRE_COUPLE_RING, m_ItemID, m_ObjectID, m_ItemType, ownerID,
                                                   (int)storage, storageID, (int)x, (int)y, optionField, getName(),
                                                   getPartnerItemID());

    __END_CATCH
}

//--------------------------------------------------------------------------------
// save item
//--------------------------------------------------------------------------------
void VampireCoupleRing::tinysave(const char* field) const

{
    __BEGIN_TRY

    defaultItemObjectRepository().tinysaveGear(GEAR_VAMPIRE_COUPLE_RING, field, m_ItemID);

    __END_CATCH
}

void VampireCoupleRing::save(const string& ownerID, Storage storage, StorageID_t storageID, BYTE x, BYTE y)

{
    __BEGIN_TRY

    defaultItemObjectRepository().updateCoupleRing(GEAR_VAMPIRE_COUPLE_RING, m_ObjectID, m_ItemType, ownerID,
                                                   (int)storage, storageID, (int)x, (int)y, getName(),
                                                   getPartnerItemID(), m_ItemID);

    __END_CATCH
}

string VampireCoupleRing::toString() const

{
    StringStream msg;
    msg << "VampireCoupleRing(" << "ItemID:" << m_ItemID << ",ItemType:" << m_ItemType << ")";
    return msg.toString();
}

VolumeWidth_t VampireCoupleRing::getVolumeWidth() const

{
    __BEGIN_TRY

    return de::gameContext()
        .itemInfos()
        .getItemInfo(Item::ITEM_CLASS_VAMPIRE_COUPLE_RING, m_ItemType)
        ->getVolumeWidth();

    __END_CATCH
}

VolumeHeight_t VampireCoupleRing::getVolumeHeight() const

{
    __BEGIN_TRY

    return de::gameContext()
        .itemInfos()
        .getItemInfo(Item::ITEM_CLASS_VAMPIRE_COUPLE_RING, m_ItemType)
        ->getVolumeHeight();

    __END_CATCH
}

Weight_t VampireCoupleRing::getWeight() const

{
    __BEGIN_TRY

    return de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_VAMPIRE_COUPLE_RING, m_ItemType)->getWeight();

    __END_CATCH
}

bool VampireCoupleRing::hasPartnerItem()

{
    __BEGIN_TRY

    bool bRet = false;

    int count = 0;

    if (defaultItemObjectRepository().loadCoupleRingPartnerCount(GEAR_VAMPIRE_COUPLE_RING, getPartnerItemID(), count)) {
        Assert(count >= 0);
        Assert(count <= 1);

        if (count == 1)
            bRet = true;
    } else {
        bRet = false;
    }

    return bRet;

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
// class VampireCoupleRingInfo member methods
//////////////////////////////////////////////////////////////////////////////

string VampireCoupleRingInfo::toString() const

{
    StringStream msg;
    msg << "VampireCoupleRingInfo(" << "ItemType:" << m_ItemType << ",Name:" << m_Name << ",EName:" << m_EName
        << ",Price:" << m_Price << ",VolumeType:" << Volume2String[m_VolumeType] << ",Weight:" << m_Weight
        << ",Description:" << m_Description << ")";
    return msg.toString();
}

void VampireCoupleRingInfoManager::load()

{
    __BEGIN_TRY

    m_InfoCount = defaultItemObjectRepository().loadMaxGearType(GEAR_VAMPIRE_COUPLE_RING);

    m_pItemInfos = new ItemInfo*[m_InfoCount + 1];

    for (uint i = 0; i <= m_InfoCount; i++)
        m_pItemInfos[i] = NULL;

    vector<BasicInfoRow> rows = defaultItemObjectRepository().loadBasicInfos(GEAR_VAMPIRE_COUPLE_RING);

    for (size_t r = 0; r < rows.size(); r++) {
        VampireCoupleRingInfo* pVampireCoupleRingInfo = new VampireCoupleRingInfo();

        pVampireCoupleRingInfo->setItemType(rows[r].itemType);
        pVampireCoupleRingInfo->setName(rows[r].name);
        pVampireCoupleRingInfo->setEName(rows[r].ename);
        pVampireCoupleRingInfo->setPrice(rows[r].price);
        pVampireCoupleRingInfo->setVolumeType(rows[r].volume);
        pVampireCoupleRingInfo->setWeight(rows[r].weight);
        pVampireCoupleRingInfo->setRatio(rows[r].ratio);

        addItemInfo(pVampireCoupleRingInfo);
    }

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
// class VampireCoupleRingLoader member methods
//////////////////////////////////////////////////////////////////////////////

void VampireCoupleRingLoader::load(Creature* pCreature)

{
    __BEGIN_TRY

    Assert(pCreature != NULL);

    vector<CoupleRingObjectRow> rows =
        defaultItemObjectRepository().loadCoupleRingOfOwner(GEAR_VAMPIRE_COUPLE_RING, pCreature->getName());

    PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pCreature);
    if (rows.empty() && pPC->getFlagSet()->isOn(FLAGSET_IS_COUPLE)) {
        pPC->getFlagSet()->turnOff(FLAGSET_IS_COUPLE);
        pPC->getFlagSet()->save(pPC->getName());

        de::gameContext().couples().removeCoupleForce(pPC);
    }

    for (size_t r = 0; r < rows.size(); r++) {
        try {
            VampireCoupleRing* pVampireCoupleRing = new VampireCoupleRing();

            pVampireCoupleRing->setItemID(rows[r].itemID);
            pVampireCoupleRing->setObjectID(rows[r].objectID);
            pVampireCoupleRing->setItemType(rows[r].itemType);

            Storage storage = (Storage)rows[r].storage;
            BYTE x = rows[r].x;
            BYTE y = rows[r].y;

            string optionField = rows[r].optionField;
            list<OptionType_t> optionTypes;
            setOptionTypeFromField(optionTypes, optionField);
            pVampireCoupleRing->setOptionType(optionTypes);

            pVampireCoupleRing->setName(rows[r].name);
            pVampireCoupleRing->setPartnerItemID(rows[r].partnerItemID);

            // Delete the item when the partner item is gone or the two are no longer a couple.
            PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pCreature);
            if (pPC != NULL && (!de::gameContext().couples().isCouple(pPC, pVampireCoupleRing->getName()) ||
                                !pVampireCoupleRing->hasPartnerItem())) {
                de::gameContext().couples().removeCoupleForce(pPC, pVampireCoupleRing->getName());
                char sql[30];
                snprintf(sql, sizeof(sql), "Storage = 10");
                pVampireCoupleRing->tinysave(sql);
                SAFE_DELETE(pVampireCoupleRing);

                // Clear the FlagSet as well.
                pPC->getFlagSet()->turnOff(FLAGSET_IS_COUPLE);
                pPC->getFlagSet()->save(pPC->getName());
                continue;
            }

            Inventory* pInventory = NULL;
            Slayer* pSlayer = NULL;
            Vampire* pVampire = NULL;

            if (pCreature->isSlayer()) {
                pSlayer = dynamic_cast<Slayer*>(pCreature);
                pInventory = pSlayer->getInventory();
            } else if (pCreature->isVampire()) {
                pVampire = dynamic_cast<Vampire*>(pCreature);
                pInventory = pVampire->getInventory();
            } else
                throw UnsupportedError("Saving Monster/NPC inventories is not supported.");

            switch (storage) {
            case STORAGE_INVENTORY:
                if (pInventory->canAddingEx(x, y, pVampireCoupleRing)) {
                    pInventory->addItemEx(x, y, pVampireCoupleRing);
                } else {
                    processItemBugEx(pCreature, pVampireCoupleRing);
                }
                break;

            case STORAGE_GEAR:
                if (pCreature->isSlayer()) {
                    if (!pSlayer->isWear((Slayer::WearPart)x)) {
                        pSlayer->wearItem((Slayer::WearPart)x, pVampireCoupleRing);
                    } else {
                        processItemBugEx(pCreature, pVampireCoupleRing);
                    }
                } else if (pCreature->isVampire()) {
                    if (!pVampire->isWear((Vampire::WearPart)x)) {
                        pVampire->wearItem((Vampire::WearPart)x, pVampireCoupleRing);
                    } else {
                        processItemBugEx(pCreature, pVampireCoupleRing);
                    }
                }
                break;

            case STORAGE_BELT:
                processItemBugEx(pCreature, pVampireCoupleRing);
                break;

            case STORAGE_EXTRASLOT:
                if (pCreature->isSlayer())
                    pSlayer->addItemToExtraInventorySlot(pVampireCoupleRing);
                else if (pCreature->isVampire())
                    pVampire->addItemToExtraInventorySlot(pVampireCoupleRing);
                break;

            case STORAGE_MOTORCYCLE:
                processItemBugEx(pCreature, pVampireCoupleRing);
                break;

            case STORAGE_STASH:
                processItemBugEx(pCreature, pVampireCoupleRing);
                break;

            case STORAGE_GARBAGE:
                processItemBug(pCreature, pVampireCoupleRing);
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

void VampireCoupleRingLoader::load(Zone* pZone)

{
    __BEGIN_TRY

    cout << "Loading items dropped in a zone is not supported." << endl;
    Assert(false);

    Assert(pZone != NULL);

    vector<PlainZoneObjectRow> rows = defaultItemObjectRepository().loadPlainItemInZone(
        GEAR_VAMPIRE_COUPLE_RING, (int)STORAGE_ZONE, pZone->getZoneID());

    for (size_t r = 0; r < rows.size(); r++) {
        VampireCoupleRing* pVampireCoupleRing = new VampireCoupleRing();

        pVampireCoupleRing->setItemID(rows[r].itemID);
        pVampireCoupleRing->setObjectID(rows[r].objectID);
        pVampireCoupleRing->setItemType(rows[r].itemType);

        Storage storage = (Storage)rows[r].storage;
        BYTE x = rows[r].x;
        BYTE y = rows[r].y;

        switch (storage) {
        case STORAGE_ZONE: {
            Tile& pTile = pZone->getTile(x, y);
            Assert(!pTile.hasItem());
            pTile.addItem(pVampireCoupleRing);
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

void VampireCoupleRingLoader::load(StorageID_t storageID, Inventory* pInventory)

{
    __BEGIN_TRY

    Statement* pStmt;

    BEGIN_DB {}
    END_DB(pStmt)

    __END_CATCH
}
