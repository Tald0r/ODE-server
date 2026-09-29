//////////////////////////////////////////////////////////////////////////////
// Filename    : GameClientLink.h
// Description : Which packets a game client sends the gameserver on its
//               game connection, the TCP link GamePlayer reads. The rule
//               is the packet's link prefix (PacketMeta.h), CG, with two
//               corrections the prefix cannot express: the GC-named
//               packets the live client sends server-ward, and the
//               CG-named packet it sends only as a datagram.
//
//               PacketFactoryManager.cpp folds the rule over the
//               gameserver's factory table while compiling, and
//               PacketValidator admits exactly that set in GPS_NORMAL,
//               so a new CG packet is admitted once it is registered and
//               nothing else is. GamePacketDispatch.cpp checks each GC
//               handler it registers against the list below.
//////////////////////////////////////////////////////////////////////////////

#ifndef DARKEDEN_GAME_CLIENT_LINK_H
#define DARKEDEN_GAME_CLIENT_LINK_H

#include <algorithm>
#include <array>
#include <span>

#include "CGPortCheck.h"
#include "GCAddStoreItem.h"
#include "GCFriendChatting.h"
#include "GCRemoveStoreItem.h"
#include "PacketMeta.h"

namespace de::packet {

// GC-named packets the live client sends the gameserver: the friend
// system's requests ride GCFriendChatting, and the personal-store UI sends
// GCAddStoreItem and GCRemoveStoreItem. Each has a gameserver handler
// (GamePacketDispatch.cpp); the store pair's are deliberate no-ops. The
// store UI's GCMyStoreInfo and GCOtherStoreInfo are not here: the
// gameserver refuses them.
inline constexpr std::array<PacketID_t, 3> kClientSentGCPacketIDs{
    GCAddStoreItemFactory::kPacketID,
    GCFriendChattingFactory::kPacketID,
    GCRemoveStoreItemFactory::kPacketID,
};

// CG-named packets the client sends only as a UDP datagram (Datagram's
// isDatagram), never on the game connection.
inline constexpr std::array<PacketID_t, 1> kDatagramOnlyCGPacketIDs{
    CGPortCheckFactory::kPacketID,
};

namespace detail {
template <std::size_t N> constexpr bool listed(const std::array<PacketID_t, N>& ids, PacketID_t id) {
    return std::find(ids.begin(), ids.end(), id) != ids.end();
}
} // namespace detail

// Whether a game client sends this packet on the game connection.
constexpr bool sentOnGameClientLink(const Meta& meta) {
    if (meta.direction == Direction::CG)
        return !detail::listed(kDatagramOnlyCGPacketIDs, meta.id);
    return detail::listed(kClientSentGCPacketIDs, meta.id);
}

// The ids of the gameserver's registered factories that sentOnGameClientLink
// admits, ascending. Defined in PacketFactoryManager.cpp for the gameserver
// build only, where the factory table is.
std::span<const PacketID_t> gameClientLinkPacketIDs();

} // namespace de::packet

#endif // DARKEDEN_GAME_CLIENT_LINK_H
