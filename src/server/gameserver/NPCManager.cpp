//////////////////////////////////////////////////////////////////////////////
// Filename    : NPCManager.cpp
// Written By  : Reiot
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "NPCManager.h"

#include <stdio.h>

#include "GameContext.h"
#include "NPC.h"
#include "PCFinder.h"
#include "Thread.h"
#include "repository/ContentInfoRepository.h"

//////////////////////////////////////////////////////////////////////////////
// class NPCManager member methods
//////////////////////////////////////////////////////////////////////////////

NPCManager::NPCManager()

{}

NPCManager::~NPCManager()

{
    __BEGIN_TRY
    __END_CATCH_NO_RETHROW
}

void NPCManager::load(ZoneID_t zoneID, int race)

{
    __BEGIN_DEBUG

    bool bLoadAllRace = (race == 0xFF);

    vector<NPCRow> rows = bLoadAllRace ? defaultContentInfoRepository().loadNPCs((int)zoneID)
                                       : defaultContentInfoRepository().loadNPCsOfRace((int)zoneID, (int)race);

    for (size_t r = 0; r < rows.size(); r++) {
        const NPCRow& row = rows[r];

        string Name(row.name);

        if (getCreature(Name) == NULL) {
            // create NPC object
            NPC* pNPC = new NPC();

            pNPC->setName(Name);
            pNPC->setNPCID(row.npcID);
            pNPC->setSpriteType(row.spriteType);
            pNPC->setRace(row.race);
            pNPC->setMainColor(row.mainColor);
            pNPC->setSubColor(row.subColor);
            pNPC->setClanType(row.clanType);

            int ShowInMinimap = row.showInMinimap;

            if (ShowInMinimap != 0)
                pNPC->setShowInMinimap(true);
            else
                pNPC->setShowInMinimap(false);

            pNPC->setTaxingCastleZoneID(row.taxingCastleZoneID);

            filelog("NPC.log", "%s collects taxes for zone %u.", pNPC->getName().c_str(),
                    pNPC->getTaxingCastleZoneID());

            printf("NPC[%s] loading begin >> ", pNPC->getName().c_str());
            pNPC->init();
            printf("loading end\n");
            // For NPC tracing, by DEW 2003. 04. 16
            de::gameContext().playerCreatures().addNPC(pNPC);

            // NPC->init() loads the NPC's triggers and runs CONDITION_AT_FIRST;
            // its ACTION_SET_POSITION is what adds the NPC to the zone. An NPC
            // whose CONDITION_AT_FIRST is not ACTION_SET_POSITION would be a
            // problem.
            // addCreature(pNPC);
        }
    }

    __END_DEBUG
}

void NPCManager::processCreatures()

{
    __BEGIN_DEBUG

    Timeval currentTime;
    getCurrentTime(currentTime);

    try {
        unordered_map<ObjectID_t, Creature*>::iterator itr = m_Creatures.begin();
        for (; itr != m_Creatures.end(); itr++) {
            itr->second->act(currentTime);
        }
    } catch (Throwable& t) {
        filelog("NPCManagerBug.log", "ProcessCreatureBug : %s", t.toString().c_str());
        // cerr << t.toString() << endl;
    }

    __END_DEBUG
}

string NPCManager::toString() const

{
    StringStream msg;
    msg << "NPCManager(" << CreatureManager::toString() << ")";
    return msg.toString();
}
