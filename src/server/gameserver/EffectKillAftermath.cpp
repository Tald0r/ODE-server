//////////////////////////////////////////////////////////////////////////////
// Filename    : EffectKillAftermath.cpp
// Written by  : elca
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "EffectKillAftermath.h"

#include "Creature.h"
#include "GamePlayer.h"
#include "Ousters.h"
#include "Slayer.h"
#include "Vampire.h"
#include "repository/EffectSaveRepository.h"

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
EffectKillAftermath::EffectKillAftermath(Creature* pCreature)

{
    // Server-only Effect.
    m_bBroadcastingEffect = false;

    setTarget(pCreature);
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
EffectKillAftermath::~EffectKillAftermath()

{}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectKillAftermath::affect(Creature* pCreature)

{}


//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectKillAftermath::affect(Zone* pZone, ZoneCoord_t x, ZoneCoord_t y, Object* pObject)

{}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectKillAftermath::unaffect(Creature* pFromCreature)

{
    __BEGIN_DEBUG

    // cout << "EffectKillAftermath" << "unaffect BEGIN" << endl;

    Assert(pFromCreature != NULL);
    pFromCreature->removeFlag(Effect::EFFECT_CLASS_KILL_AFTERMATH);
    destroy(pFromCreature->getName());

    // cout << "EffectKillAftermath" << "unaffect END" << endl;

    __END_DEBUG
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectKillAftermath::unaffect()

{
    // cout << "EffectKillAftermath" << "unaffect BEGIN" << endl;

    Creature* pCreature = dynamic_cast<Creature*>(m_pTarget);
    unaffect(pCreature);

    // cout << "EffectKillAftermath" << "unaffect END" << endl;
}


//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectKillAftermath::unaffect(Zone* pZone, ZoneCoord_t x, ZoneCoord_t y, Object* pObject)

{}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectKillAftermath::create(const string& ownerID)

{
    Turn_t currentYearTime;

    getCurrentYearTime(currentYearTime);

    defaultEffectSaveRepository().insertDeadline(EFFECT_TABLE_KILL_AFTERMATH, ownerID, currentYearTime,
                                                 m_Deadline.tv_sec);
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectKillAftermath::destroy(const string& ownerID)

{
    defaultEffectSaveRepository().deleteDeadline(EFFECT_TABLE_KILL_AFTERMATH, ownerID);
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectKillAftermath::save(const string& ownerID)

{
    Turn_t currentYearTime;

    getCurrentYearTime(currentYearTime);

    defaultEffectSaveRepository().updateDeadline(EFFECT_TABLE_KILL_AFTERMATH, ownerID, currentYearTime,
                                                 m_Deadline.tv_sec);
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
string EffectKillAftermath::toString() const {
    StringStream msg;

    msg << "EffectKillAftermath(" << "ObjectID:" << getObjectID() << ")";

    return msg.toString();
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectKillAftermathLoader::load(Creature* pCreature)

{
    if (pCreature == NULL) {
        // cout << "EffectKillAftermathLoader : the creature is null." << endl;
        return;
    }

    vector<DWORD> dayTimes =
        defaultEffectSaveRepository().loadDeadlines(EFFECT_TABLE_KILL_AFTERMATH, pCreature->getName());

    for (size_t r = 0; r < dayTimes.size(); r++) {
        if (pCreature->isSlayer()) {
            int DayTime = dayTimes[r];

            Timeval currentTime;
            getCurrentTime(currentTime);

            EffectKillAftermath* pEffectKillAftermath = new EffectKillAftermath(pCreature);

            if (currentTime.tv_sec < DayTime) {
                pEffectKillAftermath->setDeadline((DayTime - currentTime.tv_sec) * 10);

                Slayer* pSlayer = dynamic_cast<Slayer*>(pCreature);
                pSlayer->setFlag(Effect::EFFECT_CLASS_KILL_AFTERMATH);

                EffectManager* pEffectManager = pSlayer->getEffectManager();
                pEffectManager->addEffect(pEffectKillAftermath);
            } else {
                pEffectKillAftermath->setDeadline(0);

                Slayer* pSlayer = dynamic_cast<Slayer*>(pCreature);
                pSlayer->setFlag(Effect::EFFECT_CLASS_KILL_AFTERMATH);

                EffectManager* pEffectManager = pSlayer->getEffectManager();
                pEffectManager->addEffect(pEffectKillAftermath);
            }
        } else if (pCreature->isVampire()) {
            int DayTime = dayTimes[r];

            Timeval currentTime;
            getCurrentTime(currentTime);

            EffectKillAftermath* pEffectKillAftermath = new EffectKillAftermath(pCreature);

            if (currentTime.tv_sec < DayTime) {
                pEffectKillAftermath->setDeadline((DayTime - currentTime.tv_sec) * 10);

                Vampire* pVampire = dynamic_cast<Vampire*>(pCreature);
                pVampire->setFlag(Effect::EFFECT_CLASS_KILL_AFTERMATH);

                EffectManager* pEffectManager = pVampire->getEffectManager();
                pEffectManager->addEffect(pEffectKillAftermath);
            } else {
                pEffectKillAftermath->setDeadline(0);

                Vampire* pVampire = dynamic_cast<Vampire*>(pCreature);
                pVampire->setFlag(Effect::EFFECT_CLASS_KILL_AFTERMATH);

                EffectManager* pEffectManager = pVampire->getEffectManager();
                pEffectManager->addEffect(pEffectKillAftermath);
            }
        } else if (pCreature->isOusters()) {
            int DayTime = dayTimes[r];

            Timeval currentTime;
            getCurrentTime(currentTime);

            EffectKillAftermath* pEffectKillAftermath = new EffectKillAftermath(pCreature);

            if (currentTime.tv_sec < DayTime) {
                pEffectKillAftermath->setDeadline((DayTime - currentTime.tv_sec) * 10);

                Ousters* pOusters = dynamic_cast<Ousters*>(pCreature);
                pOusters->setFlag(Effect::EFFECT_CLASS_KILL_AFTERMATH);

                EffectManager* pEffectManager = pOusters->getEffectManager();
                pEffectManager->addEffect(pEffectKillAftermath);
            } else {
                pEffectKillAftermath->setDeadline(0);

                Ousters* pOusters = dynamic_cast<Ousters*>(pCreature);
                pOusters->setFlag(Effect::EFFECT_CLASS_KILL_AFTERMATH);

                EffectManager* pEffectManager = pOusters->getEffectManager();
                pEffectManager->addEffect(pEffectKillAftermath);
            }
        }
    }
}
