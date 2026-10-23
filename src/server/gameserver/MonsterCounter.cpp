//////////////////////////////////////////////////////////////////////////////
// Filename    : MonsterCounter.h
// Written By  : excel96
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "MonsterCounter.h"

#include "Assert.h"

DWORD MonsterCounter::s_MonsterCounter = 0;

MonsterCounter::MonsterCounter(SpriteType_t spriteType, WORD maxMonsters, WORD currentMonsters)

{
    m_SpriteType = spriteType;
    m_MaxMonsters = maxMonsters;
    m_CurrentMonsters = currentMonsters;

    s_MonsterCounter += maxMonsters;
}

void MonsterCounter::addMonster()

{
    // Assert(m_CurrentMonsters <= m_MaxMonsters);
    m_CurrentMonsters++;
}

void MonsterCounter::addMonster(WORD Count)

{
    // Assert(m_CurrentMonsters <= m_MaxMonsters);
    m_CurrentMonsters += Count;
}

void MonsterCounter::deleteMonster()

{
    Assert(m_CurrentMonsters > 0);
    m_CurrentMonsters--;
}

// get debug string
string MonsterCounter::toString() const {
    StringStream msg;
    msg << "MonsterCounter(" << "SpriteType:" << m_SpriteType << "," << m_CurrentMonsters << "/" << m_MaxMonsters
        << ")";
    return msg.toString();
}
