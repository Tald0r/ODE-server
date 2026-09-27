//////////////////////////////////////////////////////////////////////////////
// Filename    : EffectYellowPoison.cpp
// Written by  : excel96
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "EffectYellowPoison.h"

#include "DB.h"
#include "EffectLight.h"
#include "EffectProtectionFromPoison.h"
#include "EffectYellowPoisonToCreature.h"
#include "GCAddEffect.h"
#include "GCChangeDarkLight.h"
#include "GCModifyInformation.h"
#include "GCRemoveEffect.h"
#include "HitRoll.h"
#include "Player.h"
#include "SkillUtil.h"
#include "Slayer.h"
#include "repository/ZoneInfoRepository.h"

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
EffectYellowPoison::EffectYellowPoison(Zone* pZone, ZoneCoord_t ZoneX, ZoneCoord_t ZoneY)

{
    __BEGIN_TRY

    m_pZone = pZone;
    m_X = ZoneX;
    m_Y = ZoneY;

    m_bVampire = false;
    m_bForce = false;

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectYellowPoison::affect(Zone* pZone, ZoneCoord_t x, ZoneCoord_t y, Object* pObject)

{
    __BEGIN_TRY
    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
bool EffectYellowPoison::affectCreature(Creature* pTargetCreature, bool bAffectByMove)

{
    __BEGIN_TRY

    Assert(pTargetCreature != NULL);

    // Only a Slayer or an Ousters can be affected, unless it is forced.
    if (!pTargetCreature->isSlayer() && !pTargetCreature->isOusters() && !isForce())
        return false;
    if (pTargetCreature->getCompetence() != 3)
        return false;

    // Check whether it is a safe zone.
    // 2003.1.10 by bezz.Sequoia
    if (!checkZoneLevelToHitTarget(pTargetCreature)) {
        return false;
    }

    Player* pPlayer = pTargetCreature->getPlayer();
    Assert(pPlayer != NULL);

    // The target's poison resistance decides whether it takes hold.
    Resist_t resist = pTargetCreature->getResist(MAGIC_DOMAIN_POISON);

    // Not affected, thanks to the poison resistance.
    if (m_bVampire) {
        if (!HitRoll::isSuccessVampireCurse(m_Level, resist))
            return false;
    } else {
        if (!HitRoll::isSuccessCurse(m_Level, resist))
            return false;
    }

    // Do not stack.
    // Stacking broke the lookup of OldSight, which seemed to leave Sight
    // stuck at 3.
    if (!pTargetCreature->isFlag(Effect::EFFECT_CLASS_YELLOW_POISON_TO_CREATURE)) {
        Zone* pZone = pTargetCreature->getZone();

        // The flag is off so there should be no effect, but just in case,
        // delete the same effect if one is already applied.
        if (pTargetCreature->isFlag(Effect::EFFECT_CLASS_YELLOW_POISON_TO_CREATURE)) {
            pTargetCreature->deleteEffect(Effect::EFFECT_CLASS_YELLOW_POISON_TO_CREATURE);
        }

        Sight_t CurrentSight = pTargetCreature->getSight();
        Sight_t oldSight = CurrentSight;

        // Create the effect and set its parameters.
        EffectYellowPoisonToCreature* pEffect = new EffectYellowPoisonToCreature(pTargetCreature);
        pEffect->setDeadline(m_Duration);
        pEffect->setOldSight(CurrentSight);
        pEffect->setLevel(m_Level);

        // Turn the Effect flag on for the Creature.
        pTargetCreature->setFlag(Effect::EFFECT_CLASS_YELLOW_POISON_TO_CREATURE);
        pTargetCreature->addEffect(pEffect);

        // Create the Effect in the DB.

        // This used to check the zone level and, in a safe zone, send
        // GCChangeDarkLight with a dark level of 0, but that
        // looked unnecessary and was taken out.
        GCChangeDarkLight gcChangeDarkLight;
        gcChangeDarkLight.setDarkLevel(15);
        gcChangeDarkLight.setLightLevel(1);
        pPlayer->sendPacket(&gcChangeDarkLight);

        pTargetCreature->setSight(pTargetCreature->getEffectedSight());
        GCModifyInformation gcMI;

        // The sight changed, so update the sight.
        if (oldSight != pTargetCreature->getSight()) {
            gcMI.addShortData(MODIFY_VISION, pTargetCreature->getSight());
        }

        // send GCModifyInformation for sight change
        gcMI.addShortData(MODIFY_EFFECT_STAT, Effect::EFFECT_CLASS_YELLOW_POISON_TO_CREATURE);
        gcMI.addLongData(MODIFY_DURATION, m_Duration);
        pPlayer->sendPacket(&gcMI);

        GCAddEffect gcAddEffect;
        gcAddEffect.setObjectID(pTargetCreature->getObjectID());
        gcAddEffect.setEffectID(Effect::EFFECT_CLASS_YELLOW_POISON_TO_CREATURE);
        gcAddEffect.setDuration(m_Duration);

        pZone->broadcastPacket(pTargetCreature->getX(), pTargetCreature->getY(), &gcAddEffect, pTargetCreature);

        return true;
    }

    return false;

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectYellowPoison::affect(Creature* pTargetCreature)

{
    __BEGIN_TRY
    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectYellowPoison::unaffect(Creature* pCreature)

{
    __BEGIN_TRY
    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectYellowPoison::unaffect()

{
    __BEGIN_TRY

    Tile& tile = m_pZone->getTile(m_X, m_Y);
    tile.deleteEffect(m_ObjectID);

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectYellowPoison::unaffect(Zone* pZone, ZoneCoord_t x, ZoneCoord_t y, Object* pObject)

    {__BEGIN_TRY __END_CATCH}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
string EffectYellowPoison::toString() const {
    __BEGIN_TRY

    StringStream msg;
    msg << "EffectYellowPoison(" << "ObjectID:" << getObjectID() << ")";
    return msg.toString();

    __END_CATCH
}

void EffectYellowPoisonLoader::load(Zone* pZone)

{
    __BEGIN_TRY

    vector<ZoneEffectRow> rows =
        defaultZoneInfoRepository().loadZoneEffectRects(pZone->getZoneID(), (int)Effect::EFFECT_CLASS_YELLOW_POISON);

    for (size_t r = 0; r < rows.size(); r++) {
        ZoneCoord_t left = rows[r].left;
        ZoneCoord_t top = rows[r].top;
        ZoneCoord_t right = rows[r].right;
        ZoneCoord_t bottom = rows[r].bottom;
        int value1 = rows[r].value1;

        VSRect rect(0, 0, pZone->getWidth() - 1, pZone->getHeight() - 1);

        for (int X = left; X <= right; X++)
            for (int Y = top; Y <= bottom; Y++) {
                if (rect.ptInRect(X, Y)) {
                    Tile& tile = pZone->getTile(X, Y);
                    if (tile.canAddEffect()) {
                        EffectYellowPoison* pEffect = new EffectYellowPoison(pZone, X, Y);
                        pEffect->setForce(true);
                        pEffect->setDuration(value1);
                        pEffect->setLevel(100);

                        // Register the effect in the zone and add it to the tile.
                        pZone->registerObject(pEffect);
                        tile.addEffect(pEffect);
                    }
                }
            }
    }

    __END_CATCH
}
