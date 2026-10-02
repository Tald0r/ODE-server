//----------------------------------------------------------------------
//
// Filename    : LCServerList.cpp
// Written By  : Reiot
// Description :
//
//----------------------------------------------------------------------

// include files
#include "LCServerList.h"

#include "PCSlayerInfo.h"
#include "PCVampireInfo.h"

//----------------------------------------------------------------------
// constructor
//----------------------------------------------------------------------
LCServerList::LCServerList()

{
    m_CurrentServerGroupID = 0;
}


//----------------------------------------------------------------------
// destructor
//----------------------------------------------------------------------
LCServerList::~LCServerList() noexcept

{
    clearList();
}


//----------------------------------------------------------------------
// Read data from the input stream (buffer) and initialise the packet.
//----------------------------------------------------------------------
void LCServerList::read(SocketInputStream& iStream)

{
    __BEGIN_TRY

    iStream.read(m_CurrentServerGroupID);

    BYTE ListNum;

    // State the actual size when optimizing.
    iStream.read(ListNum);
    for (int i = 0; i < ListNum; i++) {
        ServerGroupInfo* pServerGroupInfo = new ServerGroupInfo();
        pServerGroupInfo->read(iStream);
        m_ServerGroupInfoList.push_back(pServerGroupInfo);
    }

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////
// Send the packet's binary image to the output stream (buffer).
//////////////////////////////////////////////////////////////////////
void LCServerList::write(SocketOutputStream& oStream) const

{
    __BEGIN_TRY

    oStream.write(m_CurrentServerGroupID);

    BYTE ListNum = m_ServerGroupInfoList.size();
    // State the actual size when optimizing.
    oStream.write(ListNum);

    for (list<ServerGroupInfo*>::const_iterator itr = m_ServerGroupInfoList.begin(); itr != m_ServerGroupInfoList.end();
         itr++) {
        (*itr)->write(oStream);
    }

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
PacketSize_t LCServerList::getPacketSize() const

{
    __BEGIN_TRY

    // Includes the size of the list count.
    PacketSize_t PacketSize = szServerGroupID + szBYTE;

    for (list<ServerGroupInfo*>::const_iterator itr = m_ServerGroupInfoList.begin(); itr != m_ServerGroupInfoList.end();
         itr++) {
        PacketSize += (*itr)->getSize();
    }

    return PacketSize;

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////
//
// get packet's debug string
//
//////////////////////////////////////////////////////////////////////
string LCServerList::toString() const

{
    __BEGIN_TRY

    StringStream msg;

    msg << "LCServerList(CurrentServerGroupID : " << (int)m_CurrentServerGroupID;
    for (list<ServerGroupInfo*>::const_iterator itr = m_ServerGroupInfoList.begin(); itr != m_ServerGroupInfoList.end();
         itr++) {
        msg << (*itr)->toString();
    }
    msg << ")";

    return msg.toString();

    __END_CATCH
}
