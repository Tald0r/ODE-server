#include "QuestStatusInfo.h"

#include <memory>

#include "Exception.h"

QuestStatusInfo::~QuestStatusInfo() {
    clearMissions();
}

void QuestStatusInfo::clearMissions() {
    list<MissionInfo*>::iterator itr = m_Missions.begin();
    for (; itr != m_Missions.end(); ++itr)
        delete *itr;

    m_Missions.clear();
}

void QuestStatusInfo::read(SocketInputStream& iStream) {
    // The missions replace the ones the record holds.
    clearMissions();

    iStream.read(m_QuestID);
    iStream.read(m_Status);
    BYTE size;
    iStream.read(size);

    if (size > MAX_MISSION_NUM)
        throw InvalidProtocolException("too many quest missions");

    for (int i = 0; i < size; ++i) {
        std::unique_ptr<MissionInfo> temp(new MissionInfo);
        temp->read(iStream);
        m_Missions.push_back(temp.release());
    }
}

void QuestStatusInfo::write(SocketOutputStream& oStream) const {
    if (m_Missions.size() > MAX_MISSION_NUM)
        throw InvalidProtocolException("too many quest missions");

    oStream.write(m_QuestID);
    oStream.write(m_Status);
    BYTE size = m_Missions.size();
    oStream.write(size);

    list<MissionInfo*>::const_iterator itr = m_Missions.begin();
    for (; itr != m_Missions.end(); ++itr) {
        (*itr)->write(oStream);
    }
}

PacketSize_t QuestStatusInfo::getSize() const {
    PacketSize_t ret = szDWORD + szBYTE + szBYTE;
    list<MissionInfo*>::const_iterator itr = m_Missions.begin();
    for (; itr != m_Missions.end(); ++itr) {
        ret += (*itr)->getSize();
    }

    return ret;
}
