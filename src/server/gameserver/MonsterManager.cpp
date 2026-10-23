////////////////////////////////////////////////////////////////////////////////
// Filename    : MonsterManager.h
// Written By  : Reiot
// Description :
////////////////////////////////////////////////////////////////////////////////


#include "MonsterManager.h"

#include <stdlib.h> // atoi()

#include <fstream>

#include "Assert.h"
#include "CastleInfoManager.h"
#include "Creature.h"
#include "CreatureUtil.h"
#include "Deployment.h"
#include "DynamicZone.h"
#include "DynamicZoneGateOfAlter.h"
#include "EffectPacketSend.h"
#include "EventItemUtil.h"
#include "GCAddEffect.h"
#include "GCAddEffectToTile.h"
#include "GCCreatureDied.h"
#include "GCDeleteObject.h"
#include "GCSay.h"
#include "GDRLairManager.h"
#include "GameContext.h"
#include "GamePlayer.h"
#include "ItemFactoryManager.h"
#include "ItemGradeManager.h"
#include "ItemInfoManager.h"
#include "ItemUtil.h"
#include "KernelContext.h"
#include "MasterLairInfoManager.h"
#include "Monster.h"
#include "MonsterCorpse.h"
#include "MonsterInfo.h"
#include "MonsterNameManager.h"
#include "OptionInfo.h"
#include "Player.h"
#include "Profile.h"
#include "Properties.h"
#include "Skull.h"
#include "StringPool.h"
#include "SweeperBonusManager.h"
#include "Thread.h"
#include "Tile.h"
#include "Treasure.h"
#include "VariableManager.h"
#include "WarSystem.h"
#include "Zone.h"
#include "ZoneUtil.h"
#include "repository/ZoneInfoRepository.h"
#include "skill/EffectHarpoonBomb.h"
#include "skill/SummonGroundElemental.h"

#define __MONSTER_FIGHTING__
extern bool isPotentialEnemy(Monster* pMonster, Creature* pCreature);
extern void countResurrectItem();

// The highest luck level a drop's item-type or option-type upgrade roll uses.
const int MAX_LUCK_LEVEL = 140;


#ifdef __PROFILE_MONSTER__
#define __BEGIN_PROFILE_MONSTER(name) beginProfileEx(name);
#define __END_PROFILE_MONSTER(name) endProfileEx(name);
#else
#define __BEGIN_PROFILE_MONSTER(name) ((void)0);
#define __END_PROFILE_MONSTER(name) ((void)0);
#endif

////////////////////////////////////////////////////////////////////////////////
// Item looting probability bonus percentage for pay zones
////////////////////////////////////////////////////////////////////////////////

bool isLottoWinning();

////////////////////////////////////////////////////////////////////////////////
//
// constructor
//
////////////////////////////////////////////////////////////////////////////////
MonsterManager::MonsterManager(Zone* pZone)

{
    Assert(pZone != NULL);
    m_pZone = pZone;

    m_CastleZoneID = 0;
    de::gameContext().castleInfos().getCastleZoneID(m_pZone->getZoneID(), m_CastleZoneID);

    m_nEventMonster = 0;
    m_pEventMonsterInfo = NULL;

    getCurrentTime(m_RegenTime);
}

////////////////////////////////////////////////////////////////////////////////
//
// destructor
//
////////////////////////////////////////////////////////////////////////////////
MonsterManager::~MonsterManager()

{
    __BEGIN_TRY

    SAFE_DELETE(m_pEventMonsterInfo);

    __END_CATCH_NO_RETHROW
}

////////////////////////////////////////////////////////////////////////////////
// load from database
////////////////////////////////////////////////////////////////////////////////
void MonsterManager::load()

{
    string text, eventText;

    m_RICE_CAKE_PROB_RATIO[0] = 100;
    m_RICE_CAKE_PROB_RATIO[1] = 33;
    m_RICE_CAKE_PROB_RATIO[2] = 33;
    m_RICE_CAKE_PROB_RATIO[3] = 33;
    m_RICE_CAKE_PROB_RATIO[4] = 1;
    m_SumOfCakeRatio = 0;

    for (int i = 0; i < 5; i++)
        m_SumOfCakeRatio += m_RICE_CAKE_PROB_RATIO[i];

    // If MonsterCounters already exist, delete all of them.
    bool bReload = false;
    unordered_map<SpriteType_t, MonsterCounter*>::iterator iMC = m_Monsters.begin();
    while (iMC != m_Monsters.end()) {
        MonsterCounter* pMC = iMC->second;
        SAFE_DELETE(pMC);

        iMC++;

        // If m_Monsters already existed, treat this as a reload.
        bReload = true;
    }

    // A dynamic zone loads its template zone's monster lists.
    ZoneID_t zoneID = m_pZone->getZoneID();
    if (m_pZone->isDynamicZone()) {
        DynamicZone* pDynamicZone = m_pZone->getDynamicZone();
        Assert(pDynamicZone != NULL);

        zoneID = pDynamicZone->getTemplateZoneID();
    }

    if (!defaultZoneInfoRepository().loadMonsterLists(zoneID, text, eventText)) {
        // the zone has no row
        return;
    }


    parseMonsterList(text, bReload);
    parseEventMonsterList(eventText, bReload);
}

void MonsterManager::parseMonsterList(const string& text, bool bReload)

{
    if (text.size() <= 0)
        return;

    //--------------------------------------------------------------------------------
    //
    // The text parameter is the value of the Monsters (TEXT) column of the ZoneInfo table.
    // The format is as follows.
    //
    // (MonsterType1,#Monster1) (MonsterType2,#Monter2)(..,..)
    // i            j         k i            j        k
    //
    //--------------------------------------------------------------------------------

    size_t i = 0, j = 0, k = 0;

    do {
        // parse string
        i = text.find_first_of('(', k);
        j = text.find_first_of(',', i + 1);
        k = text.find_first_of(')', j + 1);

        if (i == string::npos || j == string::npos || k == string::npos || i > j || j > k)
            break;

        // Get the monster type and the maximum count.
        uint monsterType = atoi(text.substr(i + 1, j - i - 1).c_str());
        uint maxMonsters = atoi(text.substr(j + 1, k - j - 1).c_str());

        Assert(maxMonsters > 0);

        // Get the monster sprite type from the monster info.
        const MonsterInfo* pMonsterInfo = de::gameContext().monsterInfos().getMonsterInfo(monsterType);
        SpriteType_t spriteType = pMonsterInfo->getSpriteType();

        // Check whether it already exists.
        unordered_map<SpriteType_t, MonsterCounter*>::iterator itr = m_Monsters.find(spriteType);

        if (itr != m_Monsters.end()) {
            WORD CurrentMaxCount = itr->second->getMaxMonsters();
            WORD NewMaxCount = CurrentMaxCount + maxMonsters;
            itr->second->setMaxMonsters(NewMaxCount);
        } else {
            // Create a MonsterCounter object and register it in the unordered_map.
            MonsterCounter* pMonsterCounter = new MonsterCounter(monsterType, maxMonsters, 0);

            // If it does not exist, add it.
            m_Monsters[spriteType] = pMonsterCounter;
        }

        //--------------------------------------------------------------------------------
        // Add monsters of that type to the zone.
        //--------------------------------------------------------------------------------
        if (!bReload) // Only when this is not a reload.
        {
            for (uint m = 0; m < maxMonsters; m++) {
                // Find an empty coordinate in the zone.
                ZoneCoord_t x, y;
                if (!findPosition(monsterType, x, y)) {
                    Assert(false);
                    return;
                }

                // Create the monster object and initialize its stats.
                Monster* pMonster = new Monster(monsterType);

                ////////////////////////////////////////////////////////////////////////////////
                // World Cup event related (gone from July 1)
                ///////////////////////////////////////////////////////////////////////////
                Assert(pMonster != NULL);

                try {
                    m_pZone->addCreature(pMonster, x, y, Directions(rand() & 0x07));
                } catch (EmptyTileNotExistException&) {
                    SAFE_DELETE(pMonster);
                }
            }
        }
    } while (k < text.size() - 1);
}

void MonsterManager::parseEventMonsterList(const string& text, bool bReload)

