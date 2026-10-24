//////////////////////////////////////////////////////////////////////////////
// Filename    : CGGlobalChatHandler.cc
// Written By  : reiot@ewestsoft.com
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "CGGlobalChat.h"

#ifdef __GAME_SERVER__
#include "Creature.h"
#include "GCGlobalChat.h"
#include "GamePlayer.h"
#include "IncomingPlayerManager.h"
#include "LogNameManager.h"
#include "Zone.h"
#endif

//////////////////////////////////////////////////////////////////////////////
// Packet used to float a GlobalChat string above the head.
// Broadcast to every player that can see this creature.
// For now the GCGlobalChat packet is broadcast to every player in the
// current zone (or to all players).
//////////////////////////////////////////////////////////////////////////////
void CGGlobalChatHandler::execute(CGGlobalChat* pPacket, Player* pPlayer)

{
    __BEGIN_DEBUG_EX

#ifdef __GAME_SERVER__

        Assert(pPacket != NULL);
    Assert(pPlayer != NULL);

    GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(pPlayer);

    if (pGamePlayer->getPlayerStatus() == GPS_NORMAL) {
        if (pGamePlayer->isPenaltyFlag(PENALTY_TYPE_MUTE)) {
            return;
        }
        Creature* pCreature = pGamePlayer->getCreature();

        Assert(pCreature != NULL);

        // It goes from server to client, so a GC- packet must be used.
        GCGlobalChat gcGlobalChat;

        size_t i = pPacket->getMessage().find_first_of('*', 0);

        if (i == 0)
            return;

        // text color setting
        gcGlobalChat.setColor(pPacket->getColor());

        // Put the creature name and the message into the packet.
        StringStream msg;
        msg << pCreature->getName() << " " << pPacket->getMessage();

        gcGlobalChat.setMessage(msg.toString());
        gcGlobalChat.setRace(pCreature->getRace());

        // Broadcast to the PCs nearby.
        pCreature->getZone()->broadcastPacket(&gcGlobalChat, pCreature);


        // Leave a chat log.
        if (LogNameManager::getInstance().isExist(pCreature->getName())) {
            filelog("chatLog.txt", "[Global] %s> %s", pCreature->getName().c_str(), pPacket->getMessage().c_str());
        }
    }

#endif

    __END_DEBUG_EX
}
