//////////////////////////////////////////////////////////////////////////////
// FileName 	: Zone.cpp
// WrittenBy	:
// Description	:
//////////////////////////////////////////////////////////////////////////////

#include "Zone.h"

#include <stdio.h>
#include <string.h>

#include "Assert.h"
#include "GameContext.h"
#include "MasterLairInfoManager.h"
#include "MasterLairManager.h"
#include "MonsterManager.h"
#include "NPCManager.h"
#include "PCManager.h"
#include "QuestManager.h"
#include "VisionInfo.h"
#include "War.h"
#include "WarScheduler.h"
#include "WarSystem.h"
#include "ZoneGroup.h"
#include "ZoneInfo.h"
#include "ZoneInfoManager.h"
#include "ZoneInternal.h"

// #include "EventMonsterManager.h"

#include <math.h>

#include <fstream>

#include "BloodBibleBonusManager.h"
#include "CastleInfoManager.h"
#include "CombatInfoManager.h"
#include "Creature.h"
#include "DarkLightInfo.h"
#include "DefaultOptionSetInfo.h"
#include "EffectDarkness.h"
#include "EffectDecayCorpse.h"
#include "EffectDecayItem.h"
#include "EffectManager.h"
#include "EffectSchedule.h"
#include "EffectVampirePortal.h"
#include "FlagSet.h"
#include "GamePlayer.h"
#include "HolyLandManager.h"
#include "Item.h"
#include "ItemFactoryManager.h"
#include "ItemInfo.h"
#include "LevelWarZoneInfoManager.h"
#include "Monster.h"
#include "MonsterCorpse.h"
#include "NPC.h"
#include "NPCInfo.h"
#include "Ousters.h"
#include "OustersCorpse.h"
#include "PCFinder.h"
#include "PKZoneInfoManager.h"
#include "PacketUtil.h"
#include "ParkingCenter.h"
#include "Party.h"
#include "PaySystem.h"
#include "Player.h"
#include "Properties.h"
#include "RegenZoneManager.h"
#include "Relic.h"
#include "RelicUtil.h"
#include "ShrineInfoManager.h"
#include "Slayer.h"
#include "SlayerCorpse.h"
#include "StringPool.h"
#include "SweeperBonusManager.h"
#include "TimeManager.h"
#include "TradeManager.h"
#include "Vampire.h"
#include "VampireCorpse.h"
#include "VariableManager.h"
#include "WeatherManager.h"
#include "ZoneUtil.h"
#include "ctf/FlagManager.h"
#include "repository/ComebackEventRepository.h"
#include "repository/MessageRepository.h"
#include "repository/ZoneInfoRepository.h"
// #include "EffectRevealer.h"
#include "EffectAddItem.h"
#include "EffectAddItemToCorpse.h"
#include "EffectDeleteItem.h"
#include "EffectGnomesWhisper.h"
#include "EffectHasBloodBible.h"
#include "EffectHasSlayerRelic.h"
#include "EffectHasVampireRelic.h"
#include "EffectObservingEye.h"
#include "EffectRelicTable.h"
#include "EffectSanctuary.h"
#include "EffectShrineGuard.h"
#include "EffectShrineHoly.h"
#include "EffectShrineShield.h"
#include "EffectSlayerRelic.h"
#include "EffectTransportItem.h"
#include "EffectTransportItemToCorpse.h"
#include "EffectVampireRelic.h"
// #include "EffectDropBloodBible.h"
#include "EffectContinualGroundAttack.h"
#include "EffectHasCastleSymbol.h"
#include "EffectPKZoneRegen.h"
#include "EventTransport.h"
#include "GCAddBat.h"
#include "GCAddBurrowingCreature.h"
#include "GCAddEffect.h"
#include "GCAddEffectToTile.h"
#include "GCAddInstalledMineToZone.h"
#include "GCAddMonster.h"
#include "GCAddMonsterCorpse.h"
#include "GCAddMonsterFromBurrowing.h"
#include "GCAddMonsterFromTransformation.h"
#include "GCAddNPC.h"
#include "GCAddNewItemToZone.h"
#include "GCAddOusters.h"
#include "GCAddOustersCorpse.h"
#include "GCAddSlayer.h"
#include "GCAddSlayerCorpse.h"
#include "GCAddVampire.h"
#include "GCAddVampireCorpse.h"
#include "GCAddVampireFromBurrowing.h"
#include "GCAddVampireFromTransformation.h"
#include "GCAddVampirePortal.h"
#include "GCAddWolf.h"
#include "GCDeleteEffectFromTile.h"
#include "GCDeleteObject.h"
#include "GCDropItemToZone.h"
#include "GCFastMove.h"
#include "GCHolyLandBonusInfo.h"
#include "GCKnockBack.h"
#include "GCMineExplosionOK1.h"
#include "GCMineExplosionOK2.h"
#include "GCModifyInformation.h"
#include "GCMove.h"
#include "GCMoveError.h"
#include "GCMoveOK.h"
#include "GCMyStoreInfo.h"
#include "GCNPCInfo.h"
#include "GCNoticeEvent.h"
#include "GCRegenZoneStatus.h"
#include "GCRemoveEffect.h"
#include "GCSetPosition.h"
#include "GCSweeperBonusInfo.h"
#include "GCSystemMessage.h"
#include "GCUnburrowFail.h"
#include "GCUnburrowOK.h"
#include "GCUnionOfferList.h"
#include "GCUntransformFail.h"
#include "GCUntransformOK.h"
// #include "CGItemNameInfoList.h"

