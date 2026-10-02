#include "AllocationProbe.h"

#include <cstdlib>
#include <new>

namespace {
struct ProbeState {
    bool active = false;
    bool rejected = false;
    std::size_t failAt = 0;
    std::size_t attempts = 0;
    std::size_t live = 0;
    void* allocations[512]{};
};
thread_local ProbeState state;

void* allocate(std::size_t size, std::size_t alignment) {
    if (state.active && ++state.attempts == state.failAt) {
        state.rejected = true;
        throw std::bad_alloc();
    }
    void* pointer = nullptr;
    for (;;) {
        if (alignment <= alignof(std::max_align_t))
            pointer = std::malloc(size == 0 ? 1 : size);
        else if (posix_memalign(&pointer, alignment, size == 0 ? 1 : size) != 0)
            pointer = nullptr;
        if (pointer)
            break;
        const auto handler = std::get_new_handler();
        if (!handler)
            throw std::bad_alloc();
        handler();
    }
    if (state.active) {
        for (auto& allocation : state.allocations) {
            if (!allocation) {
                allocation = pointer;
                ++state.live;
                return pointer;
            }
        }
        std::_Exit(97); // bookkeeping capacity exceeded, never hide a leak
    }
    return pointer;
}

void release(void* pointer) noexcept {
    if (pointer && state.live) {
        for (auto& allocation : state.allocations) {
            if (allocation == pointer) {
                allocation = nullptr;
                --state.live;
                break;
            }
        }
    }
    std::free(pointer);
}
} // namespace

AllocationProbe::AllocationProbe(std::size_t failAt) noexcept {
    if (state.active || state.live)
        std::_Exit(98); // nested probes or a previous operation left allocations
    state.active = true;
    state.rejected = false;
    state.failAt = failAt;
    state.attempts = 0;
}

AllocationProbe::~AllocationProbe() {
    state.active = false;
}

std::size_t AllocationProbe::attempts() const noexcept {
    return state.attempts;
}

std::size_t AllocationProbe::outstanding() const noexcept {
    return state.live;
}

bool AllocationProbe::rejected() const noexcept {
    return state.rejected;
}

void AllocationProbe::stopFailing() noexcept {
    state.failAt = 0;
}

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
    release(pointer);
}
void operator delete[](void* pointer) noexcept {
    release(pointer);
}
void operator delete(void* pointer, std::size_t) noexcept {
    release(pointer);
}
void operator delete[](void* pointer, std::size_t) noexcept {
    release(pointer);
}
void operator delete(void* pointer, std::align_val_t) noexcept {
    release(pointer);
}
void operator delete[](void* pointer, std::align_val_t) noexcept {
    release(pointer);
}
void operator delete(void* pointer, std::size_t, std::align_val_t) noexcept {
    release(pointer);
}
void operator delete[](void* pointer, std::size_t, std::align_val_t) noexcept {
    release(pointer);
}
