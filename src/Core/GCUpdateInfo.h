//--------------------------------------------------------------------------------
//
// Filename    : GCUpdateInfo.h
// Written By  : Reiot
// Description :
//
//--------------------------------------------------------------------------------

#ifndef __GC_UPDATE_INFO_H__
#define __GC_UPDATE_INFO_H__

// include files
#include "Assert1.h"
#include "BloodBibleSignInfo.h"
#include "EffectInfo.h"
#include "ExtraInfo.h"
#include "GameTime.h"
#include "GearInfo.h"
#include "InventoryInfo.h"
#include "NPCInfo.h"
#include "NicknameInfo.h"
#include "PCSlayerInfo2.h"
#include "PCVampireInfo2.h"
#include "Packet.h"
#include "PacketFactory.h"
#include "RideMotorcycleInfo.h"

#define FLAG_PREMIUM_ZONE 0x10 // Player is in a premium zone.
#define FLAG_PREMIUM_PLAY 0x01 // Player has premium play active?

//--------------------------------------------------------------------------------
//
// class GCUpdateInfo;
//
// After the client loads core info and sends CGConnect, it prepares to enter the
// world by loading all core data. The PC information is bundled and delivered to
// the client via GCUpdateInfo.
//
//--------------------------------------------------------------------------------

class GCUpdateInfo : public Packet {
public:
    enum {
        UNION_MASTER = 0,   // Union master
        UNION_GUILD_MASTER, // Guild master within a union but not the union master
        UNION_NOTHING,      // No union role
        UNION_MAX
    };

    // The NPC record count goes on the wire as a BYTE.
    static constexpr size_t kMaxNPCInfos = 255;

    // constructor
    GCUpdateInfo();

    // destructor
    ~GCUpdateInfo();

    // Initialize packet by reading data from the incoming stream.
    void read(SocketInputStream& iStream);

    // Serialize packet data to the outgoing stream.
    void write(SocketOutputStream& oStream) const;


    // get packet id
    PacketID_t getPacketID() const {
        return PACKET_GC_UPDATE_INFO;
    }

    // get packet's body size
    PacketSize_t getPacketSize() const {
        // [PCType][PCInfo]
        // [GameTime][Weather][WeatherLevel][DarkLevel][LightLevel]
        // [#NPCTypes][NPCType1]...[NPCTypeN]
        // [#MonsterTypes][MonsterType1]...[MonsterTypeM]
        PacketSize_t size = 0;

        WORD MotorcycleSize = 0;
        if (m_hasMotorcycle) {
            MotorcycleSize = m_pRideMotorcycleInfo->getSize();
        }

        size += szBYTE;
        size += m_pPCInfo->getSize();
        size += m_pInventoryInfo->getSize();
        size += m_pGearInfo->getSize();
        size += m_pExtraInfo->getSize();
        size += szBYTE;
        size += MotorcycleSize;
        size += m_pEffectInfo->getSize();
        size += szZoneID;
        size += szCoord;
        size += szCoord;
        size += m_GameTime.getSize();
        size += szWeather;
        size += szWeatherLevel;
        size += szDarkLevel;
        size += szLightLevel;
        size += szBYTE;
        size += szNPCType * m_nNPCs;
        size += szBYTE;
        size += szMonsterType * m_nMonsters;

        size += szBYTE;
        list<NPCInfo*>::const_iterator itr = m_NPCInfos.begin();
        for (; itr != m_NPCInfos.end(); itr++) {
            NPCInfo* pInfo = *itr;
            size += pInfo->getSize();
        }
        // Server status
        size += szBYTE;
        // Premium flag
        size += szBYTE;
        // SMS flag
        size += szDWORD;

        if (m_pNicknameInfo == NULL) {
            NicknameInfo noNick;
            noNick.setNicknameType(NicknameInfo::NICK_NONE);
            size += noNick.getSize();
        } else {
            size += m_pNicknameInfo->getSize();
        }

        size += szBYTE;

        // GuildUnion
        size += sizeof(uint);
        size += szBYTE;

        if (m_pBloodBibleSign == NULL) {
            BloodBibleSignInfo noSign;
            size += noSign.getSize();
        } else {
            size += m_pBloodBibleSign->getSize();
        }

        // Deforum list
        size += szint;

        return size;
    }

