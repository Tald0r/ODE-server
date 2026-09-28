// concrete_item_test.cpp - pins ConcreteItem's grade and durability
// behaviour, for every item class built on it, against de-core's per-class
// table (src/domain/ItemGrade.h).
//
// ConcreteItem reads whether its class keeps a grade and a durability, and
// how far a grade moves each attribute, through the adapter functions in
// ConcreteItem.cpp. The parity vectors pin de-core's table; this pins the
// wiring on top of it: which getter takes which offset, the -1 grade and
// ignored setGrade of a class without a grade, the durability of 1 and
// ignored setDurability of a class without one, the hasDurability input of
// the maximum, the floors the getters apply, the luck narrowed to Luck_t,
// and a new item's grade of 4.
//
// Each class is probed through the exact ConcreteItem instantiation the
// real class derives from (its stack, option, attacking-stat and enchant
// policies included), deduced from the real header; the list below must
// name every class in src/server/gameserver/item/ that derives from
// ConcreteItem, which a test checks against the headers themselves.
//
// The item sources are not linked. ConcreteItem's getters reach the item
// table through de::gameContext().itemInfos(), so the test registers an
// ItemInfoManager whose getItemInfo, stubbed below, hands back the
// test's own ItemInfo; and the out-of-line members of Item, ItemInfo,
// EffectManager and ItemInfoManager that the probes' constructors and
// vtables reference are stubbed below too, as gm_console_command_test
// stubs the command bodies.

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <regex>
#include <set>
#include <string>
#include <vector>

#include <gtest/gtest.h>
#include <type_traits>

#include "ConcreteItem.h"
#include "EffectManager.h"
#include "GameContext.h"
#include "ItemInfo.h"
#include "ItemInfoManager.h"
#include "domain/ItemDurability.h"
#include "domain/ItemGrade.h"
#include "item/AR.h"
#include "item/Belt.h"
#include "item/Blade.h"
#include "item/Bracelet.h"
#include "item/CarryingReceiver.h"
#include "item/Coat.h"
#include "item/ComposMei.h"
#include "item/CoreZap.h"
#include "item/Cross.h"
#include "item/Dermis.h"
#include "item/DyePotion.h"
#include "item/EffectItem.h"
#include "item/Fascia.h"
#include "item/Glove.h"
#include "item/Helm.h"
#include "item/Mace.h"
#include "item/Mitten.h"
#include "item/Necklace.h"
#include "item/OustersArmsband.h"
#include "item/OustersBoots.h"
#include "item/OustersChakram.h"
#include "item/OustersCirclet.h"
#include "item/OustersCoat.h"
#include "item/OustersPendent.h"
#include "item/OustersRing.h"
#include "item/OustersStone.h"
#include "item/OustersWristlet.h"
#include "item/Persona.h"
#include "item/Potion.h"
#include "item/Ring.h"
#include "item/SG.h"
#include "item/SMG.h"
#include "item/SMSItem.h"
#include "item/SR.h"
#include "item/Shield.h"
#include "item/Shoes.h"
#include "item/ShoulderArmor.h"
#include "item/Sword.h"
#include "item/Trouser.h"
#include "item/VampireAmulet.h"
#include "item/VampireBracelet.h"
#include "item/VampireCoat.h"
#include "item/VampireEarring.h"
#include "item/VampireNecklace.h"
#include "item/VampireRing.h"
#include "item/VampireWeapon.h"

