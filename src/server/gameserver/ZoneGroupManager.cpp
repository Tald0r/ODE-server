//--------------------------------------------------------------------------------
//
// Filename    : ZoneGroupManager.cpp
// Written By  : Reiot
// Description :
//
//--------------------------------------------------------------------------------

// include files
#include "ZoneGroupManager.h"

#include <stdio.h>

#include <list>

#include <unordered_map>

#include "GameContext.h"
#include "GamePlayer.h"
#include "IncomingPlayerManager.h"
#include "LoginServerManager.h"
#include "PCManager.h"
#include "Portal.h"
#include "Tile.h"
#include "WarZoneWork.h"
#include "ZoneGroup.h"
#include "ZoneInfoManager.h"
#include "ZonePlayerManager.h"
#include "repository/ZoneInfoRepository.h"

//--------------------------------------------------------------------------------
// constructor
//--------------------------------------------------------------------------------
ZoneGroupManager::ZoneGroupManager()

    : m_ZoneGroups(10) {}


//--------------------------------------------------------------------------------
// destructor
//--------------------------------------------------------------------------------
ZoneGroupManager::~ZoneGroupManager()

{
    __BEGIN_TRY

    unordered_map<ZoneGroupID_t, ZoneGroup*>::iterator itr = m_ZoneGroups.begin();
    for (; itr != m_ZoneGroups.end(); itr++) {
        ZoneGroup* pZoneGroup = itr->second;
        SAFE_DELETE(pZoneGroup);
    }

    // Delete every pair held in the hash map.
    m_ZoneGroups.clear();

    __END_CATCH_NO_RETHROW
}


//--------------------------------------------------------------------------------
// initialize zone manager
//--------------------------------------------------------------------------------
void ZoneGroupManager::init()

{
    load();
}


//--------------------------------------------------------------------------------
//
// load data from database
//
// Connect to the database and load the ZoneGroups.
//
//--------------------------------------------------------------------------------
void ZoneGroupManager::load()

{
    __BEGIN_DEBUG

    list<ZoneGroupID_t> ZoneGroupIDList;

    // Read the zone group ids first.
    vector<int> zoneGroupIDs = defaultZoneInfoRepository().loadZoneGroupIDs(true);
    for (size_t g = 0; g < zoneGroupIDs.size(); g++) {
        ZoneGroupID_t ID = zoneGroupIDs[g];
        ZoneGroupIDList.push_back(ID);
    }


    list<ZoneGroupID_t>::iterator itr = ZoneGroupIDList.begin();
    for (; itr != ZoneGroupIDList.end(); itr++) {
        ZoneGroupID_t ID = (*itr);

        // Create the zone group for this id and add it to the manager.
        ZoneGroup* pZoneGroup = new ZoneGroup(ID);
        ZonePlayerManager* pZonePlayerManager = new ZonePlayerManager();
        pZonePlayerManager->setZGID(ID);
        pZoneGroup->setZonePlayerManager(pZonePlayerManager);
        addZoneGroup(pZoneGroup);

        // Read and initialize the zones that belong to this group.
        vector<int> zoneIDs = defaultZoneInfoRepository().loadZoneIDsOfGroup((int)ID, true);

        for (size_t z = 0; z < zoneIDs.size(); z++) {
            ZoneID_t zoneID = zoneIDs[z];

            // Create and initialize the zone object, then add it to the group.
            Zone* pZone = new Zone(zoneID);
            Assert(pZone != NULL);

            pZone->setZoneGroup(pZoneGroup);

            pZoneGroup->addZone(pZone);

            //--------------------------------------------------------------------------------
            // Mind the order: init() loads the NPCs, and the AtFirst-SetPosition
            // condition/action reaches the ZoneGroupManager while it runs, so the
            // zone must be added to the manager BEFORE it is initialized.
            //--------------------------------------------------------------------------------

            printf("\n@@@@@@@@@@@@@@@ [%d]th ZONE INITIALIZATION START @@@@@@@@@@@@@@@\n", zoneID);

            pZone->init();

            printf("\n@@@@@@@@@@@@@@@ [%d]th ZONE INITIALIZATION SUCCESS @@@@@@@@@@@@@@@\n", zoneID);
        }
    }

    ZoneGroupIDList.clear();

    __END_DEBUG
}


