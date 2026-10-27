//////////////////////////////////////////////////////////////////////////////
// FileName 	: Zone.h
// Written By	: Reiot
// Description	:
//////////////////////////////////////////////////////////////////////////////

#ifndef __ZONE_H__
#define __ZONE_H__

#include <queue>
#include <vector>

#include <unordered_map>

#include "Effect.h"
#include "Encrypter.h"
#include "Exception.h"
#include "Mutex.h"
#include "ObjectRegistry.h"
#include "PCManager.h"
#include "Party.h"
#include "Sector.h"
#include "Tile.h"
#include "Types.h"

//////////////////////////////////////////////////////////////////////////////
// forward declaration
//////////////////////////////////////////////////////////////////////////////
class ZoneGroup;
class NPC;
class NPCManager;
class MonsterManager;
class PCManager;
class Slayer;

class MasterLairManager;
class WarScheduler;
// class EventMonsterManager;

class EffectManager;
class NPCInfo;
class WeatherManager;
class Creature;
class Vampire;
class Monster;
class Item;
class VampirePortalItem;
class EffectVampirePortal;
class EffectScheduleManager;
class TradeManager;
class Motorcycle;
class LevelWarManager;
class DynamicZone;

//////////////////////////////////////////////////////////////////////////////
// Zone type
//////////////////////////////////////////////////////////////////////////////
enum ZoneType {
    ZONE_NORMAL_FIELD,          // Normal field
    ZONE_NORMAL_DUNGEON,        // Normal dungeon
    ZONE_SLAYER_GUILD,          // Slayer guild
    ZONE_RESERVED_SLAYER_GUILD, // ...
    ZONE_PC_VAMPIRE_LAIR,       // PC vampire lair
    ZONE_NPC_VAMPIRE_LAIR,      // NPC vampire lair
    ZONE_NPC_HOME,              // ...
    ZONE_NPC_SHOP,              // ...
    ZONE_RANDOM_MAP,            // -_-;
    ZONE_CASTLE,                // Castle
};

//////////////////////////////////////////////////////////////////////////////
// Zone access mode
//////////////////////////////////////////////////////////////////////////////
enum ZoneAccessMode {
    PUBLIC = 0, // Anyone may enter this zone.
    PRIVATE     // Only designated people may enter (guild zone, lair and so on).
};

//////////////////////////////////////////////////////////////////////////////
// Move type for darkness
//////////////////////////////////////////////////////////////////////////////
#define INTO_DARKNESS 0
#define OUTER_DARKNESS 1
#define IN_DARKNESS 2
#define OUT_DARKNESS 3


//////////////////////////////////////////////////////////////////////////////
//
// class Zone;
//
//////////////////////////////////////////////////////////////////////////////

class Zone {
    // The runtime tests give a zone the managers its load() would make from
    // the database (tests/game_level_war_command_test.cpp).
    friend class ZoneTestAccess;

public: // constructor & destructor
    Zone(ZoneID_t zoneID);
    Zone(ZoneID_t zoneID, ZoneCoord_t width, ZoneCoord_t height);
    ~Zone();

public:
    void init();
    void load(bool bOutput = false);
    void reload(bool bOutput = false);
    void loadTriggeredPortal();
    void initSpriteCount();
    void save();

public:
    void pushPC(Creature* pCreature);
    void addPC(Creature* pCreature, ZoneCoord_t cx, ZoneCoord_t cy, Dir_t dir);
    void addPC(Creature* pCreature);
    void replacePC(Creature* pFrom, Creature* pTo, ZoneCoord_t nx, ZoneCoord_t ny, Dir_t dir,
                   bool bFindSuitablePosition = false, bool bCheckEffect = true, bool bCheckPortal = true);
    void addCreature(Creature* pCreature, ZoneCoord_t cx, ZoneCoord_t cy, Dir_t dir);

    // Tile-only creature writes; see Zone.cpp for the contract.
    bool addCreatureToTile(Creature* pCreature, ZoneCoord_t x, ZoneCoord_t y, bool bCheckEffect = true,
                           bool bCheckPortal = true);
    void deleteCreatureFromTile(Creature* pCreature, ZoneCoord_t x, ZoneCoord_t y);

