//////////////////////////////////////////////////////////////////////////////
// Filename    : GuildStepRunner.cpp
// Description : performs the steps a guild decision answered.
//////////////////////////////////////////////////////////////////////////////

#include "GuildStepRunner.h"

#include <stdio.h>

#include "GameServerManager.h"
#include "Guild.h"
#include "GuildManager.h"
#include "SGDeleteGuildOK.h"
#include "SGExpelGuildMemberOK.h"
#include "SGModifyGuildMemberOK.h"
#include "SGModifyGuildOK.h"
#include "SGQuitGuildOK.h"
#include "SharedContext.h"
#include "StringPool.h"
#include "repository/SharedGuildRepository.h"

void runGuildSteps(const std::vector<SharedGuildStep>& steps, Guild* pGuild, const char* departureLabel) {
    // Five of the steps below answer the game servers; bind the manager once.
    GameServerManager& gameServers = de::sharedContext().gameServers();

    for (const SharedGuildStep& step : steps) {
        switch (step.action) {
        case SharedGuildAction::LogGuildExit:
            if (step.sender.empty())
                filelog("GuildExit.log", "GuildID: %d, GuildName: %s, %s: %s", pGuild->getID(),
                        pGuild->getName().c_str(), departureLabel, step.name.c_str());
            else
                filelog("GuildExit.log", "GuildID: %d, GuildName: %s, %s: %s, By: %s", pGuild->getID(),
                        pGuild->getName().c_str(), departureLabel, step.name.c_str(), step.sender.c_str());
            break;

        case SharedGuildAction::LogGuildBroken:
            filelog("GuildBroken.log", "GuildID: %d, GuildName: %s, MemberCount: %d, %s: %s", pGuild->getID(),
                    pGuild->getName().c_str(), step.memberCount, departureLabel, step.name.c_str());
            break;

        case SharedGuildAction::StampRequestDateTime:
            defaultSharedGuildRepository().stampMemberRequestDateTime(step.name);
            break;

        case SharedGuildAction::SetCharacterGuildID:
            defaultSharedGuildRepository().setCharacterGuildID(step.race, step.characterGuildID, step.name);
            break;

        case SharedGuildAction::AddCharacterGold:
            defaultSharedGuildRepository().addCharacterGold(step.race, (int)step.gold, step.name);
            break;

        case SharedGuildAction::InsertMessage:
            defaultSharedGuildRepository().insertMessage(step.spelling, step.name,
                                                         de::sharedContext().strings().c_str(step.message));
            break;

        case SharedGuildAction::ExpireMember: {
            GuildMember* pGuildMember = pGuild->getMember(step.name);
            if (pGuildMember != NULL)
                pGuildMember->expire();
            break;
        }

        case SharedGuildAction::LeaveMember: {
            GuildMember* pGuildMember = pGuild->getMember(step.name);
            if (pGuildMember != NULL)
                pGuildMember->leave();
            break;
        }

        case SharedGuildAction::DropMember:
            pGuild->deleteMember(step.name);
            break;

        case SharedGuildAction::FreeMember: {
            // The guild's map still holds the pointer; the ClearMembers step
            // that ends the walk drops every one of them at once.
            GuildMember* pGuildMember = pGuild->getMember(step.name);
            SAFE_DELETE(pGuildMember);
            break;
        }

        case SharedGuildAction::ClearMembers:
            pGuild->getMembers().clear();
            break;

        case SharedGuildAction::ModifyMemberRank:
            pGuild->modifyMemberRank(step.name, step.rank);
            break;

        case SharedGuildAction::SetGuildMaster: {
            pGuild->setMaster(step.name);

            char field[30];
            snprintf(field, sizeof(field), "Master='%s'", step.name.c_str());
            pGuild->tinysave(field);
            break;
        }

        case SharedGuildAction::SetGuildState:
            pGuild->setState(step.state);
            pGuild->save();
            break;

        case SharedGuildAction::DeleteGuild:
            SAFE_DELETE(pGuild);
            de::sharedContext().guilds().deleteGuild(step.guildID);
            break;

        case SharedGuildAction::SendModifyGuildOK: {
            SGModifyGuildOK sgModifyGuildOK;
            sgModifyGuildOK.setGuildID(step.guildID);
            sgModifyGuildOK.setGuildState(step.state);

            gameServers.broadcast(&sgModifyGuildOK);
            break;
        }

        case SharedGuildAction::SendExpelGuildMemberOK: {
            SGExpelGuildMemberOK sgExpelGuildMemberOK;
            sgExpelGuildMemberOK.setGuildID(step.guildID);
            sgExpelGuildMemberOK.setName(step.name);
            sgExpelGuildMemberOK.setSender(step.sender);

            gameServers.broadcast(&sgExpelGuildMemberOK);
            break;
        }

        case SharedGuildAction::SendQuitGuildOK: {
            SGQuitGuildOK sgQuitGuildOK;
            sgQuitGuildOK.setGuildID(step.guildID);
            sgQuitGuildOK.setName(step.name);

            gameServers.broadcast(&sgQuitGuildOK);
            break;
        }

        case SharedGuildAction::SendDeleteGuildOK: {
            SGDeleteGuildOK sgDeleteGuildOK;
            sgDeleteGuildOK.setGuildID(step.guildID);

            gameServers.broadcast(&sgDeleteGuildOK);
            break;
        }

        case SharedGuildAction::SendModifyGuildMemberOK: {
            SGModifyGuildMemberOK sgModifyGuildMemberOK;
            sgModifyGuildMemberOK.setGuildID(step.guildID);
            sgModifyGuildMemberOK.setName(step.name);
            sgModifyGuildMemberOK.setGuildMemberRank(step.rank);
            sgModifyGuildMemberOK.setSender(step.sender);

            gameServers.broadcast(&sgModifyGuildMemberOK);
            break;
        }
        }
    }
}