#include "DynamicZone.h"
#include "EffectCastingTrap.h"
#include "GDRLairManager.h"
#include "GGCommand.h"
#include "GQuestManager.h"
#include "GameServerInfoManager.h"
#include "GuildManager.h"
#include "GuildUnion.h"
#include "LevelWarManager.h"
#include "LoginServerManager.h"
#include "NicknameBook.h"
#include "Profile.h"
#include "ResurrectLocationManager.h"
#include "SiegeManager.h"
#include "SkillUtil.h"
#include "Store.h"
#include "item/Motorcycle.h"
#include "item/VampirePortalItem.h"


#ifdef __PROFILE_BROADCAST__
#define __BEGIN_PROFILE_ZONE(name) beginProfileEx(name);
#define __END_PROFILE_ZONE(name) endProfileEx(name);
#else
#define __BEGIN_PROFILE_ZONE(name) ((void)0);
#define __END_PROFILE_ZONE(name) ((void)0);
#endif


#ifndef __FULL_PROFILE__
#undef beginProfileEx
#define beginProfileEx(name) ((void)0)
#undef endProfileEx
#define endProfileEx(name) ((void)0)
#endif


void strlwr(char* str) {
    while (*str != '\0') {
        *str = tolower(*str);

        str++;
    }
}

//////////////////////////////////////////////////////////////////////////////
// Decides whether ordinary monsters recognize the creature as an enemy.
//////////////////////////////////////////////////////////////////////////////
bool isPotentialEnemy(Monster* pMonster, Creature* pCreature) {
    Assert(pCreature != NULL);

    if (pMonster->getOwnerObjectID() == pCreature->getObjectID())
        return false;

    if (pMonster->getOwnerObjectID() != 0) {
        Creature* pOwner = pMonster->getZone()->getCreature(pMonster->getOwnerObjectID());
        if (pOwner != NULL && pOwner->getCreatureClass() == pCreature->getCreatureClass() &&
            canAttack(pOwner, pCreature)) {
            return false;
        }

        if (GDRLairManager::Instance().isGDRLairZone(pMonster->getZoneID()) && pCreature->isPC())
            return false;
    }

    if (SiegeManager::Instance().isSiegeZone(pMonster->getZoneID())) {
        if (pCreature->isFlag(Effect::EFFECT_CLASS_SIEGE_DEFENDER) ||
            pCreature->isFlag(Effect::EFFECT_CLASS_SIEGE_REINFORCE))
            return false;
    }

    // A Slayer is always an enemy; an Ousters only above level 10 or when the monster outlevels it by 10.
    if (pCreature->isSlayer())
        return true;

    if (pCreature->isOusters()) {
        Ousters* pOusters = dynamic_cast<Ousters*>(pCreature);

        if ((pOusters->getLevel() + 10) <= pMonster->getLevel())
            return true;
        if (pOusters->getLevel() > 10)
            return true;
    } else if (pCreature->isVampire()) {
        Vampire* pVampire = dynamic_cast<Vampire*>(pCreature);

        // When the monster's level is at least 10 levels above the vampire's level,
        // recognize it as an enemy.
        if ((pVampire->getLevel() + 10) <= pMonster->getLevel()) {
            return true;
        }

        // Vampires of level 10 or higher are enemies.
        if (pVampire->getLevel() > 10) {
            return true;
        }
    } else if (pCreature->isMonster()) {
        Monster* pOtherMonster = dynamic_cast<Monster*>(pCreature);

        return pMonster->isFlag(Effect::EFFECT_CLASS_HALLUCINATION) ||
               pMonster->getClanType() != pOtherMonster->getClanType();
    }

    return false;
}


list<Packet*>* getRelicEffectPacket(MonsterCorpse* pMonsterCorpse, Effect::EffectClass EClass,
                                    list<Packet*>* pPackets) {
    if (pMonsterCorpse->isFlag(EClass)) {
        Effect* pEffect = pMonsterCorpse->getEffectManager().findEffect(EClass);

        if (pEffect == NULL)
            cout << (int)EClass << endl;

        Assert(pEffect != NULL);

        GCAddEffect* pPacket = new GCAddEffect;
        pPacket->setObjectID(pMonsterCorpse->getObjectID());
        pPacket->setEffectID(pEffect->getSendEffectClass());
        pPacket->setDuration(pEffect->getRemainDuration());

        if (pPackets == NULL)
            pPackets = new list<Packet*>;
        pPackets->push_back(pPacket);
    }

    return pPackets;
}


