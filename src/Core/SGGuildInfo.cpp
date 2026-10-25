//////////////////////////////////////////////////////////////////////
//
// Filename    : SGGuildInfo.cpp
// Written By  :
// Description :
//
//////////////////////////////////////////////////////////////////////

// include files
#include "SGGuildInfo.h"

#include <memory>


//////////////////////////////////////////////////////////////////////
// constructor
//////////////////////////////////////////////////////////////////////
SGGuildInfo::SGGuildInfo() {}

//////////////////////////////////////////////////////////////////////
// destructor
//////////////////////////////////////////////////////////////////////
SGGuildInfo::~SGGuildInfo() noexcept {
    // Destroys every guild record the list still owns.
    clearGuildInfoList();
}

//////////////////////////////////////////////////////////////////////
// Read the packet body from the input stream.
//////////////////////////////////////////////////////////////////////
void SGGuildInfo::read(SocketInputStream& iStream) {
    __BEGIN_TRY

    WORD szGuildInfo;

    iStream.read(szGuildInfo);

    if (szGuildInfo > GuildInfo2::kMaxCount)
        throw InvalidProtocolException("too many guild infos");

    for (int i = 0; i < szGuildInfo; i++) {
        std::unique_ptr<GuildInfo2> pGuildInfo(new GuildInfo2);
        pGuildInfo->read(iStream);
        m_GuildInfoList.push_back(pGuildInfo.release());
    }

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////
// Write the packet body to the output stream.
//////////////////////////////////////////////////////////////////////
void SGGuildInfo::write(SocketOutputStream& oStream) const {
    __BEGIN_TRY

    if (m_GuildInfoList.size() > GuildInfo2::kMaxCount)
        throw InvalidProtocolException("too many guild infos");

    WORD szGuildInfo = m_GuildInfoList.size();

    oStream.write(szGuildInfo);
    GuildInfoListConstItor2 itr = m_GuildInfoList.begin();
    for (; itr != m_GuildInfoList.end(); itr++) {
        (*itr)->write(oStream);
    }

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////
// clear guild info list
//////////////////////////////////////////////////////////////////////
void SGGuildInfo::clearGuildInfoList() {
    __BEGIN_TRY

    // Destroy every guild record in the list.
    while (!m_GuildInfoList.empty()) {
        GuildInfo2* pGuildInfo = m_GuildInfoList.front();
        m_GuildInfoList.pop_front();
        SAFE_DELETE(pGuildInfo);
    }

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////
// get packet size
//////////////////////////////////////////////////////////////////////
PacketSize_t SGGuildInfo::getPacketSize() const {
    __BEGIN_TRY

    PacketSize_t PacketSize = szWORD;

    GuildInfoListConstItor2 itr = m_GuildInfoList.begin();
    for (; itr != m_GuildInfoList.end(); itr++) {
        PacketSize += (*itr)->getSize();
    }

    return PacketSize;

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
string SGGuildInfo::toString() const {
    StringStream msg;

    msg << "SGGuildInfo()";

    return msg.toString();
}
