//////////////////////////////////////////////////////////////////////////////
// Filename    : PetItem.cpp
// Written By  : excel96
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "PetItem.h"

#include "Belt.h"
#include "CreatureUtil.h"
#include "DB.h"
#include "EffectHasPet.h"
#include "GameContext.h"
#include "GamePlayer.h"
#include "ItemInfoManager.h"
#include "ItemUtil.h"
#include "Motorcycle.h"
#include "Ousters.h"
#include "PetTypeInfo.h"
#include "PetUtil.h"
#include "Slayer.h"
#include "Stash.h"
#include "Utility.h"
#include "Vampire.h"
#include "repository/ItemObjectRepository.h"

string getDBString(const string& str);

ItemID_t PetItem::m_ItemIDRegistry = 0;
Mutex PetItem::m_Mutex;

//////////////////////////////////////////////////////////////////////////////
// class PetItem member methods
//////////////////////////////////////////////////////////////////////////////

PetItem::PetItem()

{
    m_ItemType = 0;
    m_pPetInfo = NULL;
}

PetItem::PetItem(ItemType_t itemType, const list<OptionType_t>& optionType)

{
    m_ItemType = itemType;
    m_pPetInfo = NULL;

    if (!de::gameContext().itemInfos().isPossibleItem(getItemClass(), m_ItemType, optionType)) {
        filelog("itembug.log", "PetItem::PetItem() : Invalid item type or option type");
        throw Error("PetItem::PetItem() : Invalid item type or optionType");
    }
}

void PetItem::create(const string& ownerID, Storage storage, StorageID_t storageID, BYTE x, BYTE y, ItemID_t itemID)

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

    if (m_pPetInfo == NULL) {
        defaultItemObjectRepository().insertPetItem(GEAR_PET_ITEM, m_ItemID, m_ObjectID, m_ItemType, ownerID, storage,
                                                    storageID, x, y, m_CreateType);
    } else {
        defaultItemObjectRepository().insertPetItemWithInfo(
            GEAR_PET_ITEM, m_ItemID, m_ObjectID, m_ItemType, ownerID, storage, storageID, x, y, m_CreateType,
            m_pPetInfo->getPetCreatureType(), m_pPetInfo->getPetLevel(), m_pPetInfo->getPetExp(),
            m_pPetInfo->getPetHP(), m_pPetInfo->getPetAttr(), m_pPetInfo->getPetAttrLevel(), m_pPetInfo->getPetOption(),
            m_pPetInfo->getFoodType(), m_pPetInfo->canGamble(), m_pPetInfo->canCutHead(), m_pPetInfo->canAttack(),
            m_pPetInfo->getLastFeedTime().toDateTime());
    }

    __END_CATCH
}

//--------------------------------------------------------------------------------
// save item
//--------------------------------------------------------------------------------
void PetItem::tinysave(const char* field) const

{
    __BEGIN_TRY

    defaultItemObjectRepository().tinysaveGear(GEAR_PET_ITEM, field, m_ItemID);

    __END_CATCH
}

void PetItem::save(const string& ownerID, Storage storage, StorageID_t storageID, BYTE x, BYTE y)

{
    __BEGIN_TRY

    if (m_pPetInfo == NULL) {
        defaultItemObjectRepository().updatePetItem(GEAR_PET_ITEM, m_ObjectID, m_ItemType, ownerID, (int)storage,
                                                    storageID, (int)x, (int)y, m_ItemID);
    } else {
        defaultItemObjectRepository().updatePetItemWithInfo(
            GEAR_PET_ITEM, m_ObjectID, m_ItemType, ownerID, (int)storage, storageID, (int)x, (int)y,
            m_pPetInfo->getPetCreatureType(), m_pPetInfo->getPetLevel(), m_pPetInfo->getPetAttr(),
            m_pPetInfo->getPetAttrLevel(), m_pPetInfo->getPetExp(), m_pPetInfo->getPetHP(), m_pPetInfo->getFoodType(),
            m_pPetInfo->canGamble(), m_pPetInfo->canCutHead(), m_pPetInfo->canAttack(),
            m_pPetInfo->getLastFeedTime().toDateTime(), getDBString(m_pPetInfo->getNickname()), m_ItemID);
    }

    __END_CATCH
}