//////////////////////////////////////////////////////////////////////////////
// createRelicEffect( MonsterCorpse* )
//////////////////////////////////////////////////////////////////////////////
// Builds the packet list for the Effects attached to pMonsterCorpse.
//////////////////////////////////////////////////////////////////////////////
list<Packet*>* createRelicEffect(MonsterCorpse* pMonsterCorpse) {
    list<Packet*>* pPackets = NULL;

    if (pMonsterCorpse->isFlag(Effect::EFFECT_CLASS_SLAYER_RELIC)) {
        GCAddEffect* pPacket = new GCAddEffect;
        pPacket->setObjectID(pMonsterCorpse->getObjectID());
        pPacket->setEffectID(Effect::EFFECT_CLASS_SLAYER_RELIC);
        pPacket->setDuration(65000);

        if (pPackets == NULL)
            pPackets = new list<Packet*>;
        pPackets->push_back(pPacket);
    }

    if (pMonsterCorpse->isFlag(Effect::EFFECT_CLASS_VAMPIRE_RELIC)) {
        GCAddEffect* pPacket = new GCAddEffect;
        pPacket->setObjectID(pMonsterCorpse->getObjectID());
        pPacket->setEffectID(Effect::EFFECT_CLASS_VAMPIRE_RELIC);
        pPacket->setDuration(65000);

        if (pPackets == NULL)
            pPackets = new list<Packet*>;
        pPackets->push_back(pPacket);
    }

    Effect::EffectClass EClass = Effect::EFFECT_CLASS_SHRINE_GUARD;
    if (pMonsterCorpse->isFlag(EClass)) {
        Effect* pEffect = pMonsterCorpse->getEffectManager().findEffect(EClass);
        Assert(pEffect != NULL);

        EffectShrineGuard* pEffectShrineGuard = dynamic_cast<EffectShrineGuard*>(pEffect);
        Assert(pEffectShrineGuard != NULL);

        GCAddEffect* pPacket = new GCAddEffect;
        pPacket->setObjectID(pMonsterCorpse->getObjectID());
        pPacket->setEffectID(EClass + pEffectShrineGuard->getShrineID());
        pPacket->setDuration(65000);

        if (pPackets == NULL)
            pPackets = new list<Packet*>;
        pPackets->push_back(pPacket);
    }

    EClass = Effect::EFFECT_CLASS_SHRINE_HOLY;
    if (pMonsterCorpse->isFlag(EClass)) {
        Effect* pEffect = pMonsterCorpse->getEffectManager().findEffect(EClass);
        Assert(pEffect != NULL);

        EffectShrineHoly* pEffectShrineHoly = dynamic_cast<EffectShrineHoly*>(pEffect);
        Assert(pEffectShrineHoly != NULL);

        GCAddEffect* pPacket = new GCAddEffect;
        pPacket->setObjectID(pMonsterCorpse->getObjectID());
        pPacket->setEffectID(EClass + pEffectShrineHoly->getShrineID());
        pPacket->setDuration(65000);

        if (pPackets == NULL)
            pPackets = new list<Packet*>;
        pPackets->push_back(pPacket);
    }

    EClass = Effect::EFFECT_CLASS_SHRINE_SHIELD;
    if (pMonsterCorpse->isFlag(EClass)) {
        Effect* pEffect = pMonsterCorpse->getEffectManager().findEffect(EClass);
        Assert(pEffect != NULL);

        EffectShrineShield* pEffectShrineShield = dynamic_cast<EffectShrineShield*>(pEffect);
        Assert(pEffectShrineShield != NULL);

        GCAddEffect* pPacket = new GCAddEffect;
        pPacket->setObjectID(pMonsterCorpse->getObjectID());
        pPacket->setEffectID(EClass);
        pPacket->setDuration(65000);

        if (pPackets == NULL)
            pPackets = new list<Packet*>;
        pPackets->push_back(pPacket);
    }

    EClass = Effect::EFFECT_CLASS_HAS_BLOOD_BIBLE;
    if (pMonsterCorpse->isFlag(EClass)) {
        Effect* pEffect = pMonsterCorpse->getEffectManager().findEffect(EClass);
        Assert(pEffect != NULL);

        EffectHasBloodBible* pEffectHasBloodBible = dynamic_cast<EffectHasBloodBible*>(pEffect);
        Assert(pEffectHasBloodBible != NULL);

        GCAddEffect* pPacket = new GCAddEffect;
        pPacket->setObjectID(pMonsterCorpse->getObjectID());
        pPacket->setEffectID(pEffectHasBloodBible->getSendEffectClass());
        pPacket->setDuration(65000);

        if (pPackets == NULL)
            pPackets = new list<Packet*>;
        pPackets->push_back(pPacket);
    }

    EClass = Effect::EFFECT_CLASS_HAS_CASTLE_SYMBOL;
    if (pMonsterCorpse->isFlag(EClass)) {
        Effect* pEffect = pMonsterCorpse->getEffectManager().findEffect(EClass);
        Assert(pEffect != NULL);

        EffectHasCastleSymbol* pEffectHasCastleSymbol = dynamic_cast<EffectHasCastleSymbol*>(pEffect);
        Assert(pEffectHasCastleSymbol != NULL);

        GCAddEffect* pPacket = new GCAddEffect;
        pPacket->setObjectID(pMonsterCorpse->getObjectID());
        pPacket->setEffectID(pEffectHasCastleSymbol->getSendEffectClass());
        pPacket->setDuration(65000);

        if (pPackets == NULL)
            pPackets = new list<Packet*>;
        pPackets->push_back(pPacket);
    }

    EClass = Effect::EFFECT_CLASS_FLAG_INSERT;
    if (pMonsterCorpse->isFlag(EClass)) {
        Effect* pEffect = pMonsterCorpse->getEffectManager().findEffect(EClass);
        Assert(pEffect != NULL);

        GCAddEffect* pPacket = new GCAddEffect;
        pPacket->setObjectID(pMonsterCorpse->getObjectID());
        pPacket->setEffectID(pEffect->getSendEffectClass());
        pPacket->setDuration(65000);

        if (pPackets == NULL)
            pPackets = new list<Packet*>;
        pPackets->push_back(pPacket);
    }

    EClass = Effect::EFFECT_CLASS_KEEP_SWEEPER;
    if (pMonsterCorpse->isFlag(EClass)) {
        Effect* pEffect = pMonsterCorpse->getEffectManager().findEffect(EClass);
        Assert(pEffect != NULL);

        GCAddEffect* pPacket = new GCAddEffect;
        pPacket->setObjectID(pMonsterCorpse->getObjectID());
        pPacket->setEffectID(pEffect->getSendEffectClass());
        pPacket->setDuration(65000);

        if (pPackets == NULL)
            pPackets = new list<Packet*>;
        pPackets->push_back(pPacket);
    }

    pPackets = getRelicEffectPacket(pMonsterCorpse, Effect::EFFECT_CLASS_SLAYER_REGEN_ZONE, pPackets);
    pPackets = getRelicEffectPacket(pMonsterCorpse, Effect::EFFECT_CLASS_SLAYER_TRYING_1, pPackets);
    pPackets = getRelicEffectPacket(pMonsterCorpse, Effect::EFFECT_CLASS_VAMPIRE_TRYING_1, pPackets);
    pPackets = getRelicEffectPacket(pMonsterCorpse, Effect::EFFECT_CLASS_OUSTERS_TRYING_1, pPackets);

    return pPackets;
}

