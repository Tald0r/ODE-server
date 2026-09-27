////////////////////////////////////////////////////////////////////////////////
// Filename    : ActionEnterCastleWithFee.cpp
// Written By  :
// Description :
////////////////////////////////////////////////////////////////////////////////

#include "ActionEnterCastleWithFee.h"

#include <stdio.h>

#include "CastleInfoManager.h"
#include "GCModifyInformation.h"
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
void ActionEnterCastleWithFee::read(PropertyBuffer& pb)

{
    __BEGIN_TRY

    try {
        m_ZoneID = pb.getPropertyInt("ZoneID");
        m_X = pb.getPropertyInt("X");
        m_Y = pb.getPropertyInt("Y");
    } catch (NoSuchElementException& nsee) {
        throw Error(nsee.toString());
    }

    __END_CATCH
}

////////////////////////////////////////////////////////////////////////////////
// Execute the action.
////////////////////////////////////////////////////////////////////////////////
void ActionEnterCastleWithFee::execute(Creature* pNPC, Creature* pCreature)

{
    __BEGIN_TRY
    __BEGIN_DEBUG

    Assert(pCreature != NULL);
    Assert(pCreature->isPC());

    GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(pCreature->getPlayer());
    PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pCreature);

    Assert(pPC != NULL);

    CastleInfoManager& castleInfos = context().castleInfos();

    bool bTransport = true;

    if (bTransport) {
        if (castleInfos.isPossibleEnter(m_ZoneID, pPC)) {
            Gold_t fee = castleInfos.getEntranceFee(m_ZoneID, pPC);
            Gold_t remain = pPC->getGold();

            if (remain < fee) {
                static char buf[200];
                snprintf(buf, sizeof(buf), context().strings().c_str(STRID_NOT_ENOUGH_ENTRANCE_FEE), (int)fee);
                // Not enough gold.
                GCSystemMessage message;
                message.setType(SYSTEM_MESSAGE_HOLY_LAND);
                message.setMessage(buf);
                pGamePlayer->sendPacket(&message);

                bTransport = false;
            } else {
                if (fee > 0) {
                    // Pay the entrance fee.
                    pPC->decreaseGoldEx(fee);
                    castleInfos.increaseTaxBalance(m_ZoneID, fee);

                    GCModifyInformation gcMI;
                    gcMI.addLongData(MODIFY_GOLD, pPC->getGold());
                    pGamePlayer->sendPacket(&gcMI);
                }
            }
        } else {
            GCSystemMessage message;
            message.setType(SYSTEM_MESSAGE_HOLY_LAND);
            message.setMessage(context().strings().getString(STRID_CANNOT_ENTER));
            pGamePlayer->sendPacket(&message);

            bTransport = false;
        }
    }

    if (bTransport) {
        transportCreature(pCreature, m_ZoneID, m_X, m_Y, true);
    } else {
        GCNPCResponse response;
        response.setCode(NPC_RESPONSE_QUIT_DIALOGUE);
        pGamePlayer->sendPacket(&response);
    }

    __END_DEBUG
    __END_CATCH
}


////////////////////////////////////////////////////////////////////////////////
// get debug string
////////////////////////////////////////////////////////////////////////////////
string ActionEnterCastleWithFee::toString() const

{
    __BEGIN_TRY

    StringStream msg;
    msg << "ActionEnterCastleWithFee(" << "ZoneID:" << (int)m_ZoneID << ",X:" << (int)m_X << ",Y:" << (int)m_Y << ")";
    return msg.toString();

    __END_CATCH
}
