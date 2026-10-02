#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <gtest/gtest.h>
#include <initializer_list>

#include "KernelContext.h"
#include "Properties.h"
#include "ServerApplication.h"
#include "ServerShutdown.h"

namespace {

class ServerApplicationFixture : public ::testing::Test {
protected:
    void SetUp() override {
        previousRequest = ServerShutdown::requested.exchange(false);
        previousFailure = ServerShutdown::failed.exchange(false);
        previousGlobal = de::kernelContext().exchangeConfig(&globalConfig);
        previousConfig.setProperty("Marker", "previous");
        globalConfig.setProperty("Marker", "global");
        context.setConfig(&previousConfig);

        std::string pattern = (std::filesystem::temp_directory_path() / "darkeden-application-XXXXXX").string();
        ASSERT_NE(nullptr, mkdtemp(pattern.data()));
        directory = pattern;
        filename = directory + "/server.conf";
        instantLog = directory + "/instant.log";
        writeConfig("Marker : ready\n");
        actions = {
            .initialize =
                [&] {
                    calls.emplace_back("initialize");
                    observedConfig = &context.config();
                    EXPECT_NE(&previousConfig, observedConfig);
                    EXPECT_EQ("ready", observedConfig->getProperty("Marker"));
                    EXPECT_EQ(&globalConfig, &de::kernelContext().config());
                },
            .start =
                [&] {
                    calls.emplace_back("start");
                    EXPECT_EQ(observedConfig, &context.config());
                },
            .stop =
                [&] {
                    calls.emplace_back("stop");
                    EXPECT_TRUE(ServerShutdown::isRequested());
                    EXPECT_EQ(observedConfig, &context.config());
                    EXPECT_EQ("ready", context.config().getProperty("Marker"));
                },
        };
    }

    void TearDown() override {
        de::kernelContext().setConfig(previousGlobal);
        ServerShutdown::requested.store(previousRequest);
        ServerShutdown::failed.store(previousFailure);
        if (!directory.empty()) {
            std::error_code error;
            std::filesystem::remove_all(directory, error);
            EXPECT_FALSE(error) << error.message();
        }
    }

    void writeConfig(const char* text) {
        std::ofstream file(filename);
        ASSERT_TRUE(file);
        file << text;
    }

    std::optional<de::ServerLifecycleResult> run(de::ServerApplication& application, de::ServerKind server,
                                                 std::initializer_list<const char*> arguments) {
        std::vector<const char*> argv{"server"};
        argv.insert(argv.end(), arguments.begin(), arguments.end());
        return application.run(server, static_cast<int>(argv.size()), argv.data(), actions, output, errors, instantLog);
    }

    std::optional<de::ServerLifecycleResult> run(de::ServerApplication& application, de::ServerKind server) {
        return run(application, server, {"-f", filename.c_str()});
    }

    void expectUntouchedConfiguration() {
        EXPECT_EQ(&previousConfig, &context.config());
        EXPECT_EQ("previous", context.config().getProperty("Marker"));
        EXPECT_EQ(&globalConfig, &de::kernelContext().config());
    }

    void expectNoLifecycle() {
        EXPECT_TRUE(calls.empty());
        EXPECT_FALSE(ServerShutdown::isRequested());
        EXPECT_FALSE(ServerShutdown::failed.load());
        EXPECT_EQ(std::string::npos, output.str().find("WORKERS STOPPED"));
        EXPECT_FALSE(std::filesystem::exists(instantLog));
        expectUntouchedConfiguration();
    }

