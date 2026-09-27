#include "FlagManager.h"

#include <stdio.h>

#include <cstdlib>

#include "EffectFlagInsert.h"
#include "EffectManager.h"
#include "FlagWar.h"
#include "GCAddEffect.h"
#include "GCDeleteInventoryItem.h"
#include "GCFlagWarStatus.h"
#include "MonsterCorpse.h"
#include "NPCInfo.h"
#include "NewbieFlagWar.h"
#include "Player.h"
#include "PlayerCreature.h"
#include "Properties.h"
#include "SystemAvailabilitiesManager.h"
#include "Zone.h"
#include "ZoneUtil.h"
#include "repository/FlagWarRepository.h"
#include "war/WarZoneWork.h"

FlagManager::FlagManager(de::GameContext& context) : m_Context(context) {
    m_Mutex.setName("FlagManager");
    m_FlagCount.clear();

    m_StatusPacket.setTimeRemain(0);
    m_StatusPacket.setFlagCount(RACE_SLAYER, 0);
    m_StatusPacket.setFlagCount(RACE_VAMPIRE, 0);
    m_StatusPacket.setFlagCount(RACE_OUSTERS, 0);

    m_PutTime[RACE_SLAYER] = m_PutTime[RACE_VAMPIRE] = m_PutTime[RACE_OUSTERS] = VSDateTime::currentDateTime();

    FlagWar* pFlagWar = new FlagWar(*this, m_Context);
    addSchedule(new Schedule(pFlagWar, pFlagWar->getNextFlagWarTime()));

    pFlagWar = new NewbieFlagWar(*this, m_Context);
    addSchedule(new Schedule(pFlagWar, pFlagWar->getNextFlagWarTime()));
}

FlagManager::~FlagManager() {}

void FlagManager::init() {
    SYSTEM_RETURN_IF_NOT(SYSTEM_FLAG_WAR);
    vector<FlagPoleRow> poles = defaultFlagWarRepository().loadFlagPoles();

    for (size_t r = 0; r < poles.size(); r++) {
        ZoneID_t zoneID = (ZoneID_t)poles[r].zoneID;
        Zone* pZone = getZoneByZoneID(zoneID);

        ZoneCoord_t left = (ZoneCoord_t)poles[r].centerX;
        ZoneCoord_t top = (ZoneCoord_t)poles[r].centerY;
        uint width = (ZoneCoord_t)poles[r].width;
        uint height = (ZoneCoord_t)poles[r].height;
        Race_t race = (Race_t)poles[r].race;
        MonsterType_t type = (MonsterType_t)poles[r].monsterType;

        addPoleField(pZone, left, top, width, height, race, type);
    }
}

void FlagManager::addPoleField(Zone* pZone, ZoneCoord_t left, ZoneCoord_t top, uint width, uint height, Race_t race,
                               MonsterType_t type) {
    Assert(pZone != NULL);
    Assert(isValidZoneCoord(pZone, left, top));
    Assert(isValidZoneCoord(pZone, left + width, top + height));

    NPCInfo* pNPCInfo = new NPCInfo();
    pNPCInfo->setName("Flag Pole");
    pNPCInfo->setNPCID(type);
    pNPCInfo->setX(left);
    pNPCInfo->setY(top);

    pZone->addNPCInfo(pNPCInfo);

    for (uint i = 0; i < width; ++i)
        for (uint j = 0; j < height; ++j) {
            MonsterCorpse* pFlagPole = new MonsterCorpse(type, "Flag Pole", 2);
            Assert(pFlagPole != NULL);

            pFlagPole->setZone(pZone);
            pFlagPole->setShrine(true);
            pZone->registerObject(pFlagPole);

            m_FlagPoles[pFlagPole] = race;

            TPOINT tp = pZone->addItem(pFlagPole, left + (i * 2), top + (j * 2));
            Assert(tp.x != -1);

            forbidDarkness(pZone, tp.x, tp.y, 1);
        }

    m_PoleFields.push_back(PoleFieldInfo(pZone->getZoneID(), left, top, width * 2, height * 2));
}

void FlagManager::manualStart() {
    if (!isEmpty()) {
        cout << "Pulling the schedule forward.." << endl;
        addSchedule(new Schedule(popRecentWork(), VSDateTime::currentDateTime()));
    } else {
        cout << "Creating a schedule.." << endl;
        addSchedule(new Schedule(new FlagWar(*this, m_Context), VSDateTime::currentDateTime()));
    }
}

