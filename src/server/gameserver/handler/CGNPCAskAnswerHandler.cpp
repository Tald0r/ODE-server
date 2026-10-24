//////////////////////////////////////////////////////////////////////////////
// Filename    : CGNPCAskAnswer.cpp
// Written By  : excel96
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "CGNPCAskAnswer.h"

#ifdef __GAME_SERVER__
#include <fstream>

#include "GCNPCResponse.h"
#include "GamePlayer.h"
#include "NPC.h"
#include "quest/Action.h"
#include "quest/Condition.h"
#include "quest/Trigger.h"
#include "quest/TriggerManager.h"
#endif

//////////////////////////////////////////////////////////////////////////////
// When a player clicks an NPC the client sends the CGNPCAskAnswer packet
// to the server. When the server handles this packet, and the NPC has
// the CONDITION_TALKED_BY condition flag set,
// it searches the triggers for a fitting one and runs the associated action.
//////////////////////////////////////////////////////////////////////////////
void CGNPCAskAnswerHandler::execute(CGNPCAskAnswer* pPacket, Player* pPlayer)

{
    __BEGIN_DEBUG_EX

#ifdef __GAME_SERVER__

        Assert(pPacket != NULL);
    Assert(pPlayer != NULL);

    GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(pPlayer);
    Creature* pPC = pGamePlayer->getCreature();
    Creature* pCreature = NULL;

    if (pPC == NULL)
        return;

    Zone* pZone = pPC->getZone();

    if (pZone == NULL)
        return;


    // NoSuch removed.
    pCreature = pZone->getCreature(pPacket->getObjectID());

    if (pCreature == NULL || !pCreature->isNPC()) {
        GCNPCResponse okpkt;
        pPlayer->sendPacket(&okpkt);


        return;
    }

    NPC* pNPC = dynamic_cast<NPC*>(pCreature);

    COND_ANSWERED_BY cond;
    cond.ScriptID = pPacket->getScriptID();
    cond.AnswerID = pPacket->getAnswerID();

    // get NPC's trigger manager
    const TriggerManager& triggerManager = pNPC->getTriggerManager();

    // check main condition
    if (triggerManager.hasCondition(Condition::CONDITION_ANSWERED_BY)) {
        const list<Trigger*>& triggers = triggerManager.getTriggers();
        for (list<Trigger*>::const_iterator itr = triggers.begin(); itr != triggers.end(); itr++) {
            Trigger* pTrigger = *itr;
            if (pTrigger == NULL) {
                return;
            }

            // check all condition after check main condition
            if (pTrigger->hasCondition(Condition::CONDITION_ANSWERED_BY) &&
                pTrigger->isAllSatisfied(Trigger::PASSIVE_TRIGGER, pNPC, pPC, (void*)&cond)) {
                // Run only the first trigger found, then break.
                pTrigger->activate(pNPC, pPC);
                break;
            }
        }
    }

#endif

    __END_DEBUG_EX
}
