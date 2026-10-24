//////////////////////////////////////////////////////////////////////////////
// Filename    : GCStashListFill.cpp
// Description : gameserver-side half of GCStashList -- setStashItem()
//               converts a live Item into the packet's wire fields, so its
//               definition lives with the game objects, out of the wire
//               library (see src/Core/GCStashList.cpp).
//////////////////////////////////////////////////////////////////////////////

#include "AR.h"
#include "Assert1.h"
#include "Belt.h"
#include "GCStashList.h"
#include "GameContext.h"
#include "Inventory.h"
#include "Item.h"
#include "ItemInfoManager.h"
#include "OustersArmsband.h"
#include "PetItem.h"
#include "SG.h"
#include "SMG.h"
#include "SR.h"

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void GCStashList::setStashItem(BYTE rack, BYTE index, Item* pItem)

{
    Assert(rack < STASH_RACK_MAX && index < STASH_INDEX_MAX);
    Assert(pItem != NULL);

    ItemInfo* pItemInfo = NULL;
    AR* pAR = NULL;
    SR* pSR = NULL;
    SG* pSG = NULL;
    SMG* pSMG = NULL;
    Belt* pBelt = NULL;
    OustersArmsband* pOustersArmsband = NULL;
    PetItem* pPetItem = NULL;
    BYTE pocketCount = 0;
    Inventory* pBeltInventory = NULL;
    Inventory* pOustersArmsbandInventory = NULL;
    BYTE i = 0;
    PetInfo* pPetInfo = 0;

    switch (pItem->getItemClass()) {
    case Item::ITEM_CLASS_AR:
        pAR = dynamic_cast<AR*>(pItem);
        m_pItems[rack][index].objectID = pItem->getObjectID();
        m_pItems[rack][index].itemClass = pItem->getItemClass();
        m_pItems[rack][index].itemType = pItem->getItemType();
        m_pItems[rack][index].optionType = pItem->getOptionTypeList();
        m_pItems[rack][index].durability = pItem->getDurability();
        m_pItems[rack][index].num = pAR->getBulletCount();
        m_pItems[rack][index].silver = pItem->getSilver();
        m_pItems[rack][index].grade = pItem->getGrade();
        m_pItems[rack][index].enchantLevel = pItem->getEnchantLevel();
        break;
    case Item::ITEM_CLASS_SR:
        pSR = dynamic_cast<SR*>(pItem);
        m_pItems[rack][index].objectID = pItem->getObjectID();
        m_pItems[rack][index].itemClass = pItem->getItemClass();
        m_pItems[rack][index].itemType = pItem->getItemType();
        m_pItems[rack][index].optionType = pItem->getOptionTypeList();
        m_pItems[rack][index].durability = pItem->getDurability();
        m_pItems[rack][index].num = pSR->getBulletCount();
        m_pItems[rack][index].silver = pItem->getSilver();
        m_pItems[rack][index].grade = pItem->getGrade();
        m_pItems[rack][index].enchantLevel = pItem->getEnchantLevel();
        break;
    case Item::ITEM_CLASS_SG:
        pSG = dynamic_cast<SG*>(pItem);
        m_pItems[rack][index].objectID = pItem->getObjectID();
        m_pItems[rack][index].itemClass = pItem->getItemClass();
        m_pItems[rack][index].itemType = pItem->getItemType();
        m_pItems[rack][index].optionType = pItem->getOptionTypeList();
        m_pItems[rack][index].durability = pItem->getDurability();
        m_pItems[rack][index].num = pSG->getBulletCount();
        m_pItems[rack][index].grade = pItem->getGrade();
        m_pItems[rack][index].enchantLevel = pItem->getEnchantLevel();
        break;
    case Item::ITEM_CLASS_SMG:
        pSMG = dynamic_cast<SMG*>(pItem);
        m_pItems[rack][index].objectID = pItem->getObjectID();
        m_pItems[rack][index].itemClass = pItem->getItemClass();
        m_pItems[rack][index].itemType = pItem->getItemType();
        m_pItems[rack][index].optionType = pItem->getOptionTypeList();
        m_pItems[rack][index].durability = pItem->getDurability();
        m_pItems[rack][index].num = pSMG->getBulletCount();
        m_pItems[rack][index].grade = pItem->getGrade();
        m_pItems[rack][index].enchantLevel = pItem->getEnchantLevel();
        break;
    case Item::ITEM_CLASS_BELT:
        m_pItems[rack][index].objectID = pItem->getObjectID();
        m_pItems[rack][index].itemClass = pItem->getItemClass();
        m_pItems[rack][index].itemType = pItem->getItemType();
        m_pItems[rack][index].optionType = pItem->getOptionTypeList();
        m_pItems[rack][index].durability = pItem->getDurability();
        m_pItems[rack][index].num = pItem->getNum();
        m_pItems[rack][index].grade = pItem->getGrade();
        m_pItems[rack][index].enchantLevel = pItem->getEnchantLevel();

        // For a belt, the items inside it have to be set too.
        // Which is a nuisance.
        pBelt = dynamic_cast<Belt*>(pItem);
        pItemInfo = de::gameContext().itemInfos().getItemInfo(pBelt->getItemClass(), pBelt->getItemType());
        pocketCount = dynamic_cast<BeltInfo*>(pItemInfo)->getPocketCount();
        pBeltInventory = pBelt->getInventory();

        for (i = 0; i < pocketCount; i++) {
            Item* pBeltItem = pBeltInventory->getItem((int)i, 0);
            // If there is an item in the slot...
            if (pBeltItem != NULL) {
                SubItemInfo* pSubItemInfo = new SubItemInfo;
                Assert(pSubItemInfo != NULL);

                pSubItemInfo->setObjectID(pBeltItem->getObjectID());
                pSubItemInfo->setItemClass(pBeltItem->getItemClass());
                pSubItemInfo->setItemType(pBeltItem->getItemType());
                pSubItemInfo->setItemNum(pBeltItem->getNum());
                pSubItemInfo->setSlotID(i);

                // Add the information that was built to the matching list.
                m_pSubItems[rack][index].push_back(pSubItemInfo);
            }
        }

        break;
    case Item::ITEM_CLASS_OUSTERS_ARMSBAND:
        m_pItems[rack][index].objectID = pItem->getObjectID();
        m_pItems[rack][index].itemClass = pItem->getItemClass();
        m_pItems[rack][index].itemType = pItem->getItemType();
        m_pItems[rack][index].optionType = pItem->getOptionTypeList();
        m_pItems[rack][index].durability = pItem->getDurability();
        m_pItems[rack][index].num = pItem->getNum();
        m_pItems[rack][index].silver = pItem->getSilver();
        m_pItems[rack][index].grade = pItem->getGrade();
        m_pItems[rack][index].enchantLevel = pItem->getEnchantLevel();

        // For a belt, the items inside it have to be set too.
        // Which is a nuisance.
        pOustersArmsband = dynamic_cast<OustersArmsband*>(pItem);
        pItemInfo = de::gameContext().itemInfos().getItemInfo(pOustersArmsband->getItemClass(),
                                                              pOustersArmsband->getItemType());
        pocketCount = dynamic_cast<OustersArmsbandInfo*>(pItemInfo)->getPocketCount();
        pOustersArmsbandInventory = pOustersArmsband->getInventory();

        for (i = 0; i < pocketCount; i++) {
            Item* pOustersArmsbandItem = pOustersArmsbandInventory->getItem((int)i, 0);
            // If there is an item in the slot...
            if (pOustersArmsbandItem != NULL) {
                SubItemInfo* pSubItemInfo = new SubItemInfo;
                Assert(pSubItemInfo != NULL);

                pSubItemInfo->setObjectID(pOustersArmsbandItem->getObjectID());
                pSubItemInfo->setItemClass(pOustersArmsbandItem->getItemClass());
                pSubItemInfo->setItemType(pOustersArmsbandItem->getItemType());
                pSubItemInfo->setItemNum(pOustersArmsbandItem->getNum());
                pSubItemInfo->setSlotID(i);

                // Add the information that was built to the matching list.
                m_pSubItems[rack][index].push_back(pSubItemInfo);
            }
        }

        break;

    case Item::ITEM_CLASS_PET_ITEM:
        m_pItems[rack][index].objectID = pItem->getObjectID();
        m_pItems[rack][index].itemClass = pItem->getItemClass();
        m_pItems[rack][index].itemType = pItem->getItemType();

        pPetItem = dynamic_cast<PetItem*>(pItem);
        pPetInfo = pPetItem->getPetInfo();

        if (pPetInfo != NULL) {
            list<OptionType_t> olist;
            if (pPetInfo->getPetOption() != 0)
                m_pItems[rack][index].optionType.push_back(pPetInfo->getPetOption());

            m_pItems[rack][index].durability = pPetInfo->getPetHP();
            m_pItems[rack][index].enchantLevel = pPetInfo->getPetAttr();
            m_pItems[rack][index].silver = pPetInfo->getPetAttrLevel();
            m_pItems[rack][index].grade = (pPetInfo->getPetHP() == 0)
                                              ? (pPetInfo->getLastFeedTime().daysTo(VSDateTime::currentDateTime()))
                                              : (-1);
            m_pItems[rack][index].num = pPetInfo->getPetLevel();
        }

        break;
    default:
        m_pItems[rack][index].objectID = pItem->getObjectID();
        m_pItems[rack][index].itemClass = pItem->getItemClass();
        m_pItems[rack][index].itemType = pItem->getItemType();
        m_pItems[rack][index].optionType = pItem->getOptionTypeList();
        m_pItems[rack][index].durability = pItem->getDurability();
        m_pItems[rack][index].num = pItem->getNum();
        m_pItems[rack][index].silver = pItem->getSilver();
        m_pItems[rack][index].grade = pItem->getGrade();
        m_pItems[rack][index].enchantLevel = pItem->getEnchantLevel();
        break;
    }

    m_bExist[rack][index] = true;
}
