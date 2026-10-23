#include "RegenZoneManager.h"

#include "CastleInfoManager.h"
#include "EffectRegenZone.h"
#include "EffectTryingPosition.h"
#include "GCAddEffect.h"
#include "GCAddEffectToTile.h"
#include "GCRegenZoneStatus.h"
#include "GCRemoveEffect.h"
#include "GameContext.h"
#include "HolyLandManager.h"
#include "Monster.h"
#include "MonsterCorpse.h"
#include "MonsterManager.h"
#include "Ousters.h"
#include "PlayerCreature.h"
#include "SiegeManager.h"
#include "Slayer.h"
#include "StringPool.h"
#include "Tile.h"
#include "Vampire.h"
#include "WarZoneWork.h"
#include "Zone.h"
#include "ZoneUtil.h"
#include "repository/RegenZoneRepository.h"
#include "war/WarSystem.h"

void RegenZoneInfo::putTryingPosition() {
    Zone* pZone = m_pRegenZoneTower->getZone();
    ZoneCoord_t X = m_pRegenZoneTower->getX() - 2;
    ZoneCoord_t Y = m_pRegenZoneTower->getY() + 2;

    __ENTER_CRITICAL_SECTION((*pZone));

    EffectTryingPosition* pEffect = new EffectTryingPosition(pZone, X, Y, m_pRegenZoneTower);
    pZone->registerObject(pEffect);
    pZone->addEffect(pEffect);
    pZone->getTile(X, Y).addEffect(pEffect);

    GCAddEffectToTile gcAddEffectToTile;
    gcAddEffectToTile.setXY(X, Y);
    gcAddEffectToTile.setEffectID(pEffect->getSendEffectClass());
    gcAddEffectToTile.setObjectID(pEffect->getObjectID());
    gcAddEffectToTile.setDuration(pEffect->getRemainDuration());

    pZone->broadcastPacket(X, Y, &gcAddEffectToTile);

    __LEAVE_CRITICAL_SECTION((*pZone));
}

void RegenZoneInfo::deleteTryingPosition() {
    Zone* pZone = m_pRegenZoneTower->getZone();
    ZoneCoord_t X = m_pRegenZoneTower->getX() - 2;
    ZoneCoord_t Y = m_pRegenZoneTower->getY() + 2;

    __ENTER_CRITICAL_SECTION((*pZone));

    EffectTryingPosition* pEffect =
        dynamic_cast<EffectTryingPosition*>(pZone->getTile(X, Y).getEffect(Effect::EFFECT_CLASS_TRYING_POSITION));
    if (pEffect != NULL)
        pEffect->setDeadline(0);

    __LEAVE_CRITICAL_SECTION((*pZone));
}

RegenZoneManager::RegenZoneManager() {}
RegenZoneManager::~RegenZoneManager() {
    map<uint, RegenZoneInfo*>::iterator itr = m_RegenZoneInfos.begin();
    map<uint, RegenZoneInfo*>::iterator endItr = m_RegenZoneInfos.end();

    for (; itr != endItr; ++itr) {
        SAFE_DELETE(itr->second);
    }
}

// Reads the regen zones' owners back from the database, as the race war left
// them, and hands each tower's owner to the tower's zone thread, which sets it
// and tells the holy land. The table of regen zones itself is loaded once and
// never changes, so the zone threads may look a regen zone up in it.
void RegenZoneManager::reload() {
    vector<RegenZoneRow> rows = defaultRegenZoneRepository().loadPositions();

    for (size_t r = 0; r < rows.size(); r++) {
        uint ID = rows[r].id;
        ZoneCoord_t ZoneX = rows[r].zoneX;
        ZoneCoord_t ZoneY = rows[r].zoneY;
        uint Owner = rows[r].owner;

        Assert(Owner < 4);

        de::war::postToZone(rows[r].zoneID, [ID, ZoneX, ZoneY, Owner](Zone& zone) {
            RegenZoneManager::getInstance()->reloadOwner(zone, ID, ZoneX, ZoneY, Owner);
        });
    }
}

