////////////////////////////////////////////////////////////////////////////////
// Filename    : ConditionEnterHolyLand.cpp
// Written By  :
// Description :
////////////////////////////////////////////////////////////////////////////////

#include "ConditionEnterHolyLand.h"

#include "DB.h"
#include "FlagSet.h"
#include "GameContext.h"
#include "GamePlayer.h"
#include "PacketUtil.h"
#include "PaySystem.h"
#include "PlayerCreature.h"
#include "VariableManager.h"
#include "WarSystem.h"
#include "Zone.h"
#include "ZoneGroupManager.h"
#include "ZoneInfoManager.h"
#include "ZoneUtil.h"

////////////////////////////////////////////////////////////////////////////////
// is satisfied?
////////////////////////////////////////////////////////////////////////////////
bool ConditionEnterHolyLand::isSatisfied(Creature* pCreature1, Creature* pCreature2, void* pParam) const

{
    Assert(pCreature2 != NULL);
    Assert(pCreature2->isPC());


    bool bPayPlay = false;

    GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(pCreature2->getPlayer());
    Assert(pGamePlayer != NULL);

    bPayPlay = true;

    // The pay-to-play gate is always open: bPayPlay is set true just above.
    if (bPayPlay) {
        // During a race war, if the number of participants is limited.
        if (de::gameContext().warSystem().hasActiveRaceWar() &&
            de::gameContext().variables().isActiveRaceWarLimiter()) {
            Zone* pZone = getZoneByZoneID(m_TargetZoneID);
            Assert(pZone != NULL);

            // Only a holy land is limited; any other zone is entered freely.
            if (!pZone->isHolyLand()) {
                return true;
            }

            PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pCreature2);

            // The PC must have joined the war.
            return pPC->isFlag(Effect::EFFECT_CLASS_RACE_WAR_JOIN_TICKET);
        }

        // When no war is going on, entry is free.
        return true;
    }

    return false;
}

////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////
void ConditionEnterHolyLand::read(PropertyBuffer& propertyBuffer)

{
    try {
        // read turn
        m_TargetZoneID = propertyBuffer.getPropertyInt("TargetZoneID");
    } catch (NoSuchElementException& nsee) {
        throw Error(nsee.toString());
    }
}

////////////////////////////////////////////////////////////////////////////////
// get debug string
////////////////////////////////////////////////////////////////////////////////
string ConditionEnterHolyLand::toString() const

{
    StringStream msg;
    msg << "ConditionEnterHolyLand(" << "TargetZoneID:" << (int)m_TargetZoneID << ")";
    return msg.toString();
}
