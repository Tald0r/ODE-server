///////////////////////////////////////////////////////////////////////////
// Project     : DARKEDEN
// Module      : Skill - Effect
// File Name   : EffectEnergyDrop.cpp
// Date        : 2002.3.28
// Description :
//               This effect implements what happens when an Energy Drop
//               falls to the ground.
//
// History
//     DATE      WRITER         DESCRIPTION
// =========== =========== =====================================================
//

#include "EffectEnergyDrop.h"

#include "EffectEnergyDropToCreature.h"
#include "GCAddEffect.h"
#include "GCModifyInformation.h"
#include "GCStatusCurrentHP.h"
#include "GamePlayer.h"
#include "Monster.h"
#include "SkillUtil.h"
#include "Slayer.h"
#include "Vampire.h"
#include "Zone.h"

EffectEnergyDrop::EffectEnergyDrop(Zone* pZone, ZoneCoord_t zoneX, ZoneCoord_t zoneY)

{
    m_pZone = pZone;
    m_X = zoneX;
    m_Y = zoneY;
    m_UserObjectID = 0;
}


bool EffectEnergyDrop::affectCreature(Creature* pTargetCreature, bool bAffectByMove)

{
    Assert(pTargetCreature != NULL);

    // Not applied if the target already has the acid storm effect.
    if (pTargetCreature->isFlag(Effect::EFFECT_CLASS_ENERGY_DROP_TO_CREATURE)) {
        return false;
    }

    // Safe zone check
    // 2003.1.10 by bezz, Sequoia
    if (!checkZoneLevelToHitTarget(pTargetCreature)) {
        return false;
    }

    Zone* pZone = pTargetCreature->getZone();

    // Computes the poison damage dealt to the target.
    int DropDamage = computeMagicDamage(pTargetCreature, m_Damage, SKILL_ENERGY_DROP);

    if (DropDamage > 0) {
        // Create the effect, attach it to the target creature and set the flag.

        // The EnergyDrop effect does not run continuously. For other effect skills the
        // duration grows with the caster's level and an effect is attached accordingly,
        // but once a --Drop or --Storm skill succeeds it hits everyone in the area
        // and, rather than damaging each over time, it splits a fixed amount of damage
        // into three. No function for dealing damage several times exists yet, so this
        // is implemented ad hoc with a deadline and a tick.
        // Damaging every 0.5 seconds over 1.6 seconds gives three hits.
        // The values are hard-coded here and should be replaced by another approach.
        // Adding member variables to the EffectEnergyDrop class
        //   m_Tick
        //   m_Count
        // and computing the deadline from them would be easier.

        EffectEnergyDropToCreature* pEffectEnergyDropToCreature = new EffectEnergyDropToCreature(pTargetCreature);

        // Carry the caster's object id over to the creature effect.
        pEffectEnergyDropToCreature->setUserObjectID(m_UserObjectID);

        pEffectEnergyDropToCreature->setLevel(m_Level);
        pEffectEnergyDropToCreature->setPoint(DropDamage / 3);
        pEffectEnergyDropToCreature->setDeadline(16); // This value needs to change.
        pEffectEnergyDropToCreature->setTick(5);      // This value needs to change too.
        pEffectEnergyDropToCreature->affect(pTargetCreature);
        pTargetCreature->addEffect(pEffectEnergyDropToCreature);
        pTargetCreature->setFlag(Effect::EFFECT_CLASS_ENERGY_DROP_TO_CREATURE);

        // Tells the surroundings that the effect was attached.
        GCAddEffect gcAddEffect;
        gcAddEffect.setObjectID(pTargetCreature->getObjectID());
        gcAddEffect.setEffectID(Effect::EFFECT_CLASS_ENERGY_DROP_TO_CREATURE);
        gcAddEffect.setDuration(m_Duration);
        pZone->broadcastPacket(pTargetCreature->getX(), pTargetCreature->getY(), &gcAddEffect);
    }


    return true;
}


void EffectEnergyDrop::affect()

{}

void EffectEnergyDrop::affect(Creature* pCreature)

{}

void EffectEnergyDrop::affect(Zone* pZone, ZoneCoord_t x, ZoneCoord_t y, Object* pObject)

{}

void EffectEnergyDrop::unaffect(Creature* pCreature)

{}

void EffectEnergyDrop::unaffect()

{
    Tile& tile = m_pZone->getTile(m_X, m_Y);
    tile.deleteEffect(m_ObjectID);
}

void EffectEnergyDrop::unaffect(Zone* pZone, ZoneCoord_t x, ZoneCoord_t y, Object* pObjbect)

{}

string EffectEnergyDrop::toString() const {
    StringStream msg;

    msg << "EffectEnergyDrop(" << "ObjectID:" << getObjectID() << ")";

    return msg.toString();
}
