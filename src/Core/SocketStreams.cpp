#include "SocketStreams.h"

namespace de {
SocketStreams makeSocketStreams(Socket* socket, uint inputBufferSize, uint outputBufferSize) {
    SocketStreams streams;
    if (inputBufferSize != 0)
        streams.input = std::make_unique<SocketInputStream>(socket, inputBufferSize);
    if (outputBufferSize != 0)
        streams.output = std::make_unique<SocketOutputStream>(socket, outputBufferSize);
    return streams;
}
} // namespace de
