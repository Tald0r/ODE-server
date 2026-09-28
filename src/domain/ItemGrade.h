//////////////////////////////////////////////////////////////////////////////
// Filename    : ItemGrade.h
// Description :
// de-core: what an item's grade does to its attributes, and which item
// classes keep a grade and a durability of their own.
//
// Every item class the server builds on ConcreteItem has a grade policy
// and a durability policy. The policy decides whether the class keeps a
// grade at all and, if it does, how far each grade step moves the item's
// durability, damage, critical bonus, defense, protection and luck away
// from grade 4, which moves nothing. The per-class choice is this file's
// table, keyed by the wire item class, and the server's ConcreteItem reads
// it, so there is no second copy on the server to disagree with.
//
// The table is the server's, oddities included: CoreZap keeps a grade that
// moves nothing; ShoulderArmor, Persona, Fascia and Mitten move like the
// grocery armor (belt, glove, helm, shield, shoes) rather than like
// accessories or cloth; Dermis, Fascia, CarryingReceiver, CoreZap and
// VampireAmulet keep no durability, so their maximum is their item info's
// durability untouched by grade (maxDurabilityBase with hasDurability
// false). For VampireAmulet that is the item table's durability. The
// server never reads a durability for the other four, so theirs is 1,
// whatever the table's Durability column holds (Dermis, Fascia and
// CarryingReceiver have one; it is 0 in the seed data): the infoDurability
// input for them is 1. Changing any of these is a balance decision,
// recorded in docs/FIXES.md.
//
// Classes the server does not build on ConcreteItem (money, motorcycle,
// relic, ...) are None and have no durability here, which is right for
// their grade (-1) and their grade offsets (0). Their maximum durability
// is not maxDurabilityBase at all: the server reports 1 for each of them,
// whatever the item table holds, so a motorcycle's wear never moves its
// price. A caller computing a maximum for them must not use
// maxDurabilityBase.
//////////////////////////////////////////////////////////////////////////////

#ifndef DECORE_ITEM_GRADE_H
#define DECORE_ITEM_GRADE_H

namespace decore {

// ItemPolicies.h's grade policies.
enum class GradePolicy {
    None,     // NoGrade: no grade; the item's grade reads -1
    Plain,    // HasGrade: a grade that moves nothing
    Weapon,   // WeaponGrade
    Cloth,    // ClothGrade (ArmorGrade<2, 1, 1, 1, 1000>)
    Grocery,  // GroceryGrade (ArmorGrade<1, 1, 1, 2, 500>)
    Accessory // AccessoryGrade
};

// How far a grade moves each attribute from the item table's value.
// The callers clamp the sum: damage at 1, critical, defense and
// protection at 0, durability at 1000 (maxDurabilityBase). luck is the
// item's luck outright, 0 at grade 4; the server narrows it to its 16-bit
// Luck_t.
struct GradeOffsets {
    int durability;
    int damage;
    int critical;
    int defense;
    int protection;
    int luck;
};

// The offsets `grade` gives under `policy`. None and Plain give 0 for
// every attribute whatever the grade.
GradeOffsets gradeOffsets(GradePolicy policy, int grade);

// The grade policy of a wire item class; None for a class without one and
// for an id outside the table.
GradePolicy gradePolicyOf(int itemClass);

// Whether the server's ConcreteItem tracks a durability for a wire item
// class (the HasDurability policy), the hasDurability input of
// maxDurabilityBase; false for an id outside the table. It is false for the
// classes the server does not build on ConcreteItem even where they store a
// durability of their own (the motorcycle, the relics, the castle symbols,
// the blood bibles, the sweepers): see the note at the top of this file.
bool hasDurability(int itemClass);

} // namespace decore

#endif
