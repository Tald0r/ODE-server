//////////////////////////////////////////////////////////////////////////////
// Filename    : CGSayHandler.cc
// Written By  : elca@ewestsoft.com
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "CGSay.h"

#ifdef __GAME_SERVER__
#include "GCSay.h"
#include "GamePlayer.h"
#include "LogNameManager.h"
#include "Vampire.h"
#include "Zone.h"
#include "ZoneUtil.h"
#include "gm/CommandGating.h"
#include "gm/GMCommands.h"
#include "skill/Sniping.h"
#endif


//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void CGSayHandler::execute(CGSay* pPacket, Player* pPlayer) {
    __BEGIN_TRY __BEGIN_DEBUG_EX

#ifdef __GAME_SERVER__

        Assert(pPacket != NULL);
    Assert(pPlayer != NULL);

    try {
        GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(pPlayer);

        // It goes from server to client, so a GC- packet must be used.
        GCSay gcSay;

        Creature* pCreature = pGamePlayer->getCreature();

        // Put the creature name and the message into the packet.
        gcSay.setObjectID(pCreature->getObjectID());
        gcSay.setColor(pPacket->getColor());

        string msg = pPacket->getMessage();

        gcSay.setMessage(msg);

        bool Success = true;

        size_t i = msg.find_first_of('*', 0);

        if (i == 0) {
            Success = false;

            const de::gm::CommandContext command{pCreature, pGamePlayer, msg, static_cast<int>(i),
                                                 de::gm::permissionOf(pCreature)};

            // A broadcast prefix answers the message on its own; anything
            // else is looked up among the operator commands.
            if (!de::gm::broadcastCommands().dispatch(command))
                de::gm::operatorCommands().dispatch(command);
        }

        // Leave a chat log.
        if (LogNameManager::getInstance().isExist(pCreature->getName())) {
            filelog("chatLog.txt", "[Say] %s> %s", pCreature->getName().c_str(), msg.c_str());
        }

        // Clear the invisibility state.
        if (pCreature->isFlag(Effect::EFFECT_CLASS_INVISIBILITY)) {
            Zone* pZone = pCreature->getZone();
            Assert(pZone);
            addVisibleCreature(pZone, pCreature, true);
        }

        if (pCreature->isFlag(Effect::EFFECT_CLASS_SNIPING_MODE)) {
            g_Sniping.checkRevealRatio(pCreature, 20, 10);
        }

        // A transformed Vampire cannot speak.
        if (pCreature->isVampire()) {
            Vampire* pVampire = dynamic_cast<Vampire*>(pCreature);

            if (pVampire->isFlag(Effect::EFFECT_CLASS_TRANSFORM_TO_WOLF) ||
                pVampire->isFlag(Effect::EFFECT_CLASS_TRANSFORM_TO_BAT) ||
                pVampire->isFlag(Effect::EFFECT_CLASS_TRANSFORM_TO_WERWOLF)) {
                Success = false;
            }
        }

        if (pGamePlayer->isPenaltyFlag(PENALTY_TYPE_MUTE)) {
            Success = false;
        }

        if (pCreature->isFlag(Effect::EFFECT_CLASS_MUTE)) {
            Success = false;
        }

        if (Success) {
            // Broadcast to the PCs nearby.
            //		pCreature->getZone()->broadcastSayPacket(pCreature->getX() , pCreature->getY() , &gcSay , pCreature,
            // isVampire);
            pCreature->getZone()->broadcastPacket(pCreature->getX(), pCreature->getY(), &gcSay, pCreature);
        }
    } catch (Throwable& t) {
    }

#endif

    __END_DEBUG_EX __END_CATCH
}
