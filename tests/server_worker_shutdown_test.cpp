#include <atomic>
#include <chrono>
#include <exception>
#include <functional>
#include <future>
#include <memory>
#include <new>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "ManagedThread.h"
#include "ServerLifecycle.h"
#include "ServerWorkerShutdown.h"
#include "ThreadPool.h"

using namespace std::chrono_literals;

namespace {

class ObservedWorker : public ManagedThread {
public:
    ObservedWorker(std::string name, std::vector<std::string>& events) : name(std::move(name)), events(events) {}

    ~ObservedWorker() noexcept override {
        // Cleanup also covers a failed assertion or an injected stop/join error.
        ManagedThread::stop();
        ManagedThread::join();
    }

    void run() override {
        entered.set_value();
        while (pauseFor(5s)) {
        }
        if (dependencyAlive)
            sawLiveDependency = dependencyAlive->load();
        finished = true;
        if (runFailure)
            std::rethrow_exception(runFailure);
    }

    void stop() override {
        events.push_back(name + ".stop");
        if (stopFailure)
            std::rethrow_exception(stopFailure);
        ManagedThread::stop();
    }

    void join() override {
        events.push_back(name + ".join");
        if (joinFailure)
            std::rethrow_exception(joinFailure);
        ManagedThread::join();
        joined = true;
    }

    std::string getName() const override {
        ++nameAttempts;
        if (nameFailure)
            std::rethrow_exception(nameFailure);
        return name;
    }

    std::promise<void> entered;
    std::atomic<bool> finished{false};
    std::atomic<bool>* dependencyAlive = nullptr;
    std::atomic<bool> sawLiveDependency{false};
    std::exception_ptr runFailure;
    std::exception_ptr stopFailure;
    std::exception_ptr joinFailure;
    std::exception_ptr nameFailure;
    mutable unsigned nameAttempts = 0;
    bool joined = false;

private:
    const std::string name;
    std::vector<std::string>& events;
};

class ServerWorkerShutdownTest : public ::testing::Test {
protected:
    void SetUp() override {
        previousRequest = ServerShutdown::requested.exchange(false);
        previousFailure = ServerShutdown::failed.exchange(false);
    }

    void TearDown() override {
        ServerShutdown::requested.store(previousRequest);
        ServerShutdown::failed.store(previousFailure);
    }

    void start(ObservedWorker& worker) {
        auto entered = worker.entered.get_future();
        worker.start();
        ASSERT_EQ(std::future_status::ready, entered.wait_for(2s));
    }