// Every item class built on ConcreteItem: the class and its wire item class.
#define DE_CONCRETE_ITEM_CLASSES(X)        \
    X(AR, AR)                              \
    X(Belt, BELT)                          \
    X(Blade, BLADE)                        \
    X(Bracelet, BRACELET)                  \
    X(CarryingReceiver, CARRYING_RECEIVER) \
    X(Coat, COAT)                          \
    X(ComposMei, COMPOS_MEI)               \
    X(CoreZap, CORE_ZAP)                   \
    X(Cross, CROSS)                        \
    X(Dermis, DERMIS)                      \
    X(DyePotion, DYE_POTION)               \
    X(EffectItem, EFFECT_ITEM)             \
    X(Fascia, FASCIA)                      \
    X(Glove, GLOVE)                        \
    X(Helm, HELM)                          \
    X(Mace, MACE)                          \
    X(Mitten, MITTEN)                      \
    X(Necklace, NECKLACE)                  \
    X(OustersArmsband, OUSTERS_ARMSBAND)   \
    X(OustersBoots, OUSTERS_BOOTS)         \
    X(OustersChakram, OUSTERS_CHAKRAM)     \
    X(OustersCirclet, OUSTERS_CIRCLET)     \
    X(OustersCoat, OUSTERS_COAT)           \
    X(OustersPendent, OUSTERS_PENDENT)     \
    X(OustersRing, OUSTERS_RING)           \
    X(OustersStone, OUSTERS_STONE)         \
    X(OustersWristlet, OUSTERS_WRISTLET)   \
    X(Persona, PERSONA)                    \
    X(Potion, POTION)                      \
    X(Ring, RING)                          \
    X(SG, SG)                              \
    X(SMG, SMG)                            \
    X(SMSItem, SMS_ITEM)                   \
    X(SR, SR)                              \
    X(Shield, SHIELD)                      \
    X(Shoes, SHOES)                        \
    X(ShoulderArmor, SHOULDER_ARMOR)       \
    X(Sword, SWORD)                        \
    X(Trouser, TROUSER)                    \
    X(VampireAmulet, VAMPIRE_AMULET)       \
    X(VampireBracelet, VAMPIRE_BRACELET)   \
    X(VampireCoat, VAMPIRE_COAT)           \
    X(VampireEarring, VAMPIRE_EARRING)     \
    X(VampireNecklace, VAMPIRE_NECKLACE)   \
    X(VampireRing, VAMPIRE_RING)           \
    X(VampireWeapon, VAMPIRE_WEAPON)

namespace {

// The item info every probe reads. Its values are set per check.
class TestItemInfo : public ItemInfo {
public:
    Item::ItemClass getItemClass() const override {
        return m_ItemClass;
    }
    string toString() const override {
        return "TestItemInfo";
    }
    Durability_t getDurability() const override {
        return m_Durability;
    }
    Damage_t getMinDamage() const override {
        return m_MinDamage;
    }
    Damage_t getMaxDamage() const override {
        return m_MaxDamage;
    }
    int getCriticalBonus(void) const override {
        return m_CriticalBonus;
    }
    Defense_t getDefenseBonus() const override {
        return m_DefenseBonus;
    }
    Protection_t getProtectionBonus() const override {
        return m_ProtectionBonus;
    }

    Item::ItemClass m_ItemClass = Item::ITEM_CLASS_MAX;
    Durability_t m_Durability = 0;
    Damage_t m_MinDamage = 0;
    Damage_t m_MaxDamage = 0;
    int m_CriticalBonus = 0;
    Defense_t m_DefenseBonus = 0;
    Protection_t m_ProtectionBonus = 0;
};

TestItemInfo g_itemInfo;

} // namespace

//////////////////////////////////////////////////////////////////////////////
// Stubs for the members the item sources would define. None of them is
// exercised except ItemInfoManager::getItemInfo.
//////////////////////////////////////////////////////////////////////////////
Item::Item() {}
Item::~Item() {}
bool Item::destroy() {
    return false;
}
const list<OptionType_t>& Item::getDefaultOptions(void) const {
    static const list<OptionType_t> none;
    return none;
}
void Item::makePCItemInfo(PCItemInfo&) const {}
void Item::whenPCTake(PlayerCreature*) {}
void Item::whenPCLost(PlayerCreature*) {}

ItemInfo::ItemInfo() {}
ItemInfo::~ItemInfo() {}
VolumeWidth_t ItemInfo::getVolumeWidth() const {
    return 1;
}
VolumeHeight_t ItemInfo::getVolumeHeight() const {
    return 1;
}
void ItemInfo::setReqAbility(const string&) {}
void ItemInfo::setDefaultOptions(const string&) {}

EffectManager::EffectManager() {}
EffectManager::~EffectManager() {}

ItemInfoManager::ItemInfoManager() {}
ItemInfoManager::~ItemInfoManager() {}
ItemInfo* ItemInfoManager::getItemInfo(Item::ItemClass, ItemType_t) const {
    return &g_itemInfo;
}

