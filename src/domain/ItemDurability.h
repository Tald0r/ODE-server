//////////////////////////////////////////////////////////////////////////////
// Filename    : ItemDurability.h
// Description :
// de-core: an item's maximum durability.
//
// The server computes it in two steps, and each is a function here:
// the item's own maximum (the table durability moved by the grade and
// floored at 1000, for an item that tracks durability at all), then the
// durability options applied to that maximum. maxDurability() composes
// the two for a caller that holds the inputs of both.
//
// Durability is an unsigned 32-bit quantity on the wire and on the server,
// so these take and return `unsigned`. The option step is computed in 64
// bits, as the server's LP64 `unsigned long` does, and truncated to 32
// bits at the end: a negative option total wraps rather than shrinking the
// maximum, and that wrap is kept.
//////////////////////////////////////////////////////////////////////////////

#ifndef DECORE_ITEM_DURABILITY_H
#define DECORE_ITEM_DURABILITY_H

namespace decore {

// The item's own maximum durability (ConcreteItem::getMaxDurability).
// infoDurability is the item table's durability; hasDurability says
// whether the item class tracks durability (its durability policy);
// gradeDurabilityOffset is the grade policy's durability offset, 0 for a
// class without grade effects. A class without durability gets the table
// value as it is.
unsigned maxDurabilityBase(unsigned infoDurability, bool hasDurability, int gradeDurabilityOffset);

// The maximum after the durability options (computeMaxDurability).
// durabilityPlusPoints holds the plus-point of each OPTION_DURABILITY
// option the item carries, count of them; 100 is neutral.
unsigned maxDurabilityWithOptions(unsigned baseMaxDurability, const int* durabilityPlusPoints, int count);

// maxDurabilityWithOptions(maxDurabilityBase(...), ...).
unsigned maxDurability(unsigned infoDurability, bool hasDurability, int gradeDurabilityOffset,
                       const int* durabilityPlusPoints, int count);

} // namespace decore

#endif
