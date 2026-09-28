//////////////////////////////////////////////////////////////////////////////
// Filename    : ItemDurability.cpp
// Description :
// An item's maximum durability. See ItemDurability.h.
//////////////////////////////////////////////////////////////////////////////

#include "domain/ItemDurability.h"

#include <algorithm>

namespace decore {

unsigned maxDurabilityBase(unsigned infoDurability, bool hasDurability, int gradeDurabilityOffset) {
    if (hasDurability) {
        unsigned baseDur = infoDurability;
        return (unsigned)std::max(1000, (int)baseDur + gradeDurabilityOffset);
    } else
        return infoDurability;
}

unsigned maxDurabilityWithOptions(unsigned baseMaxDurability, const int* durabilityPlusPoints, int count) {
    // 64 bits wide on every toolchain: the server's accumulator is an LP64
    // `unsigned long`, which is 32 bits under MSVC.
    unsigned long long maxDurability = baseMaxDurability;

    // Start from 100%
    unsigned long long plusPoint = 100;

    for (int i = 0; i < count; i++) {
        plusPoint += (unsigned long long)(durabilityPlusPoints[i] - 100);
    }

    maxDurability = (maxDurability * plusPoint / 100);

    return (unsigned)maxDurability;
}

unsigned maxDurability(unsigned infoDurability, bool hasDurability, int gradeDurabilityOffset,
                       const int* durabilityPlusPoints, int count) {
    return maxDurabilityWithOptions(maxDurabilityBase(infoDurability, hasDurability, gradeDurabilityOffset),
                                    durabilityPlusPoints, count);
}

} // namespace decore
