////////////////////////////////////////////////////////////////////////////////
// Filename    : ActionSetPosition.cpp
// Written By  :
// Description :
////////////////////////////////////////////////////////////////////////////////

#include "ActionSetPosition.h"

#include <fstream>

#include "NPC.h"
#include "NPCInfo.h"
#include "Tile.h"
#include "Zone.h"

////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////
void ActionSetPosition::read(PropertyBuffer& propertyBuffer)

{
    try {
        m_ZoneID = propertyBuffer.getPropertyInt("ZoneID");
        m_X = propertyBuffer.getPropertyInt("X");
        m_Y = propertyBuffer.getPropertyInt("Y");
        m_Dir = propertyBuffer.getPropertyInt("Dir");
    } catch (NoSuchElementException& nsee) {
        throw Error(nsee.toString());
    }
}

////////////////////////////////////////////////////////////////////////////////
// Execute the action.
////////////////////////////////////////////////////////////////////////////////
void ActionSetPosition::execute(Creature* pCreature1, Creature* pCreature2)

{
    try {
        Assert(pCreature1 != NULL);
        Assert(pCreature2 == NULL);
        Assert(!pCreature1->isPC());

        ////////////////////////////////////////////////////////////////////////////////
        // Set the creature's zone.
        // Internally this searches the ZoneInfoManager and ZoneGroupManager
        // and sets the proper zone pointer.
        ////////////////////////////////////////////////////////////////////////////////
        pCreature1->setZoneID(m_ZoneID);

        ////////////////////////////////////////////////////////////////////////////////
        // Add the creature to the zone.
        // Internally this sets the creature's coordinates and direction.
        ////////////////////////////////////////////////////////////////////////////////
        Zone* pZone = pCreature1->getZone();
        Assert(pZone != NULL);

        pZone->addCreature(pCreature1, m_X, m_Y, m_Dir);

        NPC* pNPC = dynamic_cast<NPC*>(pCreature1);

        if (pNPC->isShowInMinimap()) {
            NPCInfo* pNPCInfo = new NPCInfo();
            pNPCInfo->setName(pNPC->getName());
            pNPCInfo->setNPCID(pNPC->getNPCID());
            pNPCInfo->setX(pNPC->getX());
            pNPCInfo->setY(pNPC->getY());

            pZone->addNPCInfo(pNPCInfo);
            pZone->getTile(m_X - 1, m_Y).setBlocked(Creature::MOVE_MODE_WALKING);
        }
    } catch (Throwable& t) {
        cout << t.toString() << endl;
        throw; // -_-;
    }
}


////////////////////////////////////////////////////////////////////////////////
// get debug string
////////////////////////////////////////////////////////////////////////////////
string ActionSetPosition::toString() const

{
    StringStream msg;
    msg << "ActionSetPosition(" << "ZoneID:" << (int)m_ZoneID << ",X:" << (int)m_X << ",Y:" << (int)m_Y
        << ",Dir:" << dir2String(m_Dir) << ")";
    return msg.toString();
}
