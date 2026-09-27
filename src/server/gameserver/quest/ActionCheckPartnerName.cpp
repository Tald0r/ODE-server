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
    __BEGIN_TRY

    try {
    } catch (NoSuchElementException& nsee) {
        throw Error(nsee.toString());
    }

    __END_CATCH
}


////////////////////////////////////////////////////////////////////////////////
// Execute the action.
////////////////////////////////////////////////////////////////////////////////
void ActionCheckPartnerName::execute(Creature* pCreature1, Creature* pCreature2)

{
    __BEGIN_TRY

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

    __END_CATCH
}


////////////////////////////////////////////////////////////////////////////////
// get debug string
////////////////////////////////////////////////////////////////////////////////
string ActionCheckPartnerName::toString() const

{
    __BEGIN_TRY

    StringStream msg;
    msg << "ActionCheckPartnerName(" << ")";

    return msg.toString();

    __END_CATCH
}
