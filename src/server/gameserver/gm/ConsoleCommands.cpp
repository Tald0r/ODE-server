//////////////////////////////////////////////////////////////////////////////
// Filename    : ConsoleCommands.cpp
// Description : The *command console: opcommand() reads the sub-command the
//               line names, runs it off the table in
//               ConsoleCommandRegistration.cpp and sends the reply it left.
//               Every sub-command body lives here, one function per name.
//////////////////////////////////////////////////////////////////////////////

#include <stdio.h>

#include <list>

#include "CastleInfoManager.h"
#include "ClientManager.h"
#include "CreatureUtil.h"
#include "DatabaseError.h"
#include "DynamicZone.h"
#include "DynamicZoneGroup.h"
#include "DynamicZoneManager.h"
#include "EffectCastingTrap.h"
#include "EffectGhost.h"
#include "EffectLoud.h"
#include "EventZoneInfo.h"
#include "GCAddEffect.h"
#include "GCAddEffectToTile.h"
#include "GCAddOusters.h"
#include "GCAddSlayer.h"
#include "GCAddVampire.h"
#include "GCDeleteObject.h"
#include "GCModifyNickname.h"
#include "GCNoticeEvent.h"
#include "GCNotifyWin.h"
#include "GCPetStashList.h"
#include "GCRemoveEffect.h"
#include "GCStatusCurrentHP.h"
#include "GCSystemMessage.h"
#include "GDRLairManager.h"
#include "GQuestManager.h"
#include "GameContext.h"
#include "GamePlayer.h"
#include "GoodsInventory.h"
#include "Guild.h"
#include "GuildManager.h"
#include "ItemUtil.h"
#include "LevelWarManager.h"
#include "MasterLairManager.h"
#include "Mine.h"
#include "Monster.h"
#include "MonsterManager.h"
#include "NicknameBook.h"
#include "OptionInfo.h"
#include "Ousters.h"
#include "PCFinder.h"
#include "PCManager.h"
#include "PKZoneInfoManager.h"
#include "PacketUtil.h"
#include "PlayerCreature.h"
#include "PlayerMailbox.h"
#include "Properties.h"
#include "Relic.h"
#include "RelicUtil.h"
#include "ShrineInfoManager.h"
#include "SiegeManager.h"
#include "Slayer.h"
#include "StringPool.h"
#include "UniqueItemManager.h"
#include "VSDateTime.h"
#include "Vampire.h"
#include "VariableManager.h"
#include "Zone.h"
#include "ZoneGroupManager.h"
#include "ZoneInfoManager.h"
#include "ZoneUtil.h"
#include "ctf/FlagManager.h"
#include "gm/ConsoleSubcommands.h"
#include "gm/GMCommands.h"
#include "mission/MonsterKillQuestStatus.h"
#include "mission/QuestManager.h"
#include "repository/NicknameRepository.h"
#include "war/WarScheduler.h"
#include "war/WarSystem.h"

