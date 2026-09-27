//----------------------------------------------------------------------
//
// Filename    : EffectShutDown.cpp
// Written by  : elca
//
//----------------------------------------------------------------------

// include files
#include "EffectShutDown.h"

#include <stdio.h>

#include "Assert.h"
#include "ClientManager.h"
#include "Corpse.h"
#include "EventShutdown.h"
#include "GCSystemMessage.h"
#include "GameContext.h"
#include "StringPool.h"
#include "Zone.h"
#include "ZoneGroupManager.h"
#include "ZoneInfoManager.h"
#include "ZonePlayerManager.h"
#include "repository/ZoneInfoRepository.h"
#include "signal.h"
#include "unistd.h"


//----------------------------------------------------------------------
// constructor
//----------------------------------------------------------------------
EffectShutDown::EffectShutDown()

    {__BEGIN_TRY

         __END_CATCH}

//----------------------------------------------------------------------
// constructor
//----------------------------------------------------------------------
EffectShutDown::EffectShutDown(Zone* pZone, ZoneCoord_t x, ZoneCoord_t y, Corpse* pCorpse, Turn_t delay)

    : Effect(pZone, x, y, pCorpse, delay) {
    __BEGIN_TRY

    Assert(getZone() != NULL);
    Assert(getTarget() != NULL);

    __END_CATCH
}


//----------------------------------------------------------------------
// destructor
//----------------------------------------------------------------------
EffectShutDown::~EffectShutDown()

{
    __BEGIN_TRY
    __END_CATCH_NO_RETHROW
}

void EffectShutDown::affect()

{
    __BEGIN_TRY

    Creature* pCreature = dynamic_cast<Creature*>(m_pTarget);

    affect(pCreature);

    __END_CATCH
}

//----------------------------------------------------------------------
// affect
//----------------------------------------------------------------------
void EffectShutDown::affect(Creature* pCreature)

{
    __BEGIN_TRY

    Timeval nextTime = getNextTime();
    Timeval deadLine = getDeadline();
    Turn_t RemainTime = deadLine.tv_sec - nextTime.tv_sec;

    char msg[80];
    snprintf(msg, sizeof(msg), de::gameContext().strings().c_str(STRID_SERVER_SHUT_DOWN_COUNT_DOWN), (int)RemainTime);

    GCSystemMessage gcSystemMessage;
    gcSystemMessage.setMessage(msg);

    setNextTime(m_Delay);


    int maxZoneGroupID = 0;
    if (!defaultZoneInfoRepository().loadMaxZoneGroupID(maxZoneGroupID)) {
        throw Error("Critical Error : ZoneGroupInfo table is empty.");
    }

    BYTE GroupCount = maxZoneGroupID + 1;

    for (int i = 1; i < GroupCount; i++) {
        ZoneGroup* pZoneGroup;

        try {
            pZoneGroup = de::gameContext().zoneGroups().getZoneGroup(i);
        } catch (NoSuchElementException&) {
            throw Error("Critical Error : ZoneInfoManager has no such zone group.");
        }

        ZonePlayerManager* pZonePlayerManager = pZoneGroup->getZonePlayerManager();
        pZonePlayerManager->broadcastPacket(&gcSystemMessage);
    }


    __END_CATCH
}


//----------------------------------------------------------------------
// affect to target
// This effect is not bound to a tile, so affect() is never called.
// The target is set in the constructor and this overload does nothing.
//----------------------------------------------------------------------
void EffectShutDown::affect(Zone* pZone, ZoneCoord_t x, ZoneCoord_t y, Object* pTarget)

{
    __BEGIN_TRY

    throw UnsupportedError();

    __END_CATCH
}

//--------------------------------------------------------------------
// unaffect()
//--------------------------------------------------------------------
void EffectShutDown::unaffect()

{
    __BEGIN_TRY

    //	StringStream msg;

    // msg << "The server is shutting down.";

    GCSystemMessage gcSystemMessage;
    gcSystemMessage.setMessage(de::gameContext().strings().getString(STRID_SERVER_SHUT_DOWN));


    int maxZoneGroupID = 0;
    if (!defaultZoneInfoRepository().loadMaxZoneGroupID(maxZoneGroupID)) {
        throw Error("Critical Error : ZoneGroupInfo table is empty.");
    }

    BYTE GroupCount = maxZoneGroupID + 1;

    for (int i = 1; i < GroupCount; i++) {
        ZoneGroup* pZoneGroup;

        try {
            pZoneGroup = de::gameContext().zoneGroups().getZoneGroup(i);
        } catch (NoSuchElementException&) {
            throw Error("Critical Error : ZoneInfoManager has no such zone group.");
        }

        ZonePlayerManager* pZonePlayerManager = pZoneGroup->getZonePlayerManager();
        pZonePlayerManager->broadcastPacket(&gcSystemMessage);
    }


    // kill(getpid() , 9);
    EventShutdown* pEventShutdown = new EventShutdown(NULL);
    pEventShutdown->setDeadline(0);

    de::gameContext().clients().addEvent(pEventShutdown);

    __END_CATCH
}

//--------------------------------------------------------------------
// unaffect
//--------------------------------------------------------------------
void EffectShutDown::unaffect(Creature* pCreature)

{
    __BEGIN_TRY
    __END_CATCH
}


//----------------------------------------------------------------------
// remove effect from target
//----------------------------------------------------------------------
void EffectShutDown::unaffect(Zone* pZone, ZoneCoord_t x, ZoneCoord_t y, Object* pTarget)

    {__BEGIN_TRY

         __END_CATCH}


//----------------------------------------------------------------------
// get debug string
//----------------------------------------------------------------------
string EffectShutDown::toString() const

{
    StringStream msg;

    if (m_pZone) {
        msg << "EffectShutDown(" << "ZoneID:" << (int)m_pZone->getZoneID() << ",X:" << (int)getX()
            << ",Y:" << (int)getY();
    }

    if (m_pTarget) {
        msg << ",Target:" << m_pTarget->toString();
    } else {
        msg << ",Target:NULL";
    }

    msg << ",Deadline:" << (int)m_Deadline.tv_sec << "." << (int)m_Deadline.tv_usec << ")";

    return msg.toString();
}
