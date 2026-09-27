#ifndef __WAR_SYSTEM_H__
#define __WAR_SYSTEM_H__

#include <atomic>

#include "Exception.h"
#include "GCWarList.h"
#include "Mutex.h"
#include "Scheduler.h"
#include "Types.h"

class War;
class WarSchedule;
class Player;
class GamePlayer;
class PlayerCreature;
struct WarScheduleInfo;

class ActiveWarInfo {
public:
    ActiveWarInfo(ZoneID_t zoneID, GuildID_t attackGuildID = 0) : ZoneID(zoneID), AttackGuildID(attackGuildID) {}

    bool operator==(const ActiveWarInfo& awi) const {
        return ZoneID == awi.ZoneID;
    }

    bool operator!=(const ActiveWarInfo& awi) const {
        return ZoneID != awi.ZoneID;
    }

public:
    ZoneID_t ZoneID;
    GuildID_t AttackGuildID;
};

// The wars running on this server. A castle zone's scheduler hands each
// castle war over when it starts (addWarDelayed); the race war is scheduled
// here. The main thread's heartbeat ends a war when its time runs out, or
// once endWar has moved its end to now, and frees it.
//
// Locks, and the order they are taken in:
//
// - m_Mutex guards the schedules -- the running wars -- and the race war's
//   schedule. The heartbeat holds it while it starts and ends wars, and
//   under it takes: the holy land and shrine managers' mutexes, the player
//   finder's lock, every group's ZonePlayerManager mutex, zones' object
//   registries and cross-thread effect queues
//   (the dragon eyes laid out at the race war's start), the client manager's
//   event mutex, the guild manager's mutex (guild names), the mailboxes'
//   own mutexes, and the three mutexes below. It takes no zone group's mutex,
//   no zone's own mutex and no castle scheduler's: every change a war's start
//   or end makes to a zone is posted to the zone's group (war/WarZoneWork.h)
//   -- a castle's owner change and fee (CastleInfoManager::postCastleWarEnd),
//   the castle symbols' and blood bibles' returns, which go first to
//   whichever group or player holds each one, the shrine shields, the siege
//   zone's reset, the castles' safe zones and transports, and the holy
//   land's time, monsters, players, regen zone towers and join flags. The
//   broadcasts to the players of the holy land, of a castle or of a level
//   war's bonus zones are posted the same way (de::war::postBroadcast):
//   the packet's body is captured on the calling thread and each zone's own
//   thread sends it, so they add only the mailboxes' mutexes above. The
//   broadcasts to every player of the server walk each group's players
//   under that group's ZonePlayerManager mutex, also above.
// - So a thread may take m_Mutex holding its zone group's mutex -- a CG
//   handler or a quest action answering a player: endWar, isModifyCastleOwner,
//   getSiegeGuildSide, mayModifyShrineOwner, addRaceWarScheduleInfo.
// - Never holding a castle scheduler's mutex (WarScheduler::m_Mutex), which
//   Zone::heartbeat takes under the zone's own: that would close the cycle
//   below. A scheduler takes nothing of the war system's or of any zone under
//   its mutex -- makeGCWarScheduleList asks for the race war's line after
//   releasing it, and a castle war whose time has come is taken out of the
//   queue under it and started after -- only the database and the guild
//   manager's and a guild's mutexes (guild names), and neither of those is
//   held while a scheduler's mutex is taken: a guild's deletion posts its
//   schedule cancel to the castle's group.
// - Nothing may take m_Mutex holding a zone's own mutex, which Zone::heartbeat
//   holds over NPC, monster and effect processing: the heartbeat, holding
//   m_Mutex, waits for that zone's mutex, and the zone thread waits for
//   m_Mutex. That deadlock is why hasCastleActiveWar answers from its own
//   list. Code there asks the lock-free race war hints or the running-war
//   list below.
// - m_MutexActiveWars (the running castle wars' zones and attackers) and
//   m_MutexWarQueue (wars handed over, not yet added) are leaves: nothing is
//   taken under them, so they may be taken anywhere, a zone's heartbeat
//   included. m_MutexWarList guards the cached war list, and a player's send
//   and the guild manager's mutex are taken under it.
//
// No war is handed out past m_Mutex: every question about a running war is
// answered inside the lock, because the heartbeat frees the war under it.
class WarSystem : public Scheduler {
public:
    WarSystem();
    ~WarSystem();

    void init();
    void load();

    bool addWarDelayed(War* pWar);
    bool endWar(PlayerCreature* pPC, ZoneID_t castleZoneID);
    bool removeWar(ZoneID_t castleZoneID);
    bool removeRaceWar();

