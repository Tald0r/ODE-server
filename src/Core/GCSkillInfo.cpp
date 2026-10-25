//-------------------------------------------------------------------------------- //
// Filename    : GCSkillInfo.cpp
// Written By  : elca@ewestsoft.com
// Description :
//
//--------------------------------------------------------------------------------

// include files
#include "GCSkillInfo.h"

#include "Assert1.h"
#include "OustersSkillInfo.h"
#include "SlayerSkillInfo.h"
#include "VampireSkillInfo.h"
#include <memory>

//--------------------------------------------------------------------------------
// constructor
//--------------------------------------------------------------------------------
GCSkillInfo::GCSkillInfo()

{}

//--------------------------------------------------------------------------------
// destructor
//--------------------------------------------------------------------------------
GCSkillInfo::~GCSkillInfo()

{
    clearList();
}

//--------------------------------------------------------------------------------
// The packet owns the records it holds.
//--------------------------------------------------------------------------------
void GCSkillInfo::clearList()

{
    while (!m_pPCSkillInfoList.empty()) {
        PCSkillInfo* pPCSkillInfo = m_pPCSkillInfoList.front();
        SAFE_DELETE(pPCSkillInfo);
        m_pPCSkillInfoList.pop_front();
    }
}

//--------------------------------------------------------------------------------
// Read data from the input stream (buffer) and initialise the packet.
//--------------------------------------------------------------------------------
void GCSkillInfo::read(SocketInputStream& iStream)

{
    __BEGIN_TRY

    // The book replaces the one the packet holds.
    clearList();

    //--------------------------------------------------
    // read pc type/info
    //--------------------------------------------------
    BYTE PCType = 0;
    iStream.read(PCType);

    if (!isKnownPCType(PCType))
        throw InvalidProtocolException("invalid pc type");

    m_PCType = PCType;

    BYTE m_ListNum = 0;
    iStream.read(m_ListNum);

    if (m_ListNum > kMaxRecords)
        throw InvalidProtocolException("too many skill records");

    switch (m_PCType) {
    case PC_SLAYER:
        for (int i = 0; i < m_ListNum; i++) {
            std::unique_ptr<SlayerSkillInfo> pSlayerSkillInfo(new SlayerSkillInfo);
            pSlayerSkillInfo->read(iStream);
            m_pPCSkillInfoList.push_back(pSlayerSkillInfo.release());
        }
        break;

    case PC_VAMPIRE:
        for (int i = 0; i < m_ListNum; i++) {
            std::unique_ptr<VampireSkillInfo> pVampireSkillInfo(new VampireSkillInfo);
            pVampireSkillInfo->read(iStream);
            m_pPCSkillInfoList.push_back(pVampireSkillInfo.release());
        }
        break;

    case PC_OUSTERS:
        for (int i = 0; i < m_ListNum; i++) {
            std::unique_ptr<OustersSkillInfo> pOustersSkillInfo(new OustersSkillInfo);
            pOustersSkillInfo->read(iStream);
            m_pPCSkillInfoList.push_back(pOustersSkillInfo.release());
        }
        break;

    default:
        throw InvalidProtocolException("invalid pc type");
    }

    __END_CATCH
}

//--------------------------------------------------------------------------------
// Send the packet's binary image to the output stream (buffer).
//--------------------------------------------------------------------------------
void GCSkillInfo::write(SocketOutputStream& oStream) const

{
    __BEGIN_TRY

    //--------------------------------------------------
    // write pc type
    //--------------------------------------------------
    if (!isKnownPCType(m_PCType))
        throw InvalidProtocolException("invalid pc type");

    if (m_pPCSkillInfoList.size() > kMaxRecords)
        throw InvalidProtocolException("too many skill records");

    oStream.write(m_PCType);

    BYTE szSkill = m_pPCSkillInfoList.size();
    oStream.write(szSkill);

    for (list<PCSkillInfo*>::const_iterator itr = m_pPCSkillInfoList.begin(); itr != m_pPCSkillInfoList.end(); itr++) {
        (*itr)->write(oStream);
    }

    __END_CATCH
}

//--------------------------------------------------------------------
// getSize
//--------------------------------------------------------------------
PacketSize_t GCSkillInfo::getPacketSize() const

{
    PacketSize_t PacketSize = szBYTE;

    PacketSize += 1; // szSkill

    for (list<PCSkillInfo*>::const_iterator itr = m_pPCSkillInfoList.begin(); itr != m_pPCSkillInfoList.end(); itr++) {
        PacketSize += (*itr)->getSize();
    }

    return PacketSize;
}


//--------------------------------------------------------------------------------
// get packet's debug string
//--------------------------------------------------------------------------------
string GCSkillInfo::toString() const

{
    __BEGIN_TRY

    StringStream msg;

    msg << "GCSkillInfo(";
    for (list<PCSkillInfo*>::const_iterator itr = m_pPCSkillInfoList.begin(); itr != m_pPCSkillInfoList.end(); itr++) {
        msg << (*itr)->toString() << ",";
    }
    msg << ")";

    return msg.toString();

    __END_CATCH
}
