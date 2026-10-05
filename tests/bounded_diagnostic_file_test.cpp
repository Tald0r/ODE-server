#include <pthread.h>
#include <unistd.h>

#include <csignal>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <set>
#include <string>

#include <gtest/gtest.h>
#include <sys/resource.h>
#include <sys/wait.h>

#include "BoundedDiagnosticFile.h"
#include "Utility.h"

namespace {
class BoundedDiagnosticFile : public ::testing::Test {
protected:
    void SetUp() override {
        std::string pattern = (std::filesystem::temp_directory_path() / "diagnostic-storage-XXXXXX").string();
        ASSERT_NE(nullptr, ::mkdtemp(pattern.data()));
        directory = pattern;
        path = directory / "events.log";
    }
    void TearDown() override {
        std::filesystem::remove_all(directory);
    }
    std::string contents(const std::filesystem::path& file) {
        std::ifstream input(file);
        return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
    }
    std::filesystem::path directory, path;
};

TEST_F(BoundedDiagnosticFile, RotationKeepsCurrentAndTwoBoundedBackups) {
    for (unsigned value = 0; value < 8; ++value) {
        const std::string record = std::to_string(value) + std::string(14, 'x') + "\n";
        ASSERT_TRUE(de::appendBoundedDiagnosticFile(path.c_str(), record, {32, 2}));
    }
    EXPECT_EQ(contents(path), "6xxxxxxxxxxxxxx\n7xxxxxxxxxxxxxx\n");
    EXPECT_EQ(contents(path.string() + ".1"), "4xxxxxxxxxxxxxx\n5xxxxxxxxxxxxxx\n");
    EXPECT_EQ(contents(path.string() + ".2"), "2xxxxxxxxxxxxxx\n3xxxxxxxxxxxxxx\n");
    EXPECT_FALSE(std::filesystem::exists(path.string() + ".3"));
    for (const auto& entry : std::filesystem::directory_iterator(directory))
        EXPECT_LE(entry.file_size(), 32u);
}

TEST_F(BoundedDiagnosticFile, AnOversizedRecordIsVisiblyTruncatedAndTheNextAppendRotatesIt) {
    ASSERT_TRUE(de::appendBoundedDiagnosticFile(path.c_str(), std::string(1024, 'x'), {64, 2}));
    EXPECT_EQ(std::filesystem::file_size(path), 64u);
    EXPECT_TRUE(contents(path).ends_with(" [truncated]\n"));
    ASSERT_TRUE(de::appendBoundedDiagnosticFile(path.c_str(), "next\n", {64, 2}));
    EXPECT_EQ(contents(path), "next\n");
    EXPECT_EQ(std::filesystem::file_size(path.string() + ".1"), 64u);
}

TEST_F(BoundedDiagnosticFile, LegacyAndDiagnosticProcessesAppendWholeRecordsThroughOneStableLock) {
    pid_t children[4];
    for (unsigned child = 0; child < 4; ++child) {
        children[child] = ::fork();
        ASSERT_GE(children[child], 0);
        if (children[child] == 0) {
            for (unsigned line = 0; line < 40; ++line) {
                const auto record = std::to_string(child) + ":" + std::to_string(line) + "\n";
                const bool appended = child % 2 == 0 ? de::appendBoundedDiagnosticFile(path.c_str(), record, {65536, 2})
                                                     : de::appendUnrotatedLogFile(path.c_str(), record);
                if (!appended)
                    std::_Exit(1);
            }
            std::_Exit(0);
        }
    }
    for (const auto child : children) {
        int status;
        ASSERT_EQ(::waitpid(child, &status, 0), child);
        ASSERT_TRUE(WIFEXITED(status));
        ASSERT_EQ(WEXITSTATUS(status), 0);
    }
    std::ifstream input(path);
    std::set<std::string> lines;
    for (std::string line; std::getline(input, line);)
        EXPECT_TRUE(lines.insert(line).second);
    EXPECT_EQ(lines.size(), 160u);
    for (unsigned child = 0; child < 4; ++child)
        for (unsigned line = 0; line < 40; ++line)
            EXPECT_TRUE(lines.contains(std::to_string(child) + ":" + std::to_string(line)));
}

TEST_F(BoundedDiagnosticFile, IndependentWritersRotateWithoutSplittingOrMixingRecords) {
    pid_t children[3];
    for (unsigned child = 0; child < 3; ++child) {
        children[child] = ::fork();
        ASSERT_GE(children[child], 0);
        if (children[child] == 0) {
            const std::string record = std::string(63, static_cast<char>('A' + child)) + "\n";
            for (unsigned line = 0; line < 40; ++line)
                if (!de::appendBoundedDiagnosticFile(path.c_str(), record, {128, 2}))
                    std::_Exit(1);
            std::_Exit(0);
        }
    }
    for (const auto child : children) {
        int status;
        ASSERT_EQ(::waitpid(child, &status, 0), child);
        ASSERT_TRUE(WIFEXITED(status));
        ASSERT_EQ(WEXITSTATUS(status), 0);
    }
    for (const std::string suffix : {"", ".1", ".2"}) {
        const auto file = path.string() + suffix;
        ASSERT_EQ(std::filesystem::file_size(file), 128u);
        std::ifstream input(file);
        for (std::string line; std::getline(input, line);) {
            ASSERT_EQ(line.size(), 63u);
            EXPECT_TRUE(line == std::string(63, 'A') || line == std::string(63, 'B') || line == std::string(63, 'C'));
        }
    }
}

TEST_F(BoundedDiagnosticFile, RotationFailurePreservesCurrentDataAndCanRetry) {
    ASSERT_TRUE(de::appendBoundedDiagnosticFile(path.c_str(), "current record\n", {24, 2}));
    std::ofstream(path.string() + ".1") << "previous record\n";
    std::filesystem::create_directory(path.string() + ".2");
    std::ofstream(path.string() + ".2/block") << "occupied";
    EXPECT_FALSE(de::appendBoundedDiagnosticFile(path.c_str(), "next record\n", {24, 2}));
    EXPECT_EQ(contents(path), "current record\n");
    EXPECT_EQ(contents(path.string() + ".1"), "previous record\n");
    std::filesystem::remove_all(path.string() + ".2");
    EXPECT_TRUE(de::appendBoundedDiagnosticFile(path.c_str(), "next record\n", {24, 2}));
    EXPECT_EQ(contents(path), "next record\n");
    EXPECT_EQ(contents(path.string() + ".1"), "current record\n");
}

TEST_F(BoundedDiagnosticFile, MissingAndUnwritableSinksDoNotThrowAndCanRecover) {
    auto missing = directory / "missing/events.log";
    EXPECT_FALSE(de::appendBoundedDiagnosticFile(missing.c_str(), "record\n"));
    EXPECT_FALSE(de::appendUnrotatedLogFile(missing.c_str(), "record\n"));
    EXPECT_NO_THROW(filelog(missing.c_str(), "%s", "record"));
    EXPECT_NO_THROW(diagnosticFilelog(missing.c_str(), "%s", "record"));
    std::filesystem::create_directory(missing.parent_path());
    EXPECT_TRUE(de::appendBoundedDiagnosticFile(missing.c_str(), "recovered\n"));
    std::filesystem::create_directory(path);
    EXPECT_FALSE(de::appendBoundedDiagnosticFile(path.c_str(), "record\n"));
    EXPECT_FALSE(de::appendUnrotatedLogFile(path.c_str(), "record\n"));
    std::filesystem::remove(path);
    EXPECT_TRUE(de::appendBoundedDiagnosticFile(path.c_str(), "recovered\n"));
}

TEST_F(BoundedDiagnosticFile, AFullDeviceReportsFailureWithoutThrowing) {
    if (!std::filesystem::exists("/dev/full"))
        GTEST_SKIP() << "platform has no full-device test sink";
    std::filesystem::create_symlink("/dev/full", path);
    EXPECT_FALSE(de::appendBoundedDiagnosticFile(path.c_str(), "record\n"));
    EXPECT_FALSE(de::appendUnrotatedLogFile(path.c_str(), "record\n"));
    EXPECT_NO_THROW(filelog(path.c_str(), "%s", "record"));
    EXPECT_NO_THROW(diagnosticFilelog(path.c_str(), "%s", "record"));
    std::filesystem::remove(path);
    EXPECT_TRUE(de::appendBoundedDiagnosticFile(path.c_str(), "recovered\n"));
}

TEST_F(BoundedDiagnosticFile, PartialWriteFailureDoesNotHoldTheLockOrPreventRetry) {
    ASSERT_EXIT(
        {
            ::signal(SIGXFSZ, SIG_IGN);
            rlimit original{};
            if (::getrlimit(RLIMIT_FSIZE, &original) != 0)
                std::_Exit(1);
            rlimit limited = original;
            limited.rlim_cur = 32;
            if (::setrlimit(RLIMIT_FSIZE, &limited) != 0)
                std::_Exit(2);
            const bool refused = !de::appendBoundedDiagnosticFile(path.c_str(), std::string(64, 'x'));
            const bool restored = ::setrlimit(RLIMIT_FSIZE, &original) == 0;
            const bool retried = de::appendBoundedDiagnosticFile(path.c_str(), "retry\n");
            std::_Exit(refused && restored && retried ? 0 : 3);
        },
        ::testing::ExitedWithCode(0), "");
    EXPECT_TRUE(contents(path).ends_with("retry\n"));
}

TEST_F(BoundedDiagnosticFile, ClosedStderrPipeCannotRaiseSigpipeBeforeProcessInitialization) {
    ASSERT_EXIT(
        {
            ::signal(SIGPIPE, SIG_DFL);
            sigset_t signal;
            ::sigemptyset(&signal);
            ::sigaddset(&signal, SIGPIPE);
            if (::pthread_sigmask(SIG_UNBLOCK, &signal, nullptr) != 0)
                std::_Exit(1);
            int pipe[2];
            if (::pipe(pipe) != 0 || ::dup2(pipe[1], STDERR_FILENO) < 0)
                std::_Exit(2);
            ::close(pipe[0]);
            ::close(pipe[1]);
            de::detail::writeDiagnosticStderr("fallback\n");
            sigset_t mask;
            sigset_t pending;
            if (::pthread_sigmask(SIG_BLOCK, nullptr, &mask) != 0 || ::sigpending(&pending) != 0)
                std::_Exit(3);
            std::_Exit(::sigismember(&mask, SIGPIPE) == 0 && ::sigismember(&pending, SIGPIPE) == 0 ? 0 : 4);
        },
        ::testing::ExitedWithCode(0), "");
}

TEST_F(BoundedDiagnosticFile, FallbackPreservesAnAlreadyBlockedAndPendingSigpipe) {
    ASSERT_EXIT(
        {
            ::signal(SIGPIPE, SIG_DFL);
            sigset_t signal;
            ::sigemptyset(&signal);
            ::sigaddset(&signal, SIGPIPE);
            if (::pthread_sigmask(SIG_BLOCK, &signal, nullptr) != 0 || ::raise(SIGPIPE) != 0)
                std::_Exit(1);
            int pipe[2];
            if (::pipe(pipe) != 0 || ::dup2(pipe[1], STDERR_FILENO) < 0)
                std::_Exit(2);
            ::close(pipe[0]);
            ::close(pipe[1]);
            de::detail::writeDiagnosticStderr("fallback\n");
            sigset_t mask;
            sigset_t pending;
            if (::pthread_sigmask(SIG_BLOCK, nullptr, &mask) != 0 || ::sigpending(&pending) != 0)
                std::_Exit(3);
            std::_Exit(::sigismember(&mask, SIGPIPE) == 1 && ::sigismember(&pending, SIGPIPE) == 1 ? 0 : 4);
        },
        ::testing::ExitedWithCode(0), "");
}

TEST_F(BoundedDiagnosticFile, LegacyFilelogPreservesHistoryAndOnlyExplicitDiagnosticsRotate) {
    const auto diagnostics = directory / "other-events.log";
    const auto limit = de::DiagnosticFileLimits{}.bytes;
    for (const auto& file : {path, diagnostics}) {
        std::ofstream(file) << "original history\n";
        // A sparse historical file exercises the public default without a huge
        // diagnostic payload or any dependency on the destination's name.
        std::filesystem::resize_file(file, limit + 1);
    }

    EXPECT_NO_THROW(filelog(path.c_str(), "event %d", 42));
    EXPECT_GT(std::filesystem::file_size(path), limit + 1);
    EXPECT_TRUE(contents(path).starts_with("original history\n"));
    EXPECT_TRUE(contents(path).ends_with(" : event 42\n"));
    EXPECT_FALSE(std::filesystem::exists(path.string() + ".1"));

    EXPECT_NO_THROW(diagnosticFilelog(diagnostics.c_str(), "event %d", 42));
    EXPECT_LT(std::filesystem::file_size(diagnostics), 200u);
    EXPECT_TRUE(contents(diagnostics).ends_with(" : event 42\n"));
    EXPECT_EQ(std::filesystem::file_size(diagnostics.string() + ".1"), limit + 1);
    EXPECT_TRUE(contents(diagnostics.string() + ".1").starts_with("original history\n"));
}

TEST_F(BoundedDiagnosticFile, FilelogKeepsTimestampAndContentAndTruncatesHugeMessages) {
    EXPECT_NO_THROW(filelog(path.c_str(), "event %d", 42));
    EXPECT_TRUE(contents(path).ends_with(" : event 42\n"));
    const std::string large(40000, 'x');
    EXPECT_NO_THROW(filelog(path.c_str(), "%s", large.c_str()));
    EXPECT_TRUE(contents(path).ends_with(" [truncated]\n"));
    EXPECT_LT(std::filesystem::file_size(path), 30200u);
}
} // namespace
