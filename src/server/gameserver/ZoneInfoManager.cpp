//////////////////////////////////////////////////////////////////////////////
// Filename    : ZoneInfoManager.cpp
// Written By  : reiot
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "ZoneInfoManager.h"

#include "SystemAvailabilitiesManager.h"
#include "ZoneUtil.h"
#include "repository/ZoneInfoRepository.h"

//////////////////////////////////////////////////////////////////////////////
// constructor
//////////////////////////////////////////////////////////////////////////////
ZoneInfoManager::ZoneInfoManager()

{}


//////////////////////////////////////////////////////////////////////////////
// destructor
//////////////////////////////////////////////////////////////////////////////
ZoneInfoManager::~ZoneInfoManager()

{
    // Teardown: free the ZoneInfo objects through the published tables; the
    // Snapshot member releases the tables themselves with this object.
    const std::shared_ptr<const Tables> tables = m_Tables.load();
    for (unordered_map<ZoneID_t, ZoneInfo*>::const_iterator itr = tables->byID.begin(); itr != tables->byID.end();
         itr++) {
        ZoneInfo* pInfo = itr->second;
        SAFE_DELETE(pInfo);
    }
}


//////////////////////////////////////////////////////////////////////////////
// initialize zone info manager
//////////////////////////////////////////////////////////////////////////////
void ZoneInfoManager::init()

{
    // init == load
    load();
}


//////////////////////////////////////////////////////////////////////////////
// load from database
//////////////////////////////////////////////////////////////////////////////
void ZoneInfoManager::load()

{
    const std::shared_ptr<const Tables> loaded = m_Tables.load();
    bool bReload = !loaded->byID.empty();

    vector<ZoneInfoRow> rows = defaultZoneInfoRepository().loadZoneInfos();

    for (size_t r = 0; r < rows.size(); r++) {
        const ZoneInfoRow& row = rows[r];

        ZoneID_t zoneID = row.zoneID;


        ZoneInfo* pZoneInfo = NULL;
        bool bExistInfo = false;

        if (bReload) {
            unordered_map<ZoneID_t, ZoneInfo*>::const_iterator itr = loaded->byID.find(zoneID);

            if (itr != loaded->byID.end()) {
                pZoneInfo = itr->second;
                bExistInfo = true;
            } else {
                pZoneInfo = new ZoneInfo();
            }
        } else {
            pZoneInfo = new ZoneInfo();
        }

        {
            pZoneInfo->setZoneID(zoneID);
            pZoneInfo->setZoneGroupID(row.zoneGroupID);
            pZoneInfo->setZoneType(row.type);
            pZoneInfo->setZoneLevel(row.level);
            pZoneInfo->setZoneAccessMode(row.accessMode);
            pZoneInfo->setZoneOwnerID(row.ownerID);
            pZoneInfo->setPayPlay(row.payPlayZone != 0);
            pZoneInfo->setPremiumZone(row.premiumZone != 0);
            pZoneInfo->setPKZone(row.pkZone != 0);
            pZoneInfo->setNoPortalZone(row.noPortalZone != 0);
            pZoneInfo->setHolyLand(row.holyLand != 0);
            pZoneInfo->setAvailable(row.available != 0);
            pZoneInfo->setOpenLevel(row.openLevel);
            pZoneInfo->setSMPFilename(row.smpFilename);
            pZoneInfo->setSSIFilename(row.ssiFilename);
            pZoneInfo->setFullName(row.fullName);
            pZoneInfo->setShortName(row.shortName);

            pZoneInfo->setAvailable(pZoneInfo->isAvailable() &&
                                    pZoneInfo->getOpenLevel() <
                                        SystemAvailabilitiesManager::getInstance()->getZoneOpenDegree());

            if (!bExistInfo) {
                addZoneInfo(pZoneInfo);
            }
        }
    }
}


//////////////////////////////////////////////////////////////////////////////
// add zone info to zone info manager
//////////////////////////////////////////////////////////////////////////////
void ZoneInfoManager::addZoneInfo(ZoneInfo* pZoneInfo)

