////////////////////////////////////////////////////////////////////////////////
// Filename    : ActionMiniGame.cpp
// Written By  :
// Description :
////////////////////////////////////////////////////////////////////////////////

#include "ActionMiniGame.h"

#include "Creature.h"
#include "GCNoticeEvent.h"
#include "GamePlayer.h"
#include "NPC.h"

////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////
void ActionMiniGame::read(PropertyBuffer& propertyBuffer)

{
    try {
        // read script id
        m_GameType = propertyBuffer.getPropertyInt("GameType");
    } catch (NoSuchElementException& nsee) {
        throw Error(nsee.toString());
    }
}


////////////////////////////////////////////////////////////////////////////////
// Execute the action.
////////////////////////////////////////////////////////////////////////////////
void ActionMiniGame::execute(Creature* pCreature1, Creature* pCreature2)

{
    Assert(pCreature1 != NULL);
    Assert(pCreature2 != NULL);
    Assert(pCreature1->isNPC());
    Assert(pCreature2->isPC());

    GCNoticeEvent gcNoticeEvent;
    gcNoticeEvent.setCode(NOTICE_EVENT_MINI_GAME);
    gcNoticeEvent.setParameter((uint)m_GameType);

    Player* pPlayer = pCreature2->getPlayer();
    pPlayer->sendPacket(&gcNoticeEvent);
}


////////////////////////////////////////////////////////////////////////////////
// get debug string
////////////////////////////////////////////////////////////////////////////////
string ActionMiniGame::toString() const

{
    StringStream msg;
    msg << "ActionMiniGame(" << ",GameType:" << (int)m_GameType << ")";

    return msg.toString();
}
