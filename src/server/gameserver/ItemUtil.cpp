//////////////////////////////////////////////////////////////////////////////
// Filename    : ItemUtil.cpp
// Written by  : excel96
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "ItemUtil.h"

#include <stdio.h>

#include <fstream>
#include <vector>

#include <unordered_set>

#include "ConcreteItem.h"
#include "Corpse.h"
#include "GCCreateItem.h"
#include "GameContext.h"
#include "GoodsInfoManager.h"
#include "Inventory.h"
#include "ItemFactoryManager.h"
#include "ItemGradeManager.h"
#include "ItemInfo.h"
#include "ItemInfoManager.h"
#include "MonsterCorpse.h"
#include "OptionInfo.h"
#include "Ousters.h"
#include "PetExpInfo.h"
#include "PetInfo.h"
#include "PetTypeInfo.h"
#include "Player.h"
#include "PlayerCreature.h"
#include "RelicUtil.h"
#include "Slayer.h"
#include "Treasure.h"
#include "UniqueItemManager.h"
#include "Utility.h"
#include "VSDateTime.h"
#include "Vampire.h"
#include "VariableManager.h"
#include "ctf/FlagManager.h"
#include "domain/EquipRequirement.h"
#include "domain/ItemDurability.h"
#include "item/AR.h"
#include "item/Magazine.h"
#include "item/OustersSummonItem.h"
#include "item/PetItem.h"
#include "item/Relic.h"
#include "item/SG.h"
#include "item/SMG.h"
#include "item/SR.h"
#include "item/SlayerPortalItem.h"
#include "repository/ItemRepository.h"

//////////////////////////////////////////////////////////////////////////////
// Is this a stackable item?
//////////////////////////////////////////////////////////////////////////////
bool isStackable(Item::ItemClass IClass) {
    switch (IClass) {
    case Item::ITEM_CLASS_POTION:
    case Item::ITEM_CLASS_MAGAZINE:
    case Item::ITEM_CLASS_EVENT_STAR:
    case Item::ITEM_CLASS_SKULL:
    case Item::ITEM_CLASS_SERUM:
    case Item::ITEM_CLASS_VAMPIRE_ETC:
    case Item::ITEM_CLASS_WATER:
    case Item::ITEM_CLASS_HOLYWATER:
    case Item::ITEM_CLASS_BOMB_MATERIAL:
    case Item::ITEM_CLASS_BOMB:
    case Item::ITEM_CLASS_MINE:
    case Item::ITEM_CLASS_EVENT_ETC:
    case Item::ITEM_CLASS_RESURRECT_ITEM:
    case Item::ITEM_CLASS_ETC:
    case Item::ITEM_CLASS_MIXING_ITEM:
    case Item::ITEM_CLASS_LARVA:
    case Item::ITEM_CLASS_PUPA:
    case Item::ITEM_CLASS_COMPOS_MEI:
    case Item::ITEM_CLASS_EFFECT_ITEM:
    case Item::ITEM_CLASS_MOON_CARD:
    case Item::ITEM_CLASS_PET_ENCHANT_ITEM:
    case Item::ITEM_CLASS_LUCKY_BAG:
    case Item::ITEM_CLASS_PET_FOOD:
    case Item::ITEM_CLASS_MONEY: // Money is stackable.
        return true;
    default:
        return false;
    }

    return false;
}

//////////////////////////////////////////////////////////////////////////////
// Is this a stackable item?
//////////////////////////////////////////////////////////////////////////////
bool isStackable(const Item* pItem) {
    // if (pItem == NULL) return false;

    // by sigi. 2002.5.13
    return pItem != NULL && pItem->isStackable();
}

//////////////////////////////////////////////////////////////////////////////
// Are the two items of the same class and type?
//////////////////////////////////////////////////////////////////////////////
bool isSameItem(Item::ItemClass IClass1, Item::ItemClass IClass2, ItemType_t type1, ItemType_t type2) {
    return IClass1 == IClass2 && type1 == type2;
}

//////////////////////////////////////////////////////////////////////////////
// Are the two items of the same class and type?
//////////////////////////////////////////////////////////////////////////////
bool isSameItem(const Item* pItem1, const Item* pItem2) {
    return pItem1 != NULL && pItem2 != NULL && pItem1->getItemClass() == pItem2->getItemClass() &&
           pItem1->getItemType() == pItem2->getItemType();
}

//////////////////////////////////////////////////////////////////////////////
// Can the two items be stacked?
//////////////////////////////////////////////////////////////////////////////
bool canStack(const Item* pItem1, const Item* pItem2) {
    return isStackable(pItem1) && isSameItem(pItem1, pItem2);
}

//////////////////////////////////////////////////////////////////////////////
// Is this a two-handed weapon?
//////////////////////////////////////////////////////////////////////////////
bool isTwohandWeapon(const Item* pItem) {
    if (pItem == NULL)
        return false;

    switch (pItem->getItemClass()) {
    case Item::ITEM_CLASS_BLADE:
    case Item::ITEM_CLASS_AR:
    case Item::ITEM_CLASS_SR:
    case Item::ITEM_CLASS_SG:
    case Item::ITEM_CLASS_SMG:
    case Item::ITEM_CLASS_CROSS:
    case Item::ITEM_CLASS_MACE:
    case Item::ITEM_CLASS_VAMPIRE_WEAPON:
        return true;
    default:
        return false;
    }

    return false;
}

//////////////////////////////////////////////////////////////////////////////
// Is this a melee weapon?
//////////////////////////////////////////////////////////////////////////////
bool isMeleeWeapon(const Item* pItem) {
    if (pItem == NULL)
        return false;

    switch (pItem->getItemClass()) {
    case Item::ITEM_CLASS_SWORD:
    case Item::ITEM_CLASS_BLADE:
    case Item::ITEM_CLASS_CROSS:
    case Item::ITEM_CLASS_MACE:
    case Item::ITEM_CLASS_VAMPIRE_WEAPON:
    case Item::ITEM_CLASS_OUSTERS_CHAKRAM:
    case Item::ITEM_CLASS_OUSTERS_WRISTLET:
        return true;
    default:
        return false;
    }

    return false;
}

//////////////////////////////////////////////////////////////////////////////
// Is this a warrior, soldier or priest weapon?
//////////////////////////////////////////////////////////////////////////////
bool isFighterWeapon(const Item* pItem) {
    if (pItem == NULL)
        return false;

    switch (pItem->getItemClass()) {
    case Item::ITEM_CLASS_SWORD:
    case Item::ITEM_CLASS_BLADE:
        return true;
    default:
        return false;
    }

    return false;
}

bool isArmsWeapon(const Item* pItem) {
    if (pItem == NULL)
        return false;

    switch (pItem->getItemClass()) {
    case Item::ITEM_CLASS_AR:
    case Item::ITEM_CLASS_SR:
    case Item::ITEM_CLASS_SG:
    case Item::ITEM_CLASS_SMG:
        return true;
        break;
    default:
        return false;
        break;
    }

    return false;
}

bool isClericWeapon(const Item* pItem) {
    if (pItem == NULL)
        return false;

    switch (pItem->getItemClass()) {
    case Item::ITEM_CLASS_CROSS:
    case Item::ITEM_CLASS_MACE:
        return true;
    default:
        return false;
    }

    return false;
}

//////////////////////////////////////////////////////////////////////////////
// Is this magazine suitable for the gun?
//////////////////////////////////////////////////////////////////////////////
bool isSuitableMagazine(const Item* pGun, const Item* pMagazine, bool hasVivid) {
    if (pGun == NULL || pMagazine == NULL)
        return false;
    if (pMagazine->getItemClass() != Item::ITEM_CLASS_MAGAZINE)
        return false;

    ItemType_t magazineType = pMagazine->getItemType();
    MagazineInfo* pInfo =
        dynamic_cast<MagazineInfo*>(de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_MAGAZINE, magazineType));

    switch (pGun->getItemClass()) {
    case Item::ITEM_CLASS_SG:
        if (pInfo->getGunType() != MagazineInfo::SG)
            return false;
        break;
    case Item::ITEM_CLASS_AR:
        if (pInfo->getGunType() != MagazineInfo::AR)
            return false;
        break;
    case Item::ITEM_CLASS_SMG:
        if (pInfo->getGunType() != MagazineInfo::SMG)
            return false;
        break;
    case Item::ITEM_CLASS_SR:
        if (pInfo->getGunType() != MagazineInfo::SR)
            return false;
        break;
    default:
        return false;
    }

    if (pInfo->isVivid() && !hasVivid)
        return false;

    return true;
}

//////////////////////////////////////////////////////////////////////////////
// Is this a Slayer weapon?
//////////////////////////////////////////////////////////////////////////////
bool isSlayerWeapon(Item::ItemClass IClass) {
    switch (IClass) {
    case Item::ITEM_CLASS_SWORD:
    case Item::ITEM_CLASS_BLADE:
    case Item::ITEM_CLASS_CROSS:
    case Item::ITEM_CLASS_MACE:
    case Item::ITEM_CLASS_AR:
    case Item::ITEM_CLASS_SR:
    case Item::ITEM_CLASS_SG:
    case Item::ITEM_CLASS_SMG:
        return true;
    default:
        return false;
    }

    return false;
}

//////////////////////////////////////////////////////////////////////////////
// Is this a Vampire weapon?
//////////////////////////////////////////////////////////////////////////////
bool isVampireWeapon(Item::ItemClass IClass) {
    switch (IClass) {
    case Item::ITEM_CLASS_VAMPIRE_WEAPON:
        return true;
    default:
        return false;
    }

    return false;
}

//////////////////////////////////////////////////////////////////////////////
// Is this an Ousters weapon?
//////////////////////////////////////////////////////////////////////////////
bool isOustersWeapon(Item::ItemClass IClass) {
    switch (IClass) {
    case Item::ITEM_CLASS_OUSTERS_CHAKRAM:
    case Item::ITEM_CLASS_OUSTERS_WRISTLET:
        return true;
    default:
        return false;
    }

    return false;
}

//////////////////////////////////////////////////////////////////////////////
// Is this Slayer armor?
//////////////////////////////////////////////////////////////////////////////
bool isSlayerArmor(Item::ItemClass IClass) {
    switch (IClass) {
    case Item::ITEM_CLASS_COAT:
    case Item::ITEM_CLASS_TROUSER:
    case Item::ITEM_CLASS_GLOVE:
    case Item::ITEM_CLASS_BELT:
    case Item::ITEM_CLASS_SHOES:
    case Item::ITEM_CLASS_SHIELD:
    case Item::ITEM_CLASS_HELM:
    case Item::ITEM_CLASS_SHOULDER_ARMOR:
        return true;
    default:
        break;
    }

    return false;
}
//////////////////////////////////////////////////////////////////////////////
// Is this Vampire armor?
//////////////////////////////////////////////////////////////////////////////
bool isVampireArmor(Item::ItemClass IClass) {
    switch (IClass) {
    case Item::ITEM_CLASS_VAMPIRE_COAT:
    case Item::ITEM_CLASS_PERSONA:
        return true;
    default:
        break;
    }

    return false;
}

//////////////////////////////////////////////////////////////////////////////
// Is this Ousters armor?
//////////////////////////////////////////////////////////////////////////////
bool isOustersArmor(Item::ItemClass IClass) {
    switch (IClass) {
    case Item::ITEM_CLASS_OUSTERS_COAT:
    case Item::ITEM_CLASS_OUSTERS_BOOTS:
    case Item::ITEM_CLASS_OUSTERS_CIRCLET:
    case Item::ITEM_CLASS_OUSTERS_ARMSBAND:
    case Item::ITEM_CLASS_MITTEN:
        return true;
    default:
        break;
    }

    return false;
}

