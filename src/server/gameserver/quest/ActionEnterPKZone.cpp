////////////////////////////////////////////////////////////////////////////////
// Filename    : ActionEnterPKZone.cpp
// Written By  :
// Description :
////////////////////////////////////////////////////////////////////////////////

#include "ActionEnterPKZone.h"

#include <stdio.h>

#include "CastleInfoManager.h"
#include "GCModifyInformation.h"
#include "GCMoveOK.h"
#include "GCNPCResponse.h"
#include "GCSystemMessage.h"
#include "GCUpdateInfo.h"
#include "GQuestManager.h"
#include "GameContext.h"
#include "GamePlayer.h"
#include "IncomingPlayerManager.h"
#include "Ousters.h"
#include "PCOustersInfo2.h"
#include "PCSlayerInfo2.h"
#include "PCVampireInfo2.h"
#include "PKZoneInfoManager.h"
#include "PacketUtil.h"
#include "PaySystem.h"
#include "Properties.h"
#include "Slayer.h"
#include "StringPool.h"
#include "StringStream.h"
#include "SystemAvailabilitiesManager.h"
#include "Vampire.h"
#include "WeatherManager.h"
#include "Zone.h"
#include "ZoneGroup.h"
#include "ZoneGroupManager.h"
#include "ZoneInfo.h"
#include "ZoneInfoManager.h"
#include "ZonePlayerManager.h"
#include "ZoneUtil.h"

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
void ActionEnterPKZone::read(PropertyBuffer& pb)

{
    try {
        m_ZoneID = pb.getPropertyInt("ZoneID");
    } catch (NoSuchElementException& nsee) {
        throw Error(nsee.toString());
    }
}

////////////////////////////////////////////////////////////////////////////////
// Execute the action.
////////////////////////////////////////////////////////////////////////////////
void ActionEnterPKZone::execute(Creature* pNPC, Creature* pCreature)

{
    __BEGIN_DEBUG

    StringPool& strings = context().strings();

    Assert(pCreature != NULL);
    Assert(pCreature->isPC());

    SYSTEM_RETURN_IF_NOT(SYSTEM_PK_ZONE);

    GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(pCreature->getPlayer());
    PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pCreature);

    Assert(pPC != NULL);

    bool bTransport = true;

    if (bTransport) {
        if (pPC->isPLAYER() && !context().pkZoneInfos().canEnterPKZone(m_ZoneID))
            bTransport = false;
    }

    if (bTransport) {
        PKZoneInfo* pPKZoneInfo = context().pkZoneInfos().getPKZoneInfo(m_ZoneID);

        if (pPKZoneInfo == NULL) {
            GCSystemMessage gcSystemMessage;
            gcSystemMessage.setMessage(strings.getString(STRID_CANNOT_ENTER));
            pGamePlayer->sendPacket(&gcSystemMessage);
        }

        // Game masters enter without the head-count limit.
        if (!pPC->isPLAYER() || pPKZoneInfo->enterZone()) {
            if (pPC->isSlayer()) {
                Slayer* pSlayer = dynamic_cast<Slayer*>(pPC);
                Assert(pSlayer != NULL);

                // Get off the motorcycle if riding one.
                if (pSlayer->hasRideMotorcycle()) {
                    pSlayer->getOffMotorcycle();
                }
            }

            if (pPC->isOusters()) {
                Ousters* pOusters = dynamic_cast<Ousters*>(pPC);
                Assert(pOusters != NULL);

                // Get off the sylph if riding one.
                if (pOusters->isFlag(Effect::EFFECT_CLASS_SUMMON_SYLPH)) {
                    Effect* pEffect = pOusters->findEffect(Effect::EFFECT_CLASS_SUMMON_SYLPH);
                    if (pEffect != NULL)
                        pEffect->setDeadline(0);
                }
            }

            if (pNPC != NULL)
                pPC->getGQuestManager()->illegalWarp();
            transportCreature(pCreature, m_ZoneID, pPKZoneInfo->getEnterX(), pPKZoneInfo->getEnterY(), true);
        } else {
            GCSystemMessage gcSystemMessage;
            gcSystemMessage.setMessage(strings.getString(STRID_PKZONE_PC_LIMITED));
            pGamePlayer->sendPacket(&gcSystemMessage);
        }
    } else {
        GCSystemMessage gcSystemMessage;
        gcSystemMessage.setMessage(strings.getString(STRID_PKZONE_PC_LIMITED));
        pGamePlayer->sendPacket(&gcSystemMessage);
    }

    __END_DEBUG
}


////////////////////////////////////////////////////////////////////////////////
// get debug string
////////////////////////////////////////////////////////////////////////////////
string ActionEnterPKZone::toString() const

{
    StringStream msg;
    msg << "ActionEnterPKZone(" << "ZoneID:" << (int)m_ZoneID << ")";
    return msg.toString();
}