{
    if (text.size() <= 0)
        return;

    //--------------------------------------------------------------------------------
    //
    // The text parameter is the value of the Monsters (TEXT) column of the ZoneInfo table.
    // The format is as follows.
    //
    // (MonsterType1,#Monster1,RegenDelay) (MonsterType2,#Monter2,RegenDelay)(..,..)
    // i            j         k          l i            j        k          l
    //
    //--------------------------------------------------------------------------------

    size_t i = 0, j = 0, k = 0, l = 0, m = 0, n = 0;

    do {
        // parse string
        i = text.find_first_of('(', l);
        j = text.find_first_of(',', i + 1);
        k = text.find_first_of(',', j + 1);
        l = text.find_first_of(')', k + 1);
        m = text.find_first_of(',', k + 1);
        n = text.find_first_of(',', m + 1);

        if (i == string::npos || j == string::npos || k == string::npos || l == string::npos || i > j || j > k || k > l)
            break;

        int tx = -1;
        int ty = -1;
        if (m != string::npos && n != string::npos && k < m && m < n && n < l) {
            tx = atoi(text.substr(m + 1, n - m - 1).c_str());
            ty = atoi(text.substr(n + 1, l - n - 1).c_str());
            cout << "x : " << tx << endl;
            cout << "y : " << ty << endl;
            l = m;
        }

        // Get the monster type and the maximum count.
        uint monsterType = atoi(text.substr(i + 1, j - i - 1).c_str());
        uint maxMonsters = atoi(text.substr(j + 1, k - j - 1).c_str());
        uint regenDelay = atoi(text.substr(k + 1, l - k - 1).c_str());

        Assert(maxMonsters > 0);

        //--------------------------------------------------------------------------------
        // Add monsters of that type to the zone.
        //--------------------------------------------------------------------------------
        if (!bReload) // Only when this is not a reload.
        {
            if (m_pEventMonsterInfo == NULL) {
                m_pEventMonsterInfo = new vector<EventMonsterInfo>;
            }


            for (uint m = 0; m < maxMonsters; m++) {
                if (de::gameContext().variables().isActiveChiefMonster()) {
                    // Find an empty coordinate in the zone.
                    ZoneCoord_t x, y;
                    if (tx != -1) {
                        x = tx;
                        y = ty;
                    } else if (!findPosition(monsterType, x, y)) {
                        Assert(false);
                        return;
                    }

                    // Create the monster object and initialize its stats.
                    Monster* pMonster = new Monster(monsterType);
                    Assert(pMonster != NULL);

                    pMonster->setEventMonsterIndex(m_nEventMonster++);

                    EventMonsterInfo info;
                    info.monsterType = monsterType;
                    info.regenDelay = regenDelay;
                    info.bExist = true;
                    info.x = tx;
                    info.y = ty;

                    m_pEventMonsterInfo->push_back(info);

                    try {
                        m_pZone->addCreature(pMonster, x, y, Directions(rand() & 0x07));

                        // Great Ruffian
                    } catch (EmptyTileNotExistException&) {
                        SAFE_DELETE(pMonster);
                    }
                } else {
                    m_nEventMonster++;

                    EventMonsterInfo info;
                    info.monsterType = monsterType;
                    info.regenDelay = regenDelay;
                    getCurrentTime(info.regenTime);
                    info.bExist = false;

                    m_pEventMonsterInfo->push_back(info);
                }
            }
        }
    } while (l < text.size() - 1);
}

////////////////////////////////////////////////////////////////////////////////
void MonsterManager::addCreature(Creature* pCreature)

{
    Monster* pMonster = dynamic_cast<Monster*>(pCreature);

    // Add it to the creature hash map.
    CreatureManager::addCreature(pMonster);

    // Event monsters are kept out of the MonsterCounter.
    if (m_pEventMonsterInfo != NULL && pMonster->isEventMonster()) {
        uint index = pMonster->getEventMonsterIndex();

        if (index < m_pEventMonsterInfo->size()) {
            EventMonsterInfo& info = (*m_pEventMonsterInfo)[index];

            getCurrentTime(info.regenTime);
            info.regenTime.tv_sec += info.regenDelay;

            info.bExist = true;
        }

        return;
    }

    // Check whether such a monster type can exist in the zone.
    unordered_map<SpriteType_t, MonsterCounter*>::iterator itr = m_Monsters.find(pMonster->getSpriteType());

    if (itr == m_Monsters.end()) {
        StringStream msg;
        msg << "A monster of a type that cannot exist in this zone was added.\n"
            << "The current zone is [" << m_pZone->getZoneID() << "].\n"
            << "The type of the monster being added is [" << pMonster->getMonsterType() << "].\n";
    } else {
        // Increment the monster counter.
        itr->second->addMonster();
    }
}


////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
void MonsterManager::deleteCreature(ObjectID_t creatureID)
// NoSuchElementException , Error)
{
    // Check whether a monster with that OID exists in the creature hash map.
    unordered_map<ObjectID_t, Creature*>::iterator itr = m_Creatures.find(creatureID);

    if (itr == m_Creatures.end()) {
        cerr << "MonsterManager::deleteCreature() : NoSuchElementException" << endl;

        // This one does not seem to be handled properly on the outside either.
        // by sigi. 2002.5.9

        return;
    }

    Monster* pMonster = dynamic_cast<Monster*>(itr->second);

    // Erase the matching node from the creature hash map.
    // The node must only be erased after the last use of itr.
    m_Creatures.erase(itr);


    // Event monsters have nothing to do with the MonsterCounter.
    if (m_pEventMonsterInfo != NULL && pMonster->isEventMonster() && pMonster->getMonsterType() != 764) {
        uint index = pMonster->getEventMonsterIndex();

        if (index < m_pEventMonsterInfo->size()) {
            EventMonsterInfo& info = (*m_pEventMonsterInfo)[index];
            info.bExist = false;
        }

        return;
    }

    // Check whether the monster counter holds that monster type.
    unordered_map<SpriteType_t, MonsterCounter*>::iterator itr2 = m_Monsters.find(pMonster->getSpriteType());

    if (itr2 == m_Monsters.end()) {
        cerr << "MonsterManager::deleteCreature() : NoSuchElementException" << endl;
    } else {
        // Decrement the monster count.
        itr2->second->deleteMonster();
    }
}

////////////////////////////////////////////////////////////////////////////////
// Make every monster recognize it as a potential enemy,
// for the case where pCreature attacked pAttackedMonster.
////////////////////////////////////////////////////////////////////////////////
void MonsterManager::addPotentialEnemy(Monster* pAttackedMonster, Creature* pCreature)

{
    unordered_map<ObjectID_t, Creature*>::const_iterator itr = m_Creatures.begin();

    for (; itr != m_Creatures.end(); itr++) {
        Creature* pMonsterCreature = itr->second;

        // Must be close enough to see the attacker.
        Distance_t dist = pMonsterCreature->getDistance(pCreature->getX(), pCreature->getY());

        if (dist <= pMonsterCreature->getSight()
            // The monster itself is checked by other code.
            && pMonsterCreature != pAttackedMonster) {
            Monster* pMonster = dynamic_cast<Monster*>(pMonsterCreature);
            pMonster->addPotentialEnemy(pCreature);
        }
    }
}

////////////////////////////////////////////////////////////////////////////////
// Make every monster recognize it as an enemy,
// for the case where pCreature attacked pAttackedMonster.
////////////////////////////////////////////////////////////////////////////////
void MonsterManager::addEnemy(Monster* pAttackedMonster, Creature* pCreature)

{
    unordered_map<ObjectID_t, Creature*>::const_iterator itr = m_Creatures.begin();

    for (; itr != m_Creatures.end(); itr++) {
        Creature* pMonsterCreature = itr->second;

        // Must be close enough to see the attacker.
        Distance_t dist = pMonsterCreature->getDistance(pCreature->getX(), pCreature->getY());

        if (dist <= pMonsterCreature->getSight()
            // The monster itself is checked by other code.
            && pMonsterCreature != pAttackedMonster) {
            Monster* pMonster = dynamic_cast<Monster*>(pMonsterCreature);
            pMonster->addEnemy(pCreature);
        }
    }
}

////////////////////////////////////////////////////////////////////////////////
// Run the AI-driven actions of the monsters owned by the creature manager.
////////////////////////////////////////////////////////////////////////////////
void MonsterManager::processCreatures()

