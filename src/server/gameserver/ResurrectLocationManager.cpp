//////////////////////////////////////////////////////////////////////////////
// Filename    : ResurrectLocationManager.cpp
// Written by  : excel96
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "ResurrectLocationManager.h"

#include "CastleInfoManager.h"
#include "Effect.h"
#include "GameContext.h"
#include "GamePlayer.h"
#include "PacketUtil.h"
#include "Slayer.h"
#include "Vampire.h"
#include "VariableManager.h"
#include "WarSystem.h"
#include "Zone.h"
#include "ZoneInfo.h"
#include "ZoneInfoManager.h"
#include "repository/ZoneInfoRepository.h"

//////////////////////////////////////////////////////////////////////////////
// ZONEID const Variable
//////////////////////////////////////////////////////////////////////////////
const ZoneID_t SLAYER_NOVICE_ZONE_ID = 12;
const ZoneID_t SLAYER_DEFAULT_ZONE_ID = 13;
const ZoneID_t VAMPIRE_DEFAULT_ZONE_ID = 23;
const ZoneID_t OUSTERS_DEFAULT_ZONE_ID = 1311;

//////////////////////////////////////////////////////////////////////////////
// class ResurrectLocationManager member methods
//////////////////////////////////////////////////////////////////////////////

ResurrectLocationManager::ResurrectLocationManager()

    {__BEGIN_TRY __END_CATCH}

ResurrectLocationManager::~ResurrectLocationManager()

{
    __BEGIN_TRY

    m_SlayerPosition.clear();
    m_VampirePosition.clear();

    __END_CATCH_NO_RETHROW
}

void ResurrectLocationManager::init()

{
    __BEGIN_TRY

    load();

    __END_CATCH
}

void ResurrectLocationManager::load()

{
    __BEGIN_TRY

    vector<ResurrectLocationRow> rows = defaultZoneInfoRepository().loadResurrectLocations();

    if (rows.empty()) {
        // cerr << "ResurrectLocationManager::load() : TABLE DOES NOT EXIST!" << endl;
        throw Error("ResurrectLocationManager::load() : TABLE DOES NOT EXIST!");
    }

    for (size_t r = 0; r < rows.size(); r++) {
        ZoneID_t ID = 0;
        ZONE_COORD slayer_coord;
        ZONE_COORD vampire_coord;
        ZONE_COORD ousters_coord;

        ID = rows[r].zoneID;
        slayer_coord.id = rows[r].slayerZoneID;
        slayer_coord.x = rows[r].slayerX;
        slayer_coord.y = rows[r].slayerY;
        vampire_coord.id = rows[r].vampireZoneID;
        vampire_coord.x = rows[r].vampireX;
        vampire_coord.y = rows[r].vampireY;
        ousters_coord.id = rows[r].oustersZoneID;
        ousters_coord.x = rows[r].oustersX;
        ousters_coord.y = rows[r].oustersY;

        addSlayerPosition(ID, slayer_coord);
        addVampirePosition(ID, vampire_coord);
        addOustersPosition(ID, ousters_coord);
    }

    __END_CATCH
}

bool ResurrectLocationManager::getSlayerPosition(ZoneID_t id, ZONE_COORD& zoneCoord) const
// NoSuchElementException)
{
    __BEGIN_TRY

    unordered_map<ZoneID_t, ZONE_COORD>::const_iterator itr = m_SlayerPosition.find(id);

    if (itr == m_SlayerPosition.end()) {
        // cerr << "ResurrectLocationManager::getPosition() : No Such ZoneID" << endl;
        // throw NoSuchElementException("ResurrectLocationManager::getPosition() : No Such ZoneID");

        // Returns false instead of throwing NoSuchElementException.
        return false;
    }

    // return itr->second;
    zoneCoord = itr->second;

    return true;

    __END_CATCH
}


void ResurrectLocationManager::addSlayerPosition(ZoneID_t id, const ZONE_COORD& coord)

{
    __BEGIN_TRY

    unordered_map<ZoneID_t, ZONE_COORD>::const_iterator itr = m_SlayerPosition.find(id);

    if (itr != m_SlayerPosition.end()) {
        // cerr << "ResurrectLocationManager::addPosition() : ZoneID already exist!" << endl;
        throw NoSuchElementException("ResurrectLocationManager::addPosition() : ZoneID already exist!");
    }

    m_SlayerPosition[id] = coord;

    __END_CATCH
}

