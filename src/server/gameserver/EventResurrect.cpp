//////////////////////////////////////////////////////////////////////////////
// Filename    : EventResurrect.cpp
// Written by  : Reiot
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "EventResurrect.h"

#include "GCUpdateInfo.h"
#include "GamePlayer.h"
#include "IncomingPlayerManager.h"
#include "Ousters.h"
#include "PCSlayerInfo2.h"
#include "PCVampireInfo2.h"
#include "PacketUtil.h"
#include "PlayerStatus.h"
#include "Slayer.h"
#include "TimeManager.h"
#include "Vampire.h"
#include "Zone.h"
#include "ZoneGroup.h"
#include "ZoneGroupManager.h"
#include "ZoneInfo.h"
#include "ZoneInfoManager.h"
#include "ZonePlayerManager.h"

//////////////////////////////////////////////////////////////////////////////
// class EventResurrect member methods
//////////////////////////////////////////////////////////////////////////////

EventResurrect::EventResurrect(GamePlayer* pGamePlayer)

    : Event(pGamePlayer) {
    //	m_pResurrectZone = NULL;
    //	m_X = m_Y = 0;
}

EventResurrect::~EventResurrect()

{}

void EventResurrect::activate()

{
    __BEGIN_TRY
    __BEGIN_DEBUG

    Assert(m_pGamePlayer != NULL);

    Creature* pDeadPC = m_pGamePlayer->getCreature();

    Assert(pDeadPC != NULL);

    // If the player died while hidden, clear hide.
    pDeadPC->removeFlag(Effect::EFFECT_CLASS_HIDE);

    // Change the move mode.
    if (pDeadPC->isVampire() && pDeadPC->isFlag(Effect::EFFECT_CLASS_TRANSFORM_TO_BAT)) {
        pDeadPC->setMoveMode(Creature::MOVE_MODE_FLYING);
    } else {
        pDeadPC->setMoveMode(Creature::MOVE_MODE_WALKING);
    }

    // Refill HP.
    if (pDeadPC->isSlayer()) {
        Slayer* pSlayer = dynamic_cast<Slayer*>(pDeadPC);
        pSlayer->setHP(pSlayer->getHP(ATTR_MAX), ATTR_CURRENT);
    } else if (pDeadPC->isVampire()) {
        Vampire* pVampire = dynamic_cast<Vampire*>(pDeadPC);
        pVampire->setHP(pVampire->getHP(ATTR_MAX), ATTR_CURRENT);
    } else if (pDeadPC->isOusters()) {
        Ousters* pOusters = dynamic_cast<Ousters*>(pDeadPC);
        pOusters->setHP(pOusters->getHP(ATTR_MAX), ATTR_CURRENT);
    }

    // The new zone is not set here.
    Zone* pOldZone = pDeadPC->getZone();
    Assert(pOldZone != NULL);

    try {
        // Reserve its outgoing entry before releasing the zone's table owner.
        pOldZone->getZoneGroup()->getZonePlayerManager()->moveToOutgoing(m_pGamePlayer);

        // Setting it here is what keeps the Save event from running in the IPM.
        m_pGamePlayer->setPlayerStatus(GPS_WAITING_FOR_CG_READY);

    } catch (NoSuchElementException& t) {
        filelog("eventRessurect.txt", "%s-%s", t.toString().c_str(), pDeadPC->getName().c_str());
        cerr << "EventResurrect::activate() : NoSuchElementException" << endl;
        // throw Error("The player does not exist in the zone.");
        //  It must have disappeared somehow.
        //  Ignore it and simply carry on.
        //  by sigi. 2002.11.25
    }

    // killCreature set the zone at the time of death, so it can just be taken as is.

    // This is handled in ZonePlayerManager's heartbeat.
    // Commented out.
    // pDeadPC->registerObject();

    __END_DEBUG
    __END_CATCH
}

string EventResurrect::toString() const

{
    StringStream msg;
    msg << "EventResurrect(" << ")";
    return msg.toString();
}
