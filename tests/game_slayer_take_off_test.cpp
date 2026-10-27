//////////////////////////////////////////////////////////////////////////////
// Filename    : game_slayer_take_off_test.cpp
// Description : The addon slot the zone is told when a slayer's worn item
//               comes off or leaves its shape, one per item class with a
//               shape. Slayer::takeOffItem and Slayer::removeShape both take
//               it from slayerTakeOffSlot, whose table this pins.
//////////////////////////////////////////////////////////////////////////////

#include <gtest/gtest.h>

#include "Item.h"
#include "Slayer.h"
#include "types/CreatureTypes.h"

namespace {

TEST(GameSlayerTakeOff, EachShapedItemClassNamesItsOwnSlot) {
    const struct {
        Item::ItemClass itemClass;
        SlotID_t slot;
    } shaped[] = {
        {Item::ITEM_CLASS_MACE, ADDON_RIGHTHAND},
        {Item::ITEM_CLASS_CROSS, ADDON_RIGHTHAND},
        {Item::ITEM_CLASS_BLADE, ADDON_RIGHTHAND},
        {Item::ITEM_CLASS_AR, ADDON_RIGHTHAND},
        {Item::ITEM_CLASS_SR, ADDON_RIGHTHAND},
        {Item::ITEM_CLASS_SMG, ADDON_RIGHTHAND},
        {Item::ITEM_CLASS_SG, ADDON_RIGHTHAND},
        {Item::ITEM_CLASS_SWORD, ADDON_RIGHTHAND},
        {Item::ITEM_CLASS_HELM, ADDON_HELM},
        {Item::ITEM_CLASS_SHIELD, ADDON_LEFTHAND},
        {Item::ITEM_CLASS_COAT, ADDON_COAT},
        {Item::ITEM_CLASS_TROUSER, ADDON_TROUSER},
        {Item::ITEM_CLASS_SHOULDER_ARMOR, ADDON_SHOULDER},
    };
    for (const auto& row : shaped) {
        SCOPED_TRACE(row.itemClass);
        SlotID_t slot = 0xff;
        EXPECT_TRUE(slayerTakeOffSlot(row.itemClass, slot));
        EXPECT_EQ(slot, row.slot);
    }

    SlotID_t slot = 0xff;
    EXPECT_FALSE(slayerTakeOffSlot(Item::ITEM_CLASS_RING, slot));
}

} // namespace
