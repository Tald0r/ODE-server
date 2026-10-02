#include <cstdint>
#include <string>

#include <gtest/gtest.h>

#include "Exception.h"
#include "Properties.h"
#include "ServerPortSettings.h"

namespace {

struct ValidPort {
    const char* text;
    std::uint16_t value;
};

class ServerPortTest : public ::testing::TestWithParam<ValidPort> {};

TEST_P(ServerPortTest, ReadsTheCompleteValueWithoutChangingItsSpelling) {
    Properties config;
    config.setProperty("TestPort", GetParam().text);
    EXPECT_EQ(GetParam().value, de::readServerPort(config, "TestPort"));
    EXPECT_EQ(GetParam().text, config.getProperty("TestPort"));
}

INSTANTIATE_TEST_SUITE_P(Valid, ServerPortTest,
                         ::testing::Values(ValidPort{"1", 1}, ValidPort{"65535", 65535}, ValidPort{"9998", 9998},
                                           ValidPort{"+65535", 65535}, ValidPort{"00001", 1}, ValidPort{"+09998", 9998},
                                           ValidPort{" \t123 \t", 123}, ValidPort{"9998\r", 9998},
                                           ValidPort{" \t+00080 \t\r", 80}));

class InvalidServerPortTest : public ::testing::TestWithParam<std::string> {};

TEST_P(InvalidServerPortTest, RejectsMalformedAndOutOfRangePortsWithoutMutatingConfiguration) {
    Properties config;
    config.setProperty("TestPort", GetParam());
    try {
        (void)de::readServerPort(config, "TestPort");
        FAIL() << "invalid port accepted";
    } catch (const Error& error) {
        EXPECT_NE(std::string::npos, error.toString().find("TestPort must be a decimal port from 1 to 65535"));
    }
    EXPECT_EQ(GetParam(), config.getProperty("TestPort"));
}

INSTANTIATE_TEST_SUITE_P(Invalid, InvalidServerPortTest,
                         ::testing::Values("", " \t\r", "0", "0000", "+0", "-0", "-1", "65536", "6553600", "4294967297",
                                           "18446744073709551616", "none", "9998junk", "1e3", "0x20", "12.5", "12 34",
                                           "++1", "+-1", "+", "\n123", "123\n", "\r123", std::string("123\0junk", 8)));

TEST(ServerPortSettings, AMissingPropertyNamesItsKeyWithoutProvidingADefault) {
    Properties config;
    const auto original = config.toString();
    try {
        (void)de::readServerPort(config, "MissingPort");
        FAIL() << "missing property accepted";
    } catch (const NoSuchElementException& error) {
        EXPECT_NE(std::string::npos, error.toString().find("MissingPort"));
    }
    EXPECT_EQ(original, config.toString());
    EXPECT_THROW(config.getProperty("MissingPort"), NoSuchElementException);
}

TEST(ServerPortSettings, GameRequiresItsTcpAndUdpListenersOnly) {
    Properties config;
    config.setProperty("TCPPort", "1");
    config.setProperty("GameServerUDPPort", "65535");
    config.setProperty("LoginServerPort", "unrelated");
    config.setProperty("LoginServerUDPPort", "unrelated");
    const auto original = config.toString();
    EXPECT_NO_THROW(de::validateServerListenerPorts(de::ServerKind::Game, config));
    EXPECT_EQ(original, config.toString());
}

TEST(ServerPortSettings, LoginRequiresItsTcpAndUdpListenersOnly) {
    Properties config;
    config.setProperty("LoginServerPort", "65535");
    config.setProperty("LoginServerUDPPort", "1");
    config.setProperty("TCPPort", "unrelated");
    config.setProperty("GameServerUDPPort", "unrelated");
    const auto original = config.toString();
    EXPECT_NO_THROW(de::validateServerListenerPorts(de::ServerKind::Login, config));
    EXPECT_EQ(original, config.toString());
}

TEST(ServerPortSettings, SharedRequiresOnlyItsTcpListener) {
    Properties config;
    config.setProperty("TCPPort", "9998");
    config.setProperty("GameServerUDPPort", "unrelated");
    config.setProperty("LoginServerPort", "unrelated");
    config.setProperty("LoginServerUDPPort", "unrelated");
    const auto original = config.toString();
    EXPECT_NO_THROW(de::validateServerListenerPorts(de::ServerKind::Shared, config));
    EXPECT_EQ(original, config.toString());
}

TEST(ServerPortSettings, TcpAndUdpMayUseTheSamePortNumber) {
    Properties config;
    config.setProperty("TCPPort", "9998");
    config.setProperty("GameServerUDPPort", "9998");
    EXPECT_NO_THROW(de::validateServerListenerPorts(de::ServerKind::Game, config));
}

TEST(ServerPortSettings, AnUnknownServerKindDoesNotSilentlySkipValidation) {
    Properties config;
    EXPECT_THROW(de::validateServerListenerPorts(static_cast<de::ServerKind>(99), config), Error);
}

} // namespace
