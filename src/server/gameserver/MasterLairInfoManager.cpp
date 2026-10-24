//////////////////////////////////////////////////////////////////////////////
// Filename    : MasterLairInfoManager.cpp
// Written By  : reiot
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "MasterLairInfoManager.h"

#include "Assert.h"
#include "GameContext.h"
#include "MonsterInfo.h"
#include "ZoneInfo.h"
#include "ZoneInfoManager.h"
#include "repository/WarInfoRepository.h"

//////////////////////////////////////////////////////////////////////////////
// constructor
//////////////////////////////////////////////////////////////////////////////
MasterLairInfo::MasterLairInfo() {
    m_ZoneID = 0;
    m_MasterMonsterType = 0;
    m_MasterNotReadyMonsterType = 0;
    m_MasterRemainNotReady = false;
    m_MasterX = 0;
    m_MasterY = 0;
    m_MasterDir = 0;
    m_MaxPassPlayer = 0;
    // m_MaxSummonMonster = 0;
    m_SummonX = 0;
    m_SummonY = 0;
    m_FirstRegenDelay = 0;
    m_RegenDelay = 0;
    m_StartDelay = 0;
    m_EndDelay = 0;
    m_KickOutDelay = 0;
    m_KickZoneID = 0;
    m_KickZoneX = 0;
    m_KickZoneY = 0;
    m_LairAttackTick = 0;
    m_LairAttackMinNumber = 0;
    m_LairAttackMaxNumber = 0;
}

MasterLairInfo::~MasterLairInfo() {}

// a      bc
// "hihihi", "dsfsdfsd sdfds, sfd", "anfdsf"
void MasterLairInfo::setSayText(vector<string>& sayWords, const string& text)

{
    sayWords.clear();

    if (text.empty())
        return;

    size_t a = 0, b = 0, c = 0;
    while (c < text.size()) {
        a = text.find_first_of('\"', c);
        b = text.find_first_of('\"', a + 1);
        c = text.find_first_of(',', b + 1);

        if (a == string::npos || b == string::npos)
            break;

        string words = text.substr(a + 1, b - a - 1);

        sayWords.push_back(words);

        //		cout << "SayWord: " << words.c_str() << endl;
    }
}

void MasterLairInfo::setMasterSummonSay(const string& text)

{
    setSayText(m_MasterSummonSay, text);
}

void MasterLairInfo::setMasterDeadSlayerSay(const string& text)

{
    setSayText(m_MasterDeadSlayerSay, text);
}

void MasterLairInfo::setMasterDeadVampireSay(const string& text)

{
    setSayText(m_MasterDeadVampireSay, text);
}

void MasterLairInfo::setMasterNotDeadSay(const string& text)

{
    setSayText(m_MasterNotDeadSay, text);
}

const string& MasterLairInfo::getRandomMasterSummonSay() const

{
    static string stringNULL = "";

    if (m_MasterSummonSay.empty())
        return stringNULL;

    return m_MasterSummonSay[rand() % m_MasterSummonSay.size()];
}


const string& MasterLairInfo::getRandomMasterDeadSlayerSay() const

{
    static string stringNULL = "";

    if (m_MasterDeadSlayerSay.empty())
        return stringNULL;

    return m_MasterDeadSlayerSay[rand() % m_MasterDeadSlayerSay.size()];
}

const string& MasterLairInfo::getRandomMasterDeadVampireSay() const

{
    static string stringNULL = "";

    if (m_MasterDeadVampireSay.empty())
        return stringNULL;

    return m_MasterDeadVampireSay[rand() % m_MasterDeadVampireSay.size()];
}

const string& MasterLairInfo::getRandomMasterNotDeadSay() const

{
    static string stringNULL = "";

    if (m_MasterNotDeadSay.empty())
        return stringNULL;

    return m_MasterNotDeadSay[rand() % m_MasterNotDeadSay.size()];
}

