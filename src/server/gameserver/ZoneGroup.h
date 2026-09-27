//////////////////////////////////////////////////////////////////////
//
// Filename    : ZoneGroup.h
// Written By  : reiot@ewestsoft.com
// Description :
//
//////////////////////////////////////////////////////////////////////

#ifndef __ZONE_GROUP_H__
#define __ZONE_GROUP_H__

// include files
#include <cstddef>
#include <functional>
#include <memory>

#include <unordered_map>

#include "Exception.h"
#include "GMServerInfo.h"
#include "GameTime.h"
#include "Mailbox.h"
#include "Snapshot.h"
#include "Thread.h"
#include "Types.h"
#include "Zone.h"

// forward declaration
class ZonePlayerManager;

// type redefinition
// typedef unordered_map<ZoneID_t,Zone*> ZONE_HASHMAP;

//////////////////////////////////////////////////////////////////////
//
// class ZoneGroup;
//
//////////////////////////////////////////////////////////////////////
class ZoneGroup {
    // friend declaration

public:
    // constructor
    ZoneGroup(ZoneGroupID_t zoneGroupID);

    // destructor
    ~ZoneGroup();


public:
    // initialize zone group
    void init();

    // load sub zones from database
    void load();

    // save sub zones to database
    void save();

    //
    void processPlayers();
    void heartbeat();

    // Cross-thread mailbox (CLAUDE.md, "Thread ownership"). Work that must
    // touch this group's state but originates on another thread is posted
    // here and run by this group's ZoneGroupThread at the top of its next
    // tick, under the group mutex. One producer is dynamic-zone recycling:
    // the requesting player's zone thread hands an instance's init() to the
    // group that owns the instance, then transports the player, whose
    // arrival (ZonePlayerManager's queue) that same tick sees after the
    // drain. The others are the wars, whose every change to a zone is posted
    // to the zone's group (war/WarZoneWork.h, and the castle owner change in
    // CastleInfoManager). post() never takes the group mutex, so a caller
    // holding its own group's mutex or the PCFinder lock cannot deadlock
    // on it. Work aimed at one player goes through the player's own box
    // instead (PlayerMailbox.h), which follows the player between owners.
    void post(std::function<void()> command) {
        m_Mailbox.post(std::move(command));
    }
    // Runs the posted commands. The caller must hold the group mutex
    // (asserted under DE_OWNERSHIP_CHECKS); a command that throws -- any
    // type, Throwable or not -- is logged to errorLog.txt and the rest still
    // run, so a command must leave the state consistent at every throw
    // point. Returns how many ran; a batch deeper than kMailboxDepthWarning
    // is logged, since the box is unbounded.
    static constexpr std::size_t kMailboxDepthWarning = 1000;
    std::size_t drainMailbox();

public:
    // add zone to zone group
    void addZone(Zone* pZone);

    // delete zone from zone group
    void deleteZone(ZoneID_t zoneID);
    Zone* removeZone(ZoneID_t zoneID);

    // get zone from zone group
    Zone* getZone(ZoneID_t zoneID) const;

    // #ifdef __NO_COMBAT__
    Zone* getCombatZone(ZoneID_t zoneID) const; // Same as getZone, but may return NULL
    // #endif

    //--------------------------------------------------
    // get/set methods
    //--------------------------------------------------
public:
    // get/set zone group id
    ZoneGroupID_t getZoneGroupID() const {
        return m_ZoneGroupID;
    }
    void setZoneGroupID(ZoneGroupID_t zoneGroupID) {
        m_ZoneGroupID = zoneGroupID;
    }

    // get zone player manager
    ZonePlayerManager* getZonePlayerManager() {
        return m_pZonePlayerManager;
    }
    void setZonePlayerManager(ZonePlayerManager* pZonePlayerManager) {
        m_pZonePlayerManager = pZonePlayerManager;
    }

    // get/set game time
    GameTime getGameTime() const {
        return m_GameTime;
    }
    void setGameTime(const GameTime& gameTime) {
        m_GameTime = gameTime;
    }

    void makeZoneUserInfo(GMServerInfo& gmServerInfo);

