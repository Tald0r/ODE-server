//////////////////////////////////////////////////////////////////////
//
// Filename    : packet_meta_test.cpp
// Description : Pins the compile-time packet metadata (PacketMeta.h).
//
// Four things are checked, and the first three fail the BUILD:
//   1. every factory in the kernel satisfies de::PacketFactoryType, and
//      the whole kernel folds into one FactoryList without a duplicate
//      or out-of-range id -- the same fact wire_layout_test proves at
//      run time, now proved while compiling;
//   2. every kernel factory's name parses to a known link, and each
//      prefix parses to the link it names;
//   3. validateRegistry rejects each kind of bad table, so the assert in
//      FactoryList is known to fire, not just known to compile;
//   4. at run time, each factory's constexpr members agree with what its
//      virtual getters and its packet report. The dispatcher now
//      registers handlers by Factory::kPacketID, so the factory/packet id
//      agreement is what keeps dispatch pointed at the same handlers.
//
// This kernel-only test does not reach the per-server registration lists or
// the composition roots' DirectionSets. The separate *_server_runtime_tests
// link those production objects with their server macros and exercise dispatch.
//
//////////////////////////////////////////////////////////////////////

#include <algorithm>
#include <array>
#include <memory>
#include <string>
#include <vector>

#include <gtest/gtest.h>
#include <string_view>
#include <type_traits>

#include "Packet.h"
#include "PacketFactory.h"
#include "PacketMeta.h"

#define ALL_PACKET_FACTORIES_INCLUDES
#include "AllPacketFactories.inc"
#undef ALL_PACKET_FACTORIES_INCLUDES

