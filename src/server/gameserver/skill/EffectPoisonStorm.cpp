#include "EffectPoisonStorm.h"

#include "EffectStormPoison.h"
#include "GCAddEffect.h"
#include "GCModifyInformation.h"
#include "GCStatusCurrentHP.h"
#include "GamePlayer.h"
#include "Monster.h"
#include "SkillUtil.h"
#include "Slayer.h"
#include "Vampire.h"
#include "Zone.h"

EffectPoisonStorm::EffectPoisonStorm(Zone* pZone, ZoneCoord_t zoneX, ZoneCoord_t zoneY)

{
    m_pZone = pZone;
    m_X = zoneX;
    m_Y = zoneY;
    m_bVampire = false;
}


bool EffectPoisonStorm::affectCreature(Creature* pTargetCreature, bool bAffectByMove)

{
    Assert(pTargetCreature != NULL);

    // Not applied if the target already has the acid storm effect.
    if (pTargetCreature->isFlag(Effect::EFFECT_CLASS_STORM_POSION)) {
        return false;
    }

    Zone* pZone = pTargetCreature->getZone();

    Creature* pAttacker = pZone->getCreature(m_UserObjectID);
    // Computes the poison damage dealt to the target.
    int StormDamage = computeMagicDamage(pTargetCreature, m_Damage, SKILL_POISON_STORM, m_bVampire, pAttacker);

    if (StormDamage > 0) {
        // Create the effect, attach it to the target creature and set the flag.
        EffectStormPoison* pEffectStormPoison = new EffectStormPoison(pTargetCreature);
        pEffectStormPoison->setLevel(m_Level);
        pEffectStormPoison->setPoint(StormDamage / 3);
        pEffectStormPoison->setDeadline(16); // Hard-coded duration
        pEffectStormPoison->setTick(5);      // Hard-coded tick interval
        pEffectStormPoison->setUserObjectID(m_UserObjectID);
        pEffectStormPoison->affect(pTargetCreature);
        pTargetCreature->addEffect(pEffectStormPoison);
        pTargetCreature->setFlag(Effect::EFFECT_CLASS_STORM_POSION);

        // Tells the surroundings that the effect was attached.
        GCAddEffect gcAddEffect;
        gcAddEffect.setObjectID(pTargetCreature->getObjectID());
        gcAddEffect.setEffectID(Effect::EFFECT_CLASS_STORM_POSION);
        gcAddEffect.setDuration(m_Duration);
        pZone->broadcastPacket(pTargetCreature->getX(), pTargetCreature->getY(), &gcAddEffect);
    }


    return true;
}


void EffectPoisonStorm::affect()

{}

void EffectPoisonStorm::affect(Creature* pCreature)

{}

void EffectPoisonStorm::affect(Zone* pZone, ZoneCoord_t x, ZoneCoord_t y, Object* pObject)

{}

void EffectPoisonStorm::unaffect(Creature* pCreature)

{}

void EffectPoisonStorm::unaffect()

{
    Tile& tile = m_pZone->getTile(m_X, m_Y);
    tile.deleteEffect(m_ObjectID);
}

void EffectPoisonStorm::unaffect(Zone* pZone, ZoneCoord_t x, ZoneCoord_t y, Object* pObjbect)

{}

string EffectPoisonStorm::toString() const {
    StringStream msg;

    msg << "EffectPoisonStorm(" << "ObjectID:" << getObjectID() << ")";

    return msg.toString();
}
