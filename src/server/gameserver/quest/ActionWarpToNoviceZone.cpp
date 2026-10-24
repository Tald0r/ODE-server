////////////////////////////////////////////////////////////////////////////////
// Filename    : ActionWarpToNoviceZone.cpp
// Written By  :
// Description :
////////////////////////////////////////////////////////////////////////////////

#include "ActionWarpToNoviceZone.h"

#include "GCMoveOK.h"
#include "GCNPCResponse.h"
#include "GCSystemMessage.h"
#include "GCUpdateInfo.h"
#include "GameContext.h"
#include "GamePlayer.h"
#include "IncomingPlayerManager.h"
#include "PCSlayerInfo2.h"
#include "PCVampireInfo2.h"
#include "PacketUtil.h"
#include "PaySystem.h"
#include "Properties.h"
#include "Slayer.h"
#include "StringPool.h"
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
void ActionWarpToNoviceZone::read(PropertyBuffer& pb)

{
    try {
        m_NoviceZoneID = pb.getPropertyInt("NoviceZoneID");
        m_NoviceX = pb.getPropertyInt("NoviceX");
        m_NoviceY = pb.getPropertyInt("NoviceY");

        m_BeginnerZoneID = pb.getPropertyInt("BeginnerZoneID");
        m_BeginnerX = pb.getPropertyInt("BeginnerX");
        m_BeginnerY = pb.getPropertyInt("BeginnerY");
    } catch (NoSuchElementException& nsee) {
        throw Error(nsee.toString());
    }
}

////////////////////////////////////////////////////////////////////////////////
// Execute the action.
////////////////////////////////////////////////////////////////////////////////
void ActionWarpToNoviceZone::execute(Creature* pNPC, Creature* pCreature)

{
    __BEGIN_DEBUG

    Assert(pCreature != NULL);
    Assert(pCreature->isPC());

    if (!pCreature->isSlayer())
        return;

    Slayer* pSlayer = dynamic_cast<Slayer*>(pCreature);

    ZoneID_t ZoneID = pSlayer->getZoneID();
    ZoneCoord_t X = pSlayer->getX();
    ZoneCoord_t Y = pSlayer->getY();
    Attr_t totalAttr = pSlayer->getTotalAttr(ATTR_BASIC);

    GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(pCreature->getPlayer());

    bool bTransport = true;

    if (totalAttr <= 40) {
        ZoneID = m_NoviceZoneID;
        X = m_NoviceX;
        Y = m_NoviceY;
    } else if (totalAttr <= 60) {
        ZoneID = m_BeginnerZoneID;
        X = m_BeginnerX;
        Y = m_BeginnerY;
    } else {
        GCSystemMessage gcSystemMessage;
        gcSystemMessage.setMessage(context().strings().getString(STRID_NOT_BEGINNER));
        pGamePlayer->sendPacket(&gcSystemMessage);

        bTransport = false;
    }


    if (bTransport) {
        transportCreature(pCreature, ZoneID, X, Y, true);
    } else {
        if (pNPC != NULL) {
            GCNPCResponse response;
            response.setCode(NPC_RESPONSE_QUIT_DIALOGUE);
            pGamePlayer->sendPacket(&response);
        }
    }

    __END_DEBUG
}


////////////////////////////////////////////////////////////////////////////////
// get debug string
////////////////////////////////////////////////////////////////////////////////
string ActionWarpToNoviceZone::toString() const

{
    StringStream msg;
    msg << "ActionWarpToNoviceZone(" << "NoviceZoneID:" << (int)m_NoviceZoneID << ",NoviceX:" << (int)m_NoviceX
        << ",NoviceY:" << (int)m_NoviceY << "BeginnerZoneID:" << (int)m_BeginnerZoneID
        << ",BeginnerX:" << (int)m_BeginnerX << ",BeginnerY:" << (int)m_BeginnerY << ")";
    return msg.toString();
}
