//////////////////////////////////////////////////////////////////////////////
// Filename    : EffectMute.cpp
// Written by  : elca
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "EffectMute.h"

#include "Creature.h"
#include "EventMorph.h"
#include "EventRegeneration.h"
#include "GCChangeDarkLight.h"
#include "GCModifyInformation.h"
#include "GCMorph1.h"
#include "GCMorphVampire2.h"
#include "GCRemoveEffect.h"
#include "GamePlayer.h"
#include "Monster.h"
#include "PCFinder.h"
#include "PCManager.h"
#include "Slayer.h"
#include "Vampire.h"
#include "repository/EffectSaveRepository.h"

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
EffectMute::EffectMute(Creature* pCreature)

{
    setTarget(pCreature);
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectMute::affect(Creature* pCreature)

{}


//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectMute::affect(Zone* pZone, ZoneCoord_t x, ZoneCoord_t y, Object* pObject)

{}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectMute::unaffect(Creature* pFromCreature)

{
    Assert(pFromCreature != NULL);

    pFromCreature->removeFlag(Effect::EFFECT_CLASS_MUTE);

    GCRemoveEffect gcRemoveEffect;
    gcRemoveEffect.setObjectID(pFromCreature->getObjectID());
    gcRemoveEffect.addEffectList(Effect::EFFECT_CLASS_MUTE);
    pFromCreature->getPlayer()->sendPacket(&gcRemoveEffect);

    destroy(pFromCreature->getName());
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectMute::unaffect()

{
    // cout << "EffectMute" << "unaffect BEGIN" << endl;

    Creature* pCreature = dynamic_cast<Creature*>(m_pTarget);
    unaffect(pCreature);

    // cout << "EffectMute" << "unaffect END" << endl;
}


//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectMute::unaffect(Zone* pZone, ZoneCoord_t x, ZoneCoord_t y, Object* pObject)

{}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectMute::create(const string& ownerID)

{
    Turn_t currentYearTime;

    getCurrentYearTime(currentYearTime);

    defaultEffectSaveRepository().insertDeadline(EFFECT_TABLE_MUTE, ownerID, currentYearTime, m_Deadline.tv_sec);
}


//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectMute::destroy(const string& ownerID)

{
    defaultEffectSaveRepository().deleteDeadline(EFFECT_TABLE_MUTE, ownerID);
}


//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectMute::save(const string& ownerID)

{
    Turn_t currentYearTime;

    getCurrentYearTime(currentYearTime);

    defaultEffectSaveRepository().updateDeadline(EFFECT_TABLE_MUTE, ownerID, currentYearTime, m_Deadline.tv_sec);
}


//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
string EffectMute::toString() const {
    StringStream msg;

    msg << "EffectMute(" << "ObjectID:" << getObjectID() << ")";

    return msg.toString();
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectMuteLoader::load(Creature* pCreature)

{
    Assert(pCreature != NULL);

    vector<DWORD> dayTimes = defaultEffectSaveRepository().loadDeadlines(EFFECT_TABLE_MUTE, pCreature->getName());

    for (size_t r = 0; r < dayTimes.size(); r++) {
        int DayTime = dayTimes[r];

        Timeval currentTime;
        getCurrentTime(currentTime);

        EffectMute* pEffectMute = new EffectMute(pCreature);

        if (currentTime.tv_sec < DayTime) {
            pEffectMute->setDeadline((DayTime - currentTime.tv_sec) * 10);

            pCreature->addEffect(pEffectMute);
            pCreature->setFlag(Effect::EFFECT_CLASS_MUTE);
        } else {
            pEffectMute->destroy(pCreature->getName());
        }
    }
}
