//////////////////////////////////////////////////////////////////////////////
// Filename    : ItemGrade.cpp
// Description :
// The grade policies and the item-class table. See ItemGrade.h.
//////////////////////////////////////////////////////////////////////////////

#include "domain/ItemGrade.h"

#include "domain/ItemClass.h"

namespace decore {

namespace {
// ArmorGrade<DefensePitch, DefenseGrade, ProtectionPitch, ProtectionGrade,
// DurabilityPitch>: defense and protection move by their pitch once every
// defenseGrade or protectionGrade grades from grade 4. The division
// truncates toward zero, so a partial step on either side of grade 4
// moves nothing.
GradeOffsets armorGrade(int grade, int defensePitch, int defenseGrade, int protectionPitch, int protectionGrade,
                        int durabilityPitch) {
    GradeOffsets offsets = {};
    offsets.durability = (grade - 4) * durabilityPitch;
    offsets.defense = (grade - 4) / defenseGrade * defensePitch;
    offsets.protection = (grade - 4) / protectionGrade * protectionPitch;
    return offsets;
}
} // namespace

GradeOffsets gradeOffsets(GradePolicy policy, int grade) {
    GradeOffsets offsets = {};

    switch (policy) {
    case GradePolicy::None:
    case GradePolicy::Plain:
        break;
    case GradePolicy::Weapon:
        offsets.durability = (grade - 4) * 1000;
        offsets.damage = (grade - 4);
        offsets.critical = (grade - 4) * 2;
        break;
    case GradePolicy::Cloth:
        offsets = armorGrade(grade, 2, 1, 1, 1, 1000);
        break;
    case GradePolicy::Grocery:
        offsets = armorGrade(grade, 1, 1, 1, 2, 500);
        break;
    case GradePolicy::Accessory:
        offsets.durability = (grade - 4) * 1000;
        offsets.luck = (grade - 4);
        break;
    }

    return offsets;
}

GradePolicy gradePolicyOf(int itemClass) {
    switch (itemClass) {
    case itemclass::Sword:
    case itemclass::Blade:
    case itemclass::Cross:
    case itemclass::SG:
    case itemclass::SMG:
    case itemclass::AR:
    case itemclass::SR:
    case itemclass::Mace:
    case itemclass::VampireWeapon:
    case itemclass::OustersChakram:
    case itemclass::OustersWristlet:
        return GradePolicy::Weapon;
    case itemclass::Coat:
    case itemclass::Trouser:
    case itemclass::VampireCoat:
    case itemclass::OustersBoots:
    case itemclass::OustersCoat:
        return GradePolicy::Cloth;
    case itemclass::Shoes:
    case itemclass::Shield:
    case itemclass::Glove:
    case itemclass::Helm:
    case itemclass::Belt:
    case itemclass::OustersArmsband:
    case itemclass::OustersCirclet:
    case itemclass::ShoulderArmor:
    case itemclass::Persona:
    case itemclass::Fascia:
    case itemclass::Mitten:
        return GradePolicy::Grocery;
    case itemclass::Ring:
    case itemclass::Bracelet:
    case itemclass::Necklace:
    case itemclass::VampireRing:
    case itemclass::VampireBracelet:
    case itemclass::VampireNecklace:
    case itemclass::VampireEarring:
    case itemclass::VampireAmulet:
    case itemclass::OustersPendent:
    case itemclass::OustersRing:
    case itemclass::OustersStone:
    case itemclass::CarryingReceiver:
    case itemclass::Dermis:
        return GradePolicy::Accessory;
    case itemclass::CoreZap:
        return GradePolicy::Plain;
    default:
        // NoGrade: Potion, DyePotion, ComposMei, EffectItem and SMSItem,
        // and every class not built on ConcreteItem.
        return GradePolicy::None;
    }
}

bool hasDurability(int itemClass) {
    switch (itemClass) {
    case itemclass::Ring:
    case itemclass::Bracelet:
    case itemclass::Necklace:
    case itemclass::Coat:
    case itemclass::Trouser:
    case itemclass::Shoes:
    case itemclass::Sword:
    case itemclass::Blade:
    case itemclass::Shield:
    case itemclass::Cross:
    case itemclass::Glove:
    case itemclass::Helm:
    case itemclass::SG:
    case itemclass::SMG:
    case itemclass::AR:
    case itemclass::SR:
    case itemclass::Belt:
    case itemclass::VampireRing:
    case itemclass::VampireBracelet:
    case itemclass::VampireNecklace:
    case itemclass::VampireCoat:
    case itemclass::Mace:
    case itemclass::VampireEarring:
    case itemclass::VampireWeapon:
    case itemclass::OustersArmsband:
    case itemclass::OustersBoots:
    case itemclass::OustersChakram:
    case itemclass::OustersCirclet:
    case itemclass::OustersCoat:
    case itemclass::OustersPendent:
    case itemclass::OustersRing:
    case itemclass::OustersStone:
    case itemclass::OustersWristlet:
    case itemclass::ShoulderArmor:
    case itemclass::Persona:
    case itemclass::Mitten:
        return true;
    default:
        // NoDurability: Potion, VampireAmulet, DyePotion, ComposMei,
        // EffectItem, SMSItem, CoreZap, CarryingReceiver, Dermis and
        // Fascia, and every class not built on ConcreteItem.
        return false;
    }
}

} // namespace decore
