//////////////////////////////////////////////////////////////////////////////
// Filename    : EffectFlare.cpp
// Written by  :
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "EffectFlare.h"

#include "DB.h"
#include "DarkLightInfo.h"
#include "Flare.h"
#include "GCChangeDarkLight.h"
#include "GCModifyInformation.h"
#include "GCRemoveEffect.h"
#include "GameContext.h"
#include "Monster.h"
#include "MonsterInfo.h"
#include "Player.h"
#include "Vampire.h"
#include "domain/DarkLight.h"
#include "repository/EffectSaveRepository.h"

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
EffectFlare::EffectFlare(Creature* pCreature)

{
    __BEGIN_TRY

    setTarget(pCreature);

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectFlare::affect(Zone* pZone, ZoneCoord_t x, ZoneCoord_t y, Object* pObject)

{
    __BEGIN_TRY
    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectFlare::affect(Creature* pCreature)

{
    __BEGIN_TRY

    Assert(pCreature != NULL);
    Assert(!pCreature->isSlayer()); // Slayers are not affected.
    Assert(!pCreature->isNPC());    // NPCs are not affected either.

    pCreature->setSight(pCreature->getEffectedSight());

    if (pCreature->isPC() && pCreature->getSight() == FLARE_SIGHT) {
        Player* pPlayer = pCreature->getPlayer();
        Assert(pPlayer != NULL);

        GCChangeDarkLight gcChangeDarkLight;
        gcChangeDarkLight.setDarkLevel(15);
        gcChangeDarkLight.setLightLevel(1);
        pPlayer->sendPacket(&gcChangeDarkLight);
    }

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectFlare::unaffect(Creature* pCreature)

{
    __BEGIN_TRY
    __BEGIN_DEBUG


    if (pCreature == NULL) {
    }

    pCreature->removeFlag(Effect::EFFECT_CLASS_FLARE);

    Zone* pZone = pCreature->getZone();

    if (pCreature->isVampire()) {
        Vampire* pVampire = dynamic_cast<Vampire*>(pCreature);
        Player* pPlayer = pVampire->getPlayer();

        Sight_t NewSight = getOldSight();
        pVampire->setSight(NewSight);

        GCModifyInformation _GCModifyInformation;
        _GCModifyInformation.addShortData(MODIFY_VISION, NewSight);
        pPlayer->sendPacket(&_GCModifyInformation);

        // The zone's levels as a vampire sees them. The zone's type is not
        // consulted here: a vampire in a castle or a PK zone is sent the
        // same inverted levels as anywhere else.
        decore::DarkLightViewer viewer = {};
        viewer.race = decore::DarkLightRace::Vampire;
        const decore::DarkLight levels =
            decore::darkLightForViewer(viewer, pZone->getDarkLevel(), pZone->getLightLevel());
        DarkLevel_t darkLevel = levels.darkLevel;
        LightLevel_t lightLevel = levels.lightLevel;

        GCChangeDarkLight gcChangeDarkLight;
        gcChangeDarkLight.setDarkLevel(darkLevel);
        gcChangeDarkLight.setLightLevel(lightLevel);
        pPlayer->sendPacket(&gcChangeDarkLight);

        destroy(pVampire->getName());
    } else if (pCreature->isMonster()) {
        Monster* pMonster = dynamic_cast<Monster*>(pCreature);

        // Sets the sight back to the monster default.
        const MonsterInfo* pMonsterInfo = de::gameContext().monsterInfos().getMonsterInfo(pMonster->getMonsterType());
        pMonster->setSight(pMonsterInfo->getSight());

        // The sight was changed, so nearby enemies would have to be searched again.
    }

    // Tells clients that the effect is gone.
    GCRemoveEffect gcRemoveEffect;
    gcRemoveEffect.setObjectID(pCreature->getObjectID());
    gcRemoveEffect.addEffectList(Effect::EFFECT_CLASS_FLARE);
    pZone->broadcastPacket(pCreature->getX(), pCreature->getY(), &gcRemoveEffect);


    __END_DEBUG
    __END_CATCH
}
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectFlare::unaffect()

{
    __BEGIN_TRY
    __BEGIN_DEBUG


    Creature* pCreature = dynamic_cast<Creature*>(m_pTarget);
    unaffect(pCreature);


    __END_DEBUG
    __END_CATCH
}


//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectFlare::unaffect(Zone* pZone, ZoneCoord_t x, ZoneCoord_t y, Object* pObject)

{
    __BEGIN_TRY
    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectFlare::create(const string& ownerID)

{
    __BEGIN_TRY

    Timeval currentTime;

    getCurrentTime(currentTime);

    Turn_t currentYearTime;
    getCurrentYearTime(currentYearTime);

    defaultEffectSaveRepository().insertCreatureEffect(CREATURE_EFFECT_FLARE, ownerID, currentYearTime,
                                                       m_Deadline.tv_sec, 0, (int)m_OldSight);

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectFlare::destroy(const string& ownerID)

{
    __BEGIN_TRY

    defaultEffectSaveRepository().deleteCreatureEffect(CREATURE_EFFECT_FLARE, ownerID);

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectFlare::save(const string& ownerID)

{
    __BEGIN_TRY

    Turn_t currentYearTime;
    getCurrentYearTime(currentYearTime);

    defaultEffectSaveRepository().updateCreatureEffect(CREATURE_EFFECT_FLARE, ownerID, currentYearTime,
                                                       m_Deadline.tv_sec, 0, (int)m_OldSight);

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
string EffectFlare::toString() const {
    __BEGIN_TRY

    StringStream msg;

    msg << "EffectFlare(" << "ObjectID:" << getObjectID() << ")";

    return msg.toString();

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectFlareLoader::load(Creature* pCreature)

{
    __BEGIN_TRY

    Assert(pCreature != NULL);

    vector<CreatureEffectRow> rows =
        defaultEffectSaveRepository().loadCreatureEffects(CREATURE_EFFECT_FLARE, pCreature->getName());

    for (size_t r = 0; r < rows.size(); r++) {
        if (pCreature->isVampire()) {
            Turn_t YearTime = rows[r].yearTime;
            int DayTime = rows[r].dayTime;

            Turn_t currentYearTime;

            Timeval currentTime;

            getCurrentYearTime(currentYearTime);

            getCurrentTime(currentTime);

            int leftTime = ((YearTime - currentYearTime) * 24 * 60 * 60 + (DayTime - currentTime.tv_sec)) * 10;
            EffectFlare* pEffect = new EffectFlare(pCreature);

            if (leftTime > 0) {
                pEffect->setDeadline(leftTime);
            } else {
                pEffect->setDeadline(10);
            }

            pEffect->setOldSight(13);
            pCreature->setFlag(Effect::EFFECT_CLASS_FLARE);
            pCreature->addEffect(pEffect);
        }
    }

    __END_CATCH
}
