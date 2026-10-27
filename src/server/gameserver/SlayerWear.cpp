//////////////////////////////////////////////////////////////////////////////
// FileName 	: SlayerWear.cpp
// Description	: Slayer gear: putting an item on and taking it off, deleting the worn set, and which worn items meet their requirements.
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

const Color_t QUEST_COLOR = 0xFFFE;

// Slayer::wearItem()
// Equip an Item in the gear window and compute the attributes.
// This wearItem is the one used while loading Items at connection time.
// It is better not to broadcast inside this method.
void Slayer::wearItem(WearPart Part, Item* pItem)

{
    __BEGIN_DEBUG

    Assert(pItem != NULL);

    Item::ItemClass IClass = pItem->getItemClass();
    Item* pLeft = NULL;
    Item* pRight = NULL;
    Item* pPrevItem = NULL;
    OptionInfo* pOptionInfo = NULL;

    // Take the colour of the first option.
    if (pItem->getFirstOptionType() != 0)
        pOptionInfo = de::gameContext().optionInfos().getOptionInfo(pItem->getFirstOptionType());

    // By the current design an item can always be used even when the
    // attributes are too low, but then the item's attribute bonuses do
    // not apply. So the item is put into the matching gear slot first.
    // A two-handed weapon puts one item pointer into both hand slots.
    if (isTwohandWeapon(pItem)) {
        // Both hands hold an item.
        if (isWear(WEAR_RIGHTHAND) && isWear(WEAR_LEFTHAND)) {
            pLeft = getWearItem(WEAR_RIGHTHAND);
            pRight = getWearItem(WEAR_LEFTHAND);

            // A two-handed weapon is held.
            if (pLeft == pRight) {
                // Put the requested item into the gear slot,
                m_pWearItem[WEAR_RIGHTHAND] = pItem;
                m_pWearItem[WEAR_LEFTHAND] = pItem;

                // by sigi. 2002.5.15
                char pField[80];
                // pItem->save(m_Name, STORAGE_GEAR, 0, Part, 0);
                snprintf(pField, sizeof(pField), "Storage=%d, X=%d", STORAGE_GEAR, Part);
                pItem->tinysave(pField);

                // and hang the previous item on the mouse cursor.
                addItemToExtraInventorySlot(pLeft);
                // pLeft->save(m_Name, STORAGE_EXTRASLOT, 0, 0, 0);
                snprintf(pField, sizeof(pField), "Storage=%d", STORAGE_EXTRASLOT);
                pLeft->tinysave(pField);
            }
            // A sword and a shield are held.
            else {
                // With a sword and a shield in both hands and a two-handed
                // weapon incoming, the sword can go on the mouse cursor but
                // there is nowhere to put the shield. It would have to go
                // into the inventory; for now just refuse to equip it.
                cerr << "Holding a sword and a shield, so a two-handed weapon cannot be equipped." << endl;
                return;
            }
        }
        // Not both hands hold an item.
        else {
            char pField[80];

            // The right hand holds an item.
            if (isWear(WEAR_RIGHTHAND)) {
                pRight = getWearItem(WEAR_RIGHTHAND);
                // Put the requested item into the gear slot.
                m_pWearItem[WEAR_RIGHTHAND] = pItem;
                m_pWearItem[WEAR_LEFTHAND] = pItem;

                // by sigi. 2002.5.15
                // pItem->save(m_Name, STORAGE_GEAR, 0, Part, 0);
                snprintf(pField, sizeof(pField), "Storage=%d, X=%d", STORAGE_GEAR, Part);
                pItem->tinysave(pField);

                // Hang the previous item on the mouse cursor.
                addItemToExtraInventorySlot(pRight);
                // pRight->save(m_Name, STORAGE_EXTRASLOT, 0, 0, 0);
                snprintf(pField, sizeof(pField), "Storage=%d", STORAGE_EXTRASLOT);
                pRight->tinysave(pField);
            }
            // The left hand holds an item.
            else if (isWear(WEAR_LEFTHAND)) {
                pLeft = getWearItem(WEAR_LEFTHAND);
                // Put the requested item into the gear slot.
                m_pWearItem[WEAR_RIGHTHAND] = pItem;
                m_pWearItem[WEAR_LEFTHAND] = pItem;

                // by sigi. 2002.5.15
                // pItem->save(m_Name, STORAGE_GEAR, 0, Part, 0);
                snprintf(pField, sizeof(pField), "Storage=%d, X=%d", STORAGE_GEAR, Part);
                pItem->tinysave(pField);

                // Hang the previous item on the mouse cursor.
                addItemToExtraInventorySlot(pLeft);
                // pLeft->save(m_Name, STORAGE_EXTRASLOT, 0, 0, 0);
                snprintf(pField, sizeof(pField), "Storage=%d", STORAGE_EXTRASLOT);
                pLeft->tinysave(pField);
            }
            // Neither hand holds an item.
            else {
                // Put the requested item into the gear slot.
                m_pWearItem[WEAR_RIGHTHAND] = pItem;
                m_pWearItem[WEAR_LEFTHAND] = pItem;

                // by sigi. 2002.5.15
                // pItem->save(m_Name, STORAGE_GEAR, 0, Part, 0);
                snprintf(pField, sizeof(pField), "Storage=%d, X=%d", STORAGE_GEAR, Part);
                pItem->tinysave(pField);
            }
        }
    } else {
        char pField[80];

        if (isWear(Part)) {
            pPrevItem = getWearItem(Part);
            // Put the requested item into the gear slot.
            m_pWearItem[Part] = pItem;

            // by sigi. 2002.5.15
            // pItem->save(m_Name, STORAGE_GEAR, 0, Part, 0);
            snprintf(pField, sizeof(pField), "Storage=%d, X=%d", STORAGE_GEAR, Part);
            pItem->tinysave(pField);

            // Hang the previous item on the mouse cursor.
            addItemToExtraInventorySlot(pPrevItem);

            // pPrevItem->save(m_Name, STORAGE_EXTRASLOT, 0, 0, 0);
            snprintf(pField, sizeof(pField), "Storage=%d", STORAGE_EXTRASLOT);
            pPrevItem->tinysave(pField);
        } else {
            // Put the requested item into the gear slot.
            m_pWearItem[Part] = pItem;
            // pItem->save(m_Name, STORAGE_GEAR, 0, Part, 0);
            snprintf(pField, sizeof(pField), "Storage=%d, X=%d", STORAGE_GEAR, Part);
            pItem->tinysave(pField);
        }
    }

    ItemType_t IType = pItem->getItemType();

    // Record which weapon is being held.
    // It must be set in SlayerInfo so the next viewer is sent it.
    Color_t color = getItemShapeColor(pItem, pOptionInfo);

    switch (IClass) {
    case Item::ITEM_CLASS_MACE:
        // m_SlayerInfo.setWeaponType(WEAPON_MACE);
        m_SlayerInfo.setWeaponType(slayerWeaponShape(WEAPON_MACE, IType));
        m_SlayerInfo.setWeaponColor(color);
        break;
    case Item::ITEM_CLASS_CROSS:
        m_SlayerInfo.setWeaponType(slayerWeaponShape(WEAPON_CROSS, IType));
        m_SlayerInfo.setWeaponColor(color);
        break;
    case Item::ITEM_CLASS_BLADE:
        m_SlayerInfo.setWeaponType(slayerWeaponShape(WEAPON_BLADE, IType));
        m_SlayerInfo.setWeaponColor(color);
        break;
    case Item::ITEM_CLASS_AR:
        m_SlayerInfo.setWeaponType(slayerWeaponShape(WEAPON_AR, IType));
        m_SlayerInfo.setWeaponColor(color);
        break;
    case Item::ITEM_CLASS_SR:
        m_SlayerInfo.setWeaponType(slayerWeaponShape(WEAPON_SR, IType));
        m_SlayerInfo.setWeaponColor(color);
        break;
    case Item::ITEM_CLASS_SMG:
        m_SlayerInfo.setWeaponType(slayerWeaponShape(WEAPON_SMG, IType));
        m_SlayerInfo.setWeaponColor(color);
        break;
    case Item::ITEM_CLASS_SG:
        m_SlayerInfo.setWeaponType(slayerWeaponShape(WEAPON_SG, IType));
        m_SlayerInfo.setWeaponColor(color);
        break;
    case Item::ITEM_CLASS_HELM:
        m_SlayerInfo.setHelmetType(getHelmetType(IType));
        m_SlayerInfo.setHelmetColor(color);
        break;
    case Item::ITEM_CLASS_SHIELD:
        m_SlayerInfo.setShieldType(getShieldType(IType));
        m_SlayerInfo.setShieldColor(color);
        break;
    case Item::ITEM_CLASS_SWORD:
        m_SlayerInfo.setWeaponType(slayerWeaponShape(WEAPON_SWORD, IType));
        m_SlayerInfo.setWeaponColor(color);
        break;
    case Item::ITEM_CLASS_COAT:
        m_SlayerInfo.setJacketType(getJacketType(IType));
        // cout << "Jacket: ItemType=" << (int)IType << ", JacketType=" <<
        // JacketType2String[m_SlayerInfo.getJacketType()] << endl;
        m_SlayerInfo.setJacketColor(color);
        break;
    case Item::ITEM_CLASS_TROUSER:
        m_SlayerInfo.setPantsType(getPantsType(IType));
        // cout << "Pants: ItemType=" << (int)IType << ", PantsType=" << PantsType2String[m_SlayerInfo.getPantsType()]
        // << endl;
        m_SlayerInfo.setPantsColor(color);
        break;
    case Item::ITEM_CLASS_SHOULDER_ARMOR:
        m_SlayerInfo.setShoulderType(getShoulderType(IType));
        // cout << "Pants: ItemType=" << (int)IType << ", PantsType=" << PantsType2String[m_SlayerInfo.getPantsType()]
        // << endl;
        m_SlayerInfo.setShoulderColor(color);
        break;
    default:
        break;
    }

    __END_DEBUG
}

