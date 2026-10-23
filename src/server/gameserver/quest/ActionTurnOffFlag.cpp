////////////////////////////////////////////////////////////////////////////////
// Filename    : ActionTurnOffFlag.cpp
// Written By  :
// Description :
////////////////////////////////////////////////////////////////////////////////

#include "ActionTurnOffFlag.h"

#include "FlagSet.h"
#include "PlayerCreature.h"

////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////
void ActionTurnOffFlag::read(PropertyBuffer& propertyBuffer)

{
    try {
        m_Index = propertyBuffer.getPropertyInt("Index");
    } catch (NoSuchElementException& nsee) {
        cout << nsee.toString() << endl;
        throw Error(nsee.toString());
    }
}


////////////////////////////////////////////////////////////////////////////////
// Execute the action.
////////////////////////////////////////////////////////////////////////////////
void ActionTurnOffFlag::execute(Creature* pCreature1, Creature* pCreature2)

{
    Assert(pCreature2 != NULL);
    Assert(pCreature2->isPC());

    PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pCreature2);
    FlagSet* pFlagSet = pPC->getFlagSet();

    if (pFlagSet->isOn(m_Index) == true) {
        pFlagSet->turnOff(m_Index);
        pFlagSet->save(pCreature2->getName());
    }
}


////////////////////////////////////////////////////////////////////////////////
// get debug string
////////////////////////////////////////////////////////////////////////////////
string ActionTurnOffFlag::toString() const

{
    StringStream msg;
    msg << "ActionTurnOffFlag(" << "Index:" << m_Index << ")";
    return msg.toString();
}