namespace {

// The ConcreteItem instantiation a real item class derives from.
template <Item::ItemClass C, typename... Policies>
ConcreteItem<C, Policies...>* concreteBaseOf(ConcreteItem<C, Policies...>*);
template <typename Real>
using ConcreteBase = std::remove_pointer_t<decltype(concreteBaseOf(static_cast<Real*>(nullptr)))>;

template <Item::ItemClass C, typename... Policies>
constexpr Item::ItemClass concreteClassOf(ConcreteItem<C, Policies...>*) {
    return C;
}

// Each listed class is built on ConcreteItem for its own wire item class.
#define DE_CHECK_CONCRETE_CLASS(Real, CLASS) \
    static_assert(concreteClassOf(static_cast<Real*>(nullptr)) == Item::ITEM_CLASS_##CLASS, #Real);
DE_CONCRETE_ITEM_CLASSES(DE_CHECK_CONCRETE_CLASS)
#undef DE_CHECK_CONCRETE_CLASS

// A real class's ConcreteItem base with the persistence members filled in,
// so it can be built without the class's own source.
template <typename Base> class Probe final : public Base {
public:
    Probe() {
        this->setBonusDamage(0);
    }
    string toString() const override {
        return "Probe";
    }
    void create(const string&, Storage, DWORD, BYTE, BYTE, ItemID_t) override {}
    void save(const string&, Storage, DWORD, BYTE, BYTE) override {}
    void tinysave(const string&) const override {}
};

// The grades every class is probed at: each side of 4, both far ends of
// the linear formulas, and the half step of the grocery protection.
const std::vector<int> kGrades = {-5, -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 20};

struct InfoValues {
    Durability_t durability;
    Damage_t minDamage;
    Damage_t maxDamage;
    int critical;
    Defense_t defense;
    Protection_t protection;
};

// Large enough that no grade reaches a floor, and small enough that every
// negative offset does.
const InfoValues kRoomy = {20000, 5000, 7000, 3000, 4000, 4500};
const InfoValues kTight = {0, 0, 0, 0, 0, 0};

void setInfo(Item::ItemClass itemClass, const InfoValues& values) {
    g_itemInfo.m_ItemClass = itemClass;
    g_itemInfo.m_Durability = values.durability;
    g_itemInfo.m_MinDamage = values.minDamage;
    g_itemInfo.m_MaxDamage = values.maxDamage;
    g_itemInfo.m_CriticalBonus = values.critical;
    g_itemInfo.m_DefenseBonus = values.defense;
    g_itemInfo.m_ProtectionBonus = values.protection;
}

// Checks one class against the table, independently of the adapter: the
// expectations call de-core directly.
template <typename Real> void checkClass(const char* name, Item::ItemClass itemClass) {
    SCOPED_TRACE(name);
    using Item_ = Probe<ConcreteBase<Real>>;
    const decore::GradePolicy policy = decore::gradePolicyOf(itemClass);
    const bool hasGrade = policy != decore::GradePolicy::None;
    const bool hasDurability = decore::hasDurability(itemClass);

    Item_ fresh;
    EXPECT_EQ(itemClass, fresh.getItemClass());
    EXPECT_EQ(hasGrade ? 4 : -1, fresh.getGrade()) << "a new item's grade";
    if (!hasDurability)
        EXPECT_EQ(1u, fresh.getDurability()) << "a class without durability reads 1";

    Item_ item;
    item.setDurability(777);
    EXPECT_EQ(hasDurability ? 777u : 1u, item.getDurability()) << "setDurability";

    for (const InfoValues& info : {kRoomy, kTight}) {
        setInfo(itemClass, info);
        for (int grade : kGrades) {
            SCOPED_TRACE("grade " + std::to_string(grade) + (info.durability == 0 ? ", tight" : ", roomy"));
            item.setGrade(grade);
            const int readGrade = hasGrade ? grade : -1;
            EXPECT_EQ(readGrade, item.getGrade()) << "setGrade";

            const decore::GradeOffsets offsets = decore::gradeOffsets(policy, readGrade);
            EXPECT_EQ((Luck_t)offsets.luck, item.getLuck());
            EXPECT_EQ(std::max(1, (int)info.minDamage + offsets.damage), (int)item.getMinDamage());
            EXPECT_EQ(std::max(1, (int)info.maxDamage + offsets.damage), (int)item.getMaxDamage());
            EXPECT_EQ(std::max(0, info.critical + offsets.critical), item.getCriticalBonus());
            EXPECT_EQ(std::max(0, (int)info.defense + offsets.defense), (int)item.getDefenseBonus());
            EXPECT_EQ(std::max(0, (int)info.protection + offsets.protection), (int)item.getProtectionBonus());
            EXPECT_EQ((Durability_t)decore::maxDurabilityBase(info.durability, hasDurability, offsets.durability),
                      item.getMaxDurability());
        }
    }
}

class ConcreteItemTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        static ItemInfoManager manager;
        de::gameContext().setItemInfoManager(&manager);
    }
};

} // namespace

