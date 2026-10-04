//////////////////////////////////////////////////////////////////////////////
// Filename    : CGConnectHandler.cpp
// Written By  : Reiot
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "CGConnect.h"

#ifdef __GAME_SERVER__
#include <stdio.h>

#include <fstream>
#include <list>

#include "CastleInfoManager.h"
#include "CreatureUtil.h"
#include "DiagnosticTrace.h"
#include "EffectGhost.h"
#include "Encrypter.h"
#include "EventHeadCount.h"
#include "EventRegeneration.h"
#include "EventSave.h"
#include "EventSystemMessage.h"
#include "EventZoneInfo.h"
#include "GCDisconnect.h"
#include "GCSystemMessage.h"
#include "GCUpdateInfo.h"
#include "GSGuildMemberLogOn.h"
#include "GameContext.h"
#include "GamePlayer.h"
#include "Guild.h"
#include "GuildManager.h"
#include "IncomingPlayerManager.h"
#include "LogDef.h"
#include "NPCInfo.h"
#include "NicknameBook.h"
#include "Ousters.h"
#include "PCFinder.h"
#include "PCSlayerInfo2.h"
#include "PCVampireInfo2.h"
#include "PKZoneInfoManager.h"
#include "PacketUtil.h"
#include "PlayerCreature.h"
#include "Properties.h"
#include "ResurrectLocationManager.h"
#include "SharedServerManager.h"
#include "Slayer.h"
#include "TelephoneCenter.h"
#include "Thread.h"
#include "Vampire.h"
#include "WeatherManager.h"
#include "Zone.h"
#include "ZoneGroup.h"
#include "ZoneGroupManager.h"
#include "ZoneInfoManager.h"
#include "ZonePlayerManager.h"
#include "ZoneUtil.h"
#include "mission/QuestManager.h"
#include "repository/CharacterRepository.h"
#include "repository/SessionRepository.h"
#include "skill/EffectGnomesWhisper.h"
// #include "GCLoadInventory.h"
#include "DynamicZoneManager.h"
#include "GDRLairManager.h"
#include "KernelContext.h"
#include "SystemAvailabilitiesManager.h"
#include "types/ServerType.h"

#endif

bool isAdultByBirthdayDate(const string& birthday);


//////////////////////////////////////////////////////////////////////////////
// CGConnectHandler::execute()
//
// This is the first packet the client sends to the server, either when the login
// server first connects it to a game server or when it moves to another game server.
// At that point the player object has just been created, and it is
// managed by the IPM.
//
// So another packet arriving first can safely be taken for a hacking attempt,
// and this packet has to be checked for being the first. The player object
// stores the previous packet, so it is enough to see whether that value is NULL.
//
// A wrong packet is registered in the ban list and the connection is closed.
//////////////////////////////////////////////////////////////////////////////
void CGConnectHandler::execute(CGConnect* pPacket, Player* pPlayer)

