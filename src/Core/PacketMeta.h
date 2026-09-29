//////////////////////////////////////////////////////////////////////////////
// Filename    : PacketMeta.h
// Description : Compile-time packet factory metadata (docs/TOOLCHAIN.md section 3,
//               "Compile-time packet metadata"). Every packet factory states
//               its packet id, name and maximum body size as static
//               constexpr members; PacketFactoryType names that contract,
//               and FactoryList folds a pack of factories into a constexpr
//               table whose duplicate or out-of-range ids fail compilation.
//               The link a packet rides is parsed out of its name, so a
//               name with no known two-letter prefix fails compilation too.
//               Runtime creation stays virtual: PacketFactoryManager still
//               owns one heap factory per id and PacketFactory::createPacket
//               still allocates the packet.
//////////////////////////////////////////////////////////////////////////////

#ifndef DARKEDEN_PACKET_META_H
#define DARKEDEN_PACKET_META_H

#include <array>
#include <concepts>
#include <cstddef>

#include <initializer_list>
#include <string_view>
#include <type_traits>

#include "Packet.h"
#include "PacketFactory.h"

namespace de {

// A packet factory whose static facts are constant expressions. The
// integral_constant / bool_constant instantiations are what force the
// members to be usable at compile time: a factory that still computed its
// id or size in a virtual body would satisfy a plain `F::kPacketID` check.
template <typename F>
concept PacketFactoryType = std::derived_from<F, PacketFactory> && std::default_initializable<F> && requires {
    typename std::integral_constant<PacketID_t, F::kPacketID>;
    typename std::integral_constant<PacketSize_t, F::kMaxSize>;
    requires std::bool_constant<!F::kName.empty()>::value;
    requires std::same_as<std::remove_cvref_t<decltype(F::kName)>, std::string_view>;
};

namespace packet {

// The link a packet rides, named the way the protocol names it: the two
// letters every packet name starts with, source first, where C is the
// client, G a gameserver, L the loginserver and S the sharedserver. GM is
// the one exception to that scheme: it prefixes the server-info datagram a
// gameserver sends to the loginserver for the GM/monitor user counts.
//
// This is the packet's own fact -- which link it was defined for -- and not
// a claim about who may receive it: a handful of GC packets travel
// server-ward from the live client, and the client opens its loginserver
// connection with a CG packet. Which links a server accepts is a separate
// statement each composition root makes with DirectionSet.
enum class Direction : unsigned char { Unknown, CG, GC, CL, LC, GL, LG, GS, SG, GG, GM };

// Parses the leading two letters of a packet name. An unrecognised prefix
// is Unknown, which no registration accepts.
consteval Direction directionOf(std::string_view name) {
    if (name.size() < 2)
        return Direction::Unknown;
    const std::string_view prefix = name.substr(0, 2);
    if (prefix == "CG")
        return Direction::CG;
    if (prefix == "GC")
        return Direction::GC;
    if (prefix == "CL")
        return Direction::CL;
    if (prefix == "LC")
        return Direction::LC;
    if (prefix == "GL")
        return Direction::GL;
    if (prefix == "LG")
        return Direction::LG;
    if (prefix == "GS")
        return Direction::GS;
    if (prefix == "SG")
        return Direction::SG;
    if (prefix == "GG")
        return Direction::GG;
    if (prefix == "GM")
        return Direction::GM;
    return Direction::Unknown;
}

struct Meta;

// Narrows one link of a DirectionSet to the packets it answers true for.
using LinkFilter = bool (*)(const Meta&);

// The set of links one server accepts, stated at its composition root. One
// bit per Direction, so the dispatcher's registration check is a bit test
// the compiler folds away. A link may also carry a filter (narrowed()),
// for a server that receives only some of a link's packets: the gameserver
// receives GC packets only from its client link, which admits a few.
class DirectionSet {
public:
    consteval DirectionSet(std::initializer_list<Direction> links) : m_Bits(0), m_Filters{} {
        for (Direction link : links)
            m_Bits |= bitOf(link);
    }

    // A copy whose link admits only the packets filter answers true for.
    consteval DirectionSet narrowed(Direction link, LinkFilter filter) const {
        DirectionSet copy = *this;
        copy.m_Filters[static_cast<unsigned>(link)] = filter;
        return copy;
    }

    constexpr bool contains(Direction link) const {
        return (m_Bits & bitOf(link)) != 0;
    }

    // Whether this server receives the packet: its link is in the set and,
    // for a narrowed link, the filter admits it.
    constexpr bool admits(const Meta& meta) const;

private:
    static constexpr unsigned kLinkCount = static_cast<unsigned>(Direction::GM) + 1;

