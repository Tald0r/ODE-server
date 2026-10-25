//////////////////////////////////////////////////////////////////////
//
// Filename    : GuildInfo2.cpp
// Written By  :
// Description :
//
//////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////
// include files
//////////////////////////////////////////////////////////////////////
#include "GuildInfo2.h"

#include <memory>

#include "SocketInputStream.h"
#include "SocketOutputStream.h"
#include "WireString.h"

//////////////////////////////////////////////////////////////////////
// constructor
//////////////////////////////////////////////////////////////////////
GuildInfo2::GuildInfo2(){__BEGIN_TRY __END_CATCH}


//////////////////////////////////////////////////////////////////////
// destructor
//////////////////////////////////////////////////////////////////////
GuildInfo2::~GuildInfo2() {
    __BEGIN_TRY

    clearGuildMemberInfoList();

    __END_CATCH_NO_RETHROW
}


//////////////////////////////////////////////////////////////////////
// clear guild member info list
//////////////////////////////////////////////////////////////////////
void GuildInfo2::clearGuildMemberInfoList() {
    __BEGIN_TRY

    // Delete the GuildInfoList.
    while (!m_GuildMemberInfoList.empty()) {
        GuildMemberInfo2* pGuildMemberInfo = m_GuildMemberInfoList.front();
        m_GuildMemberInfoList.pop_front();
        SAFE_DELETE(pGuildMemberInfo);
    }

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////
// Read data from the input stream (buffer) and initialise the packet.
//////////////////////////////////////////////////////////////////////
void GuildInfo2::read(SocketInputStream& iStream) {
    __BEGIN_TRY


    iStream.read(m_ID);
    de::wire::readString(iStream, m_Name, {1, 30}, "Name");
    iStream.read(m_Type);
    iStream.read(m_Race);
    iStream.read(m_State);
    iStream.read(m_ServerGroupID);
    iStream.read(m_ZoneID);
    de::wire::readString(iStream, m_Master, {1, 20}, "Master");
    de::wire::readString(iStream, m_Date, {0, 11}, "Date");

    de::wire::readString(iStream, m_Intro, {0, de::wire::kMaxByteStringLength}, "Intro");

    WORD szGuildMemberInfo;
    iStream.read(szGuildMemberInfo);
    for (int i = 0; i < szGuildMemberInfo; i++) {
        std::unique_ptr<GuildMemberInfo2> pGuildMemberInfo(new GuildMemberInfo2);
        pGuildMemberInfo->read(iStream);
        m_GuildMemberInfoList.push_back(pGuildMemberInfo.release());
    }


    __END_CATCH
}

//////////////////////////////////////////////////////////////////////
// Send the packet's binary image to the output stream (buffer).
//////////////////////////////////////////////////////////////////////
void GuildInfo2::write(SocketOutputStream& oStream) const {
    __BEGIN_TRY

    // State the actual size when optimizing.
    oStream.write(m_ID);
    de::wire::writeString(oStream, m_Name, {1, 30}, "Name");
    oStream.write(m_Type);
    oStream.write(m_Race);
    oStream.write(m_State);
    oStream.write(m_ServerGroupID);
    oStream.write(m_ZoneID);
    de::wire::writeString(oStream, m_Master, {1, 20}, "Master");
    de::wire::writeString(oStream, m_Date, {0, 11}, "Date");
    de::wire::writeString(oStream, m_Intro, {0, GUILD_INTRO_MAX_LENGTH}, "Intro");


    WORD szGuildMemberInfo = m_GuildMemberInfoList.size();
    oStream.write(szGuildMemberInfo);
    GuildMemberInfoListConstItor2 itr = m_GuildMemberInfoList.begin();
    for (; itr != m_GuildMemberInfoList.end(); itr++) {
        (*itr)->write(oStream);
    }


    __END_CATCH
}

//--------------------------------------------------------------------
// getSize
//--------------------------------------------------------------------
PacketSize_t GuildInfo2::getSize() {
    __BEGIN_TRY

    PacketSize_t PacketSize = szGuildID + de::wire::stringWireSize(m_Name) + szGuildType + szGuildRace + szGuildState +
                              szServerGroupID + szZoneID + de::wire::stringWireSize(m_Master) +
                              de::wire::stringWireSize(m_Date) + de::wire::stringWireSize(m_Intro);

    PacketSize += szWORD;

    GuildMemberInfoListConstItor2 itr = m_GuildMemberInfoList.begin();
    for (; itr != m_GuildMemberInfoList.end(); itr++) {
        PacketSize += (*itr)->getSize();
    }

    return PacketSize;

    __END_CATCH
}

/////////////////////////////////////////////////////////////////////
//
// get packet's debug string
//
//////////////////////////////////////////////////////////////////////
string GuildInfo2::toString() const {
    __BEGIN_TRY

    StringStream msg;

    msg << "GuildInfo2( " << "GuildID:" << m_ID << "GuildName:" << m_Name << "GuildType:" << (int)m_Type
        << "GuildRace:" << (int)m_Race << "GuildState:" << (int)m_State << "ServerGroupID:" << (int)m_ServerGroupID
        << "ZoneID:" << (int)m_ZoneID << "GuildMaster:" << m_Master << "GuildDate:" << m_Date
        << "GuildIntro:" << m_Intro << ")";

    return msg.toString();

    __END_CATCH
}