//--------------------------------------------------------------------------------
// save data to database
//--------------------------------------------------------------------------------
void ZoneGroupManager::save()

{
    throw UnsupportedError();
}


//--------------------------------------------------------------------------------
// add zone to zone manager
//--------------------------------------------------------------------------------
void ZoneGroupManager::addZoneGroup(ZoneGroup* pZoneGroup)

{
    unordered_map<ZoneGroupID_t, ZoneGroup*>::iterator itr = m_ZoneGroups.find(pZoneGroup->getZoneGroupID());

    if (itr != m_ZoneGroups.end())
        // The same id already exists.
        throw Error("duplicated zone id");

    // Store the zone group under its id.
    m_ZoneGroups[pZoneGroup->getZoneGroupID()] = pZoneGroup;
}

//--------------------------------------------------------------------------------
// get zone from zone manager
//--------------------------------------------------------------------------------
ZoneGroup* ZoneGroupManager::getZoneGroupByGroupID(ZoneGroupID_t ZoneGroupID) const {
    ZoneGroup* pZoneGroup = NULL;

    unordered_map<ZoneGroupID_t, ZoneGroup*>::const_iterator itr = m_ZoneGroups.find(ZoneGroupID);

    if (itr != m_ZoneGroups.end()) {
        pZoneGroup = itr->second;

    } else {
        // No such zone id could be found.
        StringStream msg;
        msg << "ZoneGroupID : " << ZoneGroupID;
        throw NoSuchElementException(msg.toString());
    }

    return pZoneGroup;
}


//--------------------------------------------------------------------------------
// Delete zone from zone manager
//--------------------------------------------------------------------------------
void ZoneGroupManager::deleteZoneGroup(ZoneGroupID_t zoneID) {
    unordered_map<ZoneGroupID_t, ZoneGroup*>::iterator itr = m_ZoneGroups.find(zoneID);

    if (itr != m_ZoneGroups.end()) {
        // Delete the zone.
        SAFE_DELETE(itr->second);

        // Erase the pair.
        m_ZoneGroups.erase(itr);
    } else {
        // No such zone id could be found.
        StringStream msg;
        msg << "ZoneGroupID : " << zoneID;
        throw NoSuchElementException(msg.toString());
    }
}


//--------------------------------------------------------------------------------
// get zone from zone manager
//--------------------------------------------------------------------------------
ZoneGroup* ZoneGroupManager::getZoneGroup(ZoneGroupID_t zoneID) const {
    ZoneGroup* pZoneGroup = NULL;

    unordered_map<ZoneGroupID_t, ZoneGroup*>::const_iterator itr = m_ZoneGroups.find(zoneID);

    if (itr != m_ZoneGroups.end()) {
        pZoneGroup = itr->second;

    } else {
        // No such zone id could be found.
        StringStream msg;
        msg << "ZoneGroupID : " << zoneID;
        throw NoSuchElementException(msg.toString());
    }

    return pZoneGroup;
}

void ZoneGroupManager::broadcast(Packet* pPacket)

{
    ZoneGroup* pZoneGroup = NULL;

    unordered_map<ZoneGroupID_t, ZoneGroup*>::const_iterator itr = m_ZoneGroups.begin();

    for (; itr != m_ZoneGroups.end(); itr++) {
        pZoneGroup = itr->second;

        pZoneGroup->getZonePlayerManager()->broadcastPacket(pPacket);
    }
}

void ZoneGroupManager::pushBroadcastPacket(Packet* pPacket, BroadcastFilter* pFilter)

{
    ZoneGroup* pZoneGroup = NULL;

    unordered_map<ZoneGroupID_t, ZoneGroup*>::const_iterator itr = m_ZoneGroups.begin();

    for (; itr != m_ZoneGroups.end(); itr++) {
        pZoneGroup = itr->second;

        pZoneGroup->getZonePlayerManager()->pushBroadcastPacket(pPacket, pFilter);
    }
}