    // The zones of this group, as an immutable snapshot the caller may keep
    // for an iteration. Zones are only ever added -- at load, and at run
    // time when a dynamic zone is created, from whichever zone thread the
    // requesting player is on -- while every transport on every thread
    // reads the map through getZone(); the snapshot is what makes that
    // concurrent insert safe (see Snapshot.h and the note at m_Zones).
    using ZoneMap = unordered_map<ZoneID_t, Zone*>;
    std::shared_ptr<const ZoneMap> getZones() const {
        return m_Zones.load();
    }

    // get debug string
    string toString() const;

public:
    // Thread-ownership contract (see "Thread ownership" in CLAUDE.md):
    // zone-group state may only be touched while this group's mutex is
    // held -- the group's ZoneGroupThread holds it for its whole tick, and
    // any other thread (e.g. GDRLairManager) must take it explicitly.
    //
    // Under DE_OWNERSHIP_CHECKS (defined only for Debug builds -- this
    // project deliberately never defines NDEBUG, so gating on our own
    // macro is the only way the checks truly vanish from release),
    // lock()/unlock() record the holding thread and assertOwned() ABORTS
    // on a violation. abort(), not a throw: an AssertionError is a
    // Throwable, and the legacy catch(Throwable&) blocks on exactly these
    // paths would swallow it -- turning a detected race into a silently
    // half-applied mutation (e.g. GamePlayer::disconnect's empty catch
    // would skip the character save). The check is armed by the
    // ZoneGroupThread when it starts; before that, single-threaded
    // startup/loading passes vacuously.
    //
    // Invariant that makes false FIRES impossible: m_LockHolder and
    // m_LockHolderValid are written only while the mutex is held (the
    // mutex is non-recursive -- Mutex::lock() throws on self-relock before
    // the holder is written -- so clearing on unlock is sound). A racing
    // reader that does not hold the mutex may see stale values, but that
    // is precisely the caller that must fail.
    void lock() {
        m_Mutex.lock();
#ifdef DE_OWNERSHIP_CHECKS
        m_LockHolder = Thread::self();
        m_LockHolderValid = true;
#endif
    }
    void unlock() {
#ifdef DE_OWNERSHIP_CHECKS
        m_LockHolderValid = false;
#endif
        m_Mutex.unlock();
    }

#ifdef DE_OWNERSHIP_CHECKS
    void armOwnershipAssert() {
        m_OwnershipArmed = true;
    }
    void assertOwned() const;
#else
    // Release: empty inlines so the gateway call sites compile away.
    void armOwnershipAssert() {}
    void assertOwned() const {}
#endif

    void initLoadValue();
    DWORD getLoadValue() const;


    //--------------------------------------------------
    // data members
    //--------------------------------------------------
private:
    // zone group id
    ZoneGroupID_t m_ZoneGroupID;

    // Hash map of zones. Published copy-on-write: readers on any thread load a
    // snapshot without waiting on a writer; addZone()/removeZone()/deleteZone() replace it.
    // Pre-snapshot, a dynamic zone created on one zone thread was inserted
    // straight into the template zone's group -- generally another group --
    // while that group's thread iterated the same unordered_map in its
    // heartbeat (the "cross-group addZone() race" CLAUDE.md listed).
    de::Snapshot<ZoneMap> m_Zones;

    // zone player manager
    ZonePlayerManager* m_pZonePlayerManager;

    // game time
    GameTime m_GameTime;

    // Tick Time
    Timeval m_TickTime;

    [[maybe_unused]] DWORD m_LoadValue;

    mutable Mutex m_Mutex;

    de::CommandMailbox m_Mailbox;

#ifdef DE_OWNERSHIP_CHECKS
    // Debug-only ownership tracking (see lock()/unlock()/assertOwned()).
    // pthread_t is opaque -- compared with pthread_equal(), never ==, and a
    // separate valid flag stands in for the old "zero tid" sentinel POSIX
    // never promised. Members exist only in checking builds; ZoneGroup's
    // layout differs between configs, which is fine for a type never
    // serialized or shared across differently-configured TUs.
    volatile TID m_LockHolder;
    volatile bool m_LockHolderValid;
    volatile bool m_OwnershipArmed;
#endif
};

#endif
