#include "RaceWarLimiter.h"

#include <stdio.h>

#include <fstream>

#include "DB.h"
#include "Ousters.h"
#include "Slayer.h"
#include "VSDateTime.h"
#include "Vampire.h"
#include "repository/WarInfoRepository.h"

//--------------------------------------------------------------------------------
//
// 					PCWarLimiter
//
//--------------------------------------------------------------------------------
PCWarLimiter::PCWarLimiter() {}

PCWarLimiter::~PCWarLimiter() {}

//--------------------------------------------------------------------------------
// load
//--------------------------------------------------------------------------------
void PCWarLimiter::load()

{
    __ENTER_CRITICAL_SECTION(m_Mutex)

    clear();

    vector<RaceWarLimitRow> limits = defaultWarInfoRepository().loadRaceWarLimits(getTableName(), (int)getRace());

    for (size_t r = 0; r < limits.size(); r++) {
        LevelLimitInfo lli(limits[r].id, limits[r].minLevel, limits[r].maxLevel, limits[r].limitNum);
        lli.setCurrent(limits[r].currentNum);

        addLimitInfo(lli);
    }

    __LEAVE_CRITICAL_SECTION(m_Mutex)
}

//--------------------------------------------------------------------------------
// saveCurrent
//
// Must be called with the lock held.
//--------------------------------------------------------------------------------
void PCWarLimiter::clearCurrent()

{
    // Set the participant count to 0
    int num = m_LimitInfos.size();
    for (int i = 0; i < num; i++) {
        LimitInfo_t* pLI = &(m_LimitInfos[i]);

        pLI->setCurrent(0);
    }

    // Set it to 0 in the DB too.
    defaultWarInfoRepository().clearRaceWarCurrentNums(getTableName());
}

//--------------------------------------------------------------------------------
// saveCurrent
//
// Must be called with the lock held.
//--------------------------------------------------------------------------------
void PCWarLimiter::saveCurrent(const LevelLimitInfo* pLI) const

{
    Assert(pLI != NULL);

    defaultWarInfoRepository().saveRaceWarCurrentNum(getTableName(), pLI->getCurrent(), pLI->getID());
}

//--------------------------------------------------------------------------------
// join
//--------------------------------------------------------------------------------
bool PCWarLimiter::join(PlayerCreature* pPC)

{
    bool isJoin = false;

    __ENTER_CRITICAL_SECTION(m_Mutex)

    LimitInfo_t* pLI = getLimitInfo(pPC);

    if (pLI != NULL && !pLI->isLimit()) {
        pLI->increase();
        saveCurrent(pLI);

        isJoin = true;
    }

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    return isJoin;
}

//--------------------------------------------------------------------------------
// leave
//--------------------------------------------------------------------------------
bool PCWarLimiter::leave(PlayerCreature* pPC)

{
    bool isLeave = false;

    __ENTER_CRITICAL_SECTION(m_Mutex)

    LimitInfo_t* pLI = getLimitInfo(pPC);

    if (pLI != NULL) {
        pLI->decrease();
        saveCurrent(pLI);

        isLeave = true;
    }

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    return isLeave;
}

//--------------------------------------------------------------------------------
//
// 					SlayerWarLimiter
//
//--------------------------------------------------------------------------------
SlayerWarLimiter::SlayerWarLimiter() {
    m_Mutex.setName("SlayerWarLimiter");
}

SlayerWarLimiter::~SlayerWarLimiter() {}

//--------------------------------------------------------------------------------
// getLimitInfo( PC )
//--------------------------------------------------------------------------------
SlayerWarLimiter::LimitInfo_t* SlayerWarLimiter::getLimitInfo(PlayerCreature* pPC)

{
    Assert(pPC != NULL);
    Assert(pPC->isSlayer());
    Assert(!m_LimitInfos.empty());

    Slayer* pSlayer = dynamic_cast<Slayer*>(pPC);
    int SUM = pSlayer->getSTR(ATTR_BASIC) + pSlayer->getDEX(ATTR_BASIC) + pSlayer->getINT(ATTR_BASIC);
    int Level = SUM;

    int num = m_LimitInfos.size();
    for (int i = 0; i < num; i++) {
        LimitInfo_t* pLI = &(m_LimitInfos[i]);

        if (pLI->isLevelInRange(Level)) {
            return pLI;
        }
    }

    return NULL;
}


//--------------------------------------------------------------------------------
//
// 					VampireWarLimiter
//
//--------------------------------------------------------------------------------
VampireWarLimiter::VampireWarLimiter() {
    m_Mutex.setName("VampireWarLimiter");
}

VampireWarLimiter::~VampireWarLimiter() {}

//--------------------------------------------------------------------------------
// getLimitInfo( PC )
//--------------------------------------------------------------------------------
VampireWarLimiter::LimitInfo_t* VampireWarLimiter::getLimitInfo(PlayerCreature* pPC)

{
    Assert(pPC != NULL);
    Assert(pPC->isVampire());
    Assert(!m_LimitInfos.empty());

    Vampire* pVampire = dynamic_cast<Vampire*>(pPC);
    int Level = pVampire->getLevel();

    int num = m_LimitInfos.size();

    for (int i = 0; i < num; i++) {
        LimitInfo_t* pLI = &(m_LimitInfos[i]);

        if (pLI->isLevelInRange(Level)) {
            return pLI;
        }
    }

    return NULL;
}

//--------------------------------------------------------------------------------
//
// 					OustersWarLimiter
//
//--------------------------------------------------------------------------------
OustersWarLimiter::OustersWarLimiter() {
    m_Mutex.setName("OustersWarLimiter");
}

