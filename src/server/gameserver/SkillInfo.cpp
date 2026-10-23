//////////////////////////////////////////////////////////////////////////////
// Filename    : SkillInfo.cpp
// Written By  : beowulf
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "SkillInfo.h"

#include "Assert.h"
#include "GameContext.h"
#include "SkillPropertyManager.h"
#include "SkillUtil.h"
#include "repository/ContentInfoRepository.h"

// #include <algo.h>

//////////////////////////////////////////////////////////////////////////////
// class SkillInfo member methods
//////////////////////////////////////////////////////////////////////////////

SkillInfo::SkillInfo() {
    m_RequireSkills.clear();

    m_RequireFire = 0;
    m_RequireWater = 0;
    m_RequireEarth = 0;
    m_RequireWind = 0;
    m_RequireSum = 0;

    m_RequireWristletElemental = ELEMENTAL_ANY;
    m_RequireStone1Elemental = ELEMENTAL_ANY;
    m_RequireStone2Elemental = ELEMENTAL_ANY;
    m_RequireStone3Elemental = ELEMENTAL_ANY;
    m_RequireStone4Elemental = ELEMENTAL_ANY;

    m_CanDelete = 0;
}

SkillInfo::~SkillInfo() {
    __BEGIN_TRY
    __END_CATCH_NO_RETHROW
}

void SkillInfo::setRequireSkill(const string& requireSkill) {
    if (requireSkill == "")
        return;

    /////////////////////////////
    // 012345678901234567890
    // 111,112,114
    // c  d
    //     c  d
    //         c  d
    /////////////////////////////
    size_t a = requireSkill.find_first_of('(', 0);
    size_t b = requireSkill.find_first_of(')', a + 1);

    if (a > b)
        return;

    string requirements = trim(requireSkill.substr(a + 1, b - a - 1));

    size_t c = 0;
    size_t d;
    do {
        d = requirements.find_first_of(',', c + 1);
        if (d == string::npos)
            d = requirements.size();

        SkillType_t skillType = (SkillType_t)atoi(trim(requirements.substr(c, d - c)).c_str());
        addRequireSkill(skillType);

        c = d + 1;
    } while (c < requirements.size() - 1);
}

void SkillInfo::setCondition(const string& condition) {
    if (condition == "")
        return;

    uint a = 0, b = 0, c = 0;

    do {
        ///////////////////////////////////////////////////////
        // 01234567890123456789012345678901234567890123456789
        // (Fire,10)(Water,5)(Wristlet,Fire)(Stone1,Water)
        // a    b  ca     b ca        b    ca      b     c
        ///////////////////////////////////////////////////////
        a = condition.find_first_of('(', c);
        b = condition.find_first_of(',', a + 1);
        c = condition.find_first_of(')', b + 1);

        if (a > b || b > c)
            break;

        Assert(a + 1 < b && b + 1 < c);

        string identifier = trim(condition.substr(a + 1, b - a - 1));
        string require = trim(condition.substr(b + 1, c - b - 1));

        if (identifier == "Fire")
            setRequireFire(atoi(require.c_str()));
        else if (identifier == "Water")
            setRequireWater(atoi(require.c_str()));
        else if (identifier == "Earth")
            setRequireEarth(atoi(require.c_str()));
        else if (identifier == "Wind")
            setRequireWind(atoi(require.c_str()));
        else if (identifier == "Sum")
            setRequireSum(atoi(require.c_str()));
        else if (identifier == "Wristlet")
            setRequireWristletElemental(getElementalTypeFromString(require));
        else if (identifier == "Stone1")
            setRequireStone1Elemental(getElementalTypeFromString(require));
        else if (identifier == "Stone2")
            setRequireStone2Elemental(getElementalTypeFromString(require));
        else if (identifier == "Stone3")
            setRequireStone3Elemental(getElementalTypeFromString(require));
        else if (identifier == "Stone4")
            setRequireStone4Elemental(getElementalTypeFromString(require));

    } while (c < condition.size() - 1);
}

