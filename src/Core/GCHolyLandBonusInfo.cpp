//////////////////////////////////////////////////////////////////////
//
// Filename    : GCHolyLandBonusInfo.cpp
// Written By  :
//
//////////////////////////////////////////////////////////////////////

// include files
#include "GCHolyLandBonusInfo.h"

#include <memory>


//////////////////////////////////////////////////////////////////////
// constructor
//////////////////////////////////////////////////////////////////////
GCHolyLandBonusInfo::GCHolyLandBonusInfo()

{}

//////////////////////////////////////////////////////////////////////
// constructor
//////////////////////////////////////////////////////////////////////
GCHolyLandBonusInfo::~GCHolyLandBonusInfo()

{
    __BEGIN_TRY

    // Delete every object in the guild list
    clearBloodBibleBonusInfoList();

    __END_CATCH_NO_RETHROW
}

//////////////////////////////////////////////////////////////////////
// Read data from the input stream (buffer) and initialise the packet.
//////////////////////////////////////////////////////////////////////
void GCHolyLandBonusInfo::read(SocketInputStream& iStream)

{
    __BEGIN_TRY

    // The listing replaces the one the packet holds.
    clearBloodBibleBonusInfoList();

    BYTE ListNum;

    iStream.read(ListNum);

    if (ListNum > kMaxEntries)
        throw InvalidProtocolException("too many holy land bonuses");

    for (int i = 0; i < ListNum; i++) {
        std::unique_ptr<BloodBibleBonusInfo> pBloodBibleBonusInfo(new BloodBibleBonusInfo);
        pBloodBibleBonusInfo->read(iStream);
        m_BloodBibleBonusInfoList.push_back(pBloodBibleBonusInfo.release());
    }

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////
// Send the packet's binary image to the output stream (buffer).
//////////////////////////////////////////////////////////////////////
void GCHolyLandBonusInfo::write(SocketOutputStream& oStream) const

{
    __BEGIN_TRY

    if (m_BloodBibleBonusInfoList.size() > kMaxEntries)
        throw InvalidProtocolException("too many holy land bonuses");

    BYTE ListNum = m_BloodBibleBonusInfoList.size();
    oStream.write(ListNum);

    BloodBibleBonusInfoListConstItor itr = m_BloodBibleBonusInfoList.begin();
    for (; itr != m_BloodBibleBonusInfoList.end(); itr++) {
        (*itr)->write(oStream);
    }

    __END_CATCH
}


void GCHolyLandBonusInfo::clearBloodBibleBonusInfoList()

{
    __BEGIN_TRY

    // Delete the BloodBibleBonusInfoList
    while (!m_BloodBibleBonusInfoList.empty()) {
        BloodBibleBonusInfo* pBloodBibleBonusInfo = m_BloodBibleBonusInfoList.front();
        m_BloodBibleBonusInfoList.pop_front();
        SAFE_DELETE(pBloodBibleBonusInfo);
    }

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////
// get packet size
//////////////////////////////////////////////////////////////////////
PacketSize_t GCHolyLandBonusInfo::getPacketSize() const

{
    __BEGIN_TRY

    PacketSize_t PacketSize = szBYTE;

    BloodBibleBonusInfoListConstItor itr = m_BloodBibleBonusInfoList.begin();

    for (; itr != m_BloodBibleBonusInfoList.end(); itr++) {
        PacketSize += (*itr)->getSize();
    }

    return PacketSize;

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////
// get packet's debug string
//////////////////////////////////////////////////////////////////////
string GCHolyLandBonusInfo::toString() const

{
    __BEGIN_TRY

    StringStream msg;

    msg << "GCHolyLandBonusInfo(";

    BloodBibleBonusInfoListConstItor itr = m_BloodBibleBonusInfoList.begin();
    for (; itr != m_BloodBibleBonusInfoList.end(); itr++) {
        msg << (*itr)->toString();
    }

    msg << ")";

    return msg.toString();

    __END_CATCH
}
