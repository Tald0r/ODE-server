////////////////////////////////////////////////////////////////////////////////
// Filename    : ActionShowDonationDialog.cpp
// Written By  :
// Description :
////////////////////////////////////////////////////////////////////////////////

#include "ActionShowDonationDialog.h"

#include "CGDonationMoney.h"
#include "Creature.h"
#include "GCNPCResponse.h"
#include "Player.h"

////////////////////////////////////////////////////////////////////////////////
// read from property buffer
////////////////////////////////////////////////////////////////////////////////
void ActionShowDonationDialog::read(PropertyBuffer& propertyBuffer)

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
void ActionShowDonationDialog::execute(Creature* pCreature1, Creature* pCreature2)

{
    Assert(pCreature1 != NULL);
    Assert(pCreature2 != NULL);
    Assert(pCreature1->isNPC());
    Assert(pCreature2->isPC());

    Player* pPlayer = pCreature2->getPlayer();
    Assert(pPlayer != NULL);

    GCNPCResponse response;
    response.setCode(NPC_RESPONSE_SHOW_DONATION_DIALOG);
    if (m_Type == "Personal")
        response.setParameter(DONATION_TYPE_200501_PERSONAL);
    else if (m_Type == "Guild")
        response.setParameter(DONATION_TYPE_200501_GUILD);

    pPlayer->sendPacket(&response);
}


////////////////////////////////////////////////////////////////////////////////
// get debug string
////////////////////////////////////////////////////////////////////////////////
string ActionShowDonationDialog::toString() const

{
    string str = "ActionShowDonationDialog";

    return str;
}
