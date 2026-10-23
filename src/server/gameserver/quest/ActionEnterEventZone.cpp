////////////////////////////////////////////////////////////////////////////////
// Filename    : ActionEnterEventZone.cpp
// Written By  :
// Description :
////////////////////////////////////////////////////////////////////////////////

#include "ActionEnterEventZone.h"

#include <stdio.h>

#include "CastleInfoManager.h"
#include "EventZoneInfo.h"
#include "GCModifyInformation.h"
#include "GCMoveOK.h"
#include "GCNPCResponse.h"
#include "GCSystemMessage.h"
#include "GCUpdateInfo.h"
#include "GameContext.h"
#include "GamePlayer.h"
#include "IncomingPlayerManager.h"
#include "Ousters.h"
#include "PCOustersInfo2.h"
#include "PCSlayerInfo2.h"
#include "PCVampireInfo2.h"
#include "PacketUtil.h"
#include "PaySystem.h"
#include "Properties.h"
#include "Slayer.h"
#include "StringPool.h"
#include "StringStream.h"
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
void ActionEnterEventZone::read(PropertyBuffer& pb)

{
    try {
        m_EventID = pb.getPropertyInt("EventID");
    } catch (NoSuchElementException& nsee) {
        throw Error(nsee.toString());
    }
}

////////////////////////////////////////////////////////////////////////////////
// Execute the action.
////////////////////////////////////////////////////////////////////////////////
void ActionEnterEventZone::execute(Creature* pNPC, Creature* pCreature)

{
    __BEGIN_DEBUG

    StringPool& strings = context().strings();

    Assert(pCreature != NULL);
    Assert(pCreature->isPC());

    GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(pCreature->getPlayer());
    PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pCreature);

    Assert(pPC != NULL);

    EventZoneInfo* pEventZoneInfo =
        EventZoneInfoManager::Instance().getZoneEventInfo(m_EventID)->getCurrentEventZoneInfo();
    if (pEventZoneInfo == NULL) {
        transportCreature(pPC, pPC->getZone()->getZoneID(), pPC->getX(), pPC->getY(), true);

        GCSystemMessage gcSystemMessage;
        gcSystemMessage.setMessage(strings.getString(STRID_CANNOT_ENTER));
        pGamePlayer->sendPacket(&gcSystemMessage);

        return;
    }

    if (!pEventZoneInfo->canEnter()) {
        transportCreature(pPC, pPC->getZone()->getZoneID(), pPC->getX(), pPC->getY(), true);

        GCSystemMessage gcSystemMessage;
        gcSystemMessage.setMessage(strings.getString(STRID_PKZONE_PC_LIMITED));
        pGamePlayer->sendPacket(&gcSystemMessage);

        return;
    }

    bool bTransport = true;

    if (bTransport) {
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

            // Dismount the sylph if the creature is riding one.
            if (pOusters->isFlag(Effect::EFFECT_CLASS_SUMMON_SYLPH)) {
                Effect* pEffect = pOusters->findEffect(Effect::EFFECT_CLASS_SUMMON_SYLPH);
                if (pEffect != NULL)
                    pEffect->setDeadline(0);
            }
        }

        if (pPC->isVampire()) {
            Vampire* pVampire = dynamic_cast<Vampire*>(pPC);
            Assert(pVampire != NULL);

            if (pVampire->isFlag(Effect::EFFECT_CLASS_TRANSFORM_TO_BAT)) {
                addUntransformCreature(pVampire->getZone(), pVampire, true);
            }
        }

        transportCreature(pCreature, pEventZoneInfo->getZoneID(), pEventZoneInfo->getEnterX(),
                          pEventZoneInfo->getEnterY(), true);
    } else {
        GCSystemMessage gcSystemMessage;
        gcSystemMessage.setMessage(strings.getString(STRID_CANNOT_ENTER));
        pGamePlayer->sendPacket(&gcSystemMessage);
    }

    __END_DEBUG
}


////////////////////////////////////////////////////////////////////////////////
// get debug string
////////////////////////////////////////////////////////////////////////////////
string ActionEnterEventZone::toString() const

{
    StringStream msg;
    msg << "ActionEnterEventZone(" << ")";
    return msg.toString();
}
