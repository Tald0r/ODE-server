#ifndef __GDR_LAIR_MANAGER_H__
#define __GDR_LAIR_MANAGER_H__

#include "FiniteStateMachine.h"
#include "GDRLairAbstractStates.h"
#include "ManagedThread.h"
#include "Types.h"

class Monster;

enum GDRLairStatus {
    GDR_LAIR_IDLE = 1,
    GDR_LAIR_ENTRANCE,
    GDR_LAIR_ILLUSIONS_WAY_ONLY,
    GDR_LAIR_ICEPOLE,
    GDR_LAIR_SCENE_1,
    GDR_LAIR_SUMMON_MONSTER,
    GDR_LAIR_SCENE_2,
    GDR_LAIR_SUMMON_GDR_DUP,
    GDR_LAIR_SCENE_3,
    GDR_LAIR_GDR_FIGHT,
    GDR_LAIR_SCENE_4,
    GDR_LAIR_AWAKENED_GDR_FIGHT,
    GDR_LAIR_SCENE_5,
    GDR_LAIR_MINION_FIGHT,
    GDR_LAIR_SCENE_6,
    GDR_LAIR_ENDING,
    GDR_LAIR_KILL_ALL,
};

// Idle. The lair rests and opens when its time comes.
class GDRLairIdle : public SetTimeState {
public:
    DWORD getStateType() const {
        return GDR_LAIR_IDLE;
    }
    GDRLairIdle(const VSDateTime& date) : SetTimeState(GDR_LAIR_ENTRANCE, date) {}

    void start();

    State* clone() {
        return new GDRLairIdle(getTimer());
    }
    string toString() const {
        return "GDRLairIdle";
    }

private:
};

// The lair is open for 20 minutes and the Illusions Way traps become active.
class GDRLairEntrance : public TimerState {
public:
    DWORD getStateType() const {
        return GDR_LAIR_ENTRANCE;
    }
    GDRLairEntrance() : TimerState(GDR_LAIR_ICEPOLE, 1210) {}

    void start();
    DWORD heartbeat(Timeval currentTime);

    State* clone() {
        return new GDRLairEntrance();
    }
    string toString() const {
        return "GDRLairEntrance";
    }
};

// After the lair entrance closes there are still 5 minutes to get through
// the Illusions Way. It just burns 5 minutes; as soon as anyone gets
// through, it moves straight to the Icepole state.

// If a player got through the Illusions Way, they are given time to break
// the ice pole. The pole is summoned at the start, and once it breaks an
// ice pole effect that deals damage appears there a minute later.
class GDRLairIcepole : public State {
public:
    void start();
    DWORD getStateType() const {
        return GDR_LAIR_ICEPOLE;
    }
    DWORD heartbeat(Timeval currentTime);

    State* clone() {
        return new GDRLairIcepole();
    }
    string toString() const {
        return "GDRLairIcepole";
    }

private:
    Timeval m_BroadcastTime;
};

// If anyone got past the ice pole, GDR appears.
// GDR is summoned, the effect plays, and GDR speaks line 1.
class GDRLairScene1 : public GDRScene {
public:
    GDRLairScene1() : GDRScene(GDR_LAIR_SUMMON_MONSTER) {}
    void start();
    DWORD getStateType() const {
        return GDR_LAIR_SCENE_1;
    }
    void end();

    State* clone() {
        return new GDRLairScene1();
    }
    string toString() const {
        return "GDRLairScene1";
    }
};

// Summon ordinary monsters into the GDR lair 10 times.
// If nobody is left alive, go to Idle;
// if someone survives all 10 waves, go to Scene2.
class GDRLairSummonMonster : public MonsterSummonState {
public:
    GDRLairSummonMonster();
    DWORD getStateType() const {
        return GDR_LAIR_SUMMON_MONSTER;
    }

    void start();
    void end();

    State* clone() {
        return new GDRLairSummonMonster();
    }
    string toString() const {
        return "GDRLairSummonMonster";
    }
};

// GDR speaks line 2, casts the effect, moves to the set position and
// then speaks line 3.
class GDRLairScene2 : public GDRScene {
public:
    GDRLairScene2() : GDRScene(GDR_LAIR_SUMMON_GDR_DUP) {}
    void start();
    DWORD getStateType() const {
        return GDR_LAIR_SCENE_2;
    }
    void end();

    State* clone() {
        return new GDRLairScene2();
    }
    string toString() const {
        return "GDRLairScene2";
    }
};

// GDR summons its duplicates.
class GDRLairSummonGDRDup : public MonsterSummonState {
public:
    GDRLairSummonGDRDup();
    DWORD getStateType() const {
        return GDR_LAIR_SUMMON_GDR_DUP;
    }

    void start();
    void end();

    State* clone() {
        return new GDRLairSummonGDRDup();
    }
    string toString() const {
        return "GDRLairSummonGDRDup";
    }
};

// GDR speaks line 4 and casts the effect.
class GDRLairScene3 : public GDRScene {
public:
    GDRLairScene3() : GDRScene(GDR_LAIR_GDR_FIGHT) {}
    void start();
    DWORD getStateType() const {
        return GDR_LAIR_SCENE_3;
    }
    void end();

    State* clone() {
        return new GDRLairScene3();
    }
    string toString() const {
        return "GDRLairScene3";
    }
};

