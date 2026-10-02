#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

#include <gtest/gtest.h>

#include "Exception.h"
#include "ServerLifecycle.h"
#include "ServerShutdown.h"

namespace {

class ServerLifecycleTest : public ::testing::Test {
protected:
    void SetUp() override {
        previousRequest = ServerShutdown::requested.exchange(false);
        previousFailure = ServerShutdown::failed.exchange(false);
        std::string pattern = (std::filesystem::temp_directory_path() / "darkeden-lifecycle-XXXXXX").string();
        ASSERT_NE(nullptr, mkdtemp(pattern.data()));
        directory = pattern;
        instantLog = directory + "/instant.log";
        actions = {
            [this] { calls.emplace_back("initialize"); },
            [this] { calls.emplace_back("start"); },
            [this] {
                EXPECT_TRUE(ServerShutdown::isRequested());
                calls.emplace_back("stop");
            },
        };
    }

    void TearDown() override {
        ServerShutdown::requested.store(previousRequest);
        ServerShutdown::failed.store(previousFailure);
        if (!directory.empty()) {
            std::error_code error;
            std::filesystem::remove_all(directory, error);
            EXPECT_FALSE(error) << error.message();
        }
    }

    de::ServerLifecycleResult run() {
        return de::runServerLifecycle(actions, output, errors, instantLog);
    }

    std::string readInstantLog() const {
        std::ifstream input(instantLog);
        return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
    }

    void expectResult(const de::ServerLifecycleResult& result, bool drained, int exitCode) {
        EXPECT_EQ(drained, result.drained);
        EXPECT_EQ(exitCode, result.exitCode);
        EXPECT_TRUE(ServerShutdown::isRequested());
        EXPECT_EQ(exitCode == EXIT_FAILURE, ServerShutdown::failed.load());
    }

