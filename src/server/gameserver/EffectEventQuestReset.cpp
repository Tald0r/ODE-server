//////////////////////////////////////////////////////////////////////////////
// Filename    : EffectEventQuestReset.cpp
// Written by  : elca
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "EffectEventQuestReset.h"

#include <cstdio>

#include "GCSystemMessage.h"
#include "GQuestManager.h"
#include "Player.h"
#include "PlayerCreature.h"

EffectEventQuestReset::EffectEventQuestReset(Creature* pCreature, int type) : m_Type(type) {
    setTarget(pCreature);

    // Server-only Effect.
    m_bBroadcastingEffect = false;
}

EffectEventQuestReset::~EffectEventQuestReset()

{
    __BEGIN_TRY
    __END_CATCH_NO_RETHROW
}

void EffectEventQuestReset::affect()

{
    setNextTime(BROADCASTING_DELAY * 10);
    Creature* pCreature = dynamic_cast<Creature*>(m_pTarget);
    affect(pCreature);
}

void EffectEventQuestReset::affect(Creature* pCreature)

{
    if (!pCreature->isPC())
        return;
    PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pCreature);

    //	int lastSec = getRemainDuration() * 10;
    int lastSec = timediff(m_Deadline, gCurrentTime).tv_sec;
    cout << "lastSec : " << lastSec << endl;
    int lastHours = lastSec / 3600;
    int lastMins = (lastSec % 3600) / 60;

    char buffer[256];
    snprintf(buffer, sizeof(buffer), "%d hours %d minutes remain before the quest time limit.", lastHours, lastMins);
    GCSystemMessage gcSM;
    gcSM.setMessage(buffer);
    pPC->getPlayer()->sendPacket(&gcSM);

    cout << gcSM.getMessage() << endl;
}

void EffectEventQuestReset::unaffect()

{
    Creature* pCreature = dynamic_cast<Creature*>(m_pTarget);
    unaffect(pCreature);
}

void EffectEventQuestReset::unaffect(Creature* pCreature)

{
    if (!pCreature->isPC())
        return;
    PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pCreature);

    pPC->getGQuestManager()->eraseQuest(1001);
    pPC->getGQuestManager()->eraseQuest(1002);
    pPC->getGQuestManager()->eraseQuest(1003);
    pPC->getGQuestManager()->eraseQuest(1004);
    pPC->getGQuestManager()->eraseQuest(2001);
    pPC->getGQuestManager()->eraseQuest(2002);
    pPC->getGQuestManager()->eraseQuest(2003);
    pPC->getGQuestManager()->eraseQuest(2004);
    pPC->getGQuestManager()->eraseQuest(3001);
    pPC->getGQuestManager()->eraseQuest(3002);
    pPC->getGQuestManager()->eraseQuest(3003);
    pPC->getGQuestManager()->eraseQuest(3004);
    pPC->getGQuestManager()->refreshQuest();

    GCSystemMessage gcSM;
    gcSM.setMessage("The 12-hour time limit has passed. You may apply for grade A quests again.");
    pPC->getPlayer()->sendPacket(&gcSM);
}

string EffectEventQuestReset::toString() const {
    StringStream msg;
    msg << "EffectEventQuestReset(" << ")";
    return msg.toString();
}
