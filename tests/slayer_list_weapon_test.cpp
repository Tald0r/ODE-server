// slayer_list_weapon_test.cpp - pins the weapon bits of a slayer's
// character-list outlook, as the gameserver writes them.
//
// Slayer::getShapeInfo saves the outlook as Slayer.Shape, and the
// loginserver sends that DWORD as PCSlayerInfo's outlook in LCPCList. Its
// weapon part is slayerListWeaponBits (SlayerListWeapon.h): the class and
// type of the item in the right hand, to the shape the client draws, to
// the four-bit view at bits 11-14 and the extension code at bits 17-18.
// The expectations are literal DWORDs, so an encoder that writes only the
// four bits, or clamps the shape before writing the code, fails here: a
// cross1, a mace and a mace1 would then lose their code.

#include <gtest/gtest.h>

#include "SlayerListWeapon.h"

namespace {

// One right-hand item: its class and type, the shape the client draws it
// as, and the weapon bits of the outlook.
struct ListWeapon {
    Item::ItemClass itemClass;
    ItemType_t itemType;
    WeaponType shape;
    DWORD bits;
};

// Every weapon class on each side of the item-type thresholds at which
// slayerWeaponShape changes the model.
const ListWeapon kListWeapons[] = {
    {Item::ITEM_CLASS_SWORD, 0, WEAPON_SWORD, 0x00000800},
    {Item::ITEM_CLASS_SWORD, 15, WEAPON_SWORD, 0x00000800},
    {Item::ITEM_CLASS_SWORD, 16, WEAPON_SWORD1, 0x00001000},
    {Item::ITEM_CLASS_BLADE, 15, WEAPON_BLADE, 0x00001800},
    {Item::ITEM_CLASS_BLADE, 16, WEAPON_BLADE1, 0x00002000},
    {Item::ITEM_CLASS_SR, 13, WEAPON_SR, 0x00002800},
    {Item::ITEM_CLASS_SR, 14, WEAPON_SR1, 0x00003000},
    {Item::ITEM_CLASS_SR, 15, WEAPON_SR2, 0x00003800},
    {Item::ITEM_CLASS_SR, 16, WEAPON_SR3, 0x00004000},
    {Item::ITEM_CLASS_AR, 13, WEAPON_AR, 0x00004800},
    {Item::ITEM_CLASS_AR, 14, WEAPON_AR1, 0x00005000},
    {Item::ITEM_CLASS_AR, 15, WEAPON_AR2, 0x00005800},
    {Item::ITEM_CLASS_AR, 16, WEAPON_AR3, 0x00006000},
    {Item::ITEM_CLASS_SG, 20, WEAPON_SG, 0x00006800},
    {Item::ITEM_CLASS_SMG, 20, WEAPON_SMG, 0x00007000},
    {Item::ITEM_CLASS_CROSS, 13, WEAPON_CROSS, 0x00007800},
    // A cross1: a cross in the four bits, extension code 1.
    {Item::ITEM_CLASS_CROSS, 14, WEAPON_CROSS1, 0x00027800},
    // A mace: no weapon in the four bits, extension code 2.
    {Item::ITEM_CLASS_MACE, 0, WEAPON_MACE, 0x00040000},
    {Item::ITEM_CLASS_MACE, 13, WEAPON_MACE, 0x00040000},
    // A mace1, from item type 14: no weapon in the four bits, code 3.
    {Item::ITEM_CLASS_MACE, 14, WEAPON_MACE1, 0x00060000},
    {Item::ITEM_CLASS_MACE, 19, WEAPON_MACE1, 0x00060000},
};

} // namespace

TEST(SlayerListWeapon, EachWeaponClassWritesItsShapesFourBitViewAndCode) {
    for (const ListWeapon& weapon : kListWeapons) {
        const DWORD bits = slayerListWeaponBits(weapon.itemClass, weapon.itemType);
        EXPECT_EQ(weapon.bits, bits) << "class " << weapon.itemClass << ", type " << weapon.itemType;
        // A client that reads the extension code sees the shape itself.
        EXPECT_EQ(weapon.shape, PCSlayerInfo::weaponFromBits(bits))
            << "class " << weapon.itemClass << ", type " << weapon.itemType;
        // One that knows only the four bits sees the clamped stand-in.
        EXPECT_EQ(slayerWeaponListShape(weapon.shape), WeaponType((bits >> 11) & 15))
            << "class " << weapon.itemClass << ", type " << weapon.itemType;
    }
}

TEST(SlayerListWeapon, AClassThatIsNoSlayerWeaponWritesNoWeaponBits) {
    for (Item::ItemClass itemClass :
         {Item::ITEM_CLASS_SHIELD, Item::ITEM_CLASS_COAT, Item::ITEM_CLASS_TROUSER, Item::ITEM_CLASS_HELM,
          Item::ITEM_CLASS_MOTORCYCLE, Item::ITEM_CLASS_VAMPIRE_WEAPON, Item::ITEM_CLASS_OUSTERS_CHAKRAM}) {
        for (ItemType_t itemType : {(ItemType_t)0, (ItemType_t)13, (ItemType_t)14, (ItemType_t)16, (ItemType_t)21})
            EXPECT_EQ(0u, slayerListWeaponBits(itemClass, itemType)) << "class " << itemClass << ", type " << itemType;
    }
}

// Whatever the class and the type, the bits stay in the two weapon fields,
// so ORing them into the outlook touches neither the shield between the
// fields nor anything below them.
TEST(SlayerListWeapon, NoClassOrTypeWritesOutsideTheTwoWeaponFields) {
    for (int itemClass = 0; itemClass < Item::ITEM_CLASS_MAX; itemClass++) {
        for (ItemType_t itemType = 0; itemType < 64; itemType++) {
            const DWORD bits = slayerListWeaponBits((Item::ItemClass)itemClass, itemType);
            EXPECT_EQ(0u, bits & ~(0x00007800u | 0x00060000u)) << "class " << itemClass << ", type " << itemType;
        }
    }
}
