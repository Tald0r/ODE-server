////////////////////////////////////////////////////////////////////////////////
// Filename    : ActionCheckPartnerName.cpp
// Written By  :
// Description :
////////////////////////////////////////////////////////////////////////////////

#include "ActionCheckPartnerName.h"

#include <cstdio>

#include "Creature.h"
#include "FlagSet.h"
#include "GCSystemMessage.h"
#include "GameContext.h"
#include "GamePlayer.h"
#include "NPC.h"
#include "PlayerCreature.h"
#include "StringPool.h"
#include "couple/CoupleManager.h"

////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////
void ActionCheckPartnerName::read(PropertyBuffer& propertyBuffer)

{
    try {
    } catch (NoSuchElementException& nsee) {
        throw Error(nsee.toString());
    }
}


////////////////////////////////////////////////////////////////////////////////
// Execute the action.
////////////////////////////////////////////////////////////////////////////////
void ActionCheckPartnerName::execute(Creature* pCreature1, Creature* pCreature2)

{
    Assert(pCreature1 != NULL);
    Assert(pCreature2 != NULL);
    Assert(pCreature1->isNPC());
    Assert(pCreature2->isPC());

    PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pCreature2);
    Assert(pPC != NULL);

    char buffer[256];
    string partnerName;

    if (!pPC->getFlagSet()->isOn(FLAGSET_IS_COUPLE) || !context().couples().getPartnerName(pPC, partnerName)) {
        snprintf(buffer, sizeof(buffer), "%s", context().strings().getString(STRID_NOT_COUPLE).c_str());
    } else {
        snprintf(buffer, sizeof(buffer), context().strings().getString(STRID_COUPLE_IS).c_str(), partnerName.c_str());
    }

    GCSystemMessage gcSM;
    gcSM.setMessage(buffer);

    pPC->getPlayer()->sendPacket(&gcSM);
}


////////////////////////////////////////////////////////////////////////////////
// get debug string
////////////////////////////////////////////////////////////////////////////////
string ActionCheckPartnerName::toString() const

{
    StringStream msg;
    msg << "ActionCheckPartnerName(" << ")";

    return msg.toString();
}
