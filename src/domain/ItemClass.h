//////////////////////////////////////////////////////////////////////////////
// Filename    : ItemClass.h
// Description :
// de-core: the wire item-class ids (Item::ItemClass on the server,
// ITEM_CLASS on the client), one constant per class, in wire order.
//
// A de-core rule that branches on an item class names the class here, and
// each adapter static_asserts the ids against its own enum, so the two
// enums cannot drift apart unseen.
//////////////////////////////////////////////////////////////////////////////

#ifndef DECORE_ITEM_CLASS_H
#define DECORE_ITEM_CLASS_H

namespace decore {

namespace itemclass {
constexpr int Motorcycle = 0;
constexpr int Potion = 1;
constexpr int Water = 2;
constexpr int HolyWater = 3;
constexpr int Magazine = 4;
constexpr int BombMaterial = 5;
constexpr int Etc = 6;
constexpr int Key = 7;
constexpr int Ring = 8;
constexpr int Bracelet = 9;
constexpr int Necklace = 10;
constexpr int Coat = 11;
constexpr int Trouser = 12;
constexpr int Shoes = 13;
constexpr int Sword = 14;
constexpr int Blade = 15;
constexpr int Shield = 16;
constexpr int Cross = 17;
constexpr int Glove = 18;
constexpr int Helm = 19;
constexpr int SG = 20;
constexpr int SMG = 21;
constexpr int AR = 22;
constexpr int SR = 23;
constexpr int Bomb = 24;
constexpr int Mine = 25;
constexpr int Belt = 26;
constexpr int LearningItem = 27;
constexpr int Money = 28;
constexpr int Corpse = 29;
constexpr int VampireRing = 30;
constexpr int VampireBracelet = 31;
constexpr int VampireNecklace = 32;
constexpr int VampireCoat = 33;
constexpr int Skull = 34;
constexpr int Mace = 35;
constexpr int Serum = 36;
constexpr int VampireEtc = 37;
constexpr int SlayerPortalItem = 38;
constexpr int VampirePortalItem = 39;
constexpr int EventGiftBox = 40;
constexpr int EventStar = 41;
constexpr int VampireEarring = 42;
constexpr int Relic = 43;
constexpr int VampireWeapon = 44;
constexpr int VampireAmulet = 45;
constexpr int QuestItem = 46;
constexpr int EventTree = 47;
constexpr int EventEtc = 48;
constexpr int BloodBible = 49;
constexpr int CastleSymbol = 50;
constexpr int CoupleRing = 51;
constexpr int VampireCoupleRing = 52;
constexpr int EventItem = 53;
constexpr int DyePotion = 54;
constexpr int ResurrectItem = 55;
constexpr int MixingItem = 56;
constexpr int OustersArmsband = 57;
constexpr int OustersBoots = 58;
constexpr int OustersChakram = 59;
constexpr int OustersCirclet = 60;
constexpr int OustersCoat = 61;
constexpr int OustersPendent = 62;
constexpr int OustersRing = 63;
constexpr int OustersStone = 64;
constexpr int OustersWristlet = 65;
constexpr int Larva = 66;
constexpr int Pupa = 67;
constexpr int ComposMei = 68;
constexpr int OustersSummonItem = 69;
constexpr int EffectItem = 70;
constexpr int CodeSheet = 71;
constexpr int MoonCard = 72;
constexpr int Sweeper = 73;
constexpr int PetItem = 74;
constexpr int PetFood = 75;
constexpr int PetEnchantItem = 76;
constexpr int LuckyBag = 77;
constexpr int SMSItem = 78;
constexpr int CoreZap = 79;
constexpr int GQuestItem = 80;
constexpr int TrapItem = 81;
constexpr int BloodBibleSign = 82;
constexpr int WarItem = 83;
constexpr int CarryingReceiver = 84;
constexpr int ShoulderArmor = 85;
constexpr int Dermis = 86;
constexpr int Persona = 87;
constexpr int Fascia = 88;
constexpr int Mitten = 89;

// One past the last id (Item::ITEM_CLASS_MAX).
constexpr int Count = 90;
} // namespace itemclass

} // namespace decore

#endif
