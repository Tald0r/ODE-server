//////////////////////////////////////////////////////////////////////
//
// Filename    : SlayerSkillInfo.cpp
// Written By  : elca@ewestsoft.com
// Description : Member definitions of the packet class that reports the
//               success of a skill used on oneself.
//
//////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////
// include files
//////////////////////////////////////////////////////////////////////
#include "SlayerSkillInfo.h"

#include <memory>

#include "Assert.h"
#include "SocketInputStream.h"
#include "SocketOutputStream.h"

//////////////////////////////////////////////////////////////////////
// constructor
//////////////////////////////////////////////////////////////////////
SlayerSkillInfo::SlayerSkillInfo() {
    __BEGIN_TRY
    m_bLearnNewSkill = false;
    m_DomainType = 0;
    __END_CATCH
}


//////////////////////////////////////////////////////////////////////
// destructor
//////////////////////////////////////////////////////////////////////
SlayerSkillInfo::~SlayerSkillInfo() noexcept {
    clearList();
}


//////////////////////////////////////////////////////////////////////
// The record owns the skills it holds.
//////////////////////////////////////////////////////////////////////
void SlayerSkillInfo::clearList() {
    while (!m_SubSlayerSkillInfoList.empty()) {
        SubSlayerSkillInfo* pSubSlayerSkillInfo = m_SubSlayerSkillInfoList.front();
        SAFE_DELETE(pSubSlayerSkillInfo);
        m_SubSlayerSkillInfoList.pop_front();
    }
}


//////////////////////////////////////////////////////////////////////
// Read data from the input stream (buffer) and initialise the packet.
//////////////////////////////////////////////////////////////////////
void SlayerSkillInfo::read(SocketInputStream& iStream) {
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

    iStream.read(m_DomainType);

    BYTE ListNum = 0;
    iStream.read(ListNum);

    if (ListNum > kMaxSkills)
        throw InvalidProtocolException("too many slayer skills");

    for (int i = 0; i < ListNum; i++) {
        std::unique_ptr<SubSlayerSkillInfo> pSubSlayerSkillInfo(new SubSlayerSkillInfo);
        pSubSlayerSkillInfo->read(iStream);
        m_SubSlayerSkillInfoList.push_back(pSubSlayerSkillInfo.release());
    }

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////
// Send the packet's binary image to the output stream (buffer).
//////////////////////////////////////////////////////////////////////
void SlayerSkillInfo::write(SocketOutputStream& oStream) const {
    __BEGIN_TRY

    // State the actual size when optimizing.
    if (m_SubSlayerSkillInfoList.size() > kMaxSkills)
        throw InvalidProtocolException("too many slayer skills");

    oStream.write(m_bLearnNewSkill);
    oStream.write(m_DomainType);
    oStream.write((BYTE)m_SubSlayerSkillInfoList.size());

    for (list<SubSlayerSkillInfo*>::const_iterator itr = m_SubSlayerSkillInfoList.begin();
         itr != m_SubSlayerSkillInfoList.end(); itr++) {
        Assert(*itr != NULL);
        (*itr)->write(oStream);
    }

    __END_CATCH
}

//--------------------------------------------------------------------
// getSize
//--------------------------------------------------------------------
PacketSize_t SlayerSkillInfo::getSize() {
    PacketSize_t PacketSize = szBYTE + szSkillDomainType + szBYTE;

    for (list<SubSlayerSkillInfo*>::const_iterator itr = m_SubSlayerSkillInfoList.begin();
         itr != m_SubSlayerSkillInfoList.end(); itr++) {
        PacketSize += (*itr)->getSize();
    }

    return PacketSize;
}


//////////////////////////////////////////////////////////////////////
//
// get packet's debug string
//
//////////////////////////////////////////////////////////////////////
string SlayerSkillInfo::toString() const {
    __BEGIN_TRY

    StringStream msg;

    msg << "SlayerSkillInfo( ListNum:" << (int)m_SubSlayerSkillInfoList.size() << " ListSet( ";

    for (list<SubSlayerSkillInfo*>::const_iterator itr = m_SubSlayerSkillInfoList.begin();
         itr != m_SubSlayerSkillInfoList.end(); itr++) {
        Assert(*itr != NULL);
        msg << (*itr)->toString() << ",";
    }

    msg << ")";

    return msg.toString();

    __END_CATCH
}