{
    __BEGIN_TRY __BEGIN_DEBUG_EX __BEGIN_DEBUG

        GuildManager& guilds = de::gameContext().guilds();
    SharedServerManager& sharedServer = de::gameContext().sharedServer();
    Properties& config = de::kernelContext().config();

#ifdef __GAME_SERVER__

    Assert(pPacket != NULL);
    Assert(pPlayer != NULL);

    GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(pPlayer);

    PCFinder& pcFinder = de::gameContext().playerCreatures();

    // set MAC Address
    pGamePlayer->setMacAddress(pPacket->getMacAddress());

    // Get this packet's ConnectionInfo object.
    // A cracker has to match the key value and the character name within a time limit to connect.
    try {
        ConnectionInfo* pConnectionInfo =
            de::gameContext().connectionInfos().getConnectionInfo(pGamePlayer->getSocket()->getHost());

        // Authenticate the key value.
        if (pPacket->getKey() != pConnectionInfo->getKey()) {
            FILELOG_INCOMING_CONNECTION("connectionError.log", "Wrong Key: [%s] %s",
                                        pConnectionInfo->getPCName().c_str(),
                                        pGamePlayer->getSocket()->getHost().c_str());
            throw InvalidProtocolException("invalid key");
        }

        // Authenticate the name.
        if (pPacket->getPCName() != pConnectionInfo->getPCName()) {
            FILELOG_INCOMING_CONNECTION("connectionError.log", "Wrong PCName: [%s] %s",
                                        pConnectionInfo->getPCName().c_str(),
                                        pGamePlayer->getSocket()->getHost().c_str());
            throw InvalidProtocolException("invalid pc name");
        }

        // Store the name for now. A later failure deletes the object anyway, so this is safe.
        pGamePlayer->setID(pConnectionInfo->getPlayerID());

        // A lucky connection can get in before the CIM's heartbeat runs.
        // (With good timing, connecting within twice the heartbeat period is enough.)
        // So compare the current time with the expire time.
        Timeval currentTime;
        getCurrentTime(currentTime);
        if (pConnectionInfo->getExpireTime() < currentTime) {
            FILELOG_INCOMING_CONNECTION("connectionError.log", "Expired: [%s] %s", pConnectionInfo->getPCName().c_str(),
                                        pGamePlayer->getSocket()->getHost().c_str());

            // Delete it first.
            de::gameContext().connectionInfos().deleteConnectionInfo(pConnectionInfo->getClientIP());
            throw InvalidProtocolException("session already expired");
        }

        // by sigi. 2002.12.7
        FILELOG_INCOMING_CONNECTION("connectionInfo.log", "Login [%s:%s] %s (%u)",
                                    pConnectionInfo->getPlayerID().c_str(), pConnectionInfo->getPCName().c_str(),
                                    pConnectionInfo->getClientIP().c_str(), pConnectionInfo->getKey());

        // Authenticated, so delete the ConnectionInfo.
        try {
            de::gameContext().connectionInfos().deleteConnectionInfo(pConnectionInfo->getClientIP());
        } catch (NoSuchElementException& nsee) {
            FILELOG_INCOMING_CONNECTION("connectionInfoDelete.log", "DeleteNoSuch [%s:%s] %s (%u)",
                                        pConnectionInfo->getPlayerID().c_str(), pConnectionInfo->getPCName().c_str(),
                                        pConnectionInfo->getClientIP().c_str(), pConnectionInfo->getKey());
        }
    } catch (NoSuchElementException& nsee) // When no CI with that IP exists
    {
        FILELOG_INCOMING_CONNECTION("connectionError.log", "NoSuchConnectionInfo: %s",
                                    pGamePlayer->getSocket()->getHost().c_str());

        // When the delay between connecting and sending the CGConnect packet is too long
        // the session expires. Cut the connection in that case too!
        // (For example, the first connection succeeds but a debugger is entered next,
        // so by the time the CGConnect packet is sent the session has expired.)
        GCDisconnect gcDisconnect;
        gcDisconnect.setMessage(nsee.toString());

        pGamePlayer->sendPacket(&gcDisconnect);

        // Throwing this way makes the enclosing IPM::processCommands() do the disconnect.
        throw InvalidProtocolException(nsee.toString().c_str());
    } catch (InvalidProtocolException& ipe) {
        FILELOG_INCOMING_CONNECTION("connectionError.log", "%s: %s", ipe.toString().c_str(),
                                    pGamePlayer->getSocket()->getHost().c_str());
        cout << endl
             << "+-----------------------+" << endl
             << "| Level 2 Access Denied |" << endl
             << "+-----------------------+" << endl
             << endl;

        GCDisconnect gcDisconnect;
        gcDisconnect.setMessage(ipe.toString());

        pGamePlayer->sendPacket(&gcDisconnect);

        // Throwing this way makes the enclosing IPM::processCommands() do the disconnect.
        throw;
    }

    //----------------------------------------------------------------------
    // Login check
    //----------------------------------------------------------------------
    // Billing
    PayType payType;
    string payPlayDate;
    uint payPlayHours;
    uint payPlayFlag;
    string familyPayPlayDate;


    try {
        // Take the character type from here rather than the one the client sent, to avoid disconnects
        string spID;
        string spRace;

        if (!defaultCharacterRepository().loadSlayerAccount(pPacket->getPCName(), spID, spRace)) {
            StringStream msg;
            msg << "Failed to load PlayerCreature data from DB. Not 1 PlayerID (" << pPacket->getPCName().c_str()
                << ")";

            filelog("connectDB_BUG.txt", "%s", msg.toString().c_str());

            throw ProtocolException(msg.toString().c_str());
        }

        {
            if (spRace == "SLAYER") {
                pPacket->setPCType(PC_SLAYER);
            } else if (spRace == "VAMPIRE") {
                pPacket->setPCType(PC_VAMPIRE);
            } else if (spRace == "OUSTERS") {
                pPacket->setPCType(PC_OUSTERS);
            } else {
                StringStream msg;
                msg << "Failed to load PlayerCreature data from DB. No Character(" << spID.c_str()
                    << "!=" << pGamePlayer->getID().c_str() << ")";

                filelog("connectDB_BUG.txt", "%s", msg.toString().c_str());

                throw ProtocolException(msg.toString().c_str());
            }


            if (strcasecmp(spID.c_str(), pGamePlayer->getID().c_str()) != 0) {
                StringStream msg;
                msg << "Failed to load PlayerCreature data from DB. No Character(" << spID.c_str()
                    << "!=" << pGamePlayer->getID().c_str() << ")";

                filelog("connectDB_BUG.txt", "%s", msg.toString().c_str());

                throw ProtocolException(msg.toString().c_str());
            }
        }

        PlayerSessionRow session;

        if (!defaultSessionRepository().loadPlayerSession(pGamePlayer->getID(), session)) {
            StringStream msg;
            msg << "Failed to load PlayerCreature data from DB. No Player(" << pPacket->getPCName().c_str() << ")";

            filelog("connectDB_BUG.txt", "%s", msg.toString().c_str());

            throw ProtocolException(msg.toString().c_str());
        }

        string playerID = session.playerID;
        ServerGroupID_t GID = session.serverGroupID;
        string logon = session.logOn;
        uint scount = session.specialEventCount;

        payType = (PayType)session.payType;
        payPlayDate = session.payPlayDate;
        payPlayHours = session.payPlayHours;
        payPlayFlag = session.payPlayFlag;
        familyPayPlayDate = session.familyPayPlayDate;

        pGamePlayer->setServerGroupID(GID);
        pGamePlayer->setSpecialEventCount(scount);

        if (logon != "LOGOFF") {
            char str[80];
            snprintf(str, sizeof(str), "Already connected player ID: %s, %s", playerID.c_str(), logon.c_str());
            throw ProtocolException(str);
        }

        bool tookTheSession = defaultSessionRepository().markPlayerLoggedOn(playerID);

        // LogOn is not LOGOFF, and so on..
        if (!tookTheSession) {
            char str[80];
            snprintf(str, sizeof(str), "Already connected player ID2: %s, %s", playerID.c_str(), logon.c_str());
            throw ProtocolException(str);
        }

        string connectIP = pGamePlayer->getSocket()->getHost();

        // Billing
        pGamePlayer->setPayPlayValue(payType, payPlayDate, payPlayHours, payPlayFlag, familyPayPlayDate);

        // NOTE: nothing in this try can raise a SQLQueryException. Each
        // repository call converts its own inside END_DB and rethrows a
        // const char*, which this catch does not match, and everything
        // else in the block is in-memory work (PaySystem::loginPayPlay
        // sits under pay-system macros that PaySystem.h comments out).
        // The catch is kept because it would matter again if a DB call
        // were added here.
        //
        // The const char* matches no handler between here and
        // GamePlayer::processCommand's catch (...), which turns it into a
        // DisconnectException; the SQL failure itself is in DBError.log.
    } catch (SQLQueryException& sqe) {
        throw Error(sqe.toString());
    }

    //----------------------------------------------------------------------
    // Load the Slayer or the Vampire character.
    //----------------------------------------------------------------------
    Slayer* pSlayer = NULL;
    Vampire* pVampire = NULL;
    Ousters* pOusters = NULL;

    bool bAlreadyConnected = false;

    if (pPacket->getPCType() == PC_SLAYER) {
        pSlayer = new Slayer();
        pSlayer->setName(pPacket->getPCName());
        pSlayer->setPlayer(pGamePlayer);

        if (!pSlayer->load()) {
            filelog("connectDB_BUG.txt", "Failed to load SLAYER(%s) data from DB", pPacket->getPCName().c_str());
            cout << " reached here" << endl;
            throw ProtocolException("Failed to load SLAYER data from DB");
        }

        if (pSlayer->getName() != pPacket->getPCName()) {
            cout << "Different Name : " << pSlayer->getName().c_str() << ", " << pPacket->getPCName().c_str() << endl;

            Assert(pSlayer->getName() == pPacket->getPCName());
        }

        pGamePlayer->setCreature(pSlayer);


        // Add the periodic recovery event to the player object.
        // Ten-second recovery is the rule here.
        // (note that setDeadline's parameter is in 0.1 seconds)
        EventRegeneration* pEventRegeneration = new EventRegeneration(pGamePlayer);
        pEventRegeneration->setDeadline(10 * 10);
        pGamePlayer->addEvent(pEventRegeneration);

        // Add to the PCFinder.
        // Removal from the PCFinder happens only in ~GamePlayer().
        try {
            pcFinder.addCreature(pSlayer);
        } catch (DuplicatedException& dupe) {
            bAlreadyConnected = true;
        }

        // Unless already connected..
        if (!bAlreadyConnected) {
            // Add to the guild's current member list.
            if (pSlayer->getGuildID() != 99) {
                Guild* pGuild = guilds.getGuild(pSlayer->getGuildID());
                if (pGuild != NULL) {
                    // Tell the sharedserver about the connection and update the DB too.
                    try {
                        pGuild->addCurrentMember(pSlayer->getName());

                        GSGuildMemberLogOn gsGuildMemberLogOn;
                        gsGuildMemberLogOn.setGuildID(pGuild->getID());
                        gsGuildMemberLogOn.setName(pSlayer->getName());
                        gsGuildMemberLogOn.setLogOn(true);
                        gsGuildMemberLogOn.setServerID(config.getPropertyInt("ServerID"));

                        sharedServer.sendPacket(&gsGuildMemberLogOn);

                        // DB update
                        { defaultSessionRepository().markGuildMemberLoggedOn(pSlayer->getName()); }
                    } catch (DuplicatedException& t) {
                        // Ignore it for now.
                        filelog("guildBug.log", "%s", t.toString().c_str());
                    }
                } else
                    filelog("GuildMissing.log", "[NoSuchGuild] GuildID : %d, Name : %s\n", (int)pSlayer->getGuildID(),
                            pSlayer->getName().c_str());
            }
        }
    } else if (pPacket->getPCType() == PC_VAMPIRE) {
        pVampire = new Vampire();
        pVampire->setName(pPacket->getPCName());
        pVampire->setPlayer(pGamePlayer);

        if (!pVampire->load()) {
            filelog("connectDB_BUG.txt", "Failed to load VAMPIRE(%s) data from DB", pPacket->getPCName().c_str());
            throw ProtocolException("Failed to VAMPIRE data from DB");
        }

        Assert(pVampire->getName() == pPacket->getPCName());

        pGamePlayer->setCreature(pVampire);

        // Add the periodic recovery event to the player object.
        // Ten-second recovery is the rule here.
        // (note that setDeadline's parameter is in 0.1 seconds)
        EventRegeneration* pEventRegeneration = new EventRegeneration(pGamePlayer);
        pEventRegeneration->setDeadline(10 * 10);
        pGamePlayer->addEvent(pEventRegeneration);

        // Add to the PCFinder.
        // Removal from the PCFinder happens only in ~GamePlayer().
        try {
            pcFinder.addCreature(pVampire);
        } catch (DuplicatedException& dupe) {
            bAlreadyConnected = true;
        }

        // Unless already connected..
        if (!bAlreadyConnected) {
            // Add to the guild's current member list.
            if (pVampire->getGuildID() != 0) {
                Guild* pGuild = guilds.getGuild(pVampire->getGuildID());
                if (pGuild != NULL) {
                    // Tell the sharedserver about the connection and update the DB too.
                    try {
                        pGuild->addCurrentMember(pVampire->getName());

                        GSGuildMemberLogOn gsGuildMemberLogOn;
                        gsGuildMemberLogOn.setGuildID(pGuild->getID());
                        gsGuildMemberLogOn.setName(pVampire->getName());
                        gsGuildMemberLogOn.setLogOn(true);
                        gsGuildMemberLogOn.setServerID(config.getPropertyInt("ServerID"));

                        sharedServer.sendPacket(&gsGuildMemberLogOn);

                        // DB update
                        { defaultSessionRepository().markGuildMemberLoggedOn(pVampire->getName()); }
                    } catch (DuplicatedException& t) {
                        // Ignore it for now.
                        filelog("guildBug.log", "%s", t.toString().c_str());
                    }
                } else
                    filelog("GuildMissing.log", "[NoSuchGuild] GuildID : %d, Name : %s\n", (int)pVampire->getGuildID(),
                            pVampire->getName().c_str());
            }
        }
    } else if (pPacket->getPCType() == PC_OUSTERS) {
        pOusters = new Ousters();
        pOusters->setName(pPacket->getPCName());
        pOusters->setPlayer(pGamePlayer);

        if (!pOusters->load()) {
            filelog("connectDB_BUG.txt", "Failed to load VAMPIRE(%s) data from DB", pPacket->getPCName().c_str());
            throw ProtocolException("Failed to VAMPIRE data from DB");
        }

        Assert(pOusters->getName() == pPacket->getPCName());
        // filelog("Ousters.txt","CGConectHandler.cpp 0,HP:%d,MAXHP:%d,MP:%d,MAXMP:%d",  (int)pOusters->getHP(
        // ATTR_CURRENT), (int)pOusters->getHP(ATTR_MAX),(int)pOusters->getMP(ATTR_CURRENT),
        // (int)pOusters->getMP(ATTR_MAX));

        pGamePlayer->setCreature(pOusters);

        // Add the periodic recovery event to the player object.
        // Ten-second recovery is the rule here.
        // (note that setDeadline's parameter is in 0.1 seconds)
        EventRegeneration* pEventRegeneration = new EventRegeneration(pGamePlayer);
        pEventRegeneration->setDeadline(10 * 10);
        pGamePlayer->addEvent(pEventRegeneration);

        // Add to the PCFinder.
        // Removal from the PCFinder happens only in ~GamePlayer().
        try {
            pcFinder.addCreature(pOusters);
        } catch (DuplicatedException& dupe) {
            bAlreadyConnected = true;
        }

        // Unless already connected..
        if (!bAlreadyConnected) {
            // Add to the guild's current member list.
            if (pOusters->getGuildID() != 66) {
                Guild* pGuild = guilds.getGuild(pOusters->getGuildID());
                if (pGuild != NULL) {
                    // Tell the sharedserver about the connection and update the DB too.
                    try {
                        pGuild->addCurrentMember(pOusters->getName());

                        GSGuildMemberLogOn gsGuildMemberLogOn;
                        gsGuildMemberLogOn.setGuildID(pGuild->getID());
                        gsGuildMemberLogOn.setName(pOusters->getName());
                        gsGuildMemberLogOn.setLogOn(true);
                        gsGuildMemberLogOn.setServerID(config.getPropertyInt("ServerID"));

                        sharedServer.sendPacket(&gsGuildMemberLogOn);

                        // DB update
                        { defaultSessionRepository().markGuildMemberLoggedOn(pOusters->getName()); }
                    } catch (DuplicatedException& t) {
                        // Ignore it for now.
                        filelog("guildBug.log", "%s", t.toString().c_str());
                    }
                } else
                    filelog("GuildMissing.log", "[NoSuchGuild] GuildID : %d, Name : %s\n", (int)pOusters->getGuildID(),
                            pOusters->getName().c_str());
            }
        }
    }

    de::diagnosticTrace([&](std::ostream& output) { output << " reached here2"; });

    // Handling when already connected.
    // In order to handle the PCFinder's DuplicatedException unambiguously,
    // a variable is used for the check.
    // The problem in the PCFinder seems to come
    // from the guild's DuplicatedException.
    // by sigi. 2002.8.29
    if (bAlreadyConnected) {
        SAFE_DELETE(pSlayer);
        SAFE_DELETE(pVampire);
        SAFE_DELETE(pOusters);

        pGamePlayer->setID(string("")); // keeps LogOn from becoming 'LOGOFF'.
        pGamePlayer->setCreature(NULL); // keeps the PCFinder from deleting it.

        char str[80];
        snprintf(str, sizeof(str), "Already connected player ID3(Dup): %s", pPacket->getPCName().c_str());
        throw ProtocolException(str);
    }

    //----------------------------------------------------------------------
    // Register the PC with the PCFinder and the zone.
    //----------------------------------------------------------------------
    Creature* pCreature = pGamePlayer->getCreature();
    Assert(pCreature != NULL);

    EventHeadCount* pEventHeadCount = new EventHeadCount(pGamePlayer);
    pEventHeadCount->setDeadline(18000);
    pGamePlayer->addEvent(pEventHeadCount);
    de::diagnosticTrace([&](std::ostream& output) { output << " reached here3"; });
    if (pCreature->isGOD()) {
        EffectGhost* pEffect = new EffectGhost(pCreature);
        pCreature->getEffectManager()->addEffect(pEffect);
        pCreature->setFlag(Effect::EFFECT_CLASS_GHOST);
        pCreature->setMoveMode(Creature::MOVE_MODE_FLYING);
    }

    if (pCreature->getCompetenceShape() == 0) {
        EffectGnomesWhisper* pEffect = new EffectGnomesWhisper(pCreature);
        pEffect->setLevel(30);

        pCreature->addEffect(pEffect);
        pCreature->setFlag(pEffect->getEffectClass());
    }
    de::diagnosticTrace([&](std::ostream& output) { output << " reached here4"; });
    // Logging in inside a castle or a castle dungeon is not allowed.
    ZoneID_t castleZoneID;
    ZoneInfo* pZoneInfo = de::gameContext().zoneInfos().getZoneInfo(pCreature->getZoneID());

    bool isCastleZone = de::gameContext().castleInfos().getCastleZoneID(pCreature->getZoneID(), castleZoneID);
    bool isMasterLair = pZoneInfo->isMasterLair() || GDRLairManager::Instance().isGDRLairZone(pCreature->getZoneID());
    bool isPKZone = de::gameContext().pkZoneInfos().isPKZone(pCreature->getZoneID());
    bool isMaze = (pCreature->getZoneID() == 3001) || (pCreature->getZoneID() == 3002) ||
                  (pCreature->getZoneID() == 3003) || (pCreature->getZoneID() == 1013); // add by sonic 2006.10.30
    bool isEventZone = EventZoneInfoManager::Instance().getEventZoneInfo(pCreature->getZoneID()) != NULL;
    bool isBeginnerZone = pCreature->getZoneID() == 1122 && !canEnterBeginnerZone(pCreature);
    bool isDynamicZone = de::gameContext().dynamicZones().isDynamicZone(pCreature->getZoneID());

    if (pCreature->isPC()) //&& pCreature->isPLAYER() )
    {
        ZONE_COORD zoneCoord;
        PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pCreature);
        Assert(pPC != NULL);

        if (isCastleZone || isMasterLair || isPKZone || isMaze || isEventZone || isBeginnerZone || isDynamicZone) {
            if (de::gameContext().resurrectLocations().getPosition(pPC, zoneCoord)) {
                pCreature->setZoneID(zoneCoord.id);
                pCreature->setXY(zoneCoord.x, zoneCoord.y);
            }
        }
    }
    de::diagnosticTrace([&](std::ostream& output) { output << " reached here5"; });
    Zone* pZone = pCreature->getZone();
    Assert(pZone != NULL);

    // If nothing at all is set,
    // just call it a paying user
    pGamePlayer->setPremiumPlay();

    // test code

    // Checked here because of items that apply only in a pay zone
    // 2002.8.26. by sigi
    de::diagnosticTrace(
        [&](std::ostream& output) { output << " Loading items: if this part fails, the error is here."; });
    if (pPacket->getPCType() == PC_SLAYER) {
        Assert(pSlayer != NULL);
        pSlayer->loadItem(true);
    } else if (pPacket->getPCType() == PC_VAMPIRE) {
        Assert(pVampire != NULL);
        pVampire->loadItem(true);
    } else if (pPacket->getPCType() == PC_OUSTERS) {
        Assert(pOusters != NULL);
        pOusters->loadItem(true);
    }


    // A Slayer starting inside a guild, or a Vampire inside a lair,
    // has its HP filled to the maximum.
    if (pCreature->isSlayer()) {
        Slayer* pSlayer = dynamic_cast<Slayer*>(pCreature);

        switch (pZone->getZoneID()) {
        case 2000:
        case 2001:
        case 2002:
        case 2003:
        case 2004:
        case 2010:
        case 2011:
        case 2012:
        case 2013:
        case 2014:
        case 2020:
        case 2021:
        case 2022:
        case 2023:
        case 2024:
            pSlayer->setHP(pSlayer->getHP(ATTR_MAX), ATTR_CURRENT);
            break;
        default:
            break;
        }
    } else if (pCreature->isVampire()) {
        Vampire* pVampire = dynamic_cast<Vampire*>(pCreature);

        switch (pZone->getZoneID()) {
        case 1003:
        case 1007:
            pVampire->setHP(pVampire->getHP(ATTR_MAX), ATTR_CURRENT);
            break;
        default:
            break;
        }
    } else if (pCreature->isOusters()) {
        Ousters* pOusters = dynamic_cast<Ousters*>(pCreature);

        switch (pZone->getZoneID()) {
        case 1311:
            // filelog("Ousters.txt","CGConectHandler.cpp 1,HP:%d,MAXHP:%d,MP:%d,MAXMP:%d",  (int)pOusters->getHP(
            // ATTR_CURRENT), (int)pOusters->getHP(ATTR_MAX),(int)pOusters->getMP(ATTR_CURRENT),
            // (int)pOusters->getMP(ATTR_MAX));
            pOusters->setHP(pOusters->getHP(ATTR_MAX), ATTR_CURRENT);
            // chengh add 2005 10 02
            pOusters->setMP(pOusters->getMP(ATTR_MAX), ATTR_CURRENT);
            // filelog("Ousters.txt","CGConectHandler.cpp 2,HP:%d,MAXHP:%d,MP:%d,MAXMP:%d",  (int)pOusters->getHP(
            // ATTR_CURRENT), (int)pOusters->getHP(ATTR_MAX),(int)pOusters->getMP(ATTR_CURRENT),
            // (int)pOusters->getMP(ATTR_MAX));

        default:
            break;
        }
    }

    // Load the quest manager.
    PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pCreature);
    pPC->getQuestManager()->load();

    // Get OIDs for the creature itself and for the items it owns.
    // The Zone must already be set by Creature::load().
    pCreature->registerObject();