string SkillInfo::toString() const {
    StringStream msg;
    msg << "SkillInfo (" << "Type:" << (int)m_Type << ",Name:" << m_Name << ",Level:" << (int)m_Level
        << ",MinDam:" << (int)m_MinDamage << ",MaxDam:" << (int)m_MaxDamage << ",MinDelay:" << (int)m_MinDelay
        << ",MaxDelay:" << (int)m_MaxDelay << ",MinCastTime:" << (int)m_MinCastTime
        << ",MaxCastTime:" << (int)m_MaxCastTime << ",MinDuration:" << (int)m_MinDuration
        << ",MaxDuration:" << (int)m_MaxDuration << ",ConsumeMP:" << (int)m_ConsumeMP << ",MaxRange:" << (int)m_MaxRange
        << ",MinRange:" << (int)m_MinRange << ",Point:" << (int)m_Point << ",Target:" << (int)m_Target
        << ",RequireFire:" << (int)m_RequireFire << ",RequireWater:" << (int)m_RequireWater
        << ",RequireEarth:" << (int)m_RequireEarth << ",RequireWind:" << (int)m_RequireWind
        << ",RequireSum:" << (int)m_RequireSum << ",RequireWristletElemental:" << (int)m_RequireWristletElemental
        << ",RequireStone1Elemental:" << (int)m_RequireStone1Elemental
        << ",RequireStone2Elemental:" << (int)m_RequireStone2Elemental
        << ",RequireStone3Elemental:" << (int)m_RequireStone3Elemental
        << ",RequireStone4Elemental:" << (int)m_RequireStone4Elemental << ",SkillPoint:" << (int)m_SkillPoint
        << ",LevelUpPoint:" << (int)m_LevelUpPoint;
    for (list<SkillType_t>::const_iterator itr = m_RequireSkills.begin(); itr != m_RequireSkills.end(); itr++) {
        msg << ",SkillType:" << (int)(*itr);
    }

    msg << ")";
    return msg.toString();
}

//////////////////////////////////////////////////////////////////////////////
// class SkillInfoManager member methods
//////////////////////////////////////////////////////////////////////////////

SkillInfoManager::SkillInfoManager()

{
    m_SkillCount = 0;
    m_SkillInfoList = NULL;
}

SkillInfoManager::~SkillInfoManager()

{
    __BEGIN_TRY

    // This alone is not enough, but it is never called anyway, so leave it.
    SAFE_DELETE_ARRAY(m_SkillInfoList);

    __END_CATCH_NO_RETHROW
}