{
    Timeval currentTime;
    getCurrentTime(currentTime);

    try {
        unordered_map<ObjectID_t, Creature*>::iterator before = m_Creatures.end();
        unordered_map<ObjectID_t, Creature*>::iterator current = m_Creatures.begin();

        while (current != m_Creatures.end()) {
            Creature* pCreature = current->second;

            Assert(pCreature != NULL);

            __BEGIN_PROFILE_MONSTER("MM_EFFECTMANAGER");

            pCreature->getEffectManager()->heartbeat(currentTime);

            __END_PROFILE_MONSTER("MM_EFFECTMANAGER");

            if (pCreature->isAlive()) {
                __BEGIN_PROFILE_MONSTER("MM_CREATURE_ACT");
                pCreature->act(currentTime);
                before = current++;
                __END_PROFILE_MONSTER("MM_CREATURE_ACT");
            } else {
                Monster* pMonster = dynamic_cast<Monster*>(pCreature);
                Assert(pMonster != NULL);


                if (pMonster->isEventMonster()) // by sigi. 2002.10.14
                {
                    if (m_pEventMonsterInfo != NULL) {
                        uint index = pMonster->getEventMonsterIndex();

                        if (index < m_pEventMonsterInfo->size()) {
                            EventMonsterInfo& info = (*m_pEventMonsterInfo)[index];
                            info.bExist = false;
                        }
                    }
                } else {
                    // Decrement the monster counter by one.
                    unordered_map<SpriteType_t, MonsterCounter*>::iterator itr =
                        m_Monsters.find(pMonster->getSpriteType());

                    if (itr == m_Monsters.end()) {
                    } else {
                        // Decrement the monster count.
                        itr->second->deleteMonster();
                    }
                }

                __BEGIN_PROFILE_MONSTER("MM_CREATURE_DEADACTION");
                // Let the monster take its last action before it is killed.
                pMonster->actDeadAction();
                __END_PROFILE_MONSTER("MM_CREATURE_DEADACTION");

                __BEGIN_PROFILE_MONSTER("MM_KILL_CREATURE");
                // Remove the monster from the zone and broadcast that.
                killCreature(pMonster);
                __END_PROFILE_MONSTER("MM_KILL_CREATURE");

                // Erase the monster's node from the creature hash map.
                // Erasing the wrong one risks breaking the iteration, so take care.
                if (before == m_Creatures.end()) {
                    m_Creatures.erase(current);
                    current = m_Creatures.begin();
                } else {
                    m_Creatures.erase(current);
                    current = before;
                    current++;
                }
                //}
            }
        }

        // The monster regeneration code contains findPosition, which retries up to
        // 300 times to find a free position. When several monsters die at once that
        // can take a long time and cause lag, so the check that counts the monsters
        // and regenerates them runs only once every 5 seconds.
        if (m_RegenTime < currentTime) {
            __BEGIN_PROFILE_MONSTER("MM_REGENERATE_CREATURES");

            regenerateCreatures();

            m_RegenTime.tv_sec = currentTime.tv_sec + 5; // Regenerate after 5 seconds
            m_RegenTime.tv_usec = currentTime.tv_usec;

            __END_PROFILE_MONSTER("MM_REGENERATE_CREATURES");
        }

    } catch (Throwable& t) {
        filelog("MonsterManagerBug.log", "ProcessCreatureBug : %s", t.toString().c_str());
    }
}

////////////////////////////////////////////////////////////////////////////////
// Recreate monsters when their number drops.
////////////////////////////////////////////////////////////////////////////////
void MonsterManager::regenerateCreatures()

