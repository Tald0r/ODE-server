////////////////////////////////////////////////////////////////////////////////
// Filename    : ActionCancelQuest.cpp
// Written By  :
// Description :
////////////////////////////////////////////////////////////////////////////////

#include "ActionCancelQuest.h"

#include "Creature.h"
#include "GCNPCAsk.h"
#include "GCNPCResponse.h"
#include "GamePlayer.h"
#include "NPC.h"
#include "PlayerCreature.h"
#include "VariableManager.h"
#include "mission/MonsterKillQuestInfo.h"
#include "mission/QuestInfoManager.h"
#include "mission/QuestManager.h"

////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////
void ActionCancelQuest::read(PropertyBuffer& propertyBuffer)

{
    try {
        m_ScriptID = propertyBuffer.getPropertyInt("ScriptID");
        m_CounterScriptID = propertyBuffer.getPropertyInt("CounterScriptID");
    } catch (NoSuchElementException& nsee) {
        throw Error(nsee.toString());
    }
}


////////////////////////////////////////////////////////////////////////////////
// Execute the action.
////////////////////////////////////////////////////////////////////////////////
void ActionCancelQuest::execute(Creature* pCreature1, Creature* pCreature2)

{
    Assert(pCreature2 != NULL);
    Assert(pCreature2->isPC());

    PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pCreature2);
    Assert(pPC != NULL);

    Player* pPlayer = pCreature2->getPlayer();
    Assert(pPlayer != NULL);

    GCNPCResponse gcNPCResponse;
    gcNPCResponse.setCode(NPC_RESPONSE_QUEST);

    QuestManager* pQM = pPC->getQuestManager();
    QuestMessage result;

    if (pQM != NULL) {
        result = pQM->cancelQuest();
    } else {
        result = CANCEL_NOT_IN_QUEST;
    }

    gcNPCResponse.setParameter((uint)result);
    pPlayer->sendPacket(&gcNPCResponse);

    if (pCreature1 != NULL && pCreature1->isNPC()) {
        ScriptID_t sID;
        if (result == CANCEL_SUCCESS)
            sID = m_ScriptID;
        else
            sID = m_CounterScriptID;

        if (sID != 0) {
            GCNPCAsk gcNPCAsk;
            gcNPCAsk.setObjectID(pCreature1->getObjectID());
            gcNPCAsk.setScriptID(sID);
            gcNPCAsk.setNPCID(dynamic_cast<NPC*>(pCreature1)->getNPCID());

            pPlayer->sendPacket(&gcNPCAsk);
        } else {
            gcNPCResponse.setCode(NPC_RESPONSE_QUIT_DIALOGUE);
            pPlayer->sendPacket(&gcNPCResponse);
        }
    }
}


////////////////////////////////////////////////////////////////////////////////
// get debug string
////////////////////////////////////////////////////////////////////////////////
string ActionCancelQuest::toString() const

{
    StringStream msg;
    msg << "ActionCancelQuest(" << ")";

    return msg.toString();
}