    bool makeGCWarList();
    bool makeGCWarList_LOCKED();
    void sendGCWarList(Player* pPlayer);
    bool addRaceWarScheduleInfo(WarScheduleInfo* pWSI);

    virtual Work* heartbeat();

    // public :
    //	void	lock() 	{ m_Mutex.lock(); }
    //	void	unlock() 	{ m_Mutex.unlock(); }

public:
    static WarID_t getWarIDSuccessor() {
        return s_WarIDSuccessor;
    }
    static void setWarIDSuccessor(WarID_t wid) {
        s_WarIDSuccessor = wid;
    }

protected:
    VSDateTime getWarEndTime(WarType_t warType) const;
    bool addQueuedWar();
    bool addWar(War* pWar);
    War* getActiveRaceWarAtSameThread() const;
    bool checkStartRaceWar();

    // The running castle war over zoneID, or its schedule. m_Mutex must be
    // held, and the pointer does not outlive it: the heartbeat frees the war
    // under the lock when it ends.
    War* getActiveWar_LOCKED(ZoneID_t zoneID) const;
    WarSchedule* getActiveWarSchedule_LOCKED(ZoneID_t zoneID);

public:
    // Whether a castle war runs over zoneID, and the guild attacking it. These
    // read the running-war list under its own leaf mutex, so they may be asked
    // from anywhere, a zone's heartbeat included.
    bool hasCastleActiveWar(ZoneID_t zoneID) const;
    bool getAttackGuildID(ZoneID_t zoneID, GuildID_t& guildID) const;

    // Questions about the running castle war itself, answered under m_Mutex.
    // Whether pPC takes the castle by ending the war now; no, when no war runs
    // over the castle.
    bool isModifyCastleOwner(ZoneID_t castleZoneID, PlayerCreature* pPC);
    // The side guildID fights on in the siege running over castleZoneID (see
    // SiegeWar::getGuildSide); false, with side untouched, when no siege runs
    // there.
    bool getSiegeGuildSide(ZoneID_t castleZoneID, GuildID_t guildID, int& side) const;

    // Whether the war fought over the shrines of Adam's holy land -- the race
    // war -- lets pPC take a shrine set for its race. With no race war running
    // there is no war to take a shrine in, so the answer is no: unlike
    // isModifyCastleOwner, which a siege reaches only while that siege runs,
    // this is asked every time a blood bible is placed, so a missing war is an
    // answer rather than an error.
    bool mayModifyShrineOwner(PlayerCreature* pPC) const;

    // Whether a race war is running. The zone threads ask this on every path
    // the holy land touches, without a lock, so it is a hint published by the
    // main thread's heartbeat: the race war object itself lives in the
    // schedules and is read only under m_Mutex, which the heartbeat holds
    // when it ends the war and frees it.
    bool hasActiveRaceWar() const {
        return m_bHasRaceWar;
    }

    bool isWarActive() const {
        return !isEmpty();
    }
    bool isRaceWarToday() const {
        return m_bRaceWarToday;
    }
    DWORD getRaceWarTimeParam() const {
        return m_RaceWarTimeParam;
    }

public:
    void broadcastWarList(GamePlayer* pGamePlayer) const;
    bool startRaceWar();
    void prepareRaceWar();

    bool canApplyBloodBibleSign() const {
        return !m_b20Minutes;
    }
    bool isSkyBlack() const {
        return m_b5Minutes;
    }

private:
    static WarID_t s_WarIDSuccessor;

    mutable Mutex m_Mutex;
    mutable Mutex m_MutexWarQueue;
    mutable Mutex m_MutexActiveWars;
    mutable Mutex m_MutexWarList;

    GCWarList m_GCWarList;
    list<War*> m_WarQueue;

    // The race war's state as the zone threads see it. These are written under
    // m_Mutex, by the main thread's heartbeat and by a GM starting the race
    // war early, and read through the lock-free getters above, so each is an
    // atomic: a reader gets a value a writer stored, and nothing more -- two
    // of them read one after the other may straddle a heartbeat.
    std::atomic<bool> m_bHasRaceWar;
    std::atomic<bool> m_bRaceWarToday;
    std::atomic<DWORD> m_RaceWarTimeParam;

    list<ActiveWarInfo> m_ActiveWars;

    Schedule* m_pRaceWarSchedule;

    std::atomic<bool> m_b20Minutes;
    std::atomic<bool> m_b5Minutes;
};

#endif
