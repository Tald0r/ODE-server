////////////////////////////////////////////////////////////////////////////////
// Filename    : ActionRestore.cpp
// Written By  :
// Description :
////////////////////////////////////////////////////////////////////////////////

#include "ActionRestore.h"

#include "Creature.h"
#include "GCNPCResponse.h"
#include "GameContext.h"
#include "NPC.h"
#include "Restore.h"
#include "SkillHandlerManager.h"

////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////
void ActionRestore::read(PropertyBuffer& propertyBuffer)

{}

////////////////////////////////////////////////////////////////////////////////
// Execute the action.
////////////////////////////////////////////////////////////////////////////////
void ActionRestore::execute(Creature* pCreature1, Creature* pCreature2)

{
    Assert(pCreature1 != NULL);
    Assert(pCreature2 != NULL);
    Assert(pCreature1->isNPC());
    Assert(pCreature2->isPC());

    GCNPCResponse okpkt;
    pCreature2->getPlayer()->sendPacket(&okpkt);

    // Get the NPC.
    NPC* pNPC = dynamic_cast<NPC*>(pCreature1);

    // Get the skill handler.
    SkillHandler* pSkillHandler = context().skillHandlers().getSkillHandler(SKILL_RESTORE);
    Assert(pSkillHandler != NULL);

    // Cast it to the Restore handler.
    Restore* pRestore = dynamic_cast<Restore*>(pSkillHandler);

    // Perform the restore.
    pRestore->execute(pNPC, pCreature2);
}


////////////////////////////////////////////////////////////////////////////////
// get debug string
////////////////////////////////////////////////////////////////////////////////
string ActionRestore::toString() const

{
    StringStream msg;
    msg << "ActionRestore(" << ")";
    return msg.toString();
}
