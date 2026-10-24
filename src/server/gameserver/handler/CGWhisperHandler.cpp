//////////////////////////////////////////////////////////////////////////////
// Filename    : CGWhisperHandler.cc
// Written By  : reiot@ewestsoft.com
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "CGWhisper.h"

#ifdef __GAME_SERVER__
#include "Creature.h"
#include "GCWhisper.h"
#include "GCWhisperFailed.h"
#include "GGServerChat.h"
#include "GameContext.h"
#include "GamePlayer.h"
#include "GameServerInfoManager.h"
#include "KernelContext.h"
#include "LogNameManager.h"
#include "LoginServerManager.h"
#include "PCFinder.h"
#include "Properties.h"
#include "ServerContext.h"
#include "repository/CharacterRepository.h"
#include "repository/SessionRepository.h"

#endif

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void CGWhisperHandler::execute(CGWhisper* pPacket, Player* pPlayer)

{
    __BEGIN_DEBUG_EX

#ifdef __GAME_SERVER__

    Assert(pPacket != NULL);
    Assert(pPlayer != NULL);

    GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(pPlayer);

    try {
        Player* pTargetPlayer = NULL;

        Creature* pCreature = pGamePlayer->getCreature();

        bool Success = false;

        // Find the user by name.
        PCFinder& pcFinder = de::gameContext().playerCreatures();

        __ENTER_CRITICAL_SECTION(pcFinder)

        Creature* pTargetCreature = pcFinder.getCreature_LOCKED(pPacket->getName());

        // NoSuch removed.
        if (pTargetCreature != NULL) {
            // Leave a chat log.
            if (LogNameManager::getInstance().isExist(pCreature->getName())) {
                filelog("chatLog.txt", "[Whisper] %s --> %s> %s", pCreature->getName().c_str(),
                        pTargetCreature->getName().c_str(), pPacket->getMessage().c_str());
            }

            pTargetPlayer = pTargetCreature->getPlayer();
            Assert(pTargetPlayer != NULL);
            Success = true;

            if (pGamePlayer->isPenaltyFlag(PENALTY_TYPE_MUTE)) {
                Success = false;
            }

            // Send the Message once the user is found
            if (Success) {
                if (((GamePlayer*)pTargetPlayer)->getPlayerStatus() == GPS_NORMAL) {
                    if (pCreature != NULL && pTargetCreature != NULL) {
                        // It goes from server to client, so a GC- packet must be used.
                        GCWhisper gcWhisper;

                        // Put the creature name and the message into the packet.
                        gcWhisper.setName(pCreature->getName());
                        gcWhisper.setColor(pPacket->getColor());
                        gcWhisper.setMessage(pPacket->getMessage());
                        gcWhisper.setRace(pCreature->getRace());
                        pTargetPlayer->sendPacket(&gcWhisper);
                    } else {
                        GCWhisperFailed gcWhisperFailed;
                        pGamePlayer->sendPacket(&gcWhisperFailed);
                    }
                }
            }

            // Failed when there is no such user or the name was wrong
        } else {
            /*inthesky*/
            /*	Search the DB for the user. What the DB has to give is the Player, the Logon information and the ServerID.
             *	Once found, send a GGServerChat packet to that game server (sender, Color, Message, Race,
             *	the server that receives the GGServerChat packet finds the player by name, builds a GCWhisper packet and sends it.
             *	If not found, give up (send Failed).
             *	Whether the user exists is decided from the DB alone. How much can the DB be trusted..??????
             */
            bool bServerFind = false;
            ServerGroupID_t CurrentServerGroupID;
            string LogOn;
            string PlayerID;

            try {
                {
                    // Find the PlayerID in the Slayer table by creature name.
                    if (defaultCharacterRepository().loadSlayerPlayerID(pPacket->getName(), PlayerID)) {
                        // With the PlayerID found, find the ServerGroupID and the LogOn information in the Player table.
                        int serverGroupID = 0;

                        // The Player information was found.
                        if (defaultSessionRepository().loadPlayerLocation(PlayerID, serverGroupID, LogOn)) {
                            CurrentServerGroupID = serverGroupID;

                            // Set the bServerFind flag to true while in game
                            if (LogOn == "GAME") {
                                bServerFind = true;
                            }
                        }
                    }

                    if (bServerFind) // when it was found
                    {
                        // Relay the whisper to the receiver's game server: the sender's name and race, the
                        // receiver's name, and the text with its color.
                        GameServerInfo* pGameServerInfo = de::serverContext().serverInfos().getGameServerInfo(
                            1, CurrentServerGroupID, de::kernelContext().config().getPropertyInt("WorldID"));
                        if (pGameServerInfo != NULL) {
                            GGServerChat ggServerChat;
                            ggServerChat.setSender(pCreature->getName());
                            ggServerChat.setReceiver(pPacket->getName());
                            ggServerChat.setColor(pPacket->getColor());
                            ggServerChat.setMessage(pPacket->getMessage());
                            ggServerChat.setRace(pCreature->getRace());

                            de::gameContext().loginServer().sendPacket(pGameServerInfo->getIP(),
                                                                       pGameServerInfo->getUDPPort(), &ggServerChat);
                        } else {
                            GCWhisperFailed gcWhisperFailed;
                            pGamePlayer->sendPacket(&gcWhisperFailed);
                        }
                    } else {
                        GCWhisperFailed gcWhisperFailed;
                        pGamePlayer->sendPacket(&gcWhisperFailed);
                    }
                }
            } catch (...) { /* write log plz */
            }
        }

        __LEAVE_CRITICAL_SECTION(pcFinder)
    } catch (Throwable& t) {
        GCWhisperFailed gcWhisperFailed;
        pGamePlayer->sendPacket(&gcWhisperFailed);
    }

#endif

    __END_DEBUG_EX
}
