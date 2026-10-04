#include <cstdlib>
#include <iostream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>

#include <gtest/gtest.h>
#include <initializer_list>

#include "DiagnosticTrace.h"
#include "Packet.h"
#include "TestStreams.h"

namespace {

class TraceEnvironment {
public:
    explicit TraceEnvironment(const char* value) {
        if (const char* previous = std::getenv("DARKEDEN_TRACE"))
            m_Previous = previous;
        if (value)
            ::setenv("DARKEDEN_TRACE", value, 1);
        else
            ::unsetenv("DARKEDEN_TRACE");
    }
    ~TraceEnvironment() {
        if (m_Previous)
            ::setenv("DARKEDEN_TRACE", m_Previous->c_str(), 1);
        else
            ::unsetenv("DARKEDEN_TRACE");
    }

private:
    std::optional<std::string> m_Previous;
};

class CaptureConsole {
public:
    CaptureConsole() : m_Out(std::cout.rdbuf(output.rdbuf())), m_Error(std::cerr.rdbuf(error.rdbuf())) {}
    ~CaptureConsole() {
        std::cout.rdbuf(m_Out);
        std::cerr.rdbuf(m_Error);
    }
    std::ostringstream output;
    std::ostringstream error;

private:
    std::streambuf* m_Out;
    std::streambuf* m_Error;
};

class PrivatePacket : public Packet {
public:
    void read(SocketInputStream& input) override {
        input.read(payload, 6);
    }
    void write(SocketOutputStream& output) const override {
        output.write(payload);
    }
    PacketID_t getPacketID() const override {
        return 42;
    }
    PacketSize_t getPacketSize() const override {
        return 6;
    }
    std::string getPacketName() const override {
        throw std::runtime_error("packet description must not be evaluated");
    }
    std::string toString() const override {
        throw std::runtime_error("private packet payload must not be evaluated");
    }
    std::string payload = "secret";
};

TEST(DiagnosticTrace, DefaultAndUnrecognizedSettingsDoNotEvaluateMessages) {
    for (const char* value : std::initializer_list<const char*>{nullptr, "", "0", "true", "debug", "01"}) {
        TraceEnvironment environment(value);
        CaptureConsole console;
        bool called = false;
        de::diagnosticTrace([&](std::ostream&) {
            called = true;
            throw std::runtime_error("unused");
        });
        EXPECT_FALSE(called);
        EXPECT_TRUE(console.output.str().empty());
        EXPECT_TRUE(console.error.str().empty());
    }
}

TEST(DiagnosticTrace, EnabledMessagesAreCompleteAndWriterFailuresAreContained) {
    TraceEnvironment environment("1");
    CaptureConsole console;
    EXPECT_NO_THROW(de::diagnosticTrace([](std::ostream& output) { output << "ready"; }));
    EXPECT_NO_THROW(de::diagnosticTrace([](std::ostream& output) {
        output << "partial";
        throw std::runtime_error("diagnostic failed");
    }));
    EXPECT_NO_THROW(de::diagnosticTrace([](std::ostream& output) {
        output << "partial";
        output.setstate(std::ios::failbit);
    }));
    EXPECT_EQ(console.error.str(), "[debug] ready\n");
    EXPECT_TRUE(console.output.str().empty());
}

TEST(DiagnosticTrace, PacketTrafficIsQuietByDefaultAndTracingNeverFormatsPayloads) {
    std::vector<unsigned char> quietBytes;
    for (const char* value : std::initializer_list<const char*>{nullptr, "1"}) {
        TraceEnvironment environment(value);
        CaptureConsole console;
        PrivatePacket sent;
        PrivatePacket received;
        received.payload.clear();
        std::vector<unsigned char> bytes;
        ASSERT_NO_THROW(bytes = wiretest::writeFramed(sent, 0));
        wiretest::Loopback connection;
        ASSERT_NO_THROW(connection.out().writePacket(&sent));
        connection.pump(szPacketHeader + sent.getPacketSize());
        ASSERT_NO_THROW(connection.in().readPacket(&received));
        EXPECT_EQ(received.payload, sent.payload);
        EXPECT_TRUE(console.output.str().empty());
        if (!value) {
            quietBytes = bytes;
            EXPECT_TRUE(console.error.str().empty());
        } else {
            EXPECT_EQ(bytes, quietBytes);
            EXPECT_NE(console.error.str().find("packet send id=42 size=6 sequence=0"), std::string::npos);
            EXPECT_NE(console.error.str().find("packet receive id=42 size=6 sequence=0"), std::string::npos);
            EXPECT_EQ(console.error.str().find("secret"), std::string::npos);
        }
    }
}

class ReplaceErrorBuffer {
public:
    explicit ReplaceErrorBuffer(std::streambuf* replacement)
        : m_Previous(std::cerr.rdbuf()), m_State(std::cerr.rdstate()), m_Exceptions(std::cerr.exceptions()) {
        std::cerr.exceptions(std::ios::goodbit);
        std::cerr.rdbuf(replacement);
        std::cerr.exceptions(std::ios::badbit | std::ios::failbit);
    }
    ~ReplaceErrorBuffer() {
        std::cerr.exceptions(std::ios::goodbit);
        std::cerr.rdbuf(m_Previous);
        std::cerr.clear(m_State);
        try {
            std::cerr.exceptions(m_Exceptions);
        } catch (...) {
        }
    }

private:
    std::streambuf* m_Previous;
    std::ios::iostate m_State;
    std::ios::iostate m_Exceptions;
};

class FailingBuffer : public std::streambuf {
public:
    enum class Failure { WriteThrows, ShortWrite, FlushThrows, FlushFails, None };