{
    // One copy-on-write publish covers all three maps, so a reader never
    // sees the id in one table and not yet in another; a duplicate throws
    // before anything is published.
    m_Tables.update([pZoneInfo](Tables& tables) {
        // First check whether a zone with the same id exists.
        if (tables.byID.find(pZoneInfo->getZoneID()) != tables.byID.end())
            // A zone with the same id already exists.
            throw Error("duplicated zone id");

        // Put the zone ID into the zone full name map.
        // This exists for the operator commands.
        if (tables.byFullName.find(pZoneInfo->getFullName()) != tables.byFullName.end()) {
            cerr << "Duplicated Zone Full Name:" << pZoneInfo->getFullName() << endl;
            throw Error("Duplicated Zone Full Name");
        }

        // Put the zone ID into the zone short name map.
        // This exists for the operator commands.
        if (tables.byShortName.find(pZoneInfo->getShortName()) != tables.byShortName.end()) {
            cerr << "Duplicated Zone Short Name" << endl;
            throw Error("Duplicated Zone Short Name");
        }

        tables.byID[pZoneInfo->getZoneID()] = pZoneInfo;
        tables.byFullName[pZoneInfo->getFullName()] = pZoneInfo;
        tables.byShortName[pZoneInfo->getShortName()] = pZoneInfo;
    });
}


//////////////////////////////////////////////////////////////////////////////
// Delete zone info from zone info manager
//////////////////////////////////////////////////////////////////////////////
void ZoneInfoManager::deleteZoneInfo(ZoneID_t zoneID) {
    // Unpublishing first only keeps NEW readers from finding the entry; one
    // that loaded the old snapshot still holds the raw ZoneInfo* and is not
    // protected by the delete coming second. Safe only while no such reader
    // can exist -- this function has no caller.
    ZoneInfo* pZoneInfo = m_Tables.update([zoneID](Tables& tables) -> ZoneInfo* {
        unordered_map<ZoneID_t, ZoneInfo*>::iterator itr = tables.byID.find(zoneID);
        if (itr == tables.byID.end())
            return NULL;
        ZoneInfo* pFound = itr->second;
        tables.byID.erase(itr);
        tables.byFullName.erase(pFound->getFullName());
        tables.byShortName.erase(pFound->getShortName());
        return pFound;
    });

    if (pZoneInfo != NULL) {
        // Delete the zone.
        SAFE_DELETE(pZoneInfo);
    } else {
        // The zone id could not be found.
        StringStream msg;
        msg << "ZoneID : " << zoneID;
        throw NoSuchElementException(msg.toString());
    }
}


//////////////////////////////////////////////////////////////////////////////
// get zone from zone info manager
//////////////////////////////////////////////////////////////////////////////
ZoneInfo* ZoneInfoManager::getZoneInfo(ZoneID_t zoneID) {
    ZoneInfo* pZoneInfo = NULL;

    const std::shared_ptr<const Tables> tables = m_Tables.load();
    unordered_map<ZoneID_t, ZoneInfo*>::const_iterator itr = tables->byID.find(zoneID);

    if (itr != tables->byID.end()) {
        pZoneInfo = itr->second;

    } else {
        // The zone id could not be found.
        StringStream msg;
        msg << "ZoneID : " << zoneID;
        throw NoSuchElementException(msg.toString());
    }

    return pZoneInfo;
}

//////////////////////////////////////////////////////////////////////////////
// get zone from zone info manager
//////////////////////////////////////////////////////////////////////////////
ZoneInfo* ZoneInfoManager::getZoneInfoByName(const string& ZoneName) {
    const std::shared_ptr<const Tables> tables = m_Tables.load();

    // Search the short name map first.
    unordered_map<string, ZoneInfo*>::const_iterator short_itr = tables->byShortName.find(ZoneName);
    if (short_itr != tables->byShortName.end()) {
        return short_itr->second;
    }

    // If it is not there, search the full name map.
    unordered_map<string, ZoneInfo*>::const_iterator full_itr = tables->byFullName.find(ZoneName);
    if (full_itr != tables->byFullName.end()) {
        return full_itr->second;
    }

    // If it was nowhere, just return NULL.
    return NULL;
}

vector<Zone*> ZoneInfoManager::getNormalFields() const {
    vector<Zone*> ret;


    ret.push_back(getZoneByZoneID(13));

    return ret;
}


//////////////////////////////////////////////////////////////////////////////
// get debug string
//////////////////////////////////////////////////////////////////////////////
string ZoneInfoManager::toString() const

{
    StringStream msg;

    msg << "ZoneInfoManager(";

    const std::shared_ptr<const Tables> tables = m_Tables.load();
    if (tables->byID.empty())
        msg << "EMPTY";
    else {
        for (unordered_map<ZoneID_t, ZoneInfo*>::const_iterator itr = tables->byID.begin(); itr != tables->byID.end();
             itr++) {
            msg << itr->second->toString();
        }
    }

    msg << ")";

    return msg.toString();
}