void RegenZoneManager::reloadOwner(Zone& zone, uint ID, ZoneCoord_t ZoneX, ZoneCoord_t ZoneY, uint Owner) {
    __ENTER_CRITICAL_SECTION(zone)

    Item* pTowerItem = zone.getTile(ZoneX, ZoneY).getItem();
    if (pTowerItem == NULL || pTowerItem->getItemClass() != Item::ITEM_CLASS_CORPSE ||
        pTowerItem->getItemType() != MONSTER_CORPSE) {
        filelog("RaceWar.log", "Reload : no regen zone tower at [%d:(%d,%d)]", zone.getZoneID(), ZoneX, ZoneY);
        return;
    }

    MonsterCorpse* pTower = dynamic_cast<MonsterCorpse*>(pTowerItem);
    Assert(pTower != NULL);

    RegenZoneInfo* pInfo = getRegenZoneInfo(ID);
    if (pInfo == NULL) {
        filelog("RaceWar.log", "Reload : no regen zone %d", ID);
        return;
    }

    EffectRegenZone* pEffect =
        dynamic_cast<EffectRegenZone*>(pTower->getEffectManager().findEffect(Effect::EFFECT_CLASS_SLAYER_REGEN_ZONE));

    {
        std::lock_guard<std::mutex> lock(m_StatusMutex);
        pInfo->setOwner((RegenZoneInfo::RegenZoneIndex)Owner);
        if (pEffect != NULL)
            m_Status.setStatus(ID, Owner);
    }

    if (pEffect == NULL) {
        filelog("RaceWar.log", "Reload : regen zone %d lost its effect", ID);
        return;
    }

    pEffect->setOwner((RegenZoneInfo::RegenZoneIndex)Owner);

    __LEAVE_CRITICAL_SECTION(zone)

    broadcastStatus();
}

RegenZoneInfo* RegenZoneManager::getRegenZoneInfo(uint ID) const {
    map<uint, RegenZoneInfo*>::const_iterator itr = m_RegenZoneInfos.find(ID);
    return (itr == m_RegenZoneInfos.end()) ? NULL : itr->second;
}

void RegenZoneManager::load() {
    vector<RegenZoneRow> rows = defaultRegenZoneRepository().loadPositions();

    for (size_t r = 0; r < rows.size(); r++) {
        uint ID = rows[r].id;
        ZoneID_t ZoneID = rows[r].zoneID;
        ZoneCoord_t ZoneX = rows[r].zoneX;
        ZoneCoord_t ZoneY = rows[r].zoneY;
        uint Owner = rows[r].owner;

        Assert(Owner < 4);

        Zone* pZone = getZoneByZoneID(ZoneID);
        Assert(pZone != NULL);

        MonsterCorpse* pTower = new MonsterCorpse(673, de::gameContext().strings().getString(STRID_REGENZONE_TOWER), 2);
        Assert(pTower != NULL);

        pTower->setShrine(true);

        pZone->registerObject(pTower);
        pZone->addItem(pTower, ZoneX, ZoneY);

        m_RegenZoneInfos[ID] = new RegenZoneInfo(ID, pTower, Owner);
        m_RegenZoneInfos[ID]->setOriginalOwner((RegenZoneInfo::RegenZoneIndex)Owner);

        EffectRegenZone* pEffect = new EffectRegenZone(pTower);
        pEffect->setOwner(m_RegenZoneInfos[ID]->getOwner());
        pTower->setFlag(pEffect->getEffectClass());

        pTower->getEffectManager().addEffect(pEffect);

        std::lock_guard<std::mutex> lock(m_StatusMutex);
        m_Status.setStatus(ID, Owner);
    }
}

void RegenZoneManager::putTryingPosition() {
    for (const auto& entry : m_RegenZoneInfos) {
        uint ID = entry.first;

        de::war::postToZone(entry.second->getTower()->getZone()->getZoneID(), [ID](Zone&) {
            RegenZoneInfo* pInfo = RegenZoneManager::getInstance()->getRegenZoneInfo(ID);
            if (pInfo != NULL)
                pInfo->putTryingPosition();
        });
    }
}

void RegenZoneManager::deleteTryingPosition() {
    for (const auto& entry : m_RegenZoneInfos) {
        uint ID = entry.first;

        de::war::postToZone(entry.second->getTower()->getZone()->getZoneID(), [ID](Zone&) {
            RegenZoneInfo* pInfo = RegenZoneManager::getInstance()->getRegenZoneInfo(ID);
            if (pInfo != NULL)
                pInfo->deleteTryingPosition();
        });
    }
}