#ifdef __USE_ENCRYPTER__
    // Register the encryption code. It uses the objectID for now.
    pGamePlayer->setEncryptCode();
#endif

    SEND_SYSTEM_AVAILABILITIES(pGamePlayer);

    //----------------------------------------------------------------------
    // Build the PC info and the SkillInfo for GCUpdateInfo.
    //----------------------------------------------------------------------

    de::diagnosticTrace([&](std::ostream& output) { output << " reached here7"; });

    GCUpdateInfo gcUpdateInfo;

    makeGCUpdateInfo(&gcUpdateInfo, pCreature);

    pGamePlayer->sendPacket(&gcUpdateInfo);

    sendPetInfo(pGamePlayer);

    string host = pGamePlayer->getSocket()->getHost();

    // Finally INSERT the IP information into the DB.

    //--------------------------------------------------
    // change player status
    //--------------------------------------------------
    pGamePlayer->setPlayerStatus(GPS_WAITING_FOR_CG_READY);

#endif

    __END_DEBUG
    __END_DEBUG_EX __END_CATCH
}

#ifdef __GAME_SERVER__

//////////////////////////////////////////////////////////////////////////////
//
// Adulthood decided from YYYY-MM-DD
//
//////////////////////////////////////////////////////////////////////////////
bool isAdultByBirthdayDate(const string& birthday) {
    StringStream AdultSSN;

    time_t daytime = time(0);
    tm Timec;
    localtime_r(&daytime, &Timec);

    AdultSSN << Timec.tm_year - 18;
    // tm_mon - range 0 to 11
    if ((Timec.tm_mon + 1) < 10)
        AdultSSN << "0";
    AdultSSN << (Timec.tm_mon + 1);
    if (Timec.tm_mday < 10)
        AdultSSN << "0";
    AdultSSN << Timec.tm_mday;

    cout << "Birthday = " << birthday.c_str() << " ADULTSSN = " << AdultSSN.toString().c_str() << endl;

    int year = atoi(birthday.substr(0, 4).c_str());
    int month = atoi(birthday.substr(5, 2).c_str());
    int day = atoi(birthday.substr(8, 2).c_str());

    if (Timec.tm_year - 18 + 1900 > year) {
        cout << "Adult" << endl;
        return true;
    } else if (Timec.tm_year - 18 + 1900 == year) {
        if (Timec.tm_mon + 1 > month) {
            cout << "Adult" << endl;
            return true;
        } else if (Timec.tm_mon + 1 == month) {
            if (Timec.tm_mday >= day) {
                cout << "Adult" << endl;
                return true;
            }
        }
    }

    cout << "Minor" << endl;
    return false;
}

#endif
