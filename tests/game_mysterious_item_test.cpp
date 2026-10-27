//////////////////////////////////////////////////////////////////////////////
// Filename    : game_mysterious_item_test.cpp
// Description : The item type a vampire's mysterious item falls back on when
//               none of its random draws fits the buyer: type 1, the women's
//               coat, only for a woman buying a vampire coat, and type 0 for
//               every other class and for men. getRandomMysteriousItem takes
//               its vampire fallback from this one function.
//////////////////////////////////////////////////////////////////////////////

#include <gtest/gtest.h>

#include "Item.h"
#include "ItemUtil.h"
#include "types/CreatureTypes.h"

namespace {

TEST(GameMysteriousItem, OnlyAWomansVampireCoatFallsBackOnTypeOne) {
    EXPECT_EQ(vampireMysteriousFallbackItemType(Item::ITEM_CLASS_VAMPIRE_COAT, FEMALE), 1);
    EXPECT_EQ(vampireMysteriousFallbackItemType(Item::ITEM_CLASS_VAMPIRE_COAT, MALE), 0);

    const Item::ItemClass others[] = {Item::ITEM_CLASS_VAMPIRE_WEAPON,   Item::ITEM_CLASS_VAMPIRE_RING,
                                      Item::ITEM_CLASS_VAMPIRE_BRACELET, Item::ITEM_CLASS_VAMPIRE_NECKLACE,
                                      Item::ITEM_CLASS_VAMPIRE_EARRING,  Item::ITEM_CLASS_VAMPIRE_AMULET};
    for (Item::ItemClass itemClass : others) {
        SCOPED_TRACE(itemClass);
        EXPECT_EQ(vampireMysteriousFallbackItemType(itemClass, FEMALE), 0);
        EXPECT_EQ(vampireMysteriousFallbackItemType(itemClass, MALE), 0);
    }
}

} // namespace
