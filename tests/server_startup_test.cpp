#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>
#include <vector>

#include <gtest/gtest.h>
#include <initializer_list>

#include "KernelContext.h"
#include "ServerStartup.h"

namespace {

de::ServerOptions parse(de::ServerKind server, std::initializer_list<const char*> arguments) {
    std::vector<const char*> argv{"server"};
    argv.insert(argv.end(), arguments.begin(), arguments.end());
    return de::parseServerOptions(server, static_cast<int>(argv.size()), argv.data());
}

class ServerOptionsTest : public ::testing::TestWithParam<de::ServerKind> {};

TEST_P(ServerOptionsTest, ReadsTheConfigPathWithoutOpeningIt) {
    const auto options = parse(GetParam(), {"-f", "/missing/a configuration.conf"});
    EXPECT_EQ("/missing/a configuration.conf", options.configFile);
    EXPECT_FALSE(options.loginIDOffset.has_value());
}

TEST_P(ServerOptionsTest, RejectsMissingArguments) {
    EXPECT_THROW(parse(GetParam(), {}), Error);
    EXPECT_THROW(parse(GetParam(), {"-f"}), Error);
    EXPECT_THROW(de::parseServerOptions(GetParam(), 0, nullptr), Error);
}

TEST_P(ServerOptionsTest, RejectsAnEmptyOrNullConfigPath) {
    EXPECT_THROW(parse(GetParam(), {"-f", ""}), Error);
    EXPECT_THROW(parse(GetParam(), {"-f", nullptr}), Error);
}

TEST_P(ServerOptionsTest, RejectsAnUnknownFlag) {
    EXPECT_THROW(parse(GetParam(), {"-x", "server.conf"}), Error);
    EXPECT_THROW(parse(GetParam(), {nullptr, "server.conf"}), Error);
}

TEST_P(ServerOptionsTest, RejectsIncompleteOrExtraOptions) {
    EXPECT_THROW(parse(GetParam(), {"-f", "server.conf", "-i"}), Error);
    EXPECT_THROW(parse(GetParam(), {"-f", "server.conf", "-p", "1000"}), Error);
    EXPECT_THROW(parse(GetParam(), {"-f", "server.conf", "-i", "2", "extra"}), Error);
}

INSTANTIATE_TEST_SUITE_P(Servers, ServerOptionsTest,
                         ::testing::Values(de::ServerKind::Game, de::ServerKind::Login, de::ServerKind::Shared));

TEST(ServerOptions, OnlyLoginAcceptsAnIDOffset) {
    EXPECT_THROW(parse(de::ServerKind::Game, {"-f", "game.conf", "-i", "1"}), Error);
    EXPECT_THROW(parse(de::ServerKind::Shared, {"-f", "shared.conf", "-i", "1"}), Error);
    const auto options = parse(de::ServerKind::Login, {"-f", "login.conf", "-i", "1"});
    ASSERT_TRUE(options.loginIDOffset.has_value());
    EXPECT_EQ(1, *options.loginIDOffset);
}

TEST(ServerOptions, AcceptsSignedDecimalOffsetsIncludingZeroAndIntegerBounds) {
    const struct {
        const char* text;
        int value;
    } cases[] = {{"0", 0},
                 {"-0", 0},
                 {"+003", 3},
                 {"-3", -3},
                 {"2147483647", std::numeric_limits<int>::max()},
                 {"-2147483648", std::numeric_limits<int>::min()}};
    for (const auto& test : cases) {
        SCOPED_TRACE(test.text);
        const auto options = parse(de::ServerKind::Login, {"-f", "login.conf", "-i", test.text});
        ASSERT_TRUE(options.loginIDOffset.has_value());
        EXPECT_EQ(test.value, *options.loginIDOffset);
    }
}

TEST(ServerOptions, RejectsMalformedAndOutOfRangeOffsets) {
    for (const char* text : {"", "abc", "12abc", "1.5", "0x10", " 1", "1 ", "+", "-", "--1", "+-1", "++1", "2147483648",
                             "-2147483649", "99999999999999999999"}) {
        SCOPED_TRACE(text);
        EXPECT_THROW(parse(de::ServerKind::Login, {"-f", "login.conf", "-i", text}), Error);
    }
    EXPECT_THROW(parse(de::ServerKind::Login, {"-f", "login.conf", "-i", nullptr}), Error);
}

TEST(ServerOptions, ReportsTheActualLoginUsage) {
    try {
        (void)parse(de::ServerKind::Login, {});
        FAIL() << "missing arguments accepted";
    } catch (const Error& error) {
        EXPECT_NE(std::string::npos, error.toString().find("loginserver -f <config file> [-i ID]"));
    }
}

Properties loginConfig() {
    Properties config;
    config.setProperty("LoginServerBasePort", "9900");
    config.setProperty("LoginServerBaseUDPPort", "9800");
    config.setProperty("LoginServerBaseID", "10");
    config.setProperty("LoginServerPort", "7777");
    config.setProperty("LoginServerUDPPort", "7778");
    config.setProperty("LoginServerID", "7");
    config.setProperty("Unrelated", "unchanged");
    return config;
}

TEST(LoginServerOffset, UsesTheBasesForAllThreeOverrides) {
    auto config = loginConfig();
    de::applyLoginServerOffset(config, 3);
    EXPECT_EQ("9903", config.getProperty("LoginServerPort"));
    EXPECT_EQ("9803", config.getProperty("LoginServerUDPPort"));
    EXPECT_EQ("13", config.getProperty("LoginServerID"));
    EXPECT_EQ("9900", config.getProperty("LoginServerBasePort"));
    EXPECT_EQ("9800", config.getProperty("LoginServerBaseUDPPort"));
    EXPECT_EQ("10", config.getProperty("LoginServerBaseID"));
    EXPECT_EQ("unchanged", config.getProperty("Unrelated"));
}

TEST(LoginServerOffset, ZeroStillReplacesTheConfiguredValuesWithTheBases) {
    auto config = loginConfig();
    de::applyLoginServerOffset(config, 0);
    EXPECT_EQ("9900", config.getProperty("LoginServerPort"));
    EXPECT_EQ("9800", config.getProperty("LoginServerUDPPort"));
    EXPECT_EQ("10", config.getProperty("LoginServerID"));
}

TEST(LoginServerOffset, PreservesSignedOffsetBehavior) {
    auto config = loginConfig();
    de::applyLoginServerOffset(config, -3);
    EXPECT_EQ("9897", config.getProperty("LoginServerPort"));
    EXPECT_EQ("9797", config.getProperty("LoginServerUDPPort"));
    EXPECT_EQ("7", config.getProperty("LoginServerID"));
}

TEST(LoginServerOffset, MissingLastBaseDoesNotPartiallyApplyOverrides) {
    Properties config;
    config.setProperty("LoginServerBasePort", "9900");
    config.setProperty("LoginServerBaseUDPPort", "9800");
    config.setProperty("LoginServerPort", "7777");
    config.setProperty("LoginServerUDPPort", "7778");
    const auto before = config.toString();
    EXPECT_THROW(de::applyLoginServerOffset(config, 1), NoSuchElementException);
    EXPECT_EQ(before, config.toString());
}

TEST(LoginServerOffset, InvalidBasesDoNotPartiallyApplyOverrides) {
    for (const char* key : {"LoginServerBasePort", "LoginServerBaseUDPPort", "LoginServerBaseID"}) {
        for (const char* value : {"", "123bad", "2147483648"}) {
            SCOPED_TRACE(std::string(key) + "=" + value);
            auto config = loginConfig();
            config.setProperty(key, value);
            const auto before = config.toString();
            EXPECT_THROW(de::applyLoginServerOffset(config, 1), Error);
            EXPECT_EQ(before, config.toString());
        }
    }
}

TEST(LoginServerOffset, OverflowInAnyOverrideLeavesAllPropertiesUnchanged) {
    for (const char* key : {"LoginServerBasePort", "LoginServerBaseUDPPort", "LoginServerBaseID"}) {
        for (int offset : {-1, 1}) {
            SCOPED_TRACE(std::string(key) + " offset " + std::to_string(offset));
            auto config = loginConfig();
            config.setProperty(
                key, std::to_string(offset < 0 ? std::numeric_limits<int>::min() : std::numeric_limits<int>::max()));
            const auto before = config.toString();
            EXPECT_THROW(de::applyLoginServerOffset(config, offset), Error);
            EXPECT_EQ(before, config.toString());
        }
    }
}

TEST(LoginServerOffset, ExactIntegerBoundsAreRepresentable) {
    auto config = loginConfig();
    config.setProperty("LoginServerBasePort", "2147483646");
    config.setProperty("LoginServerBaseUDPPort", "-2147483648");
    de::applyLoginServerOffset(config, 1);
    EXPECT_EQ("2147483647", config.getProperty("LoginServerPort"));
    EXPECT_EQ("-2147483647", config.getProperty("LoginServerUDPPort"));
    de::applyLoginServerOffset(config, 0);
    EXPECT_EQ("-2147483648", config.getProperty("LoginServerUDPPort"));
}

class ServerConfigurationTest : public ::testing::Test {
protected:
    void SetUp() override {
        std::string pattern = (std::filesystem::temp_directory_path() / "darkeden-startup-XXXXXX").string();
        ASSERT_NE(nullptr, mkdtemp(pattern.data()));
        directory = pattern;
        filename = directory + "/server.conf";
        de::kernelContext().setConfig(&published);
    }

