//////////////////////////////////////////////////////////////////////
//
// Filename    : OustersSkillInfo.cpp
// Written By  :
// Description :
//
//////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////
// include files
//////////////////////////////////////////////////////////////////////
#include "OustersSkillInfo.h"

#include <memory>

#include "Assert.h"
#include "SocketInputStream.h"
#include "SocketOutputStream.h"

//////////////////////////////////////////////////////////////////////
// constructor
//////////////////////////////////////////////////////////////////////
OustersSkillInfo::OustersSkillInfo() {
    __BEGIN_TRY
    m_bLearnNewSkill = false;
    __END_CATCH
}


//////////////////////////////////////////////////////////////////////
// destructor
//////////////////////////////////////////////////////////////////////
OustersSkillInfo::~OustersSkillInfo() {
    __BEGIN_TRY

    clearList();

    __END_CATCH_NO_RETHROW
}


//////////////////////////////////////////////////////////////////////
// The record owns the skills it holds.
//////////////////////////////////////////////////////////////////////
void OustersSkillInfo::clearList() {
    while (!m_SubOustersSkillInfoList.empty()) {
        SubOustersSkillInfo* pSubOustersSkillInfo = m_SubOustersSkillInfoList.front();
        SAFE_DELETE(pSubOustersSkillInfo);
        m_SubOustersSkillInfoList.pop_front();
    }
}


//////////////////////////////////////////////////////////////////////
// Read data from the input stream (buffer) and initialise the packet.
//////////////////////////////////////////////////////////////////////
void OustersSkillInfo::read(SocketInputStream& iStream) {
    __BEGIN_TRY

    // State the actual size when optimizing.
    // The list replaces the one the record holds.
    clearList();

    // A bool holds 0 or 1, so any other byte is refused rather than
    // stored in one.
    BYTE learnNewSkill = 0;
    iStream.read(learnNewSkill);

    if (learnNewSkill > 1)
        throw InvalidProtocolException("learn flag is not a bool");

    m_bLearnNewSkill = learnNewSkill != 0;

    BYTE ListNum = 0;
    iStream.read(ListNum);

    if (ListNum > kMaxSkills)
        throw InvalidProtocolException("too many ousters skills");

    for (int i = 0; i < ListNum; i++) {
        std::unique_ptr<SubOustersSkillInfo> pSubOustersSkillInfo(new SubOustersSkillInfo);
        pSubOustersSkillInfo->read(iStream);
        m_SubOustersSkillInfoList.push_back(pSubOustersSkillInfo.release());
    }

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////
// Send the packet's binary image to the output stream (buffer).
//////////////////////////////////////////////////////////////////////
void OustersSkillInfo::write(SocketOutputStream& oStream) const {
    __BEGIN_TRY

    // State the actual size when optimizing.
    if (m_SubOustersSkillInfoList.size() > kMaxSkills)
        throw InvalidProtocolException("too many ousters skills");

    oStream.write(m_bLearnNewSkill);
    oStream.write((BYTE)m_SubOustersSkillInfoList.size());

    for (list<SubOustersSkillInfo*>::const_iterator itr = m_SubOustersSkillInfoList.begin();
         itr != m_SubOustersSkillInfoList.end(); itr++) {
        Assert(*itr != NULL);
        (*itr)->write(oStream);
    }

    __END_CATCH
}

//--------------------------------------------------------------------
// getSize
//--------------------------------------------------------------------
PacketSize_t OustersSkillInfo::getSize() {
    PacketSize_t PacketSize = szBYTE + szBYTE;

    for (list<SubOustersSkillInfo*>::const_iterator itr = m_SubOustersSkillInfoList.begin();
         itr != m_SubOustersSkillInfoList.end(); itr++) {
        PacketSize += (*itr)->getSize();
    }

    return PacketSize;
}


//////////////////////////////////////////////////////////////////////
//
// get packet's debug string
//
//////////////////////////////////////////////////////////////////////
string OustersSkillInfo::toString() const {
    __BEGIN_TRY

    StringStream msg;

    msg << "OustersSkillInfo( ListNum:" << (int)m_SubOustersSkillInfoList.size() << " ListSet( ";

    for (list<SubOustersSkillInfo*>::const_iterator itr = m_SubOustersSkillInfoList.begin();
         itr != m_SubOustersSkillInfoList.end(); itr++) {
        Assert(*itr != NULL);
        msg << (*itr)->toString() << ",";
    }

    msg << ")";

    return msg.toString();

    __END_CATCH
}