bool FlagManager::startFlagWar() {
    if (m_bHasFlagWar)
        return false;
    m_bHasFlagWar = true;

    Work* pWork = m_RecentSchedules.top()->getWork();
    FlagWar* pFlagWar = dynamic_cast<FlagWar*>(pWork);

    VSDateTime endTime = VSDateTime::currentDateTime().addSecs((pFlagWar != NULL) ? pFlagWar->getWarTime() : 3600);

    resetFlagCounts();
    for (ZoneID_t zoneID : de::ctf::poleZonesOf(getPoleFields()))
        postPoleSweep(zoneID);

    __ENTER_CRITICAL_SECTION(m_Mutex)

    m_EndTime = endTime;
    m_StatusPacket.setTimeRemain(remainWarTimeSecs());
    m_StatusPacket.setFlagCount(RACE_SLAYER, 0);
    m_StatusPacket.setFlagCount(RACE_VAMPIRE, 0);
    m_StatusPacket.setFlagCount(RACE_OUSTERS, 0);

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    broadcastStatus();

    return true;
}

bool FlagManager::endFlagWar() {
    if (m_bHasFlagWar) {
        recordFlagWarHistory();

        // The winner and the counts are read together: zone threads may still
        // plant or pull until the war flag drops below.
        Race_t winner = (Race_t)SLAYER;
        uint slayers = 0, vampires = 0, ousters = 0;

        __ENTER_CRITICAL_SECTION(m_Mutex)

        winner = (Race_t)winnerRace_LOCKED();
        auto countOf = [this](RACEINDEX race) {
            auto found = m_FlagCount.find(race);
            return (found != m_FlagCount.end()) ? found->second : 0u;
        };
        slayers = countOf(SLAYER);
        vampires = countOf(VAMPIRE);
        ousters = countOf(OUSTERS);

        __LEAVE_CRITICAL_SECTION(m_Mutex)

        // running a script -- who would have thought the system function would be used
        char cmd[100];
        snprintf(cmd, sizeof(cmd), "/home/darkeden/vs/bin/script/recordFlagWarHistory.py %s %d %d %d %d %d %d %d ",
                 m_EndTime.toStringforWeb().c_str(), (int)winner, m_Context.config().getPropertyInt("Dimension"),
                 m_Context.config().getPropertyInt("WorldID"), m_Context.config().getPropertyInt("ServerID"), slayers,
                 vampires, ousters);

        filelog("script.log", cmd);
        system(cmd);

        m_bHasFlagWar = false;
        return true;
    }

    return false;
}

bool FlagManager::putFlag(PlayerCreature* pPC, MonsterCorpse* pFlagPole) {
    if (!isFlagPole(pFlagPole))
        return false;
    if (pPC->getRace() != m_FlagPoles[pFlagPole])
        return false;
    if (!pPC->isFlag(Effect::EFFECT_CLASS_HAS_FLAG))
        return false;

    lock();
    m_FlagCount[(RACEINDEX)(pPC->getRace())]++;
    m_StatusPacket.setFlagCount(pPC->getRace(), m_FlagCount[(RACEINDEX)(pPC->getRace())]);
    m_PutTime[pPC->getRace()] = VSDateTime::currentDateTime();
    filelog("FlagWar.log", "%s planted the flag on the pole. S : %d, V : %d, O : %d", pPC->getName().c_str(),
            m_FlagCount[SLAYER], m_FlagCount[VAMPIRE], m_FlagCount[OUSTERS]);
    unlock();

    broadcastStatus();

    return true;
}

bool FlagManager::getFlag(PlayerCreature* pPC, MonsterCorpse* pFlagPole) {
    if (!isFlagPole(pFlagPole))
        return false;
    if (pPC->getRace() == m_FlagPoles[pFlagPole])
        return false;
    if (pPC->isFlag(Effect::EFFECT_CLASS_HAS_FLAG))
        return false;

    // Poles in different groups are pulled from on different threads, so
    // the count is checked under the lock it is taken under.
    lock();
    if (m_FlagCount[(RACEINDEX)(m_FlagPoles[pFlagPole])] == 0) {
        unlock();
        return false;
    }
    m_FlagCount[(RACEINDEX)(m_FlagPoles[pFlagPole])]--;
    m_StatusPacket.setFlagCount(m_FlagPoles[pFlagPole], m_FlagCount[(RACEINDEX)(m_FlagPoles[pFlagPole])]);
    filelog("FlagWar.log", "%s pulled out the flag. S : %d, V : %d, O : %d", pPC->getName().c_str(),
            m_FlagCount[SLAYER], m_FlagCount[VAMPIRE], m_FlagCount[OUSTERS]);
    unlock();

    broadcastStatus();

    return true;
}

