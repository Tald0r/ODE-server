//////////////////////////////////////////////////////////////////////////////
// FileName 	: SlayerShape.cpp
// Description	: Slayer outfit: the shape flags and colours the worn gear shows, and the shape changes that wearing and taking off apply.
//////////////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <stdlib.h>

#include <algorithm>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include "AbilityBalance.h"
#include "AdvancementClassExpTable.h"
#include "CastleInfoManager.h"
#include "CastleSkillInfo.h"
#include "CreatureUtil.h"
#include "DynamicZone.h"
#include "EffectGrandMasterSlayer.h"
#include "EffectLoaderManager.h"
#include "EffectManager.h"
#include "ExpFwd.h"
#include "FlagSet.h"
#include "GCAddEffect.h"
#include "GCChangeShape.h"
#include "GCModifyInformation.h"
#include "GCOtherModifyInfo.h"
#include "GCPetStashList.h"
#include "GCRealWearingInfo.h"
#include "GCRemoveEffect.h"
#include "GCSkillInfo.h"
#include "GCStatusCurrentHP.h"
#include "GCTakeOff.h"
#include "GCTradeFinish.h"
#include "GameContext.h"
#include "GamePlayer.h"
#include "GuildUnion.h"
#include "ItemInfoManager.h"
#include "ItemUtil.h"
#include "LoginServerManager.h"
#include "MonsterInfo.h"
#include "OptionInfo.h"
#include "PKZoneInfoManager.h"
#include "PacketUtil.h"
#include "ParkingCenter.h"
#include "Party.h"
#include "Player.h"
#include "Properties.h"
#include "RaceWarLimiter.h"
#include "RankExpTable.h"
#include "ResurrectLocationManager.h"
#include "Shape.h"
#include "SkillDomainInfoManager.h"
#include "SkillHandlerManager.h"
#include "SkillInfo.h"
#include "Slayer.h"
#include "SlayerAttrExpTable.h"
#include "SlayerListWeapon.h"
#include "Stash.h"
#include "Store.h"
#include "SystemAvailabilitiesManager.h"
#include "TimeLimitItemManager.h"
#include "TradeManager.h"
#include "VariableManager.h"
#include "WarSystem.h"
#include "domain/EquipRequirement.h"
#include "item/AR.h"
#include "item/Belt.h"
#include "item/Motorcycle.h"
#include "item/SG.h"
#include "item/SMG.h"
#include "item/SR.h"
#include "item/Skull.h"
#include "repository/CharacterRepository.h"
#include "repository/GoldRepository.h"
#include "repository/SkillSaveRepository.h"
#include "repository/StashRepository.h"
#include "skill/CastleSkillSlot.h"
#include "skill/EffectBless.h"
#include "skill/EffectChargingPower.h"
#include "skill/EffectDancingSword.h"
#include "skill/EffectDoom.h"
#include "skill/EffectGhostBlade.h"
#include "skill/EffectParalyze.h"
#include "skill/EffectPotentialExplosion.h"
#include "skill/EffectProtectionFromAcid.h"
#include "skill/EffectProtectionFromCurse.h"
#include "skill/EffectProtectionFromPoison.h"
#include "skill/SkillUtil.h"
#include "types/ServerType.h"

const Color_t UNIQUE_OPTION = 0xFFFF;
const Color_t QUEST_OPTION = 0xFFFE;