{
    __BEGIN_DEBUG

    // Prevent monster regeneration during a war.
    if (m_pZone->isHolyLand()) {
        // A race war is in progress.
        if (de::gameContext().warSystem().hasActiveRaceWar())
            return;

        // A guild war is in progress.
        if (m_CastleZoneID != 0 && de::gameContext().warSystem().hasCastleActiveWar(m_CastleZoneID)) {
            CastleInfo* pCastleInfo = de::gameContext().castleInfos().getCastleInfo(m_CastleZoneID);
            if (pCastleInfo != NULL) {
                GuildID_t OwnerGuildID = pCastleInfo->getGuildID();

                // Regenerate only for a commonly owned castle.
                if (OwnerGuildID != SlayerCommon && OwnerGuildID != VampireCommon && OwnerGuildID != OustersCommon) {
                    return;
                }
            }
        }
    }

    // A level-based war is in progress.
    ZoneID_t zoneID = m_pZone->getZoneID();
    if (zoneID == 1131 || zoneID == 1132 || zoneID == 1133 || zoneID == 1134) {
        if (!de::gameContext().sweeperBonuses().isAble(zoneID))
            return;
    }

    unordered_map<SpriteType_t, MonsterCounter*>::iterator itr = m_Monsters.begin();
    for (; itr != m_Monsters.end(); itr++) {
        MonsterCounter* pCounter = itr->second;

        // When the number of monsters has dropped
        while (pCounter->getCurrentMonsters() < pCounter->getMaxMonsters()) {
            SpriteType_t SpriteType = itr->first;
            MonsterType_t monsterType = 0;

            vector<MonsterType_t> RegenVector = de::gameContext().monsterInfos().getMonsterTypeBySprite(SpriteType);
            Assert(RegenVector.size() > 0);

            monsterType = RegenVector[rand() % RegenVector.size()];

            // Find an empty coordinate in the zone.
            ZoneCoord_t x, y;
            if (!findPosition(monsterType, x, y)) {
                Assert(false);
                return;
            }

            // Create the monster object and initialize its stats.
            Monster* pMonster = new Monster(monsterType);
            Assert(pMonster != NULL);

            /////////////////////////////////////////////////////////////////////
            // Check whether it is an event monster at the point the monster is added.
            ///  Removed as of July 1 (the World Cup event ended)
            /////////////////////////////////////////////////////////////////////

            try {
                m_pZone->addCreature(pMonster, x, y, Directions(rand() % 8));
            } catch (EmptyTileNotExistException&) {
                SAFE_DELETE(pMonster);
            }
        }
    }

    if (de::gameContext().variables().isActiveChiefMonster() && m_pEventMonsterInfo != NULL) {
        Timeval currentTime;
        getCurrentTime(currentTime);


        for (uint i = 0; i < m_pEventMonsterInfo->size(); i++) {
            EventMonsterInfo& info = (*m_pEventMonsterInfo)[i];

            if (!info.bExist && currentTime >= info.regenTime) {
                MonsterType_t monsterType = info.monsterType;

                // Find an empty coordinate in the zone.
                ZoneCoord_t x, y;
                if (info.x != -1) {
                    x = info.x;
                    y = info.y;
                } else if (!findPosition(monsterType, x, y)) {
                    Assert(false);
                    return;
                }

                // Create the monster object and initialize its stats.
                Monster* pMonster = new Monster(monsterType);
                Assert(pMonster != NULL);

                pMonster->setEventMonsterIndex(i);

                try {
                    m_pZone->addCreature(pMonster, x, y, Directions(rand() % 8));

                    // Great Ruffian
                } catch (EmptyTileNotExistException&) {
                    SAFE_DELETE(pMonster);
                }
            }
        }
    }

    __END_DEBUG
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
bool MonsterManager::findPosition(MonsterType_t monsterType, ZoneCoord_t& RX, ZoneCoord_t& RY) const

{
    const MonsterInfo* pMonsterInfo = de::gameContext().monsterInfos().getMonsterInfo(monsterType);

    int count = 0;


    // Retry until a free position is found, giving up after 300 tries.
    while (true) {
        const BPOINT& pt = m_pZone->getRandomMonsterRegenPosition();

        Tile& rTile = m_pZone->getTile(pt.x, pt.y);

        // 1. the tile is not blocked,
        // 2. the tile has no portal, and
        // 3. it is not a safe zone
        if (!rTile.isBlocked(pMonsterInfo->getMoveMode()) && !rTile.hasPortal() &&
            !(m_pZone->getZoneLevel(pt.x, pt.y) & SAFE_ZONE)) {
            RX = pt.x;
            RY = pt.y;
            return true;
        }

        if (++count >= 300) {
            cerr << "MonsterManager::findPosition() : Max Count Exceeded" << endl;
            throw Error("MonsterManager::findPosition() : Max Count Exceeded");
        }
    }

    // The loop above never ends, so this should be unreachable.
    return false;
}

////////////////////////////////////////////////////////////////////////////////
// Handle a dead creature.
////////////////////////////////////////////////////////////////////////////////
void MonsterManager::killCreature(Creature* pDeadCreature)

{
    __BEGIN_DEBUG

    Assert(pDeadCreature->isDead());

    Zone* pZone = pDeadCreature->getZone();
    Assert(m_pZone == pZone);

    Monster* pDeadMonster = dynamic_cast<Monster*>(pDeadCreature);
    ZoneCoord_t cx = pDeadMonster->getX();
    ZoneCoord_t cy = pDeadMonster->getY();

    // It died, so the precedence is computed.
    PrecedenceTable* pTable = pDeadMonster->getPrecedenceTable();

    pTable->compute();

    if (pTable->getHostName() == "") {
        // If nobody hit it, no item is put in.
        pDeadMonster->setTreasure(false);
    } else {
        pDeadMonster->setHostName(pTable->getHostName());
        pDeadMonster->setHostPartyID(pTable->getHostPartyID());
    }

    // First send the effect that makes it fall to the ground.
    GCAddEffect gcAddEffect;
    gcAddEffect.setObjectID(pDeadCreature->getObjectID());
    gcAddEffect.setEffectID(Effect::EFFECT_CLASS_COMA);
    gcAddEffect.setDuration(0);
    pZone->broadcastPacket(cx, cy, &gcAddEffect);

    // Take the monster off the map. The manager entry is dropped by the caller.
    m_pZone->deleteCreatureFromTile(pDeadMonster, cx, cy);

    // Handling for a DynamicZone
    if (m_pZone->isDynamicZone()) {
        DynamicZone* pDynamicZone = m_pZone->getDynamicZone();
        Assert(pDynamicZone != NULL);

        if (pDynamicZone->getTemplateZoneID() == 4001) {
            // Handling for the entrance of the Alter
            DynamicZoneGateOfAlter* pGateOfAlter = dynamic_cast<DynamicZoneGateOfAlter*>(pDynamicZone);
            Assert(pGateOfAlter != NULL);

            pGateOfAlter->removeEffect(cx, cy);
        }
    }

    // A ground elemental leaves no corpse.
    if (pDeadMonster->getMonsterType() == GROUND_ELEMENTAL_TYPE) {
        GCDeleteObject* pGCDO = new GCDeleteObject;
        pGCDO->setObjectID(pDeadMonster->getObjectID());

        EffectPacketSend* pEffectPacketSend =
            new EffectPacketSend(pDeadMonster->getZone(), pDeadMonster->getX(), pDeadMonster->getY());
        pEffectPacketSend->setPacket(pGCDO);
        // The packet has to be sent one second later.
        pEffectPacketSend->setDeadline(10);
        pDeadMonster->getZone()->registerObject(pEffectPacketSend);
        pDeadMonster->getZone()->addEffect(pEffectPacketSend);

        SAFE_DELETE(pDeadMonster);
        return;
    } else if (pDeadMonster->getMonsterType() == 764) {
        // Great Ruffian
        GCDeleteObject* pGCDO = new GCDeleteObject;
        pGCDO->setObjectID(pDeadMonster->getObjectID());

        EffectPacketSend* pEffectPacketSend =
            new EffectPacketSend(pDeadMonster->getZone(), pDeadMonster->getX(), pDeadMonster->getY());
        pEffectPacketSend->setPacket(pGCDO);
        // The packet has to be sent one second later.
        pEffectPacketSend->setDeadline(10);
        pDeadMonster->getZone()->registerObject(pEffectPacketSend);
        pDeadMonster->getZone()->addEffect(pEffectPacketSend);

        Monster* pNewMonster = new Monster(765);
        pNewMonster->setClanType(pDeadMonster->getClanType());
        pNewMonster->setEventMonsterIndex(pDeadMonster->getEventMonsterIndex());
        pNewMonster->setName(pDeadMonster->getName());
        pDeadMonster->getZone()->addCreature(pNewMonster, pDeadMonster->getX(), pDeadMonster->getY(),
                                             pDeadMonster->getDir());

        SAFE_DELETE(pDeadMonster);
        return;
    } else if (pDeadMonster->getMonsterType() == 793 || pDeadMonster->getMonsterType() == 794 ||
               pDeadMonster->getMonsterType() == 795) {
        // Slayer, Vampire and Ousters offerings leave no corpse either;
        // they are removed immediately.
        GCDeleteObject gcDO;
        gcDO.setObjectID(pDeadMonster->getObjectID());
        pDeadMonster->getZone()->broadcastPacket(pDeadMonster->getX(), pDeadMonster->getY(), &gcDO);

        SAFE_DELETE(pDeadMonster);
        return;
    }

    // Create the corpse object and have an OID assigned to it.
    MonsterCorpse* pMonsterCorpse = new MonsterCorpse(pDeadMonster);
    pMonsterCorpse->setHostName(pDeadMonster->getHostName());
    pMonsterCorpse->setHostPartyID(pDeadMonster->getHostPartyID());
    pMonsterCorpse->setQuestHostName(pTable->getQuestHostName());
    pMonsterCorpse->setLevel((int)(pDeadMonster->getLevel()));
    pMonsterCorpse->setExp((Exp_t)computeCreatureExp(pDeadMonster, 100));
    pMonsterCorpse->setLastKiller(pDeadMonster->getLastKiller());

    // Add items to the corpse according to the kind of monster that died.
    addItem(pDeadMonster, pMonsterCorpse);

    // by sigi. 2002.12.12
    addCorpseToZone(pMonsterCorpse, m_pZone, cx, cy);

    if (pDeadMonster->isFlag(Effect::EFFECT_CLASS_HARPOON_BOMB)) {
        EffectHarpoonBomb* pEffect =
            dynamic_cast<EffectHarpoonBomb*>(pDeadMonster->findEffect(Effect::EFFECT_CLASS_HARPOON_BOMB));
        if (pEffect != NULL) {
            EffectHarpoonBomb* pZoneEffect =
                new EffectHarpoonBomb(m_pZone, pMonsterCorpse->getX(), pMonsterCorpse->getY());
            pZoneEffect->setDamage(pEffect->getDamage());
            pZoneEffect->setUserObjectID(pEffect->getUserObjectID());
            pZoneEffect->setNextTime(pEffect->getNextTime());
            pZoneEffect->setDeadline(pEffect->getRemainDuration());
            pEffect->setDeadline(0);
            m_pZone->registerObject(pZoneEffect);
            m_pZone->getTile(pMonsterCorpse->getX(), pMonsterCorpse->getY()).addEffect(pZoneEffect);
            m_pZone->addEffect(pZoneEffect);
        }
    }

    // Tell the surroundings that the creature died.
    GCCreatureDied gcCreatureDied;
    gcCreatureDied.setObjectID(pDeadMonster->getObjectID());
    m_pZone->broadcastPacket(cx, cy, &gcCreatureDied);

    // A master says a line as it dies.
    if (pDeadMonster->isMaster()) {
        MasterLairInfo* pMasterLairInfo = de::gameContext().masterLairInfos().getMasterLairInfo(pZone->getZoneID());

        if (pMasterLairInfo != NULL && pMasterLairInfo->getMasterMonsterType() == pDeadMonster->getMonsterType()) {
            GCSay gcSay;
            gcSay.setObjectID(pDeadMonster->getObjectID());
            gcSay.setColor(MASTER_SAY_COLOR);

            if (pDeadMonster->getLastHitCreatureClass() == Creature::CREATURE_CLASS_SLAYER) {
                gcSay.setMessage(pMasterLairInfo->getRandomMasterDeadSlayerSay());
            } else {
                gcSay.setMessage(pMasterLairInfo->getRandomMasterDeadVampireSay());
            }

            if (!gcSay.getMessage().empty())
                pZone->broadcastPacket(cx, cy, &gcSay);
        }
    }

    if (pDeadMonster->getMonsterType() == 717) {
        GCSay gcSay;
        gcSay.setObjectID(pDeadMonster->getObjectID());
        gcSay.setColor(MASTER_SAY_COLOR);

        gcSay.setMessage(de::gameContext().strings().getString(345));
        cout << gcSay.getMessage() << endl;
        pZone->broadcastPacket(cx, cy, &gcSay);
    } else if (pDeadMonster->getMonsterType() == 723) {
        GCSay gcSay;
        gcSay.setObjectID(pDeadMonster->getObjectID());
        gcSay.setColor(MASTER_SAY_COLOR);

        gcSay.setMessage(de::gameContext().strings().getString(360));
        cout << gcSay.getMessage() << endl;
        pZone->broadcastPacket(cx, cy, &gcSay);
    }

    // Delete the creature.
    SAFE_DELETE(pDeadMonster);

    __END_DEBUG
}

////////////////////////////////////////////////////////////////////////////////
// addCreature
//
// Near (x, y),
// add num Monsters of monsterType.
////////////////////////////////////////////////////////////////////////////////
void MonsterManager::addMonsters(ZoneCoord_t x, ZoneCoord_t y, MonsterType_t monsterType, int num,
                                 const SUMMON_INFO& summonInfo, list<Monster*>* pSummonedMonsters) {
    TPOINT pt;

    ClanType_t clanType = CLAN_VAMPIRE_MONSTER; // default

    // The whole group is the same clan.
    if (summonInfo.clanType == SUMMON_INFO::CLAN_TYPE_RANDOM_GROUP ||
        summonInfo.clanType == SUMMON_INFO::CLAN_TYPE_GROUP) {
        clanType = summonInfo.clanID; // rand()%90+2;
    }

    // Find an empty coordinate in the zone.
    for (int i = 0; i < num; i++) {
        pt = findSuitablePosition(m_pZone, x, y, Creature::MOVE_MODE_WALKING);

        // Cannot add if no position was found or it is a safe zone.
        if (pt.x == -1 || (m_pZone->getZoneLevel(pt.x, pt.y) & SAFE_ZONE)) {
            return;
        }

        Monster* pMonster = NULL;

        // Create the monster object and initialize its stats.
        try {
            pMonster = new Monster(monsterType);

            // Does the summoned monster carry an item?
            pMonster->setTreasure(summonInfo.hasItem);

            ////////////////////////////////////////////////////////////////////////////////
            // Check whether it is an event monster at the point the monster is added.
            //  The soccer ball no longer drops now that the July 1 event has ended.
            ///////////////////////////////////////////////////////////////////////////

            Assert(pMonster != NULL);

            if (summonInfo.regenType == REGENTYPE_PORTAL) {
                // Remove anything that may already have been set.
                pMonster->removeFlag(Effect::EFFECT_CLASS_HIDE);
                pMonster->removeFlag(Effect::EFFECT_CLASS_INVISIBILITY);
                pMonster->removeFlag(Effect::EFFECT_CLASS_TRANSFORM_TO_BAT);

                pMonster->setFlag(Effect::EFFECT_CLASS_VAMPIRE_PORTAL);
                pMonster->setMoveMode(Creature::MOVE_MODE_WALKING);
            }

            if (summonInfo.initHPPercent != 0) {
                int currentHP = pMonster->getHP(ATTR_CURRENT);
                int MaxHP = currentHP * 100 / summonInfo.initHPPercent;
                pMonster->setHP(MaxHP, ATTR_MAX);
            }

        } catch (OutOfBoundException& t) {
            filelog("MonsterManagerBug.log", "addMonsters : %s", t.toString().c_str());
            SAFE_DELETE(pMonster);
            return;
        } catch (NoSuchElementException& t) {
            filelog("MonsterManagerBug.log", "addMonsters : %s", t.toString().c_str());
            SAFE_DELETE(pMonster);
            return;
        }


        try {
            m_pZone->addCreature(pMonster, pt.x, pt.y, Directions(rand() % 8));


            // SUMMON_INFO
            if (summonInfo.clanType == SUMMON_INFO::CLAN_TYPE_RANDOM_EACH) {
                pMonster->setClanType(rand() % 90 + 2);
            } else {
                pMonster->setClanType(clanType);
            }


            //
            if (summonInfo.canScanEnemy) {
                pMonster->setScanEnemy();

                m_pZone->monsterScan(pMonster, pt.x, pt.y, pMonster->getDir());
            } else if (summonInfo.scanEnemy) {
                m_pZone->monsterScan(pMonster, pt.x, pt.y, pMonster->getDir());
            }

            if (pSummonedMonsters != NULL) {
                pSummonedMonsters->push_back(pMonster);
            }

        } catch (EmptyTileNotExistException&) {
            SAFE_DELETE(pMonster);
        }
    }
}
////////////////////////////////////////////////////////////////////////////////
// Generate items from the dead monster.
////////////////////////////////////////////////////////////////////////////////
void MonsterManager::addItem(Monster* pDeadMonster, MonsterCorpse* pMonsterCorpse)

{
    ItemFactoryManager& itemFactories = de::gameContext().itemFactories();
    VariableManager& variables = de::gameContext().variables();

    if (pDeadMonster->getMonsterType() == 734) {
        if (pDeadMonster->getZoneID() >= 1500 && pDeadMonster->getZoneID() <= 1506) {
            ItemType_t iType = pDeadMonster->getZoneID() - min((int)pDeadMonster->getZoneID(), 1501);
            Item* pItem = itemFactories.createItem(Item::ITEM_CLASS_CASTLE_SYMBOL, iType, list<OptionType_t>());
            pMonsterCorpse->setZone(pDeadMonster->getZone());
            pMonsterCorpse->addTreasure(pItem);
        }
    }

    // Add the quest item.
    if (pDeadMonster->getQuestItem() != NULL) {
        pMonsterCorpse->addTreasure(pDeadMonster->getQuestItem());
        pDeadMonster->setQuestItem(NULL);
    }

    if (variables.getVariable(PREMIUM_TRIAL_EVENT) != 0 && pDeadMonster->getMonsterType() == 705) {
        if (rand() % 100 < 30) {
            int Num = 5 + (rand() % 5); // 5~9
            for (int i = 0; i < Num; ++i) {
                Item* pItem = itemFactories.createItem(Item::ITEM_CLASS_LUCKY_BAG, 3, list<OptionType_t>());
                pMonsterCorpse->addTreasure(pItem);
            }
        }
    }

    // Check for monsters that drop no items (that is, master-summoned monsters).
    // by sigi. 2002.9.2
    if (!pDeadMonster->hasTreasure())
        return;

    MonsterType_t MonsterType = pDeadMonster->getMonsterType();
    const MonsterInfo* pMonsterInfo = de::gameContext().monsterInfos().getMonsterInfo(MonsterType);
    TreasureList* pTreasureList = NULL;

    //----------------------------------------------------------------------
    // 2002 Chuseok event item
    // Once the songpyeon has dropped at its probability, no other item may drop.
    //----------------------------------------------------------------------
    bool isHarvestFestivalItemAppeared = false;
    int PartialSumOfCakeRatio = 0;
    int itemBonusPercent = 0;

    if (variables.getHarvestFestivalItemRatio() > 0 && rand() % variables.getHarvestFestivalItemRatio() == 0) {
        // The item can be one of five kinds.
        ITEM_TEMPLATE ricecake_template;
        ricecake_template.NextOptionRatio = 0;

        bool bOK = false;
        int EventSelector = rand() % m_SumOfCakeRatio;

        for (int i = 0; i < 5; i++) {
            PartialSumOfCakeRatio += m_RICE_CAKE_PROB_RATIO[i];

            // If the dice method applies
            if (EventSelector < PartialSumOfCakeRatio) {
                if (i == 0) {
                    if (pDeadMonster->getLastHitCreatureClass() == Creature::CREATURE_CLASS_SLAYER) {
                        ricecake_template.ItemClass = Item::ITEM_CLASS_POTION;
                        ricecake_template.ItemType = 11;
                    } else {
                        ricecake_template.ItemClass = Item::ITEM_CLASS_SERUM;
                        ricecake_template.ItemType = 5;
                    }
                } else {
                    // Star
                    ricecake_template.ItemClass = Item::ITEM_CLASS_EVENT_STAR;
                    ricecake_template.ItemType = i + 7;
                }
                bOK = true;
                break;
            }
        }


        if (bOK) {
            Item* pItem = itemFactories.createItem(ricecake_template.ItemClass, ricecake_template.ItemType,
                                                   ricecake_template.OptionType);

            Assert(pItem != NULL);

            pMonsterCorpse->addTreasure(pItem);

            isHarvestFestivalItemAppeared = true;
        }
    }

    //----------------------------------------------------------------------
    // Add the Christmas firecracker.
    //----------------------------------------------------------------------
    int fireCrackerRatio = variables.getVariable(CHRISTMAS_FIRE_CRACKER_RATIO);
    if (fireCrackerRatio > 0) {
        int value = rand() % 10000;
        if (value < fireCrackerRatio) {
            // Three kinds of firecracker can drop.
            ItemType_t fireCrackerType = value % 3;

            // Create the item.
            list<OptionType_t> optionType;
            Item* pItem = itemFactories.createItem(Item::ITEM_CLASS_EVENT_ETC, fireCrackerType, optionType);

            // Put it into the monster corpse.
            pMonsterCorpse->addTreasure(pItem);
        }
    }

    //----------------------------------------------------------------------
    // Add the Christmas tree part.
    //----------------------------------------------------------------------
    int treePartRatio = variables.getVariable(CHRISTMAS_TREE_PART_RATIO);
    if (treePartRatio > 0) {
        int value = rand() % 10000;
        if (value < treePartRatio) {
            // There are 12 tree parts.
            ItemType_t treeItemType = rand() % 12;

            // Create the item.
            list<OptionType_t> optionType;
            Item* pItem = itemFactories.createItem(Item::ITEM_CLASS_EVENT_TREE, treeItemType, optionType);

            // Put it into the monster corpse.
            pMonsterCorpse->addTreasure(pItem);
        }
    }
    int value = 0;
    int PayZonecheckIn = 0;
    ItemType_t treeItemType = 3;
    if (pDeadMonster->getMonsterType() == 765) {
        treeItemType = 6;
        if (pDeadMonster->getZoneID() >= 1402) {
            value = rand() % 250;
            PayZonecheckIn = 50;
            if (value < PayZonecheckIn) {
                list<OptionType_t> optionType;
                Item* pItem = itemFactories.createItem(Item::ITEM_CLASS_EVENT_STAR, treeItemType, optionType);
                pMonsterCorpse->addTreasure(pItem);
            }
        }
        treeItemType = 3;
        for (int l = 0; l < 3; l++) {
            value = rand() % 10000;
            PayZonecheckIn = 3000;
            if (value < PayZonecheckIn) {
                list<OptionType_t> optionType;
                Item* pItem = itemFactories.createItem(Item::ITEM_CLASS_MOON_CARD, treeItemType, optionType);
                pMonsterCorpse->addTreasure(pItem);
            }
        }
    }
    // end

    //----------------------------------------------------------------------
    // Add the green gift box.
    //----------------------------------------------------------------------
    int giftBoxRatio = variables.getVariable(CHRISTMAS_GIFT_BOX_RATIO);
    if (giftBoxRatio > 0) {
        int value = rand() % 10000;
        if (value < giftBoxRatio) {
            // Create the green gift box.
            list<OptionType_t> optionType;
            Item* pItem = itemFactories.createItem(Item::ITEM_CLASS_EVENT_GIFT_BOX, 0, optionType);

            // Put it into the monster corpse.
            pMonsterCorpse->addTreasure(pItem);
        }
    }

    //----------------------------------------------------------------------
    // Add the gift box.
    //----------------------------------------------------------------------
    // The lucky pouch is handled here through affectKillCount, and the gift box
    // is handled here too because it must not go into Monster's m_pQuestItem.
    // (The lucky pouch does not really need to go into m_pQuestItem either.)
    //----------------------------------------------------------------------
    if (variables.isEventGiftBox()) {
        if (m_pZone != NULL) {
            Creature* pCreature = m_pZone->getCreature(pDeadMonster->getLastKiller());

            if (pCreature != NULL && pCreature->isPC()) {
                PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pCreature);

                if (pPC != NULL) {
                    Item* pItem = getGiftBoxItem(getGiftBoxKind(pPC, pDeadMonster));

                    // Add the GiftBox item if one has to be added.
                    if (pItem != NULL)
                        pMonsterCorpse->addTreasure(pItem);
                }
            }
        }
    }


    // Follow the race of the character that should pick the item up.
    // If that character is not in the current zone,
    // follow that character's party, and if there is no party,
    // follow LastHit.
    // by sigi. 2002.10.14
    // If the creature that last hit this monster is a slayer, create slayer items;
    // otherwise create vampire items by default.
    Creature* pItemOwnerCreature = m_pZone->getPCManager()->getCreature(pDeadMonster->getHostName());

    Creature::CreatureClass ownerCreatureClass;

    int luckLevel = 0;
    if (pItemOwnerCreature != NULL) {
        ownerCreatureClass = pItemOwnerCreature->getCreatureClass();
        luckLevel = pItemOwnerCreature->getLuck();

        GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(pItemOwnerCreature->getPlayer());

        if (pGamePlayer != NULL)
            itemBonusPercent = pGamePlayer->getItemRatioBonusPoint();
    } else if (pDeadMonster->getHostPartyID() != 0) {
        Party* pParty = m_pZone->getLocalPartyManager()->getParty(pDeadMonster->getHostPartyID());

        if (pParty != NULL) {
            ownerCreatureClass = pParty->getCreatureClass();
        } else {
            ownerCreatureClass = pDeadMonster->getLastHitCreatureClass();
        }
    } else {
        ownerCreatureClass = pDeadMonster->getLastHitCreatureClass();
    }

    // Decide the item's race from the race of the item's owner.
    if (ownerCreatureClass == Creature::CREATURE_CLASS_SLAYER) {
        pTreasureList = pMonsterInfo->getSlayerTreasureList();
    } else if (ownerCreatureClass == Creature::CREATURE_CLASS_VAMPIRE) {
        pTreasureList = pMonsterInfo->getVampireTreasureList();
    } else if (ownerCreatureClass == Creature::CREATURE_CLASS_OUSTERS) {
        pTreasureList = pMonsterInfo->getOustersTreasureList();
    }

    // Is this monster a chief monster?
    bool bChiefMonsterBonus = pDeadMonster->isChief() && variables.isActiveChiefMonster();


    if (pTreasureList != NULL) {
        const list<Treasure*>& treasures = pTreasureList->getTreasures();

        list<Treasure*>::const_iterator itr = treasures.begin();
        for (; itr != treasures.end(); itr++) {
            Treasure* pTreasure = (*itr);
            ITEM_TEMPLATE it;

            it.ItemClass = Item::ITEM_CLASS_MAX;
            it.ItemType = 0;

            int itemRatioBonus = 0;

            if (bChiefMonsterBonus) {
                it.NextOptionRatio = variables.getChiefMonsterRareItemPercent();
                itemRatioBonus = variables.getPremiumItemProbePercent();
            } else {
                it.NextOptionRatio = 0;
            }

            Item* pItem = NULL;

            // Item probability is doubled in pay zones.
            Zone* pZone = pDeadMonster->getZone();

            // Zone the jackpot event applies to.
            static bool isNetMarble = de::isNetMarbleDeployment();
            bool isLottoZone = pZone->isPayPlay() || isNetMarble;

            if (pZone->isPayPlay() || pZone->isPremiumZone()) {
                if (pDeadMonster->getZoneID() == 1013) // If the current zone is the configured treasure-drop map
                {
                    pTreasure->setRndItemOptionMax(3);
                } else
                    pTreasure->setRndItemOptionMax(2);
                if (pTreasure->getRandomItem(&it, itemRatioBonus + variables.getPremiumItemProbePercent() +
                                                      itemBonusPercent)) {
                    // by sigi. 2002.10.21
                    int upgradeLevel = upgradeItemTypeByLuck(luckLevel, ownerCreatureClass, it);
                    if (upgradeLevel != 0) {
                        GCAddEffectToTile gcAE;

                        if (upgradeLevel > 0)
                            gcAE.setEffectID(Effect::EFFECT_CLASS_LUCKY);
                        else
                            gcAE.setEffectID(Effect::EFFECT_CLASS_MISFORTUNE);

                        gcAE.setObjectID(0);
                        gcAE.setDuration(0);
                        gcAE.setXY(pDeadMonster->getX(), pDeadMonster->getY());

                        PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pItemOwnerCreature);
                        if (pPC != NULL) {
                            if (canGiveEventItem(pPC, pDeadMonster) &&
                                !GDRLairManager::Instance().isGDRLairZone(pPC->getZoneID())) {
                                if (upgradeLevel > 0)
                                    addOlympicStat(pPC, 5);
                                else
                                    addOlympicStat(pPC, 6);
                            }
                        }


                        pZone->broadcastPacket(pDeadMonster->getX(), pDeadMonster->getY(), &gcAE);
                    }

                    if (!it.OptionType.empty()) {
                        upgradeOptionByLuck(luckLevel, ownerCreatureClass, it);
                    }

                    // A chief monster raises the item by one grade.
                    // by sigi. 2002.10.23
                    if (bChiefMonsterBonus
                        // For now this uses the same probability as rare items.
                        // Later it should be split out into a separate variable.
                        && rand() % 100 < variables.getChiefMonsterRareItemPercent() &&
                        isPossibleUpgradeItemType(it.ItemClass)) {
                        // Upgrade the ItemType by one grade.
                        int upgradeCount = 1;

                        it.ItemType = getUpgradeItemType(it.ItemClass, it.ItemType, upgradeCount);
                    }

                    pItem = itemFactories.createItem(it.ItemClass, it.ItemType, it.OptionType);
                    Assert(pItem != NULL);
                    if (pItem->getItemClass() == Item::ITEM_CLASS_RESURRECT_ITEM)
                        countResurrectItem();

                    if (pItem->isUnique())
                        pItem->setGrade(6);
                    else
                        pItem->setGrade(ItemGradeManager::Instance().getRandomGrade());

                    pItem->setDurability(computeMaxDurability(pItem));

                    if (!isHarvestFestivalItemAppeared ||
                        (isHarvestFestivalItemAppeared && pItem->getItemClass() == Item::ITEM_CLASS_SKULL))
                        pMonsterCorpse->addTreasure(pItem);

                    // Jackpot event: add 8 more skulls.
                    if (isLottoZone && pItem->getItemClass() == Item::ITEM_CLASS_SKULL) {
                        int lottoSkullRatio = variables.getVariable(LOTTO_SKULL_RATIO);
                        if (lottoSkullRatio > 0) {
                            int value = rand() % 10000;
                            if (value < lottoSkullRatio) {
                                // Create and add 8 more skulls.
                                for (int i = 0; i < 8; i++) {
                                    pItem = itemFactories.createItem(it.ItemClass, it.ItemType, it.OptionType);
                                    pMonsterCorpse->addTreasure(pItem);
                                    if (pItem->getItemClass() == Item::ITEM_CLASS_RESURRECT_ITEM)
                                        countResurrectItem();
                                }
                            }
                        }
                    }
                }
            } else {
                if (pTreasure->getRandomItem(&it, variables.getItemProbRatio() + itemBonusPercent)) {
                    pItem = itemFactories.createItem(it.ItemClass, it.ItemType, it.OptionType);
                    Assert(pItem != NULL);
                    if (pItem->getItemClass() == Item::ITEM_CLASS_RESURRECT_ITEM)
                        countResurrectItem();

                    if (pItem->isUnique())
                        pItem->setGrade(6);
                    else
                        pItem->setGrade(ItemGradeManager::Instance().getRandomGrade());

                    pItem->setDurability(computeMaxDurability(pItem));

                    if (!isHarvestFestivalItemAppeared ||
                        (isHarvestFestivalItemAppeared && pItem->getItemClass() == Item::ITEM_CLASS_SKULL))
                        pMonsterCorpse->addTreasure(pItem);
                }
            }


            /////////////////////////////////////////////////////////////////////////
            // If an item other than a skull dropped and the jackpot chance hit, add a few more. Pay zones only.
            // If the chief monster has extra items configured, add that many more items.
            int nBonusItem = 0;

            if (pItem != NULL && pItem->getItemClass() != Item::ITEM_CLASS_SKULL) {
                if (bChiefMonsterBonus)
                    nBonusItem = variables.getVariable(CHIEF_ITEM_BONUS_NUM);

                if (isLottoZone && isLottoWinning())
                    nBonusItem = variables.getVariable(LOTTO_ITEM_BONUS_NUM);

                if (pDeadMonster->getMonsterType() == 765)
                    nBonusItem = 8;
                // add by sonic 2006.10.30
                if (pDeadMonster->getZoneID() == 1013 && pDeadMonster->getMonsterType() != 765 && nBonusItem == 0)
                    nBonusItem = 2;
                // end by sonic 2006.10.31
            }

            if (nBonusItem > 0) {
                int i = 0;
                int j = 0;
                static int MaxTry = 30;
                while (i < nBonusItem && j < MaxTry) {
                    Treasure* pTreasure = (*itr);
                    ITEM_TEMPLATE it;

                    it.ItemClass = Item::ITEM_CLASS_MAX;
                    it.ItemType = 0;

                    int itemRatioBonus = 0;

                    if (bChiefMonsterBonus || pDeadMonster->getMonsterType() == 765 ||
                        pDeadMonster->getZoneID() == 1013) //  add by sonic 2006.101.
                    {
                        it.NextOptionRatio = variables.getChiefMonsterRareItemPercent();
                        itemRatioBonus = variables.getPremiumItemProbePercent();
                    } else {
                        it.NextOptionRatio = 0;
                    }

                    Item* pItem = NULL;

                    // Item probability is doubled in pay zones.
                    Zone* pZone = pDeadMonster->getZone();
                    if (pZone->isPayPlay() || pZone->isPremiumZone()) {
                        if (pTreasure->getRandomItem(&it, itemRatioBonus + variables.getPremiumItemProbePercent() +
                                                              itemBonusPercent)) {
                            // by sigi. 2002.10.21
                            int upgradeLevel = upgradeItemTypeByLuck(luckLevel, ownerCreatureClass, it);

                            if (upgradeLevel != 0) {
                                GCAddEffectToTile gcAE;

                                if (upgradeLevel > 0)
                                    gcAE.setEffectID(Effect::EFFECT_CLASS_LUCKY);
                                else
                                    gcAE.setEffectID(Effect::EFFECT_CLASS_MISFORTUNE);

                                gcAE.setObjectID(0);
                                gcAE.setDuration(0);
                                gcAE.setXY(pDeadMonster->getX(), pDeadMonster->getY());

                                PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pItemOwnerCreature);
                                if (pPC != NULL) {
                                    if (canGiveEventItem(pPC, pDeadMonster) &&
                                        !GDRLairManager::Instance().isGDRLairZone(pPC->getZoneID())) {
                                        if (upgradeLevel > 0)
                                            addOlympicStat(pPC, 5);
                                        else
                                            addOlympicStat(pPC, 6);
                                    }
                                }


                                pZone->broadcastPacket(pDeadMonster->getX(), pDeadMonster->getY(), &gcAE);
                            }

                            // A chief monster raises the item by one grade.
                            // by sigi. 2002.10.23
                            if (bChiefMonsterBonus
                                // For now this uses the same probability as rare items.
                                // Later it should be split out into a separate variable.
                                && rand() % 100 < variables.getChiefMonsterRareItemPercent() &&
                                isPossibleUpgradeItemType(it.ItemClass)) {
                                // Upgrade the ItemType by one grade.
                                int upgradeCount = 1;

                                it.ItemType = getUpgradeItemType(it.ItemClass, it.ItemType, upgradeCount);
                            }

                            if (!it.OptionType.empty()) {
                                // add by sonic 2006.10.31
                                if (pDeadMonster->getZoneID() == 1013 && pDeadMonster->getMonsterType() != 765) {
                                    upgradeOptionByLuck(1, ownerCreatureClass, it);
                                } else {
                                    upgradeOptionByLuck(luckLevel, ownerCreatureClass, it);
                                }
                                // end
                            }

                            pItem = itemFactories.createItem(it.ItemClass, it.ItemType, it.OptionType);
                            Assert(pItem != NULL);
                            if (pItem->getItemClass() == Item::ITEM_CLASS_RESURRECT_ITEM)
                                countResurrectItem();

                            if (pItem->isUnique())
                                pItem->setGrade(6);
                            else
                                pItem->setGrade(ItemGradeManager::Instance().getRandomGrade());

                            pItem->setDurability(computeMaxDurability(pItem));

                            if (!isHarvestFestivalItemAppeared ||
                                (isHarvestFestivalItemAppeared && pItem->getItemClass() == Item::ITEM_CLASS_SKULL))
                                pMonsterCorpse->addTreasure(pItem);
                        }
                    } else {
                        if (pTreasure->getRandomItem(&it, variables.getItemProbRatio() + itemBonusPercent)) {
                            pItem = itemFactories.createItem(it.ItemClass, it.ItemType, it.OptionType);
                            Assert(pItem != NULL);
                            if (pItem->getItemClass() == Item::ITEM_CLASS_RESURRECT_ITEM)
                                countResurrectItem();

                            if (pItem->isUnique())
                                pItem->setGrade(6);
                            else
                                pItem->setGrade(ItemGradeManager::Instance().getRandomGrade());

                            pItem->setDurability(computeMaxDurability(pItem));

                            if (!isHarvestFestivalItemAppeared ||
                                (isHarvestFestivalItemAppeared && pItem->getItemClass() == Item::ITEM_CLASS_SKULL))
                                pMonsterCorpse->addTreasure(pItem);
                        }
                    }

                    if (pItem != NULL)
                        i++;

                    j++;
                }
            }
            /////////////////////////////////////////////////////////////////////////
            /////////////////////////////////////////////////////////////////////////
        }
    }

    // Monsters that must be given a skull.
    if (pMonsterInfo->getSkullType() != 0) {
        Item* pSkull =
            itemFactories.createItem(Item::ITEM_CLASS_SKULL, pMonsterInfo->getSkullType(), list<OptionType_t>());
        if (pSkull != NULL) {
            pMonsterCorpse->addTreasure(pSkull);
        }
    }

    //////////////////////////////////////////////////////////////////////
    // June 2002 World Cup event
    //  A monster set up as an event monster drops the soccer ball item.
    //  The soccer ball has no ITEM_TYPE of its own and works as
    //  Type 7 of EVENT_STAR.
    //  EventStarInfo and EventStarObject should later be renamed to
    //  EventItemInfo and EventItemObject.
    /////////////////////////////////////////////////////////////////////


    //////////////////////////////////////////////////////////////////////
    //   May 2002 Family Month event
    //   A star can drop from any monster, so it is hardcoded here.
    //   A star item is additionally created with a 1/1500 probability.
    //////////////////////////////////////////////////////////////////////
}

