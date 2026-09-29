//////////////////////////////////////////////////////////////////////////////
// Filename    : PacketDispatcher.h
// Description : packet-id -> handler dispatch table, filled in at each
//               app's composition root. The kernel keeps the wire
//               classes, the app owns which handler runs.
//////////////////////////////////////////////////////////////////////////////

#ifndef __PACKET_DISPATCHER_H__
#define __PACKET_DISPATCHER_H__

#include "Packet.h"
#include "PacketMeta.h"
#include "Types.h"

class Player;

class PacketDispatcher {
public:
    typedef void (*HandlerFn)(Packet* pPacket, Player* pPlayer);

    // Call at the composition root only, before any player thread runs.
    // Asserts on an out-of-range id and on double registration.
    static void registerHandler(PacketID_t packetID, HandlerFn fn);

    // Runs the registered handler for the packet's id; receiving an id
    // with no registered handler is a protocol error and throws
    // InvalidProtocolException. The table is written only during startup,
    // so this is safe to call from every zone thread without locking.
    static void dispatch(Packet* pPacket, Player* pPlayer);

private:
    static HandlerFn s_Handlers[];
};

// Registration helpers for the composition roots: bind packet class Cls
// to Cls##Handler::execute. The _NOPLAYER form is for handlers that take
// only the packet (the inter-server directions). The id and the link come
// from the factory's compile-time metadata (PacketMeta.h).
//
// Both forms require the composition root to have stated, in scope, which
// links its server accepts:
//
//     constexpr de::packet::DirectionSet kReceivedDirections{...};
//
// Registering a handler for a packet that rides any other link is a
// compile error: that packet never arrives at this server, so the entry
// would be dead and its id would shadow nothing. So is registering one for
// a packet a narrowed link refuses (DirectionSet::narrowed): the
// gameserver's GC link is its client link, which admits only the GC-named
// packets GameClientLink.h lists.
#define DE_REGISTER_PACKET_HANDLER(Cls)                                                           \
    {                                                                                             \
        static_assert(kReceivedDirections.contains(de::packet::directionOf(Cls##Factory::kName)), \
                      "handler registered for a packet on a link this server does not receive");  \
        static_assert(kReceivedDirections.admits(de::packet::metaOf<Cls##Factory>()),             \
                      "handler registered for a packet its link's filter refuses");               \
        struct Thunk {                                                                            \
            static void call(Packet* pPacket, Player* pPlayer) {                                  \
                Cls##Handler::execute(static_cast<Cls*>(pPacket), pPlayer);                       \
            }                                                                                     \
        };                                                                                        \
        PacketDispatcher::registerHandler(Cls##Factory::kPacketID, &Thunk::call);                 \
    }

#define DE_REGISTER_PACKET_HANDLER_NOPLAYER(Cls)                                                  \
    {                                                                                             \
        static_assert(kReceivedDirections.contains(de::packet::directionOf(Cls##Factory::kName)), \
                      "handler registered for a packet on a link this server does not receive");  \
        static_assert(kReceivedDirections.admits(de::packet::metaOf<Cls##Factory>()),             \
                      "handler registered for a packet its link's filter refuses");               \
        struct Thunk {                                                                            \
            static void call(Packet* pPacket, Player*) {                                          \
                Cls##Handler::execute(static_cast<Cls*>(pPacket));                                \
            }                                                                                     \
        };                                                                                        \
        PacketDispatcher::registerHandler(Cls##Factory::kPacketID, &Thunk::call);                 \
    }

// Same check for the packets whose entry needs a hand-written function:
// a handler with a different signature, or a deliberate ignore.
#define DE_REGISTER_PACKET_HANDLER_FN(Cls, Fn)                                                    \
    {                                                                                             \
        static_assert(kReceivedDirections.contains(de::packet::directionOf(Cls##Factory::kName)), \
                      "handler registered for a packet on a link this server does not receive");  \
        static_assert(kReceivedDirections.admits(de::packet::metaOf<Cls##Factory>()),             \
                      "handler registered for a packet its link's filter refuses");               \
        PacketDispatcher::registerHandler(Cls##Factory::kPacketID, &(Fn));                        \
    }

#endif
