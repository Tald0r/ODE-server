//////////////////////////////////////////////////////////////////////////////
// Filename    : EventTransport.cpp
// Written by  : Reiot
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "EventTransport.h"

#include <stdio.h>

#include "CreatureUtil.h"
#include "GCSystemMessage.h"
#include "GameContext.h"
#include "GamePlayer.h"
#include "PlayerCreature.h"
#include "PlayerStatus.h"
#include "RelicUtil.h"
#include "StringPool.h"
#include "Zone.h"
#include "ZoneGroup.h"
#include "ZonePlayerManager.h"
#include "ZoneUtil.h"

//////////////////////////////////////////////////////////////////////////////
// class EventTransport member methods
//////////////////////////////////////////////////////////////////////////////

EventTransport::EventTransport(GamePlayer* pGamePlayer)

    : Event(pGamePlayer) {
    //	m_pTransportZone = NULL;
    //	m_X = m_Y = 0;
}

EventTransport::~EventTransport()

{}

//----------------------------------------------------------------------
// affect
//----------------------------------------------------------------------
void EventTransport::sendMessage()

{
    __BEGIN_TRY

    Assert(m_pGamePlayer != NULL);
    Creature* pCreature = m_pGamePlayer->getCreature();

    Timeval currentTime;
    getCurrentTime(currentTime);

    Turn_t RemainTime = max(0, (int)(m_Deadline.tv_sec - currentTime.tv_sec));

    //	StringStream msg;
    //	msg << (int)RemainTime << " seconds until the move to " << m_ZoneName << ".";

    // Room for the longest message a GCSystemMessage carries.
    char msg[de::wire::kMaxByteStringLength + 1];
    snprintf(msg, sizeof(msg), de::gameContext().strings().c_str(STRID_TRANSPORT_CREATURE), (int)RemainTime,
             m_ZoneName.c_str());

    string sMsg(msg);

    GCSystemMessage gcSystemMessage;
    gcSystemMessage.setMessage(sMsg);
    pCreature->getPlayer()->sendPacket(&gcSystemMessage);

    // setNextTime( m_MessageTick );

    __END_CATCH
}


void EventTransport::activate()

{
    __BEGIN_TRY
    __BEGIN_DEBUG


    Assert(m_pGamePlayer != NULL);

    Creature* pCreature = m_pGamePlayer->getCreature();

    Zone* pOldZone = pCreature->getZone();
    Assert(pOldZone != NULL);

    PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pCreature);
    Assert(pPC != NULL);

    bool bSendPacket = true;
    dropRelicToZone(pPC, bSendPacket);
    dropFlagToZone(pPC, bSendPacket);
    dropSweeperToZone(pPC, bSendPacket);

    // Remove from the zone.
    pOldZone->deleteCreature(pCreature, pCreature->getX(), pCreature->getY());

    // Set the zone to move to.
    pCreature->setNewZone(getZoneByZoneID(m_ZoneID));
    pCreature->setNewXY(m_X, m_Y);

    try {
        // Reserve its outgoing entry before releasing the zone's table owner.
        pOldZone->getZoneGroup()->getZonePlayerManager()->moveToOutgoing(m_pGamePlayer);

        // Setting this here keeps the Save event from running in the IPM.
        m_pGamePlayer->setPlayerStatus(GPS_WAITING_FOR_CG_READY);

    } catch (NoSuchElementException& t) {
        filelog("eventTransport.txt", "%s", t.toString().c_str());
        cerr << "EventTransport::activate() : NoSuchElementException" << endl;
        throw Error("The player is not in the zone.");
    }


    __END_DEBUG
    __END_CATCH
}

string EventTransport::toString() const

{
    StringStream msg;
    msg << "EventTransport(" << ")";
    return msg.toString();
}