// Slayer::wearItem()
// Equip an Item in the gear window and compute the attributes.
void Slayer::wearItem(WearPart Part)

{
    __BEGIN_DEBUG

    // Take the item queued for equipping.
    Item* pItem = getExtraInventorySlotItem();
    Assert(pItem != NULL);

    Item::ItemClass IClass = pItem->getItemClass();
    OptionInfo* pOptionInfo = NULL;
    Item* pLeft = NULL;
    Item* pRight = NULL;
    Item* pPrevItem = NULL;
    GCTakeOff _GCTakeOff;

    // Take the colour of the first option.
    if (pItem->getFirstOptionType() != 0)
        pOptionInfo = de::gameContext().optionInfos().getOptionInfo(pItem->getFirstOptionType());


    if (IClass == Item::ITEM_CLASS_SHIELD)
        Part = WEAR_LEFTHAND;
    if (IClass == Item::ITEM_CLASS_SWORD)
        Part = WEAR_RIGHTHAND;

    // Save the current attributes into a buffer before putting on or
    // taking off gear, so that only the changed ones are sent later.
    SLAYER_RECORD prev;
    getSlayerRecord(prev);

    // By the current design an item can always be used even when the
    // attributes are too low, but then the item's attribute bonuses do
    // not apply. So the item is put into the matching gear slot first.
    // A two-handed weapon puts one item pointer into both hand slots.
    if (isTwohandWeapon(pItem)) {
        // Both hands hold an item.
        if (isWear(WEAR_RIGHTHAND) && isWear(WEAR_LEFTHAND)) {
            pLeft = getWearItem(WEAR_RIGHTHAND);
            pRight = getWearItem(WEAR_LEFTHAND);

            // A two-handed weapon is held.
            if (pLeft == pRight) {
                char pField[80];

                takeOffItem(WEAR_LEFTHAND, false, false);

                // Put the requested item into the gear slot,
                m_pWearItem[WEAR_RIGHTHAND] = pItem;
                m_pWearItem[WEAR_LEFTHAND] = pItem;
                // by sigi. 2002.5.15
                // pItem->save(m_Name, STORAGE_GEAR, 0, Part, 0);
                snprintf(pField, sizeof(pField), "Storage=%d, X=%d", STORAGE_GEAR, Part);
                pItem->tinysave(pField);

                // Remove the requested item from the mouse cursor.
                deleteItemFromExtraInventorySlot();
                // Hang the previous item on the mouse cursor.
                addItemToExtraInventorySlot(pLeft);
                // pLeft->save(m_Name, STORAGE_EXTRASLOT, 0, 0, 0);
                snprintf(pField, sizeof(pField), "Storage=%d", STORAGE_EXTRASLOT);
                pLeft->tinysave(pField);

            }
            // A sword and a shield are held.
            else {
                // With a sword and a shield in both hands and a two-handed
                // weapon incoming, the sword can go on the mouse cursor but
                // there is nowhere to put the shield. It would have to go
                // into the inventory; for now just refuse to equip it.
                return;
            }
        }
        // Not both hands hold an item.
        else {
            // by sigi. 2002.5.15
            char pField[80];

            // The right hand holds an item.
            if (isWear(WEAR_RIGHTHAND)) {
                pRight = getWearItem(WEAR_RIGHTHAND);

                takeOffItem(WEAR_RIGHTHAND, false, false);

                // Put the requested item into the gear slot.
                m_pWearItem[WEAR_RIGHTHAND] = pItem;
                m_pWearItem[WEAR_LEFTHAND] = pItem;
                // pItem->save(m_Name, STORAGE_GEAR, 0, Part, 0);

                // by sigi. 2002.5.15
                snprintf(pField, sizeof(pField), "Storage=%d, X=%d", STORAGE_GEAR, Part);
                pItem->tinysave(pField);

                // Remove the requested item from the mouse cursor.
                deleteItemFromExtraInventorySlot();
                // Hang the previous item on the mouse cursor.
                addItemToExtraInventorySlot(pRight);
                // pRight->save(m_Name, STORAGE_EXTRASLOT, 0, 0, 0);
                snprintf(pField, sizeof(pField), "Storage=%d", STORAGE_EXTRASLOT);
                pRight->tinysave(pField);

            }
            // The left hand holds an item.
            else if (isWear(WEAR_LEFTHAND)) {
                pLeft = getWearItem(WEAR_LEFTHAND);

                takeOffItem(WEAR_LEFTHAND, false, false);

                // Put the requested item into the gear slot.
                m_pWearItem[WEAR_RIGHTHAND] = pItem;
                m_pWearItem[WEAR_LEFTHAND] = pItem;

                // by sigi. 2002.5.15
                // pItem->save(m_Name, STORAGE_GEAR, 0, Part, 0);
                snprintf(pField, sizeof(pField), "Storage=%d, X=%d", STORAGE_GEAR, Part);
                pItem->tinysave(pField);

                // Remove the requested item from the mouse cursor.
                deleteItemFromExtraInventorySlot();
                // Hang the previous item on the mouse cursor.
                addItemToExtraInventorySlot(pLeft);
                // pLeft->save(m_Name, STORAGE_EXTRASLOT, 0, 0, 0);
                snprintf(pField, sizeof(pField), "Storage=%d", STORAGE_EXTRASLOT);
                pLeft->tinysave(pField);
            }
            // Neither hand holds an item.
            else {
                // Put the requested item into the gear slot.
                m_pWearItem[WEAR_RIGHTHAND] = pItem;
                m_pWearItem[WEAR_LEFTHAND] = pItem;

                pItem->save(m_Name, STORAGE_GEAR, 0, Part, 0);
                // Remove the requested item from the mouse cursor.
                deleteItemFromExtraInventorySlot();
            }
        }
    } else {
        char pField[80];

        if (isWear(Part)) {
            pPrevItem = getWearItem(Part);

            takeOffItem(Part, false, false);

            // Put the requested item into the gear slot.
            m_pWearItem[Part] = pItem;
            // by sigi. 2002.5.15
            // pItem->save(m_Name, STORAGE_GEAR, 0, Part, 0);
            snprintf(pField, sizeof(pField), "Storage=%d, X=%d", STORAGE_GEAR, Part);
            pItem->tinysave(pField);

            // Remove the requested item from the mouse cursor.
            deleteItemFromExtraInventorySlot();
            // Hang the previous item on the mouse cursor.
            addItemToExtraInventorySlot(pPrevItem);
            // pPrevItem->save(m_Name, STORAGE_EXTRASLOT, 0, 0, 0);
            snprintf(pField, sizeof(pField), "Storage=%d", STORAGE_EXTRASLOT);
            pPrevItem->tinysave(pField);
        } else {
            // Put the requested item into the gear slot.
            m_pWearItem[Part] = pItem;

            // by sigi. 2002.5.15
            // pItem->save(m_Name, STORAGE_GEAR, 0, Part, 0);
            snprintf(pField, sizeof(pField), "Storage=%d, X=%d", STORAGE_GEAR, Part);
            pItem->tinysave(pField);
            // Remove the requested item from the mouse cursor.
            deleteItemFromExtraInventorySlot();
        }
    }

    // Mark it as worn for now.
    // by sigi. 2002.10.31
    m_pRealWearingCheck[Part] = true;

    initAllStat();
    sendRealWearingInfo();
    sendModifyInfo(prev); // Send the attributes that changed.

    // bool bisWeapon = false;
    // ItemType_t IType = pItem->getItemType();

    Color_t color = getItemShapeColor(pItem, pOptionInfo);

    changeShape(pItem, color);

    // Change the outfit if the item can really be worn.
    if (m_pRealWearingCheck[Part])
    // if (bisChange)
    {
        GCChangeShape _GCChangeShape;
        _GCChangeShape.setObjectID(getObjectID());
        _GCChangeShape.setItemClass(IClass);
        _GCChangeShape.setItemType(pItem->getItemType());
        _GCChangeShape.setOptionType(pItem->getFirstOptionType());
        _GCChangeShape.setAttackSpeed(m_AttackSpeed[ATTR_CURRENT]);

        if (color == QUEST_COLOR)
            _GCChangeShape.setFlag(SHAPE_FLAG_QUEST);

        Zone* pZone = m_pZone;
        pZone->broadcastPacket(m_X, m_Y, &_GCChangeShape, this);

        // cout << _GCChangeShape.toString().c_str()  << endl;
    }

    if (m_pZone != NULL) {
        GCOtherModifyInfo gcOtherModifyInfo;
        makeGCOtherModifyInfo(&gcOtherModifyInfo, this, &prev);

        if (gcOtherModifyInfo.getShortCount() != 0 || gcOtherModifyInfo.getLongCount() != 0) {
            m_pZone->broadcastPacket(m_X, m_Y, &gcOtherModifyInfo, this);
        }
    }

    __END_DEBUG
}

