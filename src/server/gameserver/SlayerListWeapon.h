//////////////////////////////////////////////////////////////////////////////
// Filename    : SlayerListWeapon.h
// Description : the weapon bits of a slayer's character-list outlook
//////////////////////////////////////////////////////////////////////////////

#ifndef __SLAYER_LIST_WEAPON_H__
#define __SLAYER_LIST_WEAPON_H__

#include "Item.h"
#include "PCSlayerInfo.h"
#include "types/SlayerWeaponShape.h"

// The weapon family an item class is drawn as in a slayer's right hand,
// and WEAPON_NONE for a class that is not a slayer weapon.
inline WeaponType slayerWeaponFamily(Item::ItemClass itemClass) {
    switch (itemClass) {
    case Item::ITEM_CLASS_SWORD:
        return WEAPON_SWORD;
    case Item::ITEM_CLASS_BLADE:
        return WEAPON_BLADE;
    case Item::ITEM_CLASS_SR:
        return WEAPON_SR;
    case Item::ITEM_CLASS_AR:
        return WEAPON_AR;
    case Item::ITEM_CLASS_SG:
        return WEAPON_SG;
    case Item::ITEM_CLASS_SMG:
        return WEAPON_SMG;
    case Item::ITEM_CLASS_CROSS:
        return WEAPON_CROSS;
    case Item::ITEM_CLASS_MACE:
        return WEAPON_MACE;
    default:
        return WEAPON_NONE;
    }
}

// The outlook bits the character list carries for the item in a slayer's
// right hand. Slayer::getShapeInfo ORs them into the DWORD it saves as
// Slayer.Shape, which the loginserver sends as PCSlayerInfo's outlook.
// The item's class and type give the shape the client draws
// (slayerWeaponShape), and PCSlayerInfo::weaponBits writes that shape as
// the four-bit view and the extension code, so a cross1, a mace and a
// mace1 reach a client that reads the code. A class that is not a slayer
// weapon writes no weapon bits.
inline DWORD slayerListWeaponBits(Item::ItemClass itemClass, ItemType_t itemType) {
    return PCSlayerInfo::weaponBits(slayerWeaponShape(slayerWeaponFamily(itemClass), itemType));
}

#endif
