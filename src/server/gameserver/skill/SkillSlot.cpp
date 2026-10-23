//////////////////////////////////////////////////////////////////////////////
// Filename    : SkillSlot.cpp
// Written By  : elca
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "SkillSlot.h"

#include "repository/SkillSaveRepository.h"

SkillSlot::SkillSlot() {
    // The Vampire/Ousters slot classes zero these in their default
    // constructors; this one historically left them uninitialized, which
    // makes setRunTime(0)'s m_Interval comparison read garbage on a
    // freshly new'd slot.
    m_SkillType = 0;
    m_Exp = 0;
    m_ExpLevel = 0;
    m_Interval = 0;
    m_CastingTime = 0;
    m_Enable = true;
}

SkillSlot::SkillSlot(SkillType_t SkillType, DWORD Exp, ulong Interval) {
    m_SkillType = SkillType;
    m_Exp = Exp;
    m_ExpLevel = 0;
    m_Interval = Interval;
}

SkillSlot::~SkillSlot() {
    m_SkillType = 0;
    m_Exp = 0;
    m_ExpLevel = 0;
    m_Interval = 0;
}

void SkillSlot::create(const string& OwnerID)

{
    SlayerSkillRecord record;
    record.skillType = m_SkillType;
    record.skillLevel = m_ExpLevel;
    record.skillExp = m_Exp;
    record.delay = m_Interval;
    record.castingTime = m_CastingTime;
    record.nextTime = m_runTime.tv_sec;
    defaultSkillSaveRepository().insertSlayerSkill(OwnerID, record);
}

void SkillSlot::save(const string& OwnerID)

{
    defaultSkillSaveRepository().updateSlayerSkill(OwnerID, m_SkillType, m_ExpLevel, m_Exp, m_Interval);
}

void SkillSlot::save()

{
    defaultSkillSaveRepository().updateSlayerSkill(m_Name, m_SkillType, m_ExpLevel, m_Exp, m_Interval);
}

Turn_t SkillSlot::getRemainTurn(Timeval currentTime) const {
    // Turn_t is unsigned and the number goes out as the skill's casting
    // time, so a run time already past reports no turns left rather than a
    // remainder near 2^32.
    const long long remainTurn =
        (long long)(m_runTime.tv_sec - currentTime.tv_sec) * 10 + (m_runTime.tv_usec - currentTime.tv_usec) / 100000;

    return remainTurn > 0 ? (Turn_t)remainTurn : 0;
}

void SkillSlot::setRunTime() {
    // Get the current time.
    getCurrentTime(m_runTime);

    // Set the time at which it can be used again.
    m_runTime.tv_sec += m_Interval / 10;
    m_runTime.tv_usec += (m_Interval % 10) * 100000;
}

void SkillSlot::setRunTime(Turn_t delay, bool bSave) {
    // Get the current time.
    getCurrentTime(m_runTime);

    // Set the time at which it can be used again.
    m_runTime.tv_sec += delay / 10;
    m_runTime.tv_usec += (delay % 10) * 100000;

    if (delay != m_Interval && bSave) {
        m_Interval = delay;

        switch (m_SkillType) {
        case SKILL_ATTACK_MELEE:
        case SKILL_ATTACK_ARMS:
        case SKILL_SELF:
        case SKILL_TILE:
        case SKILL_OBJECT:
        case SKILL_BLOOD_DRAIN:
        case SKILL_UN_BURROW:
        case SKILL_UN_TRANSFORM:
        case SKILL_UN_INVISIBILITY:
        case SKILL_THROW_HOLY_WATER:
        case SKILL_EAT_CORPSE:
        case SKILL_HOWL:
            break;
        default:
            save(m_Name); // Save the changed delay.
            break;
        }
    }
}