OustersWarLimiter::~OustersWarLimiter() {}

//--------------------------------------------------------------------------------
// getLimitInfo( PC )
//--------------------------------------------------------------------------------
OustersWarLimiter::LimitInfo_t* OustersWarLimiter::getLimitInfo(PlayerCreature* pPC)

{
    Assert(pPC != NULL);
    Assert(pPC->isOusters());
    Assert(!m_LimitInfos.empty());

    Ousters* pOusters = dynamic_cast<Ousters*>(pPC);
    int Level = pOusters->getLevel();

    int num = m_LimitInfos.size();

    for (int i = 0; i < num; i++) {
        LimitInfo_t* pLI = &(m_LimitInfos[i]);

        if (pLI->isLevelInRange(Level)) {
            return pLI;
        }
    }

    return NULL;
}

//--------------------------------------------------------------------------------
//
//						RaceWarLimiter
//
//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
// clear
//--------------------------------------------------------------------------------
void RaceWarLimiter::clearCurrent()

{
    m_SlayerWarLimiter.clearCurrent();
    m_VampireWarLimiter.clearCurrent();
    m_OustersWarLimiter.clearCurrent();
}

//--------------------------------------------------------------------------------
// load
//--------------------------------------------------------------------------------
void RaceWarLimiter::load()

{
    m_SlayerWarLimiter.load();
    m_VampireWarLimiter.load();
    m_OustersWarLimiter.load();
}

//--------------------------------------------------------------------------------
// join
//--------------------------------------------------------------------------------
bool RaceWarLimiter::join(PlayerCreature* pPC)

{
    Assert(pPC != NULL);

    bool isJoin = false;

    if (pPC->isSlayer()) {
        isJoin = m_SlayerWarLimiter.join(pPC);
    } else if (pPC->isVampire()) {
        Assert(pPC->isVampire());

        isJoin = m_VampireWarLimiter.join(pPC);
    } else if (pPC->isOusters()) {
        Assert(pPC->isOusters());

        isJoin = m_OustersWarLimiter.join(pPC);
    }

    if (isJoin) {
        addPCList(pPC);
        pPC->setFlag(Effect::EFFECT_CLASS_RACE_WAR_JOIN_TICKET);
    }

    return isJoin;
}

//--------------------------------------------------------------------------------
// leave
//--------------------------------------------------------------------------------
bool RaceWarLimiter::leave(PlayerCreature* pPC)

{
    Assert(pPC != NULL);

    bool isLeave = false;

    if (pPC->isSlayer()) {
        isLeave = m_SlayerWarLimiter.leave(pPC);
    } else if (pPC->isVampire()) {
        Assert(pPC->isVampire());

        isLeave = m_VampireWarLimiter.leave(pPC);
    } else if (pPC->isOusters()) {
        Assert(pPC->isOusters());

        isLeave = m_OustersWarLimiter.leave(pPC);
    }

    if (isLeave) {
        removePCList(pPC);
    }

    return isLeave;
}

//--------------------------------------------------------------------------------
// get LimitInfo( race, index )
//--------------------------------------------------------------------------------
LevelLimitInfo* RaceWarLimiter::getLimitInfo(Race_t race, int index)

{
    if (race == RACE_SLAYER) {
        return m_SlayerWarLimiter.getLimitInfoByIndex(index);
    } else if (race == RACE_VAMPIRE) {
        return m_VampireWarLimiter.getLimitInfoByIndex(index);
    }

    Assert(race == RACE_OUSTERS);

    return m_OustersWarLimiter.getLimitInfoByIndex(index);
}

//--------------------------------------------------------------------------------
// clear PCList
//--------------------------------------------------------------------------------
void RaceWarLimiter::clearPCList()

{
    VSDateTime current = VSDateTime::currentDateTime();
    char filename[128];
    snprintf(filename, sizeof(filename), "RaceWarPCList%s.txt", current.toString().c_str());
    ofstream file(filename, ios::out | ios::app);

    int num[3] = {0, 0, 0};

    WarInfoRepository& repository = defaultWarInfoRepository();

    vector<RaceWarPCListRow> entries = repository.loadRaceWarPCList();

    for (size_t r = 0; r < entries.size(); r++) {
        const string& Name = entries[r].name;
        // Column 1 of the SELECT, which is Name -- see WarInfoRepository.h.
        int Race = entries[r].race;

        file << "[" << Race << "] " << Name << endl;

        num[Race]++;
    }

    repository.deleteRaceWarPCList();

    file << "NumSlayer = " << num[0] << endl;
    file << "NumVampire = " << num[1] << endl;
    file << "NumOusters = " << num[2] << endl;

    file.close();
}

//--------------------------------------------------------------------------------
// add PCList
//--------------------------------------------------------------------------------
void RaceWarLimiter::addPCList(PlayerCreature* pPC)

{
    defaultWarInfoRepository().insertRaceWarPCListEntry(pPC->getName(), (int)pPC->getRace());
}

//--------------------------------------------------------------------------------
// isIn PCList
//--------------------------------------------------------------------------------
bool RaceWarLimiter::isInPCList(PlayerCreature* pPC)

{
    int count = defaultWarInfoRepository().countRaceWarPCListEntries(pPC->getName());

    bool bExist = count > 0;

    return bExist;
}

//--------------------------------------------------------------------------------
// remove PCList
//--------------------------------------------------------------------------------
void RaceWarLimiter::removePCList(PlayerCreature* pPC)

{
    defaultWarInfoRepository().deleteRaceWarPCListEntry(pPC->getName());
}