    TPOINT addItem(Item* pItem, ZoneCoord_t cx, ZoneCoord_t cy, bool bAllowCreature = true, Turn_t decayTurn = 0,
                   ObjectID_t DropPetOID = 0);
    Item* getItem(ObjectID_t id) const;
    void addEffect(Effect* pEffect);
    void deleteEffect(ObjectID_t id);
    Effect* findEffect(Effect::EffectClass eid);

    // by sigi. 2002.5.4
    void addEffect_LOCKING(Effect* pEffect);
    void deleteEffect_LOCKING(ObjectID_t id);

    void deletePC(Creature* pCreature); // NoSuchElementException, Error);
    void deleteQueuePC(Creature* pCreature);
    void deleteCreature(Creature* pCreature, ZoneCoord_t x, ZoneCoord_t y);
    void deleteObject(Object* pObject, ZoneCoord_t x, ZoneCoord_t y);
    void deleteItem(Object* pObject, ZoneCoord_t x, ZoneCoord_t y);

    bool deleteNPC(Creature* pCreature); // NoSuchElementException, Error);
    void deleteNPCs(Race_t race);        // NoSuchElementException, Error);
    void loadNPCs(Race_t race);          // NoSuchElementException, Error);
    void sendNPCInfo();

    void loadEffect();


public:
    void movePC(Creature* pCreature, ZoneCoord_t nx, ZoneCoord_t ny, Dir_t dir);
    void moveCreature(Creature* pCreature, ZoneCoord_t nx, ZoneCoord_t ny, Dir_t dir);

    // The monster scans its surroundings.
    void monsterScan(Monster* pMonster, ZoneCoord_t x, ZoneCoord_t y, Dir_t dir);

    void broadcastPacket(Packet* pPacket, Creature* owner = NULL);
    void broadcastPacket(ZoneCoord_t x, ZoneCoord_t y, Packet* pPacket, Creature* owner = NULL, bool Plus = false,
                         Range_t Range = 0);
    void broadcastPacket(ZoneCoord_t x, ZoneCoord_t y, Packet* pPacket, const list<Creature*>& creatureList,
                         bool Plus = false, Range_t Range = 0);
    void broadcastDarkLightPacket(Packet* pPacket1, Packet* pPacket2, Creature* owner = NULL);
    void broadcastSayPacket(ZoneCoord_t x, ZoneCoord_t y, Packet* pPacket, Creature* owner = NULL,
                            bool isVampire = false);
    void broadcastLevelWarBonusPacket(Packet* pPacket, Creature* owner = NULL);
    list<Creature*> broadcastSkillPacket(ZoneCoord_t x1, ZoneCoord_t y1, ZoneCoord_t x2, ZoneCoord_t y2,
                                         Packet* pPacket, list<Creature*> creatureList, bool bConcernDarkness = true);

    //(x,y) Read the surroundings of the PC on that tile and send them. If pPacket is not NULL, also
    // broadcast it.
    void scan(Creature* pPC, ZoneCoord_t x, ZoneCoord_t y, Packet* pPacket);

    // Returns the list of players who can see that spot.
    list<Creature*> getWatcherList(ZoneCoord_t, ZoneCoord_t, Creature* pTargetCreature = NULL);

    // A regenerated monster broadcasts GCAddXXX packets to nearby PCs and marks them as potential enemies.
    void scanPC(Creature* pCreature);

    // Used when a hidden creature needs an update without the creature moving.
    void updateHiddenScan(Creature* pCreature);

    // Used when an installed mine needs an update without the creature moving.
    void updateMineScan(Creature* pCreature);

    // Used when an invisible creature needs an update without the creature moving.
    void updateInvisibleScan(Creature* pCreature);

    // Used when hidden and invisible creatures need an update without the creature moving.
    void updateDetectScan(Creature* pCreature);