bool ResurrectLocationManager::getVampirePosition(ZoneID_t id, ZONE_COORD& zoneCoord) const
// NoSuchElementException)
{
    __BEGIN_TRY

    unordered_map<ZoneID_t, ZONE_COORD>::const_iterator itr = m_VampirePosition.find(id);

    if (itr == m_VampirePosition.end()) {
        // cerr << "ResurrectLocationManager::getPosition() : No Such ZoneID" << endl;
        //  Returns false instead of throwing NoSuchElementException.
        // throw NoSuchElementException("ResurrectLocationManager::getPosition() : No Such ZoneID");
        return false;
    }

    // return itr->second;

    zoneCoord = itr->second;

    return true;

    __END_CATCH
}


void ResurrectLocationManager::addVampirePosition(ZoneID_t id, const ZONE_COORD& coord)

{
    __BEGIN_TRY

    unordered_map<ZoneID_t, ZONE_COORD>::const_iterator itr = m_VampirePosition.find(id);

    if (itr != m_VampirePosition.end()) {
        // cerr << "ResurrectLocationManager::addPosition() : ZoneID already exist!" << endl;
        throw NoSuchElementException("ResurrectLocationManager::addPosition() : ZoneID already exist!");
    }

    m_VampirePosition[id] = coord;

    __END_CATCH
}


bool ResurrectLocationManager::getOustersPosition(ZoneID_t id, ZONE_COORD& zoneCoord) const
// NoSuchElementException)
{
    __BEGIN_TRY

    unordered_map<ZoneID_t, ZONE_COORD>::const_iterator itr = m_OustersPosition.find(id);

    if (itr == m_OustersPosition.end()) {
        return false;
    }

    zoneCoord = itr->second;

    return true;

    __END_CATCH
}


void ResurrectLocationManager::addOustersPosition(ZoneID_t id, const ZONE_COORD& coord)

{
    __BEGIN_TRY

    unordered_map<ZoneID_t, ZONE_COORD>::const_iterator itr = m_OustersPosition.find(id);

    if (itr != m_OustersPosition.end()) {
        throw DuplicatedException("ResurrectLocationManager::addPosition() : ZoneID already exist!");
    }

    m_OustersPosition[id] = coord;

    __END_CATCH
}

bool ResurrectLocationManager::getPosition(PlayerCreature* pPC, ZONE_COORD& zoneCoord) const

{
    __BEGIN_TRY

    Assert(pPC != NULL);

    CastleInfoManager& castleInfos = de::gameContext().castleInfos();

    try {
        bool bFindPosition = false;

        ZoneID_t castleZoneID;
        bool isCastleZone = castleInfos.getCastleZoneID(pPC->getResurrectZoneID(), castleZoneID);

        if (de::gameContext().warSystem().hasActiveRaceWar() && pPC->getZone()->isHolyLand()) {
            if (pPC->isSlayer()) {
                if (rand() % 2)
                    castleZoneID = 1201;
                else
                    castleZoneID = 1203;
            } else if (pPC->isVampire()) {
                if (rand() % 2)
                    castleZoneID = 1202;
                else
                    castleZoneID = 1204;
            } else if (pPC->isOusters()) {
                if (rand() % 2)
                    castleZoneID = 1205;
                else
                    castleZoneID = 1206;
            } else
                castleZoneID = 0;

            if (castleZoneID != 0) {
                CastleInfo* pCastleInfo = castleInfos.getCastleInfo(castleZoneID);
                if (pCastleInfo != NULL) {
                    pCastleInfo->getResurrectPosition(CastleInfo::CASTLE_RESURRECT_PRIORITY_FIRST, zoneCoord);
                    bFindPosition = true;
                }
            }
        }

        // If a race war is running and the number of participants is limited
        if (!bFindPosition && de::gameContext().warSystem().hasActiveRaceWar() &&
            de::gameContext().variables().isActiveRaceWarLimiter()) {
            ZoneInfo* pResZoneInfo = NULL;

            if (pPC->getResurrectZoneID() != 0) {
                try {
                    pResZoneInfo = de::gameContext().zoneInfos().getZoneInfo(pPC->getResurrectZoneID());
                } catch (Throwable& t) {
                    filelog("ResurrectLocationError.txt", "%s", t.toString().c_str());
                }
            }

            // Entering Adam's holy land without having applied for the war
            if (((pResZoneInfo != NULL && pResZoneInfo->isHolyLand()) || pPC->getZone()->isHolyLand()) &&
                !pPC->isFlag(Effect::EFFECT_CLASS_RACE_WAR_JOIN_TICKET)) {
                // Send them to the default resurrection position of their race.
                if (getRaceDefaultPosition(pPC->getRace(), zoneCoord)) {
                    bFindPosition = true;
                } else {
                    throw Error("Critical Error : ResurrectInfo is not established!2");
                }
            }
        }

        if (!bFindPosition) {
            if (isCastleZone) {
                if (!castleInfos.getResurrectPosition(pPC, zoneCoord)) {
                    if (!getRaceDefaultPosition(pPC->getRace(), zoneCoord)) {
                        throw Error("Critical Error : ResurrectInfo is not established!2");
                    }
                }
            } else {
                if (!getBasicPosition(pPC, zoneCoord)) {
                    throw Error("Critical Error : ResurrectInfo is not established!2");
                }
            }
        }

    } catch (Error& e) {
        filelog("ResurrectLocationError.txt", "%s", e.toString().c_str());
        return false;
    } catch (Throwable& t) {
        filelog("ResurrectLocationError.txt", "%s", t.toString().c_str());
        return false;
    }

    return true;

    __END_CATCH
}

