#ifndef DARKEDEN_TEST_LOOPBACK_LISTENER_H
#define DARKEDEN_TEST_LOOPBACK_LISTENER_H

#include <poll.h>
#include <unistd.h>

#include <cstdint>
#include <cstdlib>

#include <arpa/inet.h>
#include <sys/socket.h>

// No C++ allocation or gtest assertions: usable while AllocationProbe is armed
// in a child. A fixture syscall failure has a distinct exit code.
class LoopbackListener {
public:
    LoopbackListener() {
        m_Fd = ::socket(AF_INET, SOCK_STREAM, 0);
        if (m_Fd < 0)
            std::_Exit(90);
        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        socklen_t length = sizeof(address);
        if (::bind(m_Fd, reinterpret_cast<sockaddr*>(&address), length) != 0 ||
            ::getsockname(m_Fd, reinterpret_cast<sockaddr*>(&address), &length) != 0 || ::listen(m_Fd, 32) != 0)
            std::_Exit(91);
        m_Port = ntohs(address.sin_port);
    }

    ~LoopbackListener() {
        ::close(m_Fd);
    }

    LoopbackListener(const LoopbackListener&) = delete;
    LoopbackListener& operator=(const LoopbackListener&) = delete;

    std::uint16_t port() const noexcept {
        return m_Port;
    }

    int accept() const {
        pollfd ready{m_Fd, POLLIN, 0};
        if (::poll(&ready, 1, 2000) != 1 || !(ready.revents & POLLIN))
            std::_Exit(92);
        const int peer = ::accept(m_Fd, nullptr, nullptr);
        if (peer < 0)
            std::_Exit(93);
        return peer;
    }

private:
    int m_Fd;
    std::uint16_t m_Port;
};

inline int nextSocketDescriptor() {
    const int descriptor = ::socket(AF_INET, SOCK_STREAM, 0);
    if (descriptor < 0)
        std::_Exit(94);
    ::close(descriptor);
    return descriptor;
}

#endif
