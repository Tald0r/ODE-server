//----------------------------------------------------------------------
//
// Filename    : EffectRideMotorcycle.cpp
// Written by  : Reiot
//
//----------------------------------------------------------------------

// include files
#include "EffectRideMotorcycle.h"

#include "Assert.h"
#include "CGRideMotorCycle.h"
#include "GCDeleteObject.h"
#include "GamePlayer.h"
#include "Item.h"
#include "Slayer.h"
#include "Tile.h"
#include "Zone.h"
#include "ZoneUtil.h"

//----------------------------------------------------------------------
// constructor
//----------------------------------------------------------------------
EffectRideMotorcycle::EffectRideMotorcycle(Slayer* pOwner, Item* pMotorcycle, ZoneCoord_t motorX, ZoneCoord_t motorY)

    : Effect(pOwner->getZone(), motorX, motorY, pOwner, 0), m_MotorX(motorX), m_MotorY(motorY),
      m_OwnerObjectID(pOwner->getObjectID()), m_pMotorcycle(pMotorcycle) {
    Assert(getZone() != NULL);
    Assert(getTarget() != NULL);

    // Server-only Effect.
    m_bBroadcastingEffect = false;
}


//----------------------------------------------------------------------
// destructor
//----------------------------------------------------------------------
EffectRideMotorcycle::~EffectRideMotorcycle()

{}


//----------------------------------------------------------------------
// unaffect()
//----------------------------------------------------------------------
void EffectRideMotorcycle::unaffect()

{
    Slayer* pOwner = dynamic_cast<Slayer*>(m_pTarget);
    if (pOwner == NULL)
        return;
    // cout << "EffectRideMotorcycle unaffect" << endl;
    CGRideMotorCycle cgRide;
    cgRide.setObjectID(m_pMotorcycle->getObjectID());
    cgRide.setX(m_MotorX);
    cgRide.setY(m_MotorY);

    GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(pOwner->getPlayer());
    // synthetic packet straight to the handler (2.3)
    CGRideMotorCycleHandler::execute(&cgRide, pGamePlayer);
}

//----------------------------------------------------------------------
// get debug string
//----------------------------------------------------------------------
string EffectRideMotorcycle::toString() const

{
    StringStream msg;

    msg << "EffectRideMotorcycle(" << "ZoneID:" << (int)m_pZone->getZoneID() << ",X:" << (int)getX()
        << ",Y:" << (int)getY();

    if (m_pTarget)
        msg << ",Target:" << m_pTarget->toString();
    else
        msg << ",Target:NULL";

    msg << ",Deadline:" << (int)m_Deadline.tv_sec << "." << (int)m_Deadline.tv_usec;

    msg << ")";

    return msg.toString();
}
