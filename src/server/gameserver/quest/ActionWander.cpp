////////////////////////////////////////////////////////////////////////////////
// Filename    : ActionWander.cpp
// Written By  :
// Description :
////////////////////////////////////////////////////////////////////////////////

#include "ActionWander.h"

#include "Creature.h"
#include "NPC.h"
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
void ActionWander::read(PropertyBuffer& propertyBuffer)

{
    try {
        m_X = propertyBuffer.getPropertyInt("X");
        m_Y = propertyBuffer.getPropertyInt("Y");
        m_Radius = propertyBuffer.getPropertyInt("Radius");
        m_MovePercentage = propertyBuffer.getPropertyInt("MovePercentage");

        Assert(m_MovePercentage <= 100);
    } catch (NoSuchElementException& nsee) {
        throw Error(nsee.toString());
    }
}

////////////////////////////////////////////////////////////////////////////////
// Execute the action.
////////////////////////////////////////////////////////////////////////////////
void ActionWander::execute(Creature* pCreature1, Creature* pCreature2)

{
    Assert(pCreature1 != NULL);
    Assert(pCreature2 == NULL);

    Assert(pCreature1->isNPC() || pCreature1->isMonster());

    // Check whether to move on this turn.
    uint diceResult = Dice(1, 100);

    if (diceResult < m_MovePercentage) {
        bool movableDir[8];
        uint movableDirCount = 0;
        ZoneCoord_t cx = pCreature1->getX();
        ZoneCoord_t cy = pCreature1->getY();
        ZoneCoord_t nx = 0;
        ZoneCoord_t ny = 0;

        // If it moves, check which direction it moves in.
        // First build the list of directions it can go.
        for (uint i = 0; i < 8; i++) {
            nx = cx + d[i].x;
            ny = cy + d[i].y;

            // The next destination must lie inside the (x,y,r) area and be a tile
            // the NPC can move onto, that is, one that is not blocked.
            if (max(abs(m_X - nx), abs(m_Y - ny)) <= m_Radius && pCreature1->canMove(nx, ny)) {
                movableDir[i] = true;
                movableDirCount++;
            } else {
                movableDir[i] = false;
            }
        }

        // If it cannot move from the current position, it stays put.
        // If it can move, it moves.
        if (movableDirCount > 0) {
            Dir_t nextDir = pCreature1->getDir();
            bool changeDir = false;

            if (movableDir[nextDir]) {
                if (rand() % 10 == 0)
                    changeDir = true;
            } else {
                changeDir = true;
            }

            if (changeDir) {
                uint j = rand() % movableDirCount + 1;

                uint k = 0; // Index of the direction among those with movableDir[l] == true
                for (uint l = 0; l < 8; l++) {
                    if (movableDir[l] && ++k == j) {
                        nextDir = l;
                        break;
                    }
                }
            }

            nx = cx + d[nextDir].x;
            ny = cy + d[nextDir].y;
            // move creature
            Zone* pZone = pCreature1->getZone();
            pZone->moveCreature(pCreature1, nx, ny, nextDir);
        }
    }
}


////////////////////////////////////////////////////////////////////////////////
// get debug string
////////////////////////////////////////////////////////////////////////////////
string ActionWander::toString() const

{
    StringStream msg;

    msg << "ActionWander(" << " X:" << (int)m_X << " ,Y:" << (int)m_Y << " ,Radius:" << (int)m_Radius
        << " ,MovePercentage:" << (int)m_MovePercentage << ")";

    return msg.toString();
}
