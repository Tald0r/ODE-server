//----------------------------------------------------------------------
//
// Filename    : LCWorldList.cpp
// Written By  : Reiot
// Description :
//
//----------------------------------------------------------------------

// include files
#include "LCWorldList.h"

#include "PCSlayerInfo.h"
#include "PCVampireInfo.h"

//----------------------------------------------------------------------
// constructor
//----------------------------------------------------------------------
LCWorldList::LCWorldList()

{
    m_CurrentWorldID = 1;
}


//----------------------------------------------------------------------
// destructor
//----------------------------------------------------------------------
LCWorldList::~LCWorldList() noexcept

{
    clearList();
}


//----------------------------------------------------------------------
// Read data from the input stream (buffer) and initialise the packet.
//----------------------------------------------------------------------
void LCWorldList::read(SocketInputStream& iStream)

{
    __BEGIN_TRY

    iStream.read(m_CurrentWorldID);

    BYTE ListNum;

    // State the actual size when optimizing.
    iStream.read(ListNum);
    for (int i = 0; i < ListNum; i++) {
        WorldInfo* pWorldInfo = new WorldInfo();
        pWorldInfo->read(iStream);
        m_WorldInfoList.push_back(pWorldInfo);
    }

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////
// Send the packet's binary image to the output stream (buffer).
//////////////////////////////////////////////////////////////////////
void LCWorldList::write(SocketOutputStream& oStream) const

{
    __BEGIN_TRY

    oStream.write(m_CurrentWorldID);

    BYTE ListNum = m_WorldInfoList.size();
    // State the actual size when optimizing.
    oStream.write(ListNum);

    for (list<WorldInfo*>::const_iterator itr = m_WorldInfoList.begin(); itr != m_WorldInfoList.end(); itr++) {
        (*itr)->write(oStream);
    }

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
PacketSize_t LCWorldList::getPacketSize() const

{
    __BEGIN_TRY

    // Includes the size of the list count.
    PacketSize_t PacketSize = szWorldID + szBYTE;

    for (list<WorldInfo*>::const_iterator itr = m_WorldInfoList.begin(); itr != m_WorldInfoList.end(); itr++) {
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
string LCWorldList::toString() const

{
    __BEGIN_TRY

    StringStream msg;

    msg << "LCWorldList(CurrentWorldID : " << (int)m_CurrentWorldID;
    for (list<WorldInfo*>::const_iterator itr = m_WorldInfoList.begin(); itr != m_WorldInfoList.end(); itr++) {
        msg << (*itr)->toString();
    }
    msg << ")";

    return msg.toString();

    __END_CATCH
}
