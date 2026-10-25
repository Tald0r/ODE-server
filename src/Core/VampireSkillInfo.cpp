//////////////////////////////////////////////////////////////////////
//
// Filename    : VampireSkillInfo.cpp
// Written By  : elca@ewestsoft.com
// Description : Member definitions of the packet class that reports the
//               success of a skill used on oneself.
//
//////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////
// include files
//////////////////////////////////////////////////////////////////////
#include "VampireSkillInfo.h"

#include <memory>

#include "Assert.h"
#include "SocketInputStream.h"
#include "SocketOutputStream.h"

//////////////////////////////////////////////////////////////////////
// constructor
//////////////////////////////////////////////////////////////////////
VampireSkillInfo::VampireSkillInfo() {
    __BEGIN_TRY
    m_bLearnNewSkill = false;
    __END_CATCH
}


//////////////////////////////////////////////////////////////////////
// destructor
//////////////////////////////////////////////////////////////////////
VampireSkillInfo::~VampireSkillInfo() noexcept {
    clearList();
}


//////////////////////////////////////////////////////////////////
// The record owns the skills it holds.
//////////////////////////////////////////////////////////////////
void VampireSkillInfo::clearList() {
    while (!m_SubVampireSkillInfoList.empty()) {
        SubVampireSkillInfo* pSubVampireSkillInfo = m_SubVampireSkillInfoList.front();
        SAFE_DELETE(pSubVampireSkillInfo);
        m_SubVampireSkillInfoList.pop_front();
    }
}


//////////////////////////////////////////////////////////////////////
// Read data from the input stream (buffer) and initialise the packet.
//////////////////////////////////////////////////////////////////////
void VampireSkillInfo::read(SocketInputStream& iStream) {
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
        throw InvalidProtocolException("too many vampire skills");

    for (int i = 0; i < ListNum; i++) {
        std::unique_ptr<SubVampireSkillInfo> pSubVampireSkillInfo(new SubVampireSkillInfo);
        pSubVampireSkillInfo->read(iStream);
        m_SubVampireSkillInfoList.push_back(pSubVampireSkillInfo.release());
    }

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////
// Send the packet's binary image to the output stream (buffer).
//////////////////////////////////////////////////////////////////////
void VampireSkillInfo::write(SocketOutputStream& oStream) const {
    __BEGIN_TRY

    // State the actual size when optimizing.
    if (m_SubVampireSkillInfoList.size() > kMaxSkills)
        throw InvalidProtocolException("too many vampire skills");

    oStream.write(m_bLearnNewSkill);
    oStream.write((BYTE)m_SubVampireSkillInfoList.size());

    for (list<SubVampireSkillInfo*>::const_iterator itr = m_SubVampireSkillInfoList.begin();
         itr != m_SubVampireSkillInfoList.end(); itr++) {
        Assert(*itr != NULL);
        (*itr)->write(oStream);
    }

    __END_CATCH
}

//--------------------------------------------------------------------
// getSize
//--------------------------------------------------------------------
PacketSize_t VampireSkillInfo::getSize() {
    PacketSize_t PacketSize = szBYTE + szBYTE;

    for (list<SubVampireSkillInfo*>::const_iterator itr = m_SubVampireSkillInfoList.begin();
         itr != m_SubVampireSkillInfoList.end(); itr++) {
        PacketSize += (*itr)->getSize();
    }

    return PacketSize;
}


//////////////////////////////////////////////////////////////////////
//
// get packet's debug string
//
//////////////////////////////////////////////////////////////////////
string VampireSkillInfo::toString() const {
    __BEGIN_TRY

    StringStream msg;

    msg << "VampireSkillInfo( ListNum:" << (int)m_SubVampireSkillInfoList.size() << " ListSet( ";

    for (list<SubVampireSkillInfo*>::const_iterator itr = m_SubVampireSkillInfoList.begin();
         itr != m_SubVampireSkillInfoList.end(); itr++) {
        Assert(*itr != NULL);
        msg << (*itr)->toString() << ",";
    }

    msg << ")";

    return msg.toString();

    __END_CATCH
}
