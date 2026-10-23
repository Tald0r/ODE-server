//////////////////////////////////////////////////////////////////////////////
// Filename    : EffectAftermath.cpp
// Written by  : elca
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "EffectAftermath.h"

#include "Creature.h"
#include "GamePlayer.h"
#include "Slayer.h"
#include "Vampire.h"
#include "repository/EffectSaveRepository.h"

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
EffectAftermath::EffectAftermath(Creature* pCreature)

{
    // Server-only Effect.
    m_bBroadcastingEffect = false;

    setTarget(pCreature);
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
EffectAftermath::~EffectAftermath()

{
    __BEGIN_TRY
    __END_CATCH_NO_RETHROW
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectAftermath::affect(Creature* pCreature)

{}


//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectAftermath::affect(Zone* pZone, ZoneCoord_t x, ZoneCoord_t y, Object* pObject)

{}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectAftermath::unaffect(Creature* pFromCreature)

{
    __BEGIN_DEBUG

    // cout << "EffectAftermath" << "unaffect BEGIN" << endl;

    Assert(pFromCreature != NULL);
    pFromCreature->removeFlag(Effect::EFFECT_CLASS_AFTERMATH);
    destroy(pFromCreature->getName());

    // cout << "EffectAftermath" << "unaffect END" << endl;

    __END_DEBUG
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectAftermath::unaffect()

{
    // cout << "EffectAftermath" << "unaffect BEGIN" << endl;

    Creature* pCreature = dynamic_cast<Creature*>(m_pTarget);
    unaffect(pCreature);

    // cout << "EffectAftermath" << "unaffect END" << endl;
}


//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectAftermath::unaffect(Zone* pZone, ZoneCoord_t x, ZoneCoord_t y, Object* pObject)

{}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectAftermath::create(const string& ownerID)

{
    Turn_t currentYearTime;

    getCurrentYearTime(currentYearTime);

    defaultEffectSaveRepository().insertDeadline(EFFECT_TABLE_AFTERMATH, ownerID, currentYearTime, m_Deadline.tv_sec);
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectAftermath::destroy(const string& ownerID)

{
    defaultEffectSaveRepository().deleteDeadline(EFFECT_TABLE_AFTERMATH, ownerID);
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectAftermath::save(const string& ownerID)

{
    Turn_t currentYearTime;

    getCurrentYearTime(currentYearTime);

    defaultEffectSaveRepository().updateDeadline(EFFECT_TABLE_AFTERMATH, ownerID, currentYearTime, m_Deadline.tv_sec);
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
string EffectAftermath::toString() const {
    StringStream msg;

    msg << "EffectAftermath(" << "ObjectID:" << getObjectID() << ")";

    return msg.toString();
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectAftermathLoader::load(Creature* pCreature)

{
    if (pCreature == NULL || (!pCreature->isSlayer() && !pCreature->isOusters())) {
        // cout << "EffectAftermathLoader : the creature is null." << endl;
        return;
    }

    vector<DWORD> dayTimes = defaultEffectSaveRepository().loadDeadlines(EFFECT_TABLE_AFTERMATH, pCreature->getName());

    for (size_t r = 0; r < dayTimes.size(); r++) {
        int DayTime = dayTimes[r];

        Timeval currentTime;
        getCurrentTime(currentTime);

        EffectAftermath* pEffectAftermath = new EffectAftermath(pCreature);

        if (currentTime.tv_sec < DayTime) {
            pEffectAftermath->setDeadline((DayTime - currentTime.tv_sec) * 10);

            pCreature->setFlag(Effect::EFFECT_CLASS_AFTERMATH);

            EffectManager* pEffectManager = pCreature->getEffectManager();
            pEffectManager->addEffect(pEffectAftermath);
        } else {
            pEffectAftermath->setDeadline(0);

            pCreature->setFlag(Effect::EFFECT_CLASS_AFTERMATH);

            EffectManager* pEffectManager = pCreature->getEffectManager();
            pEffectManager->addEffect(pEffectAftermath);
        }
    }
}
