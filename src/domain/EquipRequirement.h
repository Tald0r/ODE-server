//////////////////////////////////////////////////////////////////////////////
// Filename    : EquipRequirement.h
// Description :
// de-core: what a worn item requires of its wearer, and whether the wearer
// meets it.
//
// An item's requirement is its item table's requirement raised by each of
// its options and then capped; requiredStats() computes that and
// meetsRequirement() compares it with the wearer's current stats. Each race
// reads its own fields:
//
// - A slayer needs STR, DEX, INT, their sum and a gender. Each option
//   raises STR, DEX and INT by twice its sum requirement and the sum by
//   once it, but only a requirement that is not 0. STR, DEX and INT are
//   capped at 200 when the item table's value is 200 or less, else at 290;
//   the sum at 300 when the table's is 300 or less, else at 435.
// - A vampire needs a level and a gender. Each option raises the level by
//   its level requirement, also when the table's level is 0; the level is
//   capped at 100 when the table's is 100 or less, else at 150.
// - An ousters needs STR, DEX, INT, their sum and a level, and no gender.
//   Options raise each of them that is not 0: STR, DEX, INT and the sum as
//   a slayer's, the level by the option's level requirement. Only the
//   level is capped, at 150 whatever the table's level.
//
// The server stores STR, DEX, INT, their sum and the gender in 16 bits and
// a level in 8, and so do these functions: each value is narrowed as the
// server narrows it, and each option is added to the narrowed running
// value. A level of 150 raised by 120 is 14, not 270, and is then under
// the cap. A requirement that wraps to exactly 0 skips the options after
// it, because the "not 0" test reads the running value. The wearer's
// STR + DEX + INT wraps in 16 bits too. These are the server's behaviour,
// kept; changing one is a balance decision recorded in docs/FIXES.md.
//
// What stays with the caller: the item and option table lookups, the
// advancement-class checks, time-limited items, the premium-zone and pay
// gate, couple rings, and which race may wear the item at all.
//////////////////////////////////////////////////////////////////////////////

#ifndef DECORE_EQUIP_REQUIREMENT_H
#define DECORE_EQUIP_REQUIREMENT_H

namespace decore {

// A wearer's sex, as the server's Sex enum numbers it.
namespace sex {
constexpr int Female = 0;
constexpr int Male = 1;
} // namespace sex

// An item's gender requirement, as the server's GenderRestriction numbers
// it.
namespace gender {
constexpr int Both = 0;
constexpr int Male = 1;
constexpr int Female = 2;
} // namespace gender

enum class EquipRace { Slayer, Vampire, Ousters };

// An item's requirement: the item table's values as requiredStats() takes
// them, or the raised and capped values it returns.
struct EquipRequirement {
    int str;
    int dex;
    int inte;
    int sum;
    int level;
    int gender;
};

// The wearer's current stats.
struct EquipStats {
    int str;
    int dex;
    int inte;
    int level;
    int sex;
};

// The requirement of an item with `count` options, from the item table's
// requirement `base` and each option's sum requirement (optionReqSums) and
// level requirement (optionReqLevels), in the item's option order. A field
// the race does not read is returned as it was given.
EquipRequirement requiredStats(EquipRace race, const EquipRequirement& base, const int* optionReqSums,
                               const int* optionReqLevels, int count);

// Whether a wearer with `current` stats meets `required`, a requirement
// requiredStats() returned for the same race. Fields the race does not
// read are ignored.
bool meetsRequirement(EquipRace race, const EquipRequirement& required, const EquipStats& current);

// Whether a wearer of `sex` may wear an item with gender requirement
// `reqGender`: every pair but a male wearer of a female item and a female
// wearer of a male item, so a value that names neither sex allows anyone.
// meetsRequirement() applies it for a slayer and a vampire.
bool genderAllows(int sex, int reqGender);

} // namespace decore

#endif