//////////////////////////////////////////////////////////////////////////////
// Is this a Slayer accessory?
//////////////////////////////////////////////////////////////////////////////
bool isSlayerAccessory(Item::ItemClass IClass) {
    switch (IClass) {
    case Item::ITEM_CLASS_NECKLACE:
    case Item::ITEM_CLASS_BRACELET:
    case Item::ITEM_CLASS_RING:
    case Item::ITEM_CLASS_CARRYING_RECEIVER:
        return true;
    default:
        break;
    }
    return false;
}

//////////////////////////////////////////////////////////////////////////////
// Is this a Vampire accessory?
//////////////////////////////////////////////////////////////////////////////
bool isVampireAccessory(Item::ItemClass IClass) {
    switch (IClass) {
    case Item::ITEM_CLASS_VAMPIRE_NECKLACE:
    case Item::ITEM_CLASS_VAMPIRE_BRACELET:
    case Item::ITEM_CLASS_VAMPIRE_RING:
    case Item::ITEM_CLASS_VAMPIRE_AMULET:
    case Item::ITEM_CLASS_VAMPIRE_EARRING:
    case Item::ITEM_CLASS_DERMIS:
        return true;
    default:
        break;
    }
    return false;
}

//////////////////////////////////////////////////////////////////////////////
// Is this an Ousters accessory?
//////////////////////////////////////////////////////////////////////////////
bool isOustersAccessory(Item::ItemClass IClass) {
    switch (IClass) {
    case Item::ITEM_CLASS_OUSTERS_ARMSBAND:
    case Item::ITEM_CLASS_OUSTERS_PENDENT:
    case Item::ITEM_CLASS_OUSTERS_RING:
    case Item::ITEM_CLASS_OUSTERS_STONE:
    case Item::ITEM_CLASS_FASCIA:
        return true;
    default:
        break;
    }
    return false;
}


//////////////////////////////////////////////////////////////////////////////
// Is this a repairable item?
//////////////////////////////////////////////////////////////////////////////
bool isRepairableItem(const Item* pItem) {
    // Unique items do not need repairing.
    if (pItem == NULL || pItem->isUnique() || pItem->isTimeLimitItem())
        return false;
    if (pItem->isFlagItem())
        return false;

    switch (pItem->getItemClass()) {
    case Item::ITEM_CLASS_MOTORCYCLE:
    case Item::ITEM_CLASS_POTION:
    case Item::ITEM_CLASS_MAGAZINE:
    case Item::ITEM_CLASS_WATER:
    case Item::ITEM_CLASS_HOLYWATER:
    case Item::ITEM_CLASS_BOMB_MATERIAL:
    case Item::ITEM_CLASS_ETC:
    case Item::ITEM_CLASS_KEY:
    case Item::ITEM_CLASS_BOMB:
    case Item::ITEM_CLASS_MINE:
    case Item::ITEM_CLASS_LEARNINGITEM:
    case Item::ITEM_CLASS_CORPSE:
    case Item::ITEM_CLASS_SKULL:
    case Item::ITEM_CLASS_SERUM:
    case Item::ITEM_CLASS_VAMPIRE_ETC:
    case Item::ITEM_CLASS_VAMPIRE_PORTAL_ITEM:
    case Item::ITEM_CLASS_EVENT_GIFT_BOX:
    case Item::ITEM_CLASS_EVENT_STAR:
    case Item::ITEM_CLASS_MONEY:
    case Item::ITEM_CLASS_VAMPIRE_AMULET:
    case Item::ITEM_CLASS_QUEST_ITEM:
    case Item::ITEM_CLASS_RELIC:
    case Item::ITEM_CLASS_BLOOD_BIBLE:
    case Item::ITEM_CLASS_CASTLE_SYMBOL:
    case Item::ITEM_CLASS_COUPLE_RING:
    case Item::ITEM_CLASS_VAMPIRE_COUPLE_RING:
    case Item::ITEM_CLASS_EVENT_ITEM:
    case Item::ITEM_CLASS_MOON_CARD:
    case Item::ITEM_CLASS_SWEEPER:
        //		case Item::ITEM_CLASS_PET_ITEM:
    case Item::ITEM_CLASS_LUCKY_BAG:
    case Item::ITEM_CLASS_CARRYING_RECEIVER:
    case Item::ITEM_CLASS_DERMIS:
    case Item::ITEM_CLASS_FASCIA:
        return false;
    default:
        return true;
    }

    return true;
}


//////////////////////////////////////////////////////////////////////////////
// Repair an item.
//////////////////////////////////////////////////////////////////////////////
void repairItem(Item* pItem) {
    if (pItem != NULL && !pItem->isUnique() && isRepairableItem(pItem)) {
        Item::ItemClass IClass = pItem->getItemClass();

        if (IClass == Item::ITEM_CLASS_SLAYER_PORTAL_ITEM) {
            SlayerPortalItem* pSlayerPortalItem = dynamic_cast<SlayerPortalItem*>(pItem);
            pSlayerPortalItem->setCharge(pSlayerPortalItem->getMaxCharge());
        } else if (IClass == Item::ITEM_CLASS_OUSTERS_SUMMON_ITEM) {
            OustersSummonItem* pOustersSummonItem = dynamic_cast<OustersSummonItem*>(pItem);
            pOustersSummonItem->setCharge(pOustersSummonItem->getMaxCharge());
        } else {
            // Obtain the maximum durability,
            Durability_t maxDurability = computeMaxDurability(pItem);
            // then repair.
            pItem->setDurability(maxDurability);
        }
    }
}

//////////////////////////////////////////////////////////////////////////////
// Get the item's maximum durability.
//////////////////////////////////////////////////////////////////////////////
Durability_t computeMaxDurability(Item* pItem) {
    if (pItem == NULL)
        return 0;

    // The item's own maximum: computeBaseMaxDurability for a ConcreteItem,
    // Item's default of 1 for every other item.
    Durability_t maxDurability = pItem->getMaxDurability();

    // The plus-points of the durability options, in the order the item
    // carries them. An option type of 0 is an empty slot.
    const list<OptionType_t>& optionTypes = pItem->getOptionTypeList();
    std::vector<int> plusPoints;

    list<OptionType_t>::const_iterator itr;

    for (itr = optionTypes.begin(); itr != optionTypes.end(); itr++) {
        OptionType_t OptionType = *itr;

        if (OptionType != 0) {
            OptionInfo* pOptionInfo = de::gameContext().optionInfos().getOptionInfo(OptionType);

            if (pOptionInfo->getClass() == OPTION_DURABILITY) {
                plusPoints.push_back(pOptionInfo->getPlusPoint());
            }
        }
    }

    return decore::maxDurabilityWithOptions(maxDurability, plusPoints.data(), (int)plusPoints.size());
}

// de-core names the server's sex and gender restriction numbers.
static_assert(decore::sex::Female == FEMALE && decore::sex::Male == MALE);
static_assert(decore::gender::Both == GENDER_BOTH && decore::gender::Male == GENDER_MALE &&
              decore::gender::Female == GENDER_FEMALE);

//////////////////////////////////////////////////////////////////////////////
// What an item requires of its wearer.
//////////////////////////////////////////////////////////////////////////////
decore::EquipRequirement computeEquipRequirement(decore::EquipRace race, const Item* pItem, const ItemInfo* pItemInfo) {
    decore::EquipRequirement base = {};
    base.str = pItemInfo->getReqSTR();
    base.dex = pItemInfo->getReqDEX();
    base.inte = pItemInfo->getReqINT();
    base.sum = pItemInfo->getReqSum();
    base.level = pItemInfo->getReqLevel();
    base.gender = pItemInfo->getReqGender();

    // The sum and level requirement of each option, in the order the item
    // carries them.
    const list<OptionType_t>& optionTypes = pItem->getOptionTypeList();
    std::vector<int> optionReqSums;
    std::vector<int> optionReqLevels;

    list<OptionType_t>::const_iterator itr;
    for (itr = optionTypes.begin(); itr != optionTypes.end(); itr++) {
        OptionInfo* pOptionInfo = de::gameContext().optionInfos().getOptionInfo(*itr);
        optionReqSums.push_back(pOptionInfo->getReqSum());
        optionReqLevels.push_back(pOptionInfo->getReqLevel());
    }

    return decore::requiredStats(race, base, optionReqSums.data(), optionReqLevels.data(), (int)optionReqSums.size());
}

//////////////////////////////////////////////////////////////////////////////
// Change the magazine.
//////////////////////////////////////////////////////////////////////////////
Bullet_t reloadArmsItem(Item* pGun, Item* pMagazine) {
    Assert(pGun != NULL);
    Assert(pMagazine != NULL);

    // The vivid magazine check is done before entering here. This function always reloads.
    if (isSuitableMagazine(pGun, pMagazine, true) == false)
        return false;

    Item::ItemClass IClass = pGun->getItemClass();
    ItemType_t MagazineType = pMagazine->getItemType();
    ItemInfo* pItemInfo = de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_MAGAZINE, MagazineType);
    MagazineInfo* pMagazineInfo = dynamic_cast<MagazineInfo*>(pItemInfo);
    Bullet_t BulletCount = pMagazineInfo->getMaxBullets();
    Silver_t Silver = pMagazineInfo->getMaxSilver();

    if (IClass == Item::ITEM_CLASS_AR) {
        AR* pAR = dynamic_cast<AR*>(pGun);
        pAR->setBulletCount(BulletCount);
        pAR->setSilver(Silver);
        return pAR->getBulletCount();
    } else if (IClass == Item::ITEM_CLASS_SR) {
        SR* pSR = dynamic_cast<SR*>(pGun);
        pSR->setBulletCount(BulletCount);
        pSR->setSilver(Silver);
        return pSR->getBulletCount();
    } else if (IClass == Item::ITEM_CLASS_SG) {
        SG* pSG = dynamic_cast<SG*>(pGun);
        pSG->setBulletCount(BulletCount);
        pSG->setSilver(Silver);
        return pSG->getBulletCount();
    } else if (IClass == Item::ITEM_CLASS_SMG) {
        SMG* pSMG = dynamic_cast<SMG*>(pGun);
        pSMG->setBulletCount(BulletCount);
        pSMG->setSilver(Silver);
        return pSMG->getBulletCount();
    }
    return 0;
}

//////////////////////////////////////////////////////////////////////////////
// Remove a bullet.
//////////////////////////////////////////////////////////////////////////////
Bullet_t decreaseBullet(Item* pWeapon)

