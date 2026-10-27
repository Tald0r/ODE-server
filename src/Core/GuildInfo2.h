//////////////////////////////////////////////////////////////////////
//
// Filename    : GuildInfo2.h
// Written By  :
// Description :
//
//////////////////////////////////////////////////////////////////////

#ifndef __GUILD_INFO2_H__
#define __GUILD_INFO2_H__

// include files
#include <list>

#include "Exception.h"
#include "GuildMemberInfo2.h"
#include "Packet.h"
#include "Types.h"

typedef list<GuildMemberInfo2*> GuildMemberInfoList2;
typedef list<GuildMemberInfo2*>::const_iterator GuildMemberInfoListConstItor2;

//////////////////////////////////////////////////////////////////////
//
// class GuildInfo2;
//
//////////////////////////////////////////////////////////////////////

class GuildInfo2 {
public:
    // constructor
    GuildInfo2();

    // destructor
    ~GuildInfo2();

public:
    // Read data from the input stream (buffer) and initialise the packet.
    void read(SocketInputStream& iStream);

    // Send the packet's binary image to the output stream (buffer).
    void write(SocketOutputStream& oStream) const;

    // get packet's body size
    // When optimizing, use the precomputed constant.
    PacketSize_t getSize();

    // The list packet SGGuildInfo budgets this many guilds in its factory
    // max; it refuses one more.
    static constexpr uint kMaxCount = 500;
    // The members one guild's record budgets; read() refuses one more.
    static constexpr uint kMaxMemberCount = 220;

    static constexpr uint getMaxSize() {
        return (szGuildID + szBYTE + 30 + szGuildType + szGuildRace + szGuildState + szServerGroupID + szZoneID +
                szBYTE + 20 + szBYTE + 11 + szBYTE + 256) +
               szWORD + GuildMemberInfo2::getMaxSize() * kMaxMemberCount;
    }

    // get packet's debug string
    string toString() const;

    // get/set GuildID
    GuildID_t getID() const {
        return m_ID;
    }
    void setID(GuildID_t GuildID) {
        m_ID = GuildID;
    }

    // get/set Guild Name
    string getName() const {
        return m_Name;
    }
    void setName(const string& GuildName) {
        m_Name = GuildName;
    }

    // get/set GuildType
    GuildType_t getType() const {
        return m_Type;
    }
    void setType(GuildType_t type) {
        m_Type = type;
    }

    // get/set Race
    GuildRace_t getRace() const {
        return m_Race;
    }
    void setRace(GuildRace_t race) {
        m_Race = race;
    }

    // get/set state
    GuildState_t getState() const {
        return m_State;
    }
    void setState(GuildState_t state) {
        m_State = state;
    }

    // get/set server group ID
    ServerGroupID_t getServerGroupID() const {
        return m_ServerGroupID;
    }
    void setServerGroupID(ServerGroupID_t serverGroupID) {
        m_ServerGroupID = serverGroupID;
    }

    // get/set Zone ID
    ZoneID_t getZoneID() const {
        return m_ZoneID;
    }
    void setZoneID(ZoneID_t zoneID) {
        m_ZoneID = zoneID;
    }

    // get/set Guild Master
    string getMaster() const {
        return m_Master;
    }
    void setMaster(const string& master) {
        m_Master = master;
    }

    // get/set Guild Expire Date
    string getDate() const {
        return m_Date;
    }
    void setDate(const string& date) {
        m_Date = date;
    }

    // get/set guild intro
    string getIntro() const {
        return m_Intro;
    }
    // Truncates to the width the length byte and the factory max allow.
    void setIntro(const string& intro) {
        m_Intro = (intro.size() > GUILD_INTRO_MAX_LENGTH) ? intro.substr(0, GUILD_INTRO_MAX_LENGTH) : intro;
    }

    // get guild member info list num
    WORD getGuildMemberInfoListNum() const {
        return m_GuildMemberInfoList.size();
    }

    // add GuildMemberInfo
    void addGuildMemberInfo(GuildMemberInfo2* pGuildMemberInfo) {
        m_GuildMemberInfoList.push_front(pGuildMemberInfo);
    }

    // clear GuildMemberInfoList
    void clearGuildMemberInfoList();

    // pop front element in GuildMemberInfoList
    GuildMemberInfo2* popFrontGuildMemberInfoList() {
        if (m_GuildMemberInfoList.empty())
            return NULL;

        GuildMemberInfo2* pGuildMemberInfo = m_GuildMemberInfoList.front();
        m_GuildMemberInfoList.pop_front();
        return pGuildMemberInfo;
    }

private:
    // Guild ID
    GuildID_t m_ID;

    // Guild name
    string m_Name;

    // guild type
    GuildType_t m_Type;

    // guild race
    GuildRace_t m_Race;

    // guild state
    GuildState_t m_State;

    // guild server group ID
    ServerGroupID_t m_ServerGroupID;

    // guild zone ID
    ZoneID_t m_ZoneID;

    // Guild master
    string m_Master;

    // Guild expire date
    string m_Date;

    // guild intro
    string m_Intro;

    // guild member list
    GuildMemberInfoList2 m_GuildMemberInfoList;
};

#endif
