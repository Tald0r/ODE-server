////////////////////////////////////////////////////////////////////////////////
// Filename    : ConditionCanEnterGDRLair.cpp
// Written By  :
// Description :
////////////////////////////////////////////////////////////////////////////////

#include "ConditionCanEnterGDRLair.h"

#include "Effect.h"
#include "GCSystemMessage.h"
#include "GDRLairManager.h"
#include "Player.h"

////////////////////////////////////////////////////////////////////////////////
// is satisfied?
////////////////////////////////////////////////////////////////////////////////
bool ConditionCanEnterGDRLair::isSatisfied(Creature* pCreature1, Creature* pCreature2, void* pParam) const

{
    Assert(pCreature2 != NULL);
    Assert(pCreature2->isPC());


    if (!GDRLairManager::Instance().canEnter()) {
        GCSystemMessage gcSystemMessage;
        gcSystemMessage.setMessage("The Gilles de Rais lair is not open yet.");
        pCreature2->getPlayer()->sendPacket(&gcSystemMessage);
        return false;
    }

    if (!pCreature2->isFlag(Effect::EFFECT_CLASS_CAN_ENTER_GDR_LAIR)) {
        GCSystemMessage gcSystemMessage;
        gcSystemMessage.setMessage("You must destroy the Hymez Frozen Statue on floor 2 of the Lust Tower.");
        pCreature2->getPlayer()->sendPacket(&gcSystemMessage);
        return false;
    }

    return true;
}

////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////
void ConditionCanEnterGDRLair::read(PropertyBuffer& propertyBuffer)

{}

////////////////////////////////////////////////////////////////////////////////
// get debug string
////////////////////////////////////////////////////////////////////////////////
string ConditionCanEnterGDRLair::toString() const

{
    StringStream msg;
    msg << "ConditionCanEnterGDRLair(" << ")";
    return msg.toString();
}
