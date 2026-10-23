////////////////////////////////////////////////////////////////////////////////
// Filename    : ConditionEnterMasterLair.cpp
// Written By  :
// Description :
////////////////////////////////////////////////////////////////////////////////

#include "ConditionEnterMasterLair.h"

#include "DB.h"
#include "FlagSet.h"
#include "GameContext.h"
#include "GamePlayer.h"
#include "MasterLairManager.h"
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
bool ConditionEnterMasterLair::isSatisfied(Creature* pCreature1, Creature* pCreature2, void* pParam) const

{
    if (!de::gameContext().variables().isActiveMasterLair()) {
        return false;
    }

    Assert(pCreature2 != NULL);
    Assert(pCreature2->isPC());


    bool bPayPlay = false;

    GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(pCreature2->getPlayer());
    Assert(pGamePlayer != NULL);

    bPayPlay = true;

    // Only someone who has paid can enter the master lair.
    if (bPayPlay) {
        // Find the zone.
        Zone* pZone = getZoneByZoneID(m_TargetZoneID);
        Assert(pZone != NULL);

        // If it is not a master lair, there is nothing to check.
        if (!pZone->isMasterLair()) {
            return true;
        }

        MasterLairManager* pMasterLairManager = pZone->getMasterLairManager();
        Assert(pMasterLairManager != NULL);

        if (pMasterLairManager->enterCreature(pCreature2)) {
            // Entry allowed
            return true;
        }
    }

    return false;
}

////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////
void ConditionEnterMasterLair::read(PropertyBuffer& propertyBuffer)

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
string ConditionEnterMasterLair::toString() const

{
    StringStream msg;
    msg << "ConditionEnterMasterLair(" << "TargetZoneID:" << (int)m_TargetZoneID << ")";
    return msg.toString();
}