namespace de::gm {
static const Effect::EffectClass FirecrackerEffects[] = {
    Effect::EFFECT_CLASS_FIRE_CRACKER_1,             // 0
    Effect::EFFECT_CLASS_FIRE_CRACKER_2,             // 1
    Effect::EFFECT_CLASS_FIRE_CRACKER_3,             // 2
    Effect::EFFECT_CLASS_DRAGON_FIRE_CRACKER,        // 3
    Effect::EFFECT_CLASS_FIRE_CRACKER_4,             // 4
    Effect::EFFECT_CLASS_FIRE_CRACKER_VOLLEY_1,      // 5
    Effect::EFFECT_CLASS_FIRE_CRACKER_VOLLEY_2,      // 6
    Effect::EFFECT_CLASS_FIRE_CRACKER_VOLLEY_3,      // 7
    Effect::EFFECT_CLASS_FIRE_CRACKER_VOLLEY_4,      // 8
    Effect::EFFECT_CLASS_FIRE_CRACKER_WIDE_VOLLEY_1, // 9
    Effect::EFFECT_CLASS_FIRE_CRACKER_WIDE_VOLLEY_2, // 10
    Effect::EFFECT_CLASS_FIRE_CRACKER_WIDE_VOLLEY_3, // 11
    Effect::EFFECT_CLASS_FIRE_CRACKER_WIDE_VOLLEY_4, // 12
    Effect::EFFECT_CLASS_FIRE_CRACKER_STORM          // 13
};

namespace {
// A body that ends here answers nothing at all: the console sends its reply
// only while bSendPacket stands.
void answerNothing(bool& bSendPacket) {
    bSendPacket = false;
}
} // namespace

//////////////////////////////////////////////////////////////////////////////
// The console itself: the line is "*command <name> <value1>", the reply is
// "nothing" until a body says otherwise, and a name no row answers sends no
// reply at all.
//////////////////////////////////////////////////////////////////////////////
void opcommand(GamePlayer* pGamePlayer, string msg, int i) {
    __BEGIN_TRY __BEGIN_DEBUG_EX

        size_t j = msg.find_first_of(' ', i + 1);
    size_t k = msg.find_first_of(' ', j + 1);
    string command = trim(msg.substr(j + 1, k - j - 1));
    string value1 = msg.substr(k + 1, msg.size() - k - 1).c_str();

    if (pGamePlayer != NULL) {
        Creature* pCreature = pGamePlayer->getCreature();
        filelog("change.txt", "%s , %s , %s", pCreature->getName().c_str(), command.c_str(), value1.c_str());
    }

    GCSystemMessage gcSystemMessage;
    gcSystemMessage.setMessage("nothing");
    bool bSendPacket = true;

    // The console is reached from a GOD-gated chat command and from the relay
    // another game server sends, which carries no creature to ask a level of.
    if (!consoleSubcommands().dispatch(command, Permission::God, pGamePlayer, value1, gcSystemMessage, bSendPacket))
        bSendPacket = false;

    if (pGamePlayer != NULL && bSendPacket) {
        pGamePlayer->sendPacket(&gcSystemMessage);
        filelog("change.txt", "[%s] %s", pGamePlayer->getCreature()->getName().c_str(),
                gcSystemMessage.getMessage().c_str());
    }

    __END_DEBUG_EX __END_CATCH
}

// *command balanceZoneGroup
void opBalanceZoneGroup(GamePlayer* pGamePlayer, const string& value1, GCSystemMessage& gcSystemMessage,
                        bool& bSendPacket) {
    bool defaultZoneGroup = false;

    if (value1 == "default")
        defaultZoneGroup = true;

    de::gameContext().clients().setBalanceZoneGroup(0, true, defaultZoneGroup);

    gcSystemMessage.setMessage(de::gameContext().strings().getString(STRID_ZONE_GROUP_BALANCING));
}

// *command regenMasterLair
void opRegenMasterLair(GamePlayer* pGamePlayer, const string& value1, GCSystemMessage& gcSystemMessage,
                       bool& bSendPacket) {
    if (pGamePlayer == NULL)
        return answerNothing(bSendPacket);
    Creature* pCreature = pGamePlayer->getCreature();
    if (pCreature == NULL)
        return answerNothing(bSendPacket);
    Zone* pZone = pCreature->getZone();
    Assert(pZone != NULL);

    if (pZone->isMasterLair()) {
        // balancing
        MasterLairManager* pMasterLairManager = pZone->getMasterLairManager();
        Assert(pMasterLairManager != NULL);

        pMasterLairManager->startEvent();

        gcSystemMessage.setMessage(de::gameContext().strings().getString(STRID_MASTER_LAIR_REGEN));
    } else {
        gcSystemMessage.setMessage(de::gameContext().strings().getString(STRID_NOT_IN_MASTER_LAIR));
    }
}

// *command showMasterLairStatus
void opShowMasterLairStatus(GamePlayer* pGamePlayer, const string& value1, GCSystemMessage& gcSystemMessage,
                            bool& bSendPacket) {
    if (pGamePlayer == NULL)
        return answerNothing(bSendPacket);
    Creature* pCreature = pGamePlayer->getCreature();
    if (pCreature == NULL)
        return answerNothing(bSendPacket);
    Zone* pZone = pCreature->getZone();
    Assert(pZone != NULL);

    if (pZone->isMasterLair()) {
        MasterLairManager* pMasterLairManager = pZone->getMasterLairManager();
        Assert(pMasterLairManager != NULL);

        gcSystemMessage.setMessage(pMasterLairManager->toString());
    } else {
        gcSystemMessage.setMessage(de::gameContext().strings().getString(STRID_NOT_IN_MASTER_LAIR));
    }
}

// *command invincible
void opInvincible(GamePlayer* pGamePlayer, const string& value1, GCSystemMessage& gcSystemMessage, bool& bSendPacket) {
    Creature* pCreature = pGamePlayer->getCreature();
    Assert(pCreature != NULL);

    bool bInvincible = pCreature->isFlag(Effect::EFFECT_CLASS_NO_DAMAGE);

    if (value1 == "on") {
        char msg[50];
        snprintf(msg, sizeof(msg), de::gameContext().strings().c_str(STRID_INVINCIBLE), "ON");

        if (bInvincible)
            gcSystemMessage.setMessage(msg);
        else {
            pCreature->setFlag(Effect::EFFECT_CLASS_NO_DAMAGE);
            gcSystemMessage.setMessage(msg);
        }
        filelog("change.txt", "[%s]%s", pCreature->getName().c_str(), gcSystemMessage.toString().c_str());
    } else if (value1 == "off") {
        char msg[50];
        snprintf(msg, sizeof(msg), de::gameContext().strings().c_str(STRID_INVINCIBLE), "OFF");

        if (!bInvincible)
            gcSystemMessage.setMessage(msg);
        else {
            pCreature->removeFlag(Effect::EFFECT_CLASS_NO_DAMAGE);
            gcSystemMessage.setMessage(msg);
        }

        filelog("change.txt", "[%s]%s", pCreature->getName().c_str(), gcSystemMessage.toString().c_str());
    } else {
        bSendPacket = false;
    }
}

// *command ghost
void opGhost(GamePlayer* pGamePlayer, const string& value1, GCSystemMessage& gcSystemMessage, bool& bSendPacket) {
    Creature* pCreature = pGamePlayer->getCreature();
    Assert(pCreature != NULL);

    bool bGhost = pCreature->isFlag(Effect::EFFECT_CLASS_GHOST);

    if (value1 == "on") {
        char msg[50];
        snprintf(msg, sizeof(msg), de::gameContext().strings().c_str(STRID_GHOST), "ON");

        if (bGhost)
            gcSystemMessage.setMessage(msg);
        else {
            Zone* pZone = pCreature->getZone();
            Assert(pZone != NULL);

            Tile& rTile = pZone->getTile(pCreature->getX(), pCreature->getY());
            if (!rTile.isAirBlocked() && !rTile.hasPortal()) {
                // Tell the surrounding PCs that the creature has disappeared.
                GCDeleteObject gcDeleteObject(pCreature->getObjectID());
                pZone->broadcastPacket(pCreature->getX(), pCreature->getY(), &gcDeleteObject, pCreature);

                // A tile files a creature under its move mode, so the mode is changed
                // by taking the creature off its tile and adding it again.
                pZone->deleteCreatureFromTile(pCreature, pCreature->getX(), pCreature->getY());
                pCreature->setMoveMode(Creature::MOVE_MODE_FLYING);
                pZone->addCreatureToTile(pCreature, pCreature->getX(), pCreature->getY());

                GCAddEffect gcAddEffect;
                gcAddEffect.setObjectID(pCreature->getObjectID());
                gcAddEffect.setEffectID(Effect::EFFECT_CLASS_GHOST);
                gcAddEffect.setDuration(static_cast<Duration_t>(999999));
                pGamePlayer->sendPacket(&gcAddEffect);

                EffectGhost* pEffect = new EffectGhost(pCreature);
                pCreature->getEffectManager()->addEffect(pEffect);
                pCreature->setFlag(Effect::EFFECT_CLASS_GHOST);

                gcSystemMessage.setMessage(msg);
            } else {
                gcSystemMessage.setMessage(de::gameContext().strings().getString(STRID_AIR_BLOCKED));
            }
        }
        filelog("change.txt", "[%s]%s", pCreature->getName().c_str(), gcSystemMessage.toString().c_str());
    } else if (value1 == "off") {
        char msg[50];
        snprintf(msg, sizeof(msg), de::gameContext().strings().c_str(STRID_GHOST), "OFF");

        if (!bGhost)
            gcSystemMessage.setMessage(msg);
        else {
            pCreature->getEffectManager()->deleteEffect(Effect::EFFECT_CLASS_GHOST);
            pCreature->removeFlag(Effect::EFFECT_CLASS_GHOST);
            gcSystemMessage.setMessage(msg);

            GCRemoveEffect gcRemoveEffect;
            gcRemoveEffect.setObjectID(pCreature->getObjectID());
            gcRemoveEffect.addEffectList(Effect::EFFECT_CLASS_GHOST);
            pGamePlayer->sendPacket(&gcRemoveEffect);

            Zone* pZone = pCreature->getZone();
            Assert(pZone != NULL);

            Tile& rTile = pZone->getTile(pCreature->getX(), pCreature->getY());
            if (!rTile.isGroundBlocked() && !rTile.hasPortal()) {
                // A tile files a creature under its move mode, so the mode is changed
                // by taking the creature off its tile and adding it again.
                pZone->deleteCreatureFromTile(pCreature, pCreature->getX(), pCreature->getY());
                pCreature->setMoveMode(Creature::MOVE_MODE_WALKING);
                pZone->addCreatureToTile(pCreature, pCreature->getX(), pCreature->getY());

                if (pCreature->isSlayer()) {
                    Slayer* pSlayer = dynamic_cast<Slayer*>(pCreature);

                    GCAddSlayer gcAddSlayer;
                    makeGCAddSlayer(&gcAddSlayer, pSlayer);
                    pZone->broadcastPacket(&gcAddSlayer, pCreature);
                } else if (pCreature->isVampire()) {
                    Vampire* pVampire = dynamic_cast<Vampire*>(pCreature);

                    GCAddVampire gcAddVampire;
                    makeGCAddVampire(&gcAddVampire, pVampire);
                    pZone->broadcastPacket(&gcAddVampire, pCreature);
                } else if (pCreature->isOusters()) {
                    Ousters* pOusters = dynamic_cast<Ousters*>(pCreature);

                    GCAddOusters gcAddOusters;
                    makeGCAddOusters(&gcAddOusters, pOusters);
                    pZone->broadcastPacket(&gcAddOusters, pCreature);
                }
            } else {
                gcSystemMessage.setMessage(de::gameContext().strings().getString(STRID_GROUND_BLOCKED));
            }
        }

        filelog("change.txt", "[%s]%s", pCreature->getName().c_str(), gcSystemMessage.toString().c_str());
    } else {
        bSendPacket = false;
    }
}

// *command clearInventory
void opClearInventory(GamePlayer* pGamePlayer, const string& value1, GCSystemMessage& gcSystemMessage,
                      bool& bSendPacket) {
    gcSystemMessage.setMessage(de::gameContext().strings().getString(STRID_CLEAR_INVENTORY));

    Creature* pCreature = pGamePlayer->getCreature();
    PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pCreature);
    Inventory* pInventory = pPC->getInventory();

    int i, j;

    try {
        if (pInventory != NULL) {
            for (j = 0; j < pInventory->getHeight(); j++) {
                for (i = 0; i < pInventory->getWidth(); i++) {
                    Item* pItem = pInventory->getItem(i, j);

                    if (pItem != NULL) {
                        // Delete it unless it is a key or a relic.
                        if (pItem->getItemClass() != Item::ITEM_CLASS_KEY && !isRelicItem(pItem)) {
                            // Leave a log for a unique item.
                            if (pItem->isUnique()) {
                                filelog("uniqueItem.txt", "[ClearInventory] %s", pItem->toString().c_str());
                            }

                            pInventory->deleteItem(i, j);
                            pItem->whenPCLost(pPC);
                            pItem->destroy();

                            // Leave an ItemTrace Log
                            if (pItem != NULL && pItem->isTraceItem()) {
                                remainTraceLog(pItem, pCreature->getName(), "GOD", ITEM_LOG_DELETE, DETAIL_OPCLEAR);
                            }

                            SAFE_DELETE(pItem);
                        }
                    } // end of if (pItem != NULL)
                } // end of for
            } // end of for
        }

        // Re-send the inventory packets.
        transportCreature(pCreature, pCreature->getZone()->getZoneID(), pCreature->getX(), pCreature->getY(), false);
    } catch (Throwable& t) {
        gcSystemMessage.setMessage(t.toString().c_str());
    }
}

