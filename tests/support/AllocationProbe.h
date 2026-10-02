#ifndef DARKEDEN_TEST_ALLOCATION_PROBE_H
#define DARKEDEN_TEST_ALLOCATION_PROBE_H

#include <cstddef>

// This probe replaces allocation only in the test executables that link its
// implementation. Use inside a single-threaded death-test child, with no gtest
// calls while armed. It fails one allocation and tracks surviving allocations
// without allocating its own bookkeeping. A zero failure index only tracks.
class AllocationProbe {
public:
    explicit AllocationProbe(std::size_t failAt = 0) noexcept;
    ~AllocationProbe();
    AllocationProbe(const AllocationProbe&) = delete;
    AllocationProbe& operator=(const AllocationProbe&) = delete;

    std::size_t attempts() const noexcept;
    std::size_t outstanding() const noexcept;
    bool rejected() const noexcept;
    void stopFailing() noexcept;
};

#endif