    // get packet name
    string getPacketName() const {
        return "GCUpdateInfo";
    }

    // get packet's debug string
    string toString() const;

    //--------------------------------------------------
    // methods
    //--------------------------------------------------
public:
    // get/set PC info
    PCInfo* getPCInfo() const {
        return m_pPCInfo;
    }
    void setPCInfo(PCInfo* pPCInfo) {
        m_pPCInfo = pPCInfo;
    }

    // get/set Inventory Info
    InventoryInfo* getInventoryInfo() const {
        return m_pInventoryInfo;
    }
    void setInventoryInfo(InventoryInfo* pInventoryInfo) {
        m_pInventoryInfo = pInventoryInfo;
    }

    // get/set Gear Info
    GearInfo* getGearInfo() const {
        return m_pGearInfo;
    }
    void setGearInfo(GearInfo* pGearInfo) {
        m_pGearInfo = pGearInfo;
    }

    // get/set ExtraInfo
    ExtraInfo* getExtraInfo() const {
        return m_pExtraInfo;
    }
    void setExtraInfo(ExtraInfo* pExtraInfo) {
        m_pExtraInfo = pExtraInfo;
    }

    // get/set EffectInfo
    EffectInfo* getEffectInfo() const {
        return m_pEffectInfo;
    }
    void setEffectInfo(EffectInfo* pEffectInfo) {
        m_pEffectInfo = pEffectInfo;
    }

    // get/set hasMotorcycle
    bool hasMotorcycle() const {
        return m_hasMotorcycle;
    }

    // get/set RideMotorcycleInfo
    RideMotorcycleInfo* getRideMotorcycleInfo() const {
        return m_pRideMotorcycleInfo;
    }
    void setRideMotorcycleInfo(RideMotorcycleInfo* pRideMotorcycleInfo) {
        m_pRideMotorcycleInfo = pRideMotorcycleInfo;
        m_hasMotorcycle = true;
    }

    // get/set ZoneID
    ZoneID_t getZoneID() const {
        return m_ZoneID;
    }
    void setZoneID(const ZoneID_t& zoneID) {
        m_ZoneID = zoneID;
    }

    // get / set ZoneX
    Coord_t getZoneX() const {
        return m_ZoneX;
    }
    void setZoneX(Coord_t ZoneX) {
        m_ZoneX = ZoneX;
    }

    // get / set ZoneY
    Coord_t getZoneY() const {
        return m_ZoneY;
    }
    void setZoneY(Coord_t ZoneY) {
        m_ZoneY = ZoneY;
    }

    // get/set GameTime
    GameTime getGameTime() const {
        return m_GameTime;
    }
    void setGameTime(const GameTime& gameTime) {
        m_GameTime = gameTime;
    }

    // get/set weather
    Weather getWeather() const {
        return m_Weather;
    }
    void setWeather(Weather weather) {
        m_Weather = weather;
    }

    // get/set weather level
    WeatherLevel_t getWeatherLevel() const {
        return m_WeatherLevel;
    }
    void setWeatherLevel(WeatherLevel_t weatherLevel) {
        m_WeatherLevel = weatherLevel;
    }

    // get/set darklevel
    DarkLevel_t getDarkLevel() const {
        return m_DarkLevel;
    }
    void setDarkLevel(DarkLevel_t darkLevel) {
        m_DarkLevel = darkLevel;
    }

    // get/set lightlevel
    LightLevel_t getLightLevel() const {
        return m_LightLevel;
    }
    void setLightLevel(LightLevel_t lightLevel) {
        m_LightLevel = lightLevel;
    }

    // get/set # of NPC
    uint getNPCCount() const {
        return m_nNPCs;
    }
    void setNPCCount(uint n) {
        Assert(n <= maxNPCPerZone);
        m_nNPCs = n;
    }

    // get/set NPC type
    NPCType_t getNPCType(uint n) const {
        Assert(n < maxNPCPerZone);
        return m_NPCTypes[n];
    }
    void setNPCType(uint n, NPCType_t npcType) {
        Assert(n < maxNPCPerZone);
        m_NPCTypes[n] = npcType;
    }

    // get/set # of monster
    uint getMonsterCount() const {
        return m_nMonsters;
    }
    void setMonsterCount(uint n) {
        Assert(n <= maxMonsterPerZone);
        m_nMonsters = n;
    }