    // Broadcast to the surroundings that the PC moved from P(x1,y1) to Q(x2,y2).
    void movePCBroadcast(Creature* pPC, ZoneCoord_t x1, ZoneCoord_t y1, ZoneCoord_t x2, ZoneCoord_t y2,
                         bool bSendMove = true, bool bKnockback = false);

    // Broadcast to the surroundings that the non-PC creature moved from P(x1,y1) to Q(x2,y2).
    void moveCreatureBroadcast(Creature* pCreature, ZoneCoord_t x1, ZoneCoord_t y1, ZoneCoord_t x2, ZoneCoord_t y2,
                               bool bSendMove = true, bool bKnockback = false);

    // Move the PC to Q(x2,y2) with a skill and broadcast the move to the surroundings.
    bool moveFastPC(Creature* pPC, ZoneCoord_t x1, ZoneCoord_t y1, ZoneCoord_t x2, ZoneCoord_t y2,
                    SkillType_t skillType);
    bool moveFastMonster(Monster* pMonster, ZoneCoord_t x1, ZoneCoord_t y1, ZoneCoord_t x2, ZoneCoord_t y2,
                         SkillType_t skillType);

    // During a war the castle interior turns into a safe zone and back; clear or restore the safe zone.
    void releaseSafeZone();
    void resetSafeZone();

    // Register an object
    void registerObject(Object* pObject) {
        getObjectRegistry().registerObject(pObject);
    }

public:
    void heartbeat();

    // Access creature objects in the zone by name, creature class, OID and so on
    Creature* getCreature(const string& Name) const;  // NoSuchElementException, Error);
    Creature* getCreature(ObjectID_t objectID) const; // NoSuchElementException, Error);
    Creature* getCreature(Creature::CreatureClass creatureClass, ObjectID_t objectID) const;


public:
    void lock() {
        m_Mutex.lock();
    }
    void unlock() {
        m_Mutex.unlock();
    }

    // get debug string
    string toString() const;

public:
    ObjectRegistry& getObjectRegistry() {
        return m_ObjectRegistry;
    }

    const Tile& getTile(ZoneCoord_t x, ZoneCoord_t y) const;
    Tile& getTile(ZoneCoord_t x, ZoneCoord_t y);

    Sector* getSector(ZoneCoord_t x, ZoneCoord_t y);

    ZoneID_t getZoneID() const {
        return m_ZoneID;
    }
    void setZoneID(ZoneID_t zoneID) {
        m_ZoneID = zoneID;
    }

    ZoneGroup* getZoneGroup() const {
        return m_pZoneGroup;
    }
    void setZoneGroup(ZoneGroup* pZoneGroup) {
        m_pZoneGroup = pZoneGroup;
    }

    ZoneType getZoneType() const {
        return m_ZoneType;
    }
    void setZoneType(ZoneType zoneType) {
        m_ZoneType = zoneType;
    }

    ZoneLevel_t getZoneLevel() const {
        return m_ZoneLevel;
    }
    void setZoneLevel(ZoneLevel_t zoneLevel) {
        m_ZoneLevel = zoneLevel;
    }
    ZoneLevel_t getZoneLevel(ZoneCoord_t x, ZoneCoord_t y) const;

    ZoneAccessMode getZoneAccessMode() const {
        return m_ZoneAccessMode;
    }
    void setZoneAccessMode(ZoneAccessMode zoneAccessMode) {
        m_ZoneAccessMode = zoneAccessMode;
    }

    string getOwnerID() const {
        return m_OwnerID;
    }
    void setOwnerID(const string& ownerID) {
        m_OwnerID = ownerID;
    }

    DarkLevel_t getDarkLevel() const {
        return m_DarkLevel;
    }
    void setDarkLevel(DarkLevel_t darkLevel) {
        m_DarkLevel = darkLevel;
    }

    LightLevel_t getLightLevel() const {
        return m_LightLevel;
    }
    void setLightLevel(LightLevel_t lightLevel) {
        m_LightLevel = lightLevel;
    }

