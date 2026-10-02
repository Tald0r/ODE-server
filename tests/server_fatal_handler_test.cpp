#include <unistd.h>

#include <atomic>
#include <csignal>
#include <cstddef>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <new>
#include <stdexcept>
#include <string>
#include <thread>

#include <gtest/gtest.h>
#include <sys/resource.h>

#include "ServerFatalHandlers.h"

namespace {

// Replacements belong only to this isolated test executable. Fault injection
// is enabled inside a death-test child after its fixture and handlers exist.
std::atomic<bool> rejectAllocations{false};
std::atomic<unsigned> rejectedAllocations{0};

void* allocate(std::size_t size, std::size_t alignment) {
    for (;;) {
        if (!rejectAllocations) {
            void* result = nullptr;
            if (alignment <= alignof(std::max_align_t))
                result = std::malloc(size == 0 ? 1 : size);
            else if (posix_memalign(&result, alignment, size == 0 ? 1 : size) != 0)
                result = nullptr;
            if (result != nullptr)
                return result;
        } else if (++rejectedAllocations > 1) {
            // A fatal handler that allocates would recurse. Give that defect a
            // distinct exit code instead of exhausting the child process stack.
            std::_Exit(98);
        }
        const auto handler = std::get_new_handler();
        if (handler == nullptr)
            throw std::bad_alloc();
        handler();
    }
}

} // namespace

void* operator new(std::size_t size) {
    return allocate(size, alignof(std::max_align_t));
}

void* operator new[](std::size_t size) {
    return ::operator new(size);
}

void* operator new(std::size_t size, std::align_val_t alignment) {
    return allocate(size, static_cast<std::size_t>(alignment));
}

void* operator new[](std::size_t size, std::align_val_t alignment) {
    return ::operator new(size, alignment);
}

void operator delete(void* pointer) noexcept {
    std::free(pointer);
}

void operator delete[](void* pointer) noexcept {
    std::free(pointer);
}

void operator delete(void* pointer, std::size_t) noexcept {
    std::free(pointer);
}

void operator delete[](void* pointer, std::size_t) noexcept {
    std::free(pointer);
}

void operator delete(void* pointer, std::align_val_t) noexcept {
    std::free(pointer);
}

void operator delete[](void* pointer, std::align_val_t) noexcept {
    std::free(pointer);
}

void operator delete(void* pointer, std::size_t, std::align_val_t) noexcept {
    std::free(pointer);
}

void operator delete[](void* pointer, std::size_t, std::align_val_t) noexcept {
    std::free(pointer);
}

namespace {

[[noreturn]] void previousNewHandler() {
    std::_Exit(85);
}

[[noreturn]] void previousTerminateHandler() {
    std::_Exit(86);
}

[[noreturn]] void unexpectedTeardown() {
    std::_Exit(87);
}

[[noreturn]] void failAllocation() {
    rejectAllocations = true;
    void* volatile pointer = ::operator new(1);
    (void)pointer;
    std::_Exit(2);
}

bool expectedMemoryExit(de::ServerKind server, int status) {
    return server == de::ServerKind::Game ? ::testing::KilledBySignal(SIGABRT)(status)
                                          : ::testing::ExitedWithCode(EXIT_FAILURE)(status);
}

class FatalHandlerFixture : public ::testing::Test {
protected:
    void SetUp() override {
        originalNew = std::set_new_handler(previousNewHandler);
        originalTerminate = std::set_terminate(previousTerminateHandler);
        std::string pattern = (std::filesystem::temp_directory_path() / "darkeden-fatal-XXXXXX").string();
        ASSERT_NE(nullptr, mkdtemp(pattern.data()));
        directory = pattern;
    }

    void TearDown() override {
        std::set_terminate(originalTerminate);
        std::set_new_handler(originalNew);
        if (!directory.empty()) {
            std::error_code error;
            std::filesystem::remove_all(directory, error);
            EXPECT_FALSE(error) << error.message();
        }
    }

    void prepareChild() const {
        struct rlimit noCore {};
        if (setrlimit(RLIMIT_CORE, &noCore) != 0 || chdir(directory.c_str()) != 0 ||
            std::atexit(unexpectedTeardown) != 0)
            std::_Exit(2);
    }

    std::string logPath() const {
        return directory + "/CriticalError.log";
    }

    std::string readLog() const {
        std::ifstream input(logPath());
        return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
    }

    void expectPreviousHandlers() {
        EXPECT_EQ(previousNewHandler, std::get_new_handler());
        EXPECT_EQ(previousTerminateHandler, std::get_terminate());
    }