void SkillInfoManager::init() {
    load();

    for (uint i = 0; i < m_SkillCount; ++i) {
        if (m_SkillInfoList[i] != NULL) {
            list<SkillType_t>& rSkills = m_SkillInfoList[i]->getRequireSkills();
            list<SkillType_t>::iterator itr = rSkills.begin();

            for (; itr != rSkills.end(); ++itr) {
                SkillInfo* pRequireSkillInfo = m_SkillInfoList[(*itr)];

                if (pRequireSkillInfo == NULL) {
                    cout << "Skills were loaded out of order. : " << (int)m_SkillInfoList[i]->getType() << " / "
                         << (int)(*itr) << endl;
                    Assert(false);
                }

                pRequireSkillInfo->addRequiredSkill(m_SkillInfoList[i]->getType());
            }
        }
    }

    // Initialize the skill level map.
    for (int i = 0; i < SKILL_DOMAIN_MAX; i++) {
        for (int j = 0; j < SLAYER_MAX_DOMAIN_LEVEL + 1; j++) {
            m_SkillLevelMap[i][j] = 0;
        }
    }

    // Initialize the domain grade per level.
    for (int i = 0; i < GRADE_APPRENTICE_LIMIT_LEVEL + 1; i++) {
        m_DomainGradeMap[i] = SKILL_GRADE_APPRENTICE;
    }
    for (int i = GRADE_APPRENTICE_LIMIT_LEVEL + 1; i < GRADE_ADEPT_LIMIT_LEVEL + 1; i++) {
        m_DomainGradeMap[i] = SKILL_GRADE_ADEPT;
    }
    for (int i = GRADE_ADEPT_LIMIT_LEVEL + 1; i < GRADE_EXPERT_LIMIT_LEVEL + 1; i++) {
        m_DomainGradeMap[i] = SKILL_GRADE_EXPERT;
    }
    for (int i = GRADE_EXPERT_LIMIT_LEVEL + 1; i < GRADE_MASTER_LIMIT_LEVEL + 1; i++) {
        m_DomainGradeMap[i] = SKILL_GRADE_MASTER;
    }
    for (int i = GRADE_MASTER_LIMIT_LEVEL + 1; i < GRADE_GRAND_MASTER_LIMIT_LEVEL + 1; i++) {
        m_DomainGradeMap[i] = SKILL_GRADE_GRAND_MASTER;
    }
    for (int i = GRADE_GRAND_MASTER_LIMIT_LEVEL + 1; i < SLAYER_MAX_DOMAIN_LEVEL + 1; i++) {
        m_DomainGradeMap[i] = SKILL_GRADE_GRAND_MASTER;
    }


    // Initialize the skill level limit per domain grade.
    m_LimitLevelMap[SKILL_GRADE_APPRENTICE] = GRADE_APPRENTICE_LIMIT_LEVEL;
    m_LimitLevelMap[SKILL_GRADE_ADEPT] = GRADE_ADEPT_LIMIT_LEVEL;
    m_LimitLevelMap[SKILL_GRADE_EXPERT] = GRADE_EXPERT_LIMIT_LEVEL;
    m_LimitLevelMap[SKILL_GRADE_MASTER] = GRADE_MASTER_LIMIT_LEVEL;
    m_LimitLevelMap[SKILL_GRADE_GRAND_MASTER] = GRADE_GRAND_MASTER_LIMIT_LEVEL;
    m_LimitLevelMap[SKILL_GRADE_GRAND_MASTER + 1] = GRADE_GRAND_MASTER_LIMIT_LEVEL;

    // Initialize the MP reduction rate table per INT.
    for (int i = 0; i < 43; i++) {
        m_decreaseConsumeMP[i] = 0;
    }

    int provide = 8;
    int Multiple = 1;
    for (int i = 43; i < 85; i++) {
        m_decreaseConsumeMP[i] = 5 * Multiple;
        provide--;
        if (provide == 0) {
            provide = 8;
            Multiple++;
        }
    }

    provide = 10;

    for (int i = 85; i < 140; i++) {
        m_decreaseConsumeMP[i] = 5 * Multiple;
        provide--;
        if (provide == 0) {
            provide = 10;
            Multiple++;
        }
    }

    provide = 15;
    for (int i = 140; i < 201; i++) {
        m_decreaseConsumeMP[i] = 5 * Multiple;
        provide--;
        if (provide == 0) {
            provide = 15;
            Multiple++;
        }
    }

    provide = 15;
    for (int i = 201; i < 301; i++) {
        m_decreaseConsumeMP[i] = 5 * Multiple;
        provide--;
        if (provide == 0) {
            provide = 15;
            Multiple++;
        }
    }
}

void SkillInfoManager::load() {
    __BEGIN_DEBUG

    // Initialise the skill property manager first.
    de::gameContext().skillProps().init();

    ContentInfoRepository& repository = defaultContentInfoRepository();

    int maxType = 0;
    if (!repository.loadMaxSkillBalanceType(maxType)) {
        throw Error("There is no data in SkillInfo Table");
    }

    m_SkillCount = maxType + 1;

    Assert(m_SkillCount > 0);

    m_SkillInfoList = new SkillInfo*[m_SkillCount];

    for (uint i = 0; i < m_SkillCount; i++)
        m_SkillInfoList[i] = NULL;

    vector<SkillBalanceRow> rows = repository.loadSkillBalances();

    for (size_t r = 0; r < rows.size(); r++) {
        const SkillBalanceRow& row = rows[r];
        SkillInfo* pSkillInfo = new SkillInfo();

        pSkillInfo->setType(row.type);
        pSkillInfo->setName(row.name);
        pSkillInfo->setLevel(row.level);

        pSkillInfo->setMinDamage(row.minDamage);
        pSkillInfo->setMaxDamage(row.maxDamage);
        pSkillInfo->setMinDelay(row.minDelay);
        pSkillInfo->setMaxDelay(row.maxDelay);
        pSkillInfo->setMinDuration(row.minDuration);
        pSkillInfo->setMaxDuration(row.maxDuration);

        pSkillInfo->setConsumeMP(row.mana);
        pSkillInfo->setMinRange(row.minRange);
        pSkillInfo->setMaxRange(row.maxRange);
        pSkillInfo->setTarget(row.target);
        pSkillInfo->setSubSkill(row.subExp);
        pSkillInfo->setPoint(row.point);
        pSkillInfo->setDomainType(row.domain);
        pSkillInfo->setMagicDomain(row.magicDomain);

        // Add the skill property.
        SkillProperty* pSkillProperty = new SkillProperty();

        pSkillProperty->setType(pSkillInfo->getType());
        pSkillProperty->setMelee(row.melee);
        pSkillProperty->setMagic(row.magic);
        pSkillProperty->setPhysic(row.physic);

        de::gameContext().skillProps().addSkillProperty(pSkillProperty);

        if (pSkillInfo->getDomainType() == SKILL_DOMAIN_OUSTERS) {
            pSkillInfo->setSkillPoint(row.skillPoint);
            pSkillInfo->setLevelUpPoint(row.levelUpPoint);
            pSkillInfo->setRequireSkill(row.requireSkill);
            pSkillInfo->setCondition(row.condition);
            pSkillInfo->setElementalDomain(row.elementalDomain);
            pSkillInfo->setCanDelete(row.canDelete);
        }

        // Add the skill info.
        addSkillInfo(pSkillInfo);
    }

    __END_DEBUG
}

