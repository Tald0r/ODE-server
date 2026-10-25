//-------------------------------------------------------------------------------- //
// Filename    : GCUpdateInfo.cpp
// Written By  : reiot@ewestsoft.com
// Description :
//
//--------------------------------------------------------------------------------

// include files
#include "GCUpdateInfo.h"

#include <memory>

#include "Assert1.h"
#include "PCOustersInfo2.h"
#include "PCSlayerInfo2.h"
#include "PCVampireInfo2.h"

// for client.. by sigi
#ifndef SAFE_DELETE
#define SAFE_DELETE(p) \
    if (p != NULL) {   \
        delete p;      \
        p = NULL;      \
    }
#endif

//--------------------------------------------------------------------------------
// constructor
//--------------------------------------------------------------------------------
GCUpdateInfo::GCUpdateInfo()

    : m_pPCInfo(NULL), m_pInventoryInfo(NULL), m_pGearInfo(NULL), m_pExtraInfo(NULL), m_pEffectInfo(NULL),
      m_hasMotorcycle(false), m_pRideMotorcycleInfo(NULL), m_ZoneID(0), m_ZoneX(0), m_ZoneY(0), m_GameTime(),
      m_Weather(WEATHER_CLEAR), m_WeatherLevel(0), m_DarkLevel(0), m_LightLevel(0), m_nNPCs(0), m_NPCTypes{},
      m_nMonsters(0), m_MonsterTypes{}, m_ServerStat(0), m_fPremium(0), m_SMSCharge(0), m_pNicknameInfo(NULL),
      m_NonPK(0), m_GuildUnionID(0), m_GuildUnionUserType(UNION_NOTHING), m_pBloodBibleSign(NULL), m_PowerPoint(0) {}

//--------------------------------------------------------------------------------
// destructor
//--------------------------------------------------------------------------------
GCUpdateInfo::~GCUpdateInfo()

{
    SAFE_DELETE(m_pPCInfo);
    SAFE_DELETE(m_pInventoryInfo);
    SAFE_DELETE(m_pGearInfo);
    SAFE_DELETE(m_pExtraInfo);
    SAFE_DELETE(m_pEffectInfo);
    SAFE_DELETE(m_pRideMotorcycleInfo);

    // A filler installs the Zone's NPC records and the creature's nickname
    // and sign (PacketUtil.cpp), which the packet only borrows; read()
    // allocates its own, and those go with the packet.
    if (m_bOwnsRecords) {
        for (NPCInfo* pInfo : m_NPCInfos)
            delete pInfo;
        SAFE_DELETE(m_pNicknameInfo);
        SAFE_DELETE(m_pBloodBibleSign);
    }
    m_NPCInfos.clear();
}

//--------------------------------------------------------------------------------
// Read data from the input stream (buffer) and initialise the packet.
//--------------------------------------------------------------------------------
void GCUpdateInfo::read(SocketInputStream& iStream)

