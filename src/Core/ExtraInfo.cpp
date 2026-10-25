//////////////////////////////////////////////////////////////////////
//
// Filename    : ExtraInfo.cpp
// Written By  : elca@ewestsoft.com
// Description : Member definitions of the packet class that reports the
//               success of a skill used on oneself.
//
//////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////
// include files
//////////////////////////////////////////////////////////////////////
#include "ExtraInfo.h"

#include <memory>

#include "Assert.h"
#include "SocketInputStream.h"
#include "SocketOutputStream.h"

//////////////////////////////////////////////////////////////////////
// constructor
//////////////////////////////////////////////////////////////////////
ExtraInfo::ExtraInfo() {
    __BEGIN_TRY
    m_ListNum = 0;
    __END_CATCH
}


//////////////////////////////////////////////////////////////////////
// destructor
//////////////////////////////////////////////////////////////////////
ExtraInfo::~ExtraInfo() noexcept {
    // Delete every object it owns.
    while (!m_ExtraSlotInfoList.empty()) {
        ExtraSlotInfo* pExtraSlotInfo = m_ExtraSlotInfoList.front();
        SAFE_DELETE(pExtraSlotInfo);
        m_ExtraSlotInfoList.pop_front();
    }
}


//////////////////////////////////////////////////////////////////////
// Read data from the input stream (buffer) and initialise the packet.
//////////////////////////////////////////////////////////////////////
void ExtraInfo::read(SocketInputStream& iStream) {
    __BEGIN_TRY

    // State the actual size when optimizing.
    iStream.read(m_ListNum);

    for (int i = 0; i < m_ListNum; i++) {
        std::unique_ptr<ExtraSlotInfo> pExtraSlotInfo(new ExtraSlotInfo);
        pExtraSlotInfo->read(iStream);
        m_ExtraSlotInfoList.push_back(pExtraSlotInfo.release());
    }

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////
// Send the packet's binary image to the output stream (buffer).
//////////////////////////////////////////////////////////////////////
void ExtraInfo::write(SocketOutputStream& oStream) const {
    __BEGIN_TRY

    // State the actual size when optimizing.
    oStream.write(m_ListNum);

    for (list<ExtraSlotInfo*>::const_iterator itr = m_ExtraSlotInfoList.begin(); itr != m_ExtraSlotInfoList.end();
         itr++) {
        Assert(*itr != NULL);
        (*itr)->write(oStream);
    }

    __END_CATCH
}

//--------------------------------------------------------------------
// getSize
//--------------------------------------------------------------------
PacketSize_t ExtraInfo::getSize() {
    PacketSize_t PacketSize = szBYTE;

    for (list<ExtraSlotInfo*>::const_iterator itr = m_ExtraSlotInfoList.begin(); itr != m_ExtraSlotInfoList.end();
         itr++) {
        PacketSize += (*itr)->getSize();
    }

    return PacketSize;
}


//////////////////////////////////////////////////////////////////////
//
// get packet's debug string
//
//////////////////////////////////////////////////////////////////////
string ExtraInfo::toString() const {
    __BEGIN_TRY

    StringStream msg;

    msg << "ExtraInfo( ListNum:" << (int)m_ListNum << " ListSet( ";

    for (list<ExtraSlotInfo*>::const_iterator itr = m_ExtraSlotInfoList.begin(); itr != m_ExtraSlotInfoList.end();
         itr++) {
        Assert(*itr != NULL);
        msg << (*itr)->toString() << ",";
    }

    msg << ")";

    return msg.toString();

    __END_CATCH
}
