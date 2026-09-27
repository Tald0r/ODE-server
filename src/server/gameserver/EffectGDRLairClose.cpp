#include "EffectGDRLairClose.h"

#include <cstdio>

#include "GCSystemMessage.h"
#include "GDRLairManager.h"
#include "GameContext.h"
#include "ZoneGroupManager.h"

void EffectGDRLairClose::affect() {
    setNextTime(600);

    char msg[200];
    snprintf(msg, sizeof(msg), "The Gilles de Rais Lair entrance closes in %d minutes.", m_MinutesCount);
    GCSystemMessage gcSM;
    gcSM.setMessage(msg);
    de::gameContext().zoneGroups().broadcast(&gcSM);
    m_MinutesCount--;
}

void EffectGDRLairClose::unaffect() {
    __BEGIN_TRY

    cout << "Closing the GDR lair." << endl;
    GDRLairManager::Instance().close();

    GCSystemMessage gcSM;
    gcSM.setMessage("The Gilles de Rais Lair is closed.");
    de::gameContext().zoneGroups().broadcast(&gcSM);

    __END_CATCH
}
