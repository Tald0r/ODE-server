#include <cstdarg>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>

#include <gtest/gtest.h>

#include "CGCrashReport.h"
#include "CGSMSSend.h"
#include "MPacketDiagnostics.h"
#include "PacketDiagnostics.h"

namespace {
class PrivatePacket : public Packet {
public:
    void read(SocketInputStream&) override {}
    void write(SocketOutputStream&) const override {}
    PacketID_t getPacketID() const override {
        return 42;
    }
    PacketSize_t getPacketSize() const override {
        if (failMetadata)
            throw std::runtime_error("metadata failure");
        return 19;
    }
    std::string getPacketName() const override {
        throw std::runtime_error("private-name-marker");
    }
    std::string toString() const override {
        throw std::runtime_error("private-payload-marker");
    }
    bool failMetadata = false;
};

unsigned explicitSinkCalls = 0;

void inspectPacketMetadata(const char* file, const char* format, ...) {
    ++explicitSinkCalls;
    EXPECT_STREQ(file, "explicit-diagnostics.log");
    char record[128];
    va_list arguments;
    va_start(arguments, format);
    const int count = std::vsnprintf(record, sizeof(record), format, arguments);
    va_end(arguments);
    ASSERT_GT(count, 0);
    ASSERT_LT(static_cast<std::size_t>(count), sizeof(record));
    EXPECT_STREQ(record, "handler failed packet_id=42 body_size=19");
}

TEST(GamePacketDiagnostics, ExplicitSinkReceivesOnlyMetadataAndTheRequestedDestination) {
    explicitSinkCalls = 0;
    PrivatePacket packet;
    de::logPacketMetadata("explicit-diagnostics.log", "handler failed", packet, inspectPacketMetadata);
    EXPECT_EQ(explicitSinkCalls, 1u);
    packet.failMetadata = true;
    EXPECT_NO_THROW(de::logPacketMetadata("explicit-diagnostics.log", "handler failed", packet, inspectPacketMetadata));
    EXPECT_EQ(explicitSinkCalls, 1u);
}

void failPacketSink(const char*, const char*, ...) {
    throw std::runtime_error("diagnostic sink failed");
}

TEST(GamePacketDiagnostics, ExplicitSinkFailureCannotReplaceAnActiveHandlerException) {
    PrivatePacket packet;
    try {
        try {
            throw DisconnectException("handler failed");
        } catch (...) {
            de::logPacketMetadata("unused.log", "handler failed", packet, failPacketSink);
            throw;
        }
    } catch (const DisconnectException& error) {
        EXPECT_EQ(error.getMessage(), "handler failed");
    }
}

TEST(GamePacketDiagnostics, EveryEventLogsOnlyMetadataWithoutCallingPacketDescriptions) {
    std::string pattern = (std::filesystem::temp_directory_path() / "packet-diagnostics-XXXXXX").string();
    ASSERT_NE(nullptr, ::mkdtemp(pattern.data()));
    const auto path = std::filesystem::path(pattern) / "packet.log";
    PrivatePacket packet;
    for (const char* event : {"receive", "send", "handler failed"})
        EXPECT_NO_THROW(de::logPacketMetadata(path.c_str(), event, packet));
    std::ifstream input(path);
    const std::string text{std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
    for (const char* event : {"receive", "send", "handler failed"})
        EXPECT_NE(text.find(std::string(event) + " packet_id=42 body_size=19"), std::string::npos);
    EXPECT_EQ(text.find("private-"), std::string::npos);
    input.close();
    std::filesystem::remove_all(pattern);
}

TEST(GamePacketDiagnostics, DiagnosticFailuresCannotReplaceAnActiveHandlerException) {
    PrivatePacket packet;
    packet.failMetadata = true;
    try {
        try {
            throw DisconnectException("handler failed");
        } catch (...) {
            de::logPacketMetadata("unused.log", "handler failed", packet);
            throw;
        }
    } catch (const DisconnectException& error) {
        EXPECT_EQ(error.getMessage(), "handler failed");
    }
}
TEST(GamePacketDiagnostics, SmsAndCrashReportsKeepMetadataWithoutPrivateFields) {
    std::string pattern = (std::filesystem::temp_directory_path() / "private-packet-diagnostics-XXXXXX").string();
    ASSERT_NE(nullptr, ::mkdtemp(pattern.data()));
    const auto path = std::filesystem::path(pattern) / "packet.log";
    CGSMSSend sms;
    sms.setCallerNumber("private-caller");
    sms.getNumbersList().push_back("private-recipient");
    sms.setMessage("private-message");
    CGCrashReport crash;
    crash.setExecutableTime("private-build");
    crash.setAddress("private-address");
    crash.setOS("private-os");
    crash.setCallStack("private-stack");
    crash.setMessage("private-crash-message");
    de::logPacketMetadata(path.c_str(), "SMS requested", sms);
    de::logPacketMetadata(path.c_str(), "crash report storage failed", crash);
    std::ifstream input(path);
    const std::string text{std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
    EXPECT_NE(text.find("SMS requested packet_id=" + std::to_string(sms.getPacketID())), std::string::npos);
    EXPECT_NE(text.find("crash report storage failed packet_id=" + std::to_string(crash.getPacketID())),
              std::string::npos);
    EXPECT_EQ(text.find("private-"), std::string::npos);
    input.close();
    std::filesystem::remove_all(pattern);
}

class PrivateMofusPacket : public MPacket {
public:
    MPacketID_t getID() const override {
        return 43;
    }
    MPacketSize_t getSize() const override {
        if (failMetadata)
            throw std::runtime_error("metadata failure");
        return 20;
    }
    MPacket* create() override {
        return nullptr;
    }
    void read(SocketInputStream&) override {}
    void write(SocketOutputStream&) override {}
    std::string toString() const override {
        throw std::runtime_error("private national ID and phone");
    }
    bool failMetadata = false;
};

TEST(GamePacketDiagnostics, MofusMetadataDoesNotEvaluatePrivateDescriptionsOrPropagateDiagnosticFailures) {
    std::string pattern = (std::filesystem::temp_directory_path() / "mofus-diagnostics-XXXXXX").string();
    ASSERT_NE(nullptr, ::mkdtemp(pattern.data()));
    const auto path = std::filesystem::path(pattern) / "packet.log";
    PrivateMofusPacket packet;
    de::logMofusPacketMetadata(path.c_str(), "send", packet);
    packet.failMetadata = true;
    EXPECT_NO_THROW(de::logMofusPacketMetadata(path.c_str(), "receive", packet));
    std::ifstream input(path);
    const std::string text{std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
    EXPECT_NE(text.find("send packet_id=43 packet_size=20"), std::string::npos);
    EXPECT_EQ(text.find("receive"), std::string::npos);
    EXPECT_EQ(text.find("private"), std::string::npos);
    input.close();
    std::filesystem::remove_all(pattern);
}

} // namespace
