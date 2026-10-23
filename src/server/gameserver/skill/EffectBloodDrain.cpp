//////////////////////////////////////////////////////////////////////////////
// Filename    : EffectBloodDrain.cpp
// Written by  : elca
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "EffectBloodDrain.h"

#include "Creature.h"
#include "DB.h"
#include "EventMorph.h"
#include "EventRegeneration.h"
#include "GCChangeDarkLight.h"
#include "GCModifyInformation.h"
#include "GCMorph1.h"
#include "GCMorphVampire2.h"
#include "GamePlayer.h"
#include "Monster.h"
#include "PCFinder.h"
#include "PCManager.h"
#include "Slayer.h"
#include "Vampire.h"
#include "repository/EffectSaveRepository.h"

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
EffectBloodDrain::EffectBloodDrain(Creature* pCreature)

{
    setTarget(pCreature);
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectBloodDrain::affect(Creature* pCreature)

{}


//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectBloodDrain::affect(Zone* pZone, ZoneCoord_t x, ZoneCoord_t y, Object* pObject)

{}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectBloodDrain::unaffect(Creature* pFromCreature)

{
    Assert(pFromCreature != NULL);

    if (pFromCreature->isSlayer()) {
        Player* pPlayer = pFromCreature->getPlayer();

        Assert(pPlayer != NULL);

        // Attaches an event to the GamePlayer so it is removed after the heartbeat runs.
        // According to the diagram there is no synchronization problem here.
        GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(pPlayer);

        pGamePlayer->deleteEvent(Event::EVENT_CLASS_REGENERATION);

        EventMorph* pEventMorph = new EventMorph(pGamePlayer);

        pEventMorph->setCreature(pFromCreature);
        pEventMorph->setDeadline(0);

        pGamePlayer->addEvent(pEventMorph);

        EventRegeneration* pEventRegeneration = new EventRegeneration(pGamePlayer);

        pEventRegeneration->setDeadline(10 * 10);
        pGamePlayer->addEvent(pEventRegeneration);

        destroy(pFromCreature->getName());
    } else {
        // Restores the sight.
        Assert(pFromCreature->isOusters());

        Player* pPlayer = pFromCreature->getPlayer();
        Assert(pPlayer != NULL);

        pFromCreature->removeFlag(Effect::EFFECT_CLASS_BLOOD_DRAIN);

        Sight_t oldSight = pFromCreature->getSight();
        Sight_t newSight = pFromCreature->getEffectedSight();

        if (oldSight != newSight) {
            GCModifyInformation gcMI;
            pFromCreature->setSight(newSight);
            gcMI.addShortData(MODIFY_VISION, pFromCreature->getSight());
            pFromCreature->getPlayer()->sendPacket(&gcMI);

            GCChangeDarkLight gcChangeDarkLight;
            gcChangeDarkLight.setDarkLevel(13);
            gcChangeDarkLight.setLightLevel(min(6, (int)newSight));
            pFromCreature->getPlayer()->sendPacket(&gcChangeDarkLight);
        }

        // Deletes it from the database.
        destroy(pFromCreature->getName());
    }
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectBloodDrain::unaffect()

{
    Creature* pCreature = dynamic_cast<Creature*>(m_pTarget);
    unaffect(pCreature);
}


//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectBloodDrain::unaffect(Zone* pZone, ZoneCoord_t x, ZoneCoord_t y, Object* pObject)

{}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectBloodDrain::create(const string& ownerID)

{
    Turn_t currentYearTime;

    getCurrentYearTime(currentYearTime);

    defaultEffectSaveRepository().insertCreatureEffect(CREATURE_EFFECT_BLOOD_DRAIN, ownerID, currentYearTime,
                                                       m_Deadline.tv_sec, (int)m_Level, 0);
}


//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectBloodDrain::destroy(const string& ownerID)

{
    defaultEffectSaveRepository().deleteCreatureEffect(CREATURE_EFFECT_BLOOD_DRAIN, ownerID);
}


//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectBloodDrain::save(const string& ownerID)

{
    Turn_t currentYearTime;

    getCurrentYearTime(currentYearTime);

    defaultEffectSaveRepository().updateCreatureEffect(CREATURE_EFFECT_BLOOD_DRAIN, ownerID, currentYearTime,
                                                       m_Deadline.tv_sec, m_Level, 0);
}


//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
string EffectBloodDrain::toString() const {
    StringStream msg;

    msg << "EffectBloodDrain(" << "ObjectID:" << getObjectID() << ")";

    return msg.toString();
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectBloodDrainLoader::load(Creature* pCreature)

{
    Assert(pCreature != NULL);
    if (!pCreature->isSlayer() && !pCreature->isOusters())
        return;

    vector<CreatureEffectRow> rows =
        defaultEffectSaveRepository().loadCreatureEffects(CREATURE_EFFECT_BLOOD_DRAIN, pCreature->getName());

    for (size_t r = 0; r < rows.size(); r++) {
        int DayTime = rows[r].dayTime;

        Timeval currentTime;
        getCurrentTime(currentTime);

        EffectBloodDrain* pEffectBloodDrain = new EffectBloodDrain(pCreature);

        if (currentTime.tv_sec + 600 < DayTime) {
            pEffectBloodDrain->setDeadline((DayTime - currentTime.tv_sec) * 10);
            pEffectBloodDrain->setLevel(rows[r].level);

            pCreature->addEffect(pEffectBloodDrain);
            pCreature->setFlag(Effect::EFFECT_CLASS_BLOOD_DRAIN);
        } else {
            pEffectBloodDrain->setDeadline(6000);
            pEffectBloodDrain->setLevel(rows[r].level);

            pCreature->addEffect(pEffectBloodDrain);
            pCreature->setFlag(Effect::EFFECT_CLASS_BLOOD_DRAIN);
        }
    }
}
