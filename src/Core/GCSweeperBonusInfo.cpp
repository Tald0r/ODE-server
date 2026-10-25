//////////////////////////////////////////////////////////////////////
//
// Filename    : GCSweeperBonusInfo.cpp
// Written By  :
//
//////////////////////////////////////////////////////////////////////

// include files
#include "GCSweeperBonusInfo.h"

#include <memory>


//////////////////////////////////////////////////////////////////////
// constructor
//////////////////////////////////////////////////////////////////////
GCSweeperBonusInfo::GCSweeperBonusInfo()

{}

//////////////////////////////////////////////////////////////////////
// constructor
//////////////////////////////////////////////////////////////////////
GCSweeperBonusInfo::~GCSweeperBonusInfo()

{
    __BEGIN_TRY

    // Delete every object in the guild list
    clearSweeperBonusInfoList();

    __END_CATCH_NO_RETHROW
}

//////////////////////////////////////////////////////////////////////
// Read data from the input stream (buffer) and initialise the packet.
//////////////////////////////////////////////////////////////////////
void GCSweeperBonusInfo::read(SocketInputStream& iStream)

{
    __BEGIN_TRY

    // The listing replaces the one the packet holds.
    clearSweeperBonusInfoList();

    BYTE ListNum;

    iStream.read(ListNum);

    if (ListNum > kMaxEntries)
        throw InvalidProtocolException("too many sweeper bonuses");

    for (int i = 0; i < ListNum; i++) {
        std::unique_ptr<SweeperBonusInfo> pSweeperBonusInfo(new SweeperBonusInfo);
        pSweeperBonusInfo->read(iStream);
        m_SweeperBonusInfoList.push_back(pSweeperBonusInfo.release());
    }

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////
// Send the packet's binary image to the output stream (buffer).
//////////////////////////////////////////////////////////////////////
void GCSweeperBonusInfo::write(SocketOutputStream& oStream) const

{
    __BEGIN_TRY

    if (m_SweeperBonusInfoList.size() > kMaxEntries)
        throw InvalidProtocolException("too many sweeper bonuses");

    BYTE ListNum = m_SweeperBonusInfoList.size();
    oStream.write(ListNum);

    SweeperBonusInfoListConstItor itr = m_SweeperBonusInfoList.begin();
    for (; itr != m_SweeperBonusInfoList.end(); itr++) {
        (*itr)->write(oStream);
    }

    __END_CATCH
}


void GCSweeperBonusInfo::clearSweeperBonusInfoList()

{
    __BEGIN_TRY

    // Delete the SweeperBonusInfoList
    while (!m_SweeperBonusInfoList.empty()) {
        SweeperBonusInfo* pSweeperBonusInfo = m_SweeperBonusInfoList.front();
        m_SweeperBonusInfoList.pop_front();
        SAFE_DELETE(pSweeperBonusInfo);
    }

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////
// get packet size
//////////////////////////////////////////////////////////////////////
PacketSize_t GCSweeperBonusInfo::getPacketSize() const

{
    __BEGIN_TRY

    PacketSize_t PacketSize = szBYTE;

    SweeperBonusInfoListConstItor itr = m_SweeperBonusInfoList.begin();

    for (; itr != m_SweeperBonusInfoList.end(); itr++) {
        PacketSize += (*itr)->getSize();
    }

    return PacketSize;

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////
// get packet's debug string
//////////////////////////////////////////////////////////////////////
string GCSweeperBonusInfo::toString() const

{
    __BEGIN_TRY

    StringStream msg;

    msg << "GCSweeperBonusInfo(";

    SweeperBonusInfoListConstItor itr = m_SweeperBonusInfoList.begin();
    for (; itr != m_SweeperBonusInfoList.end(); itr++) {
        msg << (*itr)->toString();
    }

    msg << ")";

    return msg.toString();

    __END_CATCH
}