    const WeatherManager* getWeatherManager() const {
        return m_pWeatherManager;
    }

    uint getNPCCount() const {
        return m_NPCCount;
    }
    void setNPCCount(uint n) {
        Assert(n <= maxNPCPerZone);
        m_NPCCount = n;
    }

    NPCType_t getNPCType(uint n) const {
        Assert(n < maxNPCPerZone);
        return m_NPCTypes[n];
    }
    void setNPCType(uint n, NPCType_t npcType) {
        Assert(n < maxNPCPerZone);
        m_NPCTypes[n] = npcType;
    }

    uint getMonsterCount() const {
        return m_MonsterCount;
    }
    void setMonsterCount(uint n) {
        Assert(n <= maxMonsterPerZone);
        m_MonsterCount = n;
    }

    MonsterType_t getMonsterType(uint n) const {
        Assert(n < maxMonsterPerZone);
        return m_MonsterTypes[n];
    }
    void setMonsterType(uint n, MonsterType_t npcType) {
        Assert(n < maxMonsterPerZone);
        m_MonsterTypes[n] = npcType;
    }

    ZoneCoord_t getWidth() const {
        return m_Width;
    }
    ZoneCoord_t getHeight() const {
        return m_Height;
    }

    uint getTimeband() const {
        return m_Timeband;
    }
    void setTimeband(uint timeband) {
        m_Timeband = timeband;
    }

    bool isTimeStop() {
        return m_bTimeStop;
    }
    void stopTime() {
        m_bTimeStop = true;
    }
    void resumeTime() {
        m_bTimeStop = false;
    }

    void resetDarkLightInfo();

    // ABCD add item to item hash map
    void addToItemList(Item* pItem);
    void deleteFromItemList(ObjectID_t id);
    unordered_map<ObjectID_t, Item*> getItems(void) {
        return m_Items;
    }

    EffectManager* getEffectManager() {
        return m_pEffectManager;
    }
    EffectManager* getVampirePortalManager() {
        return m_pVampirePortalManager;
    }
    EffectScheduleManager* getEffectScheduleManager(void) {
        return m_pEffectScheduleManager;
    }

    VSRect* getOuterRect(void) {
        return &m_OuterRect;
    }
    VSRect* getInnerRect(void) {
        return &m_InnerRect;
    }
    VSRect* getCoreRect(void) {
        return &m_CoreRect;
    }

    list<NPCInfo*>* getNPCInfos(void);
    void addNPCInfo(NPCInfo* pInfo);
    bool removeNPCInfo(NPC* pNPC);

    // Set the MarketCondition on every NPC in the zone. default(100, 25)
    // void setNPCMarketCondition(MarketCond_t NPCSell, MarketCond_t NPCBuy) ;

    void addVampirePortal(ZoneCoord_t cx, ZoneCoord_t cy, Vampire* pVampire, const ZONE_COORD& ZoneCoord);
    void deleteMotorcycle(ZoneCoord_t cx, ZoneCoord_t cy, Motorcycle* pMotorcycle);

    void addItemDelayed(Item* pItem, ZoneCoord_t cx, ZoneCoord_t cy, bool bAllowCreature = true);
    void addItemToCorpseDelayed(Item* pItem, ObjectID_t corpseObjectID);
    void deleteItemDelayed(Object* pObject, ZoneCoord_t x, ZoneCoord_t y);
    void transportItem(ZoneCoord_t x, ZoneCoord_t y, Item* pItem, Zone* pZone, ZoneCoord_t cx, ZoneCoord_t cy);
    void transportItemToCorpse(Item* pItem, Zone* pTargetZone, ObjectID_t corpseObjectID);

    LocalPartyManager* getLocalPartyManager(void) const {
        return m_pLocalPartyManager;
    }
    PartyInviteInfoManager* getPartyInviteInfoManager(void) const {
        return m_pPartyInviteInfoManager;
    }

    TradeManager* getTradeManager(void) const {
        return m_pTradeManager;
    }


