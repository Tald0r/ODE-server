//////////////////////////////////////////////////////////////////////////////
// Filename    : VampireSkillSlot.cpp
// Written By  : elca
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "VampireSkillSlot.h"

#include "repository/SkillSaveRepository.h"

void VampireSkillSlot::create(const string& OwnerID)

{
    VampireSkillRecord record;
    record.skillType = m_SkillType;
    record.delay = m_Interval;
    record.castingTime = m_CastingTime;
    record.nextTime = m_runTime.tv_sec;
    defaultSkillSaveRepository().insertVampireSkill(OwnerID, record);
}

void VampireSkillSlot::save(const string& OwnerID)

{
    defaultSkillSaveRepository().updateVampireSkill(OwnerID, m_SkillType, m_Interval);
}

void VampireSkillSlot::save()

{
    defaultSkillSaveRepository().updateVampireSkill(m_Name, m_SkillType, m_Interval);
}