// Slayer::takeOffItem()
// *NOTE : a bool parameter is passed in for now.
//         The code gets messy, so change it later.
void Slayer::takeOffItem(WearPart Part, bool bAddOnMouse, bool bSendModifyInfo)

{
    __BEGIN_DEBUG

    SLAYER_RECORD prev;

    // Take the item in the gear slot.
    Item* pItem = m_pWearItem[Part];
    Assert(pItem != NULL);
    Item::ItemClass IClass = pItem->getItemClass();

    if (Part == WEAR_LEFTHAND || Part == WEAR_RIGHTHAND) {
        if (m_pWearItem[WEAR_RIGHTHAND] && m_pWearItem[WEAR_LEFTHAND]) {
            if (m_pWearItem[WEAR_RIGHTHAND] == m_pWearItem[WEAR_LEFTHAND]) {
                m_pWearItem[WEAR_RIGHTHAND] = NULL;
                m_pWearItem[WEAR_LEFTHAND] = NULL;
            }
        }
    }

    // Remove the item from the gear point.
    if (isTwohandWeapon(pItem)) {
        m_pWearItem[WEAR_RIGHTHAND] = NULL;
        m_pWearItem[WEAR_LEFTHAND] = NULL;
    } else
        m_pWearItem[Part] = NULL;

    // When wearItem finds the given slot already occupied it takes the
    // old item off and puts the new one on, which would send one packet
    // for the removal and another for the wear. To avoid the second
    // packet a bool variable was added.
    if (bSendModifyInfo) {
        getSlayerRecord(prev);
        initAllStat();
        sendRealWearingInfo();
        sendModifyInfo(prev);
    } else {
        initAllStat();
    }

    // A check that should not be needed; a temporary patch.
    // Hang the item on the mouse cursor.
    if (bAddOnMouse) {
        addItemToExtraInventorySlot(pItem);

        // Optimized item save.
        // pItem->save(m_Name, STORAGE_EXTRASLOT, 0, 0, 0);
        char pField[80];

        if (pItem->isSilverWeapon()) {
            if (pItem->isGun()) {
                //				Gun* pGun = dynamic_cast<Gun*>(pItem);
                snprintf(pField, sizeof(pField), "Storage=%d, Durability=%d, BulletCount=%d, Silver=%d",
                         STORAGE_EXTRASLOT, pItem->getDurability(), pItem->getBulletCount(), pItem->getSilver());
            } else {
                snprintf(pField, sizeof(pField), "Storage=%d, Durability=%d, Silver=%d", STORAGE_EXTRASLOT,
                         pItem->getDurability(), pItem->getSilver());
            }
        } else {
            snprintf(pField, sizeof(pField), "Storage=%d, Durability=%d", STORAGE_EXTRASLOT, pItem->getDurability());
        }

        pItem->tinysave(pField);
    }

    switch (IClass) {
    case Item::ITEM_CLASS_MACE:
    case Item::ITEM_CLASS_CROSS:
    case Item::ITEM_CLASS_BLADE:
    case Item::ITEM_CLASS_AR:
    case Item::ITEM_CLASS_SR:
    case Item::ITEM_CLASS_SMG:
    case Item::ITEM_CLASS_SG:
        m_SlayerInfo.setWeaponType(WEAPON_NONE);
        break;
    case Item::ITEM_CLASS_HELM:
        m_SlayerInfo.setHelmetType(HELMET_NONE);
        break;
    case Item::ITEM_CLASS_SHIELD:
        m_SlayerInfo.setShieldType(SHIELD_NONE);
        break;
    case Item::ITEM_CLASS_SWORD:
        m_SlayerInfo.setWeaponType(WEAPON_NONE);
        break;
    case Item::ITEM_CLASS_COAT:
        m_SlayerInfo.setJacketType(JACKET_BASIC);
        break;
    case Item::ITEM_CLASS_TROUSER:
        m_SlayerInfo.setPantsType(PANTS_BASIC);
        break;
    case Item::ITEM_CLASS_SHOULDER_ARMOR:
        m_SlayerInfo.setShoulderType(0);
        break;
    default:
        break;
    }

    SlotID_t slot;
    if (slayerTakeOffSlot(IClass, slot)) {
        GCTakeOff _GCTakeOff;
        _GCTakeOff.setObjectID(getObjectID());
        _GCTakeOff.setSlotID(slot);
        m_pZone->broadcastPacket(getX(), getY(), &_GCTakeOff, this);
    }

    if (m_pZone != NULL) {
        GCOtherModifyInfo gcOtherModifyInfo;
        makeGCOtherModifyInfo(&gcOtherModifyInfo, this, &prev);

        if (gcOtherModifyInfo.getShortCount() != 0 || gcOtherModifyInfo.getLongCount() != 0) {
            m_pZone->broadcastPacket(m_X, m_Y, &gcOtherModifyInfo, this);
        }
    }

    __END_DEBUG
}

