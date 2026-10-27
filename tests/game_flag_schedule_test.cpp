//////////////////////////////////////////////////////////////////////////////
// Filename    : game_flag_schedule_test.cpp
// Description : FlagManager's schedule queue under two threads: the main
//               thread's heartbeat and a GM's *command CTF, which pulls the
//               earliest schedule forward from a zone thread. The manager
//               takes the due schedule out of the queue before running it and
//               takes the queue's mutex for every pop and add, so whatever
//               the interleaving, each run and each pull keeps the number of
//               queued schedules, and each work stays in exactly one.
//////////////////////////////////////////////////////////////////////////////

#include <set>
#include <thread>

#include <gtest/gtest.h>

#include "GameContext.h"
#include "VSDateTime.h"
#include "ctf/FlagManager.h"
#include "war/Schedule.h"
#include "war/Work.h"

namespace {

// Puts itself back in the queue, due at once, each time it runs, as a flag
// war's step adds its next one.
class Requeue : public Work {
public:
    explicit Requeue(FlagManager& manager) : m_Manager(manager) {}

    void execute() override {
        m_Manager.addSchedule(new Schedule(this, VSDateTime::currentDateTime()));
    }
    string toString() const override {
        return "Requeue";
    }

private:
    FlagManager& m_Manager;
};

// Reads which works the queue holds.
class InspectedFlagManager : public FlagManager {
public:
    using FlagManager::FlagManager;

    std::set<const Work*> queuedWorks() const {
        std::set<const Work*> works;
        for (const Schedule* pSchedule : m_RecentSchedules.getSchedules())
            works.insert(pSchedule->getWork());
        return works;
    }
};

TEST(GameFlagSchedule, HeartbeatAndManualStartOnTwoThreadsKeepTheQueue) {
    constexpr int kRounds = 2000;

    de::GameContext context;
    InspectedFlagManager manager(context);
    // Two schedules, so a pull always finds one while the other runs and
    // never makes a flag war of its own.
    manager.clear();
    manager.addSchedule(new Schedule(new Requeue(manager), VSDateTime::currentDateTime()));
    manager.addSchedule(new Schedule(new Requeue(manager), VSDateTime::currentDateTime()));

    std::thread gm([&manager] {
        for (int i = 0; i < kRounds; ++i)
            manager.manualStart();
    });
    for (int i = 0; i < kRounds; ++i)
        manager.heartbeat();
    gm.join();

    EXPECT_EQ(manager.getSize(), 2);
    std::set<const Work*> works = manager.queuedWorks();
    EXPECT_EQ(works.size(), 2u);
    EXPECT_EQ(works.count(nullptr), 0u);
}

} // namespace
