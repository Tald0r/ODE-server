#include <array>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <limits>
#include <new>
#include <sstream>
#include <stdexcept>
#include <vector>

#include <gtest/gtest.h>

#include "DatabaseError.h"
#include "Datagram.h"
#include "LGKickCharacter.h"
#include "LoginDatagramSend.h"
#include "support/AllocationProbe.h"

namespace {

LGKickCharacter kickPacket() {
    LGKickCharacter packet;
    packet.setID(1234);
    packet.setPCName("Rowan");
    return packet;
}

TEST(LoginDatagramSend, InvalidPortsAreRefusedBeforeSending) {
    const auto packet = kickPacket();
    for (const uint port : {0u, 65536u, 131073u, std::numeric_limits<uint>::max()}) {
        std::ostringstream errors;
        unsigned sends = 0;
        const bool sent = de::sendLoginDatagram(
            "127.0.0.1", port, packet,
            [&](Datagram& datagram) {
                ++sends;
                return datagram.getLength();
            },
            errors);
        EXPECT_FALSE(sent) << port;
        EXPECT_EQ(sends, 0u) << port;
    }
}

TEST(LoginDatagramSend, InvalidAddressesCannotTurnIntoBroadcastOrATruncatedAddress) {
    const auto packet = kickPacket();
    for (const std::string& host : {std::string("bad address"), std::string("localhost"), std::string("127.1"),
                                    std::string("127.0.0.1\0extra", 15)}) {
        std::ostringstream errors;
        unsigned sends = 0;
        const bool sent = de::sendLoginDatagram(
            host, 9992, packet,
            [&](Datagram& datagram) {
                ++sends;
                return datagram.getLength();
            },
            errors);
        EXPECT_FALSE(sent);
        EXPECT_EQ(sends, 0u);
    }
}

TEST(LoginDatagramSend, ZeroShortAndOversizedSendCountsAreFailures) {
    const auto packet = kickPacket();
    const uint size = szPacketHeader + packet.getPacketSize();
    for (const uint returned : {0u, size - 1, size + 1, std::numeric_limits<uint>::max()}) {
        std::ostringstream errors;
        EXPECT_FALSE(de::sendLoginDatagram("127.0.0.1", 9992, packet, [&](Datagram&) { return returned; }, errors));
    }
}

class ThrowingOutput : public std::streambuf {
    int_type overflow(int_type) override {
        throw std::runtime_error("report failed");
    }
};

TEST(LoginDatagramSend, AFailedReportCannotReplaceTheSendFailure) {
    const auto packet = kickPacket();
    ThrowingOutput buffer;
    std::ostream errors(&buffer);
    errors.exceptions(std::ios::badbit | std::ios::failbit);
    bool sent = true;
    EXPECT_NO_THROW(
        sent = de::sendLoginDatagram(
            "127.0.0.1", 9992, packet, [](Datagram&) -> uint { throw ConnectException("send failed"); }, errors));
    EXPECT_FALSE(sent);
}

class ObservedPacket : public LGKickCharacter {
public:
    ObservedPacket() {
        setID(1234);
        setPCName("Rowan");
    }
    PacketID_t getPacketID() const override {
        visit(0);
        return LGKickCharacter::getPacketID();
    }
    PacketSize_t getPacketSize() const override {
        visit(1);
        return LGKickCharacter::getPacketSize();
    }
    void write(Datagram& datagram) const override {
        // Failure here owns a partially serialized frame, including real bytes.
        LGKickCharacter::write(datagram);
        visit(2);
    }
    void visit(unsigned stage) const {
        ++visits[stage];
        if (failure && failureStage == stage)
            std::rethrow_exception(failure);
    }
    unsigned failureStage = 0;
    std::exception_ptr failure;
    mutable std::array<unsigned, 3> visits{};
};

TEST(LoginDatagramSend, InvalidEndpointsAreRefusedBeforeReadingAnyPacketMetadata) {
    for (const bool invalidPort : {false, true}) {
        ObservedPacket packet;
        std::ostringstream errors;
        unsigned sends = 0;
        EXPECT_FALSE(de::sendLoginDatagram(
            invalidPort ? "127.0.0.1" : "invalid", invalidPort ? 0 : 9992, packet,
            [&](Datagram& datagram) {
                ++sends;
                return datagram.getLength();
            },
            errors));
        EXPECT_EQ(packet.visits, (std::array<unsigned, 3>{}));
        EXPECT_EQ(sends, 0u);
        EXPECT_NE(errors.str().find(invalidPort ? "port" : "host"), std::string::npos);
    }
}

TEST(LoginDatagramSend, AddressParsingRejectsIncompleteAndAmbiguousText) {
    const std::vector<std::string> invalid = {"",
                                              " 127.0.0.1",
                                              "127.0.0.1 ",
                                              "127.0.0.1\n",
                                              "127.0.0.1:9992",
                                              "127.000.000.001",
                                              "127.0.0.010",
                                              "2130706433",
                                              "0x7f000001",
                                              "256.0.0.1",
                                              "1.2.3.4.5",
                                              "1.2.3.-4",
                                              "1.2.3.+4",
                                              "1.2.3",
                                              "::1",
                                              "::ffff:127.0.0.1",
                                              std::string("127.0.0.1\0", 10)};
    const auto packet = kickPacket();
    for (const auto& host : invalid) {
        SCOPED_TRACE(host);
        std::ostringstream errors;
        unsigned sends = 0;
        EXPECT_FALSE(de::sendLoginDatagram(
            host, 9992, packet,
            [&](Datagram& datagram) {
                ++sends;
                return datagram.getLength();
            },
            errors));
        EXPECT_EQ(sends, 0u);
    }
}

TEST(LoginDatagramSend, BoundaryPortsAndCompleteIPv4AddressesReachTheTransportUnchanged) {
    const auto packet = kickPacket();
    for (const std::string host : {"0.0.0.0", "127.0.0.1", "192.0.2.255", "255.255.255.255"}) {
        for (const uint port : {1u, 65535u}) {
            std::ostringstream errors;
            unsigned sends = 0;
            EXPECT_TRUE(de::sendLoginDatagram(
                host, port, packet,
                [&](Datagram& datagram) {
                    EXPECT_EQ(datagram.getHost(), host);
                    EXPECT_EQ(datagram.getPort(), port);
                    EXPECT_EQ(datagram.getAddress()->sa_family, AF_INET);
                    ++sends;
                    return datagram.getLength();
                },
                errors));
            EXPECT_EQ(sends, 1u);
            EXPECT_TRUE(errors.str().empty());
        }
    }
}

TEST(LoginDatagramSend, FramesPreservePacketIdentityBodyAndLengthAtTheNameBoundaries) {
    for (const uint id : {0u, std::numeric_limits<uint>::max()}) {
        for (const unsigned length : {1u, 20u}) {
            auto packet = kickPacket();
            packet.setID(id);
            packet.setPCName(std::string(length, 'c'));
            std::ostringstream errors;
            EXPECT_TRUE(de::sendLoginDatagram(
                "192.0.2.1", 9992, packet,
                [&](Datagram& datagram) {
                    PacketID_t packetID;
                    PacketSize_t bodySize;
                    std::memcpy(&packetID, datagram.getData(), szPacketID);
                    std::memcpy(&bodySize, datagram.getData() + szPacketID, szPacketSize);
                    EXPECT_EQ(packetID, Packet::PACKET_LG_KICK_CHARACTER);
                    EXPECT_EQ(bodySize, szuint + szBYTE + length);
                    EXPECT_EQ(datagram.getLength(), szPacketHeader + bodySize);
                    Datagram body;
                    body.setData(datagram.getData() + szPacketID + szPacketSize, bodySize);
                    LGKickCharacter decoded;
                    decoded.read(body);
                    EXPECT_EQ(decoded.getID(), id);
                    EXPECT_EQ(decoded.getPCName(), std::string(length, 'c'));
                    return datagram.getLength();
                },
                errors));
        }
    }
}

TEST(LoginDatagramSend, ThrowableFailuresAtEveryStageAreReportedAndCanRetry) {
    const std::array failures{std::make_exception_ptr(ConnectException("original failure")),
                              std::make_exception_ptr(InvalidProtocolException("original failure")),
                              std::make_exception_ptr(Error("original failure"))};
    for (unsigned stage = 0; stage < 4; ++stage) {
        for (const auto& failure : failures) {
            SCOPED_TRACE(stage);
            ObservedPacket packet;
            packet.failure = failure;
            packet.failureStage = stage;
            unsigned sends = 0;
            std::ostringstream errors;
            bool failing = true;
            const de::LoginDatagramTransport transport = [&](Datagram& datagram) {
                ++sends;
                if (failing && stage == 3)
                    std::rethrow_exception(failure);
                return datagram.getLength();
            };
            EXPECT_FALSE(de::sendLoginDatagram("127.0.0.1", 9992, packet, transport, errors));
            EXPECT_EQ(sends, stage == 3 ? 1u : 0u);
            EXPECT_NE(errors.str().find("original failure"), std::string::npos);
            packet.failure = nullptr;
            failing = false;
            EXPECT_TRUE(de::sendLoginDatagram("127.0.0.1", 9992, packet, transport, errors));
            EXPECT_EQ(sends, stage == 3 ? 2u : 1u);
        }
    }
}

TEST(LoginDatagramSend, OtherExceptionsAtEveryStagePropagateWithTheirOriginalIdentity) {
    const std::array failures{std::make_exception_ptr(std::runtime_error("original failure")),
                              std::make_exception_ptr(DatabaseError("original failure")),
                              std::make_exception_ptr(std::bad_alloc{})};
    for (unsigned stage = 0; stage < 4; ++stage) {
        for (const auto& failure : failures) {
            SCOPED_TRACE(stage);
            ObservedPacket packet;
            packet.failure = failure;
            packet.failureStage = stage;
            unsigned sends = 0;
            std::ostringstream errors;
            std::exception_ptr caught;
            try {
                (void)de::sendLoginDatagram(
                    "127.0.0.1", 9992, packet,
                    [&](Datagram& datagram) {
                        ++sends;
                        if (stage == 3)
                            std::rethrow_exception(failure);
                        return datagram.getLength();
                    },
                    errors);
            } catch (...) {
                caught = std::current_exception();
            }
            EXPECT_EQ(caught, failure);
            EXPECT_EQ(sends, stage == 3 ? 1u : 0u);
            EXPECT_TRUE(errors.str().empty());
        }
    }
}

class FailingFormatter : public ConnectException {
public:
    std::string toString() const override {
        throw std::runtime_error("format failed");
    }
};

TEST(LoginDatagramSend, AThrowingErrorFormatterStillReturnsFailure) {
    const auto packet = kickPacket();
    std::ostringstream errors;
    EXPECT_FALSE(
        de::sendLoginDatagram("127.0.0.1", 9992, packet, [](Datagram&) -> uint { throw FailingFormatter(); }, errors));
}

TEST(LoginDatagramSend, APreviouslyFailedDiagnosticStreamDoesNotAffectASuccessfulSend) {
    const auto packet = kickPacket();
    ThrowingOutput buffer;
    std::ostream errors(&buffer);
    errors.exceptions(std::ios::badbit | std::ios::failbit);
    EXPECT_FALSE(de::sendLoginDatagram(
        "127.0.0.1", 9992, packet, [](Datagram&) -> uint { throw ConnectException("send failed"); }, errors));
    ASSERT_TRUE(errors.fail());
    EXPECT_TRUE(de::sendLoginDatagram(
        "127.0.0.1", 9992, packet, [](Datagram& datagram) { return datagram.getLength(); }, errors));
}

TEST(LoginDatagramSend, IncompleteSendIsReportedOnceWithoutSendingASecondDatagram) {
    const auto packet = kickPacket();
    std::ostringstream errors;
    unsigned sends = 0;
    EXPECT_FALSE(de::sendLoginDatagram(
        "127.0.0.1", 9992, packet,
        [&](Datagram&) {
            ++sends;
            return 0u;
        },
        errors));
    EXPECT_EQ(sends, 1u);
    EXPECT_NE(errors.str().find("complete frame"), std::string::npos);
}

class DiscardOutput : public std::streambuf {
    int_type overflow(int_type value) override {
        return traits_type::not_eof(value);
    }
    std::streamsize xsputn(const char*, std::streamsize size) override {
        return size;
    }
};

class CleanupObserver : public ConnectException {
public:
    CleanupObserver(AllocationProbe*& probe, std::size_t& observed) : m_Probe(probe), m_Observed(observed) {}
    std::string toString() const override {
        m_Observed = m_Probe->outstanding();
        return {};
    }

private:
    AllocationProbe*& m_Probe;
    std::size_t& m_Observed;
};

TEST(LoginDatagramSend, TheFailedFrameIsReleasedBeforeErrorReporting) {
    ASSERT_EXIT(
        {
            const auto packet = kickPacket();
            DiscardOutput buffer;
            std::ostream errors(&buffer);
            AllocationProbe* active = nullptr;
            std::size_t observed = 99;
            const auto failure = std::make_exception_ptr(CleanupObserver(active, observed));
            const de::LoginDatagramTransport transport = [&](Datagram&) -> uint { std::rethrow_exception(failure); };
            AllocationProbe probe;
            active = &probe;
            const bool sent = de::sendLoginDatagram("127.0.0.1", 9992, packet, transport, errors);
            std::_Exit(!sent && observed == 0 && probe.outstanding() == 0 ? 0 : 1);
        },
        ::testing::ExitedWithCode(0), "");
}

int checkAllocationFailure(std::size_t failAt, int mode) {
    auto packet = kickPacket();
    packet.setPCName(std::string(mode == 2 ? 21 : 20, 'c'));
    DiscardOutput buffer;
    std::ostream errors(&buffer);
    const auto failure = std::make_exception_ptr(ConnectException("transport failed"));
    unsigned sends = 0;
    bool failing = true;
    const de::LoginDatagramTransport transport = [&](Datagram& datagram) {
        ++sends;
        if (failing && mode == 3)
            std::rethrow_exception(failure);
        return datagram.getLength();
    };
    const uint port = mode == 1 ? 65536u : 9992u;
    (void)de::sendLoginDatagram("127.0.0.1", port, packet, transport, errors);
    sends = 0;
    AllocationProbe probe(failAt);
    bool allocationFailed = false;
    bool sent = false;
    try {
        sent = de::sendLoginDatagram("127.0.0.1", port, packet, transport, errors);
    } catch (const std::bad_alloc&) {
        allocationFailed = true;
    }
    probe.stopFailing();
    if ((allocationFailed && !probe.rejected()) || (failAt == 64 && probe.rejected()) || probe.outstanding() != 0)
        return 1;
    if (mode == 0) {
        if (allocationFailed != probe.rejected() || sent == allocationFailed || sends != (sent ? 1u : 0u))
            return 2;
    } else if (sent || (mode != 3 && sends != 0)) {
        return 3;
    }
    failing = false;
    packet.setPCName("Rowan");
    sends = 0;
    if (!de::sendLoginDatagram("127.0.0.1", 9992, packet, transport, errors) || sends != 1)
        return 4;
    return probe.outstanding() == 0 ? 0 : 5;
}

TEST(LoginDatagramSend, AllocationFailuresReleaseEveryPartialFrameAndPermitRetry) {
    for (int mode = 0; mode < 4; ++mode) {
        for (std::size_t failAt = 1; failAt <= 64; ++failAt) {
            SCOPED_TRACE(::testing::Message() << mode << "/" << failAt);
            ASSERT_EXIT(std::_Exit(checkAllocationFailure(failAt, mode)), ::testing::ExitedWithCode(0), "");
        }
    }
}

TEST(LoginDatagramSend, RepeatedSuccessfulAndFailedSendsReleaseEveryFrame) {
    ASSERT_EXIT(
        {
            const auto packet = kickPacket();
            DiscardOutput buffer;
            std::ostream errors(&buffer);
            bool fail = false;
            const de::LoginDatagramTransport transport = [&](Datagram& datagram) {
                return fail ? 0u : datagram.getLength();
            };
            AllocationProbe probe;
            for (int attempt = 0; attempt < 100; ++attempt) {
                fail = (attempt % 2) != 0;
                const bool sent = de::sendLoginDatagram("127.0.0.1", 9992, packet, transport, errors);
                if (sent == fail || probe.outstanding() != 0)
                    std::_Exit(1);
            }
            std::_Exit(0);
        },
        ::testing::ExitedWithCode(0), "");
}

} // namespace