    bool previousRequest = false;
    bool previousFailure = false;
    std::string directory;
    std::string instantLog;
    std::vector<std::string> calls;
    std::ostringstream output;
    std::ostringstream errors;
    de::ServerLifecycleActions actions;
};

TEST_F(ServerLifecycleTest, NormalReturnRequestsShutdownBeforeStoppingExactlyOnce) {
    expectResult(run(), true, EXIT_SUCCESS);
    EXPECT_EQ((std::vector<std::string>{"initialize", "start", "stop"}), calls);
    EXPECT_TRUE(output.str().empty());
    EXPECT_TRUE(errors.str().empty());
    EXPECT_FALSE(std::filesystem::exists(instantLog));
}

TEST_F(ServerLifecycleTest, AnExistingShutdownRequestStillInitializesButSkipsStart) {
    ServerShutdown::request();
    expectResult(run(), true, EXIT_SUCCESS);
    EXPECT_EQ((std::vector<std::string>{"initialize", "stop"}), calls);
}

TEST_F(ServerLifecycleTest, AShutdownRequestedDuringInitializationSkipsStart) {
    actions.initialize = [&] {
        calls.emplace_back("initialize");
        ServerShutdown::request();
    };
    expectResult(run(), true, EXIT_SUCCESS);
    EXPECT_EQ((std::vector<std::string>{"initialize", "stop"}), calls);
}

TEST_F(ServerLifecycleTest, AShutdownRequestedDuringStartDrainsSuccessfully) {
    actions.start = [&] {
        calls.emplace_back("start");
        ServerShutdown::request();
    };
    expectResult(run(), true, EXIT_SUCCESS);
    EXPECT_EQ((std::vector<std::string>{"initialize", "start", "stop"}), calls);
}

TEST_F(ServerLifecycleTest, AnExistingWorkerFailureIsNotCleared) {
    ServerShutdown::fail();
    expectResult(run(), true, EXIT_FAILURE);
    EXPECT_EQ((std::vector<std::string>{"initialize", "stop"}), calls);
    EXPECT_TRUE(output.str().empty());
    EXPECT_TRUE(errors.str().empty());
}

TEST_F(ServerLifecycleTest, AWorkerFailureDuringInitializationSkipsStart) {
    actions.initialize = [&] {
        calls.emplace_back("initialize");
        ServerShutdown::fail();
    };
    expectResult(run(), true, EXIT_FAILURE);
    EXPECT_EQ((std::vector<std::string>{"initialize", "stop"}), calls);
}

TEST_F(ServerLifecycleTest, AWorkerFailureDuringStartSurvivesSuccessfulTeardown) {
    actions.start = [&] {
        calls.emplace_back("start");
        ServerShutdown::fail();
    };
    expectResult(run(), true, EXIT_FAILURE);
    EXPECT_EQ((std::vector<std::string>{"initialize", "start", "stop"}), calls);
}

TEST_F(ServerLifecycleTest, AWorkerFailureReportedDuringStopDeterminesTheExitStatus) {
    actions.stop = [&] {
        EXPECT_TRUE(ServerShutdown::isRequested());
        calls.emplace_back("stop");
        ServerShutdown::fail();
    };
    expectResult(run(), true, EXIT_FAILURE);
    EXPECT_EQ((std::vector<std::string>{"initialize", "start", "stop"}), calls);
}

TEST_F(ServerLifecycleTest, FailedConstructionReachesCleanupWithoutAServer) {
    struct UnconstructibleServer {
        UnconstructibleServer() {
            throw Error("construction failed");
        }
    };
    std::unique_ptr<UnconstructibleServer> server;
    actions.initialize = [&] { server = std::make_unique<UnconstructibleServer>(); };
    actions.stop = [&] {
        EXPECT_TRUE(ServerShutdown::isRequested());
        EXPECT_EQ(nullptr, server);
        calls.emplace_back("cleanup");
    };
    expectResult(run(), true, EXIT_FAILURE);
    EXPECT_EQ((std::vector<std::string>{"cleanup"}), calls);
    EXPECT_NE(std::string::npos, output.str().find("construction failed"));
}

TEST_F(ServerLifecycleTest, PartialInitializationKeepsDependenciesAliveForCleanupAndTheCaller) {
    std::unique_ptr<int> dependency;
    actions.initialize = [&] {
        dependency = std::make_unique<int>(42);
        throw Error("initialization failed");
    };
    actions.stop = [&] {
        EXPECT_TRUE(ServerShutdown::isRequested());
        ASSERT_NE(nullptr, dependency);
        EXPECT_EQ(42, *dependency);
        calls.emplace_back("cleanup");
    };
    expectResult(run(), true, EXIT_FAILURE);
    EXPECT_EQ((std::vector<std::string>{"cleanup"}), calls);
    ASSERT_NE(nullptr, dependency);
    EXPECT_EQ(42, *dependency);
}

enum class FailureKind { Throwable, Standard, Unknown };
struct UnknownFailure {};

[[noreturn]] void throwFailure(FailureKind kind, const std::string& message) {
    switch (kind) {
    case FailureKind::Throwable:
        throw Error(message);
    case FailureKind::Standard:
        throw std::runtime_error(message);
    case FailureKind::Unknown:
        throw UnknownFailure{};
    }
    std::abort();
}

enum class StartupStage { Initialize, Start };
class ServerStartupFailureTest : public ServerLifecycleTest,
                                 public ::testing::WithParamInterface<std::tuple<StartupStage, FailureKind>> {};

TEST_P(ServerStartupFailureTest, ReportsFailureBeforeStoppingAndReturnsFailureAfterDraining) {
    const auto [stage, kind] = GetParam();
    auto& action = stage == StartupStage::Initialize ? actions.initialize : actions.start;
    action = [original = action, kind] {
        original();
        throwFailure(kind, "startup failed");
    };
    const std::string diagnostic =
        kind == FailureKind::Throwable ? Error("startup failed").toString() + "\n" : "unknown exception...\n";
    actions.stop = [&, original = actions.stop] {
        // Preserve the diagnostic even when the subsequent stop blocks.
        EXPECT_EQ(diagnostic, output.str());
        if (kind == FailureKind::Throwable)
            EXPECT_EQ(diagnostic, readInstantLog());
        else
            EXPECT_FALSE(std::filesystem::exists(instantLog));
        EXPECT_TRUE(ServerShutdown::failed.load());
        original();
    };

    expectResult(run(), true, EXIT_FAILURE);
    const std::vector<std::string> expected = stage == StartupStage::Initialize
                                                  ? std::vector<std::string>{"initialize", "stop"}
                                                  : std::vector<std::string>{"initialize", "start", "stop"};
    EXPECT_EQ(expected, calls);
    EXPECT_TRUE(errors.str().empty());
}

INSTANTIATE_TEST_SUITE_P(Exceptions, ServerStartupFailureTest,
                         ::testing::Combine(::testing::Values(StartupStage::Initialize, StartupStage::Start),
                                            ::testing::Values(FailureKind::Throwable, FailureKind::Standard,
                                                              FailureKind::Unknown)));

class ServerStopFailureTest : public ServerLifecycleTest, public ::testing::WithParamInterface<FailureKind> {};

TEST_P(ServerStopFailureTest, ReportsFailedDrainAndDoesNotRetryStop) {
    actions.stop = [&, original = actions.stop] {
        original();
        throwFailure(GetParam(), "stop failed");
    };
    expectResult(run(), false, EXIT_FAILURE);
    EXPECT_EQ((std::vector<std::string>{"initialize", "start", "stop"}), calls);
    EXPECT_TRUE(output.str().empty());
    EXPECT_FALSE(std::filesystem::exists(instantLog));
    const std::string diagnostic = GetParam() == FailureKind::Throwable  ? Error("stop failed").toString()
                                   : GetParam() == FailureKind::Standard ? "stop failed"
                                                                         : "unknown exception";
    EXPECT_EQ("Shutdown failed: " + diagnostic + "\n", errors.str());
}

INSTANTIATE_TEST_SUITE_P(Exceptions, ServerStopFailureTest,
                         ::testing::Values(FailureKind::Throwable, FailureKind::Standard, FailureKind::Unknown));

TEST_F(ServerLifecycleTest, StartupAndStopFailuresKeepBothDiagnostics) {
    actions.initialize = [] { throw Error("startup failed"); };
    actions.stop = [&] {
        EXPECT_TRUE(ServerShutdown::isRequested());
        calls.emplace_back("stop");
        throw std::runtime_error("stop failed");
    };
    expectResult(run(), false, EXIT_FAILURE);
    EXPECT_EQ((std::vector<std::string>{"stop"}), calls);
    EXPECT_EQ(Error("startup failed").toString() + "\n", output.str());
    EXPECT_EQ(output.str(), readInstantLog());
    EXPECT_EQ("Shutdown failed: stop failed\n", errors.str());
}

TEST_F(ServerLifecycleTest, AnUnavailableInstantLogDoesNotPreventReportingOrCleanup) {
    instantLog = directory + "/missing/instant.log";
    actions.initialize = [] { throw Error("startup failed"); };
    expectResult(run(), true, EXIT_FAILURE);
    EXPECT_EQ((std::vector<std::string>{"stop"}), calls);
    EXPECT_EQ(Error("startup failed").toString() + "\n", output.str());
    EXPECT_FALSE(std::filesystem::exists(instantLog));
}

TEST_F(ServerLifecycleTest, NormalStartupPreservesAnExistingInstantLog) {
    std::ofstream(instantLog) << "previous failure\n";
    expectResult(run(), true, EXIT_SUCCESS);
    EXPECT_EQ("previous failure\n", readInstantLog());
}

TEST_F(ServerLifecycleTest, AStartupThrowableReplacesThePreviousInstantLog) {
    std::ofstream(instantLog) << "previous failure with a much longer message\n";
    actions.initialize = [] { throw Error("startup failed"); };
    expectResult(run(), true, EXIT_FAILURE);
    EXPECT_EQ(Error("startup failed").toString() + "\n", readInstantLog());
}

} // namespace