int MonsterManager::upgradeItemTypeByLuck(int luckLevel, Creature::CreatureClass ownerCreatureClass, ITEM_TEMPLATE& it)

{
    if (luckLevel == 0 || !isPossibleUpgradeItemType(it.ItemClass))
        return 0;

    luckLevel = luckLevel + (rand() % 20) - 10;
    luckLevel = min(MAX_LUCK_LEVEL, luckLevel);

    int ratio;

    switch (ownerCreatureClass) {
    case Creature::CREATURE_CLASS_SLAYER: {
        if (luckLevel >= 0) {
            ratio = (int)(((float)luckLevel / (4.254 + (1.0 + it.ItemType) / 5.0)) * 100);
        } else {
            ratio = (int)(((float)luckLevel / (2.5 - (1.0 + it.ItemType) / 20.0)) * 100);
        }
    } break;
    case Creature::CREATURE_CLASS_VAMPIRE: {
        if (luckLevel >= 0) {
            ratio = (int)(((float)luckLevel / (6.03 + (1.0 + it.ItemType) / 5.0)) * 100);
        } else {
            ratio = (int)(((float)luckLevel / (4.14 - (1.0 + it.ItemType) / 20.0)) * 100);
        }
    } break;
    case Creature::CREATURE_CLASS_OUSTERS: {
        if (luckLevel >= 0) {
            ratio = (int)(((float)luckLevel / (4.936 + (1.0 + it.ItemType) / 5.0)) * 100);
        } else {
            ratio = (int)(((float)luckLevel / (3.05 - (1.0 + it.ItemType) / 20.0)) * 100);
        }
    } break;
    default:
        return 0;
    }

    int value = rand() % 10000;


    if (ratio > 0 && value < ratio) {
        it.ItemType = getUpgradeItemType(it.ItemClass, it.ItemType, 1);
        return 1;
    } else if (ratio < 0 && value < (-ratio)) {
        it.ItemType = getDowngradeItemType(it.ItemClass, it.ItemType);
        return -1;
    }

    return 0;
}