void PetItem::savePetInfo() const {
    __BEGIN_TRY

    if (m_pPetInfo != NULL) {
        defaultItemObjectRepository().savePetItemInfo(
            GEAR_PET_ITEM, m_pPetInfo->getPetCreatureType(), m_pPetInfo->getPetLevel(), m_pPetInfo->getPetAttr(),
            m_pPetInfo->getPetAttrLevel(), m_pPetInfo->getPetExp(), m_pPetInfo->getPetHP(), m_pPetInfo->getFoodType(),
            m_pPetInfo->canGamble(), m_pPetInfo->canCutHead(), m_pPetInfo->canAttack(),
            m_pPetInfo->getLastFeedTime().toDateTime(), getDBString(m_pPetInfo->getNickname()), m_ItemID);
    }

    __END_CATCH
}

void PetItem::makePCItemInfo(PCItemInfo& result) const {
    Item::makePCItemInfo(result);

    if (m_pPetInfo != NULL) {
        list<OptionType_t> olist;

        if (m_pPetInfo->getPetOption() != 0)
            olist.push_back(m_pPetInfo->getPetOption());

        result.setOptionType(olist);
        result.setDurability(m_pPetInfo->getPetHP());
        result.setEnchantLevel(m_pPetInfo->getPetAttr());
        result.setSilver(m_pPetInfo->getPetAttrLevel());
        result.setGrade((m_pPetInfo->getPetHP() == 0)
                            ? (m_pPetInfo->getLastFeedTime().daysTo(VSDateTime::currentDateTime()))
                            : (-1));
        result.setItemNum(m_pPetInfo->getPetLevel());
        result.setMainColor(0xffff);
    }
}

void PetItem::whenPCTake(PlayerCreature* pPC) {
    Item::whenPCTake(pPC);
    pPC->getPetItems().push_back(this);

    if (!pPC->isFlag(Effect::EFFECT_CLASS_HAS_PET)) {
        EffectHasPet* pEffect = new EffectHasPet(pPC);
        pEffect->setNextTime(600);
        pPC->setFlag(Effect::EFFECT_CLASS_HAS_PET);
        pPC->addEffect(pEffect);
    }
}

void PetItem::whenPCLost(PlayerCreature* pPC) {
    Item::whenPCLost(pPC);

    if (m_pPetInfo == pPC->getPetInfo()) {
        pPC->setPetInfo(NULL);
        GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(pPC->getPlayer());
        if (pGamePlayer != NULL)
            sendPetInfo(pGamePlayer, true);
    }

    pPC->getPetItems().remove(this);
    if (pPC->getPetItems().empty()) {
        Effect* pEffect = pPC->findEffect(Effect::EFFECT_CLASS_HAS_PET);
        if (pEffect != NULL)
            pEffect->setDeadline(0);
    }
}

string PetItem::toString() const

{
    StringStream msg;

    msg << "PetItem(" << "ItemID:" << m_ItemID << ",ItemType:" << (int)m_ItemType;

    if (m_pPetInfo != NULL) {
        msg << ",PetType:" << (int)m_pPetInfo->getPetType() << ",PetLevel:" << (int)m_pPetInfo->getPetLevel()
            << ",PetExp:" << (int)m_pPetInfo->getPetExp() << ",PetHP:" << (int)m_pPetInfo->getPetHP()
            << ",PetAttr:" << (int)m_pPetInfo->getPetAttr() << ",PetAttrLevel:" << (int)m_pPetInfo->getPetAttrLevel()
            << ",PetOption:" << (int)m_pPetInfo->getPetOption() << ",CanGamble:" << (int)m_pPetInfo->canGamble()
            << ",CanAttack:" << (int)m_pPetInfo->canAttack()
            << ",FeedTime:" << m_pPetInfo->getLastFeedTime().toString();
    }

    msg << ")";

    return msg.toString();
}