// *command clearRankBonus
void opClearRankBonus(GamePlayer* pGamePlayer, const string& value1, GCSystemMessage& gcSystemMessage,
                      bool& bSendPacket) {
    gcSystemMessage.setMessage("Rank Bonus Clear");

    Creature* pCreature = pGamePlayer->getCreature();
    PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pCreature);

    pPC->clearRankBonus();
    if (pPC->isSlayer()) {
        Slayer* pSlayer = dynamic_cast<Slayer*>(pPC);
        Assert(pSlayer != NULL);
        pSlayer->initAllStat();
    } else if (pPC->isVampire()) {
        Vampire* pVampire = dynamic_cast<Vampire*>(pPC);
        Assert(pVampire != NULL);
        pVampire->initAllStat();
    } else if (pPC->isOusters()) {
        Ousters* pOusters = dynamic_cast<Ousters*>(pPC);
        Assert(pOusters != NULL);
        pOusters->initAllStat();
    }
    pPC->sendRankBonusInfo();
}

// *command setCastleOwner
void opSetCastleOwner(GamePlayer* pGamePlayer, const string& value1, GCSystemMessage& gcSystemMessage,
                      bool& bSendPacket) {
    Creature* pCreature = pGamePlayer->getCreature();
    Zone* pZone = pCreature->getZone();
    const string& Name = value1;

    bSendPacket = false;

    if (pZone->isCastle()) {
        // *command setCastleOwner SlayerCommon makes it a Slayer common castle
        if (value1 == "SlayerCommon") {
            de::gameContext().castleInfos().modifyCastleOwner(pZone->getZoneID(), RACE_SLAYER, 99);
        }
        // *command setCastleOwner VampireCommon makes it a Vampire common castle
        else if (value1 == "VampireCommon") {
            de::gameContext().castleInfos().modifyCastleOwner(pZone->getZoneID(), RACE_VAMPIRE, 0);
        } else if (value1 == "OustersCommon") {
            de::gameContext().castleInfos().modifyCastleOwner(pZone->getZoneID(), RACE_OUSTERS, 66);
        }
        // *command setCastleOwner <character name> makes it that character's guild castle
        else {
            GuildID_t guildID;
            Race_t race;
            if (getRaceFromDB(Name, race)) {
                if (getGuildIDFromDB(Name, race, guildID)) {
                    de::gameContext().castleInfos().modifyCastleOwner(pZone->getZoneID(), race, guildID);
                } else {
                    gcSystemMessage.setMessage(de::gameContext().strings().getString(STRID_DO_NOT_BELONG_TO_GUILD));
                    bSendPacket = true;
                }
            } else {
                gcSystemMessage.setMessage(de::gameContext().strings().getString(STRID_NO_SUCH_CHARACTOR));
                bSendPacket = true;
            }
        }
    } else {
        gcSystemMessage.setMessage(de::gameContext().strings().getString(STRID_NOT_IN_CASTLE));
        bSendPacket = true;
    }
}

// *command setCastleOwnerGuild
void opSetCastleOwnerGuild(GamePlayer* pGamePlayer, const string& value1, GCSystemMessage& gcSystemMessage,
                           bool& bSendPacket) {
    size_t j = value1.find_first_of(' ', 0);
    ZoneID_t zoneID = (ZoneID_t)atoi(trim(value1.substr(0, j)).c_str());
    GuildID_t guildID = (GuildID_t)atoi(trim(value1.substr(j + 1, value1.size() - j - 1)).c_str());

    bSendPacket = false;

    Zone* pZone = getZoneByZoneID(zoneID);
    Guild* pGuild = de::gameContext().guilds().getGuild(guildID);
    if (pZone == NULL || !pZone->isCastle() || (pGuild == NULL && guildID != 99 && guildID != 0 && guildID != 66))
        return;

    // The guild's race, or the race whose common guild the id names. The castle may be another zone group's, and
    // a change of its owning race reloads its war scheduler, freeing wars that group's thread executes, so the
    // change is posted to the castle's group and runs there under its mutex.
    Race_t race = (pGuild != NULL)  ? (Race_t)pGuild->getRace()
                  : (guildID == 99) ? RACE_SLAYER
                  : (guildID == 0)  ? RACE_VAMPIRE
                                    : RACE_OUSTERS;
    pZone->getZoneGroup()->post(
        [zoneID, race, guildID] { de::gameContext().castleInfos().modifyCastleOwner(zoneID, race, guildID); });
}

// *command showWarList
void opShowWarList(GamePlayer* pGamePlayer, const string& value1, GCSystemMessage& gcSystemMessage, bool& bSendPacket) {
    // Send the list of wars in progress.

    de::gameContext().warSystem().broadcastWarList(pGamePlayer);
    bSendPacket = false;
}

// *command startRaceWar
void opStartRaceWar(GamePlayer* pGamePlayer, const string& value1, GCSystemMessage& gcSystemMessage,
                    bool& bSendPacket) {
    if (!de::gameContext().variables().isWarActive()) {
        gcSystemMessage.setMessage(de::gameContext().strings().getString(STRID_WAR_OFF_DO_WAR_ACITIVE_ON));
    } else if (de::gameContext().warSystem().startRaceWar()) {
        gcSystemMessage.setMessage(de::gameContext().strings().getString(STRID_RACE_WAR_START));
    } else {
        gcSystemMessage.setMessage(de::gameContext().strings().getString(STRID_ALREADY_WAR_STARTED_OF_SERVER_ERROR));
    }
    bSendPacket = true;
}

// *command startWar
void opStartWar(GamePlayer* pGamePlayer, const string& value1, GCSystemMessage& gcSystemMessage, bool& bSendPacket) {
    Creature* pCreature = pGamePlayer->getCreature();
    Zone* pZone = pCreature->getZone();

    WarScheduler* pScheduler = pZone->getWarScheduler();
    if (pScheduler != NULL) {
        Schedule* pSchedule = pScheduler->getRecentSchedule();
        if (pSchedule != NULL)
            pSchedule->setScheduledTime(VSDateTime::currentDateTime());
    }
}

// *command removeWar
void opRemoveWar(GamePlayer* pGamePlayer, const string& value1, GCSystemMessage& gcSystemMessage, bool& bSendPacket) {
    ZoneID_t zoneID = atoi(value1.c_str());

    if (zoneID == 0) {
        if (pGamePlayer != NULL) {
            Creature* pCreature = pGamePlayer->getCreature();
            Zone* pZone = pCreature->getZone();

            zoneID = pZone->getZoneID();
        } else {
            return answerNothing(bSendPacket);
        }
    }

    char msg[100];
    if (de::gameContext().warSystem().removeWar(zoneID)) {
        snprintf(msg, sizeof(msg), de::gameContext().strings().c_str(STRID_GUILD_WAR_REMOVED), (int)zoneID);
        gcSystemMessage.setMessage(msg);
    } else {
        snprintf(msg, sizeof(msg), de::gameContext().strings().c_str(STRID_NO_GUILD_WAR_IN_ACTIVE), (int)zoneID);
        gcSystemMessage.setMessage(msg);
    }
    bSendPacket = true;
}

// *command removeRaceWar
void opRemoveRaceWar(GamePlayer* pGamePlayer, const string& value1, GCSystemMessage& gcSystemMessage,
                     bool& bSendPacket) {
    if (de::gameContext().warSystem().removeRaceWar()) {
        gcSystemMessage.setMessage(de::gameContext().strings().getString(STRID_RACE_WAR_REMOVED));
    } else {
        gcSystemMessage.setMessage(de::gameContext().strings().getString(STRID_NO_RACE_WAR_IN_ACTIVE));
    }
    bSendPacket = true;
}

// *command LevelWar
void opLevelWar(GamePlayer* pGamePlayer, const string& value1, GCSystemMessage& gcSystemMessage, bool& bSendPacket) {
    if (!de::gameContext().variables().isWarActive()) {
        gcSystemMessage.setMessage(de::gameContext().strings().getString(STRID_WAR_OFF_DO_WAR_ACITIVE_ON));
    }
    if (de::gameContext().variables().isActiveLevelWar()) {
        ZoneID_t zoneID = atoi(value1.c_str());

        if (zoneID != 1131 && zoneID != 1132 && zoneID != 1133 && zoneID != 1134)
            return answerNothing(bSendPacket);

        Zone* pZone = getZoneByZoneID(zoneID);

        Assert(pZone != NULL);

        LevelWarManager* pLevelWarManager = pZone->getLevelWarManager();

        if (pLevelWarManager != NULL) {
            gcSystemMessage.setMessage("LevelWar advanced.");
            pLevelWarManager->manualStart();
        }
    }
}