int MonsterManager::upgradeOptionByLuck(int luckLevel, Creature::CreatureClass ownerCreatureClass, ITEM_TEMPLATE& it) {
    if (it.OptionType.empty())
        return 0;

    OptionType_t optionType = it.OptionType.front();
    OptionInfo* pOptionInfo = de::gameContext().optionInfos().getOptionInfo(optionType);
    if (pOptionInfo == NULL)
        return 0;

    luckLevel = luckLevel + (rand() % 20) - 10;
    luckLevel = min(MAX_LUCK_LEVEL, luckLevel);

    int grade = pOptionInfo->getGrade() + 1;

    int ratio;

    switch (ownerCreatureClass) {
    case Creature::CREATURE_CLASS_SLAYER: {
        if (luckLevel >= 0) {
            ratio = (int)(((float)luckLevel / (grade * 25.0 - 15.2)) * 100);
        } else {
            ratio = (int)(((float)luckLevel / (7.5 - grade / 2.0)) * 100);
        }
    } break;
    case Creature::CREATURE_CLASS_VAMPIRE: {
        if (luckLevel >= 0) {
            ratio = (int)(((float)luckLevel / (grade * 25.0 - 11.3)) * 100);
        } else {
            ratio = (int)(((float)luckLevel / (10.3 - grade / 2.0)) * 100);
        }
    } break;
    case Creature::CREATURE_CLASS_OUSTERS: {
        if (luckLevel >= 0) {
            ratio = (int)(((float)luckLevel / (grade / 25.0 - 13.7)) * 100);
        } else {
            ratio = (int)(((float)luckLevel / (7.9 - grade / 2.0)) * 100);
        }
    } break;
    default:
        return 0;
    }

    int value = rand() % 10000;


    if (ratio > 0 && value < ratio && pOptionInfo->getUpgradeType() != optionType && pOptionInfo->isUpgradePossible()) {
        (*it.OptionType.begin()) = pOptionInfo->getUpgradeType();
        return 1;
    } else if (ratio < 0 && value < (-ratio) && pOptionInfo->getPreviousType() != optionType) {
        if (pOptionInfo->getPreviousType() != 0)
            (*it.OptionType.begin()) = pOptionInfo->getPreviousType();
        else
            it.OptionType.pop_front();
        return -1;
    }

    return 0;
}

