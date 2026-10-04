#ifndef DARKEDEN_PACKET_DIAGNOSTICS_H
#define DARKEDEN_PACKET_DIAGNOSTICS_H

#include "Packet.h"
#include "Utility.h"

namespace de {

using PacketLogSink = void (*)(const char*, const char*, ...);

// Packet descriptions can contain credentials, chat, and personal information.
// Diagnostics must not change delivery or replace the handler's failure.
// The caller selects the destination's retention policy through its sink.
inline void logPacketMetadata(const char* file, const char* event, const Packet& packet,
                              PacketLogSink sink = filelog) noexcept {
    try {
        sink(file, "%s packet_id=%u body_size=%u", event, static_cast<unsigned>(packet.getPacketID()),
             static_cast<unsigned>(packet.getPacketSize()));
    } catch (...) {
    }
}

} // namespace de

#endif