// *command saveBloodBibleOwner
void opSaveBloodBibleOwner(GamePlayer* pGamePlayer, const string& value1, GCSystemMessage& gcSystemMessage,
                           bool& bSendPacket) {
    de::gameContext().shrines().saveBloodBibleOwner();

    gcSystemMessage.setMessage(de::gameContext().strings().getString(STRID_SAVE_BLOOD_BIBLE_OWNER_INFO_IN_DB));

    bSendPacket = true;
}

// *command killAllMonster
void opKillAllMonster(GamePlayer* pGamePlayer, const string& value1, GCSystemMessage& gcSystemMessage,
                      bool& bSendPacket) {
    if (pGamePlayer != NULL) {
        Creature* pCreature = pGamePlayer->getCreature();
        Zone* pZone = pCreature->getZone();
        MonsterManager* pMM = pZone->getMonsterManager();
        unordered_map<ObjectID_t, Creature*>& monsters = pMM->getCreatures();
        unordered_map<ObjectID_t, Creature*>::iterator itr = monsters.begin();

        for (; itr != monsters.end(); itr++) {
            Creature* pCreature = itr->second;
            Monster* pMonster = dynamic_cast<Monster*>(pCreature);

            if (!pMonster->isFlag(Effect::EFFECT_CLASS_NO_DAMAGE)) {
                pMonster->setHP(0);
            }
        }

        gcSystemMessage.setMessage(de::gameContext().strings().getString(STRID_KILL_ALL_MONSTER_IN_ZONE));
    }
}

// *command killAllPC
void opKillAllPC(GamePlayer* pGamePlayer, const string& value1, GCSystemMessage& gcSystemMessage, bool& bSendPacket) {
    if (pGamePlayer != NULL) {
        Creature* pCreature = pGamePlayer->getCreature();
        Zone* pZone = pCreature->getZone();
        PCManager* pPM = (PCManager*)pZone->getPCManager();
        unordered_map<ObjectID_t, Creature*>& pcs = pPM->getCreatures();
        unordered_map<ObjectID_t, Creature*>::iterator itr = pcs.begin();

        for (; itr != pcs.end(); itr++) {
            Creature* pCreature = itr->second;
            PlayerCreature* pPlayerCreature = dynamic_cast<PlayerCreature*>(pCreature);

            if (!pPlayerCreature->isFlag(Effect::EFFECT_CLASS_NO_DAMAGE)) {
                if (pPlayerCreature->isSlayer()) {
                    dynamic_cast<Slayer*>(pPlayerCreature)->setHP(0);
                }
                if (pPlayerCreature->isVampire()) {
                    dynamic_cast<Vampire*>(pPlayerCreature)->setHP(0);
                }
                if (pPlayerCreature->isOusters()) {
                    dynamic_cast<Ousters*>(pPlayerCreature)->setHP(0);
                }
            }
        }

        gcSystemMessage.setMessage(de::gameContext().strings().getString(STRID_KILL_ALL_MONSTER_IN_ZONE));
    }
}

// *command showZonePCNum
void opShowZonePCNum(GamePlayer* pGamePlayer, const string& value1, GCSystemMessage& gcSystemMessage,
                     bool& bSendPacket) {
    if (pGamePlayer != NULL) {
        WORD num = pGamePlayer->getCreature()->getZone()->getPCManager()->getSize();
        char msg[100];
        snprintf(msg, sizeof(msg), de::gameContext().strings().getString(STRID_PC_NUM).c_str(), num);
        gcSystemMessage.setMessage(msg);
    }
}

// *command showPKZonePCNum
void opShowPKZonePCNum(GamePlayer* pGamePlayer, const string& value1, GCSystemMessage& gcSystemMessage,
                       bool& bSendPacket) {
    if (pGamePlayer != NULL) {
        ZoneID_t zoneID = pGamePlayer->getCreature()->getZone()->getZoneID();
        if (de::gameContext().pkZoneInfos().isPKZone(zoneID)) {
            int num = de::gameContext().pkZoneInfos().getPKZoneInfo(zoneID)->getCurrentPCNum();

            char msg[100];
            snprintf(msg, sizeof(msg), de::gameContext().strings().getString(STRID_PC_NUM).c_str(), num);
            gcSystemMessage.setMessage(msg);
        } else {
            bSendPacket = false;
        }
    }
}

// *command setPKZonePCNum
void opSetPKZonePCNum(GamePlayer* pGamePlayer, const string& value1, GCSystemMessage& gcSystemMessage,
                      bool& bSendPacket) {
    if (pGamePlayer != NULL) {
        ZoneID_t zoneID = pGamePlayer->getCreature()->getZone()->getZoneID();

        size_t j = value1.find_first_of(' ', 0);
        int num = atoi(trim(value1.substr(0, j)).c_str());

        if (de::gameContext().pkZoneInfos().isPKZone(zoneID)) {
            de::gameContext().pkZoneInfos().getPKZoneInfo(zoneID)->setCurrentPCNum(num);
        }
    }
}

// *command suicide
void opSuicide(GamePlayer* pGamePlayer, const string& value1, GCSystemMessage& gcSystemMessage, bool& bSendPacket) {
    Creature* pCreature = pGamePlayer->getCreature();
    if (pCreature != NULL) {
        if (pCreature->isSlayer()) {
            Slayer* pSlayer = dynamic_cast<Slayer*>(pCreature);
            pSlayer->setHP(0);
        } else if (pCreature->isVampire()) {
            Vampire* pVampire = dynamic_cast<Vampire*>(pCreature);
            pVampire->setHP(0);
        } else if (pCreature->isOusters()) {
            Ousters* pOusters = dynamic_cast<Ousters*>(pCreature);
            pOusters->setHP(0);
        }
    }

    bSendPacket = false;
}

// *command heal
void opHeal(GamePlayer* pGamePlayer, const string& value1, GCSystemMessage& gcSystemMessage, bool& bSendPacket) {
    Creature* pCreature = pGamePlayer->getCreature();
    if (pCreature != NULL) {
        Zone* pZone = pCreature->getZone();

        if (pCreature->isFlag(Effect::EFFECT_CLASS_COMA)) {
            // Delete the coma effect from the target's effect manager.
            pCreature->deleteEffect(Effect::EFFECT_CLASS_COMA);
            pCreature->removeFlag(Effect::EFFECT_CLASS_COMA);

            // Tell the others that the coma effect is gone.
            GCRemoveEffect gcRemoveEffect;
            gcRemoveEffect.setObjectID(pCreature->getObjectID());
            gcRemoveEffect.addEffectList((EffectID_t)Effect::EFFECT_CLASS_COMA);
            pZone->broadcastPacket(pCreature->getX(), pCreature->getY(), &gcRemoveEffect);

            // Send the effect information again.
            pCreature->getEffectManager()->sendEffectInfo(pCreature, pZone, pCreature->getX(), pCreature->getY());
        }

        HP_t hp = 0;
        MP_t mp = 0;

        if (pCreature->isSlayer()) {
            Slayer* pSlayer = dynamic_cast<Slayer*>(pCreature);
            hp = pSlayer->getHP(ATTR_MAX);
            pSlayer->setHP(hp);
            mp = pSlayer->getMP(ATTR_MAX);
            pSlayer->setMP(mp);
        } else if (pCreature->isVampire()) {
            Vampire* pVampire = dynamic_cast<Vampire*>(pCreature);
            hp = pVampire->getHP(ATTR_MAX);
            pVampire->setHP(hp);

            if (pVampire->getSilverDamage() != 0) {
                pVampire->saveSilverDamage(0);
                GCModifyInformation gcMI;
                gcMI.addShortData(MODIFY_SILVER_DAMAGE, 0);
                pGamePlayer->sendPacket(&gcMI);
            }
        } else if (pCreature->isOusters()) {
            Ousters* pOusters = dynamic_cast<Ousters*>(pCreature);
            hp = pOusters->getHP(ATTR_MAX);
            pOusters->setHP(hp);

            if (pOusters->getSilverDamage() != 0) {
                pOusters->saveSilverDamage(0);
                GCModifyInformation gcMI;
                gcMI.addShortData(MODIFY_SILVER_DAMAGE, 0);
                pGamePlayer->sendPacket(&gcMI);
            }

            mp = pOusters->getMP(ATTR_MAX) * 2;
            pOusters->setMP(mp);
        }

        if (hp != 0) {
            // Report that the HP was filled.
            GCStatusCurrentHP gcStatusCurrentHP;
            gcStatusCurrentHP.setObjectID(pCreature->getObjectID());
            gcStatusCurrentHP.setCurrentHP(hp);
            pZone->broadcastPacket(pCreature->getX(), pCreature->getY(), &gcStatusCurrentHP);
        }

        if (mp != 0) {
            GCModifyInformation gcMI;
            gcMI.addShortData(MODIFY_CURRENT_MP, mp);
            pGamePlayer->sendPacket(&gcMI);
        }
    }

    bSendPacket = false;
}