    std::vector<std::string> events;
    std::ostringstream output;
    std::ostringstream errors;

private:
    bool previousRequest = false;
    bool previousFailure = false;
};

TEST_F(ServerWorkerShutdownTest, EmptyGroupStillDrainsOtherWorkers) {
    de::stopServerWorkers({}, errors, [&] { events.emplace_back("zones"); });
    EXPECT_EQ((std::vector<std::string>{"zones"}), events);
    EXPECT_TRUE(errors.str().empty());
    EXPECT_FALSE(ServerShutdown::isRequested());
    EXPECT_FALSE(ServerShutdown::failed.load());
    EXPECT_NO_THROW(de::stopServerWorkers({}, errors));
}

TEST_F(ServerWorkerShutdownTest, AllStopRequestsPrecedeOtherDrainingAndEveryJoin) {
    ObservedWorker first("first", events);
    ObservedWorker second("second", events);
    ObservedWorker third("third", events);
    const de::ServerWorker workers[] = {{first}, {second}, {third}};

    de::stopServerWorkers(workers, errors, [&] {
        EXPECT_EQ((std::vector<std::string>{"first.stop", "second.stop", "third.stop"}), events);
        EXPECT_FALSE(first.joined || second.joined || third.joined);
        events.emplace_back("zones");
    });

    EXPECT_EQ((std::vector<std::string>{"first.stop", "second.stop", "third.stop", "zones", "first.join", "second.join",
                                        "third.join"}),
              events);
    EXPECT_TRUE(first.joined && second.joined && third.joined);
    EXPECT_TRUE(errors.str().empty());
}

TEST_F(ServerWorkerShutdownTest, PollingWorkersJoinWhileTheirDependenciesAreAlive) {
    std::atomic<bool> dependencyAlive{true};
    ObservedWorker first("first", events);
    ObservedWorker second("second", events);
    first.dependencyAlive = &dependencyAlive;
    second.dependencyAlive = &dependencyAlive;
    start(first);
    start(second);
    const de::ServerWorker workers[] = {{first}, {second}};

    const auto begin = std::chrono::steady_clock::now();
    de::stopServerWorkers(workers, errors);
    EXPECT_LT(std::chrono::steady_clock::now() - begin, 2s);
    dependencyAlive = false;

    EXPECT_TRUE(first.finished && second.finished);
    EXPECT_TRUE(first.sawLiveDependency && second.sawLiveDependency);
    EXPECT_TRUE(first.joined && second.joined);
    EXPECT_EQ(Thread::EXIT, first.getStatus());
    EXPECT_EQ(Thread::EXIT, second.getStatus());
    EXPECT_TRUE(errors.str().empty());
    EXPECT_FALSE(ServerShutdown::failed.load());
}

TEST_F(ServerWorkerShutdownTest, PartiallyStartedGroupsDrainBothStartedAndUnstartedWorkers) {
    ObservedWorker started("started", events);
    ObservedWorker unstarted("unstarted", events);
    start(started);
    const de::ServerWorker workers[] = {{started}, {unstarted}};
    de::stopServerWorkers(workers, errors);

    EXPECT_TRUE(started.finished);
    EXPECT_FALSE(unstarted.finished);
    EXPECT_TRUE(started.joined && unstarted.joined);
    EXPECT_EQ(Thread::EXIT, unstarted.getStatus());
    EXPECT_THROW(unstarted.start(), ThreadException);
    EXPECT_FALSE(ServerShutdown::failed.load());
}

TEST_F(ServerWorkerShutdownTest, AlreadyStoppedWorkersCanBeJoinedAgain) {
    ObservedWorker worker("worker", events);
    start(worker);
    worker.stop();
    worker.join();
    events.clear();
    const de::ServerWorker workers[] = {{worker}};
    EXPECT_NO_THROW(de::stopServerWorkers(workers, errors));
    EXPECT_EQ((std::vector<std::string>{"worker.stop", "worker.join"}), events);
    EXPECT_TRUE(errors.str().empty());
    EXPECT_FALSE(ServerShutdown::failed.load());
}

TEST_F(ServerWorkerShutdownTest, AnExistingProcessFailureSurvivesASuccessfulDrain) {
    ServerShutdown::fail();
    ObservedWorker worker("worker", events);
    const de::ServerWorker workers[] = {{worker}};
    de::stopServerWorkers(workers, errors);
    EXPECT_TRUE(worker.joined);
    EXPECT_TRUE(ServerShutdown::isRequested());
    EXPECT_TRUE(ServerShutdown::failed.load());
}

TEST_F(ServerWorkerShutdownTest, ExplicitNamesPreserveLegacyManagerDiagnostics) {
    ObservedWorker worker("Thread", events);
    worker.runFailure = std::make_exception_ptr(std::runtime_error("connection failed"));
    start(worker);
    const de::ServerWorker workers[] = {{worker, "GameServerManager"}};
    de::stopServerWorkers(workers, errors);
    EXPECT_EQ("GameServerManager: connection failed\n", errors.str());
    EXPECT_TRUE(worker.joined);
}

TEST_F(ServerWorkerShutdownTest, EveryRetainedFailureIsReportedAndRemainingWorkersJoin) {
    ObservedWorker first("first", events);
    ObservedWorker second("second", events);
    ObservedWorker third("third", events);
    ObservedWorker healthy("healthy", events);
    first.runFailure = std::make_exception_ptr(Error("first failure"));
    second.runFailure = std::make_exception_ptr(std::runtime_error("second failure"));
    third.runFailure = std::make_exception_ptr(42);
    start(first);
    start(second);
    start(third);
    start(healthy);
    const de::ServerWorker workers[] = {{first}, {second}, {third}, {healthy}};
    de::stopServerWorkers(workers, errors);

    EXPECT_EQ("first: " + Error("first failure").toString() +
                  "\nsecond: second failure\nthird: unknown worker failure\n",
              errors.str());
    EXPECT_TRUE(first.joined && second.joined && third.joined && healthy.joined);
    EXPECT_TRUE(healthy.finished);
    EXPECT_TRUE(ServerShutdown::isRequested());
    EXPECT_TRUE(ServerShutdown::failed.load());
    EXPECT_THROW(first.rethrowFailure(), Error);
    EXPECT_THROW(second.rethrowFailure(), std::runtime_error);
    EXPECT_THROW(third.rethrowFailure(), int);
}

TEST_F(ServerWorkerShutdownTest, OtherDrainFailureStillJoinsWorkersAndReachesTheLifecycle) {
    ObservedWorker first("first", events);
    ObservedWorker second("second", events);
    start(first);
    start(second);
    const de::ServerWorker workers[] = {{first}, {second}};
    const de::ServerLifecycleActions actions{
        .initialize = [] {},
        .start = [] {},
        .stop =
            [&] {
                de::stopServerWorkers(workers, errors, [&] {
                    events.emplace_back("zones");
                    throw Error("zone drain failed");
                });
            },
    };
    const auto result = de::runServerLifecycle(actions, output, errors);
    EXPECT_FALSE(result.drained);
    EXPECT_EQ(EXIT_FAILURE, result.exitCode);
    EXPECT_EQ((std::vector<std::string>{"first.stop", "second.stop", "zones", "first.join", "second.join"}), events);
    EXPECT_NE(std::string::npos, errors.str().find("Shutdown failed: " + Error("zone drain failed").toString()));
    EXPECT_TRUE(first.joined && second.joined);
    EXPECT_TRUE(first.finished && second.finished);
    EXPECT_TRUE(ServerShutdown::failed.load());
}

TEST_F(ServerWorkerShutdownTest, ZonePoolFailureStillJoinsEveryAuxiliaryWorker) {
    ObservedWorker first("first", events);
    ObservedWorker second("second", events);
    ThreadPool zones;
    auto owner = std::make_unique<ObservedWorker>("zone", events);
    auto* zone = owner.get();
    zone->stopFailure = std::make_exception_ptr(Error("zone stop failed"));
    auto zoneEntered = zone->entered.get_future();
    zones.addThread(std::move(owner));
    zones.start();
    ASSERT_EQ(std::future_status::ready, zoneEntered.wait_for(2s));
    start(first);
    start(second);
    const de::ServerWorker workers[] = {{first}, {second}};

    EXPECT_THROW(de::stopServerWorkers(workers, errors, [&] { zones.stop(); }), Error);
    EXPECT_EQ(
        (std::vector<std::string>{"first.stop", "second.stop", "zone.stop", "zone.join", "first.join", "second.join"}),
        events);
    EXPECT_TRUE(first.finished && second.finished && zone->finished);
    EXPECT_TRUE(first.joined && second.joined && zone->joined);
    EXPECT_TRUE(ServerShutdown::failed.load());
    EXPECT_NO_THROW(zones.stop());
}

TEST_F(ServerWorkerShutdownTest, StopErrorsPropagateAfterAllWorkersAndOtherDrainingAreAttempted) {
    ObservedWorker first("first", events);
    ObservedWorker second("second", events);
    first.stopFailure = std::make_exception_ptr(Error("stop failed"));
    start(first);
    start(second);
    const de::ServerWorker workers[] = {{first}, {second}};
    EXPECT_THROW(de::stopServerWorkers(workers, errors, [&] { events.emplace_back("zones"); }), Error);
    EXPECT_EQ((std::vector<std::string>{"first.stop", "second.stop", "zones", "first.join", "second.join"}), events);
    EXPECT_TRUE(first.joined && second.joined);
    EXPECT_TRUE(first.finished && second.finished);
    EXPECT_TRUE(ServerShutdown::isRequested());
    EXPECT_TRUE(ServerShutdown::failed.load());
    EXPECT_TRUE(errors.str().empty());
}

TEST_F(ServerWorkerShutdownTest, JoinErrorsFallBackToManagedJoinAndDoNotSkipRemainingWorkers) {
    ObservedWorker first("first", events);
    ObservedWorker second("second", events);
    first.joinFailure = std::make_exception_ptr(std::runtime_error("join failed"));
    start(first);
    start(second);
    const de::ServerWorker workers[] = {{first}, {second}};
    EXPECT_THROW(de::stopServerWorkers(workers, errors), std::runtime_error);
    EXPECT_EQ((std::vector<std::string>{"first.stop", "second.stop", "first.join", "second.join"}), events);
    EXPECT_TRUE(first.finished && second.finished);
    EXPECT_EQ(Thread::EXIT, first.getStatus());
    EXPECT_EQ(Thread::EXIT, second.getStatus());
    EXPECT_TRUE(second.joined);
    EXPECT_TRUE(ServerShutdown::failed.load());
    EXPECT_TRUE(errors.str().empty());
}

class Diagnostic : public Error {
public:
    explicit Diagnostic(std::function<std::string()> format) : format(std::move(format)) {}
    std::string toString() const override {
        return format();
    }

private:
    std::function<std::string()> format;
};

TEST_F(ServerWorkerShutdownTest, FormattingFailureDoesNotSkipJoinsOrLaterReports) {
    ObservedWorker first("first", events);
    ObservedWorker second("second", events);
    ObservedWorker healthy("healthy", events);
    unsigned reports[2]{};
    bool allJoinedWhenFormatting = false;
    first.runFailure = std::make_exception_ptr(Diagnostic([&]() -> std::string {
        ++reports[0];
        allJoinedWhenFormatting = first.joined && second.joined && healthy.joined;
        throw std::bad_alloc();
    }));
    second.runFailure = std::make_exception_ptr(Diagnostic([&] {
        ++reports[1];
        return "second failure";
    }));
    start(first);
    start(second);
    start(healthy);
    const de::ServerWorker workers[] = {{first}, {second}, {healthy}};

    EXPECT_THROW(de::stopServerWorkers(workers, errors), std::bad_alloc);
    EXPECT_TRUE(allJoinedWhenFormatting);
    EXPECT_EQ(reports[0], 1u);
    EXPECT_EQ(reports[1], 1u);
    EXPECT_EQ(errors.str(), "second: second failure\n");
    EXPECT_TRUE(healthy.finished);
    EXPECT_TRUE(ServerShutdown::failed.load());
}

TEST_F(ServerWorkerShutdownTest, NameFailureDoesNotSkipLaterDiagnosticsAndExplicitNamesBypassTheGetter) {
    ObservedWorker first("first", events);
    ObservedWorker second("second", events);
    first.nameFailure = second.nameFailure = std::make_exception_ptr(std::bad_alloc());
    first.runFailure = second.runFailure = std::make_exception_ptr(std::runtime_error("worker failed"));
    start(first);
    start(second);
    const de::ServerWorker workers[] = {{first}, {second, "manager"}};

    EXPECT_THROW(de::stopServerWorkers(workers, errors), std::bad_alloc);
    EXPECT_TRUE(first.joined && second.joined);
    EXPECT_EQ(first.nameAttempts, 1u);
    EXPECT_EQ(second.nameAttempts, 0u);
    EXPECT_EQ(errors.str(), "manager: worker failed\n");
}

class RefusingBuffer : public std::streambuf {
    std::streamsize xsputn(const char*, std::streamsize) override {
        return 0;
    }
    int_type overflow(int_type) override {
        return traits_type::eof();
    }
};

TEST_F(ServerWorkerShutdownTest, OutputFailureStillAttemptsEveryReportAfterAllJoins) {
    ObservedWorker first("first", events);
    ObservedWorker second("second", events);
    unsigned reports = 0;
    bool allJoinedWhenFormatting = true;
    first.runFailure = second.runFailure = std::make_exception_ptr(Diagnostic([&] {
        ++reports;
        allJoinedWhenFormatting = allJoinedWhenFormatting && first.joined && second.joined;
        return "worker failed";
    }));
    start(first);
    start(second);
    const de::ServerWorker workers[] = {{first}, {second}};
    RefusingBuffer buffer;
    std::ostream refused(&buffer);
    refused.exceptions(std::ios::badbit | std::ios::failbit);

    EXPECT_THROW(de::stopServerWorkers(workers, refused), std::ios_base::failure);
    EXPECT_TRUE(allJoinedWhenFormatting);
    EXPECT_EQ(reports, 2u);
    EXPECT_TRUE(first.joined && second.joined);
    EXPECT_TRUE(ServerShutdown::failed.load());
}

TEST_F(ServerWorkerShutdownTest, TheFirstFailureSurvivesLaterStopActionJoinAndDiagnosticErrors) {
    ObservedWorker first("first", events);
    ObservedWorker second("second", events);
    const auto original = std::make_exception_ptr(42);
    first.stopFailure = original;
    second.stopFailure = std::make_exception_ptr(Error("later stop failed"));
    first.joinFailure = std::make_exception_ptr(std::runtime_error("join failed"));
    unsigned reports = 0;
    second.runFailure = std::make_exception_ptr(Diagnostic([&]() -> std::string {
        ++reports;
        throw std::bad_alloc();
    }));
    start(first);
    start(second);
    const de::ServerWorker workers[] = {{first}, {second}};
    bool failedBeforeOtherDraining = false;

    try {
        de::stopServerWorkers(workers, errors, [&] {
            events.emplace_back("zones");
            failedBeforeOtherDraining = ServerShutdown::failed.load() && ServerShutdown::isRequested();
            throw std::logic_error("zone drain failed");
        });
        FAIL() << "expected the original stop failure";
    } catch (...) {
        EXPECT_EQ(std::current_exception(), original);
    }
    EXPECT_TRUE(failedBeforeOtherDraining);
    EXPECT_EQ((std::vector<std::string>{"first.stop", "second.stop", "zones", "first.join", "second.join"}), events);
    EXPECT_TRUE(first.finished && second.finished);
    EXPECT_EQ(Thread::EXIT, first.getStatus());
    EXPECT_EQ(Thread::EXIT, second.getStatus());
    EXPECT_EQ(reports, 1u);
}

class WorkerFailureTest : public ServerWorkerShutdownTest, public ::testing::WithParamInterface<int> {};

TEST_P(WorkerFailureTest, TheLifecyclePreservesWorkerFailureEvenAfterAllJoinsSucceed) {
    ObservedWorker failed("failed", events);
    ObservedWorker healthy("healthy", events);
    std::string expected;
    switch (GetParam()) {
    case 0:
        failed.runFailure = std::make_exception_ptr(Error("worker failed"));
        expected = Error("worker failed").toString();
        break;
    case 1:
        failed.runFailure = std::make_exception_ptr(std::runtime_error("worker failed"));
        expected = "worker failed";
        break;
    default:
        failed.runFailure = std::make_exception_ptr(42);
        expected = "unknown worker failure";
        break;
    }
    start(failed);
    start(healthy);
    const de::ServerWorker workers[] = {{failed}, {healthy}};
    const de::ServerLifecycleActions actions{
        .initialize = [] {}, .start = [] {}, .stop = [&] { de::stopServerWorkers(workers, errors); }};
    const auto result = de::runServerLifecycle(actions, output, errors);
    EXPECT_TRUE(result.drained);
    EXPECT_EQ(EXIT_FAILURE, result.exitCode);
    EXPECT_TRUE(failed.joined && healthy.joined);
    EXPECT_EQ("failed: " + expected + "\n", errors.str());
    EXPECT_TRUE(ServerShutdown::failed.load());
}

INSTANTIATE_TEST_SUITE_P(Failures, WorkerFailureTest, ::testing::Values(0, 1, 2));

} // namespace