namespace {

using de::PacketFactoryType;
using de::packet::Concat;
using de::packet::Direction;
using de::packet::directionOf;
using de::packet::DirectionSet;
using de::packet::FactoryList;
using de::packet::Meta;
using de::packet::RegistryError;
using de::packet::validateRegistry;

// --- 1. the kernel as one table ------------------------------------------

using KernelFactories = FactoryList<
#define ALL_PACKET_FACTORIES_TYPES
#include "AllPacketFactories.inc"
#undef ALL_PACKET_FACTORIES_TYPES
    >;

static_assert(KernelFactories::kCount > 400, "the generated factory list is unexpectedly short");
static_assert(KernelFactories::kVerdict.error == RegistryError::None,
              "two kernel factories claim one packet id, or an id is >= PACKET_MAX");

// Concept membership: a real factory is in, and the ways of being out are
// each rejected. The out-of-contract types below mirror what a factory
// looks like before this branch's edit -- id only in a virtual body.
static_assert(PacketFactoryType<CGAttackFactory>);
static_assert(PacketFactoryType<CGUseMessageItemFromInventoryFactory>, "a derived factory carries its own facts");

struct NotAFactory {
    [[maybe_unused]] static constexpr PacketID_t kPacketID = 1;
    [[maybe_unused]] static constexpr PacketSize_t kMaxSize = 1;
    [[maybe_unused]] static constexpr std::string_view kName = "NotAFactory";
};
static_assert(!PacketFactoryType<NotAFactory>, "must derive from PacketFactory");

// Not constexpr, so a value initialised from it is never a constant expression.
PacketID_t idDecidedAtRunTime() {
    return 1;
}

struct RuntimeIdFactory : PacketFactory {
    static inline const PacketID_t kPacketID = idDecidedAtRunTime();
    static constexpr PacketSize_t kMaxSize = 1;
    static constexpr std::string_view kName = "RuntimeIdFactory";
    Packet* createPacket() override {
        return nullptr;
    }
    std::string getPacketName() const override {
        return std::string(kName);
    }
    PacketID_t getPacketID() const override {
        return kPacketID;
    }
    PacketSize_t getPacketMaxSize() const override {
        return kMaxSize;
    }
};
static_assert(!PacketFactoryType<RuntimeIdFactory>, "the id must be usable at compile time");

struct UnnamedFactory : RuntimeIdFactory {
    static constexpr PacketID_t kPacketID = 1;
    static constexpr std::string_view kName = "";
};
static_assert(!PacketFactoryType<UnnamedFactory>, "the name must be non-empty");

// --- 2. the link every packet name states -------------------------------

// One per prefix, so a mis-wired branch in the parser is caught by name.
static_assert(directionOf("CGAttack") == Direction::CG);
static_assert(directionOf("GCAttack") == Direction::GC);
static_assert(directionOf("CLLogin") == Direction::CL);
static_assert(directionOf("LCLoginOK") == Direction::LC);
static_assert(directionOf("GLKickVerify") == Direction::GL);
static_assert(directionOf("LGKickCharacter") == Direction::LG);
static_assert(directionOf("GSAddGuild") == Direction::GS);
static_assert(directionOf("SGAddGuildOK") == Direction::SG);
static_assert(directionOf("GGCommand") == Direction::GG);
// The lone GM packet: the gameserver's server-info datagram, which the
// loginserver receives. It keeps its own link rather than folding into GL
// so the parse stays a plain reading of the prefix.
static_assert(directionOf("GMServerInfo") == Direction::GM);

static_assert(directionOf("") == Direction::Unknown, "a name too short to carry a prefix");
static_assert(directionOf("C") == Direction::Unknown);
static_assert(directionOf("XYMistyped") == Direction::Unknown, "an unknown prefix names no link");
static_assert(directionOf("gcLowercase") == Direction::Unknown, "the prefix is case-sensitive");

// The factories carry it, and no kernel factory is left unclassified.
// (KernelFactories would not compile if one were -- FactoryList's
// KnownDirection check names the offending factory -- so this states the
// same fact where a reader looks for it.)
static_assert(de::packet::metaOf<CGAttackFactory>().direction == Direction::CG);
static_assert(de::packet::metaOf<GMServerInfoFactory>().direction == Direction::GM);

consteval bool everyKernelFactoryHasALink() {
    for (const Meta& meta : KernelFactories::kMeta)
        if (meta.direction == Direction::Unknown)
            return false;
    return true;
}
static_assert(everyKernelFactoryHasALink());

// A DirectionSet answers only for the links it was given: this is what the
// composition roots hand DE_REGISTER_PACKET_HANDLER.
constexpr DirectionSet guildRequests{Direction::GS};
static_assert(guildRequests.contains(Direction::GS));
static_assert(!guildRequests.contains(Direction::SG), "the reply direction is not the request direction");
static_assert(!guildRequests.contains(Direction::Unknown));
constexpr DirectionSet clientAndDatagrams{Direction::CG, Direction::GC, Direction::GG, Direction::LG, Direction::SG};
static_assert(clientAndDatagrams.contains(Direction::CG));
static_assert(clientAndDatagrams.contains(Direction::SG));
static_assert(!clientAndDatagrams.contains(Direction::CL));
static_assert(!clientAndDatagrams.contains(Direction::GM));

// admits() answers for one packet: a link the set holds, filtered for a
// narrowed link. This is the second check the registration macros make,
// and how the gameserver refuses a GC handler its client link would never
// deliver.
constexpr bool onlyId7(const Meta& meta) {
    return meta.id == 7;
}
constexpr DirectionSet clientWithFewGC = clientAndDatagrams.narrowed(Direction::GC, &onlyId7);
static_assert(clientWithFewGC.admits(Meta{7, 4, "GCSeven", Direction::GC}));
static_assert(!clientWithFewGC.admits(Meta{8, 4, "GCEight", Direction::GC}), "a narrowed link admits only its filter");
static_assert(clientWithFewGC.admits(Meta{8, 4, "CGEight", Direction::CG}), "narrowing one link leaves the others");
static_assert(!clientWithFewGC.admits(Meta{7, 4, "CLSeven", Direction::CL}), "the filter never widens the set");
static_assert(clientWithFewGC.contains(Direction::GC), "a narrowed link is still received");
static_assert(clientAndDatagrams.admits(Meta{8, 4, "GCEight", Direction::GC}), "narrowed() returns a copy");
static_assert(!guildRequests.admits(Meta{1, 4, "SGOne", Direction::SG}));
static_assert(!guildRequests.admits(Meta{1, 4, "", Direction::Unknown}));

// --- 3. the rules, on hand-built tables ---------------------------------

constexpr Meta a{1, 4, "CGA", Direction::CG};
constexpr Meta b{2, 4, "GCB", Direction::GC};
constexpr Meta aAgain{1, 8, "CGA2", Direction::CG};
constexpr Meta tooHigh{static_cast<PacketID_t>(Packet::PACKET_MAX), 4, "CGHigh", Direction::CG};
constexpr Meta unnamed{3, 4, "", Direction::Unknown};
// What a mistyped kName produces: metaOf would derive exactly this.
constexpr Meta mistyped{4, 4, "XYMistyped", directionOf("XYMistyped")};

static_assert(validateRegistry(std::array<Meta, 0>{}).error == RegistryError::None);
static_assert(validateRegistry(std::array{a, b}).error == RegistryError::None);
static_assert(validateRegistry(std::array{a, b, aAgain}).error == RegistryError::DuplicateId);
static_assert(validateRegistry(std::array{a, b, aAgain}).id == 1, "the verdict names the clashing id");
static_assert(validateRegistry(std::array{a, tooHigh}).error == RegistryError::IdOutOfRange);
static_assert(validateRegistry(std::array{a, tooHigh}).id == Packet::PACKET_MAX);
static_assert(validateRegistry(std::array{a, unnamed}).error == RegistryError::EmptyName);
static_assert(validateRegistry(std::array{a, mistyped}).error == RegistryError::UnknownDirection);
static_assert(validateRegistry(std::array{a, mistyped}).id == 4, "the verdict names the unclassifiable packet");

// Concat validates the joined table: two individually valid lists that
// share an id are one invalid list. (Only the verdict is inspected here;
// naming the list's kMeta would trip its static_assert, which is the
// production behaviour and not something a test can observe.)
using Left = FactoryList<CGAttackFactory>;
using Right = FactoryList<CGMoveFactory>;
static_assert(Concat<Left, Right>::kCount == 2);
static_assert(Concat<Left, Right>::kMeta[0].id == CGAttackFactory::kPacketID);
static_assert(Concat<Left, Right>::kMeta[1].id == CGMoveFactory::kPacketID);
static_assert(validateRegistry(std::array{de::packet::metaOf<CGAttackFactory>(), de::packet::metaOf<CGAttackFactory>()})
                  .error == RegistryError::DuplicateId);

// --- 4. constexpr members agree with the virtual getters ----------------

struct Disagreement {
    std::string factory;
    std::string what;
};

template <PacketFactoryType F> void checkOne(std::vector<Disagreement>& out) {
    F factory;
    const std::string name(F::kName);
    if (factory.getPacketID() != F::kPacketID)
        out.push_back({name, "getPacketID() != kPacketID"});
    if (factory.getPacketMaxSize() != F::kMaxSize)
        out.push_back({name, "getPacketMaxSize() != kMaxSize"});
    if (factory.getPacketName() != name)
        out.push_back({name, "getPacketName() != kName"});
    std::unique_ptr<Packet> packet(factory.createPacket());
    if (!packet)
        out.push_back({name, "createPacket() returned null"});
    else if (packet->getPacketID() != F::kPacketID)
        out.push_back({name, "packet->getPacketID() != Factory::kPacketID (dispatch would miss)"});
}

TEST(PacketMeta, ConstexprMembersAgreeWithVirtuals) {
    std::vector<Disagreement> disagreements;
    KernelFactories::forEach([&]<typename F>(std::type_identity<F>) { checkOne<F>(disagreements); });
    for (const Disagreement& d : disagreements)
        ADD_FAILURE() << d.factory << ": " << d.what;
    EXPECT_TRUE(disagreements.empty());
}

// Spells a Direction back out, so the round trip below reads the parser's
// result against the name it came from and can print the packet that
// disagrees. Deliberately a second, independent table.
std::string_view spelling(Direction direction) {
    switch (direction) {
    case Direction::CG:
        return "CG";
    case Direction::GC:
        return "GC";
    case Direction::CL:
        return "CL";
    case Direction::LC:
        return "LC";
    case Direction::GL:
        return "GL";
    case Direction::LG:
        return "LG";
    case Direction::GS:
        return "GS";
    case Direction::SG:
        return "SG";
    case Direction::GG:
        return "GG";
    case Direction::GM:
        return "GM";
    case Direction::Unknown:
        break;
    }
    return "??";
}

TEST(PacketMeta, EveryPacketNameStatesItsLink) {
    for (const Meta& meta : KernelFactories::kMeta)
        EXPECT_EQ(meta.name.substr(0, 2), spelling(meta.direction)) << "packet " << meta.name;
}

TEST(PacketMeta, ForEachVisitsInPackOrder) {
    std::vector<std::string> names;
    Concat<Left, Right>::forEach([&]<typename F>(std::type_identity<F>) { names.emplace_back(F::kName); });
    ASSERT_EQ(names.size(), 2u);
    EXPECT_EQ(names[0], "CGAttack");
    EXPECT_EQ(names[1], "CGMove");
}

// The generated .inc has two sections built from one list: the REGISTER
// section (heap factories, what wire_layout_test inventories) and the TYPES
// section (this file's KernelFactories). They are only useful together if
// they name the same factories, so compare the ids the run-time section
// produces with the ids in the compile-time table.
std::vector<PacketFactory*> makeRegisteredFactories() {
    std::vector<PacketFactory*> factories;
#define ALL_PACKET_FACTORIES_REGISTER
#include "AllPacketFactories.inc"
#undef ALL_PACKET_FACTORIES_REGISTER
    return factories;
}

TEST(PacketMeta, GeneratedSectionsNameTheSameFactories) {
    std::vector<PacketFactory*> registered = makeRegisteredFactories();
    std::vector<PacketID_t> runtimeIds;
    for (PacketFactory* factory : registered)
        runtimeIds.push_back(factory->getPacketID());
    std::vector<PacketID_t> tableIds;
    for (const Meta& meta : KernelFactories::kMeta)
        tableIds.push_back(meta.id);
    std::sort(runtimeIds.begin(), runtimeIds.end());
    std::sort(tableIds.begin(), tableIds.end());
    EXPECT_EQ(runtimeIds, tableIds);
    EXPECT_EQ(registered.size(), KernelFactories::kCount);
    for (PacketFactory* factory : registered)
        delete factory;
}

} // namespace
