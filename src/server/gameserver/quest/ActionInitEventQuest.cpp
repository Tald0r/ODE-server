////////////////////////////////////////////////////////////////////////////////
// Filename    : ActionInitEventQuest.cpp
// Written By  :
// Description :
////////////////////////////////////////////////////////////////////////////////

#include "ActionInitEventQuest.h"

#include "Creature.h"
#include "NPC.h"
#include "mission/EventQuestInfoManager.h"
#include "mission/SimpleQuestRewardManager.h"

////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////
void ActionInitEventQuest::read(PropertyBuffer& propertyBuffer)

{
    try {
    } catch (NoSuchElementException& nsee) {
        throw Error(nsee.toString());
    }
}


////////////////////////////////////////////////////////////////////////////////
// Execute the action.
////////////////////////////////////////////////////////////////////////////////
void ActionInitEventQuest::execute(Creature* pCreature1, Creature* pCreature2)

{
    Assert(pCreature1 != NULL);
    Assert(pCreature1->isNPC());

    NPC* pNPC = dynamic_cast<NPC*>(pCreature1);
    EventQuestInfoManager* pQIM = new EventQuestInfoManager(pNPC);

    pQIM->load(pNPC->getName());
    pNPC->setQuestInfoManager(pQIM);

    SimpleQuestRewardManager* pRIM = new SimpleQuestRewardManager(pNPC);

    pRIM->load(pNPC->getName());
    pNPC->setRewardClassInfoManager(pRIM);
}


////////////////////////////////////////////////////////////////////////////////
// get debug string
////////////////////////////////////////////////////////////////////////////////
string ActionInitEventQuest::toString() const

{
    StringStream msg;
    msg << "ActionInitEventQuest(" << ")";

    return msg.toString();
}
