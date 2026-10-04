#ifndef DARKEDEN_MPACKET_DIAGNOSTICS_H
#define DARKEDEN_MPACKET_DIAGNOSTICS_H

#include "MPacket.h"
#include "Utility.h"

namespace de {

// Mofus payloads include national IDs and phone numbers. Keep the protocol's
// identifier/declared size, and leave transaction audits to the caller.
inline void logMofusPacketMetadata(const char* file, const char* event, const MPacket& packet) noexcept {
    try {
        filelog(file, "%s packet_id=%d packet_size=%d", event, packet.getID(), packet.getSize());
    } catch (...) {
    }
}

} // namespace de

#endif