    de::KernelContext context;
    Properties previousConfig;
    Properties globalConfig;
    Properties* previousGlobal = nullptr;
    Properties* observedConfig = nullptr;
    bool previousRequest = false;
    bool previousFailure = false;
    std::string directory;
    std::string filename;
    std::string instantLog;
    std::vector<std::string> calls;
    std::ostringstream output;
    std::ostringstream errors;
    de::ServerLifecycleActions actions;
};

struct ServerCase {
    de::ServerKind kind;
    const char* name;
    const char* stopped;
};

class ServerApplicationTest : public ServerApplicationFixture, public ::testing::WithParamInterface<ServerCase> {};

TEST_P(ServerApplicationTest, ConfigurationLivesThroughTheLifecycleAndUntilTheApplicationLeavesScope) {
    {
        de::ServerApplication application(context);
        const auto result = run(application, GetParam().kind);
        ASSERT_TRUE(result);
        EXPECT_TRUE(result->drained);
        EXPECT_EQ(EXIT_SUCCESS, result->exitCode);
        EXPECT_EQ((std::vector<std::string>{"initialize", "start", "stop"}), calls);
        EXPECT_EQ(observedConfig, &context.config());
        EXPECT_EQ("ready", context.config().getProperty("Marker"));
        EXPECT_TRUE(ServerShutdown::isRequested());
        EXPECT_FALSE(ServerShutdown::failed.load());
        const std::string prefix = GetParam().kind == de::ServerKind::Game
                                       ? ">>> COMMAND-LINE PARAMETER READING SUCCESS...\n"
                                       : "Marker : ready\n\n";
        EXPECT_EQ(prefix + GetParam().stopped + "\n", output.str());
        EXPECT_TRUE(errors.str().empty());
    }
    expectUntouchedConfiguration();
}

TEST_P(ServerApplicationTest, InvalidArgumentsNeverPublishOrInvokeTheLifecycle) {
    {
        de::ServerApplication application(context);
        EXPECT_FALSE(run(application, GetParam().kind, {"-x", filename.c_str()}));
        expectNoLifecycle();
        EXPECT_NE(std::string::npos, errors.str().find(std::string("Usage : ") + GetParam().name));
        EXPECT_TRUE(output.str().empty());
    }
    expectUntouchedConfiguration();
}

TEST_P(ServerApplicationTest, AFailedFileReadNeverPublishesOrInvokesTheLifecycle) {
    filename = directory + "/missing.conf";
    {
        de::ServerApplication application(context);
        EXPECT_FALSE(run(application, GetParam().kind));
        expectNoLifecycle();
        EXPECT_NE(std::string::npos, errors.str().find("missing.conf"));
    }
    expectUntouchedConfiguration();
}

TEST_P(ServerApplicationTest, APartiallyReadFileNeverPublishesOrInvokesTheLifecycle) {
    writeConfig("Marker : ready\nbroken configuration line\n");
    {
        de::ServerApplication application(context);
        EXPECT_FALSE(run(application, GetParam().kind));
        expectNoLifecycle();
        EXPECT_NE(std::string::npos, errors.str().find("missing separator"));
    }
    expectUntouchedConfiguration();
}

TEST_P(ServerApplicationTest, InitializationFailureKeepsConfigurationAliveThroughCleanupAndTheReturn) {
    actions.initialize = [original = actions.initialize] {
        original();
        throw Error("initialization failed");
    };
    {
        de::ServerApplication application(context);
        const auto result = run(application, GetParam().kind);
        ASSERT_TRUE(result);
        EXPECT_TRUE(result->drained);
        EXPECT_EQ(EXIT_FAILURE, result->exitCode);
        EXPECT_EQ((std::vector<std::string>{"initialize", "stop"}), calls);
        EXPECT_EQ(observedConfig, &context.config());
        EXPECT_EQ("ready", context.config().getProperty("Marker"));
        EXPECT_TRUE(ServerShutdown::failed.load());
        const auto diagnostic = output.str().find("initialization failed");
        ASSERT_NE(std::string::npos, diagnostic);
        const auto stopped = output.str().find(GetParam().stopped);
        ASSERT_NE(std::string::npos, stopped);
        EXPECT_GT(stopped, diagnostic);
        std::ifstream log(instantLog);
        const std::string logged{std::istreambuf_iterator<char>(log), std::istreambuf_iterator<char>()};
        EXPECT_EQ(Error("initialization failed").toString() + "\n", logged);
    }
    expectUntouchedConfiguration();
}

TEST_P(ServerApplicationTest, FailedCleanupRetainsConfigurationAndDoesNotReportStoppedWorkers) {
    actions.stop = [original = actions.stop] {
        original();
        throw Error("stop failed");
    };
    {
        de::ServerApplication application(context);
        const auto result = run(application, GetParam().kind);
        ASSERT_TRUE(result);
        EXPECT_FALSE(result->drained);
        EXPECT_EQ(EXIT_FAILURE, result->exitCode);
        EXPECT_EQ((std::vector<std::string>{"initialize", "start", "stop"}), calls);
        EXPECT_EQ(observedConfig, &context.config());
        EXPECT_EQ("ready", context.config().getProperty("Marker"));
        EXPECT_TRUE(ServerShutdown::failed.load());
        EXPECT_EQ(std::string::npos, output.str().find("WORKERS STOPPED"));
        EXPECT_NE(std::string::npos, errors.str().find("Shutdown failed: "));
    }
    expectUntouchedConfiguration();
}

TEST_P(ServerApplicationTest, AnExistingShutdownRequestSkipsStartWithoutLosingCleanup) {
    ServerShutdown::request();
    de::ServerApplication application(context);
    const auto result = run(application, GetParam().kind);
    ASSERT_TRUE(result);
    EXPECT_TRUE(result->drained);
    EXPECT_EQ(EXIT_SUCCESS, result->exitCode);
    EXPECT_EQ((std::vector<std::string>{"initialize", "stop"}), calls);
    EXPECT_NE(std::string::npos, output.str().find(GetParam().stopped));
}

INSTANTIATE_TEST_SUITE_P(
    Servers, ServerApplicationTest,
    ::testing::Values(ServerCase{de::ServerKind::Game, "gameserver", ">>> ALL GAME WORKERS STOPPED."},
                      ServerCase{de::ServerKind::Login, "loginserver", ">>> ALL LOGIN WORKERS STOPPED."},
                      ServerCase{de::ServerKind::Shared, "sharedserver", ">>> ALL SHARED WORKERS STOPPED."}),
    [](const ::testing::TestParamInfo<ServerCase>& info) { return info.param.name; });

class LoginApplicationOffsetTest : public ServerApplicationFixture, public ::testing::WithParamInterface<int> {};

TEST_P(LoginApplicationOffsetTest, PublishesAndPrintsEffectiveOverridesIncludingZero) {
    writeConfig("Marker : ready\nLoginServerBasePort : 9900\nLoginServerBaseUDPPort : 9800\nLoginServerBaseID : 10\n"
                "LoginServerPort : 1\nLoginServerUDPPort : 2\nLoginServerID : 3\n");
    const auto offset = std::to_string(GetParam());
    actions.initialize = [&, original = actions.initialize] {
        original();
        EXPECT_EQ(9900 + GetParam(), context.config().getPropertyInt("LoginServerPort"));
        EXPECT_EQ(9800 + GetParam(), context.config().getPropertyInt("LoginServerUDPPort"));
        EXPECT_EQ(10 + GetParam(), context.config().getPropertyInt("LoginServerID"));
        const std::string summary = "LoginServerPort : " + std::to_string(9900 + GetParam()) +
                                    "\nLoginServerUDPPort : " + std::to_string(9800 + GetParam()) +
                                    "\nLoginServerID : " + std::to_string(10 + GetParam()) + "\n";
        EXPECT_NE(std::string::npos, output.str().find(summary));
        EXPECT_EQ(0U, output.str().find(context.config().toString() + "\n"));
    };
    de::ServerApplication application(context);
    const auto result = run(application, de::ServerKind::Login, {"-f", filename.c_str(), "-i", offset.c_str()});
    ASSERT_TRUE(result);
    EXPECT_EQ(EXIT_SUCCESS, result->exitCode);
    EXPECT_EQ((std::vector<std::string>{"initialize", "start", "stop"}), calls);
}

INSTANTIATE_TEST_SUITE_P(Offsets, LoginApplicationOffsetTest, ::testing::Values(-2, 0, 2));

TEST_F(ServerApplicationFixture, AnInvalidLoginOverrideNeverPublishesOrInvokesTheLifecycle) {
    writeConfig("Marker : ready\nLoginServerBasePort : 9900\nLoginServerBaseUDPPort : 9800\n"
                "LoginServerBaseID : 2147483647\n");
    de::ServerApplication application(context);
    EXPECT_FALSE(run(application, de::ServerKind::Login, {"-f", filename.c_str(), "-i", "1"}));
    expectNoLifecycle();
    EXPECT_NE(std::string::npos, errors.str().find("LoginServerBaseID plus -i offset"));
}

TEST_F(ServerApplicationFixture, AnOriginallyEmptyContextIsRestoredAfterTheApplicationLeavesScope) {
    context.setConfig(nullptr);
    {
        de::ServerApplication application(context);
        ASSERT_TRUE(run(application, de::ServerKind::Login));
        EXPECT_EQ("ready", context.config().getProperty("Marker"));
    }
    EXPECT_EQ(nullptr, context.exchangeConfig(&previousConfig));
}

TEST_F(ServerApplicationFixture, AWorkerFailureSurvivesSuccessfulCleanupAndFinalReporting) {
    actions.start = [original = actions.start] {
        original();
        ServerShutdown::fail();
    };
    de::ServerApplication application(context);
    const auto result = run(application, de::ServerKind::Login);
    ASSERT_TRUE(result);
    EXPECT_TRUE(result->drained);
    EXPECT_EQ(EXIT_FAILURE, result->exitCode);
    EXPECT_NE(std::string::npos, output.str().find(">>> ALL LOGIN WORKERS STOPPED."));
}

TEST_F(ServerApplicationFixture, ASecondRunCannotReplaceConfigurationOrInvokeActionsAgain) {
    de::ServerApplication application(context);
    ASSERT_TRUE(run(application, de::ServerKind::Login));
    const auto* configured = &context.config();
    calls.clear();
    writeConfig("Marker : replacement\n");
    EXPECT_THROW(run(application, de::ServerKind::Login), Error);
    EXPECT_TRUE(calls.empty());
    EXPECT_EQ(configured, &context.config());
    EXPECT_EQ("ready", context.config().getProperty("Marker"));
}

TEST_F(ServerApplicationFixture, AFailedConfigurationAttemptStillConsumesTheSingleRun) {
    de::ServerApplication application(context);
    EXPECT_FALSE(run(application, de::ServerKind::Login, {}));
    EXPECT_THROW(run(application, de::ServerKind::Login), Error);
    expectNoLifecycle();
}

class RecordingBuffer : public std::stringbuf {
public:
    std::string flushed;

