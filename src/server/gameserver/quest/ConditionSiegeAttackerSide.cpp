////////////////////////////////////////////////////////////////////////////////
// Filename    : ConditionSiegeAttackerSide.cpp
// Written By  :
// Description :
////////////////////////////////////////////////////////////////////////////////

#include "ConditionSiegeAttackerSide.h"

#include "Effect.h"
#include "GCSystemMessage.h"
#include "GDRLairManager.h"
#include "Player.h"

////////////////////////////////////////////////////////////////////////////////
// is satisfied?
////////////////////////////////////////////////////////////////////////////////
bool ConditionSiegeAttackerSide::isSatisfied(Creature* pCreature1, Creature* pCreature2, void* pParam) const

{
    Assert(pCreature2 != NULL);
    Assert(pCreature2->isPC());

    if (!pCreature2->isFlag(Effect::EFFECT_CLASS_SIEGE_ATTACKER_1) &&
        !pCreature2->isFlag(Effect::EFFECT_CLASS_SIEGE_ATTACKER_2) &&
        !pCreature2->isFlag(Effect::EFFECT_CLASS_SIEGE_ATTACKER_3) &&
        !pCreature2->isFlag(Effect::EFFECT_CLASS_SIEGE_ATTACKER_4) &&
        !pCreature2->isFlag(Effect::EFFECT_CLASS_SIEGE_ATTACKER_5)) {
        return false;
    }

    return true;
}

////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////
void ConditionSiegeAttackerSide::read(PropertyBuffer& propertyBuffer)

{}

////////////////////////////////////////////////////////////////////////////////
// get debug string
////////////////////////////////////////////////////////////////////////////////
string ConditionSiegeAttackerSide::toString() const

{
    StringStream msg;
    msg << "ConditionSiegeAttackerSide(" << ")";
    return msg.toString();
}
