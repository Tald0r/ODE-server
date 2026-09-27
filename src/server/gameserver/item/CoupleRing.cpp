//////////////////////////////////////////////////////////////////////////////
// Filename    : CoupleRing.cpp
// Written By  : excel96
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "CoupleRing.h"

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

ItemID_t CoupleRing::m_ItemIDRegistry = 0;
Mutex CoupleRing::m_Mutex;

//////////////////////////////////////////////////////////////////////////////
// class CoupleRing member methods
//////////////////////////////////////////////////////////////////////////////

CoupleRing::CoupleRing()

{
    m_ItemType = 0;
}

CoupleRing::CoupleRing(ItemType_t itemType, const list<OptionType_t>& optionType)

    : m_ItemType(itemType), m_OptionType(optionType) {
    __BEGIN_TRY

    if (!de::gameContext().itemInfos().isPossibleItem(getItemClass(), m_ItemType, optionType)) {
        filelog("itembug.log", "CoupleRing::CoupleRing() : Invalid item type or option type");
        throw Error("CoupleRing::CoupleRing() : Invalid item type or optionType");
    }

    __END_CATCH
}

void CoupleRing::create(const string& ownerID, Storage storage, StorageID_t storageID, BYTE x, BYTE y, ItemID_t itemID)

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

    defaultItemObjectRepository().insertCoupleRing(GEAR_COUPLE_RING, m_ItemID, m_ObjectID, m_ItemType, ownerID,
                                                   (int)storage, storageID, (int)x, (int)y, optionField, getName(),
                                                   getPartnerItemID());

    __END_CATCH
}

//--------------------------------------------------------------------------------
// save item
//--------------------------------------------------------------------------------
void CoupleRing::tinysave(const char* field) const

{
    __BEGIN_TRY

    defaultItemObjectRepository().tinysaveGear(GEAR_COUPLE_RING, field, m_ItemID);

    __END_CATCH
}

void CoupleRing::save(const string& ownerID, Storage storage, StorageID_t storageID, BYTE x, BYTE y)

{
    __BEGIN_TRY

    defaultItemObjectRepository().updateCoupleRing(GEAR_COUPLE_RING, m_ObjectID, m_ItemType, ownerID, (int)storage,
                                                   storageID, (int)x, (int)y, getName(), getPartnerItemID(), m_ItemID);

    __END_CATCH
}

string CoupleRing::toString() const

{
    StringStream msg;
    msg << "CoupleRing(" << "ItemID:" << m_ItemID << ",ItemType:" << m_ItemType << ")";
    return msg.toString();
}

VolumeWidth_t CoupleRing::getVolumeWidth() const

{
    __BEGIN_TRY

    return de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_COUPLE_RING, m_ItemType)->getVolumeWidth();

    __END_CATCH
}

VolumeHeight_t CoupleRing::getVolumeHeight() const

{
    __BEGIN_TRY

    return de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_COUPLE_RING, m_ItemType)->getVolumeHeight();

    __END_CATCH
}

Weight_t CoupleRing::getWeight() const

{
    __BEGIN_TRY

    return de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_COUPLE_RING, m_ItemType)->getWeight();

    __END_CATCH
}

bool CoupleRing::hasPartnerItem()

