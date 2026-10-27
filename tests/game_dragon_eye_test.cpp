//////////////////////////////////////////////////////////////////////////////
// Filename    : game_dragon_eye_test.cpp
// Description : A dragon eye's effect leaves a corpse the way every relic's
//               does: deleteRelicEffect clears the corpse's dragon eye flag
//               and answers that it found a relic.
//////////////////////////////////////////////////////////////////////////////

#include <gtest/gtest.h>

#include "Effect.h"
#include "MonsterCorpse.h"
#include "RelicUtil.h"
#include "item/WarItem.h"

namespace {

TEST(GameDragonEye, ItsEffectLeavesACorpse) {
    MonsterCorpse corpse(0, "corpse", 0);
    corpse.setFlag(Effect::EFFECT_CLASS_DRAGON_EYE);
    WarItem dragonEye;

    EXPECT_TRUE(deleteRelicEffect(&corpse, &dragonEye));
    EXPECT_FALSE(corpse.isFlag(Effect::EFFECT_CLASS_DRAGON_EYE));
}

} // namespace