    std::new_handler originalNew = nullptr;
    std::terminate_handler originalTerminate = nullptr;
    std::string directory;
};

class ServerFatalHandlerTest : public FatalHandlerFixture, public ::testing::WithParamInterface<de::ServerKind> {};

TEST_P(ServerFatalHandlerTest, InstallsTheServerPolicyAndRestoresBothPreviousHandlers) {
    {
        de::ServerFatalHandlers handlers(GetParam());
        ASSERT_NE(nullptr, std::get_new_handler());
        EXPECT_NE(previousNewHandler, std::get_new_handler());
        if (GetParam() == de::ServerKind::Game)
            EXPECT_NE(previousTerminateHandler, std::get_terminate());
        else
            EXPECT_EQ(previousTerminateHandler, std::get_terminate());
    }
    expectPreviousHandlers();
    EXPECT_FALSE(std::filesystem::exists(logPath()));
}

TEST_P(ServerFatalHandlerTest, UnwindingRestoresThePreviousHandlers) {
    EXPECT_THROW(
        {
            de::ServerFatalHandlers handlers(GetParam());
            throw std::runtime_error("startup failed");
        },
        std::runtime_error);
    expectPreviousHandlers();
}

TEST_P(ServerFatalHandlerTest, RestoresAnOriginallyAbsentAllocationHandler) {
    std::set_new_handler(nullptr);
    {
        de::ServerFatalHandlers handlers(GetParam());
        ASSERT_NE(nullptr, std::get_new_handler());
    }
    EXPECT_EQ(nullptr, std::get_new_handler());
    EXPECT_EQ(previousTerminateHandler, std::get_terminate());
}

INSTANTIATE_TEST_SUITE_P(Servers, ServerFatalHandlerTest,
                         ::testing::Values(de::ServerKind::Game, de::ServerKind::Login, de::ServerKind::Shared));

TEST_F(FatalHandlerFixture, NestedServiceHandlersPreserveAndRestoreTheOuterGamePolicy) {
    {
        de::ServerFatalHandlers game(de::ServerKind::Game);
        const auto gameNew = std::get_new_handler();
        const auto gameTerminate = std::get_terminate();
        {
            de::ServerFatalHandlers login(de::ServerKind::Login);
            EXPECT_NE(gameNew, std::get_new_handler());
            EXPECT_EQ(gameTerminate, std::get_terminate());
        }
        EXPECT_EQ(gameNew, std::get_new_handler());
        EXPECT_EQ(gameTerminate, std::get_terminate());
    }
    expectPreviousHandlers();
}

class ServerFatalHandlerDeathTest : public FatalHandlerFixture, public ::testing::WithParamInterface<de::ServerKind> {};

TEST_P(ServerFatalHandlerDeathTest, FailedAllocationReportsFailureWithoutAllocatingOrRunningExitCallbacks) {
    const auto server = GetParam();
    std::ofstream(logPath()) << "previous diagnostic\n";
    ASSERT_EXIT(
        {
            prepareChild();
            de::ServerFatalHandlers handlers(server);
            failAllocation();
        },
        [server](int status) { return expectedMemoryExit(server, status); }, "CRITICAL ERROR! NOT ENOUGH MEMORY!");
    EXPECT_EQ(server == de::ServerKind::Game ? "previous diagnostic\nCRITICAL ERROR! NOT ENOUGH MEMORY!\n"
                                             : "previous diagnostic\n",
              readLog());
}

TEST_P(ServerFatalHandlerDeathTest, AllocationFailureOnAWorkerTerminatesTheProcess) {
    const auto server = GetParam();
    ASSERT_EXIT(
        {
            prepareChild();
            de::ServerFatalHandlers handlers(server);
            std::jthread worker(failAllocation);
            worker.join();
            std::_Exit(2);
        },
        [server](int status) { return expectedMemoryExit(server, status); }, "CRITICAL ERROR! NOT ENOUGH MEMORY!");
    EXPECT_EQ(server == de::ServerKind::Game ? "CRITICAL ERROR! NOT ENOUGH MEMORY!\n" : "", readLog());
}

TEST_P(ServerFatalHandlerDeathTest, AClosedStandardErrorDoesNotPreventFatalTermination) {
    const auto server = GetParam();
    ASSERT_EXIT(
        {
            prepareChild();
            de::ServerFatalHandlers handlers(server);
            if (close(STDERR_FILENO) != 0)
                std::_Exit(2);
            failAllocation();
        },
        [server](int status) { return expectedMemoryExit(server, status); }, "");
    EXPECT_EQ(server == de::ServerKind::Game ? "CRITICAL ERROR! NOT ENOUGH MEMORY!\n" : "", readLog());
}

TEST_P(ServerFatalHandlerDeathTest, TerminationUsesTheGameHandlerOrPreservesTheServiceHandler) {
    const auto server = GetParam();
    const char* diagnostic = server == de::ServerKind::Game ? "UNHANDLED EXCEPTION OCCURED" : "";
    ASSERT_EXIT(
        {
            prepareChild();
            de::ServerFatalHandlers handlers(server);
            rejectAllocations = true;
            std::terminate();
        },
        [server](int status) {
            return server == de::ServerKind::Game ? ::testing::KilledBySignal(SIGABRT)(status)
                                                  : ::testing::ExitedWithCode(86)(status);
        },
        diagnostic);
    EXPECT_EQ(server == de::ServerKind::Game ? "UNHANDLED EXCEPTION OCCURED\n" : "", readLog());
}

INSTANTIATE_TEST_SUITE_P(Servers, ServerFatalHandlerDeathTest,
                         ::testing::Values(de::ServerKind::Game, de::ServerKind::Login, de::ServerKind::Shared));

class GameFatalLoggingDeathTest : public FatalHandlerFixture {};

TEST_F(GameFatalLoggingDeathTest, AllocationFailureStillAbortsWhenTheLogCannotBeOpened) {
    ASSERT_TRUE(std::filesystem::create_directory(logPath()));
    ASSERT_EXIT(
        {
            prepareChild();
            de::ServerFatalHandlers handlers(de::ServerKind::Game);
            failAllocation();
        },
        ::testing::KilledBySignal(SIGABRT), "CRITICAL ERROR! NOT ENOUGH MEMORY!");
}

TEST_F(GameFatalLoggingDeathTest, TerminationStillAbortsWhenTheLogCannotBeOpened) {
    ASSERT_TRUE(std::filesystem::create_directory(logPath()));
    ASSERT_EXIT(
        {
            prepareChild();
            de::ServerFatalHandlers handlers(de::ServerKind::Game);
            rejectAllocations = true;
            std::terminate();
        },
        ::testing::KilledBySignal(SIGABRT), "UNHANDLED EXCEPTION OCCURED");
}

} // namespace