    // Which packet is pMonster added with when pPC sees it?
    // If pPC is NULL, assume everything is visible.
    Packet* createMonsterAddPacket(Monster* pMonster, Creature* pPC) const;

    // Monster spawn coordinates
    const BPOINT& getRandomMonsterRegenPosition() const;
    const BPOINT& getRandomEmptyTilePosition() const;

    MonsterManager* getMonsterManager(void) const {
        return m_pMonsterManager;
    }
    MasterLairManager* getMasterLairManager(void) const {
        return m_pMasterLairManager;
    }
    WarScheduler* getWarScheduler(void) const {
        return m_pWarScheduler;
    }
    LevelWarManager* getLevelWarManager() const {
        return m_pLevelWarManager;
    }

    void killAllMonsters();
    void killAllMonsters_UNLOCK();

    void killAllPCs();

    const PCManager* getPCManager() const {
        return m_pPCManager;
    } // PC Manager
    WORD getPCCount(void) const {
        return m_pPCManager->getSize();
    }

#ifdef __USE_ENCRYPTER__
    // get zone's encrypt code
    uchar getEncryptCode() const {
        return m_EncryptCode;
    }
#endif

public:
    // Pay play
    bool isPayPlay() const {
        return m_bPayPlay;
    }
    void setPayPlay(bool bPayPlay = true) {
        m_bPayPlay = bPayPlay;
    }

    bool isPremiumZone() const {
        return m_bPremiumZone;
    }
    void setPremiumZone(bool bPremiumZone = true) {
        m_bPremiumZone = bPremiumZone;
    }

    bool isPKZone() const {
        return m_bPKZone;
    }
    void setPKZone(bool bPKZone = true) {
        m_bPKZone = bPKZone;
    }

    bool isNoPortalZone() const {
        return m_bNoPortalZone;
    }
    void setNoPortalZone(bool bNoPortalZone = true) {
        m_bNoPortalZone = bNoPortalZone;
    }

    bool isMasterLair() const {
        return m_bMasterLair;
    }
    void setMasterLair(bool bMasterLair = true) {
        m_bMasterLair = bMasterLair;
    }

    bool isCastle() const {
        return m_bCastle;
    }
    void setCastle(bool bCastle = true) {
        m_bCastle = bCastle;
    }

    bool isHolyLand() const {
        return m_bHolyLand;
    }
    void setHolyLand(bool bHolyLand = true) {
        m_bHolyLand = bHolyLand;
    }

    bool isCastleZone() const {
        return m_bCastleZone;
    }
    void setCastleZone(bool bCastleZone = true) {
        m_bCastleZone = bCastleZone;
    }

    // Does the zone have a relic table?
    bool hasRelicTable() const {
        return m_bHasRelicTable;
    }
    void setRelicTable(bool bHasRelicTable = true) {
        m_bHasRelicTable = bHasRelicTable;
    }

    bool addRelicItem(int relicIndex);
    bool deleteRelicItem();

    // Refresh players when the Holy Land race bonus changes
    void setRefreshHolyLandPlayer(bool bRefresh) {
        m_pPCManager->setRefreshHolyLandPlayer(bRefresh);
    }
    //	void setRefreshLevelWarBonusZonePlayer( bool bRefresh ) { m_pPCManager->setRefreshLevelWarBonusZonePlayer(
    // bRefresh ); }

    void remainRaceWarPlayers();

    void remainPayPlayer();

    bool isLevelWarZone() const;

public:
    void initLoadValue();
    DWORD getLoadValue() const;

public:
    bool isDynamicZone() const {
        return m_pDynamicZone != NULL;
    }
    void setDynamicZone(DynamicZone* pDynamicZone) {
        m_pDynamicZone = pDynamicZone;
    }
    DynamicZone* getDynamicZone() const {
        return m_pDynamicZone;
    }

