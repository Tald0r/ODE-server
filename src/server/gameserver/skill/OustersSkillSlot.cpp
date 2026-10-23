//////////////////////////////////////////////////////////////////////////////
// Filename    : OustersSkillSlot.cpp
// Written By  : elca
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "OustersSkillSlot.h"

#include "repository/SkillSaveRepository.h"

void OustersSkillSlot::create(const string& OwnerID)

{
    OustersSkillRecord record;
    record.skillType = m_SkillType;
    record.skillLevel = m_ExpLevel;
    record.delay = m_Interval;
    record.castingTime = m_CastingTime;
    record.nextTime = m_runTime.tv_sec;
    defaultSkillSaveRepository().insertOustersSkill(OwnerID, record);
}

void OustersSkillSlot::save(const string& OwnerID)

{
    defaultSkillSaveRepository().updateOustersSkill(OwnerID, m_SkillType, m_ExpLevel, m_Interval);
}

void OustersSkillSlot::destroy(const string& OwnerID)

{
    defaultSkillSaveRepository().deleteOustersSkill(OwnerID, m_SkillType);
}

void OustersSkillSlot::save()

{
    save(m_Name);
}
