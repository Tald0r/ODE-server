//////////////////////////////////////////////////////////////////////////////
// Filename    : ItemPrice.h
// Description :
// de-core: what a shop charges or pays for an item, what a repair costs,
// and what a castle's tax adds to a purchase.
//
// The arithmetic is transplanted verbatim from the server's PriceManager
// and shop handlers, oddities included: the price stays in double from the
// table price through the grade, the options, the wear and the market
// condition, and is truncated once at the end, except where the original
// truncates in the middle (the low-stat slayer's potion discount, the
// Blood Bible adjustment). getPrice treats an item whose maximum
// durability is 1 as having none (`> 1`) while the repair price does not
// (`!= 0`); a head sells at `bonus / 100` in integers, so a 150% bonus pays
// x1; a vampire portal's repair is priced by durability, not by charges.
// Changing any of these is a balance decision, recorded in docs/FIXES.md.
//
// Every input only one side knows (the event flags, the server's crown
// price, the player's Blood Bible ratio) is an explicit field; nothing
// here reads a table or a global.
//////////////////////////////////////////////////////////////////////////////

#ifndef DECORE_ITEM_PRICE_H
#define DECORE_ITEM_PRICE_H

// The wire item-class ids the price rules branch on.
#include "domain/ItemClass.h"

namespace decore {

// The race of the creature a price is quoted to; None for no creature or
// one that is not a player race.
enum class PriceRace { None, Slayer, Vampire, Ousters };

struct ItemPriceInput {
    int itemClass;
    int itemType;
    unsigned basePrice; // the item table's price
    int grade;          // -1: the item has no grade
    // Current and maximum charges of a portal or summon item; read only
    // for those classes.
    int charge;
    int maxCharge;
    // The price multiplier of each option the item carries, optionCount of
    // them. 0 options leaves the price alone.
    const int* optionPriceMultipliers;
    int optionCount;
    unsigned curDurability;
    unsigned maxDurability; // after options (maxDurabilityWithOptions)

    // Only itemPrice() reads the fields below.
    int marketCond;       // the shop's market condition, a percentage
    bool mysteriousRack;  // the mysterious (gamble) rack prices x10
    bool createTypeGame;  // an item the game gave away sells for 1
    bool timeLimited;     // a time-limited item sells for 50
    int crownPrice;       // the server's CROWN_PRICE: moon card type 4's price
    PriceRace race;       // None when there is no creature
    int currentStatSum;   // a slayer's current STR + DEX + INT
    bool premiumHalf;     // the half-price premium event is on and the player pays
    int potionPriceRatio; // the player's Blood Bible potion ratio, 0 = none
};

// The price of one item (PriceManager::getPrice). Never below 1.
int itemPrice(const ItemPriceInput& input);

// The cost of repairing one item (PriceManager::getRepairPrice). A slayer
// portal or an ousters summon item is charged for the charges it lacks,
// whatever its durability: 0 when it is full, and below 0 when its charge
// is above its maximum. Any other item costs 0 when it has a maximum
// durability and is at it, and at least 1 otherwise, a maximum of 0
// included.
//
// The result is an int, and the server's Price_t is a 32-bit unsigned: its
// adapter converts the result modulo 2^32, so for the over-charged portal
// the vectors pin at -5000 the server charges 4294962296, the value the
// unsigned arithmetic it replaced gave. A caller with an unsigned price
// type gets the server's value by the same conversion.
int repairPrice(const ItemPriceInput& input);

// What a sale of skulls pays (the shop sell handler): the price of the
// skulls sold times the head price bonus, a percentage the server divides
// by 100 in integers first.
unsigned skullSellTotal(unsigned priceTimesNum, unsigned headPriceBonusPercent);

// What a shop purchase costs once a castle taxes it (the shop buy
// handler): the total, which is the item's price times the count bought
// or the mysterious (gamble) rack's price, times the castle's item tax
// ratio, a percentage. A ratio of 100 or below leaves the total alone, so
// it never discounts. Above 100 the server computes
// `(int)(total * (ratio / 100.0))`, taking the tax once on the whole
// total, not per item. The ratio is divided in double first, so a ratio
// whose hundredth rounds below its true value can truncate one below the
// integer answer (100 at 115 is 114). Every ratio from 101 to 112, which
// covers all a guild master can set, rounds at or above it and gives
// total * ratio / 100 in integers.
//
// The widths are the server's: the total and the result are its 32-bit
// unsigned Price_t, since an untaxed total is returned as it came, and a
// taxed one passes through an int. A taxed total of 2^31 or more has no
// int, and converting it is undefined in C++: x86 and x86-64 give INT_MIN,
// which Price_t reads as 2147483648, arm64 gives INT_MAX, and a sanitized
// build traps. This returns x86-64's 2147483648 on every target. It is
// more gold than a player can hold, so the server refuses such a purchase.
unsigned applyCastleTax(unsigned total, int ratio);

} // namespace decore

#endif