{
    __BEGIN_TRY

    if (pWeapon == NULL) {
        ofstream file("bulletBug.txt", ios::out | ios::app);
        file << "decreaseBullet() : pWeapon is NULL" << endl;
        return 0;
    }

    Item::ItemClass IClass = pWeapon->getItemClass();
    Bullet_t bullet = 0;
    Silver_t silver = 0;

    ///*
    if (IClass == Item::ITEM_CLASS_AR) {
        AR* pAR = dynamic_cast<AR*>(pWeapon);
        bullet = max(0, (int)(pAR->getBulletCount() - 1));
        pAR->setBulletCount(bullet);

        silver = max(0, (int)(pAR->getSilver() - 1));
        pAR->setSilver(silver);
    } else if (IClass == Item::ITEM_CLASS_SR) {
        SR* pSR = dynamic_cast<SR*>(pWeapon);
        bullet = max(0, (int)(pSR->getBulletCount() - 1));
        pSR->setBulletCount(bullet);

        silver = max(0, (int)(pSR->getSilver() - 1));
        pSR->setSilver(silver);
    } else if (IClass == Item::ITEM_CLASS_SG) {
        SG* pSG = dynamic_cast<SG*>(pWeapon);
        bullet = max(0, (int)(pSG->getBulletCount() - 1));
        pSG->setBulletCount(bullet);

        silver = max(0, (int)(pSG->getSilver() - 1));
        pSG->setSilver(silver);
    } else if (IClass == Item::ITEM_CLASS_SMG) {
        SMG* pSMG = dynamic_cast<SMG*>(pWeapon);
        bullet = max(0, (int)(pSMG->getBulletCount() - 1));
        pSMG->setBulletCount(bullet);

        silver = max(0, (int)(pSMG->getSilver() - 1));
        pSMG->setSilver(silver);
    } else {
        filelog("bulletBug.log", "decreaseBullet() : Invalid item class : %s\n", ItemClass2String[IClass].c_str());
        throw Error("decreaseBullet() : Invalid item class");
    }
    //*/

    return bullet;

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////////////
// Return the number of remaining bullets.
//////////////////////////////////////////////////////////////////////////////
Bullet_t getRemainBullet(Item* pWeapon)

{
    __BEGIN_TRY

    if (pWeapon == NULL) {
        return 0;
    }

    Item::ItemClass IClass = pWeapon->getItemClass();

    // by sigi. 2002.5.16
    if (IClass == Item::ITEM_CLASS_AR) {
        AR* pAR = dynamic_cast<AR*>(pWeapon);
        return pAR->getBulletCount();
    } else if (IClass == Item::ITEM_CLASS_SR) {
        SR* pSR = dynamic_cast<SR*>(pWeapon);
        return pSR->getBulletCount();
    } else if (IClass == Item::ITEM_CLASS_SG) {
        SG* pSG = dynamic_cast<SG*>(pWeapon);
        return pSG->getBulletCount();
    } else if (IClass == Item::ITEM_CLASS_SMG) {
        SMG* pSMG = dynamic_cast<SMG*>(pWeapon);
        return pSMG->getBulletCount();
    } else {
        filelog("bullet.log", "getRemainBullet() : Invalid item class : %s\n", ItemClass2String[IClass].c_str());
        throw Error("getRemainBullet() : Invalid item class");
    }

    return 0;

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
// Can this item be picked up?
//////////////////////////////////////////////////////////////////////////////
bool isPortableItem(Item* pItem) {
    Assert(pItem != NULL);

    //	if ( pItem->isTimeLimitItem() ) return false;

    switch (pItem->getItemClass()) {
    case Item::ITEM_CLASS_CORPSE:
    case Item::ITEM_CLASS_MOTORCYCLE:
    case Item::ITEM_CLASS_COUPLE_RING:
    case Item::ITEM_CLASS_VAMPIRE_COUPLE_RING:
        return false;
    default:
        return true;
    }

    return false;
}

//////////////////////////////////////////////////////////////////////////////
// Is this a usable item?
// Use here means a consumable item that disappears when used.
//////////////////////////////////////////////////////////////////////////////
bool isUsableItem(Item* pItem, Creature* pUser) {
    Assert(pItem != NULL);
    Assert(pUser != NULL);

    // Later there may be items of the same class where some can be used and
    // some cannot.
    // ItemType_t IType = pItem->getItemType();

    switch (pItem->getItemClass()) {
    case Item::ITEM_CLASS_POTION:
        if (pUser->isSlayer())
            return true;
        break;
    case Item::ITEM_CLASS_MAGAZINE:
        if (pUser->isSlayer())
            return true;
        break;
    case Item::ITEM_CLASS_KEY:
        if (de::gameContext().variables().isSummonMotorcycle() && pUser->isSlayer())
            return true;
        break;
    case Item::ITEM_CLASS_ETC:
        if (pUser->isSlayer() && pItem->getItemType() == 1)
            return true;
        break;
    case Item::ITEM_CLASS_SERUM:
        if (pUser->isVampire())
            return true;
        break;
    case Item::ITEM_CLASS_VAMPIRE_ETC:
        if (pUser->isVampire())
            return true;
        break;
    case Item::ITEM_CLASS_SLAYER_PORTAL_ITEM:
        if (pUser->isSlayer())
            return true;
        break;
    case Item::ITEM_CLASS_EVENT_TREE:
        if (pItem->getItemType() == 12 || (pItem->getItemType() >= 26 && pItem->getItemType() <= 28))
            return true; // Completed tree.
        break;

    case Item::ITEM_CLASS_EVENT_ETC:
        return true;
        break;

    case Item::ITEM_CLASS_COUPLE_RING:
        if (pUser->isSlayer())
            return true;
        break;
    case Item::ITEM_CLASS_VAMPIRE_COUPLE_RING:
        if (pUser->isVampire())
            return true;
        break;

    case Item::ITEM_CLASS_DYE_POTION:
        return true;

    case Item::ITEM_CLASS_RESURRECT_ITEM:
        if (pUser->isFlag(Effect::EFFECT_CLASS_COMA))
            return true;
        break;

    case Item::ITEM_CLASS_PUPA:
        if (pUser->isOusters())
            return true;
        break;

    case Item::ITEM_CLASS_COMPOS_MEI:
        if (pUser->isOusters())
            return true;
        break;

    case Item::ITEM_CLASS_OUSTERS_SUMMON_ITEM:
        if (pUser->isOusters())
            return true;
        break;

    case Item::ITEM_CLASS_EFFECT_ITEM:
    case Item::ITEM_CLASS_PET_ITEM:
    case Item::ITEM_CLASS_PET_FOOD:
    case Item::ITEM_CLASS_PET_ENCHANT_ITEM:
        return true;
        break;

    case Item::ITEM_CLASS_EVENT_GIFT_BOX:
        if (pItem->getItemType() >= 6 && pItem->getItemType() <= 15)
            return true;
        if (pItem->getItemType() >= 19 && pItem->getItemType() <= 21)
            return true;
        break;

    case Item::ITEM_CLASS_SMS_ITEM:
        return true;
        break;

    case Item::ITEM_CLASS_TRAP_ITEM:
        return true;
        break;
    // add by Coffee 2007-6-9
    case Item::ITEM_CLASS_MOON_CARD:
        if (pItem->getItemType() >= 5 && pItem->getItemType() <= 7)
            return true;
        break;
    default:
        return false;
    }

    return false;
}

//////////////////////////////////////////////////////////////////////////////
// Decrease the item count.
//////////////////////////////////////////////////////////////////////////////
ItemNum_t decreaseItemNum(Item* pItem, Inventory* pInventory, const string& OwnerID, Storage storage,
                          StorageID_t storageID, BYTE x, BYTE y) {
    Assert(pItem != NULL);
    Assert(pInventory != NULL);
    Assert(OwnerID != "");
    Assert(isStackable(pItem));

    if (pItem->getNum() > 1) {
        pItem->setNum(pItem->getNum() - 1);             // Decrease the item count by one.
        pInventory->decreaseItemNum();                  // Decrease the inventory's total count.
        pInventory->decreaseWeight(pItem->getWeight()); // Decrease the inventory's total weight.
        // pItem->save(OwnerID, storage, storageID, x, y); // Save the item information.
        //  Item save optimization.
        char pField[80];
        snprintf(pField, sizeof(pField), "Num=%d", pItem->getNum());
        pItem->tinysave(pField);


        return pItem->getNum();
    } else // Only one item was left, so delete it.
    {
        pInventory->deleteItem(x, y);
        pItem->destroy();
        SAFE_DELETE(pItem);
        return 0;
    }

    return 0;
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void processItemBug(Creature* pCreature, Item* pItem) {
    PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pCreature);
    Assert(pPC != NULL);
    Assert(pItem != NULL);

    pPC->addItemToGarbage(pItem);
}

void processItemBugEx(Creature* pCreature, Item* pItem) {
    PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pCreature);
    Assert(pPC != NULL);
    Assert(pItem != NULL);

    pPC->addItemToGarbage(pItem);
    pItem->save(pCreature->getName(), STORAGE_GARBAGE, 0, 0, 0);
}

bool hasOptionType(const list<OptionType_t>& optionTypes, OptionType_t optionType) {
    if (optionTypes.empty())
        return false;

    list<OptionType_t>::const_iterator itr;

    for (itr = optionTypes.begin(); itr != optionTypes.end(); itr++) {
        if (*itr == optionType)
            return true;
    }

    return false;
}

bool hasOptionClass(const list<OptionType_t>& optionTypes, OptionType_t optionType) {
    if (optionTypes.empty())
        return false;

    try {
        OptionInfo* pOptionInfo = de::gameContext().optionInfos().getOptionInfo(optionType);

        if (pOptionInfo == NULL)
            return false;

        OptionClass newOptionClass = pOptionInfo->getClass();

        list<OptionType_t>::const_iterator itr;

        for (itr = optionTypes.begin(); itr != optionTypes.end(); itr++) {
            pOptionInfo = de::gameContext().optionInfos().getOptionInfo(*itr);
            if (pOptionInfo == NULL)
                return false;
            if (pOptionInfo->getClass() == newOptionClass)
                return true;
        }
    } catch (Throwable& t) {
        // Ignored.
        filelog("hasOptionClassBug.txt", "%s", t.toString().c_str());
    }

    return false;
}

void setOptionTypeFromField(list<OptionType_t>& optionTypes, const string& optionField) {
    if (optionField.empty())
        return;

    const char* sep = " ";
    char* s = new char[optionField.size() + 1];
    strcpy(s, optionField.c_str());
    char* p;
    int aa;
    p = strtok(s, sep);
    while (p) {
        aa = atoi(p);
        optionTypes.push_back((OptionType_t)aa);
        p = strtok(NULL, sep);
    }
}


void setOptionTypeToField(const list<OptionType_t>& optionTypes, string& optionField) {
    if (optionTypes.empty())
        return;

    int ch;
    char string[128];
    list<OptionType_t>::const_iterator itr;
    for (itr = optionTypes.begin(); itr != optionTypes.end(); itr++) {
        if (itr != optionTypes.begin()) {
            optionField += " ";
        }
        ch = *itr;
        snprintf(string, sizeof(string), "%d", ch);
        optionField += string;
    }
}

string getOptionTypeToString(const list<OptionType_t>& optionTypes) {
    if (optionTypes.empty())
        return string("NONE");

    string optionField;
    unsigned char ch;

    char str[12];

    list<OptionType_t>::const_iterator itr;
    for (itr = optionTypes.begin(); itr != optionTypes.end(); itr++) {
        if (itr != optionTypes.begin()) {
            optionField += " ";
        }
        ch = *itr;
        snprintf(str, sizeof(str), "%d", (int)ch);
        optionField += str;
    }

    return optionField;
}


//////////////////////////////////////////////////////////////////////////////
// Rare item: attach the next option?
//////////////////////////////////////////////////////////////////////////////
bool isPossibleNextOption(ITEM_TEMPLATE* pTemplate) {
    // At most five options are attached: the caller has already pushed the
    // candidate when it asks.
    if (pTemplate->OptionType.size() >= 5)
        return false;

    // If nothing is attached yet, always attach one.
    if (pTemplate->OptionType.empty())
        return true;

    // Apply the option probability set specifically on the item.
    if (pTemplate->NextOptionRatio != 0) {
        int dice = rand() % 100;
        // cout << "NextOptionRatio : " << dice << " < " << (int)pTemplate->NextOptionRatio << endl;
        return dice < pTemplate->NextOptionRatio;
    }

    try {
        // Get the probability that the next option is attached, per item kind.
        ItemInfo* pItemInfo = de::gameContext().itemInfos().getItemInfo(pTemplate->ItemClass, pTemplate->ItemType);
        Ratio_t nextItemRatio = pItemInfo->getNextOptionRatio();

        // Get the probability of the next option, given the options already attached.
        list<OptionType_t>::const_iterator itr = pTemplate->OptionType.begin();
        Ratio_t nextOptionRatio = nextItemRatio; // To reduce the computation.
        Ratio_t baseMultiplier = 100;            // 100%
        for (; itr != pTemplate->OptionType.end(); itr++) {
            OptionInfo* pOptionInfo = de::gameContext().optionInfos().getOptionInfo(*itr);
            if (pOptionInfo == NULL)
                return false;
            nextOptionRatio *= pOptionInfo->getNextOptionRatio();
            baseMultiplier *= 100;
        }

        // [Example] For an item that already carries two options,
        //        the probability that a third option is attached:
        //
        // nextItemRatio = 10 %
        // nextOptionRatio1 = 20 %
        // nextOptionRatio2 = 30 %
        //
        // total nextOptionRatio = nextItemRatio * nextOptionRatio1 * nextOptionRatio2
        //                       = 10 * 20 * 30 = 6000
        //
        // baseMultiplier = 100(initial) * 100(option1) * 100(option2)
        //                = 1000000
        //
        // selectRatio = 0~baseMultiplier = 0~1000000
        //
        // Succeed = nextOptionRatio/baseMultiplier
        //         = 6000/1000000
        //         = 6 / 1000
        //         = 0.6%

        Ratio_t selectRatio = rand() % baseMultiplier;

        // Apply the rare item looting probability.
        nextOptionRatio = getPercentValue(nextOptionRatio, de::gameContext().variables().getRareItemRatio());

        // Probability check
        return selectRatio < nextOptionRatio;

    } catch (Throwable& t) {
        // Ignored for now.
        filelog("nextOptionBug.txt", "%s", t.toString().c_str());
    }

    return false;
}

//////////////////////////////////////////////////////////////////////////////
// Gamble item creation
//////////////////////////////////////////////////////////////////////////////
Item* getRandomMysteriousItem(Creature* pCreature, Item::ItemClass itemClass, int maxLevel)

{
    __BEGIN_TRY

    Assert(pCreature != NULL);

    InfoClassManager* pInfoClass = de::gameContext().itemInfos().getInfoManager(itemClass);
    Assert(pInfoClass != NULL);

    ItemType_t itemType = 0;
    OptionType_t optionType = 1;

    ItemInfo* pItemInfo = NULL;

    OptionInfoManager& optionInfos = de::gameContext().optionInfos();

    //----------------------------------------------------------------------
    // Slayer case
    //----------------------------------------------------------------------
    if (pCreature->isSlayer()) {
        Slayer* pSlayer = dynamic_cast<Slayer*>(pCreature);

        Attr_t CSTR = pSlayer->getSTR(ATTR_BASIC);
        Attr_t CDEX = pSlayer->getDEX(ATTR_BASIC);
        Attr_t CINT = pSlayer->getINT(ATTR_BASIC);
        Attr_t CSUM = CSTR + CDEX + CINT;

        Attr_t ReqSTR, ReqSTR2;
        Attr_t ReqDEX, ReqDEX2;
        Attr_t ReqINT, ReqINT2;
        Attr_t ReqSum, ReqSum2;
        Attr_t ReqGender;

        // Raise the limit that can be gambled a little.
        CSTR += 3;
        CDEX += 3;
        CINT += 3;
        CSUM += 5;

        // Level limit
        if (maxLevel != 0) {
            int maxAttr = maxLevel * 2 / 3; // attr is taken as 2/3 of SUM.
            CSTR = min((int)maxAttr, (int)CSTR);
            CDEX = min((int)maxAttr, (int)CDEX);
            CINT = min((int)maxAttr, (int)CINT);
            CSUM = min((int)maxLevel, (int)CSUM);
        }

        // Try only ten times.
        int i = 10;
        do {
            itemType = pInfoClass->getRandomItemType();

            // Check whether the level allows creating this itemType.
            pItemInfo = de::gameContext().itemInfos().getItemInfo(itemClass, itemType);

            ReqSTR2 = ReqSTR = pItemInfo->getReqSTR();
            ReqDEX2 = ReqDEX = pItemInfo->getReqDEX();
            ReqINT2 = ReqINT = pItemInfo->getReqINT();
            ReqSum2 = ReqSum = pItemInfo->getReqSum();
            ReqGender = pItemInfo->getReqGender();

            if (CSTR >= ReqSTR && CDEX >= ReqDEX && CINT >= ReqINT && CSUM >= ReqSum &&
                (ReqGender == GENDER_BOTH || (pSlayer->getSex() == MALE && ReqGender == GENDER_MALE) ||
                 (pSlayer->getSex() == FEMALE && ReqGender == GENDER_FEMALE))) {
                // Settle on this item type.
                break;
            }

        } while (--i);

        if (i == 0) {
            // For females the default item type differs.
            if ((itemClass == Item::ITEM_CLASS_COAT || itemClass == Item::ITEM_CLASS_TROUSER) &&
                pSlayer->getSex() == FEMALE) {
                itemType = 1;
            } else {
                itemType = 0;
            }

            // So that the check below ignores it.
            pItemInfo = NULL;
        }


        // If the item carries options,
        // raise the attribute limit according to the kinds of option.
        int maxOptionLevel = max(1, min(100, (int)(CSUM / 3)));

        const vector<OptionType_t>& optionVector =
            optionInfos.getPossibleGambleOptionVector((Item::ItemClass)itemClass, maxOptionLevel);
        vector<OptionType_t>::const_iterator iOption;

        // Get the total OptionRatio.
        int itemOptionRatio = optionInfos.getTotalGambleRatio((Item::ItemClass)itemClass, maxOptionLevel);

        if (optionVector.size() > 0 && itemOptionRatio > 0 && (pItemInfo == NULL || !pItemInfo->isUnique())) {
            // Try only ten times.
            int i = 10;

            do {
                // Select an option at random.
                int optionRatio = random() % itemOptionRatio;
                int ratioSum = 0;

                // cout << "Ratio = " << optionRatio << "/" << itemOptionRatio << endl;

                OptionInfo* pOptionInfo = NULL;

                for (iOption = optionVector.begin(); iOption != optionVector.end(); iOption++) {
                    optionType = *iOption;

                    pOptionInfo = optionInfos.getOptionInfo(optionType);
                    ratioSum += pOptionInfo->getRatio();

                    if (optionRatio < ratioSum) {
                        // Select this option.
                        // cout << "select : " << (int)optionType << endl;
                        break;
                    }
                }

                // Add the option's required attributes and
                if (ReqSTR != 0)
                    ReqSTR = ReqSTR2 + (pOptionInfo->getReqSum() * 2);
                if (ReqDEX != 0)
                    ReqDEX = ReqDEX2 + (pOptionInfo->getReqSum() * 2);
                if (ReqINT != 0)
                    ReqINT = ReqINT2 + (pOptionInfo->getReqSum() * 2);
                if (ReqSum != 0)
                    ReqSum = ReqSum2 + (pOptionInfo->getReqSum());

                // if (ReqSTR != 0) cout << "CSTR = " << (int)CSTR << ", ReqSTR = " << (int)ReqSTR2 << " + " <<
                // (int)pOptionInfo->getReqSum() << "*2 = " << (int)ReqSTR << endl; if (ReqDEX != 0) cout << "CDEX = "
                // << (int)CDEX << ", ReqDEX = " << (int)ReqDEX2 << " + " << (int)pOptionInfo->getReqSum() << "*2 = " <<
                // (int)ReqDEX << endl; if (ReqINT != 0) cout << "CINT = " << (int)CINT << ", ReqINT = " << (int)ReqINT2
                // << " + " << (int)pOptionInfo->getReqSum() << "*2 = " << (int)ReqINT << endl; if (ReqSum != 0) cout <<
                // "CSUM = " << (int)CSUM << ", ReqSum = " << (int)ReqSum2 << " + " << (int)pOptionInfo->getReqSum() <<
                // " = " << (int)ReqSum << endl;

                // cout << "CSTR=" << CSTR << ", "
                //	<< "CDEX=" << CDEX << ", "
                //	<< "CINT=" << CINT << endl;

                // check whether the item matches the player's attributes.
                if (CSTR >= ReqSTR && CDEX >= ReqDEX && CINT >= ReqINT && CSUM >= ReqSum) {
                    // Settle on this option type.
                    // cout << "OK!" << endl;
                    break;
                }

            } while (--i);

            if (i == 0) {
                optionType = 0; //(rand()%3==0? 1: (rand()%2? 6:11));
            }
        }
        // No option. If nothing suitable is found, any of STR+1, DEX+1, INT+1.
        else
            optionType = 0; //(rand()%3==0? 1: (rand()%2? 6:11));
    }
    //----------------------------------------------------------------------
    // Vampire case
    //----------------------------------------------------------------------
    else if (pCreature->isVampire()) {
        Vampire* pVampire = dynamic_cast<Vampire*>(pCreature);

        Level_t CLevel = pVampire->getLevel();

        // Raise the limit that can be gambled a little.
        CLevel += 3;

        // Level limit
        if (maxLevel != 0) {
            CLevel = min((int)maxLevel, (int)CLevel);
        }

        Attr_t ReqLevel, ReqLevel2;
        Attr_t ReqGender;


        // Try only ten times.
        int i = 10;
        do {
            itemType = pInfoClass->getRandomItemType();

            // Check whether the level allows creating this itemType.
            pItemInfo = de::gameContext().itemInfos().getItemInfo(itemClass, itemType);

            ReqLevel2 = ReqLevel = pItemInfo->getReqLevel();
            ReqGender = pItemInfo->getReqGender();

            // The level limit must be absent or the level high enough, and
            // the gender must match.
            if ((ReqLevel <= 0 || CLevel >= ReqLevel) &&
                (ReqGender == GENDER_BOTH || (pVampire->getSex() == MALE && ReqGender == GENDER_MALE) ||
                 (pVampire->getSex() == FEMALE && ReqGender == GENDER_FEMALE))) {
                break;
            }

        } while (--i);

        if (i == 0) {
            // For females the default item type differs.
            if (pVampire->getSex() == FEMALE) {
                itemType = 1;
            } else {
                itemType = 0;
            }
        }

        // If the item carries options,
        // raise the attribute limit according to the kinds of option.
        int maxOptionLevel = max(1, min(100, (int)CLevel));

        const vector<OptionType_t>& optionVector =
            optionInfos.getPossibleGambleOptionVector((Item::ItemClass)itemClass, maxOptionLevel);
        vector<OptionType_t>::const_iterator iOption;

        // Get the total OptionRatio.
        int itemOptionRatio = optionInfos.getTotalGambleRatio((Item::ItemClass)itemClass, maxOptionLevel);


        if (optionVector.size() > 0 && itemOptionRatio > 0 && (pItemInfo == NULL || !pItemInfo->isUnique())) {
            // Try only ten times.
            int i = 10;

            do {
                // Select an option at random.
                int optionRatio = random() % itemOptionRatio;
                int ratioSum = 0;

                // cout << "Ratio = " << optionRatio << "/" << itemOptionRatio << endl;

                OptionInfo* pOptionInfo = NULL;

                for (iOption = optionVector.begin(); iOption != optionVector.end(); iOption++) {
                    optionType = *iOption;

                    pOptionInfo = optionInfos.getOptionInfo(optionType);
                    ratioSum += pOptionInfo->getRatio();

                    if (optionRatio < ratioSum) {
                        // Select this option.
                        break;
                    }
                }

                // Add the option's required attributes and
                ReqLevel = ReqLevel2 + pOptionInfo->getReqLevel();

                // check whether the item matches the player's attributes.
                if (ReqLevel <= 0 || CLevel >= ReqLevel) {
                    // Settle on this option type.
                    break;
                }

            } while (--i);

            if (i == 0) {
                optionType = 0; //(rand()%3==0? 1: (rand()%2? 6:11));
            }
        }
        // No option. If nothing suitable is found, any of STR+1, DEX+1, INT+1.
        else
            optionType = 0; //(rand()%3==0? 1: (rand()%2? 6:11));
    }
    //----------------------------------------------------------------------
    // Ousters case
    //----------------------------------------------------------------------
    else if (pCreature->isOusters()) {
        Ousters* pOusters = dynamic_cast<Ousters*>(pCreature);

        Level_t CLevel = pOusters->getLevel();

        // Raise the limit that can be gambled a little.
        CLevel += 3;

        // Level limit
        if (maxLevel != 0) {
            CLevel = min((int)maxLevel, (int)CLevel);
        }

        Attr_t ReqLevel, ReqLevel2;

        // Try only ten times.
        int i = 10;
        do {
            itemType = pInfoClass->getRandomItemType();

            // Check whether the level allows creating this itemType.
            pItemInfo = de::gameContext().itemInfos().getItemInfo(itemClass, itemType);

            ReqLevel2 = ReqLevel = pItemInfo->getReqLevel();

            // The level limit must be absent or the level high enough.
            if ((ReqLevel <= 0 || CLevel >= ReqLevel)) {
                break;
            }

        } while (--i);

        if (i == 0) {
            itemType = 0;
        }

        // If the item carries options,
        // raise the attribute limit according to the kinds of option.
        int maxOptionLevel = max(1, min(100, (int)CLevel));

        const vector<OptionType_t>& optionVector =
            optionInfos.getPossibleGambleOptionVector((Item::ItemClass)itemClass, maxOptionLevel);
        vector<OptionType_t>::const_iterator iOption;

        // Get the total OptionRatio.
        int itemOptionRatio = optionInfos.getTotalGambleRatio((Item::ItemClass)itemClass, maxOptionLevel);


        if (optionVector.size() > 0 && itemOptionRatio > 0 && (pItemInfo == NULL || !pItemInfo->isUnique())) {
            // Try only ten times.
            int i = 10;

            do {
                // Select an option at random.
                int optionRatio = random() % itemOptionRatio;
                int ratioSum = 0;

                // cout << "Ratio = " << optionRatio << "/" << itemOptionRatio << endl;

                OptionInfo* pOptionInfo = NULL;

                for (iOption = optionVector.begin(); iOption != optionVector.end(); iOption++) {
                    optionType = *iOption;

                    pOptionInfo = optionInfos.getOptionInfo(optionType);
                    ratioSum += pOptionInfo->getRatio();

                    if (optionRatio < ratioSum) {
                        // Select this option.
                        break;
                    }
                }

                // Add the option's required attributes and
                ReqLevel = ReqLevel2 + pOptionInfo->getReqLevel();

                // check whether the item matches the player's attributes.
                if (ReqLevel <= 0 || CLevel >= ReqLevel) {
                    // Settle on this option type.
                    break;
                }

            } while (--i);

            if (i == 0) {
                optionType = 0; //(rand()%3==0? 1: (rand()%2? 6:11));
            }
        }
        // No option. If nothing suitable is found, any of STR+1, DEX+1, INT+1.
        else
            optionType = 0; //(rand()%3==0? 1: (rand()%2? 6:11));
    }

    // Create the item and hand it back.
    list<OptionType_t> optionTypes;
    if (optionType != 0)
        optionTypes.push_back(optionType);
    Item* pItem = de::gameContext().itemFactories().createItem(itemClass, itemType, optionTypes);

    pItem->setGrade(min(6, ItemGradeManager::Instance().getRandomGambleGrade()));

    return pItem;

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
// Is this an item that options can be attached to?
//////////////////////////////////////////////////////////////////////////////
bool isPossibleOptionItemClass(Item::ItemClass IClass) {
    switch (IClass) {
    case Item::ITEM_CLASS_HELM:
    case Item::ITEM_CLASS_NECKLACE:
    case Item::ITEM_CLASS_RING:
    case Item::ITEM_CLASS_BRACELET:
    case Item::ITEM_CLASS_SHIELD:
    case Item::ITEM_CLASS_GLOVE:
    case Item::ITEM_CLASS_COAT:
    case Item::ITEM_CLASS_BELT:
    case Item::ITEM_CLASS_TROUSER:
    case Item::ITEM_CLASS_SHOES:
    case Item::ITEM_CLASS_SWORD:
    case Item::ITEM_CLASS_BLADE:
    case Item::ITEM_CLASS_CROSS:
    case Item::ITEM_CLASS_MACE:
    case Item::ITEM_CLASS_AR:
    case Item::ITEM_CLASS_SR:
    case Item::ITEM_CLASS_SG:
    case Item::ITEM_CLASS_SMG:
    case Item::ITEM_CLASS_CARRYING_RECEIVER:
    case Item::ITEM_CLASS_SHOULDER_ARMOR:

    case Item::ITEM_CLASS_VAMPIRE_NECKLACE:
    case Item::ITEM_CLASS_VAMPIRE_RING:
    case Item::ITEM_CLASS_VAMPIRE_BRACELET:
    case Item::ITEM_CLASS_VAMPIRE_EARRING:
    case Item::ITEM_CLASS_VAMPIRE_COAT:
    case Item::ITEM_CLASS_VAMPIRE_WEAPON:
    case Item::ITEM_CLASS_VAMPIRE_AMULET:
    case Item::ITEM_CLASS_DERMIS:
    case Item::ITEM_CLASS_PERSONA:

    case Item::ITEM_CLASS_OUSTERS_ARMSBAND:
    case Item::ITEM_CLASS_OUSTERS_BOOTS:
    case Item::ITEM_CLASS_OUSTERS_CHAKRAM:
    case Item::ITEM_CLASS_OUSTERS_CIRCLET:
    case Item::ITEM_CLASS_OUSTERS_COAT:
    case Item::ITEM_CLASS_OUSTERS_PENDENT:
    case Item::ITEM_CLASS_OUSTERS_RING:
    case Item::ITEM_CLASS_OUSTERS_STONE:
    case Item::ITEM_CLASS_OUSTERS_WRISTLET:
    case Item::ITEM_CLASS_FASCIA:
    case Item::ITEM_CLASS_MITTEN:

    case Item::ITEM_CLASS_CORE_ZAP:

        return true;

    default:
        return false;
    }

    return false;
}

//////////////////////////////////////////////////////////////////////////////
// Is this an ItemType that can be upgraded to the next step?
//////////////////////////////////////////////////////////////////////////////
bool isPossibleUpgradeItemType(Item::ItemClass IClass) {
    switch (IClass) {
    case Item::ITEM_CLASS_HELM:
    case Item::ITEM_CLASS_NECKLACE:
    case Item::ITEM_CLASS_RING:
    case Item::ITEM_CLASS_BRACELET:
    case Item::ITEM_CLASS_SHIELD:
    case Item::ITEM_CLASS_GLOVE:
    case Item::ITEM_CLASS_COAT:
    case Item::ITEM_CLASS_BELT:
    case Item::ITEM_CLASS_TROUSER:
    case Item::ITEM_CLASS_SHOES:
    case Item::ITEM_CLASS_SWORD:
    case Item::ITEM_CLASS_BLADE:
    case Item::ITEM_CLASS_CROSS:
    case Item::ITEM_CLASS_MACE:
    case Item::ITEM_CLASS_AR:
    case Item::ITEM_CLASS_SR:
    case Item::ITEM_CLASS_SG:
    case Item::ITEM_CLASS_SMG:

    case Item::ITEM_CLASS_VAMPIRE_NECKLACE:
    case Item::ITEM_CLASS_VAMPIRE_RING:
    case Item::ITEM_CLASS_VAMPIRE_BRACELET:
    case Item::ITEM_CLASS_VAMPIRE_EARRING:
    case Item::ITEM_CLASS_VAMPIRE_COAT:
    case Item::ITEM_CLASS_VAMPIRE_WEAPON:
    case Item::ITEM_CLASS_VAMPIRE_AMULET:

    case Item::ITEM_CLASS_OUSTERS_ARMSBAND:
    case Item::ITEM_CLASS_OUSTERS_BOOTS:
    case Item::ITEM_CLASS_OUSTERS_CHAKRAM:
    case Item::ITEM_CLASS_OUSTERS_CIRCLET:
    case Item::ITEM_CLASS_OUSTERS_COAT:
    case Item::ITEM_CLASS_OUSTERS_PENDENT:
    case Item::ITEM_CLASS_OUSTERS_RING:
    case Item::ITEM_CLASS_OUSTERS_STONE:
    case Item::ITEM_CLASS_OUSTERS_WRISTLET:

        return true;

    default:
        return false;
    }

    return false;
}

ItemType_t getUpgradeItemType(Item::ItemClass IClass, ItemType_t itemType, ItemType_t upgradeCount) {
    if (upgradeCount == 0)
        return itemType;

    InfoClassManager* pInfoClass = de::gameContext().itemInfos().getInfoManager(IClass);
    Assert(pInfoClass != NULL);

    // Item upgrade information goes into the DB. Here it advances to the next ItemType the given number of times.
    ItemType_t newItemType = itemType;

    for (int i = 0; i < upgradeCount; i++) {
        ItemInfo* pItemInfo = pInfoClass->getItemInfo(newItemType);
        Assert(pItemInfo != NULL);

        newItemType = pItemInfo->getNextItemType();
    }

    // cout << "ItemType Upgrade By Luck: " << itemType << " --[+" << upgradeCount << "]--> ";

    return newItemType;
}

ItemType_t getDowngradeItemType(Item::ItemClass IClass, ItemType_t itemType) {
    InfoClassManager* pInfoClass = de::gameContext().itemInfos().getInfoManager(IClass);
    Assert(pInfoClass != NULL);

    for (int i = 0; i < pInfoClass->getInfoCount(); ++i) {
        ItemInfo* pItemInfo = pInfoClass->getItemInfo(i);
        Assert(pItemInfo != NULL);

        if (pItemInfo->getNextItemType() == itemType)
            return i;
    }

    return itemType;
}

// Generalized version; checkEventTree, checkEventDocument and checkEventDoll can be removed.
TPOINT checkEventPuzzle(PlayerCreature* pPC, CoordInven_t iX, CoordInven_t iY, int start) {
    __BEGIN_TRY

    Assert(pPC != NULL);

    TPOINT pt;
    pt.x = -1;
    pt.y = -1;

    Inventory* pInventory = pPC->getInventory();
    Item* pCurItem = pInventory->getItem(iX, iY);

    if (pCurItem == NULL)
        return pt;

    ItemType_t itemType = pCurItem->getItemType();

    itemType = itemType - start;

    CoordInven_t startX = iX - itemType % 3;
    CoordInven_t startY = iY - itemType / 3;

    if (pInventory->getWidth() - 3 < startX)
        return pt;
    if (pInventory->getHeight() - 4 < startY)
        return pt;

    CoordInven_t curIX = 0, curIY = 0;

    ItemType_t compType = start;

    for (curIY = startY; curIY < startY + 4; curIY++) {
        for (curIX = startX; curIX < startX + 3; curIX++) {
            pCurItem = pInventory->getItem(curIX, curIY);
            if (pCurItem == NULL)
                return pt;

            if (pCurItem->getItemClass() != Item::ITEM_CLASS_EVENT_TREE || pCurItem->getItemType() != compType)
                return pt;

            compType++;
        }
    }

    pt.x = startX;
    pt.y = startY;

    return pt;

    __END_CATCH
}

// Delete the items in the inventory range (X0, Y0) - (X1, y1).
void deleteInventoryItem(Inventory* pInventory, CoordInven_t invenX0, CoordInven_t invenY0, CoordInven_t invenX1,
                         CoordInven_t invenY1) {
    __BEGIN_TRY

    CoordInven_t curIX = 0, curIY = 0;
    Item* pCurItem = 0;

    // Delete the assembled tree fragments.
    for (curIY = invenY0; curIY <= invenY1; curIY++) {
        for (curIX = invenX0; curIX <= invenX1; curIX++) {
            pCurItem = pInventory->getItem(curIX, curIY);

            if (pCurItem != NULL) {
                // Delete it from the inventory.
                pInventory->deleteItem(pCurItem->getObjectID());

                // Remove it from the DB.
                pCurItem->destroy();

                SAFE_DELETE(pCurItem);
            }
        }
    }

    __END_CATCH
}

typedef struct {
    Item::ItemClass itemClass;
    ItemType_t itemType;
    CoordInven_t x;
    CoordInven_t y;
    ItemNum_t num;
} NewbieItem;

const int maxNewbieItemNum = 8;

const NewbieItem NewbieItems[maxNewbieItemNum] = {
    {Item::ITEM_CLASS_SWORD, 0, 4, 3, 1},  {Item::ITEM_CLASS_BLADE, 0, 2, 3, 1},
    {Item::ITEM_CLASS_CROSS, 0, 0, 3, 1},  {Item::ITEM_CLASS_MACE, 0, 0, 0, 1},
    {Item::ITEM_CLASS_AR, 0, 2, 0, 1},     {Item::ITEM_CLASS_MAGAZINE, 2, 4, 0, 20},
    {Item::ITEM_CLASS_POTION, 0, 9, 4, 9}, {Item::ITEM_CLASS_POTION, 5, 9, 5, 9},
};

// Put the newbie items into the inventory.
bool addNewbieItemToInventory(Slayer* pSlayer, bool sendPacket)

{
    __BEGIN_TRY

    if (sendPacket) {
        filelog("NewbieItemError.log", "Someone request packet to addNewbieItemToInventory() function!");
        return false;
    }

    Zone* pZone = pSlayer->getZone();
    Inventory* pInventory = pSlayer->getInventory();
    ObjectRegistry& objectRegister = pZone->getObjectRegistry();

    if (pInventory == NULL)
        return false;
    if (pZone == NULL)
        return false;

    if (pInventory->getItemNum() != 0)
        return false;

    list<OptionType_t> olist;
    GCCreateItem gcCreateItem;

    Item::ItemClass bestWeapon = getBestNewbieWeaponClass(pSlayer);

    for (int i = 0; i < maxNewbieItemNum; i++) {
        Item* pItem =
            de::gameContext().itemFactories().createItem(NewbieItems[i].itemClass, NewbieItems[i].itemType, olist);
        pItem->setCreateType(Item::CREATE_TYPE_GAME);
        objectRegister.registerObject(pItem);

        pItem->setNum(NewbieItems[i].num);

        bool weared = false;

        if (pItem->getItemClass() == bestWeapon) {
            if (!pSlayer->isWear(Slayer::WEAR_RIGHTHAND)) {
                pSlayer->wearItem(Slayer::WEAR_RIGHTHAND, pItem);
                pItem->create(pSlayer->getName(), STORAGE_GEAR, 0, Slayer::WEAR_RIGHTHAND, 0, pItem->getItemID());
                weared = true;
            }
        }
        if (!weared)
            if (pInventory->addItem(NewbieItems[i].x, NewbieItems[i].y, pItem)) {
                pItem->create(pSlayer->getName(), STORAGE_INVENTORY, 0, NewbieItems[i].x, NewbieItems[i].y,
                              pItem->getItemID());

                if (sendPacket) {
                }
            }
    }

    return true;

    __END_CATCH
}

bool addNewbieGoldToInventory(Slayer* pSlayer, bool sendPacket)

{
    __BEGIN_TRY

    if (sendPacket) {
        filelog("NewbieItemError.log", "Someone request packet to addNewbieGoldToInventory() function!");
        return false;
    }

    if (pSlayer == NULL)
        return false;
    if (pSlayer->getGold() != 0)
        return false;

    pSlayer->setGoldEx(500);

    return true;

    __END_CATCH
}

bool addNewbieItemToGear(Slayer* pSlayer, bool sendPacket)

{
    __BEGIN_TRY

    if (sendPacket) {
        filelog("NewbieItemError.log", "Someone request packet to addNewbieItemToGear() function!");
        return false;
    }

    if (pSlayer == NULL)
        return false;

    Zone* pZone = pSlayer->getZone();
    ObjectRegistry& objectRegister = pZone->getObjectRegistry();

    if (pZone == NULL)
        return false;

    list<OptionType_t> olist;

    if (!pSlayer->isWear(Slayer::WEAR_BODY) && !pSlayer->isWear(Slayer::WEAR_LEG)) {
        Item* pCoat = de::gameContext().itemFactories().createItem(Item::ITEM_CLASS_COAT,
                                                                   ((pSlayer->getSex() == MALE) ? 0 : 1), olist);
        Item* pTrouser = de::gameContext().itemFactories().createItem(Item::ITEM_CLASS_TROUSER,
                                                                      ((pSlayer->getSex() == MALE) ? 0 : 1), olist);
        pCoat->setCreateType(Item::CREATE_TYPE_GAME);
        pTrouser->setCreateType(Item::CREATE_TYPE_GAME);

        objectRegister.registerObject(pCoat);
        objectRegister.registerObject(pTrouser);

        pCoat->create(pSlayer->getName(), STORAGE_GEAR, 0, Slayer::WEAR_BODY, 0, pCoat->getItemID());
        pTrouser->create(pSlayer->getName(), STORAGE_GEAR, 0, Slayer::WEAR_LEG, 0, pTrouser->getItemID());

        pSlayer->wearItem(Slayer::WEAR_BODY, pCoat);
        pSlayer->wearItem(Slayer::WEAR_LEG, pTrouser);
    }

    return true;

    __END_CATCH
}

bool addNewbieGoldToInventory(Ousters* pOusters, bool sendPacket /*= false*/) {
    __BEGIN_TRY

    if (sendPacket) {
        filelog("NewbieItemError.log", "Someone request packet to addNewbieItemToInventory() function!");
        return false;
    }

    if (pOusters == NULL)
        return false;
    if (pOusters->getGold() != 0)
        return false;

    pOusters->setGoldEx(500);

    return true;

    __END_CATCH
}

bool addNewbieItemToInventory(Ousters* pOusters, bool sendPacket /*= false */) {
    __BEGIN_TRY

    if (sendPacket) {
        filelog("NewbieItemError.log", "Someone request packet to addNewbieGoldToInventory() function!");
        return false;
    }

    if (pOusters == NULL)
        return false;

    Zone* pZone = pOusters->getZone();
    if (pZone == NULL)
        return false;

    Inventory* pInventory = pOusters->getInventory();
    if (pInventory == NULL)
        return false;

    if (pInventory->getItemNum() != 0)
        return false;

    Item* pPupa = de::gameContext().itemFactories().createItem(Item::ITEM_CLASS_PUPA, 0, list<OptionType_t>());
    Item* pLarva = de::gameContext().itemFactories().createItem(Item::ITEM_CLASS_LARVA, 0, list<OptionType_t>());

    Assert(pPupa != NULL);
    Assert(pLarva != NULL);

    pPupa->setNum(9);
    pLarva->setNum(9);
    pPupa->setCreateType(Item::CREATE_TYPE_GAME);
    pLarva->setCreateType(Item::CREATE_TYPE_GAME);

    pZone->registerObject(pPupa);
    pZone->registerObject(pLarva);

    _TPOINT tp;
    pInventory->addItem(pPupa, tp);
    pPupa->create(pOusters->getName(), STORAGE_INVENTORY, 0, tp.x, tp.y, pPupa->getItemID());
    pInventory->addItem(pLarva, tp);
    pLarva->create(pOusters->getName(), STORAGE_INVENTORY, 0, tp.x, tp.y, pLarva->getItemID());

    return true;

    __END_CATCH
}

bool addNewbieItemToGear(Ousters* pOusters, bool sendPacket /*= false */) {
    __BEGIN_TRY

    if (sendPacket) {
        filelog("NewbieItemError.log", "Someone request packet to addNewbieItemToInventory() function!");
        return false;
    }

    if (pOusters == NULL)
        return false;
    Zone* pZone = pOusters->getZone();
    Assert(pZone != NULL);

    if (pOusters->getWearItem(Ousters::WEAR_RIGHTHAND) != NULL)
        return false;

    Item* pWeapon =
        de::gameContext().itemFactories().createItem(Item::ITEM_CLASS_OUSTERS_CHAKRAM, 0, list<OptionType_t>());
    Assert(pWeapon != NULL);

    pWeapon->setCreateType(Item::CREATE_TYPE_GAME);
    pZone->registerObject(pWeapon);

    pOusters->wearItem(Ousters::WEAR_RIGHTHAND, pWeapon);
    pWeapon->create(pOusters->getName(), STORAGE_GEAR, 0, Ousters::WEAR_RIGHTHAND, 0, pWeapon->getItemID());

    return true;

    __END_CATCH
}

Item::ItemClass getBestNewbieWeaponClass(Slayer* pSlayer)

{
    __BEGIN_TRY

    Assert(pSlayer != NULL);

    Attr_t STR = pSlayer->getSTR();
    Attr_t DEX = pSlayer->getDEX();
    Attr_t INT = pSlayer->getINT();

    // STR
    if (STR >= DEX && STR >= INT) {
        return (rand() % 2 ? Item::ITEM_CLASS_SWORD : Item::ITEM_CLASS_BLADE);
    }
    // DEX
    else if (DEX >= STR && DEX >= INT) {
        return Item::ITEM_CLASS_AR;
    }

    // INT
    return (rand() % 2 ? Item::ITEM_CLASS_CROSS : Item::ITEM_CLASS_MACE);


    __END_CATCH
}

void makeOptionList(const string& options, list<OptionType_t>& optionList)

{
    size_t a = 0, b = 0;

    //////////////////////////////////////////////
    // DEX+1,INT+2
    // a     ba     b
    //////////////////////////////////////////////
    optionList.clear();
    if (options.size() <= 1)
        return;

    do {
        b = options.find_first_of(',', a);

        string optionName = trim(options.substr(a, b - a));

        OptionType_t optionType;

        try {
            optionType = de::gameContext().optionInfos().getOptionType(optionName);
        } catch (NoSuchElementException&) {
            throw Error("No such option.");
        }

        optionList.push_back(optionType);

        a = b + 1;

    } while (b != string::npos && b < options.size() - 1);
}

void saveDissectionItem(Creature* pCreature, Item* pTreasure, int x, int y)

{
    __BEGIN_TRY

    Assert(pCreature != NULL);
    Assert(pTreasure != NULL);

    // Create the item in the DB only when it is not a relic.
    // A Blood Bible is already in the DB, so the DB is updated instead.
    switch (pTreasure->getItemClass()) {
    case Item::ITEM_CLASS_RELIC: {
        // A Relic does not need to be stored in the DB.
    } break;

    case Item::ITEM_CLASS_BLOOD_BIBLE:
    case Item::ITEM_CLASS_CASTLE_SYMBOL:
    case Item::ITEM_CLASS_SWEEPER: {
        char query[128];

        snprintf(query, sizeof(query), "ObjectID = %u, Storage=%u, StorageID=%u, X=%u, Y=%u", pTreasure->getObjectID(),
                 STORAGE_ZONE, pCreature->getZone()->getZoneID(), x, y);
        pTreasure->tinysave(query);
    } break;

    default: {
        ItemInfo* pItemInfo =
            de::gameContext().itemInfos().getItemInfo(pTreasure->getItemClass(), pTreasure->getItemType());
        Assert(pItemInfo != NULL);

        // For a unique item,
        // mark the item itself as unique and
        // change the unique item count in the DB.
        //
        // (*) When a monster dies,
        //     items inside the monster are neither marked unique
        //     nor counted in the DB. So they are ignored on deletion too.
        //     At present unique items appear only through monsters,
        //     are created (!) through CGDissectionCorpse and
        //     are removed only through EffectDecayItem.

        if (pItemInfo->isUnique()) {
            pTreasure->setUnique();
            UniqueItemManager::createItem(pTreasure->getItemClass(), pTreasure->getItemType());
            filelog("uniqueItem.txt", "[CGDissectionCorpse] %s %s", pCreature->getName().c_str(),
                    pTreasure->toString().c_str());
        }

        // Keep the existing ItemID.
        // If the ItemID is 0, a new ItemID is assigned on create().
        // by sigi. 2002.10.28
        pTreasure->create("", STORAGE_ZONE, pCreature->getZone()->getZoneID(), x, y, pTreasure->getItemID());
    }
    }

    __END_CATCH
}

bool canDecreaseDurability(Item* pItem)

{
    __BEGIN_TRY

    if (pItem == NULL)
        return false;
    if (pItem->isUnique())
        return false;
    if (pItem->isTimeLimitItem())
        return false;
    if (pItem->isQuestItem())
        return false;
    if (pItem->isFlagItem())
        return false;

    switch (pItem->getItemClass()) {
    case Item::ITEM_CLASS_VAMPIRE_AMULET:
    case Item::ITEM_CLASS_COUPLE_RING:
    case Item::ITEM_CLASS_VAMPIRE_COUPLE_RING:
    case Item::ITEM_CLASS_SWEEPER:
    case Item::ITEM_CLASS_CORE_ZAP:
    case Item::ITEM_CLASS_CARRYING_RECEIVER:
    case Item::ITEM_CLASS_DERMIS:
    case Item::ITEM_CLASS_FASCIA:
        return false;
    default:
        break;
    }
    return true;

    __END_CATCH
}

bool canSell(Item* pItem) {
    if (pItem == NULL)
        return false;
    if (pItem->isTimeLimitItem())
        return true;

    if (pItem->isUnique())
        return false;
    if (isCoupleRing(pItem))
        return false;


    if (pItem->isQuestItem())
        return false;
    if (pItem->isFlagItem())
        return false;
    // if ( pItem->isTimeLimitItem() ) return false;

    Item::ItemClass itemClass = pItem->getItemClass();

    if (itemClass == Item::ITEM_CLASS_MOTORCYCLE || itemClass == Item::ITEM_CLASS_KEY ||
        itemClass == Item::ITEM_CLASS_CORPSE || itemClass == Item::ITEM_CLASS_MONEY ||
        itemClass == Item::ITEM_CLASS_PET_ITEM
        //          || itemClass==Item::ITEM_CLASS_EVENT_GIFT_BOX
        || isRelicItem(itemClass))
        return false;
    if (itemClass == Item::ITEM_CLASS_SWEEPER)
        return false;

    // The quest item Life Spiral is sellable.
    if (itemClass == Item::ITEM_CLASS_EVENT_ITEM && pItem->getItemType() == 31)
        return true;

    if (itemClass == Item::ITEM_CLASS_EVENT_ITEM && pItem->getItemType() != 28)
        return false;

    return true;
}

bool canPutInStash(Item* pItem) {
    if (pItem == NULL)
        return false;
    if (pItem->isUnique())
        return false;
    if (pItem->isTimeLimitItem())
        return false;
    if (isRelicItem(pItem))
        return false;
    if (isCoupleRing(pItem))
        return false;
    if (pItem->isQuestItem())
        return false;
    if (pItem->isFlagItem())
        return false;

    Item::ItemClass itemClass = pItem->getItemClass();

    if (itemClass == Item::ITEM_CLASS_EVENT_ITEM)
        return false;
    if (itemClass == Item::ITEM_CLASS_SWEEPER)
        return false;

    if (itemClass == Item::ITEM_CLASS_LUCKY_BAG && pItem->getItemType() == 3)
        return false;
    if (itemClass == Item::ITEM_CLASS_EVENT_ITEM && pItem->getItemType() == 28)
        return false;

    return true;
}

bool canTrade(Item* pItem) {
    if (pItem == NULL)
        return false;
    if (isRelicItem(pItem))
        return false;
    if (isCoupleRing(pItem))
        return false;
    if (pItem->isTimeLimitItem())
        return false;
    if (pItem->isQuestItem())
        return false;
    if (pItem->isFlagItem())
        return false;

    Item::ItemClass itemClass = pItem->getItemClass();
    ItemType_t itemType = pItem->getItemType();

    if (itemClass == Item::ITEM_CLASS_EVENT_ITEM)
        return false;
    if (itemClass == Item::ITEM_CLASS_SWEEPER)
        return false;

    // The half moon card cannot be traded.
    if (itemClass == Item::ITEM_CLASS_MOON_CARD && pItem->getItemType() == 0)
        return false;

    // Premium trial ticket fragment
    if (itemClass == Item::ITEM_CLASS_LUCKY_BAG && pItem->getItemType() == 3)
        return false;
    if (itemClass == Item::ITEM_CLASS_EVENT_ITEM && pItem->getItemType() == 28)
        return false;
    if (itemClass == Item::ITEM_CLASS_EFFECT_ITEM && pItem->getItemType() >= 4 && pItem->getItemType() <= 6)
        return false;

    // Event rice cake soup cannot be exchanged.
    if (itemClass == Item::ITEM_CLASS_EVENT_STAR && (itemType >= 17 && itemType <= 21))
        return false;

    // The lucky strainer item cannot be exchanged.
    if (itemClass == Item::ITEM_CLASS_MIXING_ITEM && itemType == 18)
        return false;

    // Exchange System: Point-only items cannot be P2P traded
    if (isPointOnlyTradeItem(pItem))
        return false;

    return true;
}
bool isCoupleRing(Item* pItem) {
    switch (pItem->getItemClass()) {
    case Item::ITEM_CLASS_COUPLE_RING:
    case Item::ITEM_CLASS_VAMPIRE_COUPLE_RING:
        return true;
    default:
        return false;
    }
}

bool suitableItemClass(Item::ItemClass iClass, SkillDomainType_t domainType) {
    switch (domainType) {
    case SKILL_DOMAIN_BLADE:
        if (iClass == Item::ITEM_CLASS_BLADE)
            return true;
        return false;

    case SKILL_DOMAIN_SWORD:
        if (iClass == Item::ITEM_CLASS_SWORD)
            return true;
        return false;

    case SKILL_DOMAIN_GUN:
        if (iClass == Item::ITEM_CLASS_AR || iClass == Item::ITEM_CLASS_SR || iClass == Item::ITEM_CLASS_SMG ||
            iClass == Item::ITEM_CLASS_SG)
            return true;
        return false;

    case SKILL_DOMAIN_HEAL:
        if (iClass == Item::ITEM_CLASS_CROSS)
            return true;
        return false;

    case SKILL_DOMAIN_ENCHANT:
        if (iClass == Item::ITEM_CLASS_MACE)
            return true;
        return false;

    case SKILL_DOMAIN_ETC:
        return false;

    case SKILL_DOMAIN_VAMPIRE:
        if (iClass == Item::ITEM_CLASS_VAMPIRE_WEAPON)
            return true;
        return false;

    default:
        return false;
    }

    Assert(false);
}

void setItemGender(Item* pItem, GenderRestriction gender) {
    if (gender == GENDER_BOTH || gender == GENDER_MAX)
        return;

    ItemInfo* pItemInfo = de::gameContext().itemInfos().getItemInfo(pItem->getItemClass(), pItem->getItemType());
    if (pItemInfo->getReqGender() == gender)
        return;
    if (pItemInfo->getReqGender() == GENDER_BOTH || pItemInfo->getReqGender() == GENDER_MAX)
        return;

    int genderDiff = (int)gender - (int)pItemInfo->getReqGender();
    Assert(genderDiff == 1 || genderDiff == -1);

    pItem->setItemType(pItem->getItemType() + genderDiff);
}

bool bTraceLog(Item* pItem) {
    Item::ItemClass iClass = pItem->getItemClass();

    // A PetItem always leaves a trace log.
    if (iClass == Item::ITEM_CLASS_PET_ITEM || iClass == Item::ITEM_CLASS_CORE_ZAP)
        return true;

    // The blue candy potion and the white rice cake soup leave a trace log.
    if (iClass == Item::ITEM_CLASS_POTION && (pItem->getItemType() == 10 || pItem->getItemType() == 11))
        return true;

    // The red candy Serum leaves a trace log.
    if (iClass == Item::ITEM_CLASS_SERUM && (pItem->getItemType() == 4 || pItem->getItemType() == 5))
        return true;

    switch (iClass) {
    case Item::ITEM_CLASS_CARRYING_RECEIVER:
    case Item::ITEM_CLASS_SHOULDER_ARMOR:
    case Item::ITEM_CLASS_DERMIS:
    case Item::ITEM_CLASS_PERSONA:
    case Item::ITEM_CLASS_FASCIA:
    case Item::ITEM_CLASS_MITTEN:
        return true;
        break;
    default:
        break;
    }

    if (iClass == Item::ITEM_CLASS_MOTORCYCLE || iClass == Item::ITEM_CLASS_POTION ||
        iClass == Item::ITEM_CLASS_WATER || iClass == Item::ITEM_CLASS_HOLYWATER || iClass == Item::ITEM_CLASS_ETC ||
        iClass == Item::ITEM_CLASS_KEY || iClass == Item::ITEM_CLASS_MAGAZINE ||
        iClass == Item::ITEM_CLASS_BOMB_MATERIAL || iClass == Item::ITEM_CLASS_BOMB ||
        iClass == Item::ITEM_CLASS_MINE || iClass == Item::ITEM_CLASS_LEARNINGITEM ||
        iClass == Item::ITEM_CLASS_MONEY || iClass == Item::ITEM_CLASS_CORPSE || iClass == Item::ITEM_CLASS_SKULL ||
        iClass == Item::ITEM_CLASS_SERUM || iClass == Item::ITEM_CLASS_VAMPIRE_ETC ||
        iClass == Item::ITEM_CLASS_SLAYER_PORTAL_ITEM || iClass == Item::ITEM_CLASS_VAMPIRE_PORTAL_ITEM ||
        iClass == Item::ITEM_CLASS_RELIC || iClass == Item::ITEM_CLASS_BLOOD_BIBLE ||
        iClass == Item::ITEM_CLASS_CASTLE_SYMBOL || iClass == Item::ITEM_CLASS_DYE_POTION ||
        iClass == Item::ITEM_CLASS_RESURRECT_ITEM || iClass == Item::ITEM_CLASS_SWEEPER || pItem->isTimeLimitItem())
        return false;

    const list<OptionType_t>& optionList = pItem->getOptionTypeList();
    list<OptionType_t>::const_iterator itr;

    // Items with a resistance option leave a trace log.
    for (itr = optionList.begin(); itr != optionList.end(); itr++) {
        OptionInfo* pOptionInfo = de::gameContext().optionInfos().getOptionInfo(*itr);
        if (pOptionInfo == NULL)
            return false;

        OptionClass optionClass = pOptionInfo->getClass();

        if (optionClass == OPTION_POISON || optionClass == OPTION_ACID || optionClass == OPTION_CURSE ||
            optionClass == OPTION_BLOOD)
            return true;
    }

    // Bijou and pendant leave a trace log.
    // Event Star leaves a trace log.
    if (iClass == Item::ITEM_CLASS_QUEST_ITEM || iClass == Item::ITEM_CLASS_EVENT_STAR ||
        iClass == Item::ITEM_CLASS_MIXING_ITEM)
        return true;

    // Any other item at step 3 or below leaves no trace log.
    if ((int)(pItem->getItemType()) < 3)
        return false;

    return true;
}

void remainTraceLog(Item* pItem, const string& preOwner, const string& owner, ItemTraceLogType logType,
                    ItemTraceDetailType detailType)

{
    __BEGIN_TRY

    Assert(pItem != NULL);

    ItemTraceRecord record;
    record.itemID = pItem->getItemID();
    record.itemClass = ItemClass2ShortString[(int)(pItem->getItemClass())];
    record.itemType = pItem->getItemType();
    record.optionName = getOptionTypeToString(pItem->getOptionTypeList());
    record.preOwner = preOwner;
    record.owner = owner;
    record.logType = ItemTraceLogType2String[(int)logType];
    record.detailType = ItemTraceLogDetailType2String[(int)detailType];
    defaultItemRepository().insertItemTraceLog(record);

    __END_CATCH
}

void remainTraceLogNew(Item* pItem, const string& owner, ITLType logType, ITLDType detailType, ZoneID_t zid /*=0*/,
                       int x /*=0*/, int y /*=0 */)

{
    __BEGIN_TRY

    Assert(pItem != NULL);

    // The ItemTrace2Log INSERT that lived here was commented out long ago;
    // the dead block is gone so it no longer reads as inline SQL.
    __END_CATCH
}

void remainMoneyTraceLog(const string& preOwner, const string& owner, ItemTraceLogType logType,
                         ItemTraceDetailType detailType, int amount)

{
    __BEGIN_TRY

    defaultItemRepository().insertMoneyTraceLog(preOwner, owner, ItemTraceLogType2String[(int)logType],
                                                ItemTraceLogDetailType2String[(int)detailType], amount);

    __END_CATCH
}

// Creates an item bought on the web.
Item* createItemByGoodsID(DWORD goodsID) {
    GoodsInfo* pGoodsInfo = de::gameContext().goodsInfos().getGoodsInfo(goodsID);
    if (pGoodsInfo == NULL) {
        filelog("buyItemBug.txt", "buyID(%d) : no goods match it.", (int)goodsID);
        return NULL;
    }

    Item::ItemClass ItemClass = pGoodsInfo->getItemClass();
    ItemType_t ItemType = pGoodsInfo->getItemType();
    Grade_t Grade = pGoodsInfo->getGrade();
    list<OptionType_t> optionTypeList = pGoodsInfo->getOptionTypeList();
    int Num = pGoodsInfo->getNum();
    bool bTimeLimit = pGoodsInfo->isTimeLimit();
    int Hour = pGoodsInfo->getHour();

    if (!de::gameContext().itemInfos().isPossibleItem(ItemClass, ItemType, optionTypeList)) {
        filelog("buyItemBug.txt", "buyID(%d) : its item cannot be created.", (int)goodsID);
        return NULL;
    }

    Item* pItem = de::gameContext().itemFactories().createItem(ItemClass, ItemType, optionTypeList);
    if (pItem == NULL) {
        filelog("buyItemBug.txt", "buyID(%d) : failed to create its item.", (int)goodsID);
        return NULL;
    }

    pItem->setGrade(Grade);
    pItem->setNum(Num);
    pItem->setTimeLimitItem(bTimeLimit);
    pItem->setHour(Hour);
    pItem->setCreateType(Item::CREATE_TYPE_MALL);

    if (ItemClass == Item::ITEM_CLASS_PET_ITEM) {
        PetItem* pPetItem = dynamic_cast<PetItem*>(pItem);
        Assert(pPetItem != NULL);

        PetType_t petType = ItemType;
        PetTypeInfo* pPetTypeInfo = PetTypeInfoManager::getInstance()->getPetTypeInfo(petType);
        PetExpInfo* pPetExpInfo = PetExpInfoManager::Instance().getPetExpInfo(48); // modify by viva for PetInfo
        if (pPetTypeInfo == NULL || pPetExpInfo == NULL) {
            filelog("buyItemBug.txt", "buyID(%d) : its pet item info is invalid.", (int)goodsID);
            SAFE_DELETE(pItem);
            return NULL;
        }

        PetInfo* pPetInfo = new PetInfo;

        pPetInfo->setPetType(petType);
        pPetInfo->setPetLevel(49);                                          // modify by viva for PetInfo
        pPetInfo->setPetCreatureType(pPetTypeInfo->getPetCreatureType(49)); // modify by viva for PetInfo
        pPetInfo->setPetAttr(23);                                           // modify by viva for PetInfo
        pPetInfo->setPetExp(pPetExpInfo->getPetGoalExp());
        // pPetInfo->setPetExp(1999998000);
        pPetInfo->setPetAttrLevel(11); // modify by viva for PetInfo
        pPetInfo->setFoodType(0);
        pPetInfo->setGamble(1);  // modify by viva for PetInfo
        pPetInfo->setCutHead(0); // modify by viva for PetInfo
        pPetInfo->setAttack(1);  // modify by viva for PetInfo
        pPetInfo->setPetHP(5760);
        pPetInfo->setFeedTime(VSDateTime::currentDateTime());

        // Two-way link
        pPetItem->setPetInfo(pPetInfo);
        pPetInfo->setPetItem(pPetItem);
    }

    return pItem;
}

bool bWinPrize(DWORD rewardID, DWORD questLevel) {
    bool Lotto = false;

    if (defaultItemRepository().takeEventQuestReward(rewardID, questLevel))
        Lotto = true;

    return Lotto;
}

void deleteFlagEffect(Corpse* pFlagPole, Item* pFlag) {
    if (pFlag == NULL)
        return;
    if (!pFlag->isFlagItem())
        return;

    if (!pFlagPole->isFlag(Effect::EFFECT_CLASS_FLAG_INSERT))
        return;
    Effect* pEffect = pFlagPole->getEffectManager().findEffect(Effect::EFFECT_CLASS_FLAG_INSERT);
    if (pEffect != NULL)
        pEffect->setDeadline(0);
}

void countResurrectItem() {
    __BEGIN_TRY
    defaultItemRepository().incrementResurrectItemCount();
    __END_CATCH
}

Item* fitToPC(Item* pItem, PlayerCreature* pPC) {
    if (pItem == NULL || pPC == NULL)
        return pItem;

    if (isSlayerWeapon(pItem->getItemClass()) && pPC->isSlayer()) {
        Item::ItemClass targetClass = Item::ITEM_CLASS_MAX;
        Slayer* pSlayer = dynamic_cast<Slayer*>(pPC);
        int add = 0;
        switch (pSlayer->getHighestSkillDomain()) {
        case SKILL_DOMAIN_BLADE: {
            targetClass = Item::ITEM_CLASS_BLADE;
        } break;
        case SKILL_DOMAIN_SWORD: {
            targetClass = Item::ITEM_CLASS_SWORD;
        } break;
        case SKILL_DOMAIN_GUN: {
            targetClass = Item::ITEM_CLASS_AR;
        } break;
        case SKILL_DOMAIN_HEAL: {
            targetClass = Item::ITEM_CLASS_CROSS;
            add = -2;
        } break;
        case SKILL_DOMAIN_ENCHANT: {
            targetClass = Item::ITEM_CLASS_MACE;
            add = -2;
        } break;
        default:
            break;
        }

        if (targetClass != Item::ITEM_CLASS_MAX && targetClass != pItem->getItemClass()) {
            Item* pNewItem = de::gameContext().itemFactories().createItem(targetClass, pItem->getItemType() + add,
                                                                          pItem->getOptionTypeList());
            if (pNewItem != NULL) {
                SAFE_DELETE(pItem);
                pItem = pNewItem;
            }
        }
    }

    if (isOustersWeapon(pItem->getItemClass()) && pPC->isOusters()) {
        Ousters* pOusters = dynamic_cast<Ousters*>(pPC);
        ElementalDomain maxDomain = ELEMENTAL_DOMAIN_FIRE;
        SkillBonus_t maxBonus = pOusters->getSkillPointCount(maxDomain);

        for (ElementalDomain domain = ELEMENTAL_DOMAIN_WATER; domain <= ELEMENTAL_DOMAIN_COMBAT; ++(int&)domain) {
            if (domain == ELEMENTAL_DOMAIN_WIND)
                continue;
            SkillBonus_t bonus = pOusters->getSkillPointCount(domain);
            if (domain == ELEMENTAL_DOMAIN_COMBAT)
                bonus += pOusters->getSkillPointCount(ELEMENTAL_DOMAIN_ELEMENTAL_COMBAT);
            if (bonus > maxBonus) {
                maxBonus = bonus;
                maxDomain = domain;
            }
        }

        Item::ItemClass targetClass = Item::ITEM_CLASS_OUSTERS_WRISTLET;
        int add = 0;

        switch (maxDomain) {
        case ELEMENTAL_DOMAIN_FIRE: {
        } break;
        case ELEMENTAL_DOMAIN_WATER: {
            add = 10;
        } break;
        case ELEMENTAL_DOMAIN_EARTH: {
            add = 20;
        } break;
        case ELEMENTAL_DOMAIN_WIND: {
        } break;
        case ELEMENTAL_DOMAIN_COMBAT: {
            targetClass = Item::ITEM_CLASS_OUSTERS_CHAKRAM;
        } break;
        default:
            break;
        }

        if (targetClass == pItem->getItemClass()) {
            pItem->setItemType(pItem->getItemType() + add);
        } else {
            Item* pNewItem = de::gameContext().itemFactories().createItem(targetClass, pItem->getItemType() + add,
                                                                          pItem->getOptionTypeList());
            if (pNewItem != NULL) {
                SAFE_DELETE(pItem);
                pItem = pNewItem;
            }
        }
    }

    if (pItem->getItemClass() == Item::ITEM_CLASS_OUSTERS_STONE && pPC->isOusters()) {
        Ousters* pOusters = dynamic_cast<Ousters*>(pPC);
        ElementalDomain maxDomain = ELEMENTAL_DOMAIN_FIRE;
        SkillBonus_t maxBonus = pOusters->getSkillPointCount(maxDomain);

        for (ElementalDomain domain = ELEMENTAL_DOMAIN_WATER; domain <= ELEMENTAL_DOMAIN_EARTH; ++(int&)domain) {
            SkillBonus_t bonus = pOusters->getSkillPointCount(domain);
            if (bonus > maxBonus) {
                maxBonus = bonus;
                maxDomain = domain;
            }
        }

        int add = 0;

        switch (maxDomain) {
        case ELEMENTAL_DOMAIN_FIRE: {
        } break;
        case ELEMENTAL_DOMAIN_WATER: {
            add = 5;
        } break;
        case ELEMENTAL_DOMAIN_EARTH: {
            add = 10;
        } break;
        default:
            break;
        }

        pItem->setItemType(pItem->getItemType() + add);
    }

    setItemGender(pItem, (pPC->getSex() == MALE) ? GENDER_MALE : GENDER_FEMALE);

    return pItem;
}

//////////////////////////////////////////////////////////////////////////////
// Exchange System: Point-only trade item check functions
//////////////////////////////////////////////////////////////////////////////

// Check if item is Blue Sapphire (hard currency)
// Blue Sapphire: ItemClass = EVENT_STAR, ItemType = 6
bool isBlueSapphire(Item* pItem) {
    if (pItem == NULL)
        return false;

    Item::ItemClass itemClass = pItem->getItemClass();
    ItemType_t itemType = pItem->getItemType();

    return (itemClass == Item::ITEM_CLASS_EVENT_STAR && itemType == 6);
}

// Get base option type by following PreviousType chain
// Returns the root (most basic) option type in the upgrade chain
OptionType_t getBaseOptionType(OptionType_t type) {
    OptionType_t cur = type;
    unordered_set<OptionType_t> seen; // Prevent infinite loops from circular references

    while (cur != 0) {
        // Check for circular reference
        if (seen.count(cur))
            break;
        seen.insert(cur);

        OptionInfo* pInfo = de::gameContext().optionInfos().getOptionInfo(cur);
        if (pInfo == NULL)
            break; // Missing option info - stop here

        OptionType_t prev = pInfo->getPreviousType();
        if (prev == 0 || prev == cur)
            break; // Reached root or self-reference
        cur = prev;
    }

    return cur;
}

// Check if item has 3 options and at least one is upgraded
// Upgraded means: current option type != base option type
bool isUpgradedThreeOptionItem(Item* pItem) {
    if (pItem == NULL)
        return false;

    // Must have exactly 3 options
    if (pItem->getOptionTypeSize() != 3)
        return false;

    const list<OptionType_t>& optionTypes = pItem->getOptionTypeList();

    // Check if any option is upgraded
    for (OptionType_t t : optionTypes) {
        OptionType_t baseType = getBaseOptionType(t);
        if (baseType != t) {
            return true; // At least one option is upgraded
        }
    }

    return false;
}

// Check if item can ONLY be traded via exchange (using points)
// This includes:
// 1. Blue Sapphire (hard currency)
// 2. Three-option items with at least one upgraded option
bool isPointOnlyTradeItem(Item* pItem) {
    if (pItem == NULL)
        return false;

    if (isBlueSapphire(pItem))
        return true;
    if (isUpgradedThreeOptionItem(pItem))
        return true;

    return false;
}