VolumeWidth_t PetItem::getVolumeWidth() const

{
    __BEGIN_TRY

    return de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_PET_ITEM, m_ItemType)->getVolumeWidth();

    __END_CATCH
}

VolumeHeight_t PetItem::getVolumeHeight() const

{
    __BEGIN_TRY

    return de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_PET_ITEM, m_ItemType)->getVolumeHeight();

    __END_CATCH
}

Weight_t PetItem::getWeight() const

{
    __BEGIN_TRY

    return de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_PET_ITEM, m_ItemType)->getWeight();

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////////////
// class PetItemInfo member methods
//////////////////////////////////////////////////////////////////////////////

string PetItemInfo::toString() const

{
    StringStream msg;
    msg << "PetItemInfo(" << "ItemType:" << (int)m_ItemType << ",Name:" << m_Name << ",EName:" << m_EName
        << ",Price:" << (int)m_Price << ",VolumeType:" << Volume2String[m_VolumeType] << ",Weight:" << (int)m_Weight
        << ",Description:" << m_Description << ")";
    return msg.toString();
}

void PetItemInfoManager::load()

{
    __BEGIN_TRY

    m_InfoCount = defaultItemObjectRepository().loadMaxGearType(GEAR_PET_ITEM);

    m_pItemInfos = new ItemInfo*[m_InfoCount + 1];

    for (uint i = 0; i <= m_InfoCount; i++)
        m_pItemInfos[i] = NULL;

    vector<BasicInfoRow> rows = defaultItemObjectRepository().loadBasicInfos(GEAR_PET_ITEM);

    for (size_t r = 0; r < rows.size(); r++) {
        PetItemInfo* pPetItemInfo = new PetItemInfo();

        pPetItemInfo->setItemType(rows[r].itemType);
        pPetItemInfo->setName(rows[r].name);
        pPetItemInfo->setEName(rows[r].ename);
        pPetItemInfo->setPrice(rows[r].price);
        pPetItemInfo->setVolumeType(rows[r].volume);
        pPetItemInfo->setWeight(rows[r].weight);
        pPetItemInfo->setRatio(rows[r].ratio);

        addItemInfo(pPetItemInfo);
    }

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
// class PetItemLoader member methods
//////////////////////////////////////////////////////////////////////////////

void PetItemLoader::load(Creature* pCreature)

{
    __BEGIN_TRY

    Assert(pCreature != NULL);

    vector<PetItemObjectRow> rows =
        defaultItemObjectRepository().loadPetItemOfOwner(GEAR_PET_ITEM, pCreature->getName());

    for (size_t r = 0; r < rows.size(); r++) {
        try {
            PetItem* pPetItem = new PetItem();

            pPetItem->setItemID(rows[r].itemID);
            pPetItem->setObjectID(rows[r].objectID);
            pPetItem->setItemType(rows[r].itemType);

            Storage storage = (Storage)rows[r].storage;
            StorageID_t storageID = rows[r].storageID;
            BYTE x = rows[r].x;
            BYTE y = rows[r].y;

            pPetItem->setCreateType((Item::CreateType)rows[r].createType);

            PetInfo* pPetInfo = new PetInfo;
            pPetInfo->setPetType(pPetItem->getItemType());
            pPetInfo->setPetCreatureType(rows[r].petCreatureType);
            pPetInfo->setPetLevel(rows[r].petLevel);
            pPetInfo->setPetExp(rows[r].petExp);
            pPetInfo->setPetHP(rows[r].petHP);
            pPetInfo->setPetAttr(rows[r].petAttr);
            pPetInfo->setPetAttrLevel(rows[r].petAttrLevel);
            pPetInfo->setPetOption(rows[r].petOption);
            pPetInfo->setFoodType(rows[r].foodType);
            pPetInfo->setGamble(rows[r].canGamble);
            pPetInfo->setCutHead(rows[r].canCutHead);
            pPetInfo->setAttack(rows[r].canAttack);
            pPetInfo->setFeedTime(VSDateTime(rows[r].lastFeedTime));
            pPetInfo->setNickname(rows[r].nickname);

            // Two-way link.
            pPetItem->setPetInfo(pPetInfo);
            pPetInfo->setPetItem(pPetItem);

            uint ratio = 100;

            if (storage == STORAGE_PET_STASH) {
                ratio /= 2;
                pPetInfo->setFeedTurn(2);
            } else {
                pPetInfo->setFeedTurn(1);
            }

            if (pPetInfo->getPetLevel() == 50)
                ratio /= 10;
            refreshHP(pPetInfo, ratio);

            PetTypeInfo* pPetTypeInfo = PetTypeInfoManager::getInstance()->getPetTypeInfo(pPetInfo->getPetType());
            if (pPetTypeInfo != NULL) {
                pPetInfo->setPetCreatureType(pPetTypeInfo->getPetCreatureType(pPetInfo->getPetLevel()));
            }

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

            PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pCreature);
            Assert(pPC != NULL);

            switch (storage) {
            case STORAGE_INVENTORY:
                if (pInventory->canAddingEx(x, y, pPetItem)) {
                    pInventory->addItemEx(x, y, pPetItem);
                    pPetItem->whenPCTake(pPC);
                } else {
                    processItemBugEx(pCreature, pPetItem);
                }
                break;

            case STORAGE_GEAR:
                processItemBugEx(pCreature, pPetItem);
                break;

            case STORAGE_BELT:
                processItemBugEx(pCreature, pPetItem);
                break;

            case STORAGE_EXTRASLOT:
                if (pCreature->isSlayer())
                    pSlayer->addItemToExtraInventorySlot(pPetItem);
                else if (pCreature->isVampire())
                    pVampire->addItemToExtraInventorySlot(pPetItem);
                else if (pCreature->isOusters())
                    pOusters->addItemToExtraInventorySlot(pPetItem);

                pPetItem->whenPCTake(pPC);
                break;

            case STORAGE_MOTORCYCLE:
                processItemBugEx(pCreature, pPetItem);
                break;

            case STORAGE_STASH:
                if (pStash->isExist(x, y)) {
                    processItemBugEx(pCreature, pPetItem);
                } else {
                    pStash->insert(x, y, pPetItem);
                    pPetItem->whenPCTake(pPC);
                }
                break;

            case STORAGE_GARBAGE:
                processItemBug(pCreature, pPetItem);
                break;


            case STORAGE_PET_STASH:
                /* Should the pet be loaded and put into pCreature?...*/
                if (pPC->getPetStashItem(storageID) == NULL) {
                    pPC->addPetStashItem(storageID, pPetItem);
                    pPetItem->whenPCTake(pPC);
                } else
                    processItemBug(pCreature, pPetItem);
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

void PetItemLoader::load(Zone* pZone)

{
    __BEGIN_TRY

    Assert(pZone != NULL);

    vector<FlagZoneObjectRow> rows =
        defaultItemObjectRepository().loadFlagItemInZone(GEAR_PET_ITEM, (int)STORAGE_ZONE, pZone->getZoneID());

    for (size_t r = 0; r < rows.size(); r++) {
        PetItem* pPetItem = new PetItem();

        pPetItem->setItemID(rows[r].itemID);
        pPetItem->setObjectID(rows[r].objectID);
        pPetItem->setItemType(rows[r].itemType);

        Storage storage = (Storage)rows[r].storage;
        BYTE x = rows[r].x;
        BYTE y = rows[r].y;

        pPetItem->setCreateType((Item::CreateType)rows[r].createType);

        switch (storage) {
        case STORAGE_ZONE: {
            Tile& pTile = pZone->getTile(x, y);
            Assert(!pTile.hasItem());
            pTile.addItem(pPetItem);
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

void PetItemLoader::load(StorageID_t storageID, Inventory* pInventory)

{
    __BEGIN_TRY

    Statement* pStmt;

    BEGIN_DB {}
    END_DB(pStmt)

    __END_CATCH
}
