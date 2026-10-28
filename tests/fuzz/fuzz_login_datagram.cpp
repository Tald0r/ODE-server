//////////////////////////////////////////////////////////////////////
//
// Filename    : fuzz_login_datagram.cpp
// Description : Fuzz target: one UDP datagram arriving at the
//               loginserver's datagram socket, read the way
//               GameServerManager::run (src/server/loginserver/
//               GameServerManager.cpp) reads it, up to the packet
//               dispatch.
//
//               Nothing authenticates a datagram: any sender that can
//               reach the port chooses every byte. The input is the
//               datagram as recvfrom() returned it: a header of id (u16)
//               and body size (u32), little-endian, the body, and the
//               one zero byte both peers count in the length
//               (szPacketHeader + body). An empty input is not run,
//               since DatagramSocket::receive hands the loop nothing for
//               an empty datagram, nor one longer than
//               DATAGRAM_SOCKET_BUFFER_LEN, the most recvfrom() takes.
//
//               The path, in order: DatagramSocket::receive (a new
//               Datagram, setData, setAddress, here a fixed sender);
//               Datagram::read (src/Core/DatagramFactoryRead.cpp: the
//               header, PACKET_MAX, the factory's size cap, the length
//               check both ways, the datagram id set, createPacket, the
//               packet's read(Datagram&), setHost and setPort). The loop
//               dispatches the packet next, which this target does not.
//
//               What counts as a crash: the loop catches every
//               Throwable, so a refusal before the packet exists (the
//               length checks throw Error) is the loop turning the
//               datagram away. Once the packet exists, its read() may
//               refuse the body only with a ProtocolException, as a
//               stream read may. A failed Assert anywhere, a
//               non-protocol exception from read() and a std::exception
//               from anywhere abort, unless DE_FUZZ_ALLOW_EXCEPTIONS=1.
//               Every refused packet is deleted, as the loop deletes it.
//
//               Not checked: how much of its body a read() consumes. A
//               datagram's reads are bounded by its length, which counts
//               the zero byte behind the body, so a read() may read that
//               byte or leave body bytes unread without being refused.
//
//               Built by tests/fuzz/CMakeLists.txt as fuzz_replay_login_
//               datagram, the replay ctest, and as fuzz_login_datagram
//               under DARKEDEN_BUILD_FUZZERS. Run it from a scratch
//               directory: the id refusal appends to datagram.txt there
//               and Assert to assertion_failed.log.
//
//////////////////////////////////////////////////////////////////////

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <arpa/inet.h>
#include <netinet/in.h>

#include "Datagram.h"
#include "DatagramPacket.h"
#include "DatagramSocket.h"
#include "StreamFuzz.h"

namespace {

bool runsDatagram(std::size_t size) {
    return size >= 1 && size <= DATAGRAM_SOCKET_BUFFER_LEN;
}

// 192.0.2.1:9999, a TEST-NET-1 sender, so every packet records the same host.
void setSender(Datagram& datagram) {
    SOCKADDR_IN address;
    std::memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(0xC0000201u);
    address.sin_port = htons(9999);
    datagram.setAddress(&address);
}

void readDatagram(const std::uint8_t* data, std::size_t size) {
    Datagram datagram;
    datagram.setData(reinterpret_cast<char*>(const_cast<std::uint8_t*>(data)), (uint)size);
    setSender(datagram);

    DatagramPacket* pRead = NULL;
    try {
        datagram.read(pRead);
    } catch (ProtocolException&) {
        delete pRead;
        return;
    } catch (AssertionError& t) {
        delete pRead;
        if (de::fuzz::options().strictExceptions) {
            std::fprintf(stderr, "fuzz harness: an assertion failed reading the datagram: %s\n", t.toString().c_str());
            std::abort();
        }
        return;
    } catch (Throwable& t) {
        const bool readThrew = pRead != NULL;
        const PacketID_t id = readThrew ? pRead->getPacketID() : 0;
        delete pRead;
        if (readThrew && de::fuzz::options().strictExceptions) {
            std::fprintf(stderr, "fuzz harness: packet %u's read threw a non-protocol exception: %s\n", (unsigned)id,
                         t.toString().c_str());
            std::abort();
        }
        return;
    } catch (...) {
        delete pRead;
        throw;
    }

    delete pRead;
}

} // namespace

bool fuzzRunsInput(std::size_t size) {
    return runsDatagram(size);
}

extern "C" int LLVMFuzzerInitialize(int*, char***) {
    de::fuzz::initialise();
    return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    if (!runsDatagram(size))
        return 0;
    de::fuzz::run([&] { readDatagram(data, size); });
    return 0;
}