//////////////////////////////////////////////////////////////////////////////
// sendRelicEffect( MonsterCorpse* )
//////////////////////////////////////////////////////////////////////////////
// Sends the Effects attached to pMonsterCorpse to pPlayer.
//////////////////////////////////////////////////////////////////////////////
void sendRelicEffect(MonsterCorpse* pMonsterCorpse, Player* pPlayer) {
    list<Packet*>* pPackets = createRelicEffect(pMonsterCorpse);

    if (pPackets != NULL) {
        list<Packet*>::iterator itr = pPackets->begin();
        for (; itr != pPackets->end(); itr++) {
            Packet* pPacket = *itr;
            pPlayer->sendPacket(pPacket);

            SAFE_DELETE(pPacket);
        }
        SAFE_DELETE(pPackets);
    }
}

//////////////////////////////////////////////////////////////////////////////
// sendRelicEffect( MonsterCorpse* )
//////////////////////////////////////////////////////////////////////////////
// Scatters the Effects attached to pMonsterCorpse at (x,y).
//////////////////////////////////////////////////////////////////////////////
void sendRelicEffect(MonsterCorpse* pMonsterCorpse, Zone* pZone, ZoneCoord_t x, ZoneCoord_t y) {
    list<Packet*>* pPackets = createRelicEffect(pMonsterCorpse);

    if (pPackets != NULL) {
        list<Packet*>::iterator itr = pPackets->begin();
        for (; itr != pPackets->end(); itr++) {
            Packet* pPacket = *itr;
            pZone->broadcastPacket(x, y, pPacket);

            SAFE_DELETE(pPacket);
        }
        SAFE_DELETE(pPackets);
    }
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
Zone::Zone(ZoneID_t zoneID)

{
    m_Mutex.setName("Zone");
    m_MutexEffect.setName("ZoneEffect");

    m_ZoneID = zoneID;
    m_pZoneGroup = NULL;
    m_Width = 0;
    m_Height = 0;
    m_pTiles = NULL;
    m_NPCCount = 0;
    m_MonsterCount = 0;
    m_pPCManager = new PCManager();
    m_pNPCManager = new NPCManager();
    m_pMonsterManager = new MonsterManager(this);
    m_pMasterLairManager = NULL;
    m_pWarScheduler = NULL;
    m_pLevelWarManager = NULL;


    m_pWeatherManager = new WeatherManager(this);
    m_pEffectManager = new EffectManager();
    m_pLockedEffectManager = new EffectManager();
    m_pVampirePortalManager = new EffectManager();
    m_pEffectScheduleManager = new EffectScheduleManager();
    m_pLocalPartyManager = new LocalPartyManager();
    m_pPartyInviteInfoManager = new PartyInviteInfoManager();
    m_pTradeManager = new TradeManager;

    m_bPayPlay = false;
    m_bPremiumZone = false;
    m_bPKZone = false;
    m_bNoPortalZone = false;
    m_bMasterLair = false;
    m_bCastle = false;
    m_bHolyLand = false;
    m_bHasRelicTable = false;

    getCurrentTime(m_LoadValueStartTime);

    m_LoadValue = 0;

    m_bTimeStop = false;

    getCurrentTime(m_UpdateTimebandTime);

    m_pDynamicZone = NULL;
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
Zone::Zone(ZoneID_t zoneID, ZoneCoord_t width, ZoneCoord_t height)

{
    __BEGIN_TRY

    Assert(false);

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
Zone::~Zone()

{
    __BEGIN_TRY

    if (m_pTiles != NULL) {
        for (uint i = 0; i < m_Width; i++)
            SAFE_DELETE_ARRAY(m_pTiles[i]);
        SAFE_DELETE_ARRAY(m_pTiles);
    }

    if (m_ppLevel != NULL) {
        for (uint i = 0; i < m_Width; i++)
            SAFE_DELETE_ARRAY(m_ppLevel[i]);
        SAFE_DELETE_ARRAY(m_ppLevel);
    }

    if (m_pSectors != NULL) {
        for (int i = 0; i < m_SectorWidth; i++)
            SAFE_DELETE_ARRAY(m_pSectors[i]);
        SAFE_DELETE_ARRAY(m_pSectors);
    }

    SAFE_DELETE(m_pPCManager);
    SAFE_DELETE(m_pNPCManager);
    SAFE_DELETE(m_pMonsterManager);
    SAFE_DELETE(m_pMasterLairManager);
    SAFE_DELETE(m_pWarScheduler);


    SAFE_DELETE(m_pWeatherManager);
    SAFE_DELETE(m_pEffectManager);
    SAFE_DELETE(m_pLockedEffectManager);
    SAFE_DELETE(m_pVampirePortalManager);
    SAFE_DELETE(m_pEffectScheduleManager);
    SAFE_DELETE(m_pLocalPartyManager);
    SAFE_DELETE(m_pPartyInviteInfoManager);

    __END_CATCH_NO_RETHROW
}

//////////////////////////////////////////////////////////////////////////////
// Returns the zone level assigned to the tile.
//////////////////////////////////////////////////////////////////////////////
ZoneLevel_t Zone::getZoneLevel(ZoneCoord_t x, ZoneCoord_t y) const

{
    __BEGIN_TRY


    // No assert here:
    // the value went out of bounds and the assert killed the process,
    // so going without it should be fine.
    // by sigi. 2002.8.13
    if (x < m_Width && y < m_Height) {
        return m_ppLevel[x][y];
    }

    return COMPLETE_SAFE_ZONE;

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
// getTile
//////////////////////////////////////////////////////////////////////////////
const Tile& Zone::getTile(ZoneCoord_t x, ZoneCoord_t y) const

{
    __BEGIN_TRY

    Assert(x < m_Width && y < m_Height);
    return m_pTiles[x][y];

    __END_CATCH
}

Tile& Zone::getTile(ZoneCoord_t x, ZoneCoord_t y)

{
    __BEGIN_TRY

    Assert(x < m_Width && y < m_Height);
    return m_pTiles[x][y];

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
// getSector
//////////////////////////////////////////////////////////////////////////////
Sector* Zone::getSector(ZoneCoord_t x, ZoneCoord_t y)

{
    __BEGIN_TRY

    Assert(x < m_Width && y < m_Height);

    int sx = x / SECTOR_SIZE;
    int sy = y / SECTOR_SIZE;

    Assert(sx < m_SectorWidth && y < m_SectorHeight);

    return &(m_pSectors[sx][sy]);

    __END_CATCH
}


//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
void Zone::addEffect(Effect* pEffect)

{
    __BEGIN_TRY

    Assert(pEffect != NULL);

    m_pEffectManager->addEffect(pEffect);

    __END_CATCH
}

void Zone::deleteEffect(ObjectID_t id)

{
    __BEGIN_TRY

    m_pEffectManager->deleteEffect(id);

    __END_CATCH
}

Effect* Zone::findEffect(Effect::EffectClass eid)

{
    __BEGIN_TRY

    return m_pEffectManager->findEffect(eid);

    __END_CATCH
}

//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
void Zone::addEffect_LOCKING(Effect* pEffect)

{
    __BEGIN_TRY

    Assert(pEffect != NULL);

    __ENTER_CRITICAL_SECTION(m_MutexEffect)

    m_pLockedEffectManager->addEffect(pEffect);

    __LEAVE_CRITICAL_SECTION(m_MutexEffect)

    __END_CATCH
}

void Zone::deleteEffect_LOCKING(ObjectID_t id)

{
    __BEGIN_TRY

    __ENTER_CRITICAL_SECTION(m_MutexEffect)

    m_pLockedEffectManager->deleteEffect(id);

    __LEAVE_CRITICAL_SECTION(m_MutexEffect)

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////////////
// Add the work that has to run at a fixed interval here.
//////////////////////////////////////////////////////////////////////////////
void Zone::heartbeat()

{
    __BEGIN_TRY
    __BEGIN_DEBUG

    try {
        __ENTER_CRITICAL_SECTION(m_Mutex)

        beginProfileEx("Z_PCQUEUE");

        // Add the PCs in the PC queue to the zone.
        while (!m_PCListQueue.empty()) {
            Creature* pCreature = m_PCListQueue.front();
            Assert(pCreature != NULL);
            Assert(pCreature->getZone() == this);

            // Add to the zone and broadcast to the surrounding PCs.
            addPC(pCreature, pCreature->getX(), pCreature->getY(), DOWN);

            m_PCListQueue.pop_front();
        }

        endProfileEx("Z_PCQUEUE");

        beginProfileEx("Z_PC");
        m_pPCManager->processCreatures(); // process all PC
        endProfileEx("Z_PC");

        // A zone with a master lair manager is a master lair.
        // by sigi. 2002.9.2
        if (m_pMasterLairManager != NULL)
            m_pMasterLairManager->heartbeat(); // process master lair

        // A zone with a WarScheduler is a castle.
        // by sigi. 2003.1.24
        if (m_pWarScheduler != NULL && de::gameContext().variables().isWarActive()) {
            Work* pWork = m_pWarScheduler->heartbeat();

            if (pWork != NULL) {
                War* pWar = dynamic_cast<War*>(pWork);
                Assert(pWar != NULL);

                de::gameContext().warSystem().addWarDelayed(pWar);
            }
        }

        if (m_pLevelWarManager != NULL && de::gameContext().variables().isActiveLevelWar()) {
            m_pLevelWarManager->heartbeat();
            // LevelWar zones need this because the paid/free user access limit varies by time.
            m_pLevelWarManager->freeUserTimeCheck();
        }


        // Monsters are only heartbeat while a player is present.
        // That is, monsters in a zone with no player stay still.
        // The monsters' EffectManager does not run then, which could be a problem,
        // but it is considered harmless.
        if (getPCCount() > 0 || (isDynamicZone() && (m_pDynamicZone->getStatus() == DYNAMIC_ZONE_STATUS_RUNNING))) {
            beginProfileEx("Z_MONSTER");
            m_pMonsterManager->processCreatures(); // process all monsters
            endProfileEx("Z_MONSTER");
        }


        beginProfileEx("Z_NPC");
        m_pNPCManager->processCreatures(); // process all npcs
        endProfileEx("Z_NPC");

        beginProfileEx("Z_ESCH");
        // Run the effect schedule first.
        m_pEffectScheduleManager->heartbeat();
        endProfileEx("Z_ESCH");

        // So that the Item's EffectManager does not call getCurrentTime.
        // by sigi. 2002.5.8
        Timeval currentTime;
        getCurrentTime(currentTime);

        // Delete expired effects
        beginProfileEx("Z_EFFECT");
        m_pVampirePortalManager->heartbeat(currentTime);

        __ENTER_CRITICAL_SECTION(m_MutexEffect)
        m_pLockedEffectManager->heartbeat(currentTime);
        __LEAVE_CRITICAL_SECTION(m_MutexEffect)

        m_pEffectManager->heartbeat(currentTime);

        endProfileEx("Z_EFFECT");

        beginProfileEx("Z_WEATHER");
        // weather changing...
        m_pWeatherManager->heartbeat();
        endProfileEx("Z_WEATHER");

        beginProfileEx("Z_ITEM");
        // item heartbeaet

        for (unordered_map<ObjectID_t, Item*>::iterator itr = m_Items.begin(); itr != m_Items.end(); itr++) {
            Item* pItem = itr->second;
            Assert(pItem != NULL);

            // by sigi. for debugging. 2002.12.23
            m_LastItemClass = (int)pItem->getItemClass();

            EffectManager& rEffectManager = pItem->getEffectManager();
            rEffectManager.heartbeat(currentTime);
        }

        endProfileEx("Z_ITEM");

        beginProfileEx("Z_PARTY");
        // party heartbeat
        m_pLocalPartyManager->heartbeat();
        endProfileEx("Z_PARTY");

        __LEAVE_CRITICAL_SECTION(m_Mutex)

        Timeval currentTime;
        getCurrentTime(currentTime);

        // Update the time band.
        if (m_UpdateTimebandTime < currentTime) {
            if (!m_bTimeStop) {
                m_Timeband = de::gameContext().worldTime().getTimeband();
            }

            // Update the timeband every 5 seconds, which is 2 minutes of game time.
            m_UpdateTimebandTime.tv_sec += 5;
        }

        // Dynamic zone case.
        if (isDynamicZone())
            m_pDynamicZone->heartbeat();
    } catch (Throwable& t) {
        filelog("ZoneBug.txt", "%s : %s", "Zone::heartbeat(2)", t.toString().c_str());
        cerr << t.toString() << endl;
        throw;
    }

    m_LoadValue++;

    __END_DEBUG
    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
// Finds the creature with the given OID in PCManager, MonsterManager or
// NPCManager and returns it, or NULL when there is none.
//
// Use this method when the type (PC, NPC, Monster) of the creature being
// looked for is unknown. Where possible determine the type and use the
// getCreature(Creature::CreatureClass,ObjectID_t) method instead.
//////////////////////////////////////////////////////////////////////////////
Creature* Zone::getCreature(ObjectID_t objectID) const
// NoSuchElementException, Error)
{
    __BEGIN_TRY

    // Version that does not use NoSuchElementException.
    Creature* pCreature = NULL;

    pCreature = m_pMonsterManager->getCreature(objectID);

    if (pCreature == NULL) {
        pCreature = m_pPCManager->getCreature(objectID);

        if (pCreature == NULL) {
            pCreature = m_pNPCManager->getCreature(objectID);
        }
    }

    return pCreature;


    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
// Finds the creature with the given Name in PCManager, MonsterManager or
// NPCManager and returns it, or NULL when there is none.
//
// Use this method when the type (PC, NPC, Monster) of the creature being looked for is unknown.
// Where possible determine the type and use the getCreature(Creature::CreatureClass,Name)
// method instead.
//////////////////////////////////////////////////////////////////////////////
Creature* Zone::getCreature(const string& Name) const
// NoSuchElementException, Error)
{
    __BEGIN_TRY

    // Version that does not use NoSuchElementException.
    Creature* pCreature = NULL;

    pCreature = m_pPCManager->getCreature(Name);

    if (pCreature == NULL) {
        pCreature = m_pMonsterManager->getCreature(Name);

        if (pCreature == NULL) {
            pCreature = m_pNPCManager->getCreature(Name);
        }
    }

    return pCreature;


    __END_CATCH
}

//--------------------------------------------------------------------------------
//
// Finds and returns the creature with the given OID and creature type in the zone.
//
//--------------------------------------------------------------------------------
Creature* Zone::getCreature(Creature::CreatureClass creatureClass, ObjectID_t objectID) const

{
    __BEGIN_TRY

    if (creatureClass == Creature::CREATURE_CLASS_SLAYER) {
        return m_pPCManager->getCreature(objectID);
    } else if (creatureClass == Creature::CREATURE_CLASS_VAMPIRE) {
        return m_pPCManager->getCreature(objectID);
    } else if (creatureClass == Creature::CREATURE_CLASS_OUSTERS) {
        return m_pPCManager->getCreature(objectID);
    } else if (creatureClass == Creature::CREATURE_CLASS_NPC) {
        return m_pNPCManager->getCreature(objectID);
    } else if (creatureClass == Creature::CREATURE_CLASS_MONSTER) {
        return m_pMonsterManager->getCreature(objectID);
    }

    return NULL; // evade warning.

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
// get debug string
//////////////////////////////////////////////////////////////////////////////
string Zone::toString() const

{
    __BEGIN_TRY

    StringStream msg;

    msg << "Zone(" << "ZoneID:" << (int)m_ZoneID << ",ZoneGroupID:" << (int)m_pZoneGroup->getZoneGroupID()
        << ",ZoneType:" << (int)m_ZoneType << ",ZoneLevel:" << (int)m_ZoneLevel
        << ",ZoneAccessMode:" << (int)m_ZoneAccessMode << ",OwnerID:" << m_OwnerID << ",DarkLevel:" << (int)m_DarkLevel
        << ",LightLevel:" << (int)m_LightLevel << ",WeatherManager:" << m_pWeatherManager->toString();

    msg << ",#NPC:" << (int)m_NPCCount;
    for (uint i = 0; i < m_NPCCount; i++)
        msg << ",NPC[" << i << "] : " << (int)m_NPCTypes[i];

    msg << ",#Monster:" << (int)m_MonsterCount;

    Assert(m_MonsterCount < maxMonsterPerZone); // by sigi
    for (uint i = 0; i < m_MonsterCount; i++)
        msg << ",Monster[" << i << "] : " << (int)m_MonsterTypes[i];

    msg << ",Width:" << (int)m_Width << ",Height:" << (int)m_Height << ")";

    return msg.toString();

    __END_CATCH
}


list<NPCInfo*>* Zone::getNPCInfos(void) {
    return &m_NPCInfos;
}

void Zone::addNPCInfo(NPCInfo* pInfo) {
    // These must be freed when the zone is deleted.
    m_NPCInfos.push_back(pInfo);
}

const BPOINT& Zone::getRandomMonsterRegenPosition() const {
    return m_MonsterRegenPositions[rand() % m_MonsterRegenPositions.size()];
}

const BPOINT& Zone::getRandomEmptyTilePosition() const {
    return m_EmptyTilePositions[rand() % m_EmptyTilePositions.size()];
}

void Zone::initLoadValue() {
    m_LoadValue = 0;
    getCurrentTime(m_LoadValueStartTime);
}

DWORD Zone::getLoadValue() const {
    Timeval currentTime;
    getCurrentTime(currentTime);

    Timeval elapsedTime = timediff(currentTime, m_LoadValueStartTime);

    if (elapsedTime.tv_sec == 0) {
        return 200;
    }

    // Loops per 10 seconds.
    DWORD loadValue = m_LoadValue * 10 / elapsedTime.tv_sec;

    return loadValue;
}


void Zone::sendNPCInfo()

{
    __BEGIN_TRY

    // Send the NPC information to the client.
    GCNPCInfo gcNPCInfo;

    list<NPCInfo*>::const_iterator itr = m_NPCInfos.begin();
    for (; itr != m_NPCInfos.end(); itr++) {
        NPCInfo* pInfo = *itr;
        gcNPCInfo.addNPCInfo(pInfo);
    }

    broadcastPacket(&gcNPCInfo);

    __END_CATCH
}


bool Zone::removeNPCInfo(NPC* pNPC) {
    __BEGIN_TRY

    list<NPCInfo*>::iterator itr = m_NPCInfos.begin();

    for (; itr != m_NPCInfos.end(); itr++) {
        NPCInfo* pNPCInfo = *itr;

        if (pNPCInfo->getNPCID() == pNPC->getNPCID()) {
            SAFE_DELETE(pNPCInfo);
            m_NPCInfos.erase(itr);

            return true;
        }
    }

    return false;

    __END_CATCH
}

void Zone::releaseSafeZone()

{
    __BEGIN_TRY

    m_ZoneLevel = NO_SAFE_ZONE;

    // Reset the zone level of every tile.
    for (ZoneCoord_t x = 0; x < m_Width; x++)
        for (ZoneCoord_t y = 0; y < m_Height; y++)
            m_ppLevel[x][y] = m_ZoneLevel;

    __END_CATCH
}

void Zone::resetSafeZone()

{
    __BEGIN_TRY

    m_ZoneLevel = de::gameContext().zoneInfos().getZoneInfo(m_ZoneID)->getZoneLevel();

    // Reset the zone level of every tile.
    for (ZoneCoord_t x = 0; x < m_Width; x++)
        for (ZoneCoord_t y = 0; y < m_Height; y++)
            m_ppLevel[x][y] = m_ZoneLevel;

    __END_CATCH
}

void Zone::resetDarkLightInfo()

{
    __BEGIN_TRY

    m_pWeatherManager->resetDarkLightInfo();

    __END_CATCH
}


// Keeps only the players taking part in the race war and kicks the rest.
void Zone::remainRaceWarPlayers()

{
    __BEGIN_TRY

    try {
        // Ignore this when the participant limit is not active.
        if (!de::gameContext().variables().isActiveRaceWarLimiter())
            return;

        __ENTER_CRITICAL_SECTION(m_Mutex)

        unordered_map<ObjectID_t, Creature*>& creatures = m_pPCManager->getCreatures();
        unordered_map<ObjectID_t, Creature*>::iterator itr = creatures.begin();

        for (; itr != creatures.end(); itr++) {
            Creature* pCreature = itr->second;

            if (pCreature->isFlag(Effect::EFFECT_CLASS_RACE_WAR_JOIN_TICKET))
                continue;

            Player* pPlayer = pCreature->getPlayer();
            Assert(pPlayer != NULL);

            GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(pPlayer);

            Event* pEvent = pGamePlayer->getEvent(Event::EVENT_CLASS_TRANSPORT);
            if (pEvent == NULL) {
                PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pCreature);
                Assert(pPC != NULL);

                ZONE_COORD ZC;
                de::gameContext().resurrectLocations().getPosition(pPC, ZC);

                ZoneInfo* pZoneInfo = de::gameContext().zoneInfos().getZoneInfo(ZC.id);
                Assert(pZoneInfo != NULL);

                EventTransport* pEventTransport = new EventTransport(pGamePlayer);

                pEventTransport->setDeadline(15 * 10);
                pEventTransport->setTargetZone(ZC.id, ZC.x, ZC.y);
                pEventTransport->setZoneName(pZoneInfo->getFullName());

                // Tell the player where it will be moved to and in how many seconds.
                pEventTransport->sendMessage();

                pGamePlayer->addEvent(pEventTransport);
            } else {
                EventTransport* pEventTransport = dynamic_cast<EventTransport*>(pEvent);
                pEventTransport->sendMessage();
            }
        }

        __LEAVE_CRITICAL_SECTION(m_Mutex)

    } catch (Throwable& t) {
        cout << t.toString().c_str() << endl;
        throw;
    }

    __END_CATCH
}

bool Zone::isLevelWarZone() const {
    switch (m_ZoneID) {
    case 1131:
    case 1132:
    case 1133:
    case 1134: {
        return true;
    } break;
    default: {
        return false;
    } break;
    }
}

void Zone::remainPayPlayer()

{
    __BEGIN_TRY

    try {
        unordered_map<ObjectID_t, Creature*>& creatures = m_pPCManager->getCreatures();
        unordered_map<ObjectID_t, Creature*>::iterator itr = creatures.begin();

        for (; itr != creatures.end(); itr++) {
            Creature* pCreature = itr->second;
            Player* pPlayer = pCreature->getPlayer();
            Assert(pPlayer != NULL);

            GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(pPlayer);

            if (pGamePlayer->isPayPlaying())
                continue;

            Event* pEvent = pGamePlayer->getEvent(Event::EVENT_CLASS_TRANSPORT);
            if (pEvent == NULL) {
                PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pCreature);
                Assert(pPC != NULL);

                ZONE_COORD ZC;
                de::gameContext().resurrectLocations().getPosition(pPC, ZC);

                ZoneInfo* pZoneInfo = de::gameContext().zoneInfos().getZoneInfo(ZC.id);
                Assert(pZoneInfo != NULL);

                EventTransport* pEventTransport = new EventTransport(pGamePlayer);

                pEventTransport->setDeadline(60 * 10);
                pEventTransport->setTargetZone(ZC.id, ZC.x, ZC.y);
                pEventTransport->setZoneName(pZoneInfo->getFullName());

                StringPool& strings = de::gameContext().strings();
                char msg[100];
                snprintf(msg, sizeof(msg), strings.c_str(STRID_LEVEL_WAR_ZONE_FREE_CLOSE_1));

                GCSystemMessage gcSystemMessage;
                gcSystemMessage.setMessage(msg);
                pPlayer->sendPacket(&gcSystemMessage);

                snprintf(msg, sizeof(msg), strings.c_str(STRID_LEVEL_WAR_ZONE_FREE_CLOSE_2),
                         pZoneInfo->getFullName().c_str());

                gcSystemMessage.setMessage(msg);
                pPlayer->sendPacket(&gcSystemMessage);

                // Queue the transport event on the player.

                pGamePlayer->addEvent(pEventTransport);
            } else {
                EventTransport* pEventTransport = dynamic_cast<EventTransport*>(pEvent);
                pEventTransport->sendMessage();
            }
        }
    } catch (Throwable& t) {
        cout << t.toString().c_str() << endl;
        throw;
    }

    __END_CATCH
}
