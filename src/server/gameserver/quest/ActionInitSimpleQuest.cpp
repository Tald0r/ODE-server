////////////////////////////////////////////////////////////////////////////////
// Filename    : ActionInitSimpleQuest.cpp
// Written By  :
// Description :
////////////////////////////////////////////////////////////////////////////////

#include "ActionInitSimpleQuest.h"

#include "Creature.h"
#include "NPC.h"
#include "mission/SimpleQuestInfoManager.h"
#include "mission/SimpleQuestRewardManager.h"

////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////
void ActionInitSimpleQuest::read(PropertyBuffer& propertyBuffer)

{
    try {
    } catch (NoSuchElementException& nsee) {
        throw Error(nsee.toString());
    }
}


////////////////////////////////////////////////////////////////////////////////
// Execute the action.
////////////////////////////////////////////////////////////////////////////////
void ActionInitSimpleQuest::execute(Creature* pCreature1, Creature* pCreature2)

{
    Assert(pCreature1 != NULL);
    Assert(pCreature1->isNPC());

    NPC* pNPC = dynamic_cast<NPC*>(pCreature1);
    SimpleQuestInfoManager* pQIM = new SimpleQuestInfoManager(pNPC);

    pQIM->load(pNPC->getName());
    pNPC->setQuestInfoManager(pQIM);

    SimpleQuestRewardManager* pRIM = new SimpleQuestRewardManager(pNPC);

    pRIM->load(pNPC->getName());
    pNPC->setRewardClassInfoManager(pRIM);
}


////////////////////////////////////////////////////////////////////////////////
// get debug string
////////////////////////////////////////////////////////////////////////////////
string ActionInitSimpleQuest::toString() const

{
    StringStream msg;
    msg << "ActionInitSimpleQuest(" << ")";

    return msg.toString();
}