{
    __BEGIN_TRY

    // a reader's records are its own; a filler's belong to the creature
    m_bOwnsRecords = true;

    //--------------------------------------------------
    // read pc type/info
    //--------------------------------------------------
    char pcType;
    iStream.read(pcType);

    switch (pcType) {
    case 'S':
        m_pPCInfo = new PCSlayerInfo2();
        break;

    case 'V':
        m_pPCInfo = new PCVampireInfo2();
        break;

    case 'O':
        m_pPCInfo = new PCOustersInfo2();
        break;

    default:
        throw InvalidProtocolException("invalid pc type");
    }

    m_pPCInfo->read(iStream);

    m_pInventoryInfo = new InventoryInfo();
    m_pInventoryInfo->read(iStream);

    m_pGearInfo = new GearInfo();
    m_pGearInfo->read(iStream);

    m_pExtraInfo = new ExtraInfo();
    m_pExtraInfo->read(iStream);

    m_pEffectInfo = new EffectInfo();
    m_pEffectInfo->read(iStream);

    iStream.read(m_hasMotorcycle);

    if (m_hasMotorcycle) {
        m_pRideMotorcycleInfo = new RideMotorcycleInfo();
        m_pRideMotorcycleInfo->read(iStream);
    }

    //--------------------------------------------------
    // read zone info
    //--------------------------------------------------
    iStream.read(m_ZoneID);
    iStream.read(m_ZoneX);
    iStream.read(m_ZoneY);

    m_GameTime.read(iStream);

    BYTE weather;
    iStream.read(weather);
    m_Weather = (Weather)weather;

    iStream.read(m_WeatherLevel);

    iStream.read(m_DarkLevel);
    iStream.read(m_LightLevel);

    //--------------------------------------------------
    // read NPC types
    //--------------------------------------------------
    iStream.read(m_nNPCs);


    if (m_nNPCs > maxNPCPerZone)
        throw InvalidProtocolException("too many NPC types per zone");

    for (uint i = 0; i < m_nNPCs; i++)
        iStream.read(m_NPCTypes[i]);

    //--------------------------------------------------
    // read Monster types
    //--------------------------------------------------
    iStream.read(m_nMonsters);


    if (m_nMonsters > maxMonsterPerZone)
        throw InvalidProtocolException("too many Monster types per zone");

    for (uint j = 0; j < m_nMonsters; j++)
        iStream.read(m_MonsterTypes[j]);

    //////////////////////////////////////////////////
    // read npc info
    //////////////////////////////////////////////////
    BYTE NPCInfoCount = 0;
    iStream.read(NPCInfoCount);
    for (BYTE nc = 0; nc < NPCInfoCount; nc++) {
        std::unique_ptr<NPCInfo> pInfo(new NPCInfo);
        pInfo->read(iStream);
        addNPCInfo(pInfo.release());
    }

    // Server status
    iStream.read(m_ServerStat);

    // Premium
    iStream.read(m_fPremium);

    iStream.read(m_SMSCharge);

    m_pNicknameInfo = new NicknameInfo;
    m_pNicknameInfo->read(iStream);

    iStream.read(m_NonPK);

    // GuildUnion
    iStream.read(m_GuildUnionID);
    iStream.read(m_GuildUnionUserType);

    m_pBloodBibleSign = new BloodBibleSignInfo;
    m_pBloodBibleSign->read(iStream);

    // PowerZzang points
    iStream.read(m_PowerPoint);

    __END_CATCH
}


//--------------------------------------------------------------------------------
// Send the packet's binary image to the output stream (buffer).
//--------------------------------------------------------------------------------
void GCUpdateInfo::write(SocketOutputStream& oStream) const

