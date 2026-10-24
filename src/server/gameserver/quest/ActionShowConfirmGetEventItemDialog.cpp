////////////////////////////////////////////////////////////////////////////////
// Filename    : ActionShowConfirmGetEventItemDialog.cpp
// Written By  :
// Description :
////////////////////////////////////////////////////////////////////////////////

#include "ActionShowConfirmGetEventItemDialog.h"

#include "CGGetEventItem.h"
#include "Creature.h"
#include "GCNPCResponse.h"
#include "Player.h"

////////////////////////////////////////////////////////////////////////////////
// read from property buffer
////////////////////////////////////////////////////////////////////////////////
void ActionShowConfirmGetEventItemDialog::read(PropertyBuffer& propertyBuffer)

{
    try {
        m_Type = propertyBuffer.getProperty("Type");
    } catch (NoSuchElementException& nsee) {
        throw Error(nsee.toString());
    }
}


////////////////////////////////////////////////////////////////////////////////
// Execute the action.
////////////////////////////////////////////////////////////////////////////////
void ActionShowConfirmGetEventItemDialog::execute(Creature* pCreature1, Creature* pCreature2)

{
    Assert(pCreature1 != NULL);
    Assert(pCreature2 != NULL);
    Assert(pCreature1->isNPC());
    Assert(pCreature2->isPC());

    Player* pPlayer = pCreature2->getPlayer();
    Assert(pPlayer != NULL);

    GCNPCResponse response;
    response.setCode(NPC_RESPONSE_SHOW_CONFIRM_GET_EVENT_ITEM_DIALOG);
    if (m_Type == "ComebackItem")
        response.setParameter(EVENT_TYPE_200501_COMBACK_ITEM);
    else if (m_Type == "ComebackPremiumItem")
        response.setParameter(EVENT_TYPE_200501_COMBACK_PREMIUM_ITEM);
    else if (m_Type == "ComebackRecommendItem")
        response.setParameter(EVENT_TYPE_200501_COMBACK_RECOMMEND_ITEM);
    else
        return;

    pPlayer->sendPacket(&response);
}


////////////////////////////////////////////////////////////////////////////////
// get debug string
////////////////////////////////////////////////////////////////////////////////
string ActionShowConfirmGetEventItemDialog::toString() const

{
    string str = "ActionShowConfirmGetEventItemDialog";

    return str;
}