void ZoneGroupManager::outputLoadValue()

{
    //------------------------------------------------------------------
    // ZoneGroup load
    //------------------------------------------------------------------
    ofstream file("loadBalance.txt", ios::app);

    VSDateTime current = VSDateTime::currentDateTime();
    file << current.toString() << endl;

    unordered_map<ZoneGroupID_t, ZoneGroup*>::const_iterator itr;

    for (itr = m_ZoneGroups.begin(); itr != m_ZoneGroups.end(); itr++) {
        ZoneGroup* pZoneGroup = itr->second;
        file << "[" << (int)pZoneGroup->getZoneGroupID() << "] ";

        const std::shared_ptr<const ZoneGroup::ZoneMap> zones = pZoneGroup->getZones();
        unordered_map<ZoneID_t, Zone*>::const_iterator iZone;

        // Compute the loadValue of each Zone.
        int totalLoad = 0;
        for (iZone = zones->begin(); iZone != zones->end(); iZone++) {
            Zone* pZone = iZone->second;

            int load = pZone->getLoadValue();
            int playerLoad = pZone->getPCCount();

            file << (int)pZone->getZoneID() << "(" << load << ", " << playerLoad << ") ";

            totalLoad += load;
        }

        file << " = " << totalLoad << endl;
    }

    file << endl;
    file.close();
}

//---------------------------------------------------------------------------
// make Balanced LoadInfo
//---------------------------------------------------------------------------
//
// bForce : balances the ZoneGroups by force even when balancing
//          is judged to be unnecessary.
//
// The load value is the number of loop iterations a Zone ran in 10 seconds.
// For convenience of computation the actual load is defined as follows.
//
//     load = (loadLimit - load)*loadMultiplier;
//
//---------------------------------------------------------------------------
bool ZoneGroupManager::makeBalancedLoadInfo(LOAD_INFOS& loadInfos, bool bForce)

