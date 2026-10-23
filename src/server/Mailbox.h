//////////////////////////////////////////////////////////////////////////////
// Filename    : Mailbox.h
// Description : A queue of items posted from any thread and consumed by one
//               owning thread at a point of its choosing. This is the
//               "cross-group communication via queues only" primitive of
//               the thread-ownership contract
//               (docs/threading/cross-thread-communication.md): a thread
//               that must touch state it does not own posts to the owner's
//               box; the owner drains the box while holding its own lock, so
//               the work runs where the mutation is legal. GamePlayer owns
//               one for per-player work (PlayerMailbox.h).
//
//               The box is unbounded on purpose. Its producers are the
//               inter-server links (SharedServerManager, LoginServerManager
//               threads); blocking them on gameplay back-pressure would stall
//               every guild/login message for every zone group. The owner
//               watches the depth instead (the batch size drain() returns)
//               and logs when it is out of the ordinary.
//////////////////////////////////////////////////////////////////////////////

#ifndef DARKEDEN_MAILBOX_H
#define DARKEDEN_MAILBOX_H

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <functional>
#include <iterator>
#include <mutex>
#include <utility>
#include <vector>

namespace de {

template <typename Item> class Mailbox {
public:
    // Thread-safe. Never blocks on anything but the box's own mutex, which
    // is held only for the push, so a producer that already holds other
    // locks (the PCFinder lock, a group mutex) cannot deadlock here.
    void post(Item item) {
        std::lock_guard lock(m_Mutex);
        m_Pending.push_back(std::move(item));
        m_HasItems.store(true, std::memory_order_relaxed);
    }

    // Lock-free hint for the owner's hot loop: true when nothing has been
    // posted since the last drain. A post racing this read is seen on the
    // next drain -- never lost, since the flag is written under the mutex.
    bool empty() const {
        return !m_HasItems.load(std::memory_order_relaxed);
    }

    // Takes every item that was posted before the call out of the box and
    // hands each to `visit`, in posting order, on the calling thread. Items
    // posted while draining (including by the visited items themselves)
    // wait for the next drain. `onFailure` is called with the exception in
    // flight when a visit throws, and draining continues with the next
    // item; without it the exception propagates and the rest of this batch
    // is dropped. Returns the number of items taken.
    template <typename Visit, typename OnFailure> std::size_t drain(Visit&& visit, OnFailure&& onFailure) {
        return drainIf([](const Item&) { return true; }, std::forward<Visit>(visit),
                       std::forward<OnFailure>(onFailure));
    }

    template <typename Visit> std::size_t drain(Visit&& visit) {
        return drain(std::forward<Visit>(visit), [] { throw; });
    }

    // drain() restricted to the items `take` accepts. The others stay in
    // the box in their original order, ahead of anything posted later, so
    // an owner that may only run some kinds of item (the main thread and
    // player-scoped commands, PlayerMailbox.h) never reorders the rest.
    template <typename Take, typename Visit, typename OnFailure>
    std::size_t drainIf(Take&& take, Visit&& visit, OnFailure&& onFailure) {
        std::vector<Item> batch = takeBatch(std::forward<Take>(take));
        for (Item& item : batch) {
            try {
                visit(item);
            } catch (...) {
                onFailure();
            }
        }
        return batch.size();
    }

    std::size_t size() const {
        std::lock_guard lock(m_Mutex);
        return m_Pending.size();
    }

private:
    template <typename Take> std::vector<Item> takeBatch(Take&& take) {
        std::vector<Item> batch;
        std::lock_guard lock(m_Mutex);
        auto kept =
            std::stable_partition(m_Pending.begin(), m_Pending.end(), [&](const Item& item) { return !take(item); });
        batch.assign(std::make_move_iterator(kept), std::make_move_iterator(m_Pending.end()));
        m_Pending.erase(kept, m_Pending.end());
        m_HasItems.store(!m_Pending.empty(), std::memory_order_relaxed);
        return batch;
    }

    mutable std::mutex m_Mutex;
    std::vector<Item> m_Pending;
    std::atomic<bool> m_HasItems{false};
};

// The plain form: a queue of thunks the owner just runs.
using CommandMailbox = Mailbox<std::function<void()>>;

} // namespace de

#endif // DARKEDEN_MAILBOX_H
