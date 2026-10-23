////////////////////////////////////////////////////////////////////////////////
// Filename    : ActionEnterGDRLair.cpp
// Written By  :
// Description :
////////////////////////////////////////////////////////////////////////////////

#include "ActionEnterGDRLair.h"

#include <stdio.h>

#include "CastleInfoManager.h"
#include "GCModifyInformation.h"
#include "GCMoveOK.h"
#include "GCNPCResponse.h"
#include "GCSystemMessage.h"
#include "GCUpdateInfo.h"
#include "GDRLairManager.h"
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
void ActionEnterGDRLair::read(PropertyBuffer& pb)

{
    try {
        m_PortalID = pb.getPropertyInt("PortalID");
    } catch (NoSuchElementException& nsee) {
        throw Error(nsee.toString());
    }
}

////////////////////////////////////////////////////////////////////////////////
// Execute the action.
////////////////////////////////////////////////////////////////////////////////
void ActionEnterGDRLair::execute(Creature* pNPC, Creature* pCreature)

{
    __BEGIN_DEBUG

    Assert(pCreature != NULL);
    Assert(pCreature->isPC());

    GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(pCreature->getPlayer());
    PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pCreature);

    Assert(pPC != NULL);

    ZoneID_t zoneID;
    ZoneCoord_t X, Y;

    if (m_PortalID == GDRLairManager::Instance().getCorrectPortal()) {
        zoneID = 1412;
        X = 142;
        Y = 169;
    } else {
        zoneID = 1411;
        X = 125;
        Y = 58;
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

        Effect* pEffect = pPC->findEffect(Effect::EFFECT_CLASS_CAN_ENTER_GDR_LAIR);
        if (pEffect != NULL)
            pEffect->setDeadline(0);

        transportCreature(pCreature, zoneID, X, Y, true);
    } else {
        GCSystemMessage gcSystemMessage;
        gcSystemMessage.setMessage(context().strings().getString(STRID_CANNOT_ENTER));
        pGamePlayer->sendPacket(&gcSystemMessage);
    }

    __END_DEBUG
}


////////////////////////////////////////////////////////////////////////////////
// get debug string
////////////////////////////////////////////////////////////////////////////////
string ActionEnterGDRLair::toString() const

{
    StringStream msg;
    msg << "ActionEnterGDRLair(" << ")";
    return msg.toString();
}