    ////////////////////////////////////////////////////////////
    // member data
    ////////////////////////////////////////////////////////////
private:
    // Basic zone information
    ZoneID_t m_ZoneID;               // zone id
    ZoneGroup* m_pZoneGroup;         // parent zone group
    ZoneType m_ZoneType;             // Zone type (a changed zone type must be saved to the DB.)
    ZoneLevel_t m_ZoneLevel;         // Zone level.
    ZoneAccessMode m_ZoneAccessMode; // Access mode for the zone { PUBLIC | PRIVATE }
    string m_OwnerID;                // Zone owner id (slayer guild id or vampire master id)
    DarkLevel_t m_DarkLevel;         // Zone darkness
    LightLevel_t m_LightLevel;       // Zone light level
    ZoneCoord_t m_Width;             // Zone width
    ZoneCoord_t m_Height;            // Zone height
    Tile** m_pTiles;                 // Two-dimensional array of tiles
    ZoneLevel_t** m_ppLevel;         // Two-dimensional array of zone levels
    Sector** m_pSectors;             // Two-dimensional array of sectors
    int m_SectorWidth;               // Sector size
    int m_SectorHeight;              // Sector size

    // The various managers
    PCManager* m_pPCManager;           // PC Manager
    NPCManager* m_pNPCManager;         // NPC Manager
    MonsterManager* m_pMonsterManager; // Monster Manager

    //	EventMonsterManager*   m_pEventMonsterManager;

    EffectManager* m_pEffectManager;        // effect manager
    EffectManager* m_pVampirePortalManager; // effect manager
    EffectManager* m_pLockedEffectManager;  // effect manager
    EffectScheduleManager* m_pEffectScheduleManager;
    list<NPCInfo*> m_NPCInfos;         // npc infos
    WeatherManager* m_pWeatherManager; // Zone weather

    // List of NPC sprite types appearing in the zone
    BYTE m_NPCCount;
    NPCType_t m_NPCTypes[maxNPCPerZone];

    // List of monster sprite types appearing in the zone
    BYTE m_MonsterCount;
    MonsterType_t m_MonsterTypes[maxMonsterPerZone];

    // object registery
    ObjectRegistry m_ObjectRegistry;

    // Queue of PCs about to enter the zone
    queue<Creature*> m_PCQueue;
    list<Creature*> m_PCListQueue;

    // Hash map of items dropped on the zone floor
    unordered_map<ObjectID_t, Item*> m_Items;

    // Rectangles dividing the zone area up for monster AI
    VSRect m_OuterRect;
    VSRect m_InnerRect;
    VSRect m_CoreRect;

    LocalPartyManager* m_pLocalPartyManager;
    PartyInviteInfoManager* m_pPartyInviteInfoManager;
    TradeManager* m_pTradeManager;

    // Information for the monster spawn coordinates.
    vector<BPOINT> m_MonsterRegenPositions;
    vector<BPOINT> m_EmptyTilePositions;

    // mutex
    mutable Mutex m_Mutex;
    mutable Mutex m_MutexEffect;

    // Pay-play related
    bool m_bPayPlay;
    bool m_bPremiumZone;

    // Other information
    bool m_bPKZone;
    bool m_bNoPortalZone;
    bool m_bMasterLair;
    bool m_bCastle;
    bool m_bHolyLand;
    bool m_bCastleZone;

    // Does the zone have a relic table?
    bool m_bHasRelicTable;

    // Relic table related information
    ObjectID_t m_RelicTableOID;
    ZoneCoord_t m_RelicTableX;
    ZoneCoord_t m_RelicTableY;

    Timeval m_LoadValueStartTime;
    DWORD m_LoadValue;

    // Master lair.
    MasterLairManager* m_pMasterLairManager;

    // War management
    WarScheduler* m_pWarScheduler;

    LevelWarManager* m_pLevelWarManager;

    // by sigi. for debugging. 2002.12.23
    int m_LastItemClass;

    // Zone Timeband
    uint m_Timeband;

    bool m_bTimeStop;

    Timeval m_UpdateTimebandTime;

#ifdef __USE_ENCRYPTER__
    // zone's encrypt code
    uchar m_EncryptCode;
#endif

    DynamicZone* m_pDynamicZone; // Instance dungeon info
};

#endif