    int sync() override {
        flushed = str();
        return 0;
    }
};

TEST_F(ServerApplicationFixture, FinalReportingFlushesBothStreamsEvenWhenStopFails) {
    RecordingBuffer outputBuffer;
    RecordingBuffer errorBuffer;
    std::ostream bufferedOutput(&outputBuffer);
    std::ostream bufferedErrors(&errorBuffer);
    actions.stop = [&] {
        bufferedOutput << "pending output";
        bufferedErrors << "pending error";
        throw Error("stop failed");
    };
    de::ServerApplication application(context);
    const char* argv[] = {"server", "-f", filename.c_str()};
    const auto result =
        application.run(de::ServerKind::Login, 3, argv, actions, bufferedOutput, bufferedErrors, instantLog);
    ASSERT_TRUE(result);
    EXPECT_FALSE(result->drained);
    EXPECT_EQ(EXIT_FAILURE, result->exitCode);
    EXPECT_NE(std::string::npos, outputBuffer.flushed.find("pending output"));
    EXPECT_NE(std::string::npos, errorBuffer.flushed.find("pending error"));
    EXPECT_EQ(outputBuffer.str(), outputBuffer.flushed);
    EXPECT_EQ(errorBuffer.str(), errorBuffer.flushed);
}

class FailingBuffer : public std::streambuf {
    std::streamsize xsputn(const char*, std::streamsize) override {
        throw std::runtime_error("output failed");
    }
};

TEST_F(ServerApplicationFixture, UnwindingAfterPublicationRestoresThePreviousBinding) {
    FailingBuffer buffer;
    std::ostream brokenOutput(&buffer);
    brokenOutput.exceptions(std::ios::badbit);
    const char* argv[] = {"server", "-f", filename.c_str()};
    EXPECT_THROW(
        {
            de::ServerApplication application(context);
            (void)application.run(de::ServerKind::Login, 3, argv, actions, brokenOutput, errors, instantLog);
        },
        std::runtime_error);
    expectNoLifecycle();
}

} // namespace
