////////////////////////////////////////////////////////////////////////////////
// Filename    : ConditionEnterCastle.cpp
// Written By  :
// Description :
////////////////////////////////////////////////////////////////////////////////

#include "ConditionEnterCastle.h"

#include "CastleInfoManager.h"
#include "DB.h"
#include "FlagSet.h"
#include "GameContext.h"
#include "GamePlayer.h"
#include "PacketUtil.h"
#include "PaySystem.h"
#include "PlayerCreature.h"
#include "VariableManager.h"
#include "Zone.h"
#include "ZoneGroupManager.h"
#include "ZoneInfoManager.h"
#include "ZoneUtil.h"

////////////////////////////////////////////////////////////////////////////////
// is satisfied?
////////////////////////////////////////////////////////////////////////////////
bool ConditionEnterCastle::isSatisfied(Creature* pCreature1, Creature* pCreature2, void* pParam) const

{
    Assert(pCreature2 != NULL);
    Assert(pCreature2->isPC());


    bool bPayPlay = false;

    GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(pCreature2->getPlayer());
    Assert(pGamePlayer != NULL);

    bPayPlay = true;

    // The pay-to-play gate is always open: bPayPlay is set true just above.
    if (bPayPlay) {
        // Find the zone.
        Zone* pZone = getZoneByZoneID(m_TargetZoneID);
        Assert(pZone != NULL);

        // If it is not a castle, there is nothing to check.
        if (!pZone->isCastle()) {
            return true;
        }

        // The castle's own portal rules decide whether this player may enter.
        PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pCreature2);

        return de::gameContext().castleInfos().canPortalActivate(m_TargetZoneID, pPC);
    }

    return false;
}

////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////
void ConditionEnterCastle::read(PropertyBuffer& propertyBuffer)

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
string ConditionEnterCastle::toString() const

{
    StringStream msg;
    msg << "ConditionEnterCastle(" << "TargetZoneID:" << (int)m_TargetZoneID << ")";
    return msg.toString();
}
