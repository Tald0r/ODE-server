////////////////////////////////////////////////////////////////////////////////
// Filename    : ActionSystemMessage.cpp
// Written By  :
// Description :
////////////////////////////////////////////////////////////////////////////////

#include "ActionSystemMessage.h"

#include "Creature.h"
#include "GCSystemMessage.h"
#include "GameContext.h"
#include "GamePlayer.h"
#include "StringPool.h"

////////////////////////////////////////////////////////////////////////////////
// read from property buffer
////////////////////////////////////////////////////////////////////////////////
void ActionSystemMessage::read(PropertyBuffer& propertyBuffer)

{
    try {
        m_Content = propertyBuffer.getPropertyInt("Content");
    } catch (NoSuchElementException& nsee) {
        throw Error(nsee.toString());
    }
}


////////////////////////////////////////////////////////////////////////////////
// Execute the action.
////////////////////////////////////////////////////////////////////////////////
void ActionSystemMessage::execute(Creature* pCreature1, Creature* pCreature2)

{
    Assert(pCreature2 != NULL);
    Assert(pCreature2->isPC());

    Player* pPlayer = pCreature2->getPlayer();
    Assert(pPlayer != NULL);

    GCSystemMessage gcSystemMessage;
    gcSystemMessage.setMessage(context().strings().getString(m_Content));
    pPlayer->sendPacket(&gcSystemMessage);
}


////////////////////////////////////////////////////////////////////////////////
// get debug string
////////////////////////////////////////////////////////////////////////////////
string ActionSystemMessage::toString() const

{
    StringStream msg;
    msg << "ActionSystemMessage(" << "Content:" << m_Content << ")";
    return msg.toString();
}
