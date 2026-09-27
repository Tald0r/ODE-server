///////////////////////////////////////////////////////////////////
// General war information and the routines run when a war starts and ends.
///////////////////////////////////////////////////////////////////

#ifndef __SIEGE_WAR_H__
#define __SIEGE_WAR_H__

#include "SiegeRegistrationDecision.h"
#include "War.h"

class Mutex;
class PlayerCreature;

class SiegeWar : public War {
public:
    SiegeWar(ZoneID_t castleZoneID, WarState warState, WarID_t warID = 0);
    virtual ~SiegeWar();

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
        return "SIEGE";
    }
    string getWarName() const override;

    int getGuildSide(GuildID_t guildID) const;

    Gold_t getRegistrationFee() const override {
        return m_RegistrationFee;
    }
    void setRegistrationFee(Gold_t fee) {
        m_RegistrationFee = fee;
    }
    void addRegistrationFee(Gold_t fee) {
        m_RegistrationFee += fee;
    }

    uint getChallengerGuildCount() const {
        return m_ChallangerGuildCount;
    }
    bool addChallengerGuild(GuildID_t gID);

    GuildID_t getChallangerGuildID(uint index = 0) const {
        return m_ChallangerGuildID[index];
    }
    GuildID_t getAttackerGuildID() const override {
        return m_ChallangerGuildID[0];
    }
    uint getAttackerCount() const override {
        return m_ChallangerGuildCount;
    }
    GuildID_t getAttackerGuildIDAt(uint slot) const override {
        return slot < MaxSiegeChallengerGuilds ? m_ChallangerGuildID[slot] : 0;
    }
    bool isAttackerGuild(GuildID_t gID) const override {
        for (unsigned int i = 0; i < MaxSiegeChallengerGuilds; ++i)
            if (gID == m_ChallangerGuildID[i])
                return true;
        return false;
    }
    bool isWarParticipant(GuildID_t gID) override {
        return isAttackerGuild(gID) || gID == m_ReinforceGuildID;
    }

    GuildID_t getReinforceGuildID() const {
        return m_ReinforceGuildID;
    }

public:
    bool isModifyCastleOwner(PlayerCreature* pPC) override;
    GuildID_t getWinnerGuildID(PlayerCreature* pPC);

    void sendWarEndMessage() const override;

    bool endWar(PlayerCreature* pPC) override;

protected:
    void executeStart() override;
    void executeEnd() override;

public:
    void makeWarScheduleInfo(WarScheduleInfo* pWSI) const override;
    void makeWarInfo(WarInfo* pWarInfo) const override;
    virtual string toString() const override;

public:
    BYTE canReinforce(GuildID_t gID);
    GuildID_t recentReinforceGuild();
    GuildID_t recentReinforceCandidate() const {
        return m_RecentReinforceCandidate;
    }
    BYTE registerReinforce(GuildID_t gID);
    bool acceptReinforce();
    bool denyReinforce();
    void clearReinforceRegisters();
    void setReinforceGuildID(GuildID_t gID) {
        m_ReinforceGuildID = gID;
    }

private:
    ZoneID_t m_CastleZoneID;                                 // the ZoneID of the castle the war concerns
    uint m_ChallangerGuildCount;                             // the number of guilds that applied for the war
    GuildID_t m_ChallangerGuildID[MaxSiegeChallengerGuilds]; // the IDs of the guilds that applied for the war
    GuildID_t m_ReinforceGuildID;                            // the ID of the defending side's reinforcing guild
    Gold_t m_RegistrationFee;                                // the war application fee that was paid

    GuildID_t m_RecentReinforceCandidate;

    // war result
    Race_t m_WinnerRace = 0;
    GuildID_t m_WinnerGuildID = 0;
    bool m_bModifyCastleOwner;
};

#endif // __WAR_H__
