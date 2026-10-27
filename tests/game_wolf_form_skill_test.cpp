//////////////////////////////////////////////////////////////////////////////
// Filename    : game_wolf_form_skill_test.cpp
// Description : The skills a creature in wolf form may use, by the four
//               isAbleToUse*Skill guards: Howl and Eat Corpse everywhere,
//               Untransform on itself and the melee attack on an object,
//               and nothing else; and the werwolf's inventory skills, held
//               to its four like its object and tile skills.
//////////////////////////////////////////////////////////////////////////////

#include <gtest/gtest.h>

#include "Creature.h"
#include "CreatureUtil.h"
#include "Effect.h"
#include "types/SkillTypes.h"

namespace {

// A creature of no race in no zone: the guards read only its flags and
// whether it is dead, a slayer or a player.
class FormCreature : public Creature {
public:
    explicit FormCreature(Effect::EffectClass form) {
        setFlag(form);
    }
    string toString() const override {
        return name;
    }
    bool load() override {
        return false;
    }
    void save() const override {}
    const string& getName() const override {
        return name;
    }
    CreatureClass getCreatureClass() const override {
        return CREATURE_CLASS_NPC;
    }
    string getCreatureClassString() const override {
        return "form";
    }
    Race_t getRace() const override {
        return Race_t{};
    }
    bool isDead() const override {
        return false;
    }
    bool isAlive() const override {
        return true;
    }
    void act(const Timeval&) override {}
    void registerObject() override {}
    Level_t getLevel() const override {
        return 1;
    }

private:
    string name = "form";
};

struct Allowed {
    SkillType_t skill;
    bool self, object, tile, inventory;
};

TEST(GameWolfFormSkill, OnlyHowlEatCorpseUntransformAndMeleeAreAllowed) {
    const Allowed table[] = {
        {SKILL_HOWL, true, true, true, true},
        {SKILL_EAT_CORPSE, true, true, true, true},
        {SKILL_UN_TRANSFORM, true, false, false, false},
        {SKILL_ATTACK_MELEE, false, true, false, false},
        {SKILL_BLOOD_DRAIN, false, false, false, false},
    };
    FormCreature wolf(Effect::EFFECT_CLASS_TRANSFORM_TO_WOLF);
    for (const Allowed& row : table) {
        SCOPED_TRACE(row.skill);
        EXPECT_EQ(isAbleToUseSelfSkill(&wolf, row.skill), row.self);
        EXPECT_EQ(isAbleToUseObjectSkill(&wolf, row.skill), row.object);
        EXPECT_EQ(isAbleToUseTileSkill(&wolf, row.skill), row.tile);
        EXPECT_EQ(isAbleToUseInventorySkill(&wolf, 0, 0, 0, 0, row.skill), row.inventory);
    }
}

TEST(GameWolfFormSkill, AWerwolfsInventorySkillsAreItsFour) {
    FormCreature werwolf(Effect::EFFECT_CLASS_TRANSFORM_TO_WERWOLF);
    EXPECT_TRUE(isAbleToUseInventorySkill(&werwolf, 0, 0, 0, 0, SKILL_UN_TRANSFORM));
    EXPECT_FALSE(isAbleToUseInventorySkill(&werwolf, 0, 0, 0, 0, SKILL_BLOODY_TUNNEL));
    EXPECT_FALSE(isAbleToUseInventorySkill(&werwolf, 0, 0, 0, 0, SKILL_TRANSFORM_TO_BAT));
}

} // namespace