bool FlagManager::putFlag(PlayerCreature* pPC, Item* pItem, MonsterCorpse* pFlagPole) {
    Assert(pItem->getObjectID() == pPC->getExtraInventorySlotItem()->getObjectID());

    if (pPC->getRace() != getFlagPoleRace(pFlagPole))
        return false;
    if (pFlagPole->getTreasureCount() != 0)
        return false;
    if (!putFlag(pPC, pFlagPole))
        return false;

    pPC->deleteItemFromExtraInventorySlot();
    GCDeleteInventoryItem gcDeleteInventoryItem;
    gcDeleteInventoryItem.setObjectID(pItem->getObjectID());

    pPC->getPlayer()->sendPacket(&gcDeleteInventoryItem);

    Effect* pEffect = pPC->findEffect(Effect::EFFECT_CLASS_HAS_FLAG);
    if (pEffect != NULL) {
        pEffect->setDeadline(0);
    }

    pFlagPole->addTreasure(pItem);

    pFlagPole->setFlag(Effect::EFFECT_CLASS_FLAG_INSERT);
    EffectFlagInsert* pFlagEffect = new EffectFlagInsert(pFlagPole);
    pFlagPole->getEffectManager().addEffect(pFlagEffect);

    GCAddEffect gcAddEffect;
    gcAddEffect.setEffectID(Effect::EFFECT_CLASS_FLAG_INSERT);
    gcAddEffect.setObjectID(pFlagPole->getObjectID());
    gcAddEffect.setDuration(65535);

    recordPutFlag(pPC, pItem);

    pFlagPole->getZone()->broadcastPacket(pFlagPole->getX(), pFlagPole->getY(), &gcAddEffect);

    return true;
}

uint FlagManager::getFlagCount(Race_t race) const {
    uint count = 0;

    __ENTER_CRITICAL_SECTION(m_Mutex)

    auto itr = m_FlagCount.find((RACEINDEX)race);
    if (itr != m_FlagCount.end())
        count = itr->second;

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    return count;
}

FlagManager::RACEINDEX FlagManager::winnerRace_LOCKED() const {
    uint max = 0;
    RACEINDEX maxRace = SLAYER;

    auto putTime = [this](RACEINDEX race) {
        auto found = m_PutTime.find((Race_t)race);
        return (found != m_PutTime.end()) ? found->second : VSDateTime();
    };

    map<RACEINDEX, uint>::const_iterator itr = m_FlagCount.begin();
    map<RACEINDEX, uint>::const_iterator endItr = m_FlagCount.end();

    for (; itr != endItr; ++itr) {
        if (itr->second > max) {
            maxRace = itr->first;
            max = itr->second;
        }
        if (itr->second == max) {
            if (putTime(itr->first) > putTime(maxRace)) {
                maxRace = itr->first;
                max = itr->second;
            }
        }
    }

    return maxRace;
}

Race_t FlagManager::getWinnerRace() const {
    RACEINDEX race = SLAYER;

    __ENTER_CRITICAL_SECTION(m_Mutex)

    race = winnerRace_LOCKED();

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    return (Race_t)race;
}

Race_t FlagManager::getWinnerRace(uint& count) const {
    RACEINDEX race = SLAYER;
    count = 0;

    __ENTER_CRITICAL_SECTION(m_Mutex)

    race = winnerRace_LOCKED();
    auto itr = m_FlagCount.find(race);
    if (itr != m_FlagCount.end())
        count = itr->second;

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    return (Race_t)race;
}


void FlagManager::resetFlagCounts() {
    __ENTER_CRITICAL_SECTION(m_Mutex)

    m_FlagCount[SLAYER] = 0;
    m_FlagCount[VAMPIRE] = 0;
    m_FlagCount[OUSTERS] = 0;

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    deleteFlagWarStats();
}

// Clears out the FlagWarStat table, the round's per-player planting record.
void FlagManager::deleteFlagWarStats() {
    defaultFlagWarRepository().deleteAllFlagWarStats();
}

std::vector<de::ctf::PoleField> FlagManager::getPoleFields() const {
    std::vector<de::ctf::PoleField> fields;
    for (const PoleFieldInfo& info : m_PoleFields) {
        de::ctf::PoleField field;
        field.zoneID = info.zoneID;
        field.left = info.l;
        field.top = info.t;
        field.width = info.w;
        field.height = info.h;
        fields.push_back(field);
    }
    return fields;
}

