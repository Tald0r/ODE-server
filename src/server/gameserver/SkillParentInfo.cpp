//----------------------------------------------------------------------
//
// Filename    : SkillParentInfo.cpp
// Written By  : Reiot
// Description :
//
//----------------------------------------------------------------------

#include "SkillParentInfo.h"

#include "Assert.h"
#include "repository/GameInfoRepository.h"
// #include <algo.h>

//----------------------------------------------------------------------
//
// constructor
//
//----------------------------------------------------------------------
SkillParentInfo::SkillParentInfo(SkillType_t SkillType)

    : m_SkillType(SkillType) {
    init();
}


//----------------------------------------------------------------------
//
// destructor
//
//----------------------------------------------------------------------
SkillParentInfo::~SkillParentInfo()

{}


//----------------------------------------------------------------------
//
// SkillParentInfo::init()
//
//----------------------------------------------------------------------
void SkillParentInfo::init()

{}


//----------------------------------------------------------------------
//
// SkillParentInfo::hasParent()
//
// Check whether a parent of the given type exists.
//
//----------------------------------------------------------------------
bool SkillParentInfo::hasParent(SkillType_t SkillType) {
    try {
        SkillType_t FirstType = m_Parents.front();

        if (FirstType == 0)
            return true;

        list<SkillType_t>::iterator itr;
        for (itr = m_Parents.begin(); itr != m_Parents.end(); itr++) {
            if (*itr == SkillType) {
                break;
            }
        }

        if (itr == m_Parents.end())
            return false;

        return true;
    } catch (Throwable& t) {
        // cerr << t.toString() << endl;
    }
    return false;
}


//----------------------------------------------------------------------
// Return the parent skill type that matches SkillType.
//----------------------------------------------------------------------
SkillType_t SkillParentInfo::getParents(SkillType_t SkillType) const {
    for (list<SkillType_t>::const_iterator itr = m_Parents.begin(); itr != m_Parents.end(); itr++) {
        if (SkillType == (*itr))
            return *itr;
    }

    cerr << "SkillParentInfo::getParents() : NoSuchElementException" << endl;
    throw NoSuchElementException();
}


//----------------------------------------------------------------------
// get debug string
//----------------------------------------------------------------------
string SkillParentInfo::toString() const

{
    StringStream msg;

    msg << "SkillParentInfo(" << "SkillType:" << (int)m_SkillType << ",Parents(";

    for (list<SkillType_t>::const_iterator i = m_Parents.begin(); i != m_Parents.end(); i++) {
        msg << (int)(*i) << " ";
    }
    msg << ")";
    msg << ")";

    return msg.toString();
}


//--------------------------------------------------------------------
//
// constructor
//
//--------------------------------------------------------------------
SkillParentInfoManager::SkillParentInfoManager()

    : m_SkillCount(0), m_SkillParentInfoList(NULL) {}

//--------------------------------------------------------------------
//
// destructor
//
//--------------------------------------------------------------------
SkillParentInfoManager::~SkillParentInfoManager()

{
    // Not everything is freed here.

    SAFE_DELETE_ARRAY(m_SkillParentInfoList);
}

//--------------------------------------------------------------------
//
// SkillParentInfoManager:: init()
//
//--------------------------------------------------------------------
void SkillParentInfoManager::init()

{
    // Loading from DataBase and addSkillParentInfo
    load();
}

//--------------------------------------------------------------------
//
// SkillParentInfoManager:: load()
//
//--------------------------------------------------------------------
void SkillParentInfoManager::load()

{
    __BEGIN_DEBUG

    int maxSkillType = 0;
    if (!defaultGameInfoRepository().loadMaxSkillType(maxSkillType)) {
        throw Error("There is no data in SkillTreeInfo Table");
    }

    m_SkillCount = maxSkillType + 1;

    Assert(m_SkillCount > 0);

    m_SkillParentInfoList = new SkillParentInfo*[m_SkillCount];

    for (uint i = 0; i < m_SkillCount; i++)
        m_SkillParentInfoList[i] = NULL;

    vector<SkillParentRow> rows = defaultGameInfoRepository().loadSkillTree();

    // cout<<"======= SkillParentInfo Manager ==========="<<endl;

    SkillType_t tempSkillType = 0;

    for (size_t r = 0; r < rows.size(); r++) {
        SkillType_t SkillType = rows[r].skillType;

        if (tempSkillType != SkillType) {
            SkillParentInfo* pSkillParentInfo = new SkillParentInfo(SkillType);
            pSkillParentInfo->addParents(rows[r].parent);
            addSkillParentInfo(pSkillParentInfo);
            tempSkillType = SkillType;
        } else {
            m_SkillParentInfoList[SkillType]->addParents(rows[r].parent);
        }
    }

    // for (uint i = 0; i < m_SkillCount; i++)
    //{
    //	if (m_SkillParentInfoList[ i ] != NULL)
    //		//cout<< m_SkillParentInfoList[ i ]->toString()<<endl;
    // }

    __END_DEBUG
}

//--------------------------------------------------------------------
//
// SkillParentInfoManager:: save()
//
//--------------------------------------------------------------------
void SkillParentInfoManager::save()

{
    throw UnsupportedError();
}


//--------------------------------------------------------------------
//
// SkillParentInfoManager:: addSkillParentInfo()
//
//--------------------------------------------------------------------
void SkillParentInfoManager::addSkillParentInfo(SkillParentInfo* pSkillParentInfo)

{
    Assert(pSkillParentInfo != NULL);

    if (m_SkillParentInfoList[pSkillParentInfo->getSkillType()] != NULL)
        throw DuplicatedException();

    m_SkillParentInfoList[pSkillParentInfo->getSkillType()] = pSkillParentInfo;
}

//--------------------------------------------------------------------
//
// SkillParentInfoManager:: getSkillParentInfo()
//
//--------------------------------------------------------------------
SkillParentInfo* SkillParentInfoManager::getSkillParentInfo(SkillType_t SkillType) const {
    if (SkillType >= m_SkillCount) {
        cerr << "SkillParentInfoManager::getSkillParentInfo() : out of bounds" << endl;
        throw OutOfBoundException();
    }

    if (m_SkillParentInfoList[SkillType] == NULL) {
        cerr << "SkillParentInfoManager::getSkillParentInfo() : NoSuchElementException" << endl;
        throw NoSuchElementException();
    }

    return m_SkillParentInfoList[SkillType];
}

//--------------------------------------------------------------------
//
// SkillParentInfoManager:: toString()
//
//--------------------------------------------------------------------
string SkillParentInfoManager::toString() const

{
    StringStream msg;

    msg << "SkillParentInfoManager(";

    for (uint i = 0; i < m_SkillCount; i++) {
        if (m_SkillParentInfoList[i] != NULL) {
            msg << m_SkillParentInfoList[i]->toString();
        } else {
            msg << "NULL";
        }
    }

    msg << ")";

    return msg.toString();
}