{
    __BEGIN_TRY

    bool bRet = false;

    int count = 0;

    if (defaultItemObjectRepository().loadCoupleRingPartnerCount(GEAR_COUPLE_RING, getPartnerItemID(), count)) {
        // Dangerous!
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
// class CoupleRingInfo member methods
//////////////////////////////////////////////////////////////////////////////

string CoupleRingInfo::toString() const

{
    StringStream msg;
    msg << "CoupleRingInfo(" << "ItemType:" << m_ItemType << ",Name:" << getName().c_str() << ",EName:" << m_EName
        << ",Price:" << m_Price << ",VolumeType:" << Volume2String[m_VolumeType] << ",Weight:" << m_Weight
        << ",Description:" << m_Description << ")";
    return msg.toString();
}

void CoupleRingInfoManager::load()

{
    __BEGIN_TRY

    m_InfoCount = defaultItemObjectRepository().loadMaxGearType(GEAR_COUPLE_RING);

    m_pItemInfos = new ItemInfo*[m_InfoCount + 1];

    for (uint i = 0; i <= m_InfoCount; i++)
        m_pItemInfos[i] = NULL;

    vector<BasicInfoRow> rows = defaultItemObjectRepository().loadBasicInfos(GEAR_COUPLE_RING);

    for (size_t r = 0; r < rows.size(); r++) {
        CoupleRingInfo* pCoupleRingInfo = new CoupleRingInfo();

        pCoupleRingInfo->setItemType(rows[r].itemType);
        pCoupleRingInfo->setName(rows[r].name);
        pCoupleRingInfo->setEName(rows[r].ename);
        pCoupleRingInfo->setPrice(rows[r].price);
        pCoupleRingInfo->setVolumeType(rows[r].volume);
        pCoupleRingInfo->setWeight(rows[r].weight);
        pCoupleRingInfo->setRatio(rows[r].ratio);

        addItemInfo(pCoupleRingInfo);
    }

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
// class CoupleRingLoader member methods
//////////////////////////////////////////////////////////////////////////////

void CoupleRingLoader::load(Creature* pCreature)

{
    __BEGIN_TRY

    Assert(pCreature != NULL);

    vector<CoupleRingObjectRow> rows =
        defaultItemObjectRepository().loadCoupleRingOfOwner(GEAR_COUPLE_RING, pCreature->getName());

    PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pCreature);
    if (rows.empty() && pPC->getFlagSet()->isOn(FLAGSET_IS_COUPLE)) {
        pPC->getFlagSet()->turnOff(FLAGSET_IS_COUPLE);
        pPC->getFlagSet()->save(pPC->getName());

        de::gameContext().couples().removeCoupleForce(pPC);
    }

    for (size_t r = 0; r < rows.size(); r++) {
        try {
            CoupleRing* pCoupleRing = new CoupleRing();

            pCoupleRing->setItemID(rows[r].itemID);
            pCoupleRing->setObjectID(rows[r].objectID);
            pCoupleRing->setItemType(rows[r].itemType);

            Storage storage = (Storage)rows[r].storage;
            BYTE x = rows[r].x;
            BYTE y = rows[r].y;

            string optionField = rows[r].optionField;
            list<OptionType_t> optionTypes;
            setOptionTypeFromField(optionTypes, optionField);
            pCoupleRing->setOptionType(optionTypes);

            pCoupleRing->setName(rows[r].name);
            pCoupleRing->setPartnerItemID(rows[r].partnerItemID);

            // Delete the item when the partner item is gone or the two are no longer a couple.
            if (pPC != NULL && (!de::gameContext().couples().isCouple(pPC, pCoupleRing->getName()) ||
                                !pCoupleRing->hasPartnerItem())) {
                de::gameContext().couples().removeCoupleForce(pPC, pCoupleRing->getName());
                char sql[30];
                snprintf(sql, sizeof(sql), "Storage = 10");
                pCoupleRing->tinysave(sql);
                SAFE_DELETE(pCoupleRing);

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
                if (pInventory->canAddingEx(x, y, pCoupleRing)) {
                    pInventory->addItemEx(x, y, pCoupleRing);
                } else {
                    processItemBugEx(pCreature, pCoupleRing);
                }
                break;

            case STORAGE_GEAR:
                if (pCreature->isSlayer()) {
                    if (!pSlayer->isWear((Slayer::WearPart)x)) {
                        pSlayer->wearItem((Slayer::WearPart)x, pCoupleRing);
                    } else {
                        processItemBugEx(pCreature, pCoupleRing);
                    }
                } else if (pCreature->isVampire()) {
                    if (!pVampire->isWear((Vampire::WearPart)x)) {
                        pVampire->wearItem((Vampire::WearPart)x, pCoupleRing);
                    } else {
                        processItemBugEx(pCreature, pCoupleRing);
                    }
                }
                break;

            case STORAGE_BELT:
                processItemBugEx(pCreature, pCoupleRing);
                break;

            case STORAGE_EXTRASLOT:
                if (pCreature->isSlayer())
                    pSlayer->addItemToExtraInventorySlot(pCoupleRing);
                else if (pCreature->isVampire())
                    pVampire->addItemToExtraInventorySlot(pCoupleRing);
                break;

            case STORAGE_MOTORCYCLE:
                processItemBugEx(pCreature, pCoupleRing);
                break;

            case STORAGE_STASH:
                processItemBugEx(pCreature, pCoupleRing);
                break;

            case STORAGE_GARBAGE:
                processItemBug(pCreature, pCoupleRing);
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

void CoupleRingLoader::load(Zone* pZone)

{
    __BEGIN_TRY

    cout << "Loading items dropped in a zone is not supported." << endl;
    Assert(false);

    Assert(pZone != NULL);

    vector<PlainZoneObjectRow> rows =
        defaultItemObjectRepository().loadPlainItemInZone(GEAR_COUPLE_RING, (int)STORAGE_ZONE, pZone->getZoneID());

    for (size_t r = 0; r < rows.size(); r++) {
        CoupleRing* pCoupleRing = new CoupleRing();

        pCoupleRing->setItemID(rows[r].itemID);
        pCoupleRing->setObjectID(rows[r].objectID);
        pCoupleRing->setItemType(rows[r].itemType);

        Storage storage = (Storage)rows[r].storage;
        BYTE x = rows[r].x;
        BYTE y = rows[r].y;

        switch (storage) {
        case STORAGE_ZONE: {
            Tile& pTile = pZone->getTile(x, y);
            Assert(!pTile.hasItem());
            pTile.addItem(pCoupleRing);
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

void CoupleRingLoader::load(StorageID_t storageID, Inventory* pInventory)

{
    __BEGIN_TRY

    Statement* pStmt;

    BEGIN_DB {}
    END_DB(pStmt)

    __END_CATCH
}
