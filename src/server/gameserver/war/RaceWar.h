///////////////////////////////////////////////////////////////////
// General war information and the routines run when a war starts and ends.
///////////////////////////////////////////////////////////////////

#ifndef __RACE_WAR_H__
#define __RACE_WAR_H__

#include "War.h"

class Mutex;
class PlayerCreature;

class RaceWar : public War {
public:
    RaceWar(WarState warState, WarID_t warID = 0);
    virtual ~RaceWar();

    WarType_t getWarType() const override {
        return WAR_RACE;
    }
    string getWarType2DBString() const override {
        return "RACE";
    }
    string getWarName() const override;

public:
    // The race war is the war the shrines of Adam's holy land are fought over,
    // so it is the one war that lets a player take a shrine set for its race.
    bool mayModifyShrineOwner(PlayerCreature* pPC) override;

    void sendWarEndMessage() const override;

protected:
    void executeStart() override;
    void executeEnd() override;

    void recordRaceWarStart();
    void recordRaceWarEnd();

public:
    void makeWarScheduleInfo(WarScheduleInfo* pWSI) const override;
    void makeWarInfo(WarInfo* pWarInfo) const override;

    virtual string toString() const override;

private:
};

#endif // __WAR_H__
