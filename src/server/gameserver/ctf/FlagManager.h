///////////////////////////////////////////////////////////////////
// FlagManager.h
//
// Only one may run globally. Its schedule runs on the main thread
// (ClientManager), while a GM's *command CTF pulls it forward from a zone
// thread or the login link's, so every pop and add those threads make takes
// m_ScheduleMutex. The flag war it runs changes zones only through
// commands posted to the zones' groups (war/WarZoneWork.h): the flags it
// drops and takes back, the pole sweeps and the status broadcasts. The
// counts, the put times and the status packet are also written by the zone
// threads that plant and pull flags, so they are read and written under
// m_Mutex; the zones a war allows flags in are a published snapshot.
///////////////////////////////////////////////////////////////////

#ifndef __FLAG_MANAGER_H__
#define __FLAG_MANAGER_H__

#include <atomic>
#include <map>
#include <vector>

#include "Assert.h"
#include "Exception.h"
#include "GCFlagWarStatus.h"
#include "GameContext.h"
#include "Mutex.h"
#include "Snapshot.h"
#include "Types.h"
#include "VSDateTime.h"
#include "ctf/FlagWarPlan.h"
#include "war/Scheduler.h"
#include "war/Work.h"

class PlayerCreature;
class MonsterCorpse;
class Zone;
class Item;

class FlagManager : public Scheduler {
    enum RACEINDEX { SLAYER, VAMPIRE, OUSTERS, NONE };

    struct PoleFieldInfo {
        ZoneID_t zoneID;
        ZoneCoord_t l, t, w, h;

        PoleFieldInfo() {
            zoneID = l = t = w = h = 0;
        }
        PoleFieldInfo(ZoneID_t zID, ZoneCoord_t left, ZoneCoord_t top, ZoneCoord_t width, ZoneCoord_t height) {
            zoneID = zID;
            l = left;
            t = top;
            w = width;
            h = height;
        }
        bool isInField(ZONE_COORD pos) {
            return pos.id == zoneID && (pos.x >= l && pos.x < l + w) && (pos.y >= t && pos.y < t + h);
        }
    };

public:
    explicit FlagManager(de::GameContext& context);
    virtual ~FlagManager();

public:
    void init();
    bool putFlag(PlayerCreature* pPC, MonsterCorpse* pFlagPole);
    bool getFlag(PlayerCreature* pPC, MonsterCorpse* pFlagPole);

    void setNextSchedule();

    // Runs the earliest schedule once it is due. It is taken out of the queue
    // under m_ScheduleMutex and run after the mutex is released, so a war's
    // step adds its next schedule, and a GM's manualStart pulls one forward,
    // without meeting the schedule that is running. A run that throws puts
    // the schedule back for the next heartbeat.
    virtual Work* heartbeat();
    // Takes m_ScheduleMutex; it hides Scheduler's unlocked add.
    void addSchedule(Schedule* pSchedule);

    void manualStart();

public:
    bool hasFlagWar() const {
        return m_bHasFlagWar;
    }
    Race_t getWinnerRace() const;
    // The winner and its count read together, so the pair a notice carries
    // agrees.
    Race_t getWinnerRace(uint& count) const;
    uint getFlagCount(Race_t race) const;

    // warSecs is how long the war starting now runs.
    bool startFlagWar(int warSecs);
    bool endFlagWar();

    bool isFlagPole(MonsterCorpse* pFlagPole) {
        return m_FlagPoles.find(pFlagPole) != m_FlagPoles.end();
    }
    bool isFlagAllowedZone(ZoneID_t ZoneID) const {
        auto pAllowed = m_FlagAllowMap.load();
        return pAllowed->find(ZoneID) != pAllowed->end();
    }

    // Publishes the zones the war starting now lets flags into.
    void setAllowedZones(std::map<ZoneID_t, uint> allowed);

    // The war's status as it stands, a copy the caller may send.
    GCFlagWarStatus statusPacket() const;

    // Sends the status to the players of every zone the war allows flags in,
    // posted to each zone's group.
    void broadcastStatus() const;
    const map<MonsterCorpse*, Race_t>& getFlagPoleRaceMap() const {
        return m_FlagPoles;
    }

    Race_t getFlagPoleRace(MonsterCorpse* pFlagPole) {
        return m_FlagPoles[pFlagPole];
    }

    bool putFlag(PlayerCreature* pPC, Item* pItem, MonsterCorpse* pFlagPole);
    // Off the main thread, the caller holds m_Mutex: the war's start writes
    // m_EndTime under it.
    int remainWarTimeSecs() const {
        return VSDateTime::currentDateTime().secsTo(m_EndTime);
    }

    //	VSDateTime getNextFlagWarTime();

    // Zeroes the counts and deletes the round's flag statistics.
    void resetFlagCounts();
    void deleteFlagWarStats();

    // Posts to the pole zone zoneID the sweep that takes out the flags still
    // planted on its poles and destroys them.
    void postPoleSweep(ZoneID_t zoneID) const;

    // The pole fields init() laid out; they do not change afterwards.
    std::vector<de::ctf::PoleField> getPoleFields() const;

    bool isInPoleField(ZONE_COORD zc);

    void recordPutFlag(PlayerCreature* pPC, Item* pItem);
    void recordFlagWarHistory();

public:
    void lock() {
        m_Mutex.lock();
    }
    void unlock() {
        m_Mutex.unlock();
    }

protected:
    void addPoleField(Zone* pZone, ZoneCoord_t left, ZoneCoord_t top, uint width, uint height, Race_t race,
                      MonsterType_t type);

private:
    de::GameContext& m_Context;

    map<RACEINDEX, uint> m_FlagCount;
    // The winner by count, the latest planter winning a tie; the caller holds
    // m_Mutex.
    RACEINDEX winnerRace_LOCKED() const;

    mutable Mutex m_Mutex;
    // The schedule queue's lock, a leaf: nothing else is taken under it.
    mutable Mutex m_ScheduleMutex;

    // No more races will be added, surely?
    map<MonsterCorpse*, Race_t> m_FlagPoles;
    GCFlagWarStatus m_StatusPacket;

    de::Snapshot<std::map<ZoneID_t, uint>> m_FlagAllowMap;

    VSDateTime m_EndTime;
    map<Race_t, VSDateTime> m_PutTime;

    list<PoleFieldInfo> m_PoleFields;

    std::atomic<bool> m_bHasFlagWar{false};
};

#endif