void RegenZoneManager::changeRegenZoneOwner(MonsterCorpse* pTower, Race_t race) {
    Assert(race < 4);
    map<uint, RegenZoneInfo*>::iterator itr = m_RegenZoneInfos.begin();
    map<uint, RegenZoneInfo*>::iterator endItr = m_RegenZoneInfos.end();

    if (!de::gameContext().warSystem().hasActiveRaceWar())
        return;

    for (; itr != endItr; ++itr) {
        if (itr->second->getTower() == pTower) {
            if (itr->second->getOriginalOwner() != RegenZoneInfo::REGEN_ZONE_DEFAULT) {
                if (race != (Race_t)itr->second->getOriginalOwner())
                    race = (Race_t)RegenZoneInfo::REGEN_ZONE_DEFAULT;
            }

            EffectRegenZone* pEffect = dynamic_cast<EffectRegenZone*>(
                pTower->getEffectManager().findEffect(Effect::EFFECT_CLASS_SLAYER_REGEN_ZONE));

            {
                std::lock_guard<std::mutex> lock(m_StatusMutex);
                itr->second->setOwner((RegenZoneInfo::RegenZoneIndex)race);
                if (pEffect != NULL)
                    m_Status.setStatus(itr->second->getID(), race);
            }

            if (pEffect != NULL) {
                GCRemoveEffect gcRemoveEffect;
                gcRemoveEffect.addEffectList(pEffect->getSendEffectClass());
                gcRemoveEffect.setObjectID(pTower->getObjectID());

                pEffect->setOwner((RegenZoneInfo::RegenZoneIndex)race);

                broadcastStatus();

                GCAddEffect gcAddEffect;
                gcAddEffect.setEffectID(pEffect->getSendEffectClass());
                gcAddEffect.setObjectID(pTower->getObjectID());
                gcAddEffect.setDuration(pEffect->getRemainDuration());

                pTower->getZone()->broadcastPacket(pTower->getX(), pTower->getY(), &gcRemoveEffect);
                pTower->getZone()->broadcastPacket(pTower->getX(), pTower->getY(), &gcAddEffect);

                gcAddEffect.setEffectID(Effect::EFFECT_CLASS_CAPTURE_REGEN_ZONE);
                gcAddEffect.setObjectID(pTower->getObjectID());
                gcAddEffect.setDuration(1);

                pTower->getZone()->broadcastPacket(pTower->getX(), pTower->getY(), &gcAddEffect);
            }

            return;
        }
    }
}

bool RegenZoneManager::canTryRegenZone(PlayerCreature* pPC, MonsterCorpse* pTower) {
    map<uint, RegenZoneInfo*>::iterator itr = m_RegenZoneInfos.begin();
    map<uint, RegenZoneInfo*>::iterator endItr = m_RegenZoneInfos.end();

    for (; itr != endItr; ++itr) {
        if (itr->second->getTower() == pTower) {
            return pPC->getRace() != (Race_t)itr->second->getOwner();
        }
    }

    return false;
}

bool RegenZoneManager::canRegen(PlayerCreature* pPC, uint ID) {
    map<uint, RegenZoneInfo*>::iterator itr = m_RegenZoneInfos.find(ID);
    RegenZoneInfo* pInfo;

    if (itr == m_RegenZoneInfos.end()) {
        switch (ID) {
        case 8:  // Octavus
        case 10: // Septimus
            return pPC->isSlayer();
        case 9:  // Tertius
        case 11: // Quartus
            return pPC->isVampire();
        case 12:
        case 13:
            return pPC->isOusters();

        case 14: {
            if (!SiegeManager::Instance().isSiegeZone(pPC->getZoneID()))
                return false;
            Zone* pZone = pPC->getZone();
            MonsterManager* pMM = pZone->getMonsterManager();
            unordered_map<ObjectID_t, Creature*>& cmap = pMM->getCreatures();
            unordered_map<ObjectID_t, Creature*>::iterator itr = cmap.begin();

            // The castle gate must not be there.
            for (; itr != cmap.end(); ++itr) {
                Monster* pMonster = dynamic_cast<Monster*>((itr->second));
                if (pMonster != NULL && pMonster->getMonsterType() == 726)
                    return false;
            }
        }
            return (pPC->isFlag(Effect::EFFECT_CLASS_SIEGE_ATTACKER_1) ||
                    pPC->isFlag(Effect::EFFECT_CLASS_SIEGE_ATTACKER_2) ||
                    pPC->isFlag(Effect::EFFECT_CLASS_SIEGE_ATTACKER_3) ||
                    pPC->isFlag(Effect::EFFECT_CLASS_SIEGE_ATTACKER_4) ||
                    pPC->isFlag(Effect::EFFECT_CLASS_SIEGE_ATTACKER_5));
        default:
            return false;
            break;
        };
        return false;
    } else
        pInfo = itr->second;

    return pPC->getRace() == (Race_t)pInfo->getOwner();
}

