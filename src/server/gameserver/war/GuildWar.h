///////////////////////////////////////////////////////////////////
// General war information and the routines run when a war starts and ends.
///////////////////////////////////////////////////////////////////

#ifndef __GUILD_WAR_H__
#define __GUILD_WAR_H__

#include "War.h"

class Mutex;
class PlayerCreature;

class GuildWar : public War {
public:
    GuildWar(ZoneID_t castleZoneID, GuildID_t challenger, WarState warState, WarID_t warID = 0);
    virtual ~GuildWar();

    ZoneID_t getCastleZoneID() const override {
        return m_CastleZoneID;
    }
    void setCastleZoneID(ZoneID_t zoneID) {
        m_CastleZoneID = zoneID;
    }

    WarType_t getWarType() const override {
        return WAR_GUILD;
    }
    string getWarType2DBString() const override {
        return "GUILD";
    }
    string getCastleWarKind2DBString() const override {
        return "GUILD";
    }
    string getWarName() const override;

    GuildID_t getChallangerGuildID() const {
        return m_ChallangerGuildID;
    }
    GuildID_t getAttackerGuildID() const override {
        return m_ChallangerGuildID;
    }

    Gold_t getRegistrationFee() const override {
        return m_RegistrationFee;
    }
    void setRegistrationFee(Gold_t fee) {
        m_RegistrationFee = fee;
    }

public:
    bool isModifyCastleOwner(PlayerCreature* pPC) override;
    GuildID_t getWinnerGuildID(PlayerCreature* pPC);

    void sendWarEndMessage() const override;

    bool endWar(PlayerCreature* pPC) override;

protected:
    void executeStart() override;
    void executeEnd() override;

    void recordGuildWarStart();
    void recordGuildWarEnd();

public:
    void makeWarScheduleInfo(WarScheduleInfo* pWSI) const override;
    void makeWarInfo(WarInfo* pWarInfo) const override;
    virtual string toString() const override;

private:
    ZoneID_t m_CastleZoneID;       // the ZoneID of the castle the war concerns
    GuildID_t m_ChallangerGuildID; // the ID of the guild that applied for the war, for WAR_GUILD
    Gold_t m_RegistrationFee;      // the war application fee that was paid

    // war result
    Race_t m_WinnerRace = 0;
    GuildID_t m_WinnerGuildID = 0;
    bool m_bModifyCastleOwner;
};

#endif // __WAR_H__