// destroyGears
// Delete the equipped items.
void Slayer::destroyGears()

{
    __BEGIN_DEBUG

    for (int j = 0; j < WEAR_MAX; j++) {
        Item* pItem = m_pWearItem[j];
        if (pItem != NULL) {
            // Check whether it is a two-handed weapon so that deleting one
            // item clears both hands.
            if (isTwohandWeapon(pItem)) {
                m_pWearItem[WEAR_RIGHTHAND] = NULL;
                m_pWearItem[WEAR_LEFTHAND] = NULL;
            } else
                m_pWearItem[j] = NULL;

            SAFE_DELETE(pItem);
        }
    }

    __END_DEBUG
}

bool Slayer::isRealWearing(WearPart part) const

{
    if (part >= WEAR_MAX)
        throw Error("Slayer::isRealWearing() : invalid wear point!");
    if (m_pWearItem[part] == NULL)
        return false;
    if (part >= WEAR_ZAP1 && part <= WEAR_ZAP4) {
        // A ring must also be worn at the matching position.
        if (m_pWearItem[part - WEAR_ZAP1 + WEAR_FINGER1] == NULL)
            return false;
    }

    return isRealWearing(m_pWearItem[part]);
}

bool Slayer::isRealWearing(Item* pItem) const

{
    if (pItem == NULL)
        return false;

    ItemInfo* pItemInfo = de::gameContext().itemInfos().getItemInfo(pItem->getItemClass(), pItem->getItemType());

    Level_t ReqAdvancedLevel = pItemInfo->getReqAdvancedLevel();
    if (ReqAdvancedLevel > 0 && (!isAdvanced() || getAdvancementClassLevel() < ReqAdvancedLevel))
        return false;

    if (isSlayerWeapon(pItem->getItemClass()) || pItem->getItemClass() == Item::ITEM_CLASS_COAT ||
        pItem->getItemClass() == Item::ITEM_CLASS_TROUSER) {
        if (ReqAdvancedLevel <= 0 && isAdvanced())
            return false;
    }

    // Time-limited items work for free users too, rare or unique.
    if (pItem->isTimeLimitItem())
        return decore::genderAllows(m_Sex, pItemInfo->getReqGender());

    // In a premium zone only paying users get unique/rare items applied.
    // A couple ring is also only usable by paying users.
    if (getZone()->isPremiumZone() &&
        (pItem->isUnique() || pItem->getOptionTypeSize() > 1 || pItem->getItemClass() == Item::ITEM_CLASS_COUPLE_RING ||
         pItem->getItemClass() == Item::ITEM_CLASS_VAMPIRE_COUPLE_RING)) {
        GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(getPlayer());
        if (!pGamePlayer->isPayPlaying() && !pGamePlayer->isPremiumPlay()) {
            // cout << "Premium Item :" << pItem->getItemClassName().c_str() << endl;
            return false;
        }
    }

    if (isCoupleRing(pItem)) {
        return true;
    }

    // The item's requirement raised by its options and capped, against
    // the slayer's current STR, DEX, INT and their sum, and sex. A slayer
    // has no level requirement.
    const decore::EquipRequirement required = computeEquipRequirement(decore::EquipRace::Slayer, pItem, pItemInfo);
    const decore::EquipStats current = {m_STR[ATTR_CURRENT], m_DEX[ATTR_CURRENT], m_INT[ATTR_CURRENT], 0, m_Sex};
    return decore::meetsRequirement(decore::EquipRace::Slayer, required, current);
}

bool Slayer::isRealWearingEx(WearPart part) const {
    if (part >= WEAR_MAX)
        return false;
    return m_pRealWearingCheck[part];
}

DWORD Slayer::sendRealWearingInfo(void) const

{
    DWORD info = 0;
    DWORD flag = 1;

    for (int i = 0; i < WEAR_MAX; i++) {
        if (isRealWearing((Slayer::WearPart)i))
            info |= flag;
        flag <<= 1;
    }

    GCRealWearingInfo pkt;
    pkt.setInfo(info);
    m_pPlayer->sendPacket(&pkt);

    return info;
}
