//////////////////////////////////////////////////////////////////////////////
// Filename    : PacketUtil.cpp
// Written by  : excel96
// Description :
// Packets that are sent often and are complicated to build are built in
// this one place, which makes maintenance easier.
//////////////////////////////////////////////////////////////////////////////

#include "PacketUtil.h"

#include "AR.h"
#include "Belt.h"
#include "CastleInfoManager.h"
#include "Corpse.h"
#include "Effect.h"
#include "EventSystemMessage.h"
#include "GCAddEffect.h"
#include "GCAddMonster.h"
#include "GCAddMonsterCorpse.h"
#include "GCAddNPC.h"
#include "GCAddNewItemToZone.h"
#include "GCAddOusters.h"
#include "GCAddOustersCorpse.h"
#include "GCAddSlayer.h"
#include "GCAddSlayerCorpse.h"
#include "GCAddVampire.h"
#include "GCAddVampireCorpse.h"
#include "GCCreateItem.h"
#include "GCDropItemToZone.h"
#include "GCMiniGameScores.h"
#include "GCModifyInformation.h"
#include "GCOtherModifyInfo.h"
#include "GCPetStashList.h"
#include "GCSystemMessage.h"
#include "GCUpdateInfo.h"
#include "GCWarScheduleList.h"
#include "GameContext.h"
#include "GamePlayer.h"
#include "GameServerInfo.h"
#include "GameServerInfoManager.h"
#include "Inventory.h"
#include "Item.h"
#include "ItemInfo.h"
#include "ItemInfoManager.h"
#include "ItemNameInfo.h"
#include "Mine.h"
#include "ModifyInfo.h"
#include "Monster.h"
#include "MonsterCorpse.h"
#include "NPC.h"
#include "NPCInfo.h"
#include "Ousters.h"
#include "OustersArmsband.h"
#include "OustersCorpse.h"
#include "PCSlayerInfo2.h"
#include "PCVampireInfo2.h"
#include "PKZoneInfoManager.h"
#include "PetItem.h"
#include "PlayerCreature.h"
#include "Properties.h"
#include "RideMotorcycleInfo.h"
#include "SG.h"
#include "SMG.h"
#include "SR.h"
#include "Slayer.h"
#include "SlayerCorpse.h"
#include "SubItemInfo.h"
#include "TimeManager.h"
#include "Vampire.h"
#include "VampireCorpse.h"
#include "WarScheduler.h"
#include "WeatherManager.h"
#include "Zone.h"
#include "ZoneGroup.h"
#include "ZoneGroupManager.h"
#include "ZonePlayerManager.h"
#include "ZoneUtil.h"
// #include "GCItemNameInfoList.h"

#include <stdio.h>

#include <list>

#include "Assert.h"
#include "DynamicZone.h"
#include "GuildManager.h"
#include "GuildUnion.h"
#include "KernelContext.h"
#include "PCFinder.h"
#include "PlayerMailbox.h"
#include "ServerContext.h"
#include "Store.h"
#include "domain/DarkLight.h"
#include "repository/PlayRecordRepository.h"

// Runs on the thread that owns pc, under its group mutex: the broadcast
// walks pc's zone.
static void broadcastGuildUnionChange(PlayerCreature& pc) {
    Zone* pZone = pc.getZone();
    if (pZone == NULL)
        return;

    GCOtherModifyInfo gcOtherModifyInfo;
    makeGCOtherModifyInfoGuildUnion(&gcOtherModifyInfo, &pc);
    pZone->broadcastPacket(pc.getX(), pc.getY(), &gcOtherModifyInfo, &pc);
}

// Callable from any thread: the members are named under the finder's lock
// and each broadcast is posted to the thread that owns the member, so no
// zone-group mutex is taken here.
void sendGCOtherModifyInfoGuildUnionByGuildID(uint gID)

{
    __BEGIN_TRY

    PCFinder& pcFinder = de::gameContext().playerCreatures();
    list<string> names;

    __ENTER_CRITICAL_SECTION(pcFinder)

    names = pcFinder.getGuildPlayerNames_LOCKED(gID);

    __LEAVE_CRITICAL_SECTION(pcFinder)

    for (list<string>::const_iterator itr = names.begin(); itr != names.end(); itr++) {
        de::postToPlayer(*itr, [](PlayerCreature& pc, Player& player) { broadcastGuildUnionChange(pc); });
    }

    __END_CATCH
}
// Send GCOtherModifyInfo to every member of the guild the creature belongs to.
void sendGCOtherModifyInfoGuildUnion(Creature* pTargetCreature)

{
    __BEGIN_TRY

    PlayerCreature* pTargetPlayerCreature = dynamic_cast<PlayerCreature*>(pTargetCreature);
    Assert(pTargetPlayerCreature != NULL);

    sendGCOtherModifyInfoGuildUnionByGuildID(pTargetPlayerCreature->getGuildID());

    __END_CATCH
}


void makeGCOtherModifyInfoGuildUnion(GCOtherModifyInfo* pModifyInformation, Creature* pCreature)