{
    const int maxGroup = m_ZoneGroups.size(); // number of zone groups
    const int loadLimit = 500;                // load cap -- sleeping limits the loop count, so 500 is the maximum.
    const int stableLoad = 120;               // stable load -- at this level balancing is considered unnecessary
    // Balancing is only meaningful above a certain gap.
    const int minLoadGap =
        20; // load gap needed to balance -- balancing is meaningful only when max minus min exceeds it.
    const int averageLoadPercent = 90; // per-group load % cap; 100 would do, but 90 works out better.

    int i;

    GROUPS groups;

    unordered_map<ZoneGroupID_t, ZoneGroup*>::const_iterator itr;

    // Total load
    int totalLoad = 0;


    //------------------------------------------------------------------
    // Survey the loadValue of each ZoneGroup
    //------------------------------------------------------------------
    int maxLoadValue = 0;
    int minLoadValue = loadLimit;
    for (itr = m_ZoneGroups.begin(); itr != m_ZoneGroups.end(); itr++) {
        ZoneGroup* pZoneGroup = itr->second;

        const std::shared_ptr<const ZoneGroup::ZoneMap> zones = pZoneGroup->getZones();
        unordered_map<ZoneID_t, Zone*>::const_iterator iZone;

        // Compute the loadValue of each Zone.
        for (iZone = zones->begin(); iZone != zones->end(); iZone++) {
            Zone* pZone = iZone->second;

            int load = pZone->getLoadValue();
            load = min(load, loadLimit);

            // 10~500
            maxLoadValue = max(maxLoadValue, load);
            minLoadValue = min(minLoadValue, load);

            // A smaller number means a slower zone.
            // The number is inverted for convenience --> a larger number now means a larger load.
            // The player count is used as the load weight.
            // playerLoad is roughly 1 to 20.
            int playerLoad = pZone->getPCCount() / 10;
            playerLoad = max(1, playerLoad);
            load = (loadLimit - load) * playerLoad; // load weighting

            LoadInfo* pInfo = new LoadInfo;
            pInfo->id = pZone->getZoneID();
            pInfo->oldGroupID = itr->first;
            pInfo->groupID = -1;
            pInfo->load = load;

            // Key made up of the load and the zone id
            DWORD key = (load << 8) | pInfo->id;

            loadInfos[key] = pInfo;

            totalLoad += load;
        }
    }

    //------------------------------------------------------------------
    //
    // Check whether balancing is needed
    //
    //------------------------------------------------------------------
    if (!bForce) {
        int loadBoundary = stableLoad;

        // If the load is below the load boundary, or the gap between
        // the minimum and maximum load is at most the threshold,
        // there is no need to load balance.
        if (minLoadValue >= loadBoundary || maxLoadValue - minLoadValue <= minLoadGap) {
            // The load has to be surveyed again.
            for (itr = m_ZoneGroups.begin(); itr != m_ZoneGroups.end(); itr++) {
                ZoneGroup* pZoneGroup = itr->second;

                // Reset the loadValue.
                const std::shared_ptr<const ZoneGroup::ZoneMap> zones = pZoneGroup->getZones();
                unordered_map<ZoneID_t, Zone*>::const_iterator iZone;

                // Compute the loadValue of each Zone.
                for (iZone = zones->begin(); iZone != zones->end(); iZone++) {
                    Zone* pZone = iZone->second;

                    pZone->initLoadValue();
                }
            }

            return false;
        }
    }

    // Average load,
    // with the average taken as 90%.
    int avgLoad = totalLoad * averageLoadPercent / maxGroup / 100;

    // Prepare to compute the load of the new groups.
    groups.reserve(maxGroup);
    for (i = 0; i < maxGroup; i++) {
        groups[i] = 0;
    }

    // Print the state before balancing.

    //------------------------------------------------------------------
    //
    // load balancing
    //
    // Uses a slightly modified FirstFit.
    //------------------------------------------------------------------
    LOAD_INFOS::const_iterator iInfo = loadInfos.begin();

    int index = 0;

    for (; iInfo != loadInfos.end(); iInfo++) {
        LoadInfo* pInfo = iInfo->second;

        // Find the new group to go into.
        int newGroupID = -1;
        for (int k = 0; k < maxGroup; k++) {
            int groupLoad = groups[index];

            if (groupLoad + pInfo->load <= avgLoad) {
                newGroupID = index;

                if (++index >= maxGroup)
                    index = 0;

                break;
            }

            if (++index >= maxGroup)
                index = 0;
        }

        // If no suitable group was found, use the group with the smallest load.
        if (newGroupID == -1) {
            newGroupID = 0;
            for (int k = 1; k < maxGroup; k++) {
                if (groups[k] < groups[newGroupID]) {
                    newGroupID = k;
                }
            }
        }

        // Add the Info to newGroupID.
        pInfo->groupID = newGroupID + 1; // group ids are 1-based, so add one.
        groups[newGroupID] += pInfo->load;
    }

    return true;
}

//---------------------------------------------------------------------------
// make DefaultLoadInfo
//---------------------------------------------------------------------------
// Set the ZoneGroups to the defaults configured in the DB.
//---------------------------------------------------------------------------
bool ZoneGroupManager::makeDefaultLoadInfo(LOAD_INFOS& loadInfos)

{
    {
        __BEGIN_DEBUG

        list<ZoneGroupID_t> ZoneGroupIDList;

        // Read the zone group ids first.
        vector<int> zoneGroupIDs = defaultZoneInfoRepository().loadZoneGroupIDs(false);
        for (size_t g = 0; g < zoneGroupIDs.size(); g++) {
            ZoneGroupID_t ID = zoneGroupIDs[g];
            ZoneGroupIDList.push_back(ID);
        }


        list<ZoneGroupID_t>::iterator itr = ZoneGroupIDList.begin();
        for (; itr != ZoneGroupIDList.end(); itr++) {
            ZoneGroupID_t ID = *itr;

            vector<int> zoneIDs = defaultZoneInfoRepository().loadZoneIDsOfGroup(ID, false);

            for (size_t z = 0; z < zoneIDs.size(); z++) {
                ZoneID_t zoneID = zoneIDs[z];

                LoadInfo* pInfo = new LoadInfo;
                pInfo->id = zoneID;

                try {
                    pInfo->oldGroupID = de::gameContext().zoneInfos().getZoneInfo(zoneID)->getZoneGroupID();
                } catch (NoSuchElementException&) {
                    filelog("makeDefaultLoadInfoError.txt", "NoSuch ZoneInfo : %d", zoneID);
                    pInfo->oldGroupID = ID; // just let it through
                }

                pInfo->groupID = ID;
                pInfo->load = 0; // meaningless here

                loadInfos[zoneID] = pInfo;
            }
        }

        ZoneGroupIDList.clear();

        __END_DEBUG
    }

    return true;
}