////////////////////////////////////////////////////////////////////////////////
// Remove every creature.
////////////////////////////////////////////////////////////////////////////////
void MonsterManager::deleteAllMonsters(bool bDeleteFromZone)

{
    __BEGIN_DEBUG

    unordered_map<ObjectID_t, Creature*>::iterator current = m_Creatures.begin();

    while (current != m_Creatures.end()) {
        Creature* pCreature = current->second;

        Assert(pCreature != NULL);

        if (bDeleteFromZone) {
            try {
                Zone* pZone = pCreature->getZone();
                Assert(m_pZone == pZone);


                ZoneCoord_t cx = pCreature->getX();
                ZoneCoord_t cy = pCreature->getY();

                // Take the monster off the map. The manager is emptied below.
                m_pZone->deleteCreatureFromTile(pCreature, cx, cy);

                // Broadcast to nearby PCs that the creature has disappeared.
                GCDeleteObject gcDeleteObject(pCreature->getObjectID());
                pZone->broadcastPacket(cx, cy, &gcDeleteObject, pCreature);

            } catch (Throwable& t) {
                filelog("MonsterManagerBug.txt", "deleteAllCreatures: %s", t.toString().c_str());
            }
        }

        // Delete the creature.
        SAFE_DELETE(pCreature);

        current++;
    }

    // Remove everything.
    m_Creatures.clear();
    m_Monsters.clear();


    __END_DEBUG
}