string MasterLairInfo::toString() const {
    StringStream msg;
    msg << "MasterLairInfo(" << "ZoneID:" << (int)m_ZoneID
        << ",MasterNotReadyMonsterType:" << (int)m_MasterNotReadyMonsterType
        << ",MasterMonsterType:" << (int)m_MasterMonsterType << ",MasterRemainNotReady:" << (int)m_MasterRemainNotReady
        << ",MasterX:" << (int)m_MasterX << ",MasterY:" << (int)m_MasterY << ",MasterDir:" << (int)m_MasterDir
        << ",MaxPassPlayer:"
        << (int)m_MaxPassPlayer
        //<< ",MaxSummonMonster:" << (int)m_MaxSummonMonster
        << ",SummonX:" << (int)m_SummonX << ",SummonY:" << (int)m_SummonY
        << ",FirstRegenDelay:" << (int)m_FirstRegenDelay << ",RegenDelay:" << (int)m_RegenDelay
        << ",StartDelay:" << (int)m_StartDelay << ",EndDelay:" << (int)m_EndDelay
        << ",KickOutDelay:" << (int)m_KickOutDelay << ",KickZoneID:" << (int)m_KickZoneID
        << ",KickZoneX:" << (int)m_KickZoneX << ",KickZoneY:" << (int)m_KickZoneY
        << ",LairAttackTick:" << (int)m_LairAttackTick << ",LairAttackMinNumber:" << (int)m_LairAttackMinNumber
        << ",LairAttackMaxNumber:" << (int)m_LairAttackMaxNumber << ")";

    return msg.toString();
}

//////////////////////////////////////////////////////////////////////////////
// constructor
//////////////////////////////////////////////////////////////////////////////
MasterLairInfoManager::MasterLairInfoManager()

{}


//////////////////////////////////////////////////////////////////////////////
// destructor
//////////////////////////////////////////////////////////////////////////////
MasterLairInfoManager::~MasterLairInfoManager()

{
    try {
        unordered_map<ZoneID_t, MasterLairInfo*>::iterator itr = m_MasterLairInfos.begin();
        for (; itr != m_MasterLairInfos.end(); itr++) {
            MasterLairInfo* pInfo = itr->second;
            SAFE_DELETE(pInfo);
        }

        // Delete every pair in the hashmap.
        m_MasterLairInfos.clear();
    } catch (Throwable&) {
        // A Throwable from the try block is dropped here and goes no further.
    }
}


//////////////////////////////////////////////////////////////////////////////
// initialize zone info manager
//////////////////////////////////////////////////////////////////////////////
void MasterLairInfoManager::init()

{
    // init == load
    load();
}


// void testMaxMemory();

//////////////////////////////////////////////////////////////////////////////
// load from database
//////////////////////////////////////////////////////////////////////////////
void MasterLairInfoManager::load()

