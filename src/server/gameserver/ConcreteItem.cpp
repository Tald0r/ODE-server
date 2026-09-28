//////////////////////////////////////////////////////////////////////////////
// Filename    : ConcreteItem.cpp
// Description :
// The functions ConcreteItem.h declares over de-core: an item's base
// maximum durability, and its class's grade and durability rules read from
// the per-class table (domain/ItemGrade.h). They live here, beside nothing
// else, so the header stays free of de-core includes and a test can link
// them without the rest of the item utilities.
//////////////////////////////////////////////////////////////////////////////

#include "ConcreteItem.h"

#include "domain/ItemClass.h"
#include "domain/ItemDurability.h"
#include "domain/ItemGrade.h"

Durability_t computeBaseMaxDurability(Durability_t infoDurability, bool hasDurability, int gradeDurabilityOffset) {
    return decore::maxDurabilityBase(infoDurability, hasDurability, gradeDurabilityOffset);
}

// de-core's item-class table is keyed by the wire id: every id it names is
// this server's Item::ItemClass value, and the table covers them all.
static_assert(decore::itemclass::Motorcycle == Item::ITEM_CLASS_MOTORCYCLE);
static_assert(decore::itemclass::Potion == Item::ITEM_CLASS_POTION);
static_assert(decore::itemclass::Water == Item::ITEM_CLASS_WATER);
static_assert(decore::itemclass::HolyWater == Item::ITEM_CLASS_HOLYWATER);
static_assert(decore::itemclass::Magazine == Item::ITEM_CLASS_MAGAZINE);
static_assert(decore::itemclass::BombMaterial == Item::ITEM_CLASS_BOMB_MATERIAL);
static_assert(decore::itemclass::Etc == Item::ITEM_CLASS_ETC);
static_assert(decore::itemclass::Key == Item::ITEM_CLASS_KEY);
static_assert(decore::itemclass::Ring == Item::ITEM_CLASS_RING);
static_assert(decore::itemclass::Bracelet == Item::ITEM_CLASS_BRACELET);
static_assert(decore::itemclass::Necklace == Item::ITEM_CLASS_NECKLACE);
static_assert(decore::itemclass::Coat == Item::ITEM_CLASS_COAT);
static_assert(decore::itemclass::Trouser == Item::ITEM_CLASS_TROUSER);
static_assert(decore::itemclass::Shoes == Item::ITEM_CLASS_SHOES);
static_assert(decore::itemclass::Sword == Item::ITEM_CLASS_SWORD);
static_assert(decore::itemclass::Blade == Item::ITEM_CLASS_BLADE);
static_assert(decore::itemclass::Shield == Item::ITEM_CLASS_SHIELD);
static_assert(decore::itemclass::Cross == Item::ITEM_CLASS_CROSS);
static_assert(decore::itemclass::Glove == Item::ITEM_CLASS_GLOVE);
static_assert(decore::itemclass::Helm == Item::ITEM_CLASS_HELM);
static_assert(decore::itemclass::SG == Item::ITEM_CLASS_SG);
static_assert(decore::itemclass::SMG == Item::ITEM_CLASS_SMG);
static_assert(decore::itemclass::AR == Item::ITEM_CLASS_AR);
static_assert(decore::itemclass::SR == Item::ITEM_CLASS_SR);
static_assert(decore::itemclass::Bomb == Item::ITEM_CLASS_BOMB);
static_assert(decore::itemclass::Mine == Item::ITEM_CLASS_MINE);
static_assert(decore::itemclass::Belt == Item::ITEM_CLASS_BELT);
static_assert(decore::itemclass::LearningItem == Item::ITEM_CLASS_LEARNINGITEM);
static_assert(decore::itemclass::Money == Item::ITEM_CLASS_MONEY);
static_assert(decore::itemclass::Corpse == Item::ITEM_CLASS_CORPSE);
static_assert(decore::itemclass::VampireRing == Item::ITEM_CLASS_VAMPIRE_RING);
static_assert(decore::itemclass::VampireBracelet == Item::ITEM_CLASS_VAMPIRE_BRACELET);
static_assert(decore::itemclass::VampireNecklace == Item::ITEM_CLASS_VAMPIRE_NECKLACE);
static_assert(decore::itemclass::VampireCoat == Item::ITEM_CLASS_VAMPIRE_COAT);
static_assert(decore::itemclass::Skull == Item::ITEM_CLASS_SKULL);
static_assert(decore::itemclass::Mace == Item::ITEM_CLASS_MACE);
static_assert(decore::itemclass::Serum == Item::ITEM_CLASS_SERUM);
static_assert(decore::itemclass::VampireEtc == Item::ITEM_CLASS_VAMPIRE_ETC);
static_assert(decore::itemclass::SlayerPortalItem == Item::ITEM_CLASS_SLAYER_PORTAL_ITEM);
static_assert(decore::itemclass::VampirePortalItem == Item::ITEM_CLASS_VAMPIRE_PORTAL_ITEM);
static_assert(decore::itemclass::EventGiftBox == Item::ITEM_CLASS_EVENT_GIFT_BOX);
static_assert(decore::itemclass::EventStar == Item::ITEM_CLASS_EVENT_STAR);
static_assert(decore::itemclass::VampireEarring == Item::ITEM_CLASS_VAMPIRE_EARRING);
static_assert(decore::itemclass::Relic == Item::ITEM_CLASS_RELIC);
static_assert(decore::itemclass::VampireWeapon == Item::ITEM_CLASS_VAMPIRE_WEAPON);
static_assert(decore::itemclass::VampireAmulet == Item::ITEM_CLASS_VAMPIRE_AMULET);
static_assert(decore::itemclass::QuestItem == Item::ITEM_CLASS_QUEST_ITEM);
static_assert(decore::itemclass::EventTree == Item::ITEM_CLASS_EVENT_TREE);
static_assert(decore::itemclass::EventEtc == Item::ITEM_CLASS_EVENT_ETC);
static_assert(decore::itemclass::BloodBible == Item::ITEM_CLASS_BLOOD_BIBLE);
static_assert(decore::itemclass::CastleSymbol == Item::ITEM_CLASS_CASTLE_SYMBOL);
static_assert(decore::itemclass::CoupleRing == Item::ITEM_CLASS_COUPLE_RING);
static_assert(decore::itemclass::VampireCoupleRing == Item::ITEM_CLASS_VAMPIRE_COUPLE_RING);
static_assert(decore::itemclass::EventItem == Item::ITEM_CLASS_EVENT_ITEM);
static_assert(decore::itemclass::DyePotion == Item::ITEM_CLASS_DYE_POTION);
static_assert(decore::itemclass::ResurrectItem == Item::ITEM_CLASS_RESURRECT_ITEM);
static_assert(decore::itemclass::MixingItem == Item::ITEM_CLASS_MIXING_ITEM);
static_assert(decore::itemclass::OustersArmsband == Item::ITEM_CLASS_OUSTERS_ARMSBAND);
static_assert(decore::itemclass::OustersBoots == Item::ITEM_CLASS_OUSTERS_BOOTS);
static_assert(decore::itemclass::OustersChakram == Item::ITEM_CLASS_OUSTERS_CHAKRAM);
static_assert(decore::itemclass::OustersCirclet == Item::ITEM_CLASS_OUSTERS_CIRCLET);
static_assert(decore::itemclass::OustersCoat == Item::ITEM_CLASS_OUSTERS_COAT);
static_assert(decore::itemclass::OustersPendent == Item::ITEM_CLASS_OUSTERS_PENDENT);
static_assert(decore::itemclass::OustersRing == Item::ITEM_CLASS_OUSTERS_RING);
static_assert(decore::itemclass::OustersStone == Item::ITEM_CLASS_OUSTERS_STONE);
static_assert(decore::itemclass::OustersWristlet == Item::ITEM_CLASS_OUSTERS_WRISTLET);
static_assert(decore::itemclass::Larva == Item::ITEM_CLASS_LARVA);
static_assert(decore::itemclass::Pupa == Item::ITEM_CLASS_PUPA);
static_assert(decore::itemclass::ComposMei == Item::ITEM_CLASS_COMPOS_MEI);
static_assert(decore::itemclass::OustersSummonItem == Item::ITEM_CLASS_OUSTERS_SUMMON_ITEM);
static_assert(decore::itemclass::EffectItem == Item::ITEM_CLASS_EFFECT_ITEM);
static_assert(decore::itemclass::CodeSheet == Item::ITEM_CLASS_CODE_SHEET);
static_assert(decore::itemclass::MoonCard == Item::ITEM_CLASS_MOON_CARD);
static_assert(decore::itemclass::Sweeper == Item::ITEM_CLASS_SWEEPER);
static_assert(decore::itemclass::PetItem == Item::ITEM_CLASS_PET_ITEM);
static_assert(decore::itemclass::PetFood == Item::ITEM_CLASS_PET_FOOD);
static_assert(decore::itemclass::PetEnchantItem == Item::ITEM_CLASS_PET_ENCHANT_ITEM);
static_assert(decore::itemclass::LuckyBag == Item::ITEM_CLASS_LUCKY_BAG);
static_assert(decore::itemclass::SMSItem == Item::ITEM_CLASS_SMS_ITEM);
static_assert(decore::itemclass::CoreZap == Item::ITEM_CLASS_CORE_ZAP);
static_assert(decore::itemclass::GQuestItem == Item::ITEM_CLASS_GQUEST_ITEM);
static_assert(decore::itemclass::TrapItem == Item::ITEM_CLASS_TRAP_ITEM);
static_assert(decore::itemclass::BloodBibleSign == Item::ITEM_CLASS_BLOOD_BIBLE_SIGN);
static_assert(decore::itemclass::WarItem == Item::ITEM_CLASS_WAR_ITEM);
static_assert(decore::itemclass::CarryingReceiver == Item::ITEM_CLASS_CARRYING_RECEIVER);
static_assert(decore::itemclass::ShoulderArmor == Item::ITEM_CLASS_SHOULDER_ARMOR);
static_assert(decore::itemclass::Dermis == Item::ITEM_CLASS_DERMIS);
static_assert(decore::itemclass::Persona == Item::ITEM_CLASS_PERSONA);
static_assert(decore::itemclass::Fascia == Item::ITEM_CLASS_FASCIA);
static_assert(decore::itemclass::Mitten == Item::ITEM_CLASS_MITTEN);
static_assert(decore::itemclass::Count == Item::ITEM_CLASS_MAX);

bool itemClassHasGrade(Item::ItemClass itemClass) {
    return decore::gradePolicyOf(itemClass) != decore::GradePolicy::None;
}

bool itemClassHasDurability(Item::ItemClass itemClass) {
    return decore::hasDurability(itemClass);
}

ItemGradeOffsets itemClassGradeOffsets(Item::ItemClass itemClass, Grade_t grade) {
    const decore::GradeOffsets offsets = decore::gradeOffsets(decore::gradePolicyOf(itemClass), grade);
    ItemGradeOffsets result;
    result.durability = offsets.durability;
    result.damage = offsets.damage;
    result.critical = offsets.critical;
    result.defense = offsets.defense;
    result.protection = offsets.protection;
    result.luck = offsets.luck;
    return result;
}
