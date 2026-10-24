//////////////////////////////////////////////////////////////////////////////
// Filename    : CGNPCTalkHandler.cpp
// Written By  : Reiot
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "CGNPCTalk.h"

#ifdef __GAME_SERVER__
#include "GCNPCAsk.h"
#include "GCNPCResponse.h"
#include "GCNPCSayDynamic.h"
#include "GQuestManager.h"
#include "GameContext.h"
#include "GamePlayer.h"
#include "NPC.h"
#include "PlayerCreature.h"
#include "StringPool.h"
#include "quest/Action.h"
#include "quest/Condition.h"
#include "quest/Trigger.h"
#include "quest/TriggerManager.h"
#endif

//////////////////////////////////////////////////////////////////////////////
// When a player clicks an NPC the client sends the CGNPCTalk packet to the
// server. When the server handles this packet, and the NPC has the
// CONDITION_TALKED_BY condition flag set, it searches the triggers for a
// fitting one and runs the associated action.
//////////////////////////////////////////////////////////////////////////////
void CGNPCTalkHandler::execute(CGNPCTalk* pPacket, Player* pPlayer)

{
    __BEGIN_DEBUG_EX __BEGIN_DEBUG

        StringPool& strings = de::gameContext().strings();
#ifdef __GAME_SERVER__

    try {
        Assert(pPacket != NULL);
        Assert(pPlayer != NULL);

        GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(pPlayer);
        PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pGamePlayer->getCreature());
        Assert(pPC != NULL);
        Zone* pZone = pPC->getZone();

        // The PC that talks must currently be alive.
        // The reason for the check: a poisoned Slayer that dies of the poison just
        // after asking an NPC for a HEAL, because of the timing,
        // falls to the ground and is then healed by the NPC right after.
        // In that case the HP is full while it lies on the ground.
        // That means PCManager::heartbeat() never goes into killCreature.
        // Hard to explain; anyway, a player has to be alive to talk to an NPC.
        if (pPC->isDead()) {
            return;
        }

        Creature* pNPCBase = NULL;


        // NoSuch removed.
        pNPCBase = pZone->getCreature(pPacket->getObjectID());

        if (pNPCBase == NULL) {
            return;
        }

        NPC* pNPC = dynamic_cast<NPC*>(pNPCBase);

        // The ObjectID might not be an NPC. What is it, a cheat, or a tangled packet?
        // Ignore it either way.
        // by sigi. 2002.11.25
        if (pNPC == NULL) {
            return;
        }

        // Exchange System: Check if this is an Exchange Broker NPC
        // NPC ID for exchange broker (can be configured in database)
        const int EXCHANGE_NPC_ID = 10001; // TODO: Make configurable
        if (pNPC->getNPCID() == EXCHANGE_NPC_ID) {
            // Send exchange menu to player
            // For now, just send a basic response
            // Client will handle opening the exchange UI
            GCNPCResponse gcNPCResponse;
            pPlayer->sendPacket(&gcNPCResponse);

            // Send NPC ask packet with exchange menu options
            // Client should recognize this NPC ID and open exchange UI
            GCNPCAsk askPacket;
            askPacket.setObjectID(pNPC->getObjectID());
            askPacket.setNPCID(pNPC->getNPCID());
            // Set script ID for exchange broker menu
            askPacket.setScriptID(5001); // Exchange Broker Script ID
            pPlayer->sendPacket(&askPacket);

            return;
        }

        // When it is one met through a quest
        if (pPC->getGQuestManager()->metNPC(pNPC)) {
            // Send an OK packet first, for the client.
            GCNPCResponse gcNPCAskAnswer;
            pPlayer->sendPacket(&gcNPCAskAnswer);

            return;
        }

        // In Adam's holy land the races of the NPC and the PC are ignored.
        // In a castle the NPCs differ by race.
        if (!pZone->isHolyLand() || pZone->isCastle()) {
            // Ignore a Vampire talking to a Slayer NPC.
            if (pNPC->getRace() == NPC_RACE_SLAYER && pPC->isVampire()) {
                // Send an OK packet first, for the client.
                GCNPCResponse gcNPCAskAnswer;
                pPlayer->sendPacket(&gcNPCAskAnswer);

                // When a Vampire talks to a Slayer NPC...
                GCNPCSayDynamic saypkt;
                saypkt.setObjectID(pNPC->getObjectID());
                // An Event NPC says something different
                if (pNPC->getNPCID() == 639) {
                    saypkt.setMessage(strings.c_str(STRID_EVENT_NPC_1));
                } else {
                    saypkt.setMessage(strings.c_str(STRID_ALERT_VAMPIRE));
                }
                pPlayer->sendPacket(&saypkt);
                return;
            }
            // Ignore an Ousters talking to a Slayer NPC.
            else if (pNPC->getRace() == NPC_RACE_SLAYER && pPC->isOusters()) {
                // Send an OK packet first, for the client.
                GCNPCResponse gcNPCAskAnswer;
                pPlayer->sendPacket(&gcNPCAskAnswer);

                // When a Vampire talks to a Slayer NPC...
                GCNPCSayDynamic saypkt;
                saypkt.setObjectID(pNPC->getObjectID());
                saypkt.setMessage(strings.c_str(STRID_ALERT_OUSTERS_2));
                pPlayer->sendPacket(&saypkt);
                return;
            }
            // Ignore a Slayer talking to a Vampire NPC too.
            else if (pNPC->getRace() == NPC_RACE_VAMPIRE && pPC->isSlayer()) {
                // Send an OK packet first, for the client.
                GCNPCResponse gcNPCAskAnswer;
                pPlayer->sendPacket(&gcNPCAskAnswer);

                // When a Slayer talks to a Vampire NPC...
                GCNPCSayDynamic saypkt;
                saypkt.setObjectID(pNPC->getObjectID());
                // An Event NPC says something different
                if (pNPC->getNPCID() == 638) {
                    saypkt.setMessage(strings.c_str(STRID_EVENT_NPC_2));
                } else if (pNPC->getNPCID() == 636) {
                    saypkt.setMessage(strings.c_str(STRID_EVENT_NPC_3));
                } else if (pNPC->getNPCID() == 635) {
                    saypkt.setMessage(strings.c_str(STRID_EVENT_NPC_4));
                } else {
                    saypkt.setMessage(strings.c_str(STRID_ALERT_SLAYER));
                }
                pPlayer->sendPacket(&saypkt);
                return;
            }
            // Ignore an Ousters talking to a Vampire NPC too.
            else if (pNPC->getRace() == NPC_RACE_VAMPIRE && pPC->isOusters()) {
                // Send an OK packet first, for the client.
                GCNPCResponse gcNPCAskAnswer;
                pPlayer->sendPacket(&gcNPCAskAnswer);

                GCNPCSayDynamic saypkt;
                saypkt.setObjectID(pNPC->getObjectID());
                saypkt.setMessage(strings.c_str(STRID_ALERT_OUSTERS));
                pPlayer->sendPacket(&saypkt);
                return;
            }
            // Ignore a Slayer talking to an Ousters NPC too.
            else if (pNPC->getRace() == NPC_RACE_OUSTERS && pPC->isSlayer()) {
                // Send an OK packet first, for the client.
                GCNPCResponse gcNPCAskAnswer;
                pPlayer->sendPacket(&gcNPCAskAnswer);

                GCNPCSayDynamic saypkt;
                saypkt.setObjectID(pNPC->getObjectID());
                saypkt.setMessage(strings.c_str(STRID_ALERT_SLAYER_2));
                pPlayer->sendPacket(&saypkt);
                return;
            }
            // Ignore a Vampire talking to an Ousters NPC too.
            else if (pNPC->getRace() == NPC_RACE_OUSTERS && pPC->isVampire()) {
                // Send an OK packet first, for the client.
                GCNPCResponse gcNPCAskAnswer;
                pPlayer->sendPacket(&gcNPCAskAnswer);

                GCNPCSayDynamic saypkt;
                saypkt.setObjectID(pNPC->getObjectID());
                saypkt.setMessage(strings.c_str(STRID_ALERT_VAMPIRE_2));
                pPlayer->sendPacket(&saypkt);
                return;
            }
        }

        // Among Vampires, an NPC of a different clan is ignored too.
        // This code will have to print it another way later.
        // For now there are only two clans...
        // .....
        // There is only one clan
        // If there is an action that reacts to the player talking...
        const TriggerManager& triggerManager = pNPC->getTriggerManager();
        if (triggerManager.hasCondition(Condition::CONDITION_TALKED_BY)) {
            const list<Trigger*>& triggers = triggerManager.getTriggers();
            for (list<Trigger*>::const_iterator itr = triggers.begin(); itr != triggers.end(); itr++) {
                Trigger* pTrigger = *itr;
                Assert(pTrigger != NULL);

                // check all condition after check main condition
                if (pTrigger->hasCondition(Condition::CONDITION_TALKED_BY) &&
                    pTrigger->isAllSatisfied(Trigger::PASSIVE_TRIGGER, pNPC, pPC)) {
                    // Run only the first trigger found, then break.
                    pTrigger->activate(pNPC, pPC);
                    break;
                }
            }
        }
        // The client always waits for the server's answer after clicking an NPC, so
        // doing nothing when no such action exists puts the client into a kind of deadlock.
        // To prevent that, a message to close the dialog is sent.
        else {
            GCNPCResponse gcNPCResponse;
            gcNPCResponse.setCode(NPC_RESPONSE_QUIT_DIALOGUE);
            pPlayer->sendPacket(&gcNPCResponse);
        }
    } catch (Throwable& t) {
    }

#endif

    __END_DEBUG
    __END_DEBUG_EX
}