bool ResurrectLocationManager::getBasicPosition(PlayerCreature* pPC, ZONE_COORD& zoneCoord) const

{
    __BEGIN_TRY

    // First take the resurrection position set on the PlayerCreature.
    if (pPC->isSlayer()) {
        Slayer* pSlayer = dynamic_cast<Slayer*>(pPC);
        Assert(pSlayer != NULL);

        // A novice resurrects in the novice zone.
        if (pSlayer->isNovice()) {
            pPC->setResurrectZoneID(SLAYER_NOVICE_ZONE_ID);
        }

        if (!getSlayerPosition(pPC->getResurrectZoneID(), zoneCoord)) {
            // If no resurrection zone is set, take the position of the current zone.
            if (!getSlayerPosition(pPC->getZone()->getZoneID(), zoneCoord)) {
                // If the current zone has none either, use the default.
                if (!getSlayerPosition(SLAYER_DEFAULT_ZONE_ID, zoneCoord))
                    throw Error("Critical Error : ResurrectInfo is not established!2");
            }
        }
    } else if (pPC->isVampire()) {
        if (!getVampirePosition(pPC->getResurrectZoneID(), zoneCoord)) {
            // If no resurrection zone is set, take the position of the current zone.
            if (!getVampirePosition(pPC->getZone()->getZoneID(), zoneCoord)) {
                // If the current zone has none either, use the default.
                if (!getVampirePosition(VAMPIRE_DEFAULT_ZONE_ID, zoneCoord))
                    throw Error("Critical Error : ResurrectInfo is not established!2");
            }
        }
    } else if (pPC->isOusters()) {
        if (!getOustersPosition(pPC->getResurrectZoneID(), zoneCoord)) {
            // If no resurrection zone is set, take the position of the current zone.
            if (!getOustersPosition(pPC->getZone()->getZoneID(), zoneCoord)) {
                // If the current zone has none either, use the default.
                if (!getOustersPosition(OUSTERS_DEFAULT_ZONE_ID, zoneCoord))
                    throw Error("Critical Error : ResurrectInfo is not established!2");
            }
        }
    }

    return true;

    __END_CATCH
}

bool ResurrectLocationManager::getRaceDefaultPosition(Race_t race, ZONE_COORD& zoneCoord) const

{
    __BEGIN_TRY

    if (race == RACE_SLAYER)
        return getSlayerPosition(SLAYER_DEFAULT_ZONE_ID, zoneCoord);
    else if (race == RACE_VAMPIRE)
        return getVampirePosition(VAMPIRE_DEFAULT_ZONE_ID, zoneCoord);
    else if (race == RACE_OUSTERS)
        return getOustersPosition(OUSTERS_DEFAULT_ZONE_ID, zoneCoord);

    return false;

    __END_CATCH
}
