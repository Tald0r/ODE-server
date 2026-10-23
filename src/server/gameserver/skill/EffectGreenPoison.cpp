//////////////////////////////////////////////////////////////////////////////
// Filename    : EffectGreenPoison.cpp
// Written by  : elca
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "EffectGreenPoison.h"

#include "DB.h"
#include "EffectPoison.h"
#include "GCAddEffect.h"
#include "GCRemoveEffect.h"
#include "SkillHandler.h"
#include "Vampire.h"
#include "repository/ZoneInfoRepository.h"

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
EffectGreenPoison::EffectGreenPoison(Zone* pZone, ZoneCoord_t zoneX, ZoneCoord_t zoneY)

{
    m_UserObjectID = 0;
    m_pZone = pZone;
    m_X = zoneX;
    m_Y = zoneY;
    m_bVampire = false;
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
bool EffectGreenPoison::affectCreature(Creature* pTargetCreature, bool bAffectByMove)

{
    Assert(pTargetCreature != NULL);

    // Not applied if the target already has the acid storm effect.
    if (pTargetCreature->isFlag(Effect::EFFECT_CLASS_POISON)) {
        return false;
    }

    // Check whether it is a safe zone.
    // 2003.1.10 by bezz.Sequoia
    if (!checkZoneLevelToHitTarget(pTargetCreature)) {
        return false;
    }

    Zone* pZone = pTargetCreature->getZone();

    Creature* pAttacker = pZone->getCreature(m_UserObjectID);
    // Computes the poison damage dealt to the target.
    int PoisonDamage = computeMagicDamage(pTargetCreature, m_Damage, SKILL_GREEN_POISON, m_bVampire, pAttacker);

    if (PoisonDamage > 0) {
        // Create the effect, attach it to the target creature and set the flag.
        EffectPoison* pEffectPoison = new EffectPoison(pTargetCreature);
        pEffectPoison->setLevel(m_Level);
        pEffectPoison->setPoint(PoisonDamage);
        pEffectPoison->setDeadline(m_Duration); // This part needs to be changed.
        pEffectPoison->setTick(50);             // This part needs to be changed too.
        pEffectPoison->setUserObjectID(m_UserObjectID);
        pEffectPoison->affect(pTargetCreature);
        pTargetCreature->addEffect(pEffectPoison);
        pTargetCreature->setFlag(Effect::EFFECT_CLASS_POISON);

        // Tells the surroundings that the effect was attached.
        GCAddEffect gcAddEffect;
        gcAddEffect.setObjectID(pTargetCreature->getObjectID());
        gcAddEffect.setEffectID(Effect::EFFECT_CLASS_POISON);
        gcAddEffect.setDuration(m_Duration);
        pZone->broadcastPacket(pTargetCreature->getX(), pTargetCreature->getY(), &gcAddEffect);
    }


    return true;
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectGreenPoison::affect(Creature* pTargetCreature)

{}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectGreenPoison::affect(Zone* pZone, ZoneCoord_t x, ZoneCoord_t y, Object* pObject)

{}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectGreenPoison::unaffect()

{
    Tile& tile = m_pZone->getTile(m_X, m_Y);
    tile.deleteEffect(m_ObjectID);
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectGreenPoison::unaffect(Zone* pZone, ZoneCoord_t x, ZoneCoord_t y, Object* pObject)

{}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
string EffectGreenPoison::toString() const {
    StringStream msg;
    msg << "EffectGreenPoison(" << "DayTime:" << m_Deadline.tv_sec << ")";
    return msg.toString();
}

void EffectGreenPoisonLoader::load(Zone* pZone)

{
    vector<ZoneEffectRow> rows =
        defaultZoneInfoRepository().loadZoneEffectRects(pZone->getZoneID(), (int)Effect::EFFECT_CLASS_GREEN_POISON);

    for (size_t r = 0; r < rows.size(); r++) {
        ZoneCoord_t left = rows[r].left;
        ZoneCoord_t top = rows[r].top;
        ZoneCoord_t right = rows[r].right;
        ZoneCoord_t bottom = rows[r].bottom;
        int value1 = rows[r].value1;
        int value2 = rows[r].value2;
        int value3 = rows[r].value3;

        VSRect rect(0, 0, pZone->getWidth() - 1, pZone->getHeight() - 1);

        for (int X = left; X <= right; X++)
            for (int Y = top; Y <= bottom; Y++) {
                if (rect.ptInRect(X, Y)) {
                    Tile& tile = pZone->getTile(X, Y);
                    if (tile.canAddEffect()) {
                        EffectGreenPoison* pEffect = new EffectGreenPoison(pZone, X, Y);
                        pEffect->setDuration(value1);
                        pEffect->setNextTime(value2);
                        pEffect->setDamage(value3);

                        // Register the effect in the zone and add it to the tile.
                        pZone->registerObject(pEffect);
                        tile.addEffect(pEffect);
                    }
                }
            }
    }
}
