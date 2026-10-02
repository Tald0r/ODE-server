#ifndef DARKEDEN_CONNECTION_KEY_H
#define DARKEDEN_CONNECTION_KEY_H

#include <array>

#include "Types.h"

namespace de {
// Initial stream offset and the legacy handshake's lookup table. Streams
// borrow the table from their player; their byte-transform policy is separate.
struct ConnectionKey {
    WORD offset;
    std::array<BYTE, 512> table;
};

// Pure calculation for every received key pair; no process or socket effects.
ConnectionKey makeConnectionKey(WORD encryptKey, WORD hashKey) noexcept;
} // namespace de

#endif
