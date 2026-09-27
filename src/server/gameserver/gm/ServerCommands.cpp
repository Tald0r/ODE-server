//////////////////////////////////////////////////////////////////////////////
// Filename    : ServerCommands.cpp
// Description : GM commands that act on the server as a whole: the save and shutdown schedules,
//               the tunable variables and reloadable tables, the war switch, the world relay
//               and the reports the client sends.
//////////////////////////////////////////////////////////////////////////////

#include <stdio.h>

#include <list>

#include "CastleInfoManager.h"
#include "ClientManager.h"
#include "CombatInfoManager.h"
#include "CreatureUtil.h"
#include "DatabaseError.h"
#include "EffectShutDown.h"
#include "EventReloadInfo.h"
#include "GCNoticeEvent.h"
#include "GCSystemMessage.h"
#include "GGCommand.h"
#include "GameContext.h"
#include "GamePlayer.h"
#include "GameServerInfoManager.h"
#include "Guild.h"
#include "ItemFactoryManager.h"
#include "ItemInfoManager.h"
#include "ItemUtil.h"
#include "KernelContext.h"
#include "LoginServerManager.h"
#include "MasterLairManager.h"
#include "Monster.h"
#include "NPC.h"
#include "OptionInfo.h"
#include "PCFinder.h"
#include "PacketUtil.h"
#include "Properties.h"
#include "Relic.h"
#include "RelicUtil.h"
#include "ServerContext.h"
#include "Slayer.h"
#include "StringPool.h"
#include "VSDateTime.h"
#include "Vampire.h"
#include "VariableManager.h"
#include "WarZoneWork.h"
#include "Zone.h"
#include "ZoneGroupManager.h"
#include "ZoneInfoManager.h"
#include "ZonePlayerManager.h"
#include "ZoneUtil.h"
#include "gm/GMCommands.h"
#include "repository/SessionRepository.h"
#include "repository/ZoneInfoRepository.h"

