///////////////////////////////////////////////////////////////////////////
// Project     : DARKEDEN
// Module      : Skill - Effect
// File Name   : EffectVigorDrop.cpp
// Date        : 2002.3.28
// Description :
//               This effect implements what happens when a Vigor Drop
//               falls to the ground.
//
// History
//     DATE      WRITER         DESCRIPTION
// =========== =========== =====================================================
//

#include "EffectVigorDrop.h"

#include "EffectVigorDropToCreature.h"
#include "GCAddEffect.h"
#include "GCModifyInformation.h"
#include "GCStatusCurrentHP.h"
#include "GamePlayer.h"
#include "Monster.h"
#include "SkillUtil.h"
#include "Slayer.h"
#include "Vampire.h"
#include "Zone.h"

EffectVigorDrop::EffectVigorDrop(Zone* pZone, ZoneCoord_t zoneX, ZoneCoord_t zoneY)

{
    m_pZone = pZone;
    m_X = zoneX;
    m_Y = zoneY;
    m_UserObjectID = 0;
}


bool EffectVigorDrop::affectCreature(Creature* pTargetCreature, bool bAffectByMove)

{
    Assert(pTargetCreature != NULL);

    // Not applied if the target already has the acid storm effect.
    if (pTargetCreature->isFlag(Effect::EFFECT_CLASS_VIGOR_DROP_TO_CREATURE)) {
        return false;
    }

    // Safe zone check
    // 2003.1.10 by bezz, Sequoia
    if (!checkZoneLevelToHitTarget(pTargetCreature)) {
        return false;
    }

    Zone* pZone = pTargetCreature->getZone();

    // Computes the poison damage dealt to the target.
    int DropDamage = computeMagicDamage(pTargetCreature, m_Damage, SKILL_VIGOR_DROP);

    if (DropDamage > 0) {
        // Creates the poison effect, attaches it to the target creature, and sets the flag.

        // The VigorDrop effect is not kept running continuously. Other effect skills
        // duration grows with the caster's level and an effect is attached accordingly,
        // but once a --Drop or --Storm skill succeeds it hits everyone in the area
        // and, rather than damaging each over time, it splits a fixed amount of damage
        // into three. No function for dealing damage several times exists yet, so this
        // is implemented ad hoc with a deadline and a tick.
        // Damaging every 0.5 seconds over 1.6 seconds gives three hits.
        // The values are hard-coded here and should be replaced by another approach.
        // Keeping, as member variables of the EffectVigorDrop class,
        //   m_Tick
        //   m_Count
        // and computing the deadline from them would be easier.

        EffectVigorDropToCreature* pEffectVigorDropToCreature = new EffectVigorDropToCreature(pTargetCreature);

        // Set the name and party id for the priority system.
        pEffectVigorDropToCreature->setUserObjectID(m_UserObjectID);

        pEffectVigorDropToCreature->setLevel(m_Level);
        pEffectVigorDropToCreature->setPoint(DropDamage / 3);
        pEffectVigorDropToCreature->setDeadline(16); // This part needs to be changed.
        pEffectVigorDropToCreature->setTick(5);      // This part needs to be changed too.
        pEffectVigorDropToCreature->affect(pTargetCreature);
        pTargetCreature->addEffect(pEffectVigorDropToCreature);
        pTargetCreature->setFlag(Effect::EFFECT_CLASS_VIGOR_DROP_TO_CREATURE);

        // Tells the surroundings that the effect was attached.
        GCAddEffect gcAddEffect;
        gcAddEffect.setObjectID(pTargetCreature->getObjectID());
        gcAddEffect.setEffectID(Effect::EFFECT_CLASS_VIGOR_DROP_TO_CREATURE);
        gcAddEffect.setDuration(m_Duration);
        pZone->broadcastPacket(pTargetCreature->getX(), pTargetCreature->getY(), &gcAddEffect);
    }


    return true;
}


void EffectVigorDrop::affect()

{}

void EffectVigorDrop::affect(Creature* pCreature)

{}

void EffectVigorDrop::affect(Zone* pZone, ZoneCoord_t x, ZoneCoord_t y, Object* pObject)

{}

void EffectVigorDrop::unaffect(Creature* pCreature)

{}

void EffectVigorDrop::unaffect()

{
    Tile& tile = m_pZone->getTile(m_X, m_Y);
    tile.deleteEffect(m_ObjectID);
}

void EffectVigorDrop::unaffect(Zone* pZone, ZoneCoord_t x, ZoneCoord_t y, Object* pObjbect)

{}

string EffectVigorDrop::toString() const {
    StringStream msg;

    msg << "EffectVigorDrop(" << "ObjectID:" << getObjectID() << ")";

    return msg.toString();
}