    static constexpr unsigned bitOf(Direction link) {
        return 1u << static_cast<unsigned>(link);
    }

    unsigned m_Bits;
    std::array<LinkFilter, kLinkCount> m_Filters;
};

static_assert(static_cast<unsigned>(Direction::GM) < 32, "DirectionSet holds one bit per Direction");

struct Meta {
    PacketID_t id;
    PacketSize_t maxSize;
    std::string_view name;
    // metaOf is the only production source of a Meta and always derives
    // this from the name; the default keeps a hand-built table rejected
    // until it says which link it means.
    Direction direction = Direction::Unknown;
};

constexpr bool DirectionSet::admits(const Meta& meta) const {
    if (!contains(meta.direction))
        return false;
    const LinkFilter filter = m_Filters[static_cast<unsigned>(meta.direction)];
    return filter == nullptr || filter(meta);
}

template <PacketFactoryType F> consteval Meta metaOf() {
    return Meta{F::kPacketID, F::kMaxSize, F::kName, directionOf(F::kName)};
}

// Why a registration table is rejected. validateRegistry returns the first
// violation together with the offending id rather than asserting, so the
// tests pin each rule with a hand-built table and RegistryCheck can carry
// the id into the compiler diagnostic.
enum class RegistryError { None, IdOutOfRange, DuplicateId, EmptyName, UnknownDirection };

struct RegistryVerdict {
    RegistryError error = RegistryError::None;
    PacketID_t id = 0;
};

template <std::size_t N> consteval RegistryVerdict validateRegistry(const std::array<Meta, N>& metas) {
    std::array<bool, Packet::PACKET_MAX> seen{};
    for (const Meta& meta : metas) {
        if (meta.id >= Packet::PACKET_MAX)
            return {RegistryError::IdOutOfRange, meta.id};
        if (meta.name.empty())
            return {RegistryError::EmptyName, meta.id};
        if (meta.direction == Direction::Unknown)
            return {RegistryError::UnknownDirection, meta.id};
        if (seen[meta.id])
            return {RegistryError::DuplicateId, meta.id};
        seen[meta.id] = true;
    }
    return {};
}

// Instantiated with the verdict so a failure names the rule and the packet
// id in the "in instantiation of template class" note that follows the
// static_assert message.
template <RegistryError Error, PacketID_t Id> struct RegistryCheck {
    static_assert(Error == RegistryError::None,
                  "packet factory registration rejected: the RegistryError and the packet id are the "
                  "template arguments of this RegistryCheck");
    static constexpr bool ok = true;
};

// validateRegistry carries the packet id into its diagnostic, which is
// what a wrong id needs. A name with no known link prefix needs the
// factory instead, so it gets its own per-factory check: the failure's
// "in instantiation of KnownDirection<XFactory>" note names the type.
template <PacketFactoryType F> struct KnownDirection {
    static_assert(directionOf(F::kName) != Direction::Unknown,
                  "packet factory name does not begin with a known link prefix (CG/GC/CL/LC/GL/LG/GS/SG/GG/GM); "
                  "the factory is this template's argument");
    static constexpr bool ok = true;
};

// An ordered pack of factories with its metadata table validated while
// compiling. forEach visits each factory type in pack order through a
// std::type_identity tag, which is how PacketFactoryManager instantiates
// them without this header knowing about the manager.
template <PacketFactoryType... Factories> struct FactoryList {
    static constexpr std::size_t kCount = sizeof...(Factories);
    static constexpr std::array<Meta, kCount> kMeta{metaOf<Factories>()...};
    static constexpr RegistryVerdict kVerdict = validateRegistry(kMeta);
    // Naming ::ok forces the instantiation, so the check runs with the class.
    static_assert(RegistryCheck<kVerdict.error, kVerdict.id>::ok);
    static_assert((KnownDirection<Factories>::ok && ... && true));

    template <typename Fn> static void forEach(Fn&& fn) {
        (fn(std::type_identity<Factories>{}), ...);
    }
};

template <typename... Lists> struct ConcatImpl;

template <PacketFactoryType... A> struct ConcatImpl<FactoryList<A...>> {
    using type = FactoryList<A...>;
};

template <PacketFactoryType... A, PacketFactoryType... B, typename... Rest>
struct ConcatImpl<FactoryList<A...>, FactoryList<B...>, Rest...> {
    using type = typename ConcatImpl<FactoryList<A..., B...>, Rest...>::type;
};

// FactoryList<A...> ++ FactoryList<B...> ++ ... ; the result is validated as
// one table, so ids duplicated across the parts are rejected too.
template <typename... Lists> using Concat = typename ConcatImpl<Lists...>::type;

} // namespace packet
} // namespace de

#endif // DARKEDEN_PACKET_META_H