// GDR becomes active and fights the players.
class GDRLairGDRFight : public State {
public:
    DWORD getStateType() const {
        return GDR_LAIR_GDR_FIGHT;
    }
    void start();
    DWORD heartbeat(Timeval currentTime);
    void end();

    State* clone() {
        return new GDRLairGDRFight();
    }
    string toString() const {
        return "GDRLairGDRFight";
    }
};

// GDR speaks line 5 and the ending sequence plays.
// Every player is then moved to the GDR core, where the
// awakened GDR speaks line 6.
class GDRLairScene4 : public GDRScene {
public:
    GDRLairScene4() : GDRScene(GDR_LAIR_AWAKENED_GDR_FIGHT) {}
    void start();
    DWORD getStateType() const {
        return GDR_LAIR_SCENE_4;
    }

    State* clone() {
        return new GDRLairScene4();
    }
    string toString() const {
        return "GDRLairScene4";
    }
};

// The awakened GDR becomes active and fights the players.
// Below 50% HP go to Scene5; on death go to Scene6.
class GDRLairAwakenedGDRFight : public State {
    bool m_bGDRDamaged;

public:
    GDRLairAwakenedGDRFight(bool damaged = false) : m_bGDRDamaged(damaged) {}
    DWORD getStateType() const {
        return GDR_LAIR_AWAKENED_GDR_FIGHT;
    }
    void start();
    DWORD heartbeat(Timeval currentTime);

    State* clone() {
        return new GDRLairAwakenedGDRFight(m_bGDRDamaged);
    }
    string toString() const {
        return "GDRLairAwakenedGDRFight";
    }
};

// Speak line 7, move to the set position, then speak line 8.
class GDRLairScene5 : public GDRScene {
public:
    GDRLairScene5() : GDRScene(GDR_LAIR_MINION_FIGHT) {}
    void start();
    DWORD getStateType() const {
        return GDR_LAIR_SCENE_5;
    }

    State* clone() {
        return new GDRLairScene5();
    }
    string toString() const {
        return "GDRLairScene5";
    }
};

// The mid-boss has to be killed within a time limit.
class GDRLairMinionFight : public MonsterSummonState {
public:
    GDRLairMinionFight();
    DWORD getStateType() const {
        return GDR_LAIR_MINION_FIGHT;
    }

    void start();
    void end();

    State* clone() {
        return new GDRLairMinionFight();
    }
    string toString() const {
        return "GDRLairMinionFight";
    }
};

// Speak line 9, move to the set position, then speak line 10.
class GDRLairScene6 : public GDRScene {
public:
    GDRLairScene6() : GDRScene(GDR_LAIR_AWAKENED_GDR_FIGHT) {}
    void start();
    DWORD getStateType() const {
        return GDR_LAIR_SCENE_6;
    }

    State* clone() {
        return new GDRLairScene6();
    }
    string toString() const {
        return "GDRLairScene6";
    }
};

// Speak while dying and hand out the reward items.
// There is not much else to do here.
class GDRLairEnding : public TimerState {
public:
    GDRLairEnding() : TimerState(GDR_LAIR_IDLE, 10) {}
    DWORD getStateType() const {
        return GDR_LAIR_ENDING;
    }

    void start();
    void end();

    State* clone() {
        return new GDRLairEnding();
    }
    string toString() const {
        return "GDRLairEnding";
    }
};

// It is over. Kill everyone.
class GDRLairKillAll : public TimerState {
public:
    GDRLairKillAll() : TimerState(GDR_LAIR_IDLE, 10) {}
    DWORD getStateType() const {
        return GDR_LAIR_KILL_ALL;
    }

    void start();
    //	DWORD	heartbeat(Timeval currentTime) { return GDR_LAIR_IDLE; }
    void end();

    State* clone() {
        return new GDRLairKillAll();
    }
    string toString() const {
        return "GDRLairKillAll";
    }
};

// Manager for the GDR lair. Runs on its own managed thread.
class GDRLairManager : public FiniteStateMachine, public ManagedThread {
public:
    ~GDRLairManager() noexcept override {
        stop();
        join();
    }

    enum GDRLairZones {
        ILLUSIONS_WAY_1,
        ILLUSIONS_WAY_2,
        GDR_LAIR,
        GDR_LAIR_CORE,

        GDR_LAIR_MAX
    };

    void init() override;
    void run() override;
    string getName() const override {
        return "GDRLairManager";
    }

    static GDRLairManager& Instance() {
        static GDRLairManager theInstance;
        return theInstance;
    }
    Zone* getZone(int index) const {
        return m_pZones[index];
    }

    Monster* getGDR() const {
        return m_pGDR;
    }
    void setGDR(Monster* pGDR) {
        m_pGDR = pGDR;
    }

    VSDateTime getNextOpenTime() const;

    BYTE getCorrectPortal() const {
        return m_CorrectPortal;
    }
    void setCorrectPortal(BYTE pid) {
        m_CorrectPortal = pid;
    }

    int getTotalPCs() const;
    bool isGDRLairZone(ZoneID_t ZoneID) const;

    void open() {
        m_bCanEnter = true;
    }
    void close() {
        m_bCanEnter = false;
    }
    bool canEnter() const {
        return m_bCanEnter;
    }

    friend class GDRScene;

private:
    Monster* m_pGDR;
    Zone* m_pZones[GDR_LAIR_MAX];
    BYTE m_CorrectPortal;
    bool m_bCanEnter;
};

#endif