// *command setGold
void opSetGold(GamePlayer* pGamePlayer, const string& value1, GCSystemMessage& gcSystemMessage, bool& bSendPacket) {
    Gold_t gold = atoi(value1.c_str());
    Creature* pCreature = pGamePlayer->getCreature();

    if (pCreature != NULL) {
        GCModifyInformation gcMI;

        if (pCreature->isSlayer()) {
            Slayer* pSlayer = dynamic_cast<Slayer*>(pCreature);
            pSlayer->setGoldEx(gold);

            gcMI.addLongData(MODIFY_GOLD, pSlayer->getGold());
        } else if (pCreature->isVampire()) {
            Vampire* pVampire = dynamic_cast<Vampire*>(pCreature);
            pVampire->setGoldEx(gold);

            gcMI.addLongData(MODIFY_GOLD, pVampire->getGold());
        } else if (pCreature->isOusters()) {
            Ousters* pOusters = dynamic_cast<Ousters*>(pCreature);
            pOusters->setGoldEx(gold);

            gcMI.addLongData(MODIFY_GOLD, pOusters->getGold());
        }

        pGamePlayer->sendPacket(&gcMI);

        // Leave a money log when the amount warrants one
        if (gold >= de::gameContext().variables().getMoneyTraceLogLimit()) {
            if (gold > 2000000000)
                gold = 2000000000;

            remainMoneyTraceLog("GOD", pCreature->getName(), ITEM_LOG_CREATE, DETAIL_COMMAND, gold);
        }
    }


    bSendPacket = false;
}

// *command Quest
void opQuest(GamePlayer* pGamePlayer, const string& value1, GCSystemMessage& gcSystemMessage, bool& bSendPacket) {
    Creature* pCreature = pGamePlayer->getCreature();
    PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pCreature);

    if (pPC != NULL && value1 == "Complete") {
        QuestManager* pQM = pPC->getQuestManager();
        if (pQM != NULL) {
            MonsterKillQuestStatus* pQS =
                dynamic_cast<MonsterKillQuestStatus*>(pQM->getQuestStatusByQuestClass(QUEST_CLASS_MONSTER_KILL));
            if (pQS != NULL) {
                pQS->completeQuest();
                pPC->sendCurrentQuestInfo();

                bSendPacket = false;
            }
        }
    }
}

// *command QuestEnding
void opQuestEnding(GamePlayer* pGamePlayer, const string& value1, GCSystemMessage& gcSystemMessage, bool& bSendPacket) {
    GCNoticeEvent gcNoticeEvent;
    gcNoticeEvent.setCode(NOTICE_EVENT_START_QUEST_ENDING);

    pGamePlayer->sendPacket(&gcNoticeEvent);
    bSendPacket = false;
}

// *command NotifyWin
void opNotifyWin(GamePlayer* pGamePlayer, const string& value1, GCSystemMessage& gcSystemMessage, bool& bSendPacket) {
    size_t j = value1.find_first_of(' ', 0);
    string name = value1.substr(0, j);
    int giftID = atoi(trim(value1.substr(j + 1, value1.size())).c_str());

    GCNotifyWin gcNW;
    gcNW.setGiftID(giftID);
    gcNW.setName(name);

    de::gameContext().zoneGroups().broadcast(&gcNW);
    bSendPacket = false;
}

// *command Horn
void opHorn(GamePlayer* pGamePlayer, const string& value1, GCSystemMessage& gcSystemMessage, bool& bSendPacket) {
    GCNoticeEvent gcNE;
    gcNE.setCode(NOTICE_EVENT_RUN_HORN);

    pGamePlayer->sendPacket(&gcNE);
    bSendPacket = false;
}

// *command Loud
void opLoud(GamePlayer* pGamePlayer, const string& value1, GCSystemMessage& gcSystemMessage, bool& bSendPacket) {
    Creature* pCreature = pGamePlayer->getCreature();

    size_t j = value1.find_first_of(' ', 0);
    string name = value1.substr(0, j);
    int time = atoi(trim(value1.substr(j + 1, value1.size())).c_str());

    Effect* pEffect = NULL;
    if (pCreature->isFlag(Effect::EFFECT_CLASS_LOUD)) {
        pEffect = pCreature->findEffect(Effect::EFFECT_CLASS_LOUD);

        pEffect->setDeadline(time * 10);
    } else {
        pEffect = new EffectLoud(pCreature);
        pEffect->setDeadline(time * 10);
        pCreature->addEffect(pEffect);
        pCreature->setFlag(pEffect->getEffectClass());
    }

    GCAddEffect gcAddEffect;
    gcAddEffect.setObjectID(pCreature->getObjectID());
    gcAddEffect.setEffectID(pEffect->getSendEffectClass());
    gcAddEffect.setDuration(time * 10);

    pCreature->getZone()->broadcastPacket(pCreature->getX(), pCreature->getY(), &gcAddEffect, pCreature);
    pGamePlayer->sendPacket(&gcAddEffect);

    bSendPacket = false;
}

// *command Game
void opGame(GamePlayer* pGamePlayer, const string& value1, GCSystemMessage& gcSystemMessage, bool& bSendPacket) {
    size_t j = value1.find_first_of(' ', 0);
    string name = value1.substr(0, j);

    GCNoticeEvent gcNE;
    gcNE.setCode(NOTICE_EVENT_MINI_GAME);

    if (name == "Mine") {
        gcNE.setParameter(0);
    } else if (name == "Nemo") {
        gcNE.setParameter(1);
    } else if (name == "Push") {
        gcNE.setParameter(2);
    } else if (name == "Mine") {
        gcNE.setParameter(3);
    } else if (name == "Arrow") {
        gcNE.setParameter(4);
    } else {
        gcNE.setParameter(atoi(name.c_str()));
    }
}

// *command changeSex
void opChangeSex(GamePlayer* pGamePlayer, const string& value1, GCSystemMessage& gcSystemMessage, bool& bSendPacket) {
    Creature* pCreature = pGamePlayer->getCreature();
    PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pCreature);

    if (pPC != NULL) {
        if (changeSexEx(pPC))
            bSendPacket = false;
    }
}

// *command Firecraker
void opFirecraker(GamePlayer* pGamePlayer, const string& value1, GCSystemMessage& gcSystemMessage, bool& bSendPacket) {
    Creature* pCreature = pGamePlayer->getCreature();
    if (pCreature != NULL) {
        Zone* pZone = pCreature->getZone();

        if (!isAbleToUseTileSkill(pCreature) ||
            (pZone->getZoneLevel(pCreature->getX(), pCreature->getY()) & COMPLETE_SAFE_ZONE) ||
            atoi(value1.c_str()) < 0 || atoi(value1.c_str()) > 13) {
            // firecracker
        } else {
            Effect::EffectClass effectClass = FirecrackerEffects[atoi(value1.c_str())];
            // Build the effect and broadcast it.
            GCAddEffectToTile gcAddEffectToTile;
            gcAddEffectToTile.setObjectID(pCreature->getObjectID());
            gcAddEffectToTile.setEffectID(effectClass);
            gcAddEffectToTile.setXY(pCreature->getX(), pCreature->getY());
            gcAddEffectToTile.setDuration(10); // no real meaning, just 1 second

            pZone->broadcastPacket(pCreature->getX(), pCreature->getY(), &gcAddEffectToTile);
            bSendPacket = false;
        }
    }
}

// *command Bulletin
void opBulletin(GamePlayer* pGamePlayer, const string& value1, GCSystemMessage& gcSystemMessage, bool& bSendPacket) {
    Creature* pCreature = pGamePlayer->getCreature();
    if (pCreature != NULL) {
        Zone* pZone = pCreature->getZone();

        size_t l = value1.find_first_of(' ', 0);

        int bulletinLevel = atoi(value1.substr(0, l).c_str());
        string bulletinMessage = value1.substr(l + 1, value1.size() - l - 1) + "%" + pCreature->getName();

        MonsterType_t MType = 0;
        int time = 0;

        switch (bulletinLevel) {
        case 1:
            MType = 650;
            time = 21600;
            break;
        case 2:
            MType = 650;
            time = 43200;
            break;
        case 3:
            MType = 650;
            time = 86400;
            break;
        default:
            break;
        }

        if (MType != 0 && l <= value1.size()) {
            if (createBulletinBoard(pZone, pCreature->getX(), pCreature->getY(), MType, bulletinMessage,
                                    VSDateTime::currentDateTime().addSecs(time))) {
                bSendPacket = false;
            }
        }
    }
}

