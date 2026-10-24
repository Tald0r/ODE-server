//////////////////////////////////////////////////////////////////////////////
// Filename    : GuildCommands.cpp
// Description : GM commands that act on guilds and guild unions.
//////////////////////////////////////////////////////////////////////////////

#include <stdio.h>

#include <list>

#include "CreatureUtil.h"
#include "DatabaseError.h"
#include "GCSystemMessage.h"
#include "GGCommand.h"
#include "GSModifyGuildMember.h"
#include "GameContext.h"
#include "GamePlayer.h"
#include "GameServerInfoManager.h"
#include "Guild.h"
#include "GuildManager.h"
#include "GuildUnion.h"
#include "ItemUtil.h"
#include "LoginServerManager.h"
#include "OptionInfo.h"
#include "Ousters.h"
#include "PacketUtil.h"
#include "Properties.h"
#include "RelicUtil.h"
#include "SharedServerManager.h"
#include "Slayer.h"
#include "VSDateTime.h"
#include "Vampire.h"
#include "Zone.h"
#include "ZoneUtil.h"
#include "gm/GMCommands.h"
#include "repository/CharacterRepository.h"

namespace de::gm {
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void opguild(string msg, int i, Creature* pCreature) {
    i = i + 7;

    size_t j = msg.find_first_of(' ', i);
    string command = msg.substr(i, j - i);
    string variable = trim(msg.substr(j + 1));

    GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(pCreature->getPlayer());

    if (command == "changeGuildMaster") {
        size_t a = variable.find_first_of(' ', 0);
        GuildID_t guildID = atoi(variable.substr(0, a).c_str());
        string master = trim(variable.substr(a + 1));

        Guild* pGuild = de::gameContext().guilds().getGuild(guildID);
        if (pGuild == NULL) {
            GCSystemMessage msg;
            msg.setMessage("No such Guild");
            pGamePlayer->sendPacket(&msg);

            return;
        }

        GuildMember* pGuildMember = pGuild->getMember(master);
        if (pGuildMember == NULL) {
            GCSystemMessage msg;
            msg.setMessage("No such Guild Member");
            pGamePlayer->sendPacket(&msg);

            return;
        }

        if (pGuildMember->getRank() != GuildMember::GUILDMEMBER_RANK_NORMAL &&
            pGuildMember->getRank() != GuildMember::GUILDMEMBER_RANK_SUBMASTER) {
            GCSystemMessage msg;
            msg.setMessage("New Guild Member must be submaster or normal member");
            pGamePlayer->sendPacket(&msg);

            return;
        }

        {
            if (pGuild->getRace() == Guild::GUILD_RACE_SLAYER) {
                SlayerMasterStatsRow stats;

                if (!defaultCharacterRepository().loadSlayerMasterStats(master, stats)) {
                    return;
                }

                Fame_t Fame = stats.fame;
                SkillLevel_t BladeLevel = stats.bladeLevel;
                SkillLevel_t SwordLevel = stats.swordLevel;
                SkillLevel_t GunLevel = stats.gunLevel;
                SkillLevel_t HealLevel = stats.healLevel;
                SkillLevel_t EnchantLevel = stats.enchantLevel;
                SkillDomainType_t highestDomain;
                SkillLevel_t maxLevel;

                if (BladeLevel > SwordLevel) {
                    maxLevel = BladeLevel;
                    highestDomain = SKILL_DOMAIN_BLADE;
                } else {
                    maxLevel = SwordLevel;
                    highestDomain = SKILL_DOMAIN_SWORD;
                }
                if (GunLevel > maxLevel) {
                    maxLevel = GunLevel;
                    highestDomain = SKILL_DOMAIN_GUN;
                }
                if (HealLevel > maxLevel) {
                    maxLevel = HealLevel;
                    highestDomain = SKILL_DOMAIN_HEAL;
                }
                if (EnchantLevel > maxLevel) {
                    maxLevel = EnchantLevel;
                    highestDomain = SKILL_DOMAIN_ENCHANT;
                }

                if (maxLevel < REQUIRE_SLAYER_MASTER_SKILL_DOMAIN_LEVEL) {
                    GCSystemMessage msg;
                    msg.setMessage("Master Level Limit Error");
                    pGamePlayer->sendPacket(&msg);

                    return;
                }

                if (Fame < REQUIRE_SLAYER_MASTER_FAME[highestDomain]) {
                    GCSystemMessage msg;
                    msg.setMessage("Master Fame Limit Error");
                    pGamePlayer->sendPacket(&msg);

                    return;
                }
            } else if (pGuild->getRace() == Guild::GUILD_RACE_VAMPIRE) {
                int level = 0;

                if (!defaultCharacterRepository().loadVampireLevel(master, level)) {
                    return;
                }

                Level_t Level = level;
                if (Level < REQUIRE_VAMPIRE_MASTER_LEVEL) {
                    GCSystemMessage msg;
                    msg.setMessage("Master Level Limit Error");
                    pGamePlayer->sendPacket(&msg);

                    return;
                }
            } else if (pGuild->getRace() == Guild::GUILD_RACE_OUSTERS) {
                int level = 0;

                if (!defaultCharacterRepository().loadOustersLevel(master, level)) {
                    return;
                }

                Level_t Level = level;
                if (Level < REQUIRE_OUSTERS_MASTER_LEVEL) {
                    GCSystemMessage msg;
                    msg.setMessage("Master Level Limit Error");
                    pGamePlayer->sendPacket(&msg);

                    return;
                }
            } else {
                return;
            }
        }
        GSModifyGuildMember gsPacket;
        gsPacket.setGuildID(pGuild->getID());
        gsPacket.setName(master);
        gsPacket.setGuildMemberRank(GuildMember::GUILDMEMBER_RANK_MASTER);
        gsPacket.setSender(pCreature->getName());

        de::gameContext().sharedServer().sendPacket(&gsPacket);
    }
}

void opmodifyunioninfo(GamePlayer* pGamePlayer, string msg, int i, bool bSameWorldOnly = true) {
    __BEGIN_DEBUG_EX

        size_t j = msg.find_first_of(' ', i + 1);
    uint gID = (uint)atoi(msg.substr(j + 1, msg.size() - j - 1).c_str());


    cout << "ModifyunionInfo->sharedserver() : " << (int)gID << endl;

    sendGCOtherModifyInfoGuildUnionByGuildID(gID);


    __END_DEBUG_EX
}

void oprefreshguildunion(GamePlayer* pGamePlayer, string msg, int i, bool bSameWorldOnly = true) {
    __BEGIN_DEBUG_EX

        size_t j = msg.find_first_of(' ', i + 1);
    string command = msg.substr(j + 1, msg.size() - j - 1).c_str();

    cout << "GuildUnionManager->reload()" << endl;

    GuildUnionManager::Instance().reload();


    __END_DEBUG_EX
}
} // namespace de::gm