    // get/set Monster type
    MonsterType_t getMonsterType(uint n) const {
        Assert(n < maxMonsterPerZone);
        return m_MonsterTypes[n];
    }
    void setMonsterType(uint n, MonsterType_t npcType) {
        Assert(n < maxMonsterPerZone);
        m_MonsterTypes[n] = npcType;
    }

    // get/set npc info
    void addNPCInfo(NPCInfo* pInfo) {
        // A record past the count byte would be written and never counted.
        // The caller owns the record either way.
        if (m_NPCInfos.size() >= kMaxNPCInfos)
            return;
        m_NPCInfos.push_back(pInfo);
    }
    NPCInfo* popNPCInfo(void) {
        if (m_NPCInfos.empty())
            return NULL;
        NPCInfo* pInfo = m_NPCInfos.front();
        m_NPCInfos.pop_front();
        return pInfo;
    }

    // get/set ServerStat
    void setServerStat(BYTE ServerStat) {
        m_ServerStat = ServerStat;
    }
    BYTE getServerStat() const {
        return m_ServerStat;
    }

    // premium play related
    void setPremiumZone() {
        m_fPremium |= FLAG_PREMIUM_ZONE;
    }
    void setPremiumPlay() {
        m_fPremium |= FLAG_PREMIUM_PLAY;
    }
    BYTE isPremiumZone() const {
        return m_fPremium & FLAG_PREMIUM_ZONE;
    }
    BYTE isPremiumPlay() const {
        return m_fPremium & FLAG_PREMIUM_PLAY;
    }

    DWORD getSMSCharge() const {
        return m_SMSCharge;
    }
    void setSMSCharge(DWORD charge) {
        m_SMSCharge = charge;
    }

    NicknameInfo* getNicknameInfo() const {
        return m_pNicknameInfo;
    }
    void setNicknameInfo(NicknameInfo* pNicknameInfo) {
        m_pNicknameInfo = pNicknameInfo;
    }

    BYTE isNonPK() const {
        return m_NonPK;
    }
    void setNonPK(BYTE pk) {
        m_NonPK = pk;
    }


    BYTE getGuildUnionUserType() const {
        return m_GuildUnionUserType;
    }
    void setGuildUnionUserType(BYTE type) {
        m_GuildUnionUserType = type;
    }

    uint getGuildUnionID() const {
        return m_GuildUnionID;
    }
    void setGuildUnionID(uint ID) {
        m_GuildUnionID = ID;
    }

    BloodBibleSignInfo* getBloodBibleSignInfo() {
        return m_pBloodBibleSign;
    }
    void setBloodBibleSignInfo(BloodBibleSignInfo* pInfo) {
        m_pBloodBibleSign = pInfo;
    }

    // get / set PowerPoint
    int getPowerPoint() const {
        return m_PowerPoint;
    }
    void setPowerPoint(int powerpoint) {
        m_PowerPoint = powerpoint;
    }

    //--------------------------------------------------
    // data members
    //--------------------------------------------------
private:
    //--------------------------------------------------------------------------------
    // PC Information
    //--------------------------------------------------------------------------------
    // PCSlayerInfo2 or PCVampireInfo2 is used.
    PCInfo* m_pPCInfo;

    //--------------------------------------------------------------------------------
    // Inventory Information
    //--------------------------------------------------------------------------------
    InventoryInfo* m_pInventoryInfo;

    //--------------------------------------------------------------------------------
    // Gear Information
    //--------------------------------------------------------------------------------
    GearInfo* m_pGearInfo;

    //--------------------------------------------------------------------------------
    // Extra Information
    //--------------------------------------------------------------------------------
    ExtraInfo* m_pExtraInfo;

    //--------------------------------------------------------------------------------
    // Extra Information
    //--------------------------------------------------------------------------------
    EffectInfo* m_pEffectInfo;

    //--------------------------------------------------------------------------------
    // Whether there is a motorcycle or not.
    //--------------------------------------------------------------------------------
    bool m_hasMotorcycle;

    //--------------------------------------------------------------------------------
    // Motorcycle Information
    //--------------------------------------------------------------------------------
    RideMotorcycleInfo* m_pRideMotorcycleInfo;