void RegenZoneManager::regeneratePC(PlayerCreature* pPC, uint ID) {
    map<uint, RegenZoneInfo*>::iterator itr = m_RegenZoneInfos.find(ID);
    ZONE_COORD targetPos;
    RegenZoneInfo* pInfo;

    if (itr == m_RegenZoneInfos.end()) {
        switch (ID) {
        case 8: // Octavus
        {
            targetPos.id = 1201;
            targetPos.x = 120;
            targetPos.y = 120;
            break;
        }
        case 9: // Tertius
        {
            targetPos.id = 1202;
            targetPos.x = 30;
            targetPos.y = 120;
            break;
        }
        case 10: // Septimus
        {
            targetPos.id = 1203;
            targetPos.x = 120;
            targetPos.y = 30;
            break;
        }
        case 11: // Quartus
        {
            targetPos.id = 1204;
            targetPos.x = 30;
            targetPos.y = 30;
            break;
        }
        case 12: {
            targetPos.id = 1205;
            targetPos.x = 30;
            targetPos.y = 30;
            break;
        }
        case 13: {
            targetPos.id = 1206;
            targetPos.x = 30;
            targetPos.y = 120;
            break;
        }
        case 14: {
            targetPos.id = pPC->getZoneID();
            targetPos.x = 156;
            targetPos.y = 108;
            break;
        }
        default:
            return;
            break;
        };
        CastleInfo* pCastleInfo = de::gameContext().castleInfos().getCastleInfo(targetPos.id);
        if (pCastleInfo != NULL) {
            pCastleInfo->getResurrectPosition(CastleInfo::CASTLE_RESURRECT_PRIORITY_FIRST, targetPos);
        }
    } else {
        pInfo = itr->second;
        MonsterCorpse* pTower = pInfo->getTower();
        targetPos.id = pTower->getZone()->getZoneID();
        targetPos.x = pTower->getX();
        targetPos.y = pTower->getY();
    }

    pPC->deleteEffect(Effect::EFFECT_CLASS_COMA);

    if (pPC->isSlayer()) {
        Slayer* pSlayer = dynamic_cast<Slayer*>(pPC);
        Assert(pSlayer != NULL);

        pSlayer->setHP(pSlayer->getHP(ATTR_MAX));
    } else if (pPC->isVampire()) {
        Vampire* pVampire = dynamic_cast<Vampire*>(pPC);
        Assert(pVampire != NULL);

        pVampire->setHP(pVampire->getHP(ATTR_MAX) - pVampire->getSilverDamage());
    } else if (pPC->isOusters()) {
        Ousters* pOusters = dynamic_cast<Ousters*>(pPC);
        Assert(pOusters != NULL);

        pOusters->setHP(pOusters->getHP(ATTR_MAX) - pOusters->getSilverDamage());
    }

    transportCreature(pPC, targetPos.id, targetPos.x, targetPos.y, false);
}

void RegenZoneManager::broadcastStatus() const {
    de::war::postToZones(de::gameContext().holyLands().getHolyLandZoneIDs(), [](Zone& zone) {
        GCRegenZoneStatus status = RegenZoneManager::getInstance()->getStatus();
        zone.broadcastPacket(&status);
    });
}

GCRegenZoneStatus RegenZoneManager::getStatus() const {
    std::lock_guard<std::mutex> lock(m_StatusMutex);
    return m_Status;
}
