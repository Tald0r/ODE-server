///////////////////////////////////////////////////////////////////
// General war information and the routines run when a war starts and ends.
///////////////////////////////////////////////////////////////////

#ifndef __WAR_H__
#define __WAR_H__

#include "Exception.h"
#include "Types.h"
#include "VSDateTime.h"
#include "Work.h"

class Mutex;
class PlayerCreature;
struct WarScheduleInfo;
class WarInfo;

class War : public Work {
public:
    enum WarState {
        WAR_STATE_WAIT,    // 0
        WAR_STATE_CURRENT, // 1
        WAR_STATE_END,     // 2
        WAR_STATE_CANCEL,  // 3

        MAX_WAR_STATE // 4
    };

public:
    War(WarState warState, WarID_t warID = 0);
    virtual ~War();

    virtual WarType_t getWarType() const = 0;
    virtual string getWarType2DBString() const = 0;
    virtual string getWarName() const = 0;

    WarID_t getWarID() const {
        return m_WarID;
    }
    void setWarID(WarID_t warID) {
        m_WarID = warID;
    }

    WarState getState() const {
        return m_State;
    }
    const string& getState2DBString() const;
    void setState(WarState warState) {
        m_State = warState;
    }

    const VSDateTime& getWarStartTime() const {
        return m_StartTime;
    }
    void setWarStartTime(VSDateTime dt) {
        m_StartTime = dt;
    }

    // A war fought over a castle names the castle's zone, the guild attacking
    // it and the fee paid to register it; a war fought over nothing answers
    // zero for all three. Both castle wars report WAR_GUILD, so the war system
    // and the war schedule match one by zone through these rather than by
    // casting to a particular castle war class.
    virtual ZoneID_t getCastleZoneID() const {
        return 0;
    }
    virtual GuildID_t getAttackerGuildID() const {
        return 0;
    }
    virtual Gold_t getRegistrationFee() const {
        return 0;
    }
    // The attackers a schedule row lists: how many there are, and the guild in
    // each of the row's five slots, zero past the last. A war with a single
    // attacker counts one and fills the first slot, which is the row
    // WarSchedule::create writes for it, AttackerCount taking its default of
    // one; a siege lists its challengers in the order they joined.
    virtual uint getAttackerCount() const {
        return 1;
    }
    virtual GuildID_t getAttackerGuildIDAt(uint slot) const {
        return slot == 0 ? getAttackerGuildID() : 0;
    }
    // A castle war with a single attacker has that one guild in it; a siege
    // counts its five challengers and the reinforcing guild as well.
    virtual bool isWarParticipant(GuildID_t gID) {
        return gID == getAttackerGuildID();
    }
    // Whether gID fights this war as an attacker: the guild war's one
    // challenger, or any of a siege's five. A guild that only reinforces a
    // siege takes part in it without owning it.
    virtual bool isAttackerGuild(GuildID_t gID) const {
        return gID == getAttackerGuildID();
    }
    // Which castle war class a schedule row belongs to. Both castle wars
    // report WAR_GUILD, and a siege with a single challenger writes exactly
    // the columns a guild war does, so the row carries the kind of its own
    // and a reload rebuilds the class the war was registered as. Only castle
    // wars are written to the schedule table; the answer here is the one a
    // row that names no kind is read as.
    virtual string getCastleWarKind2DBString() const {
        return "SIEGE";
    }

public:
    virtual bool isModifyCastleOwner(PlayerCreature* pPC) {
        return false;
    }

    // Whether this war lets pPC take a shrine of Adam's holy land for its race
    // by placing a blood bible on the matching holy shrine. Only the race war
    // is fought over those shrines; a war fought over a castle has nothing to
    // say about a race, and answers no.
    virtual bool mayModifyShrineOwner(PlayerCreature* pPC) {
        return false;
    }

    virtual void sendWarStartMessage() const;
    virtual void sendWarEndMessage() const;

public:
    static void initWarIDRegistry();

public:
    virtual void execute();
    virtual bool endWar(PlayerCreature* pPC) {
        return false;
    }

protected:
    virtual void executeStart() = 0;
    virtual void executeEnd() = 0;


public:
    virtual void makeWarScheduleInfo(WarScheduleInfo* pWSI) const = 0;
    virtual void makeWarInfo(WarInfo* pWarInfo) const = 0;

    virtual string toString() const = 0;

private:
    WarID_t m_WarID;
    WarState m_State;       // the war's current state.
    VSDateTime m_StartTime; // the war's start time

    static Mutex m_Mutex;
    static WarID_t m_WarIDRegistry;
};

#endif // __WAR_H__
