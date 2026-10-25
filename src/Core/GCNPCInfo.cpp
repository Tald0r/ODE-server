//-------------------------------------------------------------------------------- //
// Filename    : GCNPCInfo.cpp
// Written By  : reiot@ewestsoft.com
// Description :
//
//--------------------------------------------------------------------------------

// include files
#include "GCNPCInfo.h"

#include <memory>

#include "Assert1.h"
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
GCNPCInfo::GCNPCInfo()

{}

//--------------------------------------------------------------------------------
// destructor
//--------------------------------------------------------------------------------
GCNPCInfo::~GCNPCInfo()

{
    clearNPCInfos();
}

//--------------------------------------------------------------------------------
// A filler hands records the zone owns, so those are only dropped; the ones
// read() allocated go with the packet.
//--------------------------------------------------------------------------------
void GCNPCInfo::clearNPCInfos()

{
    if (m_OwnsNPCInfos) {
        list<NPCInfo*>::iterator itr = m_NPCInfos.begin();
        for (; itr != m_NPCInfos.end(); itr++)
            SAFE_DELETE(*itr);
    }

    m_NPCInfos.clear();
    m_OwnsNPCInfos = false;
}

//--------------------------------------------------------------------------------
// Read data from the input stream (buffer) and initialise the packet.
//--------------------------------------------------------------------------------
void GCNPCInfo::read(SocketInputStream& iStream)

{
    __BEGIN_TRY

    //////////////////////////////////////////////////
    // read npc info
    //////////////////////////////////////////////////
    BYTE NPCInfoCount = 0;
    iStream.read(NPCInfoCount);

    clearNPCInfos();
    m_OwnsNPCInfos = true;

    for (BYTE nc = 0; nc < NPCInfoCount; nc++) {
        std::unique_ptr<NPCInfo> pInfo(new NPCInfo);
        pInfo->read(iStream);
        addNPCInfo(pInfo.release());
    }

    __END_CATCH
}


//--------------------------------------------------------------------------------
// Send the packet's binary image to the output stream (buffer).
//--------------------------------------------------------------------------------
void GCNPCInfo::write(SocketOutputStream& oStream) const

{
    __BEGIN_TRY

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

    __END_CATCH
}


//--------------------------------------------------------------------------------
// get packet's debug string
//--------------------------------------------------------------------------------
string GCNPCInfo::toString() const

{
    __BEGIN_TRY

    StringStream msg;

    msg << "GCNPCInfo(" << "NPCInfos:";

    list<NPCInfo*>::const_iterator itr = m_NPCInfos.begin();
    for (; itr != m_NPCInfos.end(); itr++) {
        NPCInfo* pInfo = *itr;
        msg << pInfo->toString();
    }

    msg << ")";

    return msg.toString();

    __END_CATCH
}
