//////////////////////////////////////////////////////////////////////////////
// FileName 	: ZoneLoad.cpp
// Description	: Zone loading: building a zone from its map file and its database contents.
//////////////////////////////////////////////////////////////////////////////

#include <math.h>
#include <stdio.h>
#include <string.h>

#include <fstream>

#include "Assert.h"
#include "BloodBibleBonusManager.h"
#include "CastleInfoManager.h"
#include "CombatInfoManager.h"
#include "Creature.h"
#include "DarkLightInfo.h"
#include "DefaultOptionSetInfo.h"
#include "Deployment.h"
#include "DynamicZone.h"
#include "EffectAddItem.h"
#include "EffectAddItemToCorpse.h"
#include "EffectCastingTrap.h"
#include "EffectContinualGroundAttack.h"
#include "EffectDarkness.h"
#include "EffectDecayCorpse.h"
#include "EffectDecayItem.h"
#include "EffectDeleteItem.h"
#include "EffectGnomesWhisper.h"
#include "EffectHasBloodBible.h"
#include "EffectHasCastleSymbol.h"
#include "EffectHasSlayerRelic.h"
#include "EffectHasVampireRelic.h"
#include "EffectLoaderManager.h"
#include "EffectManager.h"
#include "EffectObservingEye.h"
#include "EffectPKZoneRegen.h"
#include "EffectRelicTable.h"
#include "EffectSanctuary.h"
#include "EffectSchedule.h"
#include "EffectShrineGuard.h"
#include "EffectShrineHoly.h"
#include "EffectShrineShield.h"
#include "EffectSlayerRelic.h"
#include "EffectTransportItem.h"
#include "EffectTransportItemToCorpse.h"
#include "EffectVampirePortal.h"
#include "EffectVampireRelic.h"
#include "EventTransport.h"
#include "FlagSet.h"
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
#include "GDRLairManager.h"
#include "GGCommand.h"
#include "GQuestManager.h"
#include "GameContext.h"
#include "GamePlayer.h"
#include "GameServerInfoManager.h"
#include "GuildManager.h"
#include "GuildUnion.h"
#include "HolyLandManager.h"
#include "Item.h"
#include "ItemFactoryManager.h"
#include "ItemInfo.h"
#include "KernelContext.h"
#include "LevelWarManager.h"
#include "LevelWarZoneInfoManager.h"
#include "LoginServerManager.h"
#include "MasterLairInfoManager.h"
#include "MasterLairManager.h"
#include "Monster.h"
#include "MonsterCorpse.h"
#include "MonsterManager.h"
#include "NPC.h"
#include "NPCInfo.h"
#include "NPCManager.h"
#include "NicknameBook.h"
#include "Ousters.h"
#include "OustersCorpse.h"
#include "PCFinder.h"
#include "PCManager.h"
#include "PKZoneInfoManager.h"
#include "PacketUtil.h"
#include "ParkingCenter.h"
#include "Party.h"
#include "PaySystem.h"
#include "Player.h"
#include "Profile.h"
#include "Properties.h"
#include "QuestManager.h"
#include "RegenZoneManager.h"
#include "Relic.h"
#include "RelicUtil.h"
#include "ResurrectLocationManager.h"
#include "ShrineInfoManager.h"
#include "SiegeManager.h"
#include "SkillUtil.h"
#include "Slayer.h"
#include "SlayerCorpse.h"
#include "Store.h"
#include "StringPool.h"
#include "SweeperBonusManager.h"
#include "TimeManager.h"
#include "TradeManager.h"
#include "Vampire.h"
#include "VampireCorpse.h"
#include "VariableManager.h"
#include "VisionInfo.h"
#include "War.h"
#include "WarScheduler.h"
#include "WarSystem.h"
#include "WeatherManager.h"
#include "Zone.h"
#include "ZoneGroup.h"
#include "ZoneInfo.h"
#include "ZoneInfoManager.h"
#include "ZoneInternal.h"
#include "ZoneUtil.h"
#include "ctf/FlagManager.h"
#include "item/Motorcycle.h"
#include "item/VampirePortalItem.h"
#include "repository/ComebackEventRepository.h"
#include "repository/MessageRepository.h"
#include "repository/ZoneInfoRepository.h"


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

//////////////////////////////////////////////////////////////////////////////
// initialize zone
//////////////////////////////////////////////////////////////////////////////
void Zone::init()