// Every class built on ConcreteItem follows de-core's table for its grade,
// its durability and every offset a grade gives.
TEST_F(ConcreteItemTest, EveryClassFollowsTheGradeAndDurabilityTable){
#define DE_CHECK_CLASS(Real, CLASS) checkClass<Real>(#Real, Item::ITEM_CLASS_##CLASS);
    DE_CONCRETE_ITEM_CLASSES(DE_CHECK_CLASS)
#undef DE_CHECK_CLASS
}

// A few rows spelled out, so a change to the table that the sweep above
// follows still shows up here as the balance change it is.
TEST_F(ConcreteItemTest, TheKeptOdditiesReadAsTheServerHadThem) {
    setInfo(Item::ITEM_CLASS_POTION, kRoomy);
    Probe<ConcreteBase<Potion>> potion;
    potion.setGrade(7);
    potion.setDurability(500);
    EXPECT_EQ(-1, potion.getGrade());
    EXPECT_EQ(1u, potion.getDurability());

    // A sword moves 1 damage, 2 critical and 1000 durability a grade.
    setInfo(Item::ITEM_CLASS_SWORD, kRoomy);
    Probe<ConcreteBase<Sword>> sword;
    EXPECT_EQ(4, sword.getGrade());
    sword.setGrade(6);
    EXPECT_EQ(5002, sword.getMinDamage());
    EXPECT_EQ(3004, sword.getCriticalBonus());
    EXPECT_EQ(22000u, sword.getMaxDurability());

    // CoreZap keeps a grade that moves nothing, and no durability.
    setInfo(Item::ITEM_CLASS_CORE_ZAP, kRoomy);
    Probe<ConcreteBase<CoreZap>> coreZap;
    coreZap.setGrade(9);
    EXPECT_EQ(9, coreZap.getGrade());
    EXPECT_EQ(4000, coreZap.getDefenseBonus());
    EXPECT_EQ(20000u, coreZap.getMaxDurability());

    // Fascia moves like the grocery armor but keeps no durability: its
    // maximum is the item info's durability whatever the grade.
    setInfo(Item::ITEM_CLASS_FASCIA, kRoomy);
    Probe<ConcreteBase<Fascia>> fascia;
    fascia.setGrade(8);
    EXPECT_EQ(4004, fascia.getDefenseBonus());
    EXPECT_EQ(4502, fascia.getProtectionBonus());
    EXPECT_EQ(20000u, fascia.getMaxDurability());
    fascia.setDurability(300);
    EXPECT_EQ(1u, fascia.getDurability());
}

// The list above names every class in src/server/gameserver/item/ that
// derives from ConcreteItem, so a new one cannot miss the sweep.
TEST(ConcreteItemClasses, TheListNamesEveryConcreteItemClass) {
    std::set<std::string> listed;
#define DE_LIST_CLASS(Real, CLASS) listed.insert(#Real);
    DE_CONCRETE_ITEM_CLASSES(DE_LIST_CLASS)
#undef DE_LIST_CLASS

    std::set<std::string> found;
    const std::regex derivation("class\\s+(\\w+)\\s*:\\s*public\\s+ConcreteItem\\s*<");
    for (const auto& entry : std::filesystem::directory_iterator(CONCRETE_ITEM_HEADER_DIR)) {
        if (entry.path().extension() != ".h")
            continue;
        std::ifstream in(entry.path());
        const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        for (std::sregex_iterator it(text.begin(), text.end(), derivation), end; it != end; ++it)
            found.insert((*it)[1].str());
    }
    EXPECT_EQ(found, listed);
    EXPECT_EQ(46u, listed.size());
}
