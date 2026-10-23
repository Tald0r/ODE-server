//////////////////////////////////////////////////////////////////////////////
// Filename    : EffectCanEnterGDRLair.cpp
// Written by  : elca
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "EffectCanEnterGDRLair.h"

#include "Creature.h"
#include "GCModifyInformation.h"
#include "GCRemoveEffect.h"
#include "Monster.h"
#include "Ousters.h"
#include "Player.h"
#include "Slayer.h"
#include "Vampire.h"
#include "repository/EffectSaveRepository.h"

EffectCanEnterGDRLair::EffectCanEnterGDRLair(Creature* pCreature)

{
    setTarget(pCreature);
}

EffectCanEnterGDRLair::~EffectCanEnterGDRLair()

{}

void EffectCanEnterGDRLair::affect(Creature* pCreature)

{}

void EffectCanEnterGDRLair::unaffect()

{
    Creature* pCreature = dynamic_cast<Creature*>(m_pTarget);
    unaffect(pCreature);
}

void EffectCanEnterGDRLair::unaffect(Creature* pCreature)

{
    Assert(pCreature != NULL);

    pCreature->removeFlag(getEffectClass());

    Zone* pZone = pCreature->getZone();
    Assert(pZone != NULL);

    // Notify that the effect should be removed.
    GCRemoveEffect gcRemoveEffect;
    gcRemoveEffect.setObjectID(pCreature->getObjectID());
    gcRemoveEffect.addEffectList(getSendEffectClass());

    pZone->broadcastPacket(pCreature->getX(), pCreature->getY(), &gcRemoveEffect);

    destroy(pCreature->getName());
}

void EffectCanEnterGDRLair::create(const string& ownerID)

{
    Turn_t currentYearTime;

    getCurrentYearTime(currentYearTime);

    defaultEffectSaveRepository().insertDeadline(EFFECT_TABLE_CAN_ENTER_GDR_LAIR, ownerID, currentYearTime,
                                                 m_Deadline.tv_sec);
}

void EffectCanEnterGDRLair::destroy(const string& ownerID)

{
    defaultEffectSaveRepository().deleteDeadline(EFFECT_TABLE_CAN_ENTER_GDR_LAIR, ownerID);
}

void EffectCanEnterGDRLair::save(const string& ownerID)

{
    Turn_t currentYearTime;

    getCurrentYearTime(currentYearTime);

    defaultEffectSaveRepository().updateDeadline(EFFECT_TABLE_CAN_ENTER_GDR_LAIR, ownerID, currentYearTime,
                                                 m_Deadline.tv_sec);
}

string EffectCanEnterGDRLair::toString() const {
    StringStream msg;
    msg << "EffectCanEnterGDRLair(" << ")";
    return msg.toString();
}

void EffectCanEnterGDRLairLoader::load(Creature* pCreature)

{
    // Assert(pCreature != NULL);
    if (pCreature == NULL) {
        return;
    }

    vector<DWORD> dayTimes =
        defaultEffectSaveRepository().loadDeadlines(EFFECT_TABLE_CAN_ENTER_GDR_LAIR, pCreature->getName());

    for (size_t r = 0; r < dayTimes.size(); r++) {
        int DayTime = dayTimes[r];

        Timeval currentTime;
        getCurrentTime(currentTime);

        EffectCanEnterGDRLair* pEffectCanEnterGDRLair = new EffectCanEnterGDRLair(pCreature);

        EffectManager* pEffectManager = pCreature->getEffectManager();

        if (currentTime.tv_sec < DayTime) {
            pEffectCanEnterGDRLair->setDeadline((DayTime - currentTime.tv_sec) * 10);
        } else {
            pEffectCanEnterGDRLair->setDeadline(100);
        }

        pEffectManager->addEffect(pEffectCanEnterGDRLair);
        pCreature->setFlag(pEffectCanEnterGDRLair->getEffectClass());
    }
}