{
    __BEGIN_TRY

#ifdef __USE_ENCRYPTER__
    int serverID = de::kernelContext().config().getPropertyInt("ServerID");

    if (!isDynamicZone()) {
        m_EncryptCode = EncryptCode(m_ZoneID, serverID);
    } else {
        m_EncryptCode = EncryptCode(m_pDynamicZone->getTemplateZoneID(), serverID);
    }
#endif

    load();

    m_pWeatherManager->init();

    DarkLightInfo* pDIInfo = NULL;

    switch (m_ZoneType) {
    case ZONE_NORMAL_FIELD:
        pDIInfo = de::gameContext().darkLights().getCurrentDarkLightInfo(this);
        m_DarkLevel = pDIInfo->getDarkLevel();
        m_LightLevel = pDIInfo->getLightLevel();
        break;
    case ZONE_NORMAL_DUNGEON:
    case ZONE_PC_VAMPIRE_LAIR:
    case ZONE_NPC_VAMPIRE_LAIR:
        m_DarkLevel = 15;
        m_LightLevel = 6;
        break;
    case ZONE_SLAYER_GUILD:
    case ZONE_RESERVED_SLAYER_GUILD:
    case ZONE_NPC_HOME:
    case ZONE_NPC_SHOP:
    case ZONE_CASTLE:
    case ZONE_RANDOM_MAP:
        m_DarkLevel = 0;
        m_LightLevel = 14;
        break;
    default:
        pDIInfo = de::gameContext().darkLights().getCurrentDarkLightInfo(this);
        m_DarkLevel = pDIInfo->getDarkLevel();
        m_LightLevel = pDIInfo->getLightLevel();
        break;
    }

    switch (m_ZoneID) {
    case 1131:
    case 1132:
    case 1133:
    case 1134: {
        m_pLevelWarManager = new LevelWarManager(m_ZoneID - 1130, this);
        m_pLevelWarManager->init();
        break;
    }

    case 1500: {
    } break;
    default:
        break;
    }

    m_bCastleZone = de::gameContext().castleInfos().isCastleZone(m_ZoneID);

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
// Read the zone information from the zone file and load it.
//////////////////////////////////////////////////////////////////////////////
void Zone::load(bool bOutput)

{
    __BEGIN_TRY
    __BEGIN_DEBUG

    Properties& config = de::kernelContext().config();

    try {
        CastleInfoManager& castleInfos = de::gameContext().castleInfos();

        DWORD versionLen;
        char* pVersionLen = (char*)(&versionLen);
        WORD zoneID;
        WORD zoneGroupID;
        DWORD zonenameLen;
        BYTE zoneType;
        BYTE zoneLevel;
        DWORD descLen;
        char* pDesc = NULL;
        char* version = new char[128];
        char* zonename = new char[128];
        char* lwrFilename = new char[256];

        ZoneInfo* pZoneInfo = de::gameContext().zoneInfos().getZoneInfo(m_ZoneID);

        Assert(pZoneInfo != NULL);

        setPayPlay(pZoneInfo->isPayPlay());
        setPremiumZone(pZoneInfo->isPremiumZone());
        setPKZone(pZoneInfo->isPKZone());
        setNoPortalZone(pZoneInfo->isNoPortalZone());
        setMasterLair(pZoneInfo->isMasterLair());
        setHolyLand(pZoneInfo->isHolyLand());

        // A Holy Land is registered with the HolyLandManager.
        if (isHolyLand()) {
            de::gameContext().holyLands().addHolyLand(this);
        }

        if (castleInfos.getCastleInfo(m_ZoneID) != NULL) {
            setCastle(true);
        } else {
            setCastle(false);
        }


        // Open the SMP information file.
        string SMPFilename = config.getProperty("HomePath") + "/data/" + pZoneInfo->getSMPFilename();
        ifstream SMP(SMPFilename.c_str(), ios::in | ios::binary);
        if (!SMP) {
            strcpy(lwrFilename, SMPFilename.c_str());
            strlwr(lwrFilename);
            SMP.open(lwrFilename, ios::in | ios::binary);


            if (!SMP) {
                StringStream msg;
                msg << SMPFilename << " not exist or cannot open it";
                cerr << msg.toString() << endl;
                throw FileNotExistException(msg.toString());
            }
        }

        // read zone version
        SMP.read(pVersionLen, szDWORD);
        SMP.read(version, versionLen);
        version[versionLen] = 0;

        // read zone id
        SMP.read((char*)&zoneID, szWORD);

        // read zone group id (no use)
        SMP.read((char*)&zoneGroupID, szWORD);

        // read zone name
        SMP.read((char*)&zonenameLen, szDWORD);
        if (zonenameLen > 0) {
            SMP.read(zonename, zonenameLen);
            zonename[zonenameLen] = 0;
        }

        // read zone type & level
        SMP.read((char*)&zoneType, szBYTE);
        SMP.read((char*)&zoneLevel, szBYTE);

        // read zone description
        SMP.read((char*)&descLen, szDWORD);
        if (descLen > 0) {
            pDesc = new char[descLen + 1];
            SMP.read(pDesc, descLen);
            pDesc[descLen] = 0;

            SAFE_DELETE_ARRAY(pDesc); // add '_ARRAY' moved to here.. by sigi 2002.5.2
        }

        // read zone width & height
        SMP.read((char*)&m_Width, szWORD);
        SMP.read((char*)&m_Height, szWORD);

        Assert(m_Width <= maxZoneWidth);
        Assert(m_Height <= maxZoneHeight);

        // DEBUG by tiancaiamao

        // Allocate the tiles as a two-dimensional array.
        m_pTiles = new Tile*[m_Width];
        for (uint i = 0; i < m_Width; i++) {
            m_pTiles[i] = new Tile[m_Height];
        }

        // Allocate the sectors as a two-dimensional array.
        m_SectorWidth = (int)ceil((float)m_Width / (float)SECTOR_SIZE);
        m_SectorHeight = (int)ceil((float)m_Height / (float)SECTOR_SIZE);
        m_pSectors = new Sector*[m_SectorWidth];
        for (int x = 0; x < m_SectorWidth; x++) {
            m_pSectors[x] = new Sector[m_SectorHeight];
        }

        // Set the sector pointer on each tile.
        for (int x = 0; x < m_Width; x++) {
            for (int y = 0; y < m_Height; y++) {
                int sx = x / SECTOR_SIZE;
                int sy = y / SECTOR_SIZE;

                Assert(sx < m_SectorWidth && sy < m_SectorHeight);

                m_pTiles[x][y].setSector(&m_pSectors[sx][sy]);
            }
        }

        // Link the sectors to each other.
        VSRect srect(0, 0, m_SectorWidth - 1, m_SectorHeight - 1);
        for (int x = 0; x < m_SectorWidth; x++) {
            for (int y = 0; y < m_SectorHeight; y++) {
                for (uint d = 0; d < 9; d++) {
                    int sectorx = x + dirMoveMask[d].x;
                    int sectory = y + dirMoveMask[d].y;

                    if (srect.ptInRect(sectorx, sectory)) {
                        m_pSectors[x][y].setNearbySector(d, &m_pSectors[sectorx][sectory]);
                    }
                }
            }
        }

        // Build the rectangles that divide the zone into regions for MonsterAI.
        m_OuterRect.set(0, 0, m_Width - 1, m_Height - 1);
        if (m_Width > 64 && m_Height > 64) {
            m_InnerRect.set(15, 15, m_Width - 15, m_Height - 15);
            m_CoreRect.set(25, 25, m_Width - 25, m_Height - 25);
        } else {
            m_InnerRect = m_CoreRect = m_OuterRect;
        }

        char str[80];
        char str2[80];

        for (ZoneCoord_t y = 0; y < m_Height; y++) {
            for (ZoneCoord_t x = 0; x < m_Width; x++) {
                BYTE flag = 0;
                SMP.read((char*)&flag, szBYTE);

                // Underground, ground and air blocking, in that order.
                if (flag & 0x01)
                    m_pTiles[x][y].setBlocked(Creature::MOVE_MODE_BURROWING);
                if (flag & 0x02)
                    m_pTiles[x][y].setBlocked(Creature::MOVE_MODE_WALKING);
                if (flag & 0x04)
                    m_pTiles[x][y].setBlocked(Creature::MOVE_MODE_FLYING);

                // Nothing at all is here..
                // Record the coordinate for monster spawning.
                if (flag == 0 && m_InnerRect.ptInRect(x, y)) {
                    m_MonsterRegenPositions.push_back(BPOINT((BYTE)x, (BYTE)y));
                }

                // For a master lair: find the places where at least one layer is not blocked.
                if ((flag & 0x07) != 0x07 && (isMasterLair() || m_ZoneID == 3002)) {
                    m_EmptyTilePositions.push_back(BPOINT((BYTE)x, (BYTE)y));
                }

                // Portal information.
                if (flag & 0x80) {
                    BYTE type;
                    ZoneID_t targetZoneID;
                    BYTE targetX, targetY;
                    SMP.read((char*)&type, szBYTE);

                    PortalType_t portalType = PORTAL_NORMAL;

                    bool bAddPortal = true;

                    if (type == PORTAL_NORMAL) {
                        SMP.read((char*)&targetZoneID, szZoneID);
                        SMP.read((char*)&targetX, szBYTE);
                        SMP.read((char*)&targetY, szBYTE);

                        // Create the portal.
                        NormalPortal* pNormalPortal = new NormalPortal();
                        pNormalPortal->setObjectType(PORTAL_NORMAL);
                        pNormalPortal->setZoneID(targetZoneID);
                        pNormalPortal->setX(targetX);
                        pNormalPortal->setY(targetY);

                        getObjectRegistry().registerObject(pNormalPortal);
                        m_pTiles[x][y].addPortal(pNormalPortal);

                        if (bOutput) {
                            cout << "Normal(" << (int)x << "," << (int)y << "," << (int)targetZoneID << ","
                                 << (int)targetX << "," << (int)targetY << ")" << endl;
                        }
                    } else if (type == PORTAL_SLAYER) {
                        SMP.read((char*)&targetZoneID, szZoneID);
                        SMP.read((char*)&targetX, szBYTE);
                        SMP.read((char*)&targetY, szBYTE);

                        // Create the portal.
                        NormalPortal* pNormalPortal = new NormalPortal();
                        pNormalPortal->setObjectType(PORTAL_SLAYER);
                        pNormalPortal->setZoneID(targetZoneID);
                        pNormalPortal->setX(targetX);
                        pNormalPortal->setY(targetY);

                        getObjectRegistry().registerObject(pNormalPortal);
                        m_pTiles[x][y].addPortal(pNormalPortal);

                        portalType = PORTAL_SLAYER;

                        if (bOutput) {
                            cout << "Slayer(" << (int)x << "," << (int)y << "," << (int)targetZoneID << ","
                                 << (int)targetX << "," << (int)targetY << ")" << endl;
                        }
                    } else if (type == PORTAL_VAMPIRE) {
                        SMP.read((char*)&targetZoneID, szZoneID);
                        SMP.read((char*)&targetX, szBYTE);
                        SMP.read((char*)&targetY, szBYTE);

                        // Create the portal.
                        NormalPortal* pNormalPortal = new NormalPortal();
                        pNormalPortal->setObjectType(PORTAL_VAMPIRE);
                        pNormalPortal->setZoneID(targetZoneID);
                        pNormalPortal->setX(targetX);
                        pNormalPortal->setY(targetY);

                        getObjectRegistry().registerObject(pNormalPortal);
                        m_pTiles[x][y].addPortal(pNormalPortal);

                        portalType = PORTAL_VAMPIRE;

                        if (bOutput) {
                            cout << "Vampire(" << (int)x << "," << (int)y << "," << (int)targetZoneID << ","
                                 << (int)targetX << "," << (int)targetY << ")" << endl;
                        }
                    } else if (type == PORTAL_MULTI_TARGET) {
                        BYTE size;
                        SMP.read((char*)&size, szBYTE);

                        // Create the portal.
                        MultiPortal* pMultiPortal = new MultiPortal();

                        for (int i = 0; i < size; i++) {
                            SMP.read((char*)&targetZoneID, szZoneID);
                            SMP.read((char*)&targetX, szBYTE);
                            SMP.read((char*)&targetY, szBYTE);

                            pMultiPortal->setObjectType(PORTAL_SLAYER);

                            // Build the target info.
                            PortalTargetInfo* pPortalTargetInfo = new PortalTargetInfo();
                            pPortalTargetInfo->setZoneID(targetZoneID);
                            pPortalTargetInfo->setX(targetX);
                            pPortalTargetInfo->setY(targetY);
                            pMultiPortal->setPortalTargetInfo(pPortalTargetInfo);
                        }

                        getObjectRegistry().registerObject(pMultiPortal);
                        m_pTiles[x][y].addPortal(pMultiPortal);

                        portalType = PORTAL_SLAYER;

                        if (bOutput) {
                            cout << "Multi(" << (int)x << "," << (int)y << "," << (int)targetZoneID << ","
                                 << (int)targetX << "," << (int)targetY << ")" << endl;
                        }
                    } else if (type == PORTAL_GUILD) {
                        SMP.read((char*)&targetZoneID, szZoneID);
                        SMP.read((char*)&targetX, szBYTE);
                        SMP.read((char*)&targetY, szBYTE);

                        // Create the portal.
                        GuildPortal* pGuildPortal = new GuildPortal();
                        pGuildPortal->setObjectType(PORTAL_GUILD);
                        pGuildPortal->setZoneID(targetZoneID);
                        pGuildPortal->setX(targetX);
                        pGuildPortal->setY(targetY);

                        getObjectRegistry().registerObject(pGuildPortal);
                        m_pTiles[x][y].addPortal(pGuildPortal);

                        if (bOutput) {
                            cout << "Guild(" << (int)x << "," << (int)y << "," << (int)targetZoneID << ","
                                 << (int)targetX << "," << (int)targetY << ")" << endl;
                        }

                    } else if (type == PORTAL_BATTLE) {
                        SMP.read((char*)&targetZoneID, szZoneID);
                        SMP.read((char*)&targetX, szBYTE);
                        SMP.read((char*)&targetY, szBYTE);

                        // Create the portal.
                        NormalPortal* pNormalPortal = new NormalPortal();
                        pNormalPortal->setObjectType(PORTAL_NORMAL);
                        pNormalPortal->setZoneID(targetZoneID);
                        pNormalPortal->setX(targetX);
                        pNormalPortal->setY(targetY);

                        getObjectRegistry().registerObject(pNormalPortal);
                        m_pTiles[x][y].addPortal(pNormalPortal);

                        if (bOutput) {
                            cout << "Battle(" << (int)x << "," << (int)y << "," << (int)targetZoneID << ","
                                 << (int)targetX << "," << (int)targetY << ")" << endl;
                        }
                    } else if (type == PORTAL_OUSTERS) {
                        SMP.read((char*)&targetZoneID, szZoneID);
                        SMP.read((char*)&targetX, szBYTE);
                        SMP.read((char*)&targetY, szBYTE);

                        // Create the portal.
                        NormalPortal* pNormalPortal = new NormalPortal();
                        pNormalPortal->setObjectType(PORTAL_OUSTERS);
                        pNormalPortal->setZoneID(targetZoneID);
                        pNormalPortal->setX(targetX);
                        pNormalPortal->setY(targetY);

                        getObjectRegistry().registerObject(pNormalPortal);
                        m_pTiles[x][y].addPortal(pNormalPortal);

                        portalType = PORTAL_OUSTERS;

                        if (bOutput) {
                            cout << "Ousters(" << (int)x << "," << (int)y << "," << (int)targetZoneID << ","
                                 << (int)targetX << "," << (int)targetY << ")" << endl;
                        }
                    } else {
                        bAddPortal = false;
                    }

                    // When a portal has been added and the destination zone is a
                    // pay zone, a TriggeredPortal has to be
                    // set up.
                    if (bAddPortal) {
                        ZoneInfo* pTargetZoneInfo = NULL;
                        try {
                            pTargetZoneInfo = de::gameContext().zoneInfos().getZoneInfo(targetZoneID);
                        } catch (NoSuchElementException& t) {
                            throw Error("No such zone");
                        }

                        Assert(pTargetZoneInfo != NULL);

                        // Should the existing Portal be deleted?
                        bool bDeleteOldPortal = false;

                        if ((pTargetZoneInfo->isPayPlay() && !pZoneInfo->isPayPlay()) ||
                            pTargetZoneInfo->isMasterLair() ||
                            (pTargetZoneInfo->isCastle() && !castleInfos.isCastleZone(targetZoneID, m_ZoneID)) ||
                            (pTargetZoneInfo->isHolyLand() && !pZoneInfo->isHolyLand()) ||
                            (isCastle() && castleInfos.isCastleZone(m_ZoneID, pTargetZoneInfo->getZoneID()))) {
                            bDeleteOldPortal = true;
                        }

                        Tile& rTile = m_pTiles[x][y];

                        // The case where the existing Portal is deleted.
                        if (bDeleteOldPortal) {
                            // Remove the portal that was already there.
                            if (rTile.hasPortal()) {
                                Portal* pOldPortal = rTile.getPortal();
                                rTile.deletePortal();

                                delete pOldPortal;
                            }
                        }

                        // Create the portal and register it.

                        //----------------------------------------
                        // For a master lair.
                        // by sigi. 2002.9.2
                        //----------------------------------------
                        if (pTargetZoneInfo->isMasterLair()) {
                            TriggeredPortal* pPortal = new TriggeredPortal();
                            getObjectRegistry().registerObject(pPortal);

                            // Load the portal contents.
                            pPortal->setObjectType(portalType);

                            TriggerManager& tm = pPortal->getTriggerManager();

                            Trigger* pTrigger = new Trigger(de::gameContext());

                            pTrigger->setTriggerID(0); // Not meaningful.

                            pTrigger->setTriggerType("QUEST");

                            snprintf(str, sizeof(str), "ConditionType : EnterMasterLair\n\t TargetZoneID : %d\n\t",
                                     (int)pTargetZoneInfo->getZoneID());
                            pTrigger->setConditions(str);
                            snprintf(str, sizeof(str),
                                     "ActionType : ActivatePortal\n\t ZoneID : %d\n\t X : %d\n\t Y : %d\n\t",
                                     targetZoneID, targetX, targetY);
                            pTrigger->setActions(str);

                            snprintf(str2, sizeof(str2), "ActionType : SystemMessage\n\t Content : %d",
                                     STRID_CANNOT_ENTER);

                            pTrigger->setCounterActions(str2);


                            tm.addTrigger(pTrigger);

                            // Attach the portal to the tile.
                            rTile.addPortal(pPortal);

                        }
                        //----------------------------------------
                        // When entering Adam's holy land.
                        //----------------------------------------
                        else if (pTargetZoneInfo->isHolyLand() && !pZoneInfo->isHolyLand()) {
                            TriggeredPortal* pPortal = new TriggeredPortal();
                            getObjectRegistry().registerObject(pPortal);

                            // Load the portal contents.
                            pPortal->setObjectType(portalType);

                            TriggerManager& tm = pPortal->getTriggerManager();

                            Trigger* pTrigger = new Trigger(de::gameContext());

                            pTrigger->setTriggerID(0); // Not meaningful.

                            pTrigger->setTriggerType("QUEST");

                            snprintf(str, sizeof(str), "ConditionType : EnterHolyLand\n\t TargetZoneID : %d\n\t",
                                     (int)pTargetZoneInfo->getZoneID());
                            pTrigger->setConditions(str);
                            snprintf(str, sizeof(str),
                                     "ActionType : ActivatePortal\n\t ZoneID : %d\n\t X : %d\n\t Y : %d\n\t",
                                     targetZoneID, targetX, targetY);
                            pTrigger->setActions(str);

                            snprintf(str2, sizeof(str2), "ActionType : SystemMessage\n\t Content : %d",
                                     STRID_CANNOT_ENTER_DURING_RACE_WAR);


                            pTrigger->setCounterActions(str2);


                            tm.addTrigger(pTrigger);

                            // Attach the portal to the tile.
                            rTile.addPortal(pPortal);

                        }
                        //----------------------------------------
                        // Entering the castle from outside it.
                        // isCastleZone checks whether the zone is one of those inside the castle.
                        // (The castle dungeon counts as inside the castle.)
                        // by bezz, Sequoia 2003. 1.20.
                        //----------------------------------------
                        else if (pTargetZoneInfo->isCastle() && !castleInfos.isCastleZone(targetZoneID, m_ZoneID)) {
                            TriggeredPortal* pPortal = new TriggeredPortal();
                            getObjectRegistry().registerObject(pPortal);

                            // Load the portal contents.
                            pPortal->setObjectType(portalType);

                            TriggerManager& tm = pPortal->getTriggerManager();

                            Trigger* pTrigger = new Trigger(de::gameContext());

                            pTrigger->setTriggerID(0); // Not meaningful.

                            pTrigger->setTriggerType("QUEST");

                            snprintf(str, sizeof(str), "ConditionType : EnterCastle\n\t TargetZoneID : %d\n\t",
                                     (int)pTargetZoneInfo->getZoneID());
                            pTrigger->setConditions(str);
                            snprintf(str, sizeof(str),
                                     "ActionType : ActivatePortal\n\t ZoneID : %d\n\t X : %d\n\t Y : %d\n\t",
                                     targetZoneID, targetX, targetY);
                            pTrigger->setActions(str);

                            snprintf(str2, sizeof(str2), "ActionType : SystemMessage\n\t Content : %d",
                                     STRID_CANNOT_ENTER);

                            pTrigger->setCounterActions(str2);


                            tm.addTrigger(pTrigger);

                            // Attach the portal to the tile.
                            rTile.addPortal(pPortal);

                        }
                        //----------------------------------------
                        // The entrance into the castle's underground dungeon.
                        // by Sequoia
                        //----------------------------------------
                        else if (isCastle() && castleInfos.isCastleZone(m_ZoneID, pTargetZoneInfo->getZoneID())) {
                            TriggeredPortal* pPortal = new TriggeredPortal();
                            getObjectRegistry().registerObject(pPortal);

                            // Load the portal contents.
                            pPortal->setObjectType(portalType);

                            TriggerManager& tm = pPortal->getTriggerManager();

                            Trigger* pTrigger = new Trigger(de::gameContext());

                            pTrigger->setTriggerID(0); // Not meaningful.
                            pTrigger->setTriggerType("QUEST");

                            snprintf(str, sizeof(str), "ConditionType : EnterCastleDungeon\n\t CastleZoneID : %d\n\t",
                                     m_ZoneID);
                            pTrigger->setConditions(str);
                            snprintf(str, sizeof(str),
                                     "ActionType : ActivatePortal\n\t ZoneID : %d\n\t X : %d\n\t Y : %d\n\t",
                                     targetZoneID, targetX, targetY);
                            pTrigger->setActions(str);

                            snprintf(str2, sizeof(str2), "ActionType : SystemMessage\n\t Content : %d",
                                     STRID_CANNOT_ENTER_NOT_OWNER_GUILD);

                            pTrigger->setCounterActions(str2);


                            tm.addTrigger(pTrigger);
                            rTile.addPortal(pPortal);
                        }
                        //----------------------------------------
                        // For a pay zone.
                        //----------------------------------------
                        else if (pTargetZoneInfo->isPayPlay() && !pZoneInfo->isPayPlay()) {
                            TriggeredPortal* pPortal = new TriggeredPortal();
                            getObjectRegistry().registerObject(pPortal);

                            // Load the portal contents.
                            pPortal->setObjectType(portalType);

                            TriggerManager& tm = pPortal->getTriggerManager();

                            Trigger* pTrigger = new Trigger(de::gameContext());

                            pTrigger->setTriggerID(0); // Not meaningful.

                            pTrigger->setTriggerType("QUEST");
                            pTrigger->setConditions("ConditionType : CanEnterPayZone\n\t");
                            snprintf(str, sizeof(str),
                                     "ActionType : ActivatePortal\n\t ZoneID : %d\n\t X : %d\n\t Y : %d\n\t",
                                     targetZoneID, targetX, targetY);
                            pTrigger->setActions(str);

                            // A player the pay-zone condition turns away is
                            // told which kind of zone refused them, the way
                            // the other pay-zone portal does. A NetMarble
                            // deployment bills through the portal and has no
                            // pay zone to name, so it refuses in general
                            // terms.
                            if (!de::isNetMarbleDeployment()) {
                                snprintf(str2, sizeof(str2), "ActionType : SystemMessage\n\t Content : %d",
                                         STRID_CANNOT_ENTER_PAY_ZONE);

                                pTrigger->setCounterActions(str2);

                            } else {
                                snprintf(str2, sizeof(str2), "ActionType : SystemMessage\n\t Content : %d",
                                         STRID_CANNOT_ENTER);

                                pTrigger->setCounterActions(str2);
                            }

                            tm.addTrigger(pTrigger);

                            // Attach the portal to the tile.
                            rTile.addPortal(pPortal);
                        }
                    }
                } // if (flag & 0x80)
            } // for
        } // for

        SMP.close();

        ///*
        if (m_MonsterRegenPositions.size() == 0) {
            printf("MonsterRegenPosition not exist: Width = %d, Height = %d\n", (int)m_Width, (int)m_Height);


            ZoneCoord_t outerMinX = m_Width / 7;
            ZoneCoord_t outerMinY = m_Height / 7;
            ZoneCoord_t outerMaxX = m_Width - outerMinX;
            ZoneCoord_t outerMaxY = m_Width - outerMinY;

            for (ZoneCoord_t y = outerMinY; y < outerMaxY; y++) {
                for (ZoneCoord_t x = outerMinX; x < outerMaxX; x++) {
                    Tile& rTile = m_pTiles[x][y];

                    if (!rTile.hasPortal() && !rTile.isGroundBlocked() && !rTile.isAirBlocked() &&
                        !rTile.isUndergroundBlocked()) {
                        m_MonsterRegenPositions.push_back(BPOINT((BYTE)x, (BYTE)y));
                    }
                }
            }

            Assert(m_MonsterRegenPositions.size() != 0);
        }

        if (isDynamicZone()) {
            cout << "MonsterRegenPositions(" << m_ZoneID << "," << m_MonsterRegenPositions.size() << ")" << endl;
        }

        if ((isMasterLair() || m_ZoneID == 3002) && m_EmptyTilePositions.size() == 0) {
            printf("MasterLair has No EmptyTilePosition\n");
            Assert(m_EmptyTilePositions.size() != 0);
        }
        //*/

        // Set the Zone information.
        m_ZoneType = pZoneInfo->getZoneType();
        m_ZoneLevel = pZoneInfo->getZoneLevel();

        // Allocate the memory...
        m_ppLevel = new ZoneLevel_t*[m_Width];
        for (uint i = 0; i < m_Width; i++)
            m_ppLevel[i] = new ZoneLevel_t[m_Height];

        // Initialize the zone level to its default value.
        for (ZoneCoord_t x = 0; x < m_Width; x++)
            for (ZoneCoord_t y = 0; y < m_Height; y++)
                m_ppLevel[x][y] = m_ZoneLevel;

        // Open the SSI information file.
        string SSIFilename = config.getProperty("HomePath") + "/data/" + pZoneInfo->getSSIFilename();
        ifstream SSI(SSIFilename.c_str(), ios::in | ios::binary);
        if (!SSI) {
            strcpy(lwrFilename, SSIFilename.c_str());
            strlwr(lwrFilename);
            SSI.open(lwrFilename, ios::in | ios::binary);


            if (!SSI) {
                StringStream msg;
                msg << SSIFilename << " not exist or cannot open it";
                throw FileNotExistException(msg.toString());
            }
        }

        int size = 0;
        SSI.read((char*)&size, szint);

        BYTE left, top, right, bottom, level;
        for (int i = 0; i < size; i++) {
            SSI.read((char*)&level, szBYTE);
            SSI.read((char*)&left, szBYTE);
            SSI.read((char*)&top, szBYTE);
            SSI.read((char*)&right, szBYTE);
            SSI.read((char*)&bottom, szBYTE);

            if (bOutput) {
                cout << "LEVEL:" << (int)level << ",(" << (int)left << "," << (int)top << "," << (int)right << ","
                     << (int)bottom << ")" << endl;
            }

            Assert(left <= right);
            Assert(top <= bottom);

            for (int bx = left; bx <= right; bx++)
                for (int by = top; by <= bottom; by++)
                    m_ppLevel[bx][by] = level;
        }

        SSI.close();

        // Load the triggered portals.
        loadTriggeredPortal();

        // Load the monsters....
        m_pMonsterManager->load();


        // For a master lair.
        // by sigi. 2002.9.2
        if (pZoneInfo->isMasterLair()) {
            SAFE_DELETE(m_pMasterLairManager);
            m_pMasterLairManager = new MasterLairManager(this);
        }

        // For a castle.
        // by sigi. 2003.1.24
        if (isCastle()) {
            SAFE_DELETE(m_pWarScheduler);
            m_pWarScheduler = new WarScheduler(this);
            m_pWarScheduler->load();

            printf("[%d] Castle : WarScheduler->load\n", (int)getZoneID());
        }

        // Load the NPCs.
        m_pNPCManager->load(m_ZoneID);
        //	}

        loadEffect();

        // Load the bulletin board.
        loadBulletinBoard(this);

        // Initialize the sprite counts.
        initSpriteCount();

        SAFE_DELETE_ARRAY(version);
        SAFE_DELETE_ARRAY(zonename);
        SAFE_DELETE_ARRAY(lwrFilename);


    } catch (Throwable& t) {
        cout << t.toString() << endl;
        Assert(false);
    }

    __END_DEBUG
    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
// Read the zone information from the zone file and load it.
//////////////////////////////////////////////////////////////////////////////
void Zone::reload(bool bOutput)

{
    __BEGIN_TRY
    __BEGIN_DEBUG
    try {
        DWORD versionLen;
        char version[128];
        WORD zoneID;
        WORD zoneGroupID;
        DWORD zonenameLen;
        char zonename[128];
        BYTE zoneType;
        BYTE zoneLevel;
        DWORD descLen;
        char* pDesc = NULL;
        char lwrFilename[256];

        ZoneInfo* pZoneInfo = de::gameContext().zoneInfos().getZoneInfo(m_ZoneID);

        Assert(pZoneInfo != NULL);

        setPayPlay(pZoneInfo->isPayPlay());
        setPremiumZone(pZoneInfo->isPremiumZone());
        setPKZone(pZoneInfo->isPKZone());
        setNoPortalZone(pZoneInfo->isNoPortalZone());
        setMasterLair(pZoneInfo->isMasterLair());


        // Open the SMP information file.
        string SMPFilename =
            de::kernelContext().config().getProperty("HomePath") + "/data/" + pZoneInfo->getSMPFilename();
        ifstream SMP(SMPFilename.c_str(), ios::in | ios::binary);
        if (!SMP) {
            strcpy(lwrFilename, SMPFilename.c_str());
            strlwr(lwrFilename);
            SMP.open(lwrFilename, ios::in | ios::binary);


            if (!SMP) {
                StringStream msg;
                msg << SMPFilename << " not exist or cannot open it";
                cerr << msg.toString() << endl;
                throw FileNotExistException(msg.toString());
            }
        }

        // read zone version
        SMP.read((char*)&versionLen, szDWORD);
        SMP.read(version, versionLen);
        version[versionLen] = 0;

        // read zone id
        SMP.read((char*)&zoneID, szWORD);

        // read zone group id (no use)
        SMP.read((char*)&zoneGroupID, szWORD);

        // read zone name
        SMP.read((char*)&zonenameLen, szDWORD);
        if (zonenameLen > 0) {
            SMP.read(zonename, zonenameLen);
            zonename[zonenameLen] = 0;
        }

        // read zone type & level
        SMP.read((char*)&zoneType, szBYTE);
        SMP.read((char*)&zoneLevel, szBYTE);

        // read zone description
        SMP.read((char*)&descLen, szDWORD);
        if (descLen > 0) {
            pDesc = new char[descLen + 1];
            SMP.read(pDesc, descLen);
            pDesc[descLen] = 0;

            SAFE_DELETE_ARRAY(pDesc); // add '_ARRAY' moved to here.. by sigi 2002.5.2
        }

        // read zone width & height
        SMP.read((char*)&m_Width, szWORD);
        SMP.read((char*)&m_Height, szWORD);

        Assert(m_Width <= maxZoneWidth);
        Assert(m_Height <= maxZoneHeight);

        uint i, x;


        if (m_pSectors == NULL) {
            // Allocate the sectors as a two-dimensional array.
            m_SectorWidth = (int)ceil((float)m_Width / (float)SECTOR_SIZE);
            m_SectorHeight = (int)ceil((float)m_Height / (float)SECTOR_SIZE);
            m_pSectors = new Sector*[m_SectorWidth];
            for (x = 0; (int)x < m_SectorWidth; x++) {
                m_pSectors[x] = new Sector[m_SectorHeight];
            }

            // Link the sectors to each other.
            VSRect srect(0, 0, m_SectorWidth - 1, m_SectorHeight - 1);
            for (int x = 0; x < m_SectorWidth; x++) {
                for (int y = 0; y < m_SectorHeight; y++) {
                    for (uint d = 0; d < 9; d++) {
                        int sectorx = x + dirMoveMask[d].x;
                        int sectory = y + dirMoveMask[d].y;

                        if (srect.ptInRect(sectorx, sectory)) {
                            m_pSectors[x][y].setNearbySector(d, &m_pSectors[sectorx][sectory]);
                        }
                    }
                }
            }
        }

        // If m_pTiles does not exist yet...
        if (m_pTiles == NULL) {
            // Allocate the tiles as a two-dimensional array.
            m_pTiles = new Tile*[m_Width];
            for (i = 0; i < m_Width; i++) {
                m_pTiles[i] = new Tile[m_Height];
            }

            // Set the sector pointer on each tile.
            for (int x = 0; x < m_Width; x++) {
                for (int y = 0; y < m_Height; y++) {
                    int sx = x / SECTOR_SIZE;
                    int sy = y / SECTOR_SIZE;

                    Assert(sx < m_SectorWidth && sy < m_SectorHeight);

                    m_pTiles[x][y].setSector(&m_pSectors[sx][sy]);
                }
            }
        }

        // Build the rectangles that divide the zone into regions for MonsterAI.
        m_OuterRect.set(0, 0, m_Width - 1, m_Height - 1);
        if (m_Width > 64 && m_Height > 64) {
            m_InnerRect.set(15, 15, m_Width - 15, m_Height - 15);
            m_CoreRect.set(25, 25, m_Width - 25, m_Height - 25);
        } else {
            m_InnerRect = m_CoreRect = m_OuterRect;
        }


        char str[80];
        char str2[80];

        // Start over.
        m_MonsterRegenPositions.clear();
        m_EmptyTilePositions.clear();

        for (ZoneCoord_t y = 0; y < m_Height; y++) {
            for (ZoneCoord_t x = 0; x < m_Width; x++) {
                BYTE flag = 0;
                SMP.read((char*)&flag, szBYTE);

                // Underground, ground and air blocking, in that order.
                if (flag & 0x01)
                    m_pTiles[x][y].setBlocked(Creature::MOVE_MODE_BURROWING);
                if (flag & 0x02)
                    m_pTiles[x][y].setBlocked(Creature::MOVE_MODE_WALKING);
                if (flag & 0x04)
                    m_pTiles[x][y].setBlocked(Creature::MOVE_MODE_FLYING);

                // Nothing at all is here..
                // Record the coordinate for monster spawning.
                if (flag == 0 && m_InnerRect.ptInRect(x, y)) {
                    m_MonsterRegenPositions.push_back(BPOINT((BYTE)x, (BYTE)y));
                }

                // For a master lair: find the places where at least one layer is not blocked.
                if ((flag & 0x07) != 0x07 && (isMasterLair() || m_ZoneID == 3002)) {
                    m_EmptyTilePositions.push_back(BPOINT((BYTE)x, (BYTE)y));
                }

                // Portal information.
                if (flag & 0x80) {
                    BYTE type;
                    ZoneID_t targetZoneID;
                    BYTE targetX, targetY;
                    SMP.read((char*)&type, szBYTE);

                    PortalType_t portalType = PORTAL_NORMAL;

                    bool bAddPortal = true;

                    // If a portal is already there, delete the existing one.
                    if (m_pTiles[x][y].hasPortal()) {
                        Portal* pPortal = m_pTiles[x][y].getPortal();
                        SAFE_DELETE(pPortal);
                        m_pTiles[x][y].deletePortal();
                    }

                    if (type == PORTAL_NORMAL) {
                        SMP.read((char*)&targetZoneID, szZoneID);
                        SMP.read((char*)&targetX, szBYTE);
                        SMP.read((char*)&targetY, szBYTE);

                        // Create the portal.
                        NormalPortal* pNormalPortal = new NormalPortal();
                        pNormalPortal->setObjectType(PORTAL_NORMAL);
                        pNormalPortal->setZoneID(targetZoneID);
                        pNormalPortal->setX(targetX);
                        pNormalPortal->setY(targetY);

                        getObjectRegistry().registerObject(pNormalPortal);
                        m_pTiles[x][y].addPortal(pNormalPortal);

                        if (bOutput) {
                            cout << "Normal(" << (int)x << "," << (int)y << "," << (int)targetZoneID << ","
                                 << (int)targetX << "," << (int)targetY << ")" << endl;
                        }
                    } else if (type == PORTAL_SLAYER) {
                        SMP.read((char*)&targetZoneID, szZoneID);
                        SMP.read((char*)&targetX, szBYTE);
                        SMP.read((char*)&targetY, szBYTE);

                        // Create the portal.
                        NormalPortal* pNormalPortal = new NormalPortal();
                        pNormalPortal->setObjectType(PORTAL_SLAYER);
                        pNormalPortal->setZoneID(targetZoneID);
                        pNormalPortal->setX(targetX);
                        pNormalPortal->setY(targetY);

                        getObjectRegistry().registerObject(pNormalPortal);
                        m_pTiles[x][y].addPortal(pNormalPortal);

                        portalType = PORTAL_SLAYER;

                        if (bOutput) {
                            cout << "Slayer(" << (int)x << "," << (int)y << "," << (int)targetZoneID << ","
                                 << (int)targetX << "," << (int)targetY << ")" << endl;
                        }
                    } else if (type == PORTAL_VAMPIRE) {
                        SMP.read((char*)&targetZoneID, szZoneID);
                        SMP.read((char*)&targetX, szBYTE);
                        SMP.read((char*)&targetY, szBYTE);

                        // Create the portal.
                        NormalPortal* pNormalPortal = new NormalPortal();
                        pNormalPortal->setObjectType(PORTAL_VAMPIRE);
                        pNormalPortal->setZoneID(targetZoneID);
                        pNormalPortal->setX(targetX);
                        pNormalPortal->setY(targetY);

                        getObjectRegistry().registerObject(pNormalPortal);
                        m_pTiles[x][y].addPortal(pNormalPortal);

                        portalType = PORTAL_VAMPIRE;

                        if (bOutput) {
                            cout << "Vampire(" << (int)x << "," << (int)y << "," << (int)targetZoneID << ","
                                 << (int)targetX << "," << (int)targetY << ")" << endl;
                        }
                    } else if (type == PORTAL_MULTI_TARGET) {
                        BYTE size;
                        SMP.read((char*)&size, szBYTE);

                        // Create the portal.
                        MultiPortal* pMultiPortal = new MultiPortal();

                        for (int i = 0; i < size; i++) {
                            SMP.read((char*)&targetZoneID, szZoneID);
                            SMP.read((char*)&targetX, szBYTE);
                            SMP.read((char*)&targetY, szBYTE);

                            pMultiPortal->setObjectType(PORTAL_SLAYER);

                            // Build the target info.
                            PortalTargetInfo* pPortalTargetInfo = new PortalTargetInfo();
                            pPortalTargetInfo->setZoneID(targetZoneID);
                            pPortalTargetInfo->setX(targetX);
                            pPortalTargetInfo->setY(targetY);
                            pMultiPortal->setPortalTargetInfo(pPortalTargetInfo);
                        }

                        getObjectRegistry().registerObject(pMultiPortal);
                        m_pTiles[x][y].addPortal(pMultiPortal);

                        portalType = PORTAL_SLAYER;

                        if (bOutput) {
                            cout << "Multi(" << (int)x << "," << (int)y << "," << (int)targetZoneID << ","
                                 << (int)targetX << "," << (int)targetY << ")" << endl;
                        }
                    } else if (type == PORTAL_GUILD) {
                        SMP.read((char*)&targetZoneID, szZoneID);
                        SMP.read((char*)&targetX, szBYTE);
                        SMP.read((char*)&targetY, szBYTE);

                        // Create the portal.
                        GuildPortal* pGuildPortal = new GuildPortal();
                        pGuildPortal->setObjectType(PORTAL_GUILD);
                        pGuildPortal->setZoneID(targetZoneID);
                        pGuildPortal->setX(targetX);
                        pGuildPortal->setY(targetY);

                        getObjectRegistry().registerObject(pGuildPortal);
                        m_pTiles[x][y].addPortal(pGuildPortal);

                        if (bOutput) {
                            cout << "Guild(" << (int)x << "," << (int)y << "," << (int)targetZoneID << ","
                                 << (int)targetX << "," << (int)targetY << ")" << endl;
                        }

                    } else if (type == PORTAL_BATTLE) {
                        SMP.read((char*)&targetZoneID, szZoneID);
                        SMP.read((char*)&targetX, szBYTE);
                        SMP.read((char*)&targetY, szBYTE);

                        // Create the portal.
                        NormalPortal* pNormalPortal = new NormalPortal();
                        pNormalPortal->setObjectType(PORTAL_NORMAL);
                        pNormalPortal->setZoneID(targetZoneID);
                        pNormalPortal->setX(targetX);
                        pNormalPortal->setY(targetY);

                        getObjectRegistry().registerObject(pNormalPortal);
                        m_pTiles[x][y].addPortal(pNormalPortal);

                        if (bOutput) {
                            cout << "Slayer(" << (int)x << "," << (int)y << "," << (int)targetZoneID << ","
                                 << (int)targetX << "," << (int)targetY << ")" << endl;
                        }
                    } else {
                        bAddPortal = false;
                    }

                    // When a portal has been added and the destination zone is a
                    // pay zone, a TriggeredPortal has to be
                    // set up.
                    if (bAddPortal) {
                        ZoneInfo* pTargetZoneInfo = NULL;
                        try {
                            pTargetZoneInfo = de::gameContext().zoneInfos().getZoneInfo(targetZoneID);
                        } catch (NoSuchElementException& t) {
                            throw Error("No such zone");
                        }

                        Assert(pTargetZoneInfo != NULL);

                        // Should the existing Portal be deleted?
                        bool bDeleteOldPortal = false;

                        if ((pTargetZoneInfo->isPayPlay() && !pZoneInfo->isPayPlay()) ||
                            pTargetZoneInfo->isMasterLair() ||
                            (pTargetZoneInfo->isCastle() &&
                             !de::gameContext().castleInfos().isCastleZone(targetZoneID, m_ZoneID)) ||
                            (pTargetZoneInfo->isHolyLand() && !pZoneInfo->isHolyLand()) ||
                            (isCastle() &&
                             de::gameContext().castleInfos().isCastleZone(m_ZoneID, pTargetZoneInfo->getZoneID()))) {
                            bDeleteOldPortal = true;
                        }


                        Tile& rTile = m_pTiles[x][y];

                        // The case where the existing Portal is deleted.
                        if (bDeleteOldPortal) {
                            // Remove the portal that was already there.
                            if (rTile.hasPortal()) {
                                Portal* pOldPortal = rTile.getPortal();
                                rTile.deletePortal();

                                delete pOldPortal;
                            }
                        }

                        // Create the portal and register it.

                        //----------------------------------------
                        // For a master lair.
                        // by sigi. 2002.9.2
                        //----------------------------------------
                        if (pTargetZoneInfo->isMasterLair()) {
                            TriggeredPortal* pPortal = new TriggeredPortal();
                            getObjectRegistry().registerObject(pPortal);

                            // Load the portal contents.
                            pPortal->setObjectType(portalType);

                            TriggerManager& tm = pPortal->getTriggerManager();

                            Trigger* pTrigger = new Trigger(de::gameContext());

                            pTrigger->setTriggerID(0); // Not meaningful.

                            pTrigger->setTriggerType("QUEST");

                            snprintf(str, sizeof(str), "ConditionType : EnterMasterLair\n\t TargetZoneID : %d\n\t",
                                     (int)pTargetZoneInfo->getZoneID());
                            pTrigger->setConditions(str);
                            snprintf(str, sizeof(str),
                                     "ActionType : ActivatePortal\n\t ZoneID : %d\n\t X : %d\n\t Y : %d\n\t",
                                     targetZoneID, targetX, targetY);
                            pTrigger->setActions(str);

                            snprintf(str2, sizeof(str2), "ActionType : SystemMessage\n\t Content : %d",
                                     STRID_CANNOT_ENTER);
                            pTrigger->setCounterActions(str2);


                            tm.addTrigger(pTrigger);

                            // Attach the portal to the tile.
                            rTile.addPortal(pPortal);

                        }
                        //----------------------------------------
                        // Entering a pay zone.
                        //----------------------------------------
                        else if (pTargetZoneInfo->isPayPlay() && !pZoneInfo->isPayPlay()) {
                            TriggeredPortal* pPortal = new TriggeredPortal();
                            getObjectRegistry().registerObject(pPortal);

                            // Load the portal contents.
                            pPortal->setObjectType(portalType);

                            TriggerManager& tm = pPortal->getTriggerManager();

                            Trigger* pTrigger = new Trigger(de::gameContext());

                            pTrigger->setTriggerID(0); // Not meaningful.

                            pTrigger->setTriggerType("QUEST");
                            pTrigger->setConditions("ConditionType : PayPlay\n\t");
                            snprintf(str, sizeof(str),
                                     "ActionType : ActivatePortal\n\t ZoneID : %d\n\t X : %d\n\t Y : %d\n\t",
                                     targetZoneID, targetX, targetY);
                            pTrigger->setActions(str);

                            snprintf(str2, sizeof(str2), "ActionType : SystemMessage\n\t Content : %d",
                                     STRID_CANNOT_ENTER_PAY_ZONE);
                            pTrigger->setCounterActions(str2);


                            tm.addTrigger(pTrigger);

                            // Attach the portal to the tile.
                            rTile.addPortal(pPortal);
                        }
                    }


                } // if (flag & 0x80)
            } // for
        } // for

        SMP.close();

        ///*
        if (m_MonsterRegenPositions.size() == 0) {
            cout << "MonsterRegenPosition not exist" << endl;
            cout << "Width = " << m_Width << endl;
            cout << "Height = " << m_Height << endl;


            ZoneCoord_t outerMinX = m_Width / 7;
            ZoneCoord_t outerMinY = m_Height / 7;
            ZoneCoord_t outerMaxX = m_Width - outerMinX;
            ZoneCoord_t outerMaxY = m_Width - outerMinY;

            for (ZoneCoord_t y = outerMinY; y < outerMaxY; y++) {
                for (ZoneCoord_t x = outerMinX; x < outerMaxX; x++) {
                    Tile& rTile = m_pTiles[x][y];

                    if (!rTile.hasPortal() && !rTile.isGroundBlocked() && !rTile.isAirBlocked() &&
                        !rTile.isUndergroundBlocked()) {
                        m_MonsterRegenPositions.push_back(BPOINT((BYTE)x, (BYTE)y));
                    }
                }
            }

            Assert(m_MonsterRegenPositions.size() != 0);
        }

        if ((isMasterLair() || m_ZoneID == 3002) && m_EmptyTilePositions.size() == 0) {
            cout << "MasterLair has No EmptyTilePosition" << endl;
            Assert(m_EmptyTilePositions.size() != 0);
        }
        // */

        // Set the Zone information.
        m_ZoneType = pZoneInfo->getZoneType();
        m_ZoneLevel = pZoneInfo->getZoneLevel();

        // Release m_ppLevel.
        for (i = 0; i < m_Width; i++) {
            SAFE_DELETE_ARRAY(m_ppLevel[i]);
        }
        SAFE_DELETE_ARRAY(m_ppLevel);

        // Allocate the memory...
        m_ppLevel = new ZoneLevel_t*[m_Width];
        for (uint i = 0; i < m_Width; i++)
            m_ppLevel[i] = new ZoneLevel_t[m_Height];

        // Initialize the zone level to its default value.
        for (ZoneCoord_t x = 0; x < m_Width; x++)
            for (ZoneCoord_t y = 0; y < m_Height; y++)
                m_ppLevel[x][y] = m_ZoneLevel;

        // Open the SSI information file.
        string SSIFilename =
            de::kernelContext().config().getProperty("HomePath") + "/data/" + pZoneInfo->getSSIFilename();
        ifstream SSI(SSIFilename.c_str(), ios::in | ios::binary);
        if (!SSI) {
            strcpy(lwrFilename, SSIFilename.c_str());
            strlwr(lwrFilename);
            SSI.open(lwrFilename, ios::in | ios::binary);


            if (!SSI) {
                StringStream msg;
                msg << SSIFilename << " not exist or cannot open it";
                throw FileNotExistException(msg.toString());
            }
        }

        int size = 0;
        SSI.read((char*)&size, szint);

        BYTE left, top, right, bottom, level;
        for (int i = 0; i < size; i++) {
            SSI.read((char*)&level, szBYTE);
            SSI.read((char*)&left, szBYTE);
            SSI.read((char*)&top, szBYTE);
            SSI.read((char*)&right, szBYTE);
            SSI.read((char*)&bottom, szBYTE);

            if (bOutput) {
                cout << "LEVEL:" << (int)level << ",(" << (int)left << "," << (int)top << "," << (int)right << ","
                     << (int)bottom << ")" << endl;
            }

            Assert(left <= right);
            Assert(top <= bottom);

            for (int bx = left; bx <= right; bx++)
                for (int by = top; by <= bottom; by++)
                    m_ppLevel[bx][by] = level;
        }

        SSI.close();

        // Load the triggered portals.
        // Skipped on reload.

        // Load the monsters....
        m_pMonsterManager->load();

        // eventMonsterManager is skipped on reload.

        // For a master lair.
        // by sigi. 2002.9.2
        if (pZoneInfo->isMasterLair()) {
            if (m_pMasterLairManager != NULL &&
                m_pMasterLairManager->getCurrentEvent() == MasterLairManager::EVENT_WAITING_REGEN) {
                SAFE_DELETE(m_pMasterLairManager);
                m_pMasterLairManager = new MasterLairManager(this);
            }
        }

        // For a castle.
        // by sigi. 2003.1.24
        if (pZoneInfo->isCastle()) {
            if (m_pWarScheduler != NULL)
            //&& m_pWarScheduler->getCurrentEvent()==WarScheduler::EVENT_WAITING_REGEN)
            {
                SAFE_DELETE(m_pWarScheduler);
                m_pWarScheduler = new WarScheduler(this);
            }
        }

        // Skipped on reload.
        // Load the NPCs.

        // Initialize the sprite counts.
        initSpriteCount();
    } catch (Throwable& t) {
        cout << t.toString() << endl;
        Assert(false);
    }

    __END_DEBUG
    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
// Load the triggered portals for the current zone.
//////////////////////////////////////////////////////////////////////////////
void Zone::loadTriggeredPortal()

{
    __BEGIN_TRY

    // A dynamic zone loads its template zone's triggers.
    ZoneID_t zoneID = m_ZoneID;
    if (isDynamicZone()) {
        zoneID = m_pDynamicZone->getTemplateZoneID();
    }

    vector<ZoneRectRow> rects = defaultZoneInfoRepository().loadTriggerRects(zoneID);

    for (size_t r = 0; r < rects.size(); r++) {
        int left = rects[r].left;
        int top = rects[r].top;
        int right = rects[r].right;
        int bottom = rects[r].bottom;

        Assert(left <= right);
        Assert(top <= bottom);

        Assert(m_OuterRect.ptInRect(left, top));
        Assert(m_OuterRect.ptInRect(right, bottom));

        for (int x = left; x <= right; x++) {
            for (int y = top; y <= bottom; y++) {
                if (getTile(x, y).hasPortal()) {
                    getTile(x, y).deletePortal();
                }

                // Create and register the portal.
                TriggeredPortal* pPortal = new TriggeredPortal();
                getObjectRegistry().registerObject(pPortal);

                // Load the portal's contents.
                pPortal->setObjectType(PORTAL_NORMAL);
                pPortal->load(zoneID, left, top, right, bottom);

                // Attach the portal to the tile.
                getTile(x, y).addPortal(pPortal);
            }
        }
    }

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
// Count the NPC and monster sprites that appear in this zone.
//////////////////////////////////////////////////////////////////////////////
void Zone::initSpriteCount()

{
    __BEGIN_TRY

    m_NPCCount = 0;
    m_MonsterCount = 0;

    // Count the NPC sprite types.
    const unordered_map<ObjectID_t, Creature*>& NPCMap = m_pNPCManager->getCreatures();
    for (unordered_map<ObjectID_t, Creature*>::const_iterator i = NPCMap.begin(); i != NPCMap.end(); i++) {
        NPC* pNPC = dynamic_cast<NPC*>(i->second);
        bool bAdd = true;

        for (int j = 0; j < m_NPCCount; j++) // among the NPC types already counted
        {
            if (pNPC->getSpriteType() == m_NPCTypes[j]) {
                bAdd = false;
                break;
            }
        }

        if (bAdd) {
            m_NPCTypes[m_NPCCount] = pNPC->getSpriteType();
            m_NPCCount++;
        }
    }

    // Count the monster sprite types.
    const unordered_map<SpriteType_t, MonsterCounter*>& MONSTER = m_pMonsterManager->getMonsters();
    for (unordered_map<SpriteType_t, MonsterCounter*>::const_iterator i = MONSTER.begin(); i != MONSTER.end(); i++) {
        Assert(m_MonsterCount < maxMonsterPerZone); // by sigi

        m_MonsterTypes[m_MonsterCount] = i->first;
        m_MonsterCount++;
    }

    __END_CATCH
}

void Zone::loadNPCs(Race_t race)

{
    __BEGIN_TRY

    m_pNPCManager->load(getZoneID(), race);

    sendNPCInfo();

    __END_CATCH
}

void Zone::loadEffect()

{
    __BEGIN_TRY

    ///////////////////////////////////////////////////////////////////////////////
    // Load the EffectPKZoneRegen rectangles.
    ///////////////////////////////////////////////////////////////////////////////
    vector<ZoneRectRow> regenRects = defaultZoneInfoRepository().loadPKZoneRegenRects(getZoneID());

    for (size_t r = 0; r < regenRects.size(); r++) {
        ZoneCoord_t left = regenRects[r].left;
        ZoneCoord_t top = regenRects[r].top;
        ZoneCoord_t right = regenRects[r].right;
        ZoneCoord_t bottom = regenRects[r].bottom;

        EffectPKZoneRegen* pEffect = new EffectPKZoneRegen(this, left, top, right, bottom);
        pEffect->setSlayer();
        pEffect->setVampire();
        pEffect->setOusters();
        pEffect->setTurn(10);
        pEffect->setHP(40);
        pEffect->setNextTime(0);

        registerObject(pEffect);
        addEffect(pEffect);
    }

    ///////////////////////////////////////////////////////////////////////////////
    // Load the Gnome's Horn way points: Ousters standing on the 3x3 tiles
    // around a way point regain 1 HP and MP per second.
    ///////////////////////////////////////////////////////////////////////////////
    vector<ZonePointRow> wayPoints = defaultZoneInfoRepository().loadWayPoints(getZoneID(), RACE_OUSTERS);

    for (size_t w = 0; w < wayPoints.size(); w++) {
        ZoneCoord_t X = wayPoints[w].x;
        ZoneCoord_t Y = wayPoints[w].y;

        if (isValidZoneCoord(this, X - 1, Y - 1) && isValidZoneCoord(this, X + 1, Y + 1)) {
            EffectPKZoneRegen* pEffect = new EffectPKZoneRegen(this, X - 1, Y - 1, X + 1, Y + 1);
            pEffect->setOusters();
            pEffect->setTurn(10);
            pEffect->setHP(4);
            pEffect->setNextTime(0);

            registerObject(pEffect);
            addEffect(pEffect);
        }
    }

    de::gameContext().effectLoaders().load(this);

    if (m_ZoneID == 3002) {
        EffectContinualGroundAttack* pEffect =
            new EffectContinualGroundAttack(this, Effect::EFFECT_CLASS_GROUND_ATTACK, 3);
        pEffect->setDeadline(99999999);
        pEffect->setNumber(7, 11);

        registerObject(pEffect);
        addEffect(pEffect);
    }

    __END_CATCH
}
