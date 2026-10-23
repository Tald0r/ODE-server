//////////////////////////////////////////////////////////////////////////////
// Filename    : EffectLight.cpp
// Written by  : elca
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "EffectLight.h"

#include "Creature.h"
#include "DB.h"
#include "GCModifyInformation.h"
#include "GCRemoveEffect.h"
#include "Player.h"
#include "Slayer.h"
#include "repository/EffectSaveRepository.h"

//////////////////////////////////////////////////////////////////////////////
// class EffectLight member methods
//////////////////////////////////////////////////////////////////////////////

EffectLight::EffectLight(Creature* pCreature)

{
    setTarget(pCreature);
}

void EffectLight::affect(Zone* pZone, ZoneCoord_t x, ZoneCoord_t y, Object* pObject)

{}

void EffectLight::affect(Creature* pCreature)

{}

void EffectLight::unaffect(Creature* pCreature)

{
    Assert(pCreature != NULL);
    Assert(pCreature->isSlayer());

    Slayer* pSlayer = dynamic_cast<Slayer*>(pCreature);

    if (pSlayer != NULL) {
        Zone* pZone = pSlayer->getZone();

        Sight_t NewSight = 0;

        if (pSlayer->isFlag(Effect::EFFECT_CLASS_YELLOW_POISON_TO_CREATURE)) {
            NewSight = 3;
        } else {
            NewSight = 13;
        }

        pSlayer->setSight(NewSight);

        pSlayer->removeFlag(Effect::EFFECT_CLASS_LIGHT);

        Player* pPlayer = pSlayer->getPlayer();

        GCModifyInformation _GCModifyInformation;

        _GCModifyInformation.addShortData(MODIFY_VISION, NewSight);

        pPlayer->sendPacket(&_GCModifyInformation);

        // Tells clients that the effect is gone.
        GCRemoveEffect gcRemoveEffect;
        gcRemoveEffect.setObjectID(pSlayer->getObjectID());
        gcRemoveEffect.addEffectList(Effect::EFFECT_CLASS_LIGHT);
        pZone->broadcastPacket(pSlayer->getX(), pSlayer->getY(), &gcRemoveEffect);

        destroy(pSlayer->getName());
    }
}

void EffectLight::unaffect()

{
    Slayer* pSlayer = dynamic_cast<Slayer*>(m_pTarget);
    unaffect(pSlayer);
}

void EffectLight::unaffect(Zone* pZone, ZoneCoord_t x, ZoneCoord_t y, Object* pObject)

{}

void EffectLight::create(const string& ownerID)

{
    Timeval currentTime;
    getCurrentTime(currentTime);

    Turn_t currentYearTime;
    getCurrentYearTime(currentYearTime);

    defaultEffectSaveRepository().insertCreatureEffect(CREATURE_EFFECT_LIGHT, ownerID, currentYearTime,
                                                       m_Deadline.tv_sec, 0, (int)m_OldSight);
}

void EffectLight::destroy(const string& ownerID)

{
    defaultEffectSaveRepository().deleteCreatureEffect(CREATURE_EFFECT_LIGHT, ownerID);
}

void EffectLight::save(const string& ownerID)

{
    Turn_t currentYearTime;
    getCurrentYearTime(currentYearTime);

    defaultEffectSaveRepository().updateCreatureEffect(CREATURE_EFFECT_LIGHT, ownerID, currentYearTime,
                                                       m_Deadline.tv_sec, 0, (int)m_OldSight);
}

string EffectLight::toString() const {
    StringStream msg;
    msg << "EffectLight(" << "ObjectID:" << getObjectID() << ")";
    return msg.toString();
}

void EffectLightLoader::load(Creature* pCreature)

{
    if (pCreature == NULL) {
        return;
    }

    vector<CreatureEffectRow> rows =
        defaultEffectSaveRepository().loadCreatureEffects(CREATURE_EFFECT_LIGHT, pCreature->getName());

    for (size_t r = 0; r < rows.size(); r++) {
        if (pCreature->isSlayer()) {
            Turn_t YearTime = rows[r].yearTime;
            int DayTime = rows[r].dayTime;

            Turn_t currentYearTime;

            Timeval currentTime;

            getCurrentYearTime(currentYearTime);

            getCurrentTime(currentTime);

            int leftTime = ((YearTime - currentYearTime) * 24 * 60 * 60 + (DayTime - currentTime.tv_sec)) * 10;
            EffectLight* pEffect = new EffectLight(pCreature);
            if (leftTime > 0) {
                pEffect->setDeadline(leftTime);
                pCreature->setFlag(Effect::EFFECT_CLASS_LIGHT);
                pCreature->addEffect(pEffect);
            } else {
                pEffect->destroy(pCreature->getName());
                SAFE_DELETE(pEffect);
            }
        }
    }
}