//---------------------------------------------------------------------------
// balance ZoneGroup ( bForce )
//---------------------------------------------------------------------------
//
// bForce : balances the ZoneGroups by force even when balancing
//          is judged to be unnecessary.
//
// bDefault : sets the ZoneGroups to the values specified in the DB.
//
// The load value is the number of loop iterations a Zone ran in 10 seconds.
// For convenience of computation the actual load is defined as follows.
//
//     load = (loadLimit - load)*loadMultiplier;
//
//---------------------------------------------------------------------------
void ZoneGroupManager::balanceZoneGroup(bool bForce, bool bDefault)

{
    filelog("balanceZoneGroup.txt", "Zone group balancing is disabled.");
    return;
}

//--------------------------------------------------------------------------------
// lock all ZoneGroup and LoginServerManager
//--------------------------------------------------------------------------------
void ZoneGroupManager::lockZoneGroups()

{
    //------------------------------------------------------------------
    // LoginServerManager UNLOCK
    //------------------------------------------------------------------
    de::gameContext().loginServer().lock();

    //------------------------------------------------------------------
    //
    // 					LOCK all ZoneGroups
    //
    //------------------------------------------------------------------
    unordered_map<ZoneGroupID_t, ZoneGroup*>::const_iterator itr;

    for (itr = m_ZoneGroups.begin(); itr != m_ZoneGroups.end(); itr++) {
        ZoneGroup* pZoneGroup = itr->second;
        pZoneGroup->lock();
    }
}

//--------------------------------------------------------------------------------
// lock all ZoneGroup and LoginServerManager
//--------------------------------------------------------------------------------
void ZoneGroupManager::unlockZoneGroups()

{
    //------------------------------------------------------------------
    //
    // 					UNLOCK all ZoneGroups
    //
    //------------------------------------------------------------------
    unordered_map<ZoneGroupID_t, ZoneGroup*>::const_iterator itr;

    for (itr = m_ZoneGroups.begin(); itr != m_ZoneGroups.end(); itr++) {
        ZoneGroup* pZoneGroup = itr->second;
        pZoneGroup->unlock();
    }

    //------------------------------------------------------------------
    // LoginServerManager UNLOCK
    //------------------------------------------------------------------
    de::gameContext().loginServer().unlock();
}

//--------------------------------------------------------------------------------
// get PlayerNum. by sigi. 2002.12.30
//--------------------------------------------------------------------------------
int ZoneGroupManager::getPlayerNum() const

{
    int numPC = 0;

    unordered_map<ZoneGroupID_t, ZoneGroup*>::const_iterator itr = m_ZoneGroups.begin();

    for (; itr != m_ZoneGroups.end(); itr++) {
        ZoneGroup* pZoneGroup = itr->second;

        // No lock is needed.
        numPC += pZoneGroup->getZonePlayerManager()->size();
    }

    return numPC;
}

void ZoneGroupManager::removeFlag(Effect::EffectClass EC) {
    de::war::postToEveryZoneGroup([EC](ZoneGroup& zoneGroup) { zoneGroup.getZonePlayerManager()->removeFlag(EC); });
}

//--------------------------------------------------------------------------------
// get debug string
//--------------------------------------------------------------------------------
string ZoneGroupManager::toString() const

{
    StringStream msg;

    msg << "ZoneGroupManager(";

    for (unordered_map<ZoneGroupID_t, ZoneGroup*>::const_iterator itr = m_ZoneGroups.begin(); itr != m_ZoneGroups.end();
         itr++) {
        msg << itr->second->toString();
    }

    msg << ")";

    return msg.toString();
}