// *command SetHP
void opSetHP(GamePlayer* pGamePlayer, const string& value1, GCSystemMessage& gcSystemMessage, bool& bSendPacket) {
    Creature* pCreature = pGamePlayer->getCreature();
    if (pCreature != NULL) {
        Zone* pZone = pCreature->getZone();

        HP_t hp = (HP_t)atoi(value1.c_str());
        if (pCreature->isSlayer()) {
            dynamic_cast<Slayer*>(pCreature)->setHP(hp);
        } else if (pCreature->isVampire()) {
            dynamic_cast<Vampire*>(pCreature)->setHP(hp);
        } else if (pCreature->isOusters()) {
            dynamic_cast<Ousters*>(pCreature)->setHP(hp);
        }

        GCStatusCurrentHP gcHP;
        gcHP.setObjectID(pCreature->getObjectID());
        gcHP.setCurrentHP(hp);

        pZone->broadcastPacket(pCreature->getX(), pCreature->getY(), &gcHP);
        bSendPacket = false;
    }
}

// *command ResetAttr
void opResetAttr(GamePlayer* pGamePlayer, const string& value1, GCSystemMessage& gcSystemMessage, bool& bSendPacket) {
    Creature* pCreature = pGamePlayer->getCreature();
    char buffer[100];

    if (pCreature != NULL) {
        if (pCreature->isSlayer()) {
            gcSystemMessage.setMessage("Slayer attributes cannot be reset.");
        } else if (pCreature->isVampire()) {
            Vampire* pVampire = dynamic_cast<Vampire*>(pCreature);
            if (pVampire != NULL)
                ;
            {
                VAMPIRE_RECORD prev;
                pVampire->getVampireRecord(prev);

                pVampire->setSTR(20, ATTR_BASIC);
                pVampire->setDEX(20, ATTR_BASIC);
                pVampire->setINT(20, ATTR_BASIC);

                pVampire->setBonus(3 * (pVampire->getLevel() - 1));

                pVampire->initAllStat();
                pVampire->sendModifyInfo(prev);

                snprintf(buffer, sizeof(buffer), "STR=20, DEX=20, INTE=20, Bonus=%d", pVampire->getBonus());
                pVampire->tinysave(buffer);
            }
        } else if (pCreature->isOusters()) {
            Ousters* pOusters = dynamic_cast<Ousters*>(pCreature);
            if (pOusters != NULL)
                ;
            {
                OUSTERS_RECORD prev;
                pOusters->getOustersRecord(prev);

                pOusters->setSTR(10, ATTR_BASIC);
                pOusters->setDEX(10, ATTR_BASIC);
                pOusters->setINT(10, ATTR_BASIC);

                pOusters->setBonus(3 * (pOusters->getLevel() - 1) + 15);

                pOusters->initAllStat();
                pOusters->sendModifyInfo(prev);

                snprintf(buffer, sizeof(buffer), "STR=10, DEX=10, INTE=10, Bonus=%d", pOusters->getBonus());
                pOusters->tinysave(buffer);
            }
        }
    }
}

// *command CTF
void opCTF(GamePlayer* pGamePlayer, const string& value1, GCSystemMessage& gcSystemMessage, bool& bSendPacket) {
    de::gameContext().flags().manualStart();
    gcSystemMessage.setMessage("CTF advanced.");
}

// *command ViewDamage
void opViewDamage(GamePlayer* pGamePlayer, const string& value1, GCSystemMessage& gcSystemMessage, bool& bSendPacket) {
    Creature* pCreature = pGamePlayer->getCreature();
    Assert(pCreature != NULL);

    bool bViewDamage = pCreature->isFlag(Effect::EFFECT_CLASS_VIEW_HP);

    if (value1 == "on") {
        if (!bViewDamage) {
            addSimpleCreatureEffect(pCreature, Effect::EFFECT_CLASS_VIEW_HP, -1, false);

            GCAddEffect gcAddEffect;
            gcAddEffect.setObjectID(pCreature->getObjectID());
            gcAddEffect.setEffectID(Effect::EFFECT_CLASS_VIEW_HP);
            gcAddEffect.setDuration(65535);

            pGamePlayer->sendPacket(&gcAddEffect);
        }
    } else if (value1 == "off") {
        if (bViewDamage) {
            Effect* pEffect = pCreature->findEffect(Effect::EFFECT_CLASS_VIEW_HP);
            if (pEffect != NULL)
                pEffect->setDeadline(0);
        }
    }
}

// *command GoodsReload
void opGoodsReload(GamePlayer* pGamePlayer, const string& value1, GCSystemMessage& gcSystemMessage, bool& bSendPacket) {
    PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pGamePlayer->getCreature());
    if (pPC != NULL) {
        pPC->loadGoods();
        pPC->registerGoodsInventory(pPC->getZone()->getObjectRegistry());
        gcSystemMessage.setMessage("Goods relading..");
    }
}

// *command PetStash
void opPetStash(GamePlayer* pGamePlayer, const string& value1, GCSystemMessage& gcSystemMessage, bool& bSendPacket) {
    PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pGamePlayer->getCreature());
    if (pPC != NULL) {
        GCPetStashList gcPetStashList;
        makeGCPetStashList(&gcPetStashList, pPC);

        pGamePlayer->sendPacket(&gcPetStashList);
        bSendPacket = false;
    }
}

// *command ZoneEvent
void opZoneEvent(GamePlayer* pGamePlayer, const string& value1, GCSystemMessage& gcSystemMessage, bool& bSendPacket) {
    Creature* pCreature = pGamePlayer->getCreature();
    EventZoneInfo* pEventZoneInfo =
        EventZoneInfoManager::Instance().getEventZoneInfo(pCreature->getZone()->getZoneID());
    if (pEventZoneInfo == NULL) {
        gcSystemMessage.setMessage("Not an event zone.");
    } else if (value1 == "on") {
        WORD EventID = pEventZoneInfo->getEventID();
        ZoneEventInfo* pZoneEventInfo = EventZoneInfoManager::Instance().getZoneEventInfo(EventID);
        EventZoneInfo* pCurrentEventZoneInfo = pZoneEventInfo->getCurrentEventZoneInfo();

        if (pCurrentEventZoneInfo != NULL && pCurrentEventZoneInfo != pEventZoneInfo) {
            gcSystemMessage.setMessage("An event is already running.");
        } else if (pCurrentEventZoneInfo == NULL) {
            pEventZoneInfo->turnOn();
            gcSystemMessage.setMessage("Event started.");
        } else
            gcSystemMessage.setMessage("An event is already running.");
    } else if (value1 == "off") {
        if (pEventZoneInfo->isEventOn()) {
            pEventZoneInfo->turnOff();
            gcSystemMessage.setMessage("Event stopped.");
        } else
            gcSystemMessage.setMessage("No event is running.");
    }
}

// *command EventZonePCLimit
void opEventZonePCLimit(GamePlayer* pGamePlayer, const string& value1, GCSystemMessage& gcSystemMessage,
                        bool& bSendPacket) {
    Creature* pCreature = pGamePlayer->getCreature();
    EventZoneInfo* pEventZoneInfo =
        EventZoneInfoManager::Instance().getEventZoneInfo(pCreature->getZone()->getZoneID());
    if (pEventZoneInfo == NULL) {
        gcSystemMessage.setMessage("Not an event zone.");
    } else {
        WORD lim = (WORD)atoi(value1.c_str());
        pEventZoneInfo->setPCLimit(lim);
        char buffer[100];
        snprintf(buffer, sizeof(buffer), "PC limit : %u", lim);
        gcSystemMessage.setMessage(buffer);
    }
}

// *command KickOutAll
void opKickOutAll(GamePlayer* pGamePlayer, const string& value1, GCSystemMessage& gcSystemMessage, bool& bSendPacket) {
    Creature* pCreature = pGamePlayer->getCreature();
    PCManager* pPCManager = (PCManager*)pCreature->getZone()->getPCManager();
    pPCManager->transportAllCreatures(1303, 46, 49);
    gcSystemMessage.setMessage("Moved every player in this zone to zone 1303.");
}

