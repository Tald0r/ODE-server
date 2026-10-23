////////////////////////////////////////////////////////////////////////////////
// Filename    : ActionEnterQuestZone.cpp
// Description :
////////////////////////////////////////////////////////////////////////////////

#include "ActionEnterQuestZone.h"

#include "DynamicZoneAlterOfBlood.h"
#include "DynamicZoneGroup.h"
#include "DynamicZoneInfo.h"
#include "DynamicZoneManager.h"
#include "GCNPCResponse.h"
#include "GCSystemMessage.h"
#include "GQuestManager.h"
#include "GameContext.h"
#include "GamePlayer.h"
#include "PacketUtil.h"
#include "PlayerCreature.h"
#include "Properties.h"
#include "StringPool.h"
#include "Zone.h"
#include "ZoneGroup.h"
#include "ZoneGroupManager.h"
#include "ZoneInfo.h"
#include "ZoneInfoManager.h"
#include "ZoneUtil.h"


////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
void ActionEnterQuestZone::read(PropertyBuffer& pb)

{
    try {
        m_ZoneID = pb.getPropertyInt("ZoneID");
        m_X = pb.getPropertyInt("X");
        m_Y = pb.getPropertyInt("Y");
    } catch (NoSuchElementException& nsee) {
        throw Error(nsee.toString());
    }
}

////////////////////////////////////////////////////////////////////////////////
// Execute the action.
////////////////////////////////////////////////////////////////////////////////
void ActionEnterQuestZone::execute(Creature* pNPC, Creature* pCreature)

{
    __BEGIN_DEBUG

    Assert(pCreature != NULL);
    Assert(pCreature->isPC());

    GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(pCreature->getPlayer());
    PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pCreature);

    Assert(pPC != NULL);

    bool bTransport = true;

    if (bTransport) {
        // Check whether the target is a dynamic zone.
        int targetDynamicZoneType = context().dynamicZoneInfos().getDynamicZoneTypeByZoneID(m_ZoneID);

        if (targetDynamicZoneType != DYNAMIC_ZONE_MAX) {
            // The target is a dynamic zone.
            DynamicZoneGroup* pDynamicZoneGroup = context().dynamicZones().getDynamicZoneGroup(targetDynamicZoneType);
            Assert(pDynamicZoneGroup != NULL);

            DynamicZone* pDynamicZone = pDynamicZoneGroup->getAvailableDynamicZone();
            Assert(pDynamicZone != NULL);

            transportCreature(pCreature, pDynamicZone->getZoneID(), m_X, m_Y, true);

            if (targetDynamicZoneType == DYNAMIC_ZONE_ALTER_OF_BLOOD) {
                DynamicZoneAlterOfBlood* pAlterOfBlood = dynamic_cast<DynamicZoneAlterOfBlood*>(pDynamicZone);
                Assert(pAlterOfBlood != NULL);
                pAlterOfBlood->setRace(pPC->getRace());
            }
        } else {
            // The target is not a dynamic zone.
            transportCreature(pCreature, m_ZoneID, m_X, m_Y, true);
        }
    } else {
        GCNPCResponse response;
        response.setCode(NPC_RESPONSE_QUIT_DIALOGUE);
        pGamePlayer->sendPacket(&response);
    }

    __END_DEBUG
}


////////////////////////////////////////////////////////////////////////////////
// get debug string
////////////////////////////////////////////////////////////////////////////////
string ActionEnterQuestZone::toString() const

{
    StringStream msg;
    msg << "ActionEnterQuestZone(" << "ZoneID:" << (int)m_ZoneID << ",X:" << (int)m_X << ",Y:" << (int)m_Y << ")";
    return msg.toString();
}