{
    __BEGIN_TRY

    //--------------------------------------------------
    // write pc type
    //--------------------------------------------------
    Assert(m_pPCInfo != NULL);
    char pcType;
    switch (m_pPCInfo->getPCType()) {
    case PC_SLAYER:
        pcType = 'S';
        break;

    case PC_VAMPIRE:
        pcType = 'V';
        break;

    case PC_OUSTERS:
        pcType = 'O';
        break;

    default:
        throw InvalidProtocolException("invalid pc type");
    }

    oStream.write(pcType);

    m_pPCInfo->write(oStream);

    m_pInventoryInfo->write(oStream);

    m_pGearInfo->write(oStream);

    m_pExtraInfo->write(oStream);

    m_pEffectInfo->write(oStream);

    oStream.write(m_hasMotorcycle);

    if (m_hasMotorcycle)
        m_pRideMotorcycleInfo->write(oStream);

    //--------------------------------------------------
    // write zone info
    //--------------------------------------------------
    oStream.write(m_ZoneID);
    oStream.write(m_ZoneX);
    oStream.write(m_ZoneY);

    m_GameTime.write(oStream);
    oStream.write((BYTE)m_Weather);
    oStream.write(m_WeatherLevel);
    oStream.write(m_DarkLevel);
    oStream.write(m_LightLevel);

    //--------------------------------------------------
    // write NPC types
    //--------------------------------------------------
    oStream.write(m_nNPCs);


    if (m_nNPCs > maxNPCPerZone)
        throw InvalidProtocolException("too many NPC types per zone");

    for (uint i = 0; i < m_nNPCs; i++)
        oStream.write(m_NPCTypes[i]);

    //--------------------------------------------------
    // write Monster types
    //--------------------------------------------------
    oStream.write(m_nMonsters);


    if (m_nMonsters > maxMonsterPerZone)
        throw InvalidProtocolException("too many Monster types per zone");

    for (uint j = 0; j < m_nMonsters; j++)
        oStream.write(m_MonsterTypes[j]);

    //////////////////////////////////////////////////
    // write npc info
    //////////////////////////////////////////////////
    BYTE NPCInfoCount = m_NPCInfos.size();
    oStream.write(NPCInfoCount);

    list<NPCInfo*>::const_iterator itr = m_NPCInfos.begin();
    for (; itr != m_NPCInfos.end(); itr++) {
        NPCInfo* pInfo = *itr;
        pInfo->write(oStream);
    }

    // Server status
    oStream.write(m_ServerStat);

    // Premium
    oStream.write(m_fPremium);

    oStream.write(m_SMSCharge);

    if (m_pNicknameInfo == NULL) {
        NicknameInfo noNick;
        noNick.setNicknameType(NicknameInfo::NICK_NONE);
        noNick.write(oStream);
    } else
        m_pNicknameInfo->write(oStream);

    oStream.write(m_NonPK);


    // GuildUnion
    oStream.write(m_GuildUnionID);
    oStream.write(m_GuildUnionUserType);

    if (m_pBloodBibleSign == NULL) {
        BloodBibleSignInfo noSign;
        noSign.write(oStream);
    } else
        m_pBloodBibleSign->write(oStream);

    // PowerZzang points
    oStream.write(m_PowerPoint);

    __END_CATCH
}


//--------------------------------------------------------------------------------
// get packet's debug string
//--------------------------------------------------------------------------------
string GCUpdateInfo::toString() const

{
    __BEGIN_TRY

    // Weather2String names only the weathers below WEATHER_MAX; any
    // other value prints as its number.
    StringStream msg;

    msg << "GCUpdateInfo(" << "PC:" << m_pPCInfo->toString() << "EffectInfo:" << m_pEffectInfo->toString()
        << ",ZoneID: " << m_ZoneID << ",ZoneX: " << m_ZoneX << ",ZoneY: " << m_ZoneY
        << ",GameTime:" << m_GameTime.toString()
        << ",Weather:" << (m_Weather < WEATHER_MAX ? Weather2String[m_Weather] : std::to_string((int)m_Weather))
        << ",WeatherLevel:" << (int)m_WeatherLevel << ",DarkLevel:" << (int)m_DarkLevel
        << ",LightLevel:" << (int)m_LightLevel;

    msg << ",nNPCs: " << (int)m_nNPCs;
    for (uint i = 0; i < m_nNPCs; i++)
        msg << ",NPC[" << i + 1 << "]:" << (int)m_NPCTypes[i];

    msg << ",nMonsters:" << (int)m_nMonsters;
    for (uint j = 0; j < m_nMonsters; j++)
        msg << ",Monster[" << j + 1 << "]:" << (int)m_MonsterTypes[j];

    msg << ",NPCInfos:";
    list<NPCInfo*>::const_iterator itr = m_NPCInfos.begin();
    for (; itr != m_NPCInfos.end(); itr++) {
        NPCInfo* pInfo = *itr;
        msg << pInfo->toString();
    }
    msg << ", ServerStat : " << (int)m_ServerStat;
    msg << ", Premium : " << (int)m_fPremium;
    msg << ", SMSCharge : " << (int)m_SMSCharge;

    msg << ", GuildUnionID: " << (int)m_GuildUnionID;
    msg << ", GuildUnionUserType: " << (int)m_GuildUnionUserType;

    msg << ", PowerPoint: " << m_PowerPoint;

    msg << ")";

    return msg.toString();

    __END_CATCH
}