// *command StartTrap
void opStartTrap(GamePlayer* pGamePlayer, const string& value1, GCSystemMessage& gcSystemMessage, bool& bSendPacket) {
    Creature* pCreature = pGamePlayer->getCreature();
    if (pCreature->getZoneID() == 1410) {
        Zone* pZone = pCreature->getZone();

        EffectCastingIcicleTrap* pEffect = new EffectCastingIcicleTrap(Effect::EFFECT_CLASS_ICICLE_DROP, pZone);
        pZone->registerObject(pEffect);

        pEffect->setStartXY(116, 66);
        pEffect->setLength(48);
        pEffect->setTick(5);
        pEffect->setUnit(10);
        pEffect->setDir(7);
        pEffect->setNextTime(0);
        pEffect->setDeadline(6000);
        pZone->addEffect(pEffect);

        {
            EffectCastingIceWall* pEffect = new EffectCastingIceWall(pZone);

            pEffect->setStartXY(63, 19);
            pEffect->setLength(48);
            pEffect->setWallLength(5);
            pEffect->setTick(15);
            pEffect->setDir(1);
            pEffect->setNextTime(0);
            pEffect->setDeadline(6000);
            pZone->addEffect(pEffect);
        }

        pEffect = new EffectCastingIcicleTrap(Effect::EFFECT_CLASS_ICICLE_AUGER_LARGE, pZone);

        pEffect->setStartXY(15, 72);
        pEffect->setLength(47);
        pEffect->setTick(10);
        pEffect->setUnit(15);
        pEffect->setDir(3);
        pEffect->setLarge(true);
        pEffect->setNextTime(0);
        pEffect->setDeadline(6000);
        pZone->addEffect(pEffect);

        {
            EffectCastingSideTrap* pEffect = new EffectCastingSideTrap(pZone);

            pEffect->setStartXY(68, 119);
            pEffect->setLength(27);
            pEffect->setTick(10);
            pEffect->setUnit(5);
            pEffect->setDir(5);
            pEffect->setNextTime(0);
            pEffect->setDeadline(6000);
            pZone->addEffect(pEffect);
        }

        gcSystemMessage.setMessage("Traps started.");
    } else {
        gcSystemMessage.setMessage("Traps cannot be started in this zone.");
    }
}

// *command SMSTest
void opSMSTest(GamePlayer* pGamePlayer, const string& value1, GCSystemMessage& gcSystemMessage, bool& bSendPacket) {
    GCNoticeEvent gcNE;
    gcNE.setCode(NOTICE_EVENT_SEND_SMS);
    pGamePlayer->sendPacket(&gcNE);
}

// *command ForceNick
void opForceNick(GamePlayer* pGamePlayer, const string& value1, GCSystemMessage& gcSystemMessage, bool& bSendPacket) {
    size_t j = value1.find_first_of(' ', 0);
    string name = trim(value1.substr(0, j));
    string nick = trim(value1.substr(j + 1, value1.size()));

    cout << "ForceNick " << name << " : " << nick << endl;
    ;

    Creature* pTargetCreature;
    Creature* pCreature = pGamePlayer->getCreature();

    __ENTER_CRITICAL_SECTION(de::gameContext().playerCreatures())

    pTargetCreature = de::gameContext().playerCreatures().getCreature_LOCKED(name);

    __LEAVE_CRITICAL_SECTION(de::gameContext().playerCreatures())

    if (pTargetCreature == NULL || pTargetCreature->getZone() != pCreature->getZone() || !pTargetCreature->isPC()) {
        gcSystemMessage.setMessage("No such character in this zone.");
    } else {
        PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pTargetCreature);
        defaultNicknameRepository().replaceForcedNickname(pPC->getName(), NicknameInfo::NICK_CUSTOM_FORCED, nick);
        NicknameBook* pNickbook = pPC->getNicknameBook();
        NicknameInfo* pNick = pNickbook->getNicknameInfo(100);
        SAFE_DELETE(pNick);

        pNick = new NicknameInfo;
        pNick->setNicknameID(100);
        pNick->setNicknameType(NicknameInfo::NICK_CUSTOM_FORCED);
        pNick->setNickname(nick);
        pNickbook->setNicknameInfo(100, pNick);

        pPC->setNickname(pNick);
        GCModifyNickname gcMN;
        gcMN.setObjectID(pPC->getObjectID());
        gcMN.setNicknameInfo(pNick);

        pPC->getZone()->broadcastPacket(pPC->getX(), pPC->getY(), &gcMN);
        pPC->getPlayer()->sendPacket(pNickbook->getNicknameBookListPacket().get());
    }
}

// *command RemoveNick
void opRemoveNick(GamePlayer* pGamePlayer, const string& value1, GCSystemMessage& gcSystemMessage, bool& bSendPacket) {
    string name = trim(value1);
    cout << "RemoveNick " << name << endl;

    Creature* pTargetCreature;
    Creature* pCreature = pGamePlayer->getCreature();

    __ENTER_CRITICAL_SECTION(de::gameContext().playerCreatures())

    pTargetCreature = de::gameContext().playerCreatures().getCreature_LOCKED(name);

    __LEAVE_CRITICAL_SECTION(de::gameContext().playerCreatures())

    if (pTargetCreature == NULL || pTargetCreature->getZone() != pCreature->getZone() || !pTargetCreature->isPC()) {
        gcSystemMessage.setMessage("No such character in this zone.");
    } else {
        PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pTargetCreature);

        NicknameBook* pNickbook = pPC->getNicknameBook();
        NicknameInfo* pNick = pNickbook->getNicknameInfo(100);

        if (pNick == NULL) {
            gcSystemMessage.setMessage("No forced nickname to remove.");
        } else {
            defaultNicknameRepository().deleteForcedNickname(pPC->getName());
            if (pPC->getNickname() == pNick) {
                pPC->setNickname(NULL);
                GCModifyNickname gcMN;
                gcMN.setObjectID(pPC->getObjectID());

                NicknameInfo noNick;
                noNick.setNicknameType(NicknameInfo::NICK_NONE);
                gcMN.setNicknameInfo(&noNick);

                pPC->getZone()->broadcastPacket(pPC->getX(), pPC->getY(), &gcMN);
            }

            pNickbook->setNicknameInfo(100, NULL);
            SAFE_DELETE(pNick);
            pPC->getPlayer()->sendPacket(pNickbook->getNicknameBookListPacket().get());
        }
    }
}

// *command StartGDRLair
void opStartGDRLair(GamePlayer* pGamePlayer, const string& value1, GCSystemMessage& gcSystemMessage,
                    bool& bSendPacket) {
    if (GDRLairManager::Instance().getCurrentState() == GDR_LAIR_IDLE) {
        GDRLairIdle* pState = dynamic_cast<GDRLairIdle*>(GDRLairManager::Instance().getCurrentState_Object());
        if (pState != NULL) {
            pState->expire();
            gcSystemMessage.setMessage("Lair entrance opened.");
        }
    } else if (GDRLairManager::Instance().getCurrentState() == GDR_LAIR_ENTRANCE) {
        GDRLairEntrance* pState = dynamic_cast<GDRLairEntrance*>(GDRLairManager::Instance().getCurrentState_Object());
        if (pState != NULL) {
            pState->expire();
            gcSystemMessage.setMessage("Lair entrance closed.");
        }
    }
}

// *command ResetGDRLair
void opResetGDRLair(GamePlayer* pGamePlayer, const string& value1, GCSystemMessage& gcSystemMessage,
                    bool& bSendPacket) {
    if (GDRLairManager::Instance().getCurrentState() != GDR_LAIR_IDLE) {
        GDRLairManager::Instance().reset();
        gcSystemMessage.setMessage("Lair reset.");
    }
}