    // inventory
    // quick item slot
    // gear

    // Journal (PDA)
    // Quest progress information
    // Notices, event information
    // Hmm.. maybe these should be downloaded the first time the PDS is opened.. - -;

    //--------------------------------------------------------------------------------
    // Zone Information
    //--------------------------------------------------------------------------------
    // Zone id
    ZoneID_t m_ZoneID;

    // Rough position in zone coordinates
    Coord_t m_ZoneX;
    Coord_t m_ZoneY;

    // Game Time
    GameTime m_GameTime;

    // Weather(weather level)
    Weather m_Weather;
    WeatherLevel_t m_WeatherLevel;

    // Dark/Light
    DarkLevel_t m_DarkLevel;
    LightLevel_t m_LightLevel;

    // Number of NPC sprite types present in the zone, and the sprite type array
    BYTE m_nNPCs;
    NPCType_t m_NPCTypes[maxNPCPerZone];

    // Number of monster sprite types present in the zone, and the sprite type array
    BYTE m_nMonsters;
    MonsterType_t m_MonsterTypes[maxMonsterPerZone];

    // Information about the NPCs present in the current zone
    list<NPCInfo*> m_NPCInfos;

    // Server status
    BYTE m_ServerStat;

    // Premium related
    BYTE m_fPremium;

    DWORD m_SMSCharge;
    NicknameInfo* m_pNicknameInfo;

    BYTE m_NonPK;

    // Guild union related
    uint m_GuildUnionID;
    BYTE m_GuildUnionUserType;

    BloodBibleSignInfo* m_pBloodBibleSign;

    // true once read() allocated the NPC, nickname and sign records
    // Set by read() alone: a setter installs a borrowed record and is for
    // a packet being filled, never one that was read.
    bool m_bOwnsRecords = false;

    // Frees the NPC, nickname and sign records when this packet owns them,
    // then forgets them either way.
    void releaseRecords();

    // PowerZzang points
    int m_PowerPoint;
};


//--------------------------------------------------------------------------------
//
// class GCUpdateInfoFactory;
//
// Factory for GCUpdateInfo
//
//--------------------------------------------------------------------------------

class GCUpdateInfoFactory : public PacketFactory {
public:
    static constexpr PacketID_t kPacketID = Packet::PACKET_GC_UPDATE_INFO;
    static constexpr std::string_view kName = "GCUpdateInfo";
    static constexpr PacketSize_t kMaxSize{[] {
        PacketSize_t size = 0;

        size += szBYTE;
        size += PCSlayerInfo2::getMaxSize();
        size += InventoryInfo::getMaxSize();
        size += GearInfo::getMaxSize();
        size += ExtraInfo::getMaxSize();
        size += EffectInfo::getMaxSize();
        size += szBYTE;
        size += RideMotorcycleInfo::getMaxSize();
        size += szZoneID;
        size += szCoord;
        size += szCoord;
        size += GameTime::getMaxSize();
        size += szWeather;
        size += szWeatherLevel;
        size += szDarkLevel;
        size += szLightLevel;
        size += szBYTE;
        size += szNPCType * maxNPCPerZone;
        size += szBYTE;
        size += szMonsterType * maxMonsterPerZone;

        size += szBYTE;
        size += NPCInfo::getMaxSize() * GCUpdateInfo::kMaxNPCInfos;
        // Server status
        size += szBYTE;
        // Premium
        size += szBYTE;
        size += szDWORD;
        size += NicknameInfo::getMaxSize();

        size += szBYTE;

        // GuildUnion
        size += sizeof(uint);
        size += szBYTE;

        size += BloodBibleSignInfo::getMaxSize();

        // PowerZzang points
        size += szint;

        return size;
    }()};

    // create packet
    Packet* createPacket() override {
        return new GCUpdateInfo();
    }

    // get packet name
    string getPacketName() const override {
        return string(kName);
    }

    // get packet id
    PacketID_t getPacketID() const override {
        return kPacketID;
    }

    // get packet's max body size
    // *OPTIMIZATION HINT*
    // Define and return const static GCUpdateInfoPacketMaxSize.
    PacketSize_t getPacketMaxSize() const override {
        return kMaxSize;
    }
};


//--------------------------------------------------------------------------------
//
//
//--------------------------------------------------------------------------------

#endif