//----------------------------------------------------------------------
// getShapeInfo
//----------------------------------------------------------------------
// Build the outfit flag/color information from the slayer's current outfit.
// This is to speed up processing at login.
//----------------------------------------------------------------------
// 32 bits for 32 kinds is considered enough for now.
// If it ever overflows a bitset will be needed.
//
// (!) The color is not an index color value but the optionType.
//     The client looks the color value up from the option.
//----------------------------------------------------------------------
void Slayer::getShapeInfo(DWORD& flag, Color_t colors[PCSlayerInfo::SLAYER_COLOR_MAX]) const
//
{
    __BEGIN_DEBUG

    Item* pItem;
    // OptionInfo* 				pOptionInfo;
    PCSlayerInfo::SlayerBits slayerBit;
    PCSlayerInfo::SlayerColors slayerColor;

    WearPart Part;

    // Initialize
    flag = 0;

    //-----------------------------------------------------------------
    // Sex
    //-----------------------------------------------------------------
    slayerBit = PCSlayerInfo::SLAYER_BIT_SEX;
    flag |= ((m_Sex ? 1 : 0) << slayerBit);

    //-----------------------------------------------------------------
    // HairStyle
    //-----------------------------------------------------------------
    slayerBit = PCSlayerInfo::SLAYER_BIT_HAIRSTYLE1;
    flag |= (m_HairStyle << slayerBit);

    //-----------------------------------------------------------------
    // Trousers
    //-----------------------------------------------------------------
    Part = WEAR_LEG;
    pItem = m_pWearItem[Part];
    slayerBit = PCSlayerInfo::SLAYER_BIT_PANTS1;
    slayerColor = PCSlayerInfo::SLAYER_COLOR_PANTS;
    if (pItem != NULL && m_pRealWearingCheck[Part]) {
        ItemType_t IType = pItem->getItemType();

        //		colors[slayerColor] = (pItem->isUnique()? UNIQUE_OPTION : pItem->getFirstOptionType());
        if (pItem->isTimeLimitItem())
            colors[slayerColor] = QUEST_OPTION;
        else if (pItem->isUnique())
            colors[slayerColor] = UNIQUE_OPTION;
        else
            colors[slayerColor] = pItem->getFirstOptionType();

        flag |= (getPantsType(IType) << slayerBit);
    } else {
        colors[slayerColor] = 0;
        flag |= (PANTS_BASIC << slayerBit);
    }
    //-----------------------------------------------------------------
    // Jacket
    //-----------------------------------------------------------------
    Part = WEAR_BODY;
    pItem = m_pWearItem[Part];
    slayerBit = PCSlayerInfo::SLAYER_BIT_JACKET1;
    slayerColor = PCSlayerInfo::SLAYER_COLOR_JACKET;
    if (pItem != NULL && m_pRealWearingCheck[Part]) {
        ItemType_t IType = pItem->getItemType();

        //		colors[slayerColor] = (pItem->isUnique()? UNIQUE_OPTION : pItem->getFirstOptionType());

        if (pItem->isTimeLimitItem())
            colors[slayerColor] = QUEST_OPTION;
        else if (pItem->isUnique())
            colors[slayerColor] = UNIQUE_OPTION;
        else
            colors[slayerColor] = pItem->getFirstOptionType();

        flag |= (getJacketType(IType) << slayerBit);
    } else {
        colors[slayerColor] = 0;
        flag |= (JACKET_BASIC << slayerBit);
    }

    //-----------------------------------------------------------------
    // Helmet
    //-----------------------------------------------------------------
    Part = WEAR_HEAD;
    pItem = m_pWearItem[Part];
    slayerBit = PCSlayerInfo::SLAYER_BIT_HELMET1;
    slayerColor = PCSlayerInfo::SLAYER_COLOR_HELMET;
    if (pItem != NULL && m_pRealWearingCheck[Part]) {
        ItemType_t IType = pItem->getItemType();

        //		colors[slayerColor] = (pItem->isUnique()? UNIQUE_OPTION : pItem->getFirstOptionType());

        if (pItem->isTimeLimitItem())
            colors[slayerColor] = QUEST_OPTION;
        else if (pItem->isUnique())
            colors[slayerColor] = UNIQUE_OPTION;
        else
            colors[slayerColor] = pItem->getFirstOptionType();

        flag |= (getHelmetType(IType) << slayerBit);

    } else {
        colors[slayerColor] = 0;
        // none
    }

    //-----------------------------------------------------------------
    // Shield
    //-----------------------------------------------------------------
    Part = WEAR_LEFTHAND;
    pItem = m_pWearItem[Part];
    slayerBit = PCSlayerInfo::SLAYER_BIT_SHIELD1;
    slayerColor = PCSlayerInfo::SLAYER_COLOR_SHIELD;
    if (pItem != NULL && m_pRealWearingCheck[Part] && pItem->getItemClass() == Item::ITEM_CLASS_SHIELD) {
        ItemType_t IType = pItem->getItemType();

        //		colors[slayerColor] = (pItem->isUnique()? UNIQUE_OPTION : pItem->getFirstOptionType());

        //		if ( pItem->isUnique() ) colors[slayerColor] = UNIQUE_OPTION;
        if (pItem->isTimeLimitItem())
            colors[slayerColor] = QUEST_OPTION;
        else if (pItem->isUnique())
            colors[slayerColor] = UNIQUE_OPTION;
        else
            colors[slayerColor] = pItem->getFirstOptionType();

        flag |= (getShieldType(IType) << slayerBit);
    } else {
        colors[slayerColor] = 0;
        // none
    }

    //-----------------------------------------------------------------
    // Weapon
    //-----------------------------------------------------------------
    Part = WEAR_RIGHTHAND;
    pItem = m_pWearItem[Part];
    slayerColor = PCSlayerInfo::SLAYER_COLOR_WEAPON;

    if (pItem != NULL && m_pRealWearingCheck[Part]) {
        // colors[slayerColor] = (pItem->isUnique()? UNIQUE_OPTION : pItem->getFirstOptionType());

        if (pItem->isUnique())
            colors[slayerColor] = UNIQUE_OPTION;
        else if (pItem->isTimeLimitItem())
            colors[slayerColor] = QUEST_OPTION;
        else
            colors[slayerColor] = pItem->getFirstOptionType();

        flag |= slayerListWeaponBits(pItem->getItemClass(), pItem->getItemType());
    } else {
        colors[slayerColor] = 0;
    }

    __END_DEBUG
}