// *command GuildRecall
void opGuildRecall(GamePlayer* pGamePlayer, const string& value1, GCSystemMessage& gcSystemMessage, bool& bSendPacket) {
    if (pGamePlayer == NULL)
        return answerNothing(bSendPacket);

    Creature* pCreature = pGamePlayer->getCreature();
    Assert(pCreature != NULL);

    size_t j = value1.find_first_of(' ', 0);
    size_t k = value1.find_first_of(' ', j + 1);

    if (k == string::npos)
        k = value1.size();
    GuildID_t gid = atoi(trim(value1.substr(0, j)).c_str());
    int num = atoi(trim(value1.substr(j + 1, k)).c_str());
    int side = 0;

    if (k != value1.size()) {
        string sidestr = trim(value1.substr(k + 1, value1.size()));
        side = atoi(sidestr.c_str());
        if (side == 0) {
            if (sidestr == "defense") {
                side = 1;
            } else if (sidestr == "reinforce") {
                side = 2;
            } else if (sidestr == "attack1") {
                side = 3;
            } else if (sidestr == "attack2") {
                side = 4;
            } else if (sidestr == "attack3") {
                side = 5;
            } else if (sidestr == "attack4") {
                side = 6;
            } else if (sidestr == "attack5") {
                side = 7;
            }
        }
    }

    // The coordinates to summon to: the GM's own, read on the GM's zone thread.
    const ZoneID_t zoneID = pCreature->getZoneID();
    const Coord_t zoneX = pCreature->getX();
    const Coord_t zoneY = pCreature->getY();

    // The guild's players are named under the finder's lock and each recall
    // is posted to the thread that owns that player (PlayerMailbox.h): the
    // effects and the transport change the player's zone, which may be
    // ticking on another thread. At most `num` players are recalled, and a
    // count below one still recalls one.
    PCFinder& pcFinder = de::gameContext().playerCreatures();
    list<string> names;

    __ENTER_CRITICAL_SECTION(pcFinder)

    names = pcFinder.getGuildPlayerNames_LOCKED(gid);

    __LEAVE_CRITICAL_SECTION(pcFinder)

    const size_t maxRecalled = num > 0 ? (size_t)num : 1;
    size_t recalled = 0;

    for (list<string>::const_iterator itr = names.begin(); itr != names.end() && recalled < maxRecalled; ++itr) {
        const bool posted = de::postToPlayer(*itr, [zoneID, zoneX, zoneY, side](PlayerCreature& pc, Player&) {
            for (int i = 0; i < 7; ++i) {
                deleteCreatureEffect(&pc, (Effect::EffectClass)(Effect::EFFECT_CLASS_SIEGE_DEFENDER + i));
            }

            if (side < 8 && side > 0) {
                addSimpleCreatureEffect(&pc, (Effect::EffectClass)(Effect::EFFECT_CLASS_SIEGE_DEFENDER + side - 1));
            }

            transportCreature(&pc, zoneID, zoneX, zoneY, false);
        });

        if (posted)
            recalled++;
    }
}

// *command ResetSiege
void opResetSiege(GamePlayer* pGamePlayer, const string& value1, GCSystemMessage& gcSystemMessage, bool& bSendPacket) {
    SiegeManager::Instance().reset(pGamePlayer->getCreature()->getZoneID());
}

// *command InitSiege
void opInitSiege(GamePlayer* pGamePlayer, const string& value1, GCSystemMessage& gcSystemMessage, bool& bSendPacket) {
    SiegeManager::Instance().start(pGamePlayer->getCreature()->getZoneID());
}

// *command IAmAttacker
void opIAmAttacker(GamePlayer* pGamePlayer, const string& value1, GCSystemMessage& gcSystemMessage, bool& bSendPacket) {
    Creature* pCreature = pGamePlayer->getCreature();
    Assert(pCreature != NULL);

    addSimpleCreatureEffect(pCreature, Effect::EFFECT_CLASS_SIEGE_ATTACKER_1, 600);
    gcSystemMessage.setMessage("Siege: attacker 1 for one minute.");
}

// *command IAmDefender
void opIAmDefender(GamePlayer* pGamePlayer, const string& value1, GCSystemMessage& gcSystemMessage, bool& bSendPacket) {
    Creature* pCreature = pGamePlayer->getCreature();
    Assert(pCreature != NULL);

    addSimpleCreatureEffect(pCreature, Effect::EFFECT_CLASS_SIEGE_DEFENDER, 600);
    gcSystemMessage.setMessage("Siege: defender for one minute.");
}

// *command IAmReinforce
void opIAmReinforce(GamePlayer* pGamePlayer, const string& value1, GCSystemMessage& gcSystemMessage,
                    bool& bSendPacket) {
    Creature* pCreature = pGamePlayer->getCreature();
    Assert(pCreature != NULL);

    addSimpleCreatureEffect(pCreature, Effect::EFFECT_CLASS_SIEGE_REINFORCE, 600);
    gcSystemMessage.setMessage("Siege: defender reinforcement for one minute.");
}

// *command showpcstat
void opShowpcstat(GamePlayer* pGamePlayer, const string& value1, GCSystemMessage& gcSystemMessage, bool& bSendPacket) {
    Creature* pCreature = pGamePlayer->getCreature();
    Assert(pCreature != NULL);
    vector<uint> num = pCreature->getZone()->getPCManager()->getPCNumByRace();
    char buffer[200];
    snprintf(buffer, sizeof(buffer), "Slayers %u, Vampires %u, Ousters %u", num[RACE_SLAYER], num[RACE_VAMPIRE],
             num[RACE_OUSTERS]);
    gcSystemMessage.setMessage(buffer);
}

// *command advanceclass
void opAdvanceclass(GamePlayer* pGamePlayer, const string& value1, GCSystemMessage& gcSystemMessage,
                    bool& bSendPacket) {
    PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pGamePlayer->getCreature());
    Assert(pPC != NULL);

    pPC->increaseAdvancementClassExp(1);
    GCModifyInformation gcMI;
    gcMI.addShortData(MODIFY_ADVANCEMENT_CLASS_LEVEL, pPC->getAdvancementClassLevel());
    pGamePlayer->sendPacket(&gcMI);
}

// *command addDynamicZone
void opAddDynamicZone(GamePlayer* pGamePlayer, const string& value1, GCSystemMessage& gcSystemMessage,
                      bool& bSendPacket) {
    int DynamicZoneType = atoi(trim(value1).c_str());

    DynamicZoneGroup* pDynamicZoneGroup = de::gameContext().dynamicZones().getDynamicZoneGroup(DynamicZoneType);
    if (pDynamicZoneGroup == NULL) {
        gcSystemMessage.setMessage("No dynamic zone group of that type.");
        return;
    }

    DynamicZone* pDynamicZone = pDynamicZoneGroup->getAvailableDynamicZone();
    if (pDynamicZone == NULL) {
        gcSystemMessage.setMessage("No dynamic zone available.");
        return;
    }

    char zoneID[32];
    snprintf(zoneID, sizeof(zoneID), "%u - %u", pDynamicZone->getTemplateZoneID(), pDynamicZone->getZoneID());

    gcSystemMessage.setMessage(zoneID);
}

// *command enterDynamicZone
void opEnterDynamicZone(GamePlayer* pGamePlayer, const string& value1, GCSystemMessage& gcSystemMessage,
                        bool& bSendPacket) {
    ZoneID_t DynamicZoneType = atoi(trim(value1).c_str());

    PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pGamePlayer->getCreature());
    Assert(pPC != NULL);

    pPC->getGQuestManager()->enterDynamicZone(DynamicZoneType);
}

// *command clearDynamicZone
void opClearDynamicZone(GamePlayer* pGamePlayer, const string& value1, GCSystemMessage& gcSystemMessage,
                        bool& bSendPacket) {
    ZoneID_t DynamicZoneType = atoi(trim(value1).c_str());

    PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pGamePlayer->getCreature());
    Assert(pPC != NULL);

    pPC->getGQuestManager()->clearDynamicZone(DynamicZoneType);
}

// *command setTimeOutAllZoneEffect
void opSetTimeOutAllZoneEffect(GamePlayer* pGamePlayer, const string& value1, GCSystemMessage& gcSystemMessage,
                               bool& bSendPacket) {
    Creature* pCreature = pGamePlayer->getCreature();
    Assert(pCreature != NULL);
    pCreature->getZone()->getEffectManager()->setTimeOutAllEffect();
    gcSystemMessage.setMessage("Timed out every effect in the zone");
}

// *command printTile
void opPrintTile(GamePlayer* pGamePlayer, const string& value1, GCSystemMessage& gcSystemMessage, bool& bSendPacket) {
    Zone* pZone = pGamePlayer->getCreature()->getZone();

    for (int y = 0; y < pZone->getHeight(); ++y) {
        for (int x = 0; x < pZone->getWidth(); ++x) {
            if (pZone->getTile(x, y).getEffect(Effect::EFFECT_CLASS_DELETE_TILE) != NULL) {
                cout << "#";
            } else {
                cout << ".";
            }
        }
        cout << endl;
    }
}
} // namespace de::gm
