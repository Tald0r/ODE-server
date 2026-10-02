#ifndef DARKEDEN_SOCKET_STREAMS_H
#define DARKEDEN_SOCKET_STREAMS_H

#include <memory>

#include "SocketInputStream.h"
#include "SocketOutputStream.h"

namespace de {
struct SocketStreams {
    std::unique_ptr<SocketInputStream> input;
    std::unique_ptr<SocketOutputStream> output;
};

// Prepare plain streams without publishing partial state. The socket is
// borrowed and must outlive the result. A zero size omits that stream; other
// sizes are passed through unchanged. Input requires a non-null socket;
// output alone can be a memory buffer with a null socket, as before.
SocketStreams makeSocketStreams(Socket* socket, uint inputBufferSize, uint outputBufferSize);
} // namespace de

#endif
