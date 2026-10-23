////////////////////////////////////////////////////////////////////////////////
// Filename    : ActionActivateMazeExit.cpp
// Written By  :
// Description :
////////////////////////////////////////////////////////////////////////////////

#include "ActionActivateMazeExit.h"

#include "EffectRefiniumTicket.h"
#include "GCMoveOK.h"
#include "GCNPCResponse.h"
#include "GCSystemMessage.h"
#include "GCUpdateInfo.h"
#include "GamePlayer.h"
#include "IncomingPlayerManager.h"
#include "PCSlayerInfo2.h"
#include "PCVampireInfo2.h"
#include "PacketUtil.h"
#include "PaySystem.h"
#include "PlayerCreature.h"
#include "Properties.h"
#include "Slayer.h"
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
void ActionActivateMazeExit::read(PropertyBuffer& pb)

{
    try {
        m_GoodZoneID = pb.getPropertyInt("GoodZoneID");
        m_GoodX = pb.getPropertyInt("GoodX");
        m_GoodY = pb.getPropertyInt("GoodY");
        m_BadZoneID = pb.getPropertyInt("BadZoneID");
        m_BadX = pb.getPropertyInt("BadX");
        m_BadY = pb.getPropertyInt("BadY");

        m_ExitID = pb.getPropertyInt("ExitID");
    } catch (NoSuchElementException& nsee) {
        throw Error(nsee.toString());
    }
}

////////////////////////////////////////////////////////////////////////////////
// Execute the action.
////////////////////////////////////////////////////////////////////////////////
void ActionActivateMazeExit::execute(Creature* pNPC, Creature* pCreature)

{
    __BEGIN_DEBUG

    Assert(pCreature != NULL);
    Assert(pCreature->isPC());

    // The maze exit lets the player through without the pay-zone check.
    PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pCreature);
    Assert(pPC != NULL);

    bool isCorrect = false;

    if (!pPC->isFlag(Effect::EFFECT_CLASS_REFINIUM_TICKET)) {
        filelog("EventBug.log", "In the maze without a Refinium ticket; logging out is the only way out.. : %s",
                pPC->getName().c_str());
    } else {
        EffectRefiniumTicket* pEffect =
            dynamic_cast<EffectRefiniumTicket*>(pPC->findEffect(Effect::EFFECT_CLASS_REFINIUM_TICKET));
        if (pEffect != NULL) {
            isCorrect = ((int)pEffect->getExit() == (int)m_ExitID);
            if (isCorrect)
                pEffect->setDeadline(0);
            else {
                pEffect->setPrevExitZoneID(pPC->getZoneID());
                pEffect->setPrevExitX(pPC->getX());
                pEffect->setPrevExitY(pPC->getY());
            }
        }
    }

    if (isCorrect) {
        transportCreature(pCreature, m_GoodZoneID, m_GoodX, m_GoodY, true);
    } else {
        transportCreature(pCreature, m_BadZoneID, m_BadX, m_BadY, true);
    }

    __END_DEBUG
}


////////////////////////////////////////////////////////////////////////////////
// get debug string
////////////////////////////////////////////////////////////////////////////////
string ActionActivateMazeExit::toString() const

{
    StringStream msg;
    msg << "ActionActivateMazeExit(" << "GoodZoneID:" << (int)m_GoodZoneID << ",GoodX:" << (int)m_GoodX
        << ",GoodY:" << (int)m_GoodY << ",BadZoneID:" << (int)m_BadZoneID << ",BadX:" << (int)m_BadX
        << ",BadY:" << (int)m_BadY << ",ExitID:" << (int)m_ExitID << ")";
    return msg.toString();
}
