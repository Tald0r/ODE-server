//////////////////////////////////////////////////////////////////////////////
// Filename    : EffectYellowPoisonToCreature.cpp
// Written by  : excel96
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "EffectYellowPoisonToCreature.h"

#include "DB.h"
#include "DarkLightInfo.h"
#include "GCChangeDarkLight.h"
#include "GCModifyInformation.h"
#include "GCRemoveEffect.h"
#include "Player.h"
#include "Slayer.h"
#include "repository/EffectSaveRepository.h"

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
EffectYellowPoisonToCreature::EffectYellowPoisonToCreature(Creature* pCreature)

{
    m_Level = 0;
    setTarget(pCreature);
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectYellowPoisonToCreature::affect(Zone* pZone, ZoneCoord_t x, ZoneCoord_t y, Object* pObject)

{}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectYellowPoisonToCreature::affect(Creature* pCreature)

{}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectYellowPoisonToCreature::unaffect(Creature* pCreature)

{
    __BEGIN_DEBUG


    if (pCreature != NULL) {
        Zone* pZone = pCreature->getZone();
        Player* pPlayer = pCreature->getPlayer();

        // The flag goes first, so the sight that is left is the one the
        // creature's remaining effects imply -- a creature still under
        // Flare keeps the Flare sight instead of being given full vision.
        pCreature->removeFlag(Effect::EFFECT_CLASS_YELLOW_POISON_TO_CREATURE);

        Sight_t NewSight = pCreature->getEffectedSight();
        pCreature->setSight(NewSight);

        if (pPlayer != NULL) {
            // Sends the vision information to the client.
            GCModifyInformation _GCModifyInformation;
            _GCModifyInformation.addShortData(MODIFY_VISION, NewSight);
            pPlayer->sendPacket(&_GCModifyInformation);

            // When Yellow Poison wears off, the scan is updated and the brightness adjusted.

            GCChangeDarkLight gcChangeDarkLight;
            gcChangeDarkLight.setDarkLevel(pZone->getDarkLevel());
            gcChangeDarkLight.setLightLevel(pZone->getLightLevel());

            pPlayer->sendPacket(&gcChangeDarkLight);
        }


        // Saves the sight when it wears off.

        // Tells clients that the effect is gone.
        GCRemoveEffect gcRemoveEffect;
        gcRemoveEffect.setObjectID(pCreature->getObjectID());
        gcRemoveEffect.addEffectList(Effect::EFFECT_CLASS_YELLOW_POISON_TO_CREATURE);
        pZone->broadcastPacket(pCreature->getX(), pCreature->getY(), &gcRemoveEffect);
    }

    __END_DEBUG
}
void EffectYellowPoisonToCreature::unaffect()

{
    Creature* pCreature = dynamic_cast<Creature*>(m_pTarget);
    unaffect(pCreature);
}


//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectYellowPoisonToCreature::unaffect(Zone* pZone, ZoneCoord_t x, ZoneCoord_t y, Object* pObject)

{}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectYellowPoisonToCreature::create(const string& ownerID)

{
    Timeval currentTime;

    getCurrentTime(currentTime);

    Turn_t currentYearTime;
    getCurrentYearTime(currentYearTime);

    defaultEffectSaveRepository().insertCreatureEffect(CREATURE_EFFECT_YELLOW_POISON_TO_CREATURE, ownerID,
                                                       currentYearTime, m_Deadline.tv_sec, (int)m_Level,
                                                       (int)m_OldSight);
}


//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectYellowPoisonToCreature::destroy(const string& ownerID)

{
    defaultEffectSaveRepository().deleteCreatureEffect(CREATURE_EFFECT_YELLOW_POISON_TO_CREATURE, ownerID);
}


//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectYellowPoisonToCreature::save(const string& ownerID)

{
    Turn_t currentYearTime;
    getCurrentYearTime(currentYearTime);

    defaultEffectSaveRepository().updateCreatureEffect(CREATURE_EFFECT_YELLOW_POISON_TO_CREATURE, ownerID,
                                                       currentYearTime, m_Deadline.tv_sec, (int)m_Level,
                                                       (int)m_OldSight);
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
string EffectYellowPoisonToCreature::toString() const {
    StringStream msg;

    msg << "EffectYellowPoisonToCreature(" << "ObjectID:" << getObjectID() << ")";

    return msg.toString();
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectYellowPoisonToCreatureLoader::load(Creature* pCreature)

{
    if (pCreature == NULL) {
        return;
    }


    vector<CreatureEffectRow> rows = defaultEffectSaveRepository().loadCreatureEffects(
        CREATURE_EFFECT_YELLOW_POISON_TO_CREATURE, pCreature->getName());

    for (size_t r = 0; r < rows.size(); r++) {
        if (pCreature->isSlayer() || pCreature->isOusters()) {
            Turn_t YearTime = rows[r].yearTime;
            int DayTime = rows[r].dayTime;

            Turn_t currentYearTime;

            Timeval currentTime;

            getCurrentYearTime(currentYearTime);

            getCurrentTime(currentTime);

            int leftTime = ((YearTime - currentYearTime) * 24 * 60 * 60 + (DayTime - currentTime.tv_sec)) * 10;
            EffectYellowPoisonToCreature* pEffect = new EffectYellowPoisonToCreature(pCreature);

            if (leftTime > 0) {
                pEffect->setDeadline(leftTime);
            } else {
                pEffect->setDeadline(10);
            }

            pEffect->setLevel(rows[r].level);

            pEffect->setOldSight(13);
            pCreature->setFlag(Effect::EFFECT_CLASS_YELLOW_POISON_TO_CREATURE);
            pCreature->addEffect(pEffect);
        }
    }
}