////////////////////////////////////////////////////////////////////////////////
// Kill every creature.
////////////////////////////////////////////////////////////////////////////////
void MonsterManager::killAllMonsters(const unordered_map<ObjectID_t, ObjectID_t>& exceptCreatures)

{
    __BEGIN_DEBUG

    unordered_map<ObjectID_t, Creature*>::iterator current = m_Creatures.begin();

    while (current != m_Creatures.end()) {
        Creature* pCreature = current->second;

        Assert(pCreature != NULL);

        if (pCreature->isAlive()) {
            if (pCreature->isMonster()) {
                unordered_map<ObjectID_t, ObjectID_t>::const_iterator itr =
                    exceptCreatures.find(pCreature->getObjectID());

                if (itr == exceptCreatures.end()) {
                    Monster* pMonster = dynamic_cast<Monster*>(pCreature);
                    pMonster->setHP(0, ATTR_CURRENT);
                }
            } else
                Assert(false);
        }
    }

    __END_DEBUG
}

////////////////////////////////////////////////////////////////////////////////
// get debug string
////////////////////////////////////////////////////////////////////////////////
string MonsterManager::toString() const

{
    StringStream msg;
    msg << "MonsterManager(" << CreatureManager::toString();

    unordered_map<SpriteType_t, MonsterCounter*>::const_iterator itr = m_Monsters.begin();
    for (; itr != m_Monsters.end(); itr++)
        msg << itr->second->toString();

    msg << ")";
    return msg.toString();
}


//////////////////////////////////////////////////////////////////////////////
// Part of the code that was used for the golden skull collecting event.
// It is kept at the very end of the file, out of the way, in case it is
// ever needed again.
//////////////////////////////////////////////////////////////////////////////


bool isLottoWinning() {
    int lottoItemRatio = de::gameContext().variables().getVariable(LOTTO_ITEM_RATIO);
    if (lottoItemRatio > 0) {
        int value = rand() % 10000;
        if (value < lottoItemRatio) {
            return true;
        }
    }

    return false;
}