    explicit FailingBuffer(Failure failure) : failure(failure) {}

    Failure failure;
    std::string text;
    int writes = 0;
    int flushes = 0;

private:
    std::streamsize xsputn(const char* data, std::streamsize size) override {
        ++writes;
        if (failure == Failure::WriteThrows)
            throw std::runtime_error("output unavailable");
        if (failure == Failure::ShortWrite)
            --size;
        text.append(data, static_cast<std::size_t>(size));
        return size;
    }
    int sync() override {
        ++flushes;
        if (failure == Failure::FlushThrows)
            throw std::runtime_error("flush unavailable");
        return failure == Failure::FlushFails ? -1 : 0;
    }
};

TEST(DiagnosticTrace, BrokenOutputPreservesPacketBytesAndErrorStreamState) {
    PrivatePacket packet;
    std::vector<unsigned char> expected;
    {
        TraceEnvironment environment(nullptr);
        expected = wiretest::writeFramed(packet, 0);
    }
    TraceEnvironment environment("1");
    for (const auto failure : {FailingBuffer::Failure::WriteThrows, FailingBuffer::Failure::ShortWrite,
                               FailingBuffer::Failure::FlushThrows, FailingBuffer::Failure::FlushFails}) {
        FailingBuffer buffer(failure);
        ReplaceErrorBuffer replacement(&buffer);
        const auto state = std::cerr.rdstate();
        const auto exceptions = std::cerr.exceptions();
        std::vector<unsigned char> actual;
        EXPECT_NO_THROW(actual = wiretest::writeFramed(packet, 0));
        EXPECT_EQ(actual, expected);
        EXPECT_EQ(buffer.writes, 1);
        EXPECT_EQ(buffer.flushes,
                  failure == FailingBuffer::Failure::WriteThrows || failure == FailingBuffer::Failure::ShortWrite ? 0
                                                                                                                  : 1);
        EXPECT_EQ(std::cerr.rdstate(), state);
        EXPECT_EQ(std::cerr.exceptions(), exceptions);

        buffer.failure = FailingBuffer::Failure::None;
        EXPECT_NO_THROW(std::cerr << "important error\n");
        EXPECT_NE(buffer.text.find("important error\n"), std::string::npos);
    }
}

} // namespace