Slayer::WearPart Slayer::getWearPart(Item::ItemClass IClass) const {
    switch (IClass) {
    case Item::ITEM_CLASS_COAT:
        return WEAR_BODY;
    case Item::ITEM_CLASS_TROUSER:
        return WEAR_LEG;

    case Item::ITEM_CLASS_SWORD:
    case Item::ITEM_CLASS_BLADE:
    case Item::ITEM_CLASS_AR:
    case Item::ITEM_CLASS_SR:
    case Item::ITEM_CLASS_SG:
    case Item::ITEM_CLASS_SMG:
    case Item::ITEM_CLASS_MACE:
    case Item::ITEM_CLASS_CROSS:
        return WEAR_RIGHTHAND;

    case Item::ITEM_CLASS_HELM:
        return WEAR_HEAD;

    case Item::ITEM_CLASS_SHIELD:
        return WEAR_LEFTHAND;

    case Item::ITEM_CLASS_SHOULDER_ARMOR:
        return WEAR_SHOULDER;

    default:
        return WEAR_MAX;
    }

    return WEAR_MAX;
}

bool Slayer::changeShape(Item* pItem, Color_t color, bool bSendPacket) {
    Item::ItemClass IClass = pItem->getItemClass();
    ItemType_t IType = pItem->getItemType();

    WearPart Part = getWearPart(IClass);

    if (Part == WEAR_MAX)
        return false;

    bool bRealWear = m_pRealWearingCheck[Part];

    if (bRealWear)
        return addShape(IClass, IType, color);

    return removeShape(IClass, bSendPacket);
}

bool Slayer::addShape(Item::ItemClass IClass, ItemType_t IType, Color_t color) {
    bool bisChange = false;

    switch (IClass) {
    case Item::ITEM_CLASS_MACE:
        bisChange = true;
        // m_SlayerInfo.setWeaponType(WEAPON_MACE);

        m_SlayerInfo.setWeaponType(slayerWeaponShape(WEAPON_MACE, IType));
        m_SlayerInfo.setWeaponColor(color);
        break;
    case Item::ITEM_CLASS_CROSS:
        bisChange = true;

        m_SlayerInfo.setWeaponType(slayerWeaponShape(WEAPON_CROSS, IType));
        m_SlayerInfo.setWeaponColor(color);
        break;
    case Item::ITEM_CLASS_BLADE:
        bisChange = true;

        m_SlayerInfo.setWeaponType(slayerWeaponShape(WEAPON_BLADE, IType));
        m_SlayerInfo.setWeaponColor(color);
        break;
    case Item::ITEM_CLASS_AR:
        bisChange = true;

        m_SlayerInfo.setWeaponType(slayerWeaponShape(WEAPON_AR, IType));
        m_SlayerInfo.setWeaponColor(color);
        break;
    case Item::ITEM_CLASS_SR:
        bisChange = true;

        m_SlayerInfo.setWeaponType(slayerWeaponShape(WEAPON_SR, IType));
        m_SlayerInfo.setWeaponColor(color);
        break;
    case Item::ITEM_CLASS_SMG:
        bisChange = true;

        m_SlayerInfo.setWeaponType(slayerWeaponShape(WEAPON_SMG, IType));
        m_SlayerInfo.setWeaponColor(color);
        break;
    case Item::ITEM_CLASS_SG:
        bisChange = true;

        m_SlayerInfo.setWeaponType(slayerWeaponShape(WEAPON_SG, IType));
        m_SlayerInfo.setWeaponColor(color);
        break;
    case Item::ITEM_CLASS_HELM:
        bisChange = true;

        m_SlayerInfo.setHelmetType(getHelmetType(IType));
        m_SlayerInfo.setHelmetColor(color);
        break;
    case Item::ITEM_CLASS_SHIELD:
        bisChange = true;

        m_SlayerInfo.setShieldType(getShieldType(IType));
        m_SlayerInfo.setShieldColor(color);
        break;
    case Item::ITEM_CLASS_SWORD:
        bisChange = true;

        m_SlayerInfo.setWeaponType(slayerWeaponShape(WEAPON_SWORD, IType));
        m_SlayerInfo.setWeaponColor(color);
        break;
    case Item::ITEM_CLASS_COAT:
        bisChange = true;

        m_SlayerInfo.setJacketType(getJacketType(IType));
        m_SlayerInfo.setJacketColor(color);
        break;
    case Item::ITEM_CLASS_TROUSER:
        bisChange = true;

        m_SlayerInfo.setPantsType(getPantsType(IType));
        m_SlayerInfo.setPantsColor(color);
        break;

    case Item::ITEM_CLASS_SHOULDER_ARMOR:
        bisChange = true;

        m_SlayerInfo.setShoulderType(getShoulderType(IType));
        m_SlayerInfo.setShoulderColor(color);
        break;

    default:
        break;
    }

    return bisChange;
}