void SkillInfoManager::save() {
    throw UnsupportedError();
}

void SkillInfoManager::addSkillInfo(SkillInfo* pSkillInfo) {
    Assert(pSkillInfo != NULL);

    if (m_SkillInfoList[pSkillInfo->getType()] != NULL)
        throw DuplicatedException();

    m_SkillInfoList[pSkillInfo->getType()] = pSkillInfo;

    m_SkillLevelMap[pSkillInfo->getDomainType()][pSkillInfo->getLevel()] = pSkillInfo->getType();
}

SkillInfo* SkillInfoManager::getSkillInfo(SkillType_t SkillType) const {
    if (SkillType >= m_SkillCount) {
        cerr << "SkillInfoManager::getSkillInfo() : out of bounds : " << (int)SkillType << endl;
        throw OutOfBoundException();
    }

    if (m_SkillInfoList[SkillType] == NULL) {
        cerr << "SkillInfoManager::getSkillInfo() : no such element : " << (int)SkillType << endl;
        throw NoSuchElementException();
    }

    return m_SkillInfoList[SkillType];
}

SkillType_t SkillInfoManager::getSkillTypeByLevel(SkillDomainType_t SkillDomain, Level_t Level) {
    if (SkillDomain >= SKILL_DOMAIN_MAX)
        throw OutOfBoundException();
    if (Level > SLAYER_MAX_DOMAIN_LEVEL)
        throw OutOfBoundException();

    return m_SkillLevelMap[SkillDomain][Level];
}

// Returns the grade a domain level belongs to.
SkillGrade SkillInfoManager::getGradeByDomainLevel(Level_t Level) {
    if (Level > SLAYER_MAX_DOMAIN_LEVEL)
        throw OutOfBoundException();
    return m_DomainGradeMap[Level];
}

// Returns how far a skill can be raised at the current grade.
Level_t SkillInfoManager::getLimitLevelByDomainGrade(SkillGrade Grade) {
    if (Grade > SKILL_GRADE_MAX)
        throw OutOfBoundException();
    return m_LimitLevelMap[Grade];
}

// SkillInfoManager:: toString()
string SkillInfoManager::toString() const

{
    StringStream msg;

    msg << "SkillInfoManager(\n";

    for (int i = 0; i < (int)m_SkillCount; i++) {
        msg << "SkillInfos[" << i << "] == ";
        msg << (m_SkillInfoList[i] == NULL ? "NULL" : m_SkillInfoList[i]->getName());
        msg << "\n";
        if (m_SkillInfoList[i] != NULL) {
            msg << m_SkillInfoList[i]->toString() << "\n";
        }
    }

    msg << "\n";

    for (int i = 0; i < SKILL_DOMAIN_MAX; i++) {
        for (int j = 0; i < SLAYER_MAX_DOMAIN_LEVEL + 1; i++) {
            msg << "SkillLevelMap [" << i << "][" << j << "] == ";
            msg << (int)m_SkillLevelMap[i][j] << "\n";
        }
    }

    for (int i = 0; i < SLAYER_MAX_DOMAIN_LEVEL + 1; i++) {
        msg << "DomainGradeMap [" << i << "] == ";
        msg << (int)m_DomainGradeMap[i] << "\n";
    }

    for (int i = 0; i < SKILL_GRADE_MAX; i++) {
        msg << "LimitLevelMap [" << i << "] == ";
        msg << (int)m_LimitLevelMap[i] << "\n";
    }

    msg << ")";

    return msg.toString();
}
