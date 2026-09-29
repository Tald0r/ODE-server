//////////////////////////////////////////////////////////////////////////////
// Filename    : SlayerWeaponShape.h
// Description : the weapon a slayer is seen holding, as the client draws it
//////////////////////////////////////////////////////////////////////////////

#ifndef __SLAYER_WEAPON_SHAPE_H__
#define __SLAYER_WEAPON_SHAPE_H__

#include "CreatureTypes.h"
#include "ItemTypes.h"

// The five weapon bits of a slayer's outlook name one of the client's
// weapon templates (Client/PacketFunction.cpp, g_pPacketItemWeapon), and
// the client draws the other player with that template's addon frames and
// attacks with its action. The retail client split the families into
// variants for the high-tier models, whose frames differ from the base
// model's, and it picks each variant by the item type of the real item.
// The server sends the same variant for the same item, so what a player
// sees on others is what those players see on themselves.
//
// The thresholds come from the client's Item.inf: the addon frames of a
// sword or blade change at type 16, those of a rifle at types 14, 15 and
// 16, those of a cross or mace at type 14; shotguns and submachine guns
// keep one frame set for the whole class.
inline WeaponType slayerWeaponShape(WeaponType family, ItemType_t itemType) {
    switch (family) {
    case WEAPON_SWORD:
        return itemType >= 16 ? WEAPON_SWORD1 : WEAPON_SWORD;
    case WEAPON_BLADE:
        return itemType >= 16 ? WEAPON_BLADE1 : WEAPON_BLADE;
    case WEAPON_SR:
        if (itemType >= 16)
            return WEAPON_SR3;
        if (itemType == 15)
            return WEAPON_SR2;
        return itemType == 14 ? WEAPON_SR1 : WEAPON_SR;
    case WEAPON_AR:
        if (itemType >= 16)
            return WEAPON_AR3;
        if (itemType == 15)
            return WEAPON_AR2;
        return itemType == 14 ? WEAPON_AR1 : WEAPON_AR;
    case WEAPON_CROSS:
        return itemType >= 14 ? WEAPON_CROSS1 : WEAPON_CROSS;
    case WEAPON_MACE:
        return itemType >= 14 ? WEAPON_MACE1 : WEAPON_MACE;
    default:
        return family;
    }
}

// The weapon the character list (PCSlayerInfo, stored as Slayer.Shape)
// carries in its four-bit weapon field. A client that knows only those
// four bits reads them with a four-bit mask, so a value of 16 or more
// cannot travel there: its high bit would land in the shield field. The
// high-tier cross shows as the base cross; a mace, which has no value
// below 16, shows as no weapon rather than as a rifle.
// PCSlayerInfo::weaponBits writes this beside the extension code that
// names the real shape.
constexpr WeaponType slayerWeaponListShape(WeaponType shape) {
    if (shape < 16)
        return shape;
    return shape == WEAPON_CROSS1 ? WEAPON_CROSS : WEAPON_NONE;
}

#endif
