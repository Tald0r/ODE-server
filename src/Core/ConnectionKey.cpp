#include "ConnectionKey.h"

namespace de {
ConnectionKey makeConnectionKey(WORD encryptKey, WORD hashKey) noexcept {
    ConnectionKey result{};
    result.offset = encryptKey % result.table.size();
    BYTE key = (hashKey + 4658) & 0x00FF;
    for (auto& byte : result.table) {
        key = (key + 0xCC) ^ (key * 0x3) ^ key;
        byte = key;
    }
    return result;
}
} // namespace de
