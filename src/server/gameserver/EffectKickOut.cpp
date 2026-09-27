#include "EffectKickOut.h"

#include <cstdio>

#include "GCSystemMessage.h"
#include "PCManager.h"
#include "Zone.h"

void EffectKickOut::affect() {
    // Once a minute
    setNextTime(600);
    m_MinutesCount--;

    char msg[200];
    snprintf(msg, sizeof(msg), "%d minutes remain before the time limit.", m_MinutesCount);
    GCSystemMessage gcSM;
    gcSM.setMessage(msg);
    m_pZone->broadcastPacket(&gcSM);
}

void EffectKickOut::unaffect() {
    __BEGIN_TRY

    GCSystemMessage gcSM;
    gcSM.setMessage("The time limit has passed. Moving to the resurrection point in 10 seconds.");
    m_pZone->broadcastPacket(&gcSM);

    if (m_pZone != NULL)
        m_pZone->getPCManager()->transportAllCreatures(0xffff);

    __END_CATCH
}