bool slayerTakeOffSlot(Item::ItemClass itemClass, SlotID_t& slot) {
    switch (itemClass) {
    case Item::ITEM_CLASS_MACE:
    case Item::ITEM_CLASS_CROSS:
    case Item::ITEM_CLASS_BLADE:
    case Item::ITEM_CLASS_AR:
    case Item::ITEM_CLASS_SR:
    case Item::ITEM_CLASS_SMG:
    case Item::ITEM_CLASS_SG:
    case Item::ITEM_CLASS_SWORD:
        slot = ADDON_RIGHTHAND;
        return true;
    case Item::ITEM_CLASS_HELM:
        slot = ADDON_HELM;
        return true;
    case Item::ITEM_CLASS_SHIELD:
        slot = ADDON_LEFTHAND;
        return true;
    case Item::ITEM_CLASS_COAT:
        slot = ADDON_COAT;
        return true;
    case Item::ITEM_CLASS_TROUSER:
        slot = ADDON_TROUSER;
        return true;
    case Item::ITEM_CLASS_SHOULDER_ARMOR:
        slot = ADDON_SHOULDER;
        return true;
    default:
        return false;
    }
}

bool Slayer::removeShape(Item::ItemClass IClass, bool bSendPacket) {
    bool bisWeapon = false;

    // When items are dropped on death the packet must go to the owner too.
    // It would be better taken as a parameter, but the header is left alone.
    // by sigi. 2002.11.7
    Creature* pOwner = (isDead() ? NULL : this);

    switch (IClass) {
    case Item::ITEM_CLASS_MACE:
    case Item::ITEM_CLASS_CROSS:
    case Item::ITEM_CLASS_BLADE:
    case Item::ITEM_CLASS_AR:
    case Item::ITEM_CLASS_SR:
    case Item::ITEM_CLASS_SMG:
    case Item::ITEM_CLASS_SG: {
        bisWeapon = true;
        m_SlayerInfo.setWeaponType(WEAPON_NONE);
    } break;

    case Item::ITEM_CLASS_HELM: {
        m_SlayerInfo.setHelmetType(HELMET_NONE);
    } break;

    case Item::ITEM_CLASS_SHIELD: {
        m_SlayerInfo.setShieldType(SHIELD_NONE);
    } break;

    case Item::ITEM_CLASS_SWORD: {
        bisWeapon = true;
        m_SlayerInfo.setWeaponType(WEAPON_NONE);
    } break;

    case Item::ITEM_CLASS_COAT: {
        m_SlayerInfo.setJacketType(JACKET_BASIC);
    } break;

    case Item::ITEM_CLASS_TROUSER: {
        m_SlayerInfo.setPantsType(PANTS_BASIC);
    } break;

    case Item::ITEM_CLASS_SHOULDER_ARMOR: {
        m_SlayerInfo.setShoulderType(0);
    } break;

    default:
        break;
    }

    SlotID_t slot;
    if (bSendPacket && slayerTakeOffSlot(IClass, slot)) {
        GCTakeOff _GCTakeOff;
        _GCTakeOff.setObjectID(getObjectID());
        _GCTakeOff.setSlotID(slot);
        m_pZone->broadcastPacket(getX(), getY(), &_GCTakeOff, pOwner);
    }

    return bisWeapon;
}
