#include "SystemAvailabilitiesManager.h"

#include "Assert.h"
#include "repository/SystemAvailabilityRepository.h"

void SystemAvailabilitiesManager::load() {
    for (int i = 0; i < SYSTEM_MAX; ++i)
        setAvailable((SystemKind)i, true);

    if (m_pAvailabilitiesPacket == NULL)
        m_pAvailabilitiesPacket = new GCSystemAvailabilities();
    m_pAvailabilitiesPacket->setFlag((DWORD)m_SystemFlags.to_ulong());
    m_pAvailabilitiesPacket->setOpenDegree(m_ZoneOpenDegree);
    m_pAvailabilitiesPacket->setSkillLimit(m_SkillLevelLimit);
    m_bEdited = false;
}
