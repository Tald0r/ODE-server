////////////////////////////////////////////////////////////////////////////////
// Filename    : ActionWarpInZone.cpp
// Written By  :
// Description :
////////////////////////////////////////////////////////////////////////////////

#include "ActionWarpInZone.h"

#include "Creature.h"
#include "GCAddNPC.h"
#include "GCDeleteObject.h"
#include "NPC.h"
#include "PacketUtil.h"
#include "Utility.h"

static const POINT d[] = {
    POINT(-1, 0),  // 0 == LEFT
    POINT(-1, 1),  // 1 == LEFTDOWN
    POINT(0, 1),   // 2 == DOWN
    POINT(1, 1),   // 3 == RIGHTDOWN
    POINT(1, 0),   // 4 == RIGHT
    POINT(1, -1),  // 5 == RIGHTUP
    POINT(0, -1),  // 6 == UP
    POINT(-1, -1), // 7 == LEFTUP
};


////////////////////////////////////////////////////////////////////////////////
// read from PropertyBuffer
////////////////////////////////////////////////////////////////////////////////
void ActionWarpInZone::read(PropertyBuffer& propertyBuffer)

{
    try {
        m_MovePercentage = propertyBuffer.getPropertyInt("MovePercentage");

        Assert(m_MovePercentage <= 100);
    } catch (NoSuchElementException& nsee) {
        throw Error(nsee.toString());
    }
}

////////////////////////////////////////////////////////////////////////////////
// Execute the action.
////////////////////////////////////////////////////////////////////////////////
void ActionWarpInZone::execute(Creature* pCreature1, Creature* pCreature2)

{
    Assert(pCreature1 != NULL);

    Assert(pCreature1->isNPC());

    // Check whether to move on this turn.
    uint diceResult = Dice(1, 100);

    if (diceResult < m_MovePercentage) {
        Zone* pZone = pCreature1->getZone();

        ZoneCoord_t tx = rand() % (pZone->getWidth() - 10) + 5;
        ZoneCoord_t ty = rand() % (pZone->getHeight() - 10) + 5;

        int count = 0;

        while (!pCreature1->canMove(tx, ty)) {
            tx = rand() % (pZone->getWidth() - 10) + 5;
            ty = rand() % (pZone->getHeight() - 10) + 5;

            // Give up after ten failed tries.
            if (++count > 10)
                return;
        }

        Dir_t dir = rand() % 8;

        pZone->deleteCreatureFromTile(pCreature1, pCreature1->getX(), pCreature1->getY());
        GCDeleteObject gcDeleteObject(pCreature1->getObjectID());
        pZone->broadcastPacket(pCreature1->getX(), pCreature1->getY(), &gcDeleteObject);

        pZone->addCreatureToTile(pCreature1, tx, ty, false);
        pCreature1->setXYDir(tx, ty, dir);
        GCAddNPC gcAddNPC;
        makeGCAddNPC(&gcAddNPC, dynamic_cast<NPC*>(pCreature1));
        pZone->broadcastPacket(tx, ty, &gcAddNPC);
    }
}


////////////////////////////////////////////////////////////////////////////////
// get debug string
////////////////////////////////////////////////////////////////////////////////
string ActionWarpInZone::toString() const

{
    StringStream msg;

    msg << "ActionWarpInZone(" << "MovePercentage:" << (int)m_MovePercentage << ")";

    return msg.toString();
}