{
    __BEGIN_TRY

    GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(pCreature->getPlayer());
    Assert(pGamePlayer != NULL);

    PlayerCreature* pPlayerCreature = dynamic_cast<PlayerCreature*>(pGamePlayer->getCreature());
    Assert(pPlayerCreature != NULL);

    GuildUnion* pUnion = NULL;
    pUnion = GuildUnionManager::Instance().getGuildUnion(pPlayerCreature->getGuildID());

    // No guild union.
    if (pUnion == NULL) {
        pModifyInformation->addShortData(MODIFY_UNIONID, 0);
        pModifyInformation->addShortData(MODIFY_UNIONGRADE, GCUpdateInfo::UNION_NOTHING);

        // cout << "GCModifyInfo->GuildInformation - NOT FOUND UNION / UNION_NOTHING" << endl;
    } else {
        // There is a guild union.
        bool isGuildMaster = false;
        bool isGuildUnionMaster = false;

        pModifyInformation->addShortData(MODIFY_UNIONID, pUnion->getUnionID());

        if (de::gameContext().guilds().isGuildMaster(pPlayerCreature->getGuildID(), pPlayerCreature)) {
            isGuildMaster = true;
        }

        if (pUnion->getMasterGuildID() == pPlayerCreature->getGuildID()) {
            isGuildUnionMaster = true;
        }

        if (isGuildMaster && isGuildUnionMaster) {
            pModifyInformation->addShortData(MODIFY_UNIONGRADE, GCUpdateInfo::UNION_MASTER);
            // cout << "GCModifyInfo->GuildInformation - " << (int)pUnion->getUnionID() << " / UNION_MASTER" << endl;
        } else if (isGuildMaster && !isGuildUnionMaster) {
            pModifyInformation->addShortData(MODIFY_UNIONGRADE, GCUpdateInfo::UNION_GUILD_MASTER);
            // cout << "GCModifyInfo->GuildInformation - " << (int)pUnion->getUnionID() << " / UNION_GUILD_MASTER" <<
            // endl;
        } else {
            pModifyInformation->addShortData(MODIFY_UNIONGRADE, GCUpdateInfo::UNION_NOTHING);
            //			cout << "GCModifyInfo->GuildInformation - " << (int)pUnion->getUnionID() << " / UNION_NOTHING"
            //<< endl;
        }
    }


    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
// void makeGCModifyInformation for GuildUnion Info()
//
// Put the guild Union information into GCModifyInformation.
//////////////////////////////////////////////////////////////////////////////


void makeGCModifyInfoGuildUnion(GCModifyInformation* pModifyInformation, Creature* pCreature)

{
    __BEGIN_TRY

    GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(pCreature->getPlayer());
    Assert(pGamePlayer != NULL);

    PlayerCreature* pPlayerCreature = dynamic_cast<PlayerCreature*>(pGamePlayer->getCreature());
    Assert(pPlayerCreature != NULL);

    GuildUnion* pUnion = NULL;
    pUnion = GuildUnionManager::Instance().getGuildUnion(pPlayerCreature->getGuildID());

    // No guild union.
    if (pUnion == NULL) {
        pModifyInformation->addShortData(MODIFY_UNIONID, 0);
        pModifyInformation->addShortData(MODIFY_UNIONGRADE, GCUpdateInfo::UNION_NOTHING);

        //		cout << "GCModifyInfo->GuildInformation - NOT FOUND UNION / UNION_NOTHING" << endl;
    } else {
        // There is a guild union.
        bool isGuildMaster = false;
        bool isGuildUnionMaster = false;

        pModifyInformation->addShortData(MODIFY_UNIONID, pUnion->getUnionID());

        if (de::gameContext().guilds().isGuildMaster(pPlayerCreature->getGuildID(), pPlayerCreature)) {
            isGuildMaster = true;
        }

        if (pUnion->getMasterGuildID() == pPlayerCreature->getGuildID()) {
            isGuildUnionMaster = true;
        }

        if (isGuildMaster && isGuildUnionMaster) {
            pModifyInformation->addShortData(MODIFY_UNIONGRADE, GCUpdateInfo::UNION_MASTER);
            //			cout << "GCModifyInfo->GuildInformation - " << (int)pUnion->getUnionID() << " / UNION_MASTER" <<
            // endl;
        } else if (isGuildMaster && !isGuildUnionMaster) {
            pModifyInformation->addShortData(MODIFY_UNIONGRADE, GCUpdateInfo::UNION_GUILD_MASTER);
            //			cout << "GCModifyInfo->GuildInformation - " << (int)pUnion->getUnionID() << " /
            // UNION_GUILD_MASTER" << endl;
        } else {
            pModifyInformation->addShortData(MODIFY_UNIONGRADE, GCUpdateInfo::UNION_NOTHING);
            //			cout << "GCModifyInfo->GuildInformation - " << (int)pUnion->getUnionID() << " / UNION_NOTHING"
            //<< endl;
        }
    }


    __END_CATCH
}


//////////////////////////////////////////////////////////////////////////////
// void makeGCUpdateInfo()
//
// Build the GCUpdateInfo used when moving between maps by portal or by dying.
//////////////////////////////////////////////////////////////////////////////
void makeGCUpdateInfo(GCUpdateInfo* pUpdateInfo, Creature* pCreature)

{
    __BEGIN_TRY

    ////////////////////////////////////////////////////////////
    // Zone position information.
    ////////////////////////////////////////////////////////////
    Zone* pZone = pCreature->getZone();
    Assert(pZone != NULL);

    ZoneCoord_t x = pCreature->getX();
    ZoneCoord_t y = pCreature->getY();

    ZoneGroup* pZoneGroup = pZone->getZoneGroup();
    Assert(pZoneGroup != NULL);

    pUpdateInfo->setZoneID(pZone->getZoneID());
    pUpdateInfo->setGameTime(de::gameContext().worldTime().getGameTime());

    pUpdateInfo->setZoneX(x);
    pUpdateInfo->setZoneY(y);

    // DynamicZone handling.
    if (pZone->isDynamicZone()) {
        DynamicZone* pDynamicZone = pZone->getDynamicZone();
        Assert(pDynamicZone != NULL);

        pUpdateInfo->setZoneID(pDynamicZone->getTemplateZoneID());
    }

    ////////////////////////////////////////////////////////////
    // Inventory and gear information.
    ////////////////////////////////////////////////////////////
    if (pCreature->isSlayer()) {
        Slayer* pSlayer = dynamic_cast<Slayer*>(pCreature);
        Assert(pSlayer != NULL);

        pUpdateInfo->setPCInfo(pSlayer->getSlayerInfo2());

        // Inventory and Gear information.
        pUpdateInfo->setInventoryInfo(pSlayer->getInventoryInfo());
        pUpdateInfo->setGearInfo(pSlayer->getGearInfo());
        pUpdateInfo->setExtraInfo(pSlayer->getExtraInfo());

        if (pSlayer->hasRideMotorcycle())
            pUpdateInfo->setRideMotorcycleInfo(pSlayer->getRideMotorcycleInfo());

        pUpdateInfo->setSMSCharge(pSlayer->getSMSCharge());
        pUpdateInfo->setNicknameInfo(pSlayer->getNickname());
    } else if (pCreature->isVampire()) {
        Vampire* pVampire = dynamic_cast<Vampire*>(pCreature);
        Assert(pVampire != NULL);

        pUpdateInfo->setPCInfo(pVampire->getVampireInfo2());

        // Inventory and Gear information.
        pUpdateInfo->setInventoryInfo(pVampire->getInventoryInfo());
        pUpdateInfo->setGearInfo(pVampire->getGearInfo());
        pUpdateInfo->setExtraInfo(pVampire->getExtraInfo());

        pUpdateInfo->setSMSCharge(pVampire->getSMSCharge());
        pUpdateInfo->setNicknameInfo(pVampire->getNickname());
    } else if (pCreature->isOusters()) {
        Ousters* pOusters = dynamic_cast<Ousters*>(pCreature);
        Assert(pOusters != NULL);

        pUpdateInfo->setPCInfo(pOusters->getOustersInfo2());

        // Inventory and Gear information.
        pUpdateInfo->setInventoryInfo(pOusters->getInventoryInfo());
        pUpdateInfo->setGearInfo(pOusters->getGearInfo());
        pUpdateInfo->setExtraInfo(pOusters->getExtraInfo());

        pUpdateInfo->setSMSCharge(pOusters->getSMSCharge());
        pUpdateInfo->setNicknameInfo(pOusters->getNickname());
    }

    ////////////////////////////////////////////////////////////
    // Effect information.
    ////////////////////////////////////////////////////////////
    pUpdateInfo->setEffectInfo(pCreature->getEffectInfo());

    ////////////////////////////////////////////////////////////
    // Sight information: the zone's dark and light levels as this
    // player sees them, by its race, the zone's type and its effects.
    // Both callers pass a player creature, so it is one of the three
    // races.
    ////////////////////////////////////////////////////////////
    decore::DarkLightViewer viewer = {};
    viewer.race = pCreature->isVampire()   ? decore::DarkLightRace::Vampire
                  : pCreature->isOusters() ? decore::DarkLightRace::Ousters
                                           : decore::DarkLightRace::Slayer;
    viewer.castleZone = pZone->getZoneType() == ZONE_CASTLE;
    viewer.pkZone = de::gameContext().pkZoneInfos().isPKZone(pZone->getZoneID());
    viewer.lightness = pCreature->isFlag(Effect::EFFECT_CLASS_LIGHTNESS);
    viewer.yellowPoison = pCreature->isFlag(Effect::EFFECT_CLASS_YELLOW_POISON_TO_CREATURE);

    const decore::DarkLight levels = decore::darkLightForViewer(viewer, pZone->getDarkLevel(), pZone->getLightLevel());
    pUpdateInfo->setDarkLevel(levels.darkLevel);
    pUpdateInfo->setLightLevel(levels.lightLevel);

    ////////////////////////////////////////////////////////////
    // Weather information.
    ////////////////////////////////////////////////////////////
    pUpdateInfo->setWeather(pZone->getWeatherManager()->getCurrentWeather());
    pUpdateInfo->setWeatherLevel(pZone->getWeatherManager()->getWeatherLevel());

    ////////////////////////////////////////////////////////////
    // NPC sprite information.
    ////////////////////////////////////////////////////////////
    pUpdateInfo->setNPCCount(pZone->getNPCCount());
    for (uint i = 0; i < pZone->getNPCCount(); i++)
        pUpdateInfo->setNPCType(i, pZone->getNPCType(i));

    ////////////////////////////////////////////////////////////
    // Monster sprite information.
    ////////////////////////////////////////////////////////////
    // Preload the monsters summoned in a master lair.
    if (pZone->isMasterLair()) {
        // These are actually SpriteTypes.
        const int num = 25;
        const MonsterType_t mtypes[num] = {
            27,  // Blood Warlock
            40,  // Golemer
            41,  // Dirty Strider
            47,  // Chaos Guardian
            48,  // Hoble
            57,  // Shadow Wing
            61,  // Widows
            62,  // Estroider
            64,  // Moderas
            68,  // Big Fang
            70,  // Dark Screamer
            71,  // Chaos Knight
            72,  // Crimson Slaughter
            73,  // Lord Darkness
            74,  // Reaper
            75,  // Hell Guardian
            76,  // Hell Wizard
            88,  // Dark Guardian
            89,  // Lord Chaos
            90,  // Chaos Greed
            91,  // Hell Fiend
            92,  // Dark Haze
            101, // Dun Wolfarch
            102, // Mum Rimmon
            103  // Shaman Oaf

            // 27, 48, 40, 41, 57,
            // 61, 62, 64, 68, 71,
            // 73, 76, 89, 90, 91,
            // 92,103,102, 101
        };

        pUpdateInfo->setMonsterCount(num);
        for (int i = 0; i < num; i++)
            pUpdateInfo->setMonsterType(i, mtypes[i]);
    } else {
        pUpdateInfo->setMonsterCount(pZone->getMonsterCount());
        for (uint i = 0; i < pZone->getMonsterCount(); i++)
            pUpdateInfo->setMonsterType(i, pZone->getMonsterType(i));
    }

    ////////////////////////////////////////////////////////////
    // NPC coordinate information.
    ////////////////////////////////////////////////////////////
    list<NPCInfo*>* pNPCInfos = pZone->getNPCInfos();
    list<NPCInfo*>::const_iterator itr = pNPCInfos->begin();
    for (; itr != pNPCInfos->end(); itr++) {
        NPCInfo* pInfo = *itr;
        pUpdateInfo->addNPCInfo(pInfo);
    }
    ////////////////////////////////////////////////////////////
    // Server status information.
    ////////////////////////////////////////////////////////////
    ServerGroupID_t ZoneGroupCount = de::gameContext().zoneGroups().size();
    UserNum_t ZoneUserNum = 0;

    for (int i = 1; i < ZoneGroupCount + 1; i++) {
        ZoneGroup* pZoneGroup;

        try {
            pZoneGroup = de::gameContext().zoneGroups().getZoneGroup(i);
        } catch (NoSuchElementException&) {
            throw Error("Critical Error : ZoneInfoManager has no such zone group.");
        }

        ZonePlayerManager* pZonePlayerManager = pZoneGroup->getZonePlayerManager();
        ZoneUserNum += pZonePlayerManager->size();
    }

    int UserModify = 0;

    // ServerGroupID_t CurrentServerGroupID = de::kernelContext().config().getPropertyInt( "ServerID" );

    UserModify = 1000;

    if (ZoneUserNum < 100 + UserModify) {
        pUpdateInfo->setServerStat(SERVER_FREE);
    } else if (ZoneUserNum < 250 + UserModify) {
        pUpdateInfo->setServerStat(SERVER_NORMAL);
    } else if (ZoneUserNum < 400 + UserModify) {
        pUpdateInfo->setServerStat(SERVER_BUSY);
    } else if (ZoneUserNum < 500 + UserModify) {
        pUpdateInfo->setServerStat(SERVER_VERY_BUSY);
    } else if (ZoneUserNum >= 800 + UserModify) {
        pUpdateInfo->setServerStat(SERVER_FULL);
    } else {
        pUpdateInfo->setServerStat(SERVER_DOWN);
    }

    // Set the premium information.
    if (pZone->isPremiumZone())
        pUpdateInfo->setPremiumZone();

    GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(pCreature->getPlayer());
    Assert(pGamePlayer != NULL);

    if (pGamePlayer->isPremiumPlay())
        pUpdateInfo->setPremiumPlay();

    static const int serverID = de::kernelContext().config().getPropertyInt("ServerID");
    static const int worldID = de::kernelContext().config().getPropertyInt("WorldID");
    static bool bNonPK = de::serverContext().serverInfos().getGameServerInfo(1, serverID, worldID)->isNonPKServer();

    if (bNonPK) {
        pUpdateInfo->setNonPK(1);
        //		cout << "PK not allowed" << endl;
    } else {
        pUpdateInfo->setNonPK(0);
        //		cout << "PK allowed" << endl;
    }

    // GuildUnion Information
    PlayerCreature* pPlayerCreature = dynamic_cast<PlayerCreature*>(pGamePlayer->getCreature());
    Assert(pPlayerCreature != NULL);

    GuildUnion* pUnion = NULL;
    pUnion = GuildUnionManager::Instance().getGuildUnion(pPlayerCreature->getGuildID());

    // No guild union.
    if (pUnion == NULL) {
        pUpdateInfo->setGuildUnionID(0);
        pUpdateInfo->setGuildUnionUserType(GCUpdateInfo::UNION_NOTHING);
        //		cout << "GCUpdateInfo->getGuildUnionUserType() : UNION_NOTHING (NOT UNION)" << endl;
    } else {
        // There is a guild union.
        bool isGuildMaster = false;
        bool isGuildUnionMaster = false;

        pUpdateInfo->setGuildUnionID(pUnion->getUnionID());

        if (de::gameContext().guilds().isGuildMaster(pPlayerCreature->getGuildID(), pPlayerCreature)) {
            isGuildMaster = true;
        }

        if (pUnion->getMasterGuildID() == pPlayerCreature->getGuildID()) {
            isGuildUnionMaster = true;
        }

        if (isGuildMaster && isGuildUnionMaster) {
            pUpdateInfo->setGuildUnionUserType(GCUpdateInfo::UNION_MASTER);
            //			cout << "GCUpdateInfo->getGuildUnionUserType() : UNION_MASTER" << endl;
        } else if (isGuildMaster && !isGuildUnionMaster) {
            pUpdateInfo->setGuildUnionUserType(GCUpdateInfo::UNION_GUILD_MASTER);
            //			cout << "GCUpdateInfo->getGuildUnionUserType() : UNION_GUILD_MASTER" << endl;
        } else {
            pUpdateInfo->setGuildUnionUserType(GCUpdateInfo::UNION_NOTHING);
            //			cout << "GCUpdateInfo->getGuildUnionUserType() : UNION_NOTHING" << endl;
        }
    }

    pUpdateInfo->setBloodBibleSignInfo(pPlayerCreature->getBloodBibleSign());

    // Power point.
    pUpdateInfo->setPowerPoint(pPlayerCreature->getPowerPoint());

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
// Build the packet that adds a slayer.
//////////////////////////////////////////////////////////////////////////////
void makeGCAddSlayer(GCAddSlayer* pAddSlayer, Slayer* pSlayer)

{
    __BEGIN_TRY

    pAddSlayer->setSlayerInfo(pSlayer->getSlayerInfo3());
    pAddSlayer->setEffectInfo(pSlayer->getEffectInfo());
    pAddSlayer->setPetInfo(pSlayer->getPetInfo());
    pAddSlayer->setNicknameInfo(pSlayer->getNickname());
    pAddSlayer->setStoreInfo(&(pSlayer->getStore()->getStoreInfo()));

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
// Build the packet that adds a vampire.
//////////////////////////////////////////////////////////////////////////////
void makeGCAddVampire(GCAddVampire* pAddVampire, Vampire* pVampire)

{
    __BEGIN_TRY

    pAddVampire->setVampireInfo(pVampire->getVampireInfo3());
    pAddVampire->setEffectInfo(pVampire->getEffectInfo());
    pAddVampire->setPetInfo(pVampire->getPetInfo());
    pAddVampire->setNicknameInfo(pVampire->getNickname());
    pAddVampire->setStoreInfo(&(pVampire->getStore()->getStoreInfo()));

    // cout << "makeGCAddVampire: CoatType=" << (int)(pAddVampire->getVampireInfo().getCoatType()) << endl;

    // If the move was made through a personal portal...
    if (pVampire->isFlag(Effect::EFFECT_CLASS_VAMPIRE_PORTAL))
        pAddVampire->setFromFlag(1);

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
// Build the packet that adds an ousters.
//////////////////////////////////////////////////////////////////////////////
void makeGCAddOusters(GCAddOusters* pAddOusters, Ousters* pOusters)

{
    __BEGIN_TRY

    pAddOusters->setOustersInfo(pOusters->getOustersInfo3());
    pAddOusters->setEffectInfo(pOusters->getEffectInfo());
    pAddOusters->setPetInfo(pOusters->getPetInfo());
    pAddOusters->setNicknameInfo(pOusters->getNickname());
    pAddOusters->setStoreInfo(&(pOusters->getStore()->getStoreInfo()));

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
// Build the packet that adds a monster.
//////////////////////////////////////////////////////////////////////////////
void makeGCAddMonster(GCAddMonster* pAddMonster, Monster* pMonster)

{
    __BEGIN_TRY

    pAddMonster->setObjectID(pMonster->getObjectID());
    pAddMonster->setMonsterType(pMonster->getMonsterType());
    pAddMonster->setMonsterName(pMonster->getName());
    pAddMonster->setX(pMonster->getX());
    pAddMonster->setY(pMonster->getY());
    pAddMonster->setDir(pMonster->getDir());
    pAddMonster->setEffectInfo(pMonster->getEffectInfo());
    pAddMonster->setCurrentHP(pMonster->getHP());
    pAddMonster->setMaxHP(pMonster->getHP(ATTR_MAX));

    // If the move was made through a personal portal...
    if (pMonster->isFlag(Effect::EFFECT_CLASS_VAMPIRE_PORTAL))
        pAddMonster->setFromFlag(1);

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
// Build the packet that adds an NPC.
//////////////////////////////////////////////////////////////////////////////
void makeGCAddNPC(GCAddNPC* pAddNPC, NPC* pNPC)

{
    __BEGIN_TRY

    pAddNPC->setObjectID(pNPC->getObjectID());
    pAddNPC->setName(pNPC->getName());
    pAddNPC->setNPCID(pNPC->getNPCID());
    pAddNPC->setSpriteType(pNPC->getSpriteType());
    pAddNPC->setMainColor(pNPC->getMainColor());
    pAddNPC->setSubColor(pNPC->getSubColor());
    pAddNPC->setX(pNPC->getX());
    pAddNPC->setY(pNPC->getY());
    pAddNPC->setDir(pNPC->getDir());

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
// Build the GCAddNewItemToZone sent when a new item is added to the zone.
//////////////////////////////////////////////////////////////////////////////
void makeGCAddNewItemToZone(GCAddNewItemToZone* pAddNewItemToZone, Item* pItem, int X, int Y)

{
    __BEGIN_TRY

    Item::ItemClass IClass = pItem->getItemClass();

    pAddNewItemToZone->setObjectID(pItem->getObjectID());
    pAddNewItemToZone->setX(X);
    pAddNewItemToZone->setY(Y);
    pAddNewItemToZone->setItemClass(IClass);
    pAddNewItemToZone->setItemType(pItem->getItemType());
    pAddNewItemToZone->setOptionType(pItem->getOptionTypeList());
    pAddNewItemToZone->setDurability(pItem->getDurability());
    pAddNewItemToZone->setSilver(pItem->getSilver());
    pAddNewItemToZone->setGrade(pItem->getGrade());
    pAddNewItemToZone->setEnchantLevel(pItem->getEnchantLevel());
    pAddNewItemToZone->setItemNum(pItem->getNum());

    // Gun-family weapons carry the bullet count in the item count.
    if (IClass == Item::ITEM_CLASS_AR) {
        AR* pAR = dynamic_cast<AR*>(pItem);
        pAddNewItemToZone->setItemNum(pAR->getBulletCount());
    } else if (IClass == Item::ITEM_CLASS_SG) {
        SG* pSG = dynamic_cast<SG*>(pItem);
        pAddNewItemToZone->setItemNum(pSG->getBulletCount());
    } else if (IClass == Item::ITEM_CLASS_SMG) {
        SMG* pSMG = dynamic_cast<SMG*>(pItem);
        pAddNewItemToZone->setItemNum(pSMG->getBulletCount());
    } else if (IClass == Item::ITEM_CLASS_SR) {
        SR* pSR = dynamic_cast<SR*>(pItem);
        pAddNewItemToZone->setItemNum(pSR->getBulletCount());
    }
    // For a belt, the potions and magazines inside must be sent as well.
    else if (IClass == Item::ITEM_CLASS_BELT) {
        Belt* pBelt = dynamic_cast<Belt*>(pItem);
        Inventory* pBeltInventory = pBelt->getInventory();
        BYTE SubItemCount = 0;

        // Read the item information for as many pockets as there are.
        for (int i = 0; i < pBelt->getPocketCount(); i++) {
            Item* pBeltItem = pBeltInventory->getItem(i, 0);
            if (pBeltItem != NULL) {
                SubItemInfo* pSubItemInfo = new SubItemInfo();
                pSubItemInfo->setObjectID(pBeltItem->getObjectID());
                pSubItemInfo->setItemClass(pBeltItem->getItemClass());
                pSubItemInfo->setItemType(pBeltItem->getItemType());
                pSubItemInfo->setItemNum(pBeltItem->getNum());
                pSubItemInfo->setSlotID(i);

                pAddNewItemToZone->addListElement(pSubItemInfo);

                SubItemCount++;
            }
        }

        pAddNewItemToZone->setListNum(SubItemCount);
    }
    // For an armsband, the potions and magazines inside must be sent as well.
    else if (IClass == Item::ITEM_CLASS_OUSTERS_ARMSBAND) {
        OustersArmsband* pOustersArmsband = dynamic_cast<OustersArmsband*>(pItem);
        Inventory* pOustersArmsbandInventory = pOustersArmsband->getInventory();
        BYTE SubItemCount = 0;

        // Read the item information for as many pockets as there are.
        for (int i = 0; i < pOustersArmsband->getPocketCount(); i++) {
            Item* pOustersArmsbandItem = pOustersArmsbandInventory->getItem(i, 0);
            if (pOustersArmsbandItem != NULL) {
                SubItemInfo* pSubItemInfo = new SubItemInfo();
                pSubItemInfo->setObjectID(pOustersArmsbandItem->getObjectID());
                pSubItemInfo->setItemClass(pOustersArmsbandItem->getItemClass());
                pSubItemInfo->setItemType(pOustersArmsbandItem->getItemType());
                pSubItemInfo->setItemNum(pOustersArmsbandItem->getNum());
                pSubItemInfo->setSlotID(i);

                pAddNewItemToZone->addListElement(pSubItemInfo);

                SubItemCount++;
            }
        }

        pAddNewItemToZone->setListNum(SubItemCount);
    }

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
// Build the GCDropItemToZone sent when an item is dropped into the zone.
//////////////////////////////////////////////////////////////////////////////
void makeGCDropItemToZone(GCDropItemToZone* pDropItemToZone, Item* pItem, int X, int Y)

{
    __BEGIN_TRY

    Item::ItemClass IClass = pItem->getItemClass();

    pDropItemToZone->setObjectID(pItem->getObjectID());
    pDropItemToZone->setX(X);
    pDropItemToZone->setY(Y);
    pDropItemToZone->setItemClass(IClass);
    pDropItemToZone->setItemType(pItem->getItemType());
    pDropItemToZone->setOptionType(pItem->getOptionTypeList());
    pDropItemToZone->setDurability(pItem->getDurability());
    pDropItemToZone->setSilver(pItem->getSilver());
    pDropItemToZone->setGrade(pItem->getGrade());
    pDropItemToZone->setEnchantLevel(pItem->getEnchantLevel());
    pDropItemToZone->setItemNum(pItem->getNum());

    // Gun-family weapons carry the bullet count in the item count.
    if (IClass == Item::ITEM_CLASS_AR) {
        AR* pAR = dynamic_cast<AR*>(pItem);
        pDropItemToZone->setItemNum(pAR->getBulletCount());
    } else if (IClass == Item::ITEM_CLASS_SG) {
        SG* pSG = dynamic_cast<SG*>(pItem);
        pDropItemToZone->setItemNum(pSG->getBulletCount());
    } else if (IClass == Item::ITEM_CLASS_SMG) {
        SMG* pSMG = dynamic_cast<SMG*>(pItem);
        pDropItemToZone->setItemNum(pSMG->getBulletCount());
    } else if (IClass == Item::ITEM_CLASS_SR) {
        SR* pSR = dynamic_cast<SR*>(pItem);
        pDropItemToZone->setItemNum(pSR->getBulletCount());
    }
    // For a belt, the potions and magazines inside must be sent as well.
    else if (IClass == Item::ITEM_CLASS_BELT) {
        Belt* pBelt = dynamic_cast<Belt*>(pItem);
        Inventory* pBeltInventory = pBelt->getInventory();
        BYTE SubItemCount = 0;

        // Read the item information for as many pockets as there are.
        for (int i = 0; i < pBelt->getPocketCount(); i++) {
            Item* pBeltItem = pBeltInventory->getItem(i, 0);
            if (pBeltItem != NULL) {
                SubItemInfo* pSubItemInfo = new SubItemInfo();
                pSubItemInfo->setObjectID(pBeltItem->getObjectID());
                pSubItemInfo->setItemClass(pBeltItem->getItemClass());
                pSubItemInfo->setItemType(pBeltItem->getItemType());
                pSubItemInfo->setItemNum(pBeltItem->getNum());
                pSubItemInfo->setSlotID(i);

                pDropItemToZone->addListElement(pSubItemInfo);

                SubItemCount++;
            }
        }

        pDropItemToZone->setListNum(SubItemCount);
    }
    // For an armsband, the potions and magazines inside must be sent as well.
    else if (IClass == Item::ITEM_CLASS_OUSTERS_ARMSBAND) {
        OustersArmsband* pOustersArmsband = dynamic_cast<OustersArmsband*>(pItem);
        Inventory* pOustersArmsbandInventory = pOustersArmsband->getInventory();
        BYTE SubItemCount = 0;

        // Read the item information for as many pockets as there are.
        for (int i = 0; i < pOustersArmsband->getPocketCount(); i++) {
            Item* pOustersArmsbandItem = pOustersArmsbandInventory->getItem(i, 0);
            if (pOustersArmsbandItem != NULL) {
                SubItemInfo* pSubItemInfo = new SubItemInfo();
                pSubItemInfo->setObjectID(pOustersArmsbandItem->getObjectID());
                pSubItemInfo->setItemClass(pOustersArmsbandItem->getItemClass());
                pSubItemInfo->setItemType(pOustersArmsbandItem->getItemType());
                pSubItemInfo->setItemNum(pOustersArmsbandItem->getNum());
                pSubItemInfo->setSlotID(i);

                pDropItemToZone->addListElement(pSubItemInfo);

                SubItemCount++;
            }
        }

        pDropItemToZone->setListNum(SubItemCount);
    }

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
// Build the packet sent when a slayer corpse is added to the zone.
//////////////////////////////////////////////////////////////////////////////
void makeGCAddSlayerCorpse(GCAddSlayerCorpse* pAddSlayerCorpse, SlayerCorpse* pSlayerCorpse)

{
    __BEGIN_TRY

    pAddSlayerCorpse->setSlayerInfo(pSlayerCorpse->getSlayerInfo());
    pAddSlayerCorpse->setTreasureCount(pSlayerCorpse->getTreasureCount());

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
// Build the packet sent when a vampire corpse is added to the zone.
//////////////////////////////////////////////////////////////////////////////
void makeGCAddVampireCorpse(GCAddVampireCorpse* pAddVampireCorpse, VampireCorpse* pVampireCorpse)

{
    __BEGIN_TRY

    pAddVampireCorpse->setVampireInfo(pVampireCorpse->getVampireInfo());
    pAddVampireCorpse->setTreasureCount(pVampireCorpse->getTreasureCount());

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
// Build the packet sent when a monster corpse is added to the zone.
//////////////////////////////////////////////////////////////////////////////
void makeGCAddMonsterCorpse(GCAddMonsterCorpse* pAddMonsterCorpse, MonsterCorpse* pMonsterCorpse, int X, int Y)

{
    __BEGIN_TRY

    pAddMonsterCorpse->setObjectID(pMonsterCorpse->getObjectID());
    pAddMonsterCorpse->setMonsterType(pMonsterCorpse->getMonsterType());
    pAddMonsterCorpse->setMonsterName(pMonsterCorpse->getMonsterName());
    pAddMonsterCorpse->setX(X);
    pAddMonsterCorpse->setY(Y);
    pAddMonsterCorpse->setDir(pMonsterCorpse->getDir());
    pAddMonsterCorpse->setTreasureCount(pMonsterCorpse->getTreasureCount());
    pAddMonsterCorpse->sethasHead(pMonsterCorpse->gethasHead());
    pAddMonsterCorpse->setLastKiller(pMonsterCorpse->getLastKiller());

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
// Build the packet sent when an ousters corpse is added to the zone.
//////////////////////////////////////////////////////////////////////////////
void makeGCAddOustersCorpse(GCAddOustersCorpse* pAddOustersCorpse, OustersCorpse* pOustersCorpse)

{
    __BEGIN_TRY

    pAddOustersCorpse->setOustersInfo(pOustersCorpse->getOustersInfo());
    pAddOustersCorpse->setTreasureCount(pOustersCorpse->getTreasureCount());

    __END_CATCH
}
//////////////////////////////////////////////////////////////////////////////
// Build the GCOtherModifyInfo sent when something like another
// player's maximum HP has changed.
//////////////////////////////////////////////////////////////////////////////
void makeGCOtherModifyInfo(GCOtherModifyInfo* pInfo, Slayer* pSlayer, const SLAYER_RECORD* prev) {
    SLAYER_RECORD cur;
    pSlayer->getSlayerRecord(cur);

    pInfo->setObjectID(pSlayer->getObjectID());

    if (prev->pHP[ATTR_CURRENT] != cur.pHP[ATTR_CURRENT])
        pInfo->addShortData(MODIFY_CURRENT_HP, cur.pHP[ATTR_CURRENT]);
    if (prev->pHP[ATTR_MAX] != cur.pHP[ATTR_MAX])
        pInfo->addShortData(MODIFY_MAX_HP, cur.pHP[ATTR_MAX]);
}

void makeGCOtherModifyInfo(GCOtherModifyInfo* pInfo, Vampire* pVampire, const VAMPIRE_RECORD* prev) {
    VAMPIRE_RECORD cur;
    pVampire->getVampireRecord(cur);

    pInfo->setObjectID(pVampire->getObjectID());

    if (prev->pHP[ATTR_CURRENT] != cur.pHP[ATTR_CURRENT])
        pInfo->addShortData(MODIFY_CURRENT_HP, cur.pHP[ATTR_CURRENT]);
    if (prev->pHP[ATTR_MAX] != cur.pHP[ATTR_MAX])
        pInfo->addShortData(MODIFY_MAX_HP, cur.pHP[ATTR_MAX]);
}

void makeGCOtherModifyInfo(GCOtherModifyInfo* pInfo, Ousters* pOusters, const OUSTERS_RECORD* prev) {
    OUSTERS_RECORD cur;
    pOusters->getOustersRecord(cur);

    pInfo->setObjectID(pOusters->getObjectID());

    if (prev->pHP[ATTR_CURRENT] != cur.pHP[ATTR_CURRENT])
        pInfo->addShortData(MODIFY_CURRENT_HP, cur.pHP[ATTR_CURRENT]);
    if (prev->pHP[ATTR_MAX] != cur.pHP[ATTR_MAX])
        pInfo->addShortData(MODIFY_MAX_HP, cur.pHP[ATTR_MAX]);
}

void makeGCCreateItem(GCCreateItem* pGCCreateItem, Item* pItem, CoordInven_t x, CoordInven_t y)

{
    pGCCreateItem->setObjectID(pItem->getObjectID());
    pGCCreateItem->setItemClass((BYTE)pItem->getItemClass());
    pGCCreateItem->setItemType(pItem->getItemType());
    pGCCreateItem->setOptionType(pItem->getOptionTypeList());
    pGCCreateItem->setDurability(pItem->getDurability());
    pGCCreateItem->setEnchantLevel(pItem->getEnchantLevel());
    pGCCreateItem->setSilver(pItem->getSilver());
    pGCCreateItem->setGrade(pItem->getGrade());
    pGCCreateItem->setItemNum(pItem->getNum());
    pGCCreateItem->setInvenX(x);
    pGCCreateItem->setInvenY(y);

    if (pItem->getItemClass() == Item::ITEM_CLASS_PET_ITEM) {
        PetItem* pPetItem = dynamic_cast<PetItem*>(pItem);
        list<OptionType_t> olist;

        if (pPetItem->getPetInfo()->getPetOption() != 0)
            olist.push_back(pPetItem->getPetInfo()->getPetOption());

        pGCCreateItem->setOptionType(olist);
        pGCCreateItem->setDurability(pPetItem->getPetInfo()->getPetHP());
        pGCCreateItem->setEnchantLevel(pPetItem->getPetInfo()->getPetAttr());
        pGCCreateItem->setSilver(pPetItem->getPetInfo()->getPetAttrLevel());
        pGCCreateItem->setGrade((pPetItem->getPetInfo()->getPetHP() == 0)
                                    ? (pPetItem->getPetInfo()->getLastFeedTime().daysTo(VSDateTime::currentDateTime()))
                                    : (-1));
        pGCCreateItem->setItemNum(pPetItem->getPetInfo()->getPetLevel());
    }
}

void sendPayInfo(GamePlayer* pGamePlayer)

{
    __BEGIN_TRY

    __END_CATCH
}

// Broadcast the LevelUp effect to the surroundings.
void sendEffectLevelUp(Creature* pCreature)

{
    __BEGIN_TRY

    Assert(pCreature != NULL);
    // Assert(pCreature->isPC());

    // Broadcast it to the surroundings.
    GCAddEffect gcAddEffect;
    gcAddEffect.setObjectID(pCreature->getObjectID());
    gcAddEffect.setDuration(10); // Not very meaningful, but set to a short fixed duration.

    if (pCreature->isSlayer()) {
        gcAddEffect.setEffectID(Effect::EFFECT_CLASS_LEVELUP_SLAYER);
    } else if (pCreature->isVampire()) {
        gcAddEffect.setEffectID(Effect::EFFECT_CLASS_LEVELUP_VAMPIRE);
    } else if (pCreature->isOusters()) {
        gcAddEffect.setEffectID(Effect::EFFECT_CLASS_LEVELUP_OUSTERS);
    }

    pCreature->getZone()->broadcastPacket(pCreature->getX(), pCreature->getY(), &gcAddEffect);

    // cout << "send LEVEL UP : " << gcAddEffect.toString().c_str() << endl;

    __END_CATCH
}

void sendSystemMessage(GamePlayer* pGamePlayer, const string& msg)

{
    __BEGIN_TRY

    Assert(pGamePlayer != NULL);

    // If the player is in a zone, send it right away.
    if (pGamePlayer->getPlayerStatus() == GPS_NORMAL) {
        GCSystemMessage gcSystemMessage;

        gcSystemMessage.setMessage(msg);

        pGamePlayer->sendPacket(&gcSystemMessage);
    }
    // If not in a zone, store it on the GamePlayer and send it later.
    else {
        Event* pEvent = pGamePlayer->getEvent(Event::EVENT_CLASS_SYSTEM_MESSAGE);
        EventSystemMessage* pEventSystemMessage = NULL;

        if (pEvent == NULL) {
            pEvent = pEventSystemMessage = new EventSystemMessage(pGamePlayer);
            // Processed as soon as the player enters a zone.
            pEvent->setDeadline(0);
            pGamePlayer->addEvent(pEvent);
        } else {
            pEventSystemMessage = dynamic_cast<EventSystemMessage*>(pEvent);
        }

        Assert(pEventSystemMessage != NULL);
        pEventSystemMessage->addMessage(msg);

        // cout << "NOT GPS_NORMAL: EventSystemMessage" << endl;
    }

    __END_CATCH
}

bool makeGCWarScheduleList(GCWarScheduleList* pGCWarScheduleList, ZoneID_t zoneID)

{
    __BEGIN_TRY

    Zone* pZone = getZoneByZoneID(zoneID);
    Assert(pZone != NULL);
    Assert(pZone->isCastle());

    WarScheduler* pWarScheduler = pZone->getWarScheduler();
    Assert(pWarScheduler != NULL);

    pWarScheduler->makeGCWarScheduleList(pGCWarScheduleList);

    __END_CATCH

    return true;
}

void sendGCMiniGameScores(PlayerCreature* pPC, BYTE gameType, BYTE Level) {
    GCMiniGameScores gcMGS;
    gcMGS.setGameType((GameType)gameType);
    gcMGS.setLevel(Level);

    string name;
    int score = 0;
    if (defaultPlayRecordRepository().loadMiniGameScore(gameType, Level, name, score)) {
        gcMGS.addScore(name, score);
    }

    pPC->getPlayer()->sendPacket(&gcMGS);
}

void makeGCPetStashList(GCPetStashList* pPacket, PlayerCreature* pPC) {
    for (int i = 0; i < MAX_PET_STASH; ++i) {
        PetItem* pPetItem = dynamic_cast<PetItem*>(pPC->getPetStashItem(i));

        if (pPetItem != NULL) {
            PetStashItemInfo* pInfo = new PetStashItemInfo;
            pInfo->pPetInfo = pPetItem->getPetInfo();
            pInfo->KeepDays = 0;

            pPacket->getPetStashItemInfos()[i] = pInfo;
        }
    }

    // cout << pPacket->toString() << endl;
}