{
    vector<MasterLairRow> rows = defaultWarInfoRepository().loadMasterLairs();

    for (size_t r = 0; r < rows.size(); r++) {
        const MasterLairRow& row = rows[r];

        ZoneID_t zoneID = row.zoneID;

        ZoneInfo* pZoneInfo = de::gameContext().zoneInfos().getZoneInfo(zoneID);
        Assert(pZoneInfo != NULL);

        pZoneInfo->setMasterLair();

        MasterLairInfo* pMasterLairInfo = new MasterLairInfo();
        // cout << "new OK" << endl;

        pMasterLairInfo->setZoneID(zoneID);
        pMasterLairInfo->setMasterNotReadyMonsterType(row.masterNotReadyMonsterType);
        pMasterLairInfo->setMasterMonsterType(row.masterMonsterType);
        pMasterLairInfo->setMasterRemainNotReady(row.masterRemainNotReady != 0);
        pMasterLairInfo->setMasterX(row.masterX);
        pMasterLairInfo->setMasterY(row.masterY);
        pMasterLairInfo->setMasterDir(row.masterDir);
        pMasterLairInfo->setMaxPassPlayer(row.maxPassPlayer);
        // pMasterLairInfo->setMaxSummonMonster( pResult->getInt(++i) );
        pMasterLairInfo->setSummonX(row.summonX);
        pMasterLairInfo->setSummonY(row.summonY);
        pMasterLairInfo->setFirstRegenDelay(row.firstRegenDelay);
        pMasterLairInfo->setRegenDelay(row.regenDelay);
        pMasterLairInfo->setStartDelay(row.startDelay);
        pMasterLairInfo->setEndDelay(row.endDelay);
        pMasterLairInfo->setKickOutDelay(row.kickOutDelay);
        pMasterLairInfo->setKickZoneID(row.kickZoneID);
        pMasterLairInfo->setKickZoneX(row.kickZoneX);
        pMasterLairInfo->setKickZoneY(row.kickZoneY);
        pMasterLairInfo->setLairAttackTick(row.lairAttackTick);
        pMasterLairInfo->setLairAttackMinNumber(row.lairAttackMinNumber);
        pMasterLairInfo->setLairAttackMaxNumber(row.lairAttackMaxNumber);
        pMasterLairInfo->setMasterSummonSay(row.masterSummonSay);
        pMasterLairInfo->setMasterDeadSlayerSay(row.masterDeadSlayerSay);
        pMasterLairInfo->setMasterDeadVampireSay(row.masterDeadVampireSay);
        pMasterLairInfo->setMasterNotDeadSay(row.masterNotDeadSay);

        addMasterLairInfo(pMasterLairInfo);

        // cout << pMasterLairInfo->toString().c_str() << endl;

        // Sanity check: the master's monster info must exist and be a master.
        try {
            const MonsterInfo* pMonsterInfo =
                de::gameContext().monsterInfos().getMonsterInfo(pMasterLairInfo->getMasterMonsterType());

            if (pMonsterInfo == NULL) // for later. -_-;
            {
                throw Error("No monster info for the master.");
            }

            if (!pMonsterInfo->isMaster()) {
                StringStream msg;
                msg << "Monster type[" << (int)pMasterLairInfo->getMasterMonsterType() << "] is not a master.";

                throw Error(msg.toString());
            }

        } catch (Throwable& t) {
            cout << t.toString().c_str() << endl;
            throw Error("No monster info for the master.");
        }
    }
}

//////////////////////////////////////////////////////////////////////////////
// reload from database
//////////////////////////////////////////////////////////////////////////////
void MasterLairInfoManager::reload()

{
    // cout << "MasterLairInfoManager::reload()" << endl;

    vector<MasterLairRow> rows = defaultWarInfoRepository().loadMasterLairs();

    for (size_t r = 0; r < rows.size(); r++) {
        const MasterLairRow& row = rows[r];

        ZoneID_t zoneID = row.zoneID;

        MasterLairInfo* pMasterLairInfo = getMasterLairInfo(zoneID);
        // cout << "new OK" << endl;

        if (pMasterLairInfo != NULL) {
            // cout << "Reload: " << zoneID << endl;
            pMasterLairInfo->setZoneID(zoneID);
            pMasterLairInfo->setMasterNotReadyMonsterType(row.masterNotReadyMonsterType);
            pMasterLairInfo->setMasterMonsterType(row.masterMonsterType);
            pMasterLairInfo->setMasterRemainNotReady(row.masterRemainNotReady != 0);
            pMasterLairInfo->setMasterX(row.masterX);
            pMasterLairInfo->setMasterY(row.masterY);
            pMasterLairInfo->setMasterDir(row.masterDir);
            pMasterLairInfo->setMaxPassPlayer(row.maxPassPlayer);
            // pMasterLairInfo->setMaxSummonMonster( pResult->getInt(++i) );
            pMasterLairInfo->setSummonX(row.summonX);
            pMasterLairInfo->setSummonY(row.summonY);
            pMasterLairInfo->setFirstRegenDelay(row.firstRegenDelay);
            pMasterLairInfo->setRegenDelay(row.regenDelay);
            pMasterLairInfo->setStartDelay(row.startDelay);
            pMasterLairInfo->setEndDelay(row.endDelay);
            pMasterLairInfo->setKickOutDelay(row.kickOutDelay);
            pMasterLairInfo->setKickZoneID(row.kickZoneID);
            pMasterLairInfo->setKickZoneX(row.kickZoneX);
            pMasterLairInfo->setKickZoneY(row.kickZoneY);
            pMasterLairInfo->setLairAttackTick(row.lairAttackTick);
            pMasterLairInfo->setLairAttackMinNumber(row.lairAttackMinNumber);
            pMasterLairInfo->setLairAttackMaxNumber(row.lairAttackMaxNumber);
            pMasterLairInfo->setMasterSummonSay(row.masterSummonSay);
            pMasterLairInfo->setMasterDeadSlayerSay(row.masterDeadSlayerSay);
            pMasterLairInfo->setMasterDeadVampireSay(row.masterDeadVampireSay);
            pMasterLairInfo->setMasterNotDeadSay(row.masterNotDeadSay);

            // Sanity check: the master's monster info must exist and be a master.
            try {
                const MonsterInfo* pMonsterInfo =
                    de::gameContext().monsterInfos().getMonsterInfo(pMasterLairInfo->getMasterMonsterType());

                if (pMonsterInfo == NULL) // for later. -_-;
                {
                    throw Error("No monster info for the master.");
                }

                if (!pMonsterInfo->isMaster()) {
                    StringStream msg;
                    msg << "Monster type[" << (int)pMasterLairInfo->getMasterMonsterType() << "] is not a master.";

                    throw Error(msg.toString());
                }

            } catch (Throwable& t) {
                cout << t.toString().c_str() << endl;
                throw Error("No monster info for the master.");
            }
        }
    }
}


