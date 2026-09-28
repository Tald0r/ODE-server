//////////////////////////////////////////////////////////////////////////////
// Filename    : ItemPrice.cpp
// Description :
// Shop and repair prices. See ItemPrice.h.
//////////////////////////////////////////////////////////////////////////////

#include "domain/ItemPrice.h"

#include <algorithm>

#include "domain/Formulas.h"

using std::max;

namespace decore {

namespace {
// What one charge of a portal or a summon item adds to its price. Unsigned,
// as on the server: a charge times this is unsigned arithmetic.
const unsigned PORTAL_ITEM_CHARGE_PRICE = 5000;
const unsigned SUMMON_ITEM_CHARGE_PRICE = 1000;
} // namespace

int itemPrice(const ItemPriceInput& input) {
    // An item that was given away for free sells for only 1.
    if (input.createTypeGame)
        return 1;
    // A time-limited quest item sells for 50.
    if (input.timeLimited)
        return 50;
    if (input.itemClass == itemclass::MoonCard && input.itemType == 4) {
        return input.crownPrice;
    }

    // Get the item's original price.
    double originalPrice = input.basePrice;
    double finalPrice = 0;

    if (input.grade != -1) {
        double gradePercent = 80 + (5 * input.grade);
        originalPrice *= (gradePercent / 100.0);
    }

    // A slayer portal adds the price of its current charges to the original price.
    if (input.itemClass == itemclass::SlayerPortalItem) {
        originalPrice += (input.charge * PORTAL_ITEM_CHARGE_PRICE);
    } else if (input.itemClass == itemclass::VampirePortalItem) {
        originalPrice += (input.charge * PORTAL_ITEM_CHARGE_PRICE);
    } else if (input.itemClass == itemclass::OustersSummonItem) {
        originalPrice += (input.charge * SUMMON_ITEM_CHARGE_PRICE);
    }

    // If the item has options, multiply the price by the option multiplier.
    if (input.optionCount != 0) {
        finalPrice = 0;

        // price = (original price * the option's PriceMultiplier / 100) + ..
        double priceMultiplier = 0;
        for (int i = 0; i < input.optionCount; i++) {
            priceMultiplier = (double)(input.optionPriceMultipliers[i]);
            finalPrice += (originalPrice * priceMultiplier / 100);
        }

        originalPrice = finalPrice;
    }

    // A damaged item loses price in proportion to the damage.
    double maxDurability = (double)input.maxDurability;
    double curDurability = (double)input.curDurability;

    // Some items have no durability, so handle that case.
    if (maxDurability > 1)
        finalPrice = originalPrice * curDurability / maxDurability;
    else
        finalPrice = originalPrice;

    // Adjust the price again for the shop's market condition.
    finalPrice = finalPrice * input.marketCond / 100;

    // Adjust the price again for the kind of shop.
    if (input.mysteriousRack) {
        finalPrice *= 10;
    }

    // Adjust the price again for the creature's own modifiers.
    if (input.race == PriceRace::Slayer) {
        if ((input.currentStatSum <= 40) && (input.itemClass == itemclass::Potion) &&
            (input.itemType == 0 || input.itemType == 5)) {
            finalPrice = percentValue((int)finalPrice, 70);
        }
    } else if (input.race == PriceRace::Vampire) {
        // A vampire selling a skull gets half the skull's price.
        if (input.itemClass == itemclass::Skull) {
            finalPrice = finalPrice / 2.0;
        }
    } else if (input.race == PriceRace::Ousters) {
        // An ousters selling a skull gets 75% of the skull's price.
        if (input.itemClass == itemclass::Skull) {
            finalPrice *= 0.75;
        }
    }

    // For a paying user in a pay zone.
    if (input.premiumHalf) {
        if (input.itemClass == itemclass::Potion || input.itemClass == itemclass::Serum ||
            input.itemClass == itemclass::Larva || input.itemClass == itemclass::Pupa ||
            input.itemClass == itemclass::ComposMei) {
            // Half price.
            finalPrice = finalPrice / 2;
        }
    }

    // Apply the Blood Bible bonus.
    if (input.itemClass == itemclass::Potion || input.itemClass == itemclass::Serum) {
        int ratio = input.potionPriceRatio;
        if (ratio != 0) {
            // The ratio value is negative.
            finalPrice += percentValue((int)finalPrice, ratio);
        }
    }

    return max(1, (int)finalPrice);
}

int repairPrice(const ItemPriceInput& input) {
    // Get the item's original price.
    double originalPrice = input.basePrice;
    double finalPrice = 0;

    if (input.grade != -1) {
        double gradePercent = 80 + (5 * input.grade);
        originalPrice *= (gradePercent / 100.0);
    }

    // A slayer portal cannot be repaired, but its charges can be topped up.
    if (input.itemClass == itemclass::SlayerPortalItem) {
        int MaxCharge = input.maxCharge;
        int CurCharge = input.charge;

        return (int)((MaxCharge - CurCharge) * PORTAL_ITEM_CHARGE_PRICE);
    }

    if (input.itemClass == itemclass::OustersSummonItem) {
        int MaxCharge = input.maxCharge;
        int CurCharge = input.charge;

        return (int)((MaxCharge - CurCharge) * SUMMON_ITEM_CHARGE_PRICE);
    }

    // If the item has options, multiply the price by the option multiplier.
    if (input.optionCount != 0) {
        finalPrice = 0;
        // price = (original price * sum of the options' PriceMultipliers / 100) * number of options
        double priceMultiplier = 0;
        for (int i = 0; i < input.optionCount; i++) {
            priceMultiplier = (double)(input.optionPriceMultipliers[i]);
            finalPrice += (originalPrice * priceMultiplier / 100);
        }

        originalPrice = finalPrice;
    }

    // A damaged item loses price in proportion to the damage.
    double maxDurability = (double)input.maxDurability;
    double curDurability = (double)input.curDurability;

    // Some items have no durability, so handle that case.
    if (maxDurability != 0) {
        // Return early if the item is at full durability.
        if (curDurability == maxDurability) {
            return 0;
        }

        // Current durability divided by maximum durability gives how damaged the item is.
        // Multiplying the original price by it lowers the value as durability drops.
        finalPrice = originalPrice * curDurability / maxDurability;
    } else {
        // An item without durability cannot be damaged, so
        // its durability-adjusted price equals the original price.
        finalPrice = originalPrice;
    }

    // The repair cost is one tenth of the lost value.
    finalPrice = (originalPrice - finalPrice) / 10.0;

    if (finalPrice < 1.0) {
        return 1;
    }

    return max(0, (int)finalPrice);
}

unsigned skullSellTotal(unsigned priceTimesNum, unsigned headPriceBonusPercent) {
    return priceTimesNum * (headPriceBonusPercent / 100);
}

} // namespace decore