// A flag the returns missed may still stand on a pole. Flags lying in the
// field are the returns' business; a flag planted on a pole is taken out
// of the pole here, with no reward, on the pole zone's own thread.
void FlagManager::postPoleSweep(ZoneID_t zoneID) const {
    std::vector<de::ctf::PoleField> fields;
    for (const de::ctf::PoleField& field : getPoleFields()) {
        if (field.zoneID == zoneID)
            fields.push_back(field);
    }

    de::war::postToZone(zoneID, [fields](Zone& zone) {
        for (const de::ctf::PoleField& field : fields) {
            for (const auto& tile : de::ctf::poleSweepTiles(field)) {
                ZoneCoord_t ix = tile.first;
                ZoneCoord_t iy = tile.second;

                if (!isValidZoneCoord(&zone, ix, iy))
                    continue;
                Item* pCorpse = zone.getTile(ix, iy).getItem();
                if (pCorpse == NULL || pCorpse->getItemClass() != Item::ITEM_CLASS_CORPSE ||
                    pCorpse->getItemType() != MONSTER_CORPSE) {
                    continue;
                }

                MonsterCorpse* pFlagPole = dynamic_cast<MonsterCorpse*>(pCorpse);
                if (pFlagPole == NULL || !de::gameContext().flags().isFlagPole(pFlagPole))
                    continue;

                // getTreasure() takes the pole's treasure out and turns its
                // flag effect off; the pole itself stays on its tile.
                Item* pItem = pFlagPole->getTreasure();
                if (pItem == NULL)
                    continue;

                if (!pItem->isFlagItem()) {
                    pFlagPole->addTreasure(pItem);
                    continue;
                }

                pItem->destroy();
                SAFE_DELETE(pItem);
            }
        }
    });
}

bool FlagManager::isInPoleField(ZONE_COORD zc) {
    list<PoleFieldInfo>::iterator itr = m_PoleFields.begin();
    list<PoleFieldInfo>::iterator endItr = m_PoleFields.end();

    for (; itr != endItr; ++itr) {
        if (itr->isInField(zc))
            return true;
    }

    return false;
}

void FlagManager::recordPutFlag(PlayerCreature* pPC, Item* pItem)

{
    FlagWarRepository& flagWars = defaultFlagWarRepository();

    // Ignore it if it is there, INSERT if it is not
    if (!flagWars.flagStatExists(pPC->getName(), pItem->getItemID())) {
        flagWars.insertFlagStat(pPC->getPlayer()->getID(), pPC->getName(), (int)pPC->getRace(),
                                m_Context.config().getPropertyInt("ServerID"), pItem->getItemID());
    }
}

void FlagManager::recordFlagWarHistory()

{
    FlagWarRepository& flagWars = defaultFlagWarRepository();

    // The tally is read in full before the first history row is written.
    // The inline version interleaved them, but on a SECOND Statement --
    // which is what kept its result alive. The stronger guarantee is
    // that the driver buffers the whole result set client-side
    // (mysql_store_result) before the first next(), so the INSERTs could
    // not perturb it whatever table they hit.
    vector<FlagWarStatTotalRow> totals = flagWars.loadFlagWarStatTotals();

    for (size_t r = 0; r < totals.size(); r++) {
        string playerID = totals[r].playerID;
        string name = totals[r].name;
        Race_t race = totals[r].race;
        int serverID = totals[r].serverID;
        int num = totals[r].flagNum;

        flagWars.insertFlagWarHistory(m_EndTime.toStringforWeb(), playerID, name, (int)race, serverID, num);
    }
}

void FlagManager::setAllowedZones(std::map<ZoneID_t, uint> allowed) {
    m_FlagAllowMap.update([&allowed](std::map<ZoneID_t, uint>& current) { current.swap(allowed); });
}

GCFlagWarStatus FlagManager::statusPacket() const {
    GCFlagWarStatus status;

    __ENTER_CRITICAL_SECTION(m_Mutex)

    status = m_StatusPacket;
    status.setTimeRemain(remainWarTimeSecs());

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    return status;
}

// Called by the main thread as the war starts and by the zone threads that
// plant and pull flags; either way the allowed zones may belong to other
// groups, so each zone's players are walked on its own group's thread.
void FlagManager::broadcastStatus() const {
    if (!hasFlagWar())
        return;

    GCFlagWarStatus status = statusPacket();

    std::vector<ZoneID_t> zoneIDs;
    for (const auto& allowed : *m_FlagAllowMap.load())
        zoneIDs.push_back(allowed.first);

    de::war::postToZones(zoneIDs, [status](Zone& zone) {
        GCFlagWarStatus packet = status;
        zone.broadcastPacket(&packet);
    });
}