namespace de::gm {
void opcombat(GamePlayer* pGamePlayer, string msg, int i) {
    __BEGIN_TRY

    StringPool& strings = de::gameContext().strings();
    CombatInfoManager& combatInfo = de::gameContext().combatInfo();
    ZoneGroupManager& zoneGroups = de::gameContext().zoneGroups();

    if (pGamePlayer == NULL)
        return;

    Creature* pCreature = pGamePlayer->getCreature();
    if (pCreature == NULL)
        return;

    filelog("change.txt", "[%s] %s", pCreature->getName().c_str(), msg.c_str());

    GCSystemMessage gcSystemMessage;

    // Blocked for now
    {
        gcSystemMessage.setMessage(strings.getString(STRID_DO_NOT_SUPPORT_OLD_WAR));
        pGamePlayer->sendPacket(&gcSystemMessage);

        return;
    }

    StringStream message;

    size_t j = msg.find_first_of(' ', i + 1);
    size_t z = msg.find_first_of(' ', j + 1);

    string set_type = msg.substr(j + 1, z - j - 1);

    if (set_type == "start") {
        if (combatInfo.isCombat() || combatInfo.isSlayerBonus() || combatInfo.isVampireBonus()) {
            cout << "The combat has already started" << endl;
            //			message << "it has already started";
            gcSystemMessage.setMessage(strings.getString(STRID_COMBAT_ALEADY_START));

            pGamePlayer->sendPacket(&gcSystemMessage);
        } else {
            cout << "The combat has started" << endl;
            //			message << "the combat has started";
            gcSystemMessage.setMessage(strings.getString(STRID_COMBAT_START));

            zoneGroups.broadcast(&gcSystemMessage);

            // by sigi. 2002.7.5
            for (int i = 0; i < maxRelic; i++) {
                const RelicInfo* pRelicInfo =
                    dynamic_cast<RelicInfo*>(de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_RELIC, i));

                int ZoneNum = pRelicInfo->zoneID;

                ZoneInfo* pZoneInfo = NULL;

                try {
                    pZoneInfo = de::gameContext().zoneInfos().getZoneInfo(ZoneNum);
                } catch (NoSuchElementException&) {
                    throw Error("No zone info for the relic zone.");
                }

                ZoneGroup* pZoneGroup = NULL;

                try {
                    pZoneGroup = zoneGroups.getZoneGroup(pZoneInfo->getZoneGroupID());
                } catch (NoSuchElementException&) {
                    throw Error("No zone group for the relic zone.");
                }

                Zone* pZone = pZoneGroup->getZone(ZoneNum);

                pZone->addRelicItem(i);
            }

            combatInfo.setCombat(true);
        }
    } else if (set_type == "end") {
        if (!combatInfo.isCombat() && (combatInfo.isSlayerBonus() || combatInfo.isVampireBonus()))

        {
            cout << "Ending the combat." << endl;
            gcSystemMessage.setMessage(strings.getString(STRID_COMBAT_END));
            zoneGroups.broadcast(&gcSystemMessage);

            // by sigi. 2002.7.5
            for (int i = 0; i < maxRelic; i++) {
                const RelicInfo* pRelicInfo =
                    dynamic_cast<RelicInfo*>(de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_RELIC, i));

                int ZoneNum = pRelicInfo->zoneID;

                ZoneInfo* pZoneInfo = NULL;

                try {
                    pZoneInfo = de::gameContext().zoneInfos().getZoneInfo(ZoneNum);
                } catch (NoSuchElementException&) {
                    throw Error("No zone info for the relic zone.");
                }

                ZoneGroup* pZoneGroup = NULL;

                try {
                    pZoneGroup = zoneGroups.getZoneGroup(pZoneInfo->getZoneGroupID());
                } catch (NoSuchElementException&) {
                    throw Error("No zone group for the relic zone.");
                }

                Zone* pZone = pZoneGroup->getZone(ZoneNum);

                pZone->deleteRelicItem();
                combatInfo.setRelicOwner(i, CombatInfoManager::RELIC_OWNER_NULL);
            }

            combatInfo.computeModify();
            combatInfo.setCombat(false);
        } else {
            cout << "No combat is running, or it cannot be ended." << endl;
            gcSystemMessage.setMessage(strings.getString(STRID_CANNOT_END_COMBAT));

            pGamePlayer->sendPacket(&gcSystemMessage);
        }
    }

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void opset(GamePlayer* pGamePlayer, string msg, int i) {
    __BEGIN_TRY

    StringPool& strings = de::gameContext().strings();
    VariableManager& variables = de::gameContext().variables();

    // [!!!] When this is handled through a GGCommand,
    // pGamePlayer can be NULL, so it has to be checked!

    GCSystemMessage gcSystemMessage;
    StringStream message;

    size_t j = msg.find_first_of(' ', i + 1);
    size_t z = msg.find_first_of(' ', j + 1);

    string set_type = msg.substr(j + 1, z - j - 1);
    string set_value = msg.substr(z + 1);

    if (set_value.length() < 1)
        return;

    if (pGamePlayer != NULL) {
        Creature* pCreature = pGamePlayer->getCreature();
        filelog("change.txt", "%s , %s , %s", pCreature->getName().c_str(), set_type.c_str(), set_value.c_str());
    }


    if (set_type == "star") {
        variables.setStar(atoi(set_value.c_str()));
        char msg[100];
        snprintf(msg, sizeof(msg), strings.c_str(STRID_SET_STAR_RATIO), set_value.c_str());
        //	    message << "the star rate was set to 1/" << set_value << ".";
        gcSystemMessage.setMessage(msg);
        filelog("change.txt", "[%s]%s",
                (pGamePlayer == NULL ? "Nobody" : pGamePlayer->getCreature()->getName().c_str()),
                gcSystemMessage.toString().c_str());
    } else if (set_type == "event_activate") {
        if (set_value == "start") {
            variables.setEventActivate(1);
            //	    	message << "the event has started";
            gcSystemMessage.setMessage(strings.getString(STRID_EVENT_START));
            filelog("change.txt", "[%s]%s",
                    (pGamePlayer == NULL ? "Nobody" : pGamePlayer->getCreature()->getName().c_str()),
                    gcSystemMessage.toString().c_str());
        } else {
            variables.setEventActivate(0);
            //	    	message << "the event has ended";
            gcSystemMessage.setMessage(strings.getString(STRID_EVENT_END));
            filelog("change.txt", "[%s]%s",
                    (pGamePlayer == NULL ? "Nobody" : pGamePlayer->getCreature()->getName().c_str()),
                    gcSystemMessage.toString().c_str());
        }
    } else if (set_type == "event_ratio") {
        variables.setEventRatio(atoi(set_value.c_str()));
        char msg[100];
        snprintf(msg, sizeof(msg), strings.c_str(STRID_SET_EVENT_MONSTER_RATIO), set_value.c_str());
        //	    message << "the event monster rate was set to 1/" << set_value << ".";
        gcSystemMessage.setMessage(msg);
        filelog("change.txt", "[%s]%s",
                (pGamePlayer == NULL ? "Nobody" : pGamePlayer->getCreature()->getName().c_str()),
                gcSystemMessage.toString().c_str());
    } else if (set_type == "exp_ratio") {
        int value = atoi(set_value.c_str());
        if (value < 100)
            return;

        variables.setExpRatio(value);
        char msg[100];
        snprintf(msg, sizeof(msg), strings.c_str(STRID_SET_EXP_RATIO), set_value.c_str());
        //	    message << "the experience gain rate was set to " << set_value << "%.";
        gcSystemMessage.setMessage(msg);
        filelog("change.txt", "[%s]%s",
                (pGamePlayer == NULL ? "Nobody" : pGamePlayer->getCreature()->getName().c_str()),
                gcSystemMessage.toString().c_str());
    } else if (set_type == "item_prob_ratio") {
        int value = atoi(set_value.c_str());

        variables.setItemProbRatio(value);
        char msg[100];
        snprintf(msg, sizeof(msg), strings.c_str(STRID_SET_ITEM_PROBE_RATIO), set_value.c_str());
        //	    message << "the item drop rate was set to " << set_value << "%.";
        gcSystemMessage.setMessage(msg);
        filelog("change.txt", "[%s]%s",
                (pGamePlayer == NULL ? "Nobody" : pGamePlayer->getCreature()->getName().c_str()),
                gcSystemMessage.toString().c_str());
    } else if (set_type == "combat_bonus_time") {
        int bonusTime = atoi(set_value.c_str());
        if (bonusTime < 1 || bonusTime > 14400)
            return;

        variables.setCombatBonusTime(bonusTime);
        char msg[100];
        snprintf(msg, sizeof(msg), strings.c_str(STRID_SET_COMBAT_BONUS_TIME), set_value.c_str());
        //	    message << "the combat victory bonus time was set to " << set_value << " minutes.";
        gcSystemMessage.setMessage(msg);
        filelog("change.txt", "[%s]%s",
                (pGamePlayer == NULL ? "Nobody" : pGamePlayer->getCreature()->getName().c_str()),
                gcSystemMessage.toString().c_str());
    } else if (set_type == "combat_bonus_slayer_hp_ratio") {
        int bonus = atoi(set_value.c_str());
        if (bonus < 0 || bonus > 100)
            return;

        variables.setCombatSlayerHPBonusRatio(bonus);
        char msg[100];
        snprintf(msg, sizeof(msg), strings.c_str(STRID_SET_COMBAT_SLAYER_BONUS_HP), set_value.c_str());
        //	    message << "the war HP bonus was set to +" << set_value << "%.";
        gcSystemMessage.setMessage(msg);
        filelog("change.txt", "[%s]%s",
                (pGamePlayer == NULL ? "Nobody" : pGamePlayer->getCreature()->getName().c_str()),
                gcSystemMessage.toString().c_str());
    } else if (set_type == "combat_bonus_vampire_hp_ratio") {
        int bonus = atoi(set_value.c_str());
        if (bonus < 0 || bonus > 100)
            return;

        variables.setCombatVampireHPBonusRatio(bonus);
        char msg[100];
        snprintf(msg, sizeof(msg), strings.c_str(STRID_SET_COMBAT_VAMPIRE_BONUS_HP), set_value.c_str());
        //	    message << "the war HP bonus was set to +" << set_value << "%.";
        gcSystemMessage.setMessage(msg);
        filelog("change.txt", "[%s]%s",
                (pGamePlayer == NULL ? "Nobody" : pGamePlayer->getCreature()->getName().c_str()),
                gcSystemMessage.toString().c_str());
    } else if (set_type == "combat_bonus_slayer_damage") {
        int bonus = atoi(set_value.c_str());
        if (bonus < 0 || bonus > 20)
            return;

        variables.setCombatSlayerDamageBonus(bonus);
        char msg[100];
        snprintf(msg, sizeof(msg), strings.c_str(STRID_SET_COMBAT_SLAYER_BONUS_DAMAGE), set_value.c_str());
        //	    message << "the war damage bonus was set to +" << set_value << ".";
        gcSystemMessage.setMessage(msg);
        filelog("change.txt", "[%s]%s",
                (pGamePlayer == NULL ? "Nobody" : pGamePlayer->getCreature()->getName().c_str()),
                gcSystemMessage.toString().c_str());
    } else if (set_type == "combat_bonus_vampire_damage") {
        int bonus = atoi(set_value.c_str());
        if (bonus < 0 || bonus > 20)
            return;

        variables.setCombatVampireDamageBonus(bonus);
        char msg[100];
        snprintf(msg, sizeof(msg), strings.c_str(STRID_SET_COMBAT_VAMPIRE_BONUS_DAMAGE), set_value.c_str());
        //	    message << "the war damage bonus was set to +" << set_value << ".";
        gcSystemMessage.setMessage(msg);
        filelog("change.txt", "[%s]%s",
                (pGamePlayer == NULL ? "Nobody" : pGamePlayer->getCreature()->getName().c_str()),
                gcSystemMessage.toString().c_str());
    } else if (set_type == "premium_exp_bonus_percent") {
        int bonus = atoi(set_value.c_str());
        if (bonus < 100 || bonus > 1000)
            return;

        variables.setPremiumExpBonusPercent(bonus);
        char msg[100];
        snprintf(msg, sizeof(msg), strings.c_str(STRID_SET_PREMIUM_EXP_RATIO), set_value.c_str());
        //	    message << "the premium experience bonus was set to " << set_value << "%.";
        gcSystemMessage.setMessage(msg);
        filelog("change.txt", "[%s]%s",
                (pGamePlayer == NULL ? "Nobody" : pGamePlayer->getCreature()->getName().c_str()),
                gcSystemMessage.toString().c_str());
    } else if (set_type == "premium_item_probe_percent") {
        int bonus = atoi(set_value.c_str());
        if (bonus < 100 || bonus > 1000)
            return;

        variables.setPremiumItemProbePercent(bonus);
        char msg[100];
        snprintf(msg, sizeof(msg), strings.c_str(STRID_SET_PREMIUM_ITEM_PROBE_RATIO), set_value.c_str());
        //	    message << "the premium item drop rate was set to " << set_value << "%.";
        gcSystemMessage.setMessage(msg);
        filelog("change.txt", "[%s]%s",
                (pGamePlayer == NULL ? "Nobody" : pGamePlayer->getCreature()->getName().c_str()),
                gcSystemMessage.toString().c_str());
    } else if (set_type == "zone_group_balancing_minute") {
        int minute = atoi(set_value.c_str());
        if (minute <= 0 || minute > 30 * 24 * 60)
            return;

        variables.setZoneGroupBalancingMinute(minute);
        de::gameContext().clients().setBalanceZoneGroup(minute);
        char msg[100];
        snprintf(msg, sizeof(msg), strings.c_str(STRID_SET_ZONE_GROUP_BALANCING_TIME), set_value.c_str());
        //	    message << "the ZoneGroupBalancing period was set to " << set_value << " minutes.";
        gcSystemMessage.setMessage(msg);
        filelog("change.txt", "[%s]%s",
                (pGamePlayer == NULL ? "Nobody" : pGamePlayer->getCreature()->getName().c_str()),
                gcSystemMessage.toString().c_str());
    } else if (set_type == "gamble_item_type_ratio") {
        int ratio = atoi(set_value.c_str());
        if (ratio <= 10 || ratio > 1000)
            return;

        variables.setGambleItemTypeRatio(ratio);
        char msg[100];
        snprintf(msg, sizeof(msg), strings.c_str(STRID_SET_GAMBLE_ITEM_TYPE_RATIO), set_value.c_str());
        //	    message << "the gamble ItemType rate was set to " << set_value << "%.";
        gcSystemMessage.setMessage(msg);
        filelog("change.txt", "[%s]%s",
                (pGamePlayer == NULL ? "Nobody" : pGamePlayer->getCreature()->getName().c_str()),
                gcSystemMessage.toString().c_str());
    } else if (set_type == "gamble_item_option_ratio") {
        int ratio = atoi(set_value.c_str());
        if (ratio <= 10 || ratio > 1000)
            return;

        variables.setGambleItemOptionRatio(ratio);
        char msg[100];
        snprintf(msg, sizeof(msg), strings.c_str(STRID_SET_GAMBLE_ITEM_OPTION_RATIO), set_value.c_str());
        //	    message << "the gamble ItemOption rate was set to " << set_value << "%.";
        gcSystemMessage.setMessage(msg);
        filelog("change.txt", "[%s]%s",
                (pGamePlayer == NULL ? "Nobody" : pGamePlayer->getCreature()->getName().c_str()),
                gcSystemMessage.toString().c_str());
    } else if (set_type == "summon_motorcycle") {
        if (set_value == "on") {
            variables.setSummonMotorcycle(true);
            char msg[100];
            snprintf(msg, sizeof(msg), strings.c_str(STRID_SET_SUMMON_MOTORCYCLE), "ON");
            //			message << "motorcycle summoning is now ON.";
            gcSystemMessage.setMessage(msg);
            filelog("change.txt", "[%s]%s",
                    (pGamePlayer == NULL ? "Nobody" : pGamePlayer->getCreature()->getName().c_str()),
                    gcSystemMessage.toString().c_str());
        } else if (set_value == "off") {
            variables.setSummonMotorcycle(false);
            char msg[100];
            snprintf(msg, sizeof(msg), strings.c_str(STRID_SET_SUMMON_MOTORCYCLE), "OFF");
            //			message << "motorcycle summoning is now OFF.";
            gcSystemMessage.setMessage(msg);
            filelog("change.txt", "[%s]%s",
                    (pGamePlayer == NULL ? "Nobody" : pGamePlayer->getCreature()->getName().c_str()),
                    gcSystemMessage.toString().c_str());
        }
    } else if (set_type == "enemy_limit_time") {
        int enemy_limit_time = atoi(set_value.c_str());
        if (enemy_limit_time <= 180 || enemy_limit_time >= 3600)
            return;

        variables.setEnemyLimitTime(enemy_limit_time);
        char msg[100];
        snprintf(msg, sizeof(msg), strings.c_str(STRID_SET_MONSTER_FORGET_TIME), set_value.c_str());
        //	    message << "the monster forget time was set to " << set_value << " seconds.";
        gcSystemMessage.setMessage(msg);
        filelog("change.txt", "[%s]%s",
                (pGamePlayer == NULL ? "Nobody" : pGamePlayer->getCreature()->getName().c_str()),
                gcSystemMessage.toString().c_str());
    } else if (set_type == "rare_item_ratio") {
        int ratio = atoi(set_value.c_str());
        if (ratio < 0 || ratio > 100000)
            return;

        variables.setRareItemRatio(ratio);
        char msg[100];
        snprintf(msg, sizeof(msg), strings.c_str(STRID_SET_RARE_ITEM_RATIO), set_value.c_str());
        //		message << "the rare item drop rate was set to " << set_value << "%.";
        gcSystemMessage.setMessage(msg);
        filelog("change.txt", "[%s]%s",
                (pGamePlayer == NULL ? "Nobody" : pGamePlayer->getCreature()->getName().c_str()),
                gcSystemMessage.toString().c_str());
    } else if (set_type == "unique_item_ratio") {
        int ratio = atoi(set_value.c_str());
        if (ratio < 0 || ratio > 10000)
            return;

        variables.setUniqueItemRatio(ratio);
        char msg[100];
        snprintf(msg, sizeof(msg), strings.c_str(STRID_SET_UNIQUE_ITEM_RATIO), set_value.c_str());
        //		message << "the unique item drop rate was set to " << set_value << "/10000.";
        gcSystemMessage.setMessage(msg);
        filelog("change.txt", "[%s]%s",
                (pGamePlayer == NULL ? "Nobody" : pGamePlayer->getCreature()->getName().c_str()),
                gcSystemMessage.toString().c_str());
    } else if (set_type == "active_master_lair") {
        MasterLairManager* pMasterLairManager = NULL;

        if (pGamePlayer != NULL) {
            Creature* pCreature = pGamePlayer->getCreature();
            Zone* pZone = pCreature->getZone();
            pMasterLairManager = pZone->getMasterLairManager();
        }

        if (set_value == "on") {
            variables.setActiveMasterLair(true);
            char msg[100];
            snprintf(msg, sizeof(msg), strings.c_str(STRID_SET_MASTER_LAIR_ACTIVATE), "ON");
            //			message << "the master lair event is now ON.";
            gcSystemMessage.setMessage(msg);
            filelog("change.txt", "[%s]%s",
                    (pGamePlayer == NULL ? "Nobody" : pGamePlayer->getCreature()->getName().c_str()),
                    gcSystemMessage.toString().c_str());

            if (pMasterLairManager != NULL) {
                pMasterLairManager->startEvent();
            }
        } else if (set_value == "off") {
            variables.setActiveMasterLair(false);
            char msg[100];
            snprintf(msg, sizeof(msg), strings.c_str(STRID_SET_MASTER_LAIR_ACTIVATE), "OFF");
            //			message << "the master lair event is now OFF.";
            gcSystemMessage.setMessage(msg);
            filelog("change.txt", "[%s]%s",
                    (pGamePlayer == NULL ? "Nobody" : pGamePlayer->getCreature()->getName().c_str()),
                    gcSystemMessage.toString().c_str());

            if (pMasterLairManager != NULL) {
                pMasterLairManager->stopEvent();
            }
        }
    } else if (set_type == "retry_master_lair") {
        if (set_value == "on") {
            variables.setRetryMasterLair(true);
            char msg[100];
            snprintf(msg, sizeof(msg), strings.c_str(STRID_SET_RETRY_MASTER_LAIR), "ON");
            //			message << "retrying the master lair after dying in it is now ON
            // .";
            gcSystemMessage.setMessage(msg);
            filelog("change.txt", "[%s]%s",
                    (pGamePlayer == NULL ? "Nobody" : pGamePlayer->getCreature()->getName().c_str()),
                    gcSystemMessage.toString().c_str());
        } else if (set_value == "off") {
            variables.setRetryMasterLair(false);
            char msg[100];
            snprintf(msg, sizeof(msg), strings.c_str(STRID_SET_RETRY_MASTER_LAIR), "OFF");
            //			message << "retrying the master lair after dying in it is now OFF
            // .";
            gcSystemMessage.setMessage(msg);
            filelog("change.txt", "[%s]%s",
                    (pGamePlayer == NULL ? "Nobody" : pGamePlayer->getCreature()->getName().c_str()),
                    gcSystemMessage.toString().c_str());
        }
    }

    else if (set_type == "harvest_festival_item_ratio") {
        int ratio = atoi(set_value.c_str());
        if (ratio < 0 || ratio > 10000)
            return;

        variables.setHarvestFestivalItemRatio(ratio);
        char msg[100];
        snprintf(msg, sizeof(msg), strings.c_str(STRID_SET_HARVEST_FESTIVAL_ITEM_RATIO), set_value.c_str());
        //		message << "the harvest festival item drop rate was set to 1/" << ratio << "
        // .";
        gcSystemMessage.setMessage(msg);
        filelog("change.txt", "[%s]%s",
                (pGamePlayer == NULL ? "Nobody" : pGamePlayer->getCreature()->getName().c_str()),
                gcSystemMessage.toString().c_str());
    } else if (set_type == "master_blood_drain_start_hp") {
        int percent = atoi(set_value.c_str());
        if (percent < 0 || percent > 100)
            return;

        variables.setMasterBloodDrainStartHP(percent);
        char msg[100];
        snprintf(msg, sizeof(msg), strings.c_str(STRID_SET_MASTER_BLOOD_DRAIN_START_HP), set_value.c_str());
        //		message << "the master's blood drain start HP is now " << percent << "%.";
        gcSystemMessage.setMessage(msg);
        filelog("change.txt", "[%s]%s",
                (pGamePlayer == NULL ? "Nobody" : pGamePlayer->getCreature()->getName().c_str()),
                gcSystemMessage.toString().c_str());
    } else if (set_type == "master_blood_drain_start_bd") {
        int percent = atoi(set_value.c_str());
        if (percent < 0 || percent > 100)
            return;

        variables.setMasterBloodDrainStartBD(percent);
        char msg[100];
        snprintf(msg, sizeof(msg), strings.c_str(STRID_SET_MASTER_BLOOD_DRAIN_START_BD), set_value.c_str());
        //		message << "the master's blood drain start rate is now " << percent << "%.";
        gcSystemMessage.setMessage(msg);
        filelog("change.txt", "[%s]%s",
                (pGamePlayer == NULL ? "Nobody" : pGamePlayer->getCreature()->getName().c_str()),
                gcSystemMessage.toString().c_str());
    } else if (set_type == "master_blood_drain_end_hp") {
        int percent = atoi(set_value.c_str());
        if (percent < 0 || percent > 100)
            return;

        variables.setMasterBloodDrainEndHP(percent);
        char msg[100];
        snprintf(msg, sizeof(msg), strings.c_str(STRID_SET_MASTER_BLOOD_DRAIN_END_HP), set_value.c_str());
        //		message << "the master's blood drain end HP is now " << percent << "%.";
        gcSystemMessage.setMessage(msg);
        filelog("change.txt", "[%s]%s",
                (pGamePlayer == NULL ? "Nobody" : pGamePlayer->getCreature()->getName().c_str()),
                gcSystemMessage.toString().c_str());
    } else if (set_type == "master_blood_drain_end_bd") {
        int percent = atoi(set_value.c_str());
        if (percent < 0 || percent > 100)
            return;

        variables.setMasterBloodDrainEndBD(percent);
        char msg[100];
        snprintf(msg, sizeof(msg), strings.c_str(STRID_SET_MASTER_BLOOD_DRAIN_END_BD), set_value.c_str());
        //		message << "the master's blood drain end rate is now " << percent << "%.";
        gcSystemMessage.setMessage(msg);
        filelog("change.txt", "[%s]%s",
                (pGamePlayer == NULL ? "Nobody" : pGamePlayer->getCreature()->getName().c_str()),
                gcSystemMessage.toString().c_str());
    } else if (set_type == "chief_monster") {
        if (set_value == "on") {
            variables.setActiveChiefMonster(true);
            char msg[100];
            snprintf(msg, sizeof(msg), strings.c_str(STRID_SET_CHIEF_MONSTER), "ON");
            //			message << "the chief monster feature is now ON.";
            gcSystemMessage.setMessage(msg);
            filelog("change.txt", "[%s]%s",
                    (pGamePlayer == NULL ? "Nobody" : pGamePlayer->getCreature()->getName().c_str()),
                    gcSystemMessage.toString().c_str());
        } else if (set_value == "off") {
            variables.setActiveChiefMonster(false);
            char msg[100];
            snprintf(msg, sizeof(msg), strings.c_str(STRID_SET_CHIEF_MONSTER), "OFF");
            //			message << "the chief monster feature is now OFF.";
            gcSystemMessage.setMessage(msg);
            filelog("change.txt", "[%s]%s",
                    (pGamePlayer == NULL ? "Nobody" : pGamePlayer->getCreature()->getName().c_str()),
                    gcSystemMessage.toString().c_str());
        }
    } else if (set_type == "chief_monster_rare_item_percent") {
        int ratio = atoi(set_value.c_str());
        if (ratio < 0 || ratio > 100)
            return;

        variables.setChiefMonsterRareItemPercent(ratio);
        char msg[100];
        snprintf(msg, sizeof(msg), strings.c_str(STRID_SET_CHIEF_MONSTER_RARE_ITEM_RATIO), set_value.c_str());
        //		message << "the chief monster rare item drop rate was set to " << set_value << "%.";
        gcSystemMessage.setMessage(msg);
        filelog("change.txt", "[%s]%s",
                (pGamePlayer == NULL ? "Nobody" : pGamePlayer->getCreature()->getName().c_str()),
                gcSystemMessage.toString().c_str());
    } else if (set_type == "newbie_transport_to_guild") {
        if (set_value == "on") {
            variables.setNewbieTransportToGuild(true);
            char msg[100];
            snprintf(msg, sizeof(msg), strings.c_str(STRID_SET_NEWBIE_TRANSPORT_TO_GUILD), "ON");
            //			message << "moving a level 40 character to its guild is now ON.";
            gcSystemMessage.setMessage(msg);
            filelog("change.txt", "[%s]%s",
                    (pGamePlayer == NULL ? "Nobody" : pGamePlayer->getCreature()->getName().c_str()),
                    gcSystemMessage.toString().c_str());
        } else if (set_value == "off") {
            variables.setNewbieTransportToGuild(false);
            char msg[100];
            snprintf(msg, sizeof(msg), strings.c_str(STRID_SET_NEWBIE_TRANSPORT_TO_GUILD), "OFF");
            //			message << "moving a level 40 character to its guild is now OFF.";
            gcSystemMessage.setMessage(msg);
            filelog("change.txt", "[%s]%s",
                    (pGamePlayer == NULL ? "Nobody" : pGamePlayer->getCreature()->getName().c_str()),
                    gcSystemMessage.toString().c_str());
        }
    } else if (set_type == "xmas_2002") {
        size_t l = set_value.find_first_of(' ', 0);
        size_t m = set_value.find_first_of(' ', l + 1);

        string value_firecracker = set_value.substr(0, l);
        string value_treepart = set_value.substr(l + 1, m - l - 1);
        string value_giftbox = set_value.substr(m + 1);

        variables.setVariable(CHRISTMAS_FIRE_CRACKER_RATIO, value_firecracker);
        const string& msg1 = variables.toString(CHRISTMAS_FIRE_CRACKER_RATIO);
        filelog("change.txt", "[%s]%s",
                (pGamePlayer == NULL ? "Nobody" : pGamePlayer->getCreature()->getName().c_str()),
                gcSystemMessage.toString().c_str());
        gcSystemMessage.setMessage(msg1);

        if (pGamePlayer != NULL)
            pGamePlayer->sendPacket(&gcSystemMessage);

        variables.setVariable(CHRISTMAS_TREE_PART_RATIO, value_treepart);
        const string& msg2 = variables.toString(CHRISTMAS_TREE_PART_RATIO);
        filelog("change.txt", "[%s]%s",
                (pGamePlayer == NULL ? "Nobody" : pGamePlayer->getCreature()->getName().c_str()),
                gcSystemMessage.toString().c_str());
        gcSystemMessage.setMessage(msg2);

        if (pGamePlayer != NULL)
            pGamePlayer->sendPacket(&gcSystemMessage);

        variables.setVariable(CHRISTMAS_GIFT_BOX_RATIO, value_giftbox);
        const string& msg3 = variables.toString(CHRISTMAS_GIFT_BOX_RATIO);
        filelog("change.txt", "[%s]%s",
                (pGamePlayer == NULL ? "Nobody" : pGamePlayer->getCreature()->getName().c_str()),
                gcSystemMessage.toString().c_str());

        gcSystemMessage.setMessage(msg3);
    } else if (set_type == "ItemTaxRatio") {
        if (pGamePlayer != NULL) {
            Creature* pCreature = pGamePlayer->getCreature();
            Zone* pZone = pCreature->getZone();
            if (set_value.size() != 0) {
                int Tax = atoi(set_value.c_str());
                de::gameContext().castleInfos().setItemTaxRatio(pZone, Tax);
            } else {
                gcSystemMessage.setMessage(strings.getString(STRID_WRONG_ITEM_TAX_RATIO));
            }
        }
    }

    else {
        // by sigi. 2002.11.19
        VariableType vt = variables.getVariableType(set_type);

        if (vt != VARIABLE_MAX) {
            variables.setVariable(vt, set_value);
            const string& msg = variables.toString(vt);
            gcSystemMessage.setMessage(msg);
            filelog("change.txt", "[%s]%s",
                    (pGamePlayer == NULL ? "Nobody" : pGamePlayer->getCreature()->getName().c_str()),
                    gcSystemMessage.toString().c_str());

            if (vt == PREMIUM_HALF_EVENT) {
                GCNoticeEvent gcNoticeEvent;

                if (variables.getVariable(vt)) {
                    gcNoticeEvent.setCode(NOTICE_EVENT_PREMIUM_HALF_START);
                } else {
                    gcNoticeEvent.setCode(NOTICE_EVENT_PREMIUM_HALF_END);
                }

                // Tell the three zones the premium half-price event runs in. The
                // command runs on the GM's zone thread or on the login server
                // link's, so each zone sends it from its own thread.
                de::war::postBroadcast({61, 64, 1007}, gcNoticeEvent);
            } else if (vt == TODAY_IS_HOLYDAY) {
                GCNoticeEvent gcNoticeEvent;
                gcNoticeEvent.setCode(NOTICE_EVENT_HOLYDAY);
                gcNoticeEvent.setParameter(variables.getVariable(vt));

                de::gameContext().zoneGroups().broadcast(&gcNoticeEvent);
            } else if (vt == CROWN_PRICE) {
                GCNoticeEvent gcNoticeEvent;
                gcNoticeEvent.setCode(NOTICE_EVENT_CROWN_PRICE);
                gcNoticeEvent.setParameter(variables.getVariable(vt));

                de::gameContext().zoneGroups().broadcast(&gcNoticeEvent);
            }
        } else {
            gcSystemMessage.setMessage(strings.getString(STRID_WRONG_VARIABLE_NAME));
        }
    }

    // Send the system message only to oneself
    if (pGamePlayer != NULL) {
        pGamePlayer->sendPacket(&gcSystemMessage);
    }

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void opview(GamePlayer* pGamePlayer, string msg, int i) {
    __BEGIN_TRY

    if (pGamePlayer == NULL)
        return;

    VariableManager& variables = de::gameContext().variables();

    Creature* pCreature = pGamePlayer->getCreature();
    GCSystemMessage gcSystemMessage;
    StringStream message;

    size_t j = msg.find_first_of(' ', i + 1);
    size_t z = msg.find_first_of(' ', j + 1);

    string set_type = msg.substr(j + 1, z - j - 1);

    cout << "modifier : " << pCreature->getName() << " set type : " << set_type << endl;

    if (set_type == "star") {
        message << "star : 1/" << variables.getStar();
        gcSystemMessage.setMessage(message.toString());
    } else if (set_type == "evnet_activate") {
        if (variables.getEventActivate() == 1) {
            // message << ((const string &) ("the event is running...")) << endl;
        } else {
            // message << "the event is stopped" << endl;
        }
        gcSystemMessage.setMessage(message.toString());
    } else if (set_type == "event_ratio") {
        message << "Event item drop rate 1/" << variables.getEventRatio();
        gcSystemMessage.setMessage(message.toString());
    } else if (set_type == "exp_ratio") {
        message << "Exp ratio: " << variables.getExpRatio();
        gcSystemMessage.setMessage(message.toString());
    } else if (set_type == "item_prob_ratio") {
        message << "Item drop ratio: " << variables.getItemProbRatio();
        gcSystemMessage.setMessage(message.toString());
    } else if (set_type == "combat_bonus_time") {
        message << "Combat victory bonus time: " << variables.getCombatBonusTime() << " min";
        gcSystemMessage.setMessage(message.toString());
    } else if (set_type == "combat_bonus_slayer_hp_ratio") {
        message << "Slayer combat HP bonus ratio: +" << variables.getCombatSlayerHPBonusRatio() << "%";
        gcSystemMessage.setMessage(message.toString());
    } else if (set_type == "combat_bonus_vampire_hp_ratio") {
        message << "Vampire combat HP bonus ratio: +" << variables.getCombatVampireHPBonusRatio() << "%";
        gcSystemMessage.setMessage(message.toString());
    } else if (set_type == "combat_bonus_slayer_damage") {
        message << "Slayer combat damage bonus: +" << variables.getCombatSlayerDamageBonus();
        gcSystemMessage.setMessage(message.toString());
    } else if (set_type == "combat_bonus_vampire_damage") {
        message << "Vampire combat damage bonus: +" << variables.getCombatVampireDamageBonus();
        gcSystemMessage.setMessage(message.toString());
    } else if (set_type == "premium_exp_bonus_percent") {
        message << "Premium user exp bonus: " << variables.getPremiumExpBonusPercent() << "%";
        gcSystemMessage.setMessage(message.toString());
    } else if (set_type == "premium_item_probe_percent") {
        message << "Premium zone item drop bonus: " << variables.getPremiumItemProbePercent() << "%";
        gcSystemMessage.setMessage(message.toString());
    } else if (set_type == "zone_group_balancing_minute") {
        message << "ZoneGroupBalancing interval: " << variables.getZoneGroupBalancingMinute() << " min";
        gcSystemMessage.setMessage(message.toString());
    } else if (set_type == "zone_group_next_balancing_time") {
        const Timeval& tv = de::gameContext().clients().getBalanceZoneGroupTime();
        Timeval currentTime;
        getCurrentTime(currentTime);
        message << "Next ZoneGroupBalancing: in " << (tv.tv_sec - currentTime.tv_sec) / 60 << " min";
        gcSystemMessage.setMessage(message.toString());
    } else if (set_type == "gamble_item_type_ratio") {
        message << "Gamble ItemType ratio: " << variables.getGambleItemTypeRatio() << "%";
        gcSystemMessage.setMessage(message.toString());
    } else if (set_type == "gamble_item_option_ratio") {
        message << "Gamble ItemOption ratio: " << variables.getGambleItemOptionRatio() << "%";
        gcSystemMessage.setMessage(message.toString());
    } else if (set_type == "summon_motorcycle") {
        message << "Motorcycle summoning : " << (variables.isSummonMotorcycle() ? "ON" : "OFF");
        gcSystemMessage.setMessage(message.toString());
    } else if (set_type == "enemy_limit_time") {
        message << "Enemy limit time: " << variables.getEnemyLimitTime() << " s";
        gcSystemMessage.setMessage(message.toString());
    } else if (set_type == "rare_item_ratio") {
        message << "Rare item ratio: " << variables.getRareItemRatio() << "%";
        gcSystemMessage.setMessage(message.toString());
    } else if (set_type == "unique_item_ratio") {
        message << "Unique item ratio: " << variables.getUniqueItemRatio() << "/10000";
        gcSystemMessage.setMessage(message.toString());
    } else if (set_type == "active_master_lair") {
        message << "Master lair : " << (variables.isActiveMasterLair() ? "ON" : "OFF");
        gcSystemMessage.setMessage(message.toString());
    } else if (set_type == "retry_master_lair") {
        message << "Master lair re-entry after death : " << (variables.isRetryMasterLair() ? "ON" : "OFF");
        gcSystemMessage.setMessage(message.toString());
    } else if (set_type == "harvest_festival_item_ratio") {
        message << "Harvest festival item drop rate: 1/" << variables.getHarvestFestivalItemRatio();
        gcSystemMessage.setMessage(message.toString());
    } else if (set_type == "master_blood_drain_start_hp") {
        message << "Master blood drain start HP: " << variables.getMasterBloodDrainStartHP() << "%";
        gcSystemMessage.setMessage(message.toString());
    } else if (set_type == "master_blood_drain_start_bd") {
        message << "Master blood drain chance at start: " << variables.getMasterBloodDrainStartBD() << "%";
        gcSystemMessage.setMessage(message.toString());
    } else if (set_type == "master_blood_drain_end_hp") {
        message << "Master blood drain end(?) HP: " << variables.getMasterBloodDrainEndHP() << "%";
        gcSystemMessage.setMessage(message.toString());
    } else if (set_type == "master_blood_drain_end_bd") {
        message << "Master blood drain chance at end(?): " << variables.getMasterBloodDrainEndBD() << "%";
        gcSystemMessage.setMessage(message.toString());
    } else if (set_type == "chief_monster") {
        message << "Chief monster spawning : " << (variables.isActiveChiefMonster() ? "ON" : "OFF");
        gcSystemMessage.setMessage(message.toString());
    } else if (set_type == "chief_monster_rare_item_percent") {
        message << "Chief monster rare item ratio: " << variables.getChiefMonsterRareItemPercent() << "%";
        gcSystemMessage.setMessage(message.toString());
    } else if (set_type == "newbie_transport_to_guild") {
        message << "Transport newbies to the guild at attribute sum 40:"
                << (variables.isNewbieTransportToGuild() ? "ON" : "OFF");
        gcSystemMessage.setMessage(message.toString());
    } else {
        // by sigi. 2002.11.19
        VariableType vt = variables.getVariableType(set_type);

        if (vt != VARIABLE_MAX) {
            const string& msg = variables.toString(vt);
            gcSystemMessage.setMessage(msg);
        } else {
            gcSystemMessage.setMessage(de::gameContext().strings().getString(STRID_WRONG_VARIABLE_NAME));
        }
    }

    // Send the system message only to oneself
    pGamePlayer->sendPacket(&gcSystemMessage);

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void opload(GamePlayer* pGamePlayer, string msg, int i) {
    __BEGIN_TRY

    StringPool& strings = de::gameContext().strings();

    GCSystemMessage gcSystemMessage;
    StringStream message;

    size_t j = msg.find_first_of(' ', i + 1);
    size_t z = msg.find_first_of(' ', j + 1);

    string load_type = msg.substr(j + 1, z - j - 1);
    string load_value = "0";

    if (z != string::npos)
        load_value = msg.substr(z + 1);

    if (load_type.length() < 1)
        return;

    if (pGamePlayer != NULL) {
        Creature* pCreature = pGamePlayer->getCreature();
        filelog("change.txt", "[LOAD] %s , %s , %s", pCreature->getName().c_str(), load_type.c_str(),
                load_value.c_str());
    }

    EventReloadInfo* pEvent = NULL;

    if (load_type == "master_lair_info") {
        pEvent = new EventReloadInfo(pGamePlayer, EventReloadInfo::MASTER_LAIR_INFO);
        gcSystemMessage.setMessage(strings.getString(STRID_LOAD_MASTER_LAIR_INFO));
        filelog("change.txt", "[%s]%s",
                (pGamePlayer == NULL ? "Nobody" : pGamePlayer->getCreature()->getName().c_str()),
                gcSystemMessage.toString().c_str());
    } else if (load_type == "monster_info") {
        string MonsterName = "all";
        int SpriteType = 0;


        bool bExist = true;

        //		StringStream msg;
        char msg[100];

        if (bExist || MonsterName == "all") {
            // For all, the SpriteType is 0.
            pEvent = new EventReloadInfo(pGamePlayer, EventReloadInfo::MONSTER_INFO, SpriteType);

            if (SpriteType != 0) {
                snprintf(msg, sizeof(msg), strings.c_str(STRID_LOAD_MONSTER_INFO), load_value.c_str());
            } else {
                snprintf(msg, sizeof(msg), strings.c_str(STRID_LOAD_ALL_MONSTER_INFO));
            }
        } else {
            snprintf(msg, sizeof(msg), strings.c_str(STRID_LOAD_WRONG_MONSTER_INFO), load_value.c_str());
        }

        gcSystemMessage.setMessage(msg);
        filelog("change.txt", "[%s]%s",
                (pGamePlayer == NULL ? "Nobody" : pGamePlayer->getCreature()->getName().c_str()),
                gcSystemMessage.toString().c_str());
    } else if (load_type == "monster_ai") {
        pEvent = new EventReloadInfo(pGamePlayer, EventReloadInfo::MONSTER_AI);
        gcSystemMessage.setMessage(strings.getString(STRID_LOAD_DIRECTIVESET));
        filelog("change.txt", "[%s]%s",
                (pGamePlayer == NULL ? "Nobody" : pGamePlayer->getCreature()->getName().c_str()),
                gcSystemMessage.toString().c_str());
    } else if (load_type == "zone_info") {
        pEvent = new EventReloadInfo(pGamePlayer, EventReloadInfo::ZONE_INFO);
        gcSystemMessage.setMessage(strings.getString(STRID_LOAD_ZONE));
        filelog("change.txt", "[%s]%s",
                (pGamePlayer == NULL ? "Nobody" : pGamePlayer->getCreature()->getName().c_str()),
                gcSystemMessage.toString().c_str());
    } else if (load_type == "zone") {
        int zoneID = atoi(load_value.c_str());

        pEvent = new EventReloadInfo(pGamePlayer, EventReloadInfo::ZONE, zoneID);

        gcSystemMessage.setMessage(strings.getString(STRID_LOAD_ZONE));
        filelog("change.txt", "[%s]%s",
                (pGamePlayer == NULL ? "Nobody" : pGamePlayer->getCreature()->getName().c_str()),
                gcSystemMessage.toString().c_str());
    } else if (load_type == "log_user") {
        pEvent = new EventReloadInfo(pGamePlayer, EventReloadInfo::LOG_USER_INFO);
        gcSystemMessage.setMessage(strings.getString(STRID_LOAD_LOG_USER));
        filelog("change.txt", "[%s]%s",
                (pGamePlayer == NULL ? "Nobody" : pGamePlayer->getCreature()->getName().c_str()),
                gcSystemMessage.toString().c_str());
    } else if (load_type == "item_info") {
        Item::ItemClass ItemClass = de::gameContext().itemFactories().getItemClassByName(load_value);

        // ItemClass being MAX means it was not found by that name.
        // In that case it has to be checked whether the item class was given directly as a number.
        if (ItemClass == Item::ITEM_CLASS_MAX) {
            int temp = atoi(load_value.c_str());
            if (temp < 0 || temp >= Item::ITEM_CLASS_MAX) {
                return;
            } else {
                ItemClass = (Item::ItemClass)(temp);
            }
        }

        pEvent = new EventReloadInfo(pGamePlayer, EventReloadInfo::ITEM_INFO, ItemClass);


        char msg[100];
        snprintf(msg, sizeof(msg), strings.c_str(STRID_LOAD_ITEM_INFO), load_value.c_str());
        gcSystemMessage.setMessage(msg);
    } else if (load_type == "option_info") {
        pEvent = new EventReloadInfo(pGamePlayer, EventReloadInfo::OPTION_INFO);
        gcSystemMessage.setMessage(strings.getString(STRID_LOAD_OPTION_INFO));
    } else if (load_type == "rank_bonus_info") {
        pEvent = new EventReloadInfo(pGamePlayer, EventReloadInfo::RANK_BONUS_INFO);
        gcSystemMessage.setMessage(strings.getString(STRID_LOAD_RANK_BONUS_INFO));
    } else if (load_type == "string_pool") {
        pEvent = new EventReloadInfo(pGamePlayer, EventReloadInfo::STRING_POOL);
        gcSystemMessage.setMessage("reload StringPool");
    } else if (load_type == "war_schedule_info") {
        ZoneID_t zoneID = atoi(load_value.c_str());

        // When no zone is given, take the zone the Creature is in
        if (zoneID == 0) {
            if (pGamePlayer != NULL) {
                Creature* pCreature = pGamePlayer->getCreature();
                Assert(pCreature != NULL);

                Zone* pZone = pCreature->getZone();
                Assert(pZone != NULL);

                zoneID = pZone->getZoneID();
            }
        }

        pEvent = new EventReloadInfo(pGamePlayer, EventReloadInfo::WAR_SCHEDULE_INFO, zoneID);

        StringStream msg;
        msg << strings.getString(STRID_LOAD_WAR_SCHEDULE_INFO);

        if (!de::gameContext().variables().isWarActive()) {
            msg << strings.getString(STRID_WAR_OFF);
        }

        gcSystemMessage.setMessage(msg.toString().c_str());
    } else if (load_type == "blood_bible_owner") {
        pEvent = new EventReloadInfo(pGamePlayer, EventReloadInfo::BLOOD_BIBLE_OWNER);

        gcSystemMessage.setMessage(strings.getString(STRID_LOAD_BLOOD_BIBLE_OWNER_INFO));
    } else if (load_type == "sweeper_owner") {
        int level = atoi(load_value.c_str());
        pEvent = new EventReloadInfo(pGamePlayer, EventReloadInfo::SWEEPER_OWNER, level);

        gcSystemMessage.setMessage(strings.getString(STRID_LOAD_BLOOD_BIBLE_OWNER_INFO));
    } else if (load_type == "race_war_pc_limit") {
        pEvent = new EventReloadInfo(pGamePlayer, EventReloadInfo::RACE_WAR_PC_LIMIT);

        gcSystemMessage.setMessage(strings.getString(STRID_LOAD_RACE_WAR_PC_LIMIT_INFO));
    } else if (load_type == "npc") {
        ZoneID_t zoneID = (ZoneID_t)(atoi(load_value.c_str()));
        Zone* pZone = getZoneByZoneID(zoneID);
        if (pZone != NULL) {
            // Elsewhere the NPCs do not seem to be created
            CastleInfo* pCastleInfo = de::gameContext().castleInfos().getCastleInfo(zoneID);

            if (pCastleInfo != NULL)
                pZone->loadNPCs(pCastleInfo->getRace());
        }
    } else if (load_type == "goods_list_info") {
        pEvent = new EventReloadInfo(pGamePlayer, EventReloadInfo::GOODS_LIST_INFO);

        gcSystemMessage.setMessage(strings.getString(STRID_LOAD_GOODS_LIST_INFO));
    } else {
        gcSystemMessage.setMessage(strings.getString(STRID_WRONG_VARIABLE_NAME));
    }

    if (pEvent != NULL) {
        de::gameContext().clients().addEvent(pEvent);
    }

    // Send the system message only to oneself
    if (pGamePlayer != NULL) {
        pGamePlayer->sendPacket(&gcSystemMessage);
    }

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void opsave(GamePlayer* pGamePlayer, string msg, int i) {
    __BEGIN_TRY __BEGIN_DEBUG_EX

        GCSystemMessage gcSystemMessage;

    gcSystemMessage.setMessage(de::gameContext().strings().getString(STRID_SAVE_YOUR_DATA));

    // A SQL failure arrives as END_DB's DatabaseError, which __END_DEBUG_EX
    // below does not match, so it is swallowed here (the text is in
    // DBError.log). On an empty table the read answers false and the 0
    // below stands, so the loop runs zero times.
    int maxZoneGroupID = 0;
    try {
        defaultZoneInfoRepository().loadMaxZoneGroupID(maxZoneGroupID);
    } catch (const DatabaseError&) {
        return;
    }

    BYTE GroupCount = maxZoneGroupID + 1;
    for (int i = 1; i < GroupCount; i++) {
        ZoneGroup* pZoneGroup = NULL;

        try {
            pZoneGroup = de::gameContext().zoneGroups().getZoneGroup(i);
        } catch (NoSuchElementException&) {
            throw Error("Critical Error : ZoneInfoManager has no such zone group.");
        }

        ZonePlayerManager* pZonePlayerManager = pZoneGroup->getZonePlayerManager();

        pZonePlayerManager->broadcastPacket(&gcSystemMessage);
        pZonePlayerManager->save();
    }

    __END_DEBUG_EX __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void opwall(GamePlayer* pGamePlayer, string msg, int i) {
    __BEGIN_TRY __BEGIN_DEBUG_EX

        size_t j = msg.find_first_of(' ', i + 1);

    GCSystemMessage gcSystemMessage;

    gcSystemMessage.setType(SYSTEM_MESSAGE_OPERATOR);
    gcSystemMessage.setMessage(msg.substr(j + 1, msg.size() - j - 1).c_str());

    if (pGamePlayer != NULL) {
        Creature* pCreature = pGamePlayer->getCreature();
        filelog("change.txt", "[Wall]%s, %s", pCreature->getName().c_str(), gcSystemMessage.getMessage().c_str());
    }

    de::gameContext().zoneGroups().broadcast(&gcSystemMessage);

    __END_DEBUG_EX __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void opshutdown(GamePlayer* pGamePlayer, string msg, int i) {
    __BEGIN_TRY __BEGIN_DEBUG_EX

        // Any zone would do..
        // but it is multithreaded.. the lock has to be taken before the effect is attached.
        size_t j = msg.find_first_of(' ', i + 1);
    Turn_t dTime = atoi(msg.substr(j + 1, msg.size() - j - 1).c_str());

    if (pGamePlayer != NULL) {
        Creature* pCreature = pGamePlayer->getCreature();
        filelog("change.txt", "[ShutDown]%s, %d", pCreature->getName().c_str(), (int)dTime);
    }

    EffectShutDown* pEffectShutDown = new EffectShutDown();
    pEffectShutDown->setNextTime(100);
    pEffectShutDown->setDelay(100);
    pEffectShutDown->setDeadline(dTime);


    if (pGamePlayer != NULL) {
        Creature* pCreature = pGamePlayer->getCreature();

        Zone* pZone = pCreature->getZone();
        (pZone->getObjectRegistry()).registerObject(pEffectShutDown);

        pZone->addEffect(pEffectShutDown);
    } else {
        int ZoneNum = 1003;

        ZoneInfo* pZoneInfo = NULL;
        try {
            pZoneInfo = de::gameContext().zoneInfos().getZoneInfo(ZoneNum);
        } catch (NoSuchElementException&) {
            throw Error("Shutdown requested, but the zone has no zone info.");
        }

        ZoneGroup* pZoneGroup = NULL;

        try {
            pZoneGroup = de::gameContext().zoneGroups().getZoneGroup(pZoneInfo->getZoneGroupID());
        } catch (NoSuchElementException&) {
            throw Error("Shutdown requested, but the zone group is missing.");
        }

        Zone* pZone = pZoneGroup->getZone(ZoneNum);

        (pZone->getObjectRegistry()).registerObject(pEffectShutDown);

        pZone->addEffect_LOCKING(pEffectShutDown);
    }

    __END_DEBUG_EX __END_CATCH
}

void oplog(GamePlayer* pPlayer, string msg, int i) {
    __BEGIN_TRY __BEGIN_DEBUG_EX

        size_t j = msg.find_first_of(' ', i + 1);
    size_t k = msg.find_first_of(' ', j + 1);
    uint sec = 0;
    string name;

    if (k == string::npos) {
        name = msg.substr(j + 1, msg.size() - j - 1).c_str();
    } else {
        name = trim(msg.substr(j + 1, k - j - 1));
        sec = (uint)atoi(msg.substr(k + 1, msg.size() - k - 1).c_str());
    }

    Creature* pTargetCreature = NULL;

    PCFinder& pcFinder = de::gameContext().playerCreatures();

    __ENTER_CRITICAL_SECTION(pcFinder)

    pTargetCreature = pcFinder.getCreature_LOCKED(name);
    if (pTargetCreature == NULL) {
        return;
    }

    GamePlayer* pTargetGamePlayer = dynamic_cast<GamePlayer*>(pTargetCreature->getPlayer());

    if (pTargetGamePlayer == NULL) {
        return;
    }

    if (sec == 0)
        sec = 600;

    if (pTargetGamePlayer->startPacketLog(sec)) {
        char msg[100];
        snprintf(msg, sizeof(msg), "%s: recording PacketLog (%u s)", name.c_str(), sec);

        GCSystemMessage gcMsg;
        gcMsg.setMessage(msg);
        pPlayer->sendPacket(&gcMsg);
    }

    __LEAVE_CRITICAL_SECTION(pcFinder)

    __END_DEBUG_EX __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void opworld(GamePlayer* pGamePlayer, string msg, int i, bool bSameWorldOnly) {
    __BEGIN_TRY __BEGIN_DEBUG_EX

        size_t j = msg.find_first_of(' ', i + 1);
    string command = msg.substr(j + 1, msg.size() - j - 1).c_str();

    // packet
    GGCommand ggCommand;
    ggCommand.setCommand(command);


    // Send it to each server.
    GameServerInfoManager& serverInfos = de::serverContext().serverInfos();
    HashMapGameServerInfo** pGameServerInfos = serverInfos.getGameServerInfos();


    static int myWorldID = de::kernelContext().config().getPropertyInt("WorldID");
    static int myServerID = de::kernelContext().config().getPropertyInt("ServerID");

    int maxWorldID = serverInfos.getMaxWorldID();
    int maxServerGroupID = serverInfos.getMaxServerGroupID();


    for (int worldID = 1; worldID < maxWorldID; worldID++) {
        for (int groupID = 0; groupID < maxServerGroupID; groupID++) {
            HashMapGameServerInfo& gameServerInfo = pGameServerInfos[worldID][groupID];

            if (!gameServerInfo.empty()) {
                HashMapGameServerInfo::const_iterator itr = gameServerInfo.begin();
                for (; itr != gameServerInfo.end(); itr++) {
                    GameServerInfo* pGameServerInfo = itr->second;

                    if (pGameServerInfo->getWorldID() == myWorldID) {
                        // Only when it is not the current server.. (handled above)
                        if (pGameServerInfo->getGroupID() == myServerID) {
                        } else {
                            de::gameContext().loginServer().sendPacket(pGameServerInfo->getIP(),
                                                                       pGameServerInfo->getUDPPort(), &ggCommand);
                        }
                    }
                    // For another World. Unless it is broadcast to this world only..
                    else if (!bSameWorldOnly) {
                        de::gameContext().loginServer().sendPacket(pGameServerInfo->getIP(),
                                                                   pGameServerInfo->getUDPPort(), &ggCommand);
                    }
                }
            }
        }
    }

    __END_DEBUG_EX __END_CATCH
}

void opbugreport(Creature* pCreature, GamePlayer* pGamePlayer, string msg, int i) {
    size_t j = msg.find_first_of(' ', i + 1);
    if (j == string::npos) {
        filelog("bugreport.log", "%s", msg.c_str());
        return;
    }

    string report = msg.substr(j + 1, msg.size() - j - 1);
    if ((j = report.find_first_of('\'')) != string::npos)
        report[j] = '_';
    if ((j = report.find_first_of('\\')) != string::npos)
        report[j] = '_';

    try {
        defaultSessionRepository().insertBugReport(pGamePlayer->getID(), pCreature->getName(), report);
        // If something odd comes in, ignore it
    } catch (...) {
        filelog("bugreport.log", "%s", msg.c_str());
    }
}

void opcrashreport(Creature* pCreature, GamePlayer* pGamePlayer, string msg, int i) {
    size_t j = msg.find_first_of(' ', i + 1);
    size_t k = msg.find_first_of(' ', j + 13);
    size_t l = msg.find_first_of(' ', k + 1);
    size_t m = msg.find_first_of(' ', l + 1);
    if (j == string::npos || k == string::npos || l == string::npos || m == string::npos) {
        filelog("CrashReport.log", "[%s] %s", pGamePlayer->getID().c_str(), msg.c_str());
        return;
    }

    string ExecutableTime = msg.substr(j + 1, k - j - 1);
    string Version = msg.substr(k + 1, l - k - 1);
    string Address = msg.substr(l + 1, m - l - 1);
    string Message = msg.substr(m + 1, msg.size() - m - 1);

    cout << ExecutableTime << endl;
    cout << Version << endl;
    cout << Address << endl;
    cout << Message << endl;

    try {
        defaultSessionRepository().insertCrashLog(pGamePlayer->getID(), pCreature->getName(), ExecutableTime, Version,
                                                  Address, Message);
        // If something odd comes in, ignore it
    } catch (...) {
        filelog("CrashReport.log", "%s", msg.c_str());
    }
}
} // namespace de::gm