    void TearDown() override {
        de::kernelContext().setConfig(nullptr);
        if (!directory.empty()) {
            std::error_code error;
            std::filesystem::remove_all(directory, error);
            EXPECT_FALSE(error) << error.message();
        }
    }

    void writeConfig(const char* text) {
        std::ofstream output(filename);
        ASSERT_TRUE(output);
        output << text;
        output.close();
        ASSERT_TRUE(output);
    }

    std::string directory;
    std::string filename;
    Properties published;
};

TEST_F(ServerConfigurationTest, LoadsPropertiesWithoutPublishingOrRequiringLoginBases) {
    writeConfig("# comment\nHomePath : /tmp/darkeden\nLoginServerPort : 9900\n");
    auto options = parse(de::ServerKind::Login, {"-f", filename.c_str()});
    const auto config = de::loadServerConfiguration(options);
    EXPECT_EQ("/tmp/darkeden", config->getProperty("HomePath"));
    EXPECT_EQ("9900", config->getProperty("LoginServerPort"));
    EXPECT_EQ(&published, &de::kernelContext().config());
}

TEST_F(ServerConfigurationTest, LoadsAndAppliesTheParsedLoginOffset) {
    writeConfig("LoginServerBasePort : 9900\nLoginServerBaseUDPPort : 9800\nLoginServerBaseID : 10\n");
    const auto options = parse(de::ServerKind::Login, {"-f", filename.c_str(), "-i", "3"});
    const auto config = de::loadServerConfiguration(options);
    EXPECT_EQ("9903", config->getProperty("LoginServerPort"));
    EXPECT_EQ("9803", config->getProperty("LoginServerUDPPort"));
    EXPECT_EQ("13", config->getProperty("LoginServerID"));
    EXPECT_EQ(&published, &de::kernelContext().config());
}

TEST_F(ServerConfigurationTest, MissingFileDoesNotReplaceThePublishedConfiguration) {
    EXPECT_THROW(de::loadServerConfiguration({filename, std::nullopt}), FileNotExistException);
    EXPECT_EQ(&published, &de::kernelContext().config());
}

TEST_F(ServerConfigurationTest, ParseFailureDoesNotPublishThePropertiesAlreadyRead) {
    writeConfig("HomePath : /tmp/darkeden\nmissing separator\n");
    EXPECT_THROW(de::loadServerConfiguration({filename, std::nullopt}), IOException);
    EXPECT_EQ(&published, &de::kernelContext().config());
}

TEST_F(ServerConfigurationTest, MissingValueIsReportedAsAParseFailure) {
    writeConfig("HomePath :\n");
    EXPECT_THROW(de::loadServerConfiguration({filename, std::nullopt}), IOException);
    EXPECT_EQ(&published, &de::kernelContext().config());
}

TEST_F(ServerConfigurationTest, BadOverridesDoNotPublishTheLoadedConfiguration) {
    writeConfig("LoginServerBasePort : 9900\nLoginServerBaseUDPPort : 9800\nLoginServerBaseID : not-an-id\n");
    EXPECT_THROW(de::loadServerConfiguration({filename, 1}), Error);
    EXPECT_EQ(&published, &de::kernelContext().config());
}

} // namespace
