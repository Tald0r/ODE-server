#include "SocketStreams.h"

#include "SocketEncryptInputStream.h"
#include "SocketEncryptOutputStream.h"

namespace de {
SocketStreams makeSocketStreams(Socket* socket, uint inputBufferSize, uint outputBufferSize) {
    SocketStreams streams;
    if (inputBufferSize != 0)
        streams.input = std::make_unique<SocketInputStream>(socket, inputBufferSize);
    if (outputBufferSize != 0)
        streams.output = std::make_unique<SocketOutputStream>(socket, outputBufferSize);
    return streams;
}

SocketStreams makeEncryptedSocketStreams(Socket* socket, uint inputBufferSize, uint outputBufferSize) {
    SocketStreams streams;
    if (inputBufferSize != 0)
        streams.input = std::make_unique<SocketEncryptInputStream>(socket, inputBufferSize);
    if (outputBufferSize != 0)
        streams.output = std::make_unique<SocketEncryptOutputStream>(socket, outputBufferSize);
    return streams;
}
} // namespace de