//////////////////////////////////////////////////////////////////////////////
// add zone info to zone info manager
//////////////////////////////////////////////////////////////////////////////
void MasterLairInfoManager::addMasterLairInfo(MasterLairInfo* pMasterLairInfo)

{
    // First check whether a zone with the same id already exists.
    unordered_map<ZoneID_t, MasterLairInfo*>::iterator itr = m_MasterLairInfos.find(pMasterLairInfo->getZoneID());

    if (itr != m_MasterLairInfos.end())
        // The same id already exists.
        throw Error("duplicated zone id");

    m_MasterLairInfos[pMasterLairInfo->getZoneID()] = pMasterLairInfo;
}


//////////////////////////////////////////////////////////////////////////////
// Delete zone info from zone info manager
//////////////////////////////////////////////////////////////////////////////
void MasterLairInfoManager::deleteMasterLairInfo(ZoneID_t zoneID) {
    unordered_map<ZoneID_t, MasterLairInfo*>::iterator itr = m_MasterLairInfos.find(zoneID);

    if (itr != m_MasterLairInfos.end()) {
        // Delete the zone.
        SAFE_DELETE(itr->second);

        // Delete the pair.
        m_MasterLairInfos.erase(itr);
    } else {
        // No such zone id was found.
        StringStream msg;
        msg << "ZoneID : " << zoneID;
        throw NoSuchElementException(msg.toString());
    }
}


//////////////////////////////////////////////////////////////////////////////
// get zone from zone info manager
//////////////////////////////////////////////////////////////////////////////
MasterLairInfo* MasterLairInfoManager::getMasterLairInfo(ZoneID_t zoneID) {
    MasterLairInfo* pMasterLairInfo = NULL;

    unordered_map<ZoneID_t, MasterLairInfo*>::iterator itr = m_MasterLairInfos.find(zoneID);

    if (itr != m_MasterLairInfos.end()) {
        pMasterLairInfo = itr->second;

    } else {
        // No such zone id was found.

        return NULL;
    }

    return pMasterLairInfo;
}

//////////////////////////////////////////////////////////////////////////////
// get debug string
//////////////////////////////////////////////////////////////////////////////
string MasterLairInfoManager::toString() const

{
    StringStream msg;

    msg << "MasterLairInfoManager(";

    if (m_MasterLairInfos.empty())
        msg << "EMPTY";
    else {
        for (unordered_map<ZoneID_t, MasterLairInfo*>::const_iterator itr = m_MasterLairInfos.begin();
             itr != m_MasterLairInfos.end(); itr++) {
            msg << itr->second->toString();
        }
    }

    msg << ")";

    return msg.toString();
}
