//////////////////////////////////////////////////////////////////////////////
// Filename    : EffectFadeOut.cpp
// Written by  : elca
// Description :
// Effect attached to a creature that is fading out because of the soldier
// skill Sniping or the Vampire skill Invisibility.
//////////////////////////////////////////////////////////////////////////////

#include "EffectFadeOut.h"

#include "DB.h"
#include "EffectInvisibility.h"
#include "EffectSnipingMode.h"
#include "GCDeleteObject.h"
#include "Slayer.h"
#include "Vampire.h"
#include "ZoneUtil.h"

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
EffectFadeOut::EffectFadeOut(Creature* pCreature)

{
    setTarget(pCreature);
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectFadeOut::affect(Creature* pCreature)

{}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectFadeOut::affect(Zone* pZone, ZoneCoord_t x, ZoneCoord_t y, Object* pObject)

{}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectFadeOut::unaffect(Creature* pCreature)

{
    Assert(pCreature != NULL);

    Zone* pZone = pCreature->getZone();
    Assert(pZone != NULL);

    ZoneCoord_t x = pCreature->getX();
    ZoneCoord_t y = pCreature->getY();
    pCreature->removeFlag(Effect::EFFECT_CLASS_FADE_OUT);

    // Caused by the soldier skill Sniping.
    if (m_isSniping) {
        // Only a Slayer applies.
        Assert(pCreature->isSlayer());
        Assert(pCreature->isFlag(Effect::EFFECT_CLASS_SNIPING_MODE) == false);
        Assert(pCreature->findEffect(Effect::EFFECT_CLASS_SNIPING_MODE) == NULL);

        Slayer* pSlayer = dynamic_cast<Slayer*>(pCreature);

        EffectSnipingMode* pEffect = new EffectSnipingMode(pSlayer);
        pEffect->setDeadline(m_Duration);
        pSlayer->addEffect(pEffect);
        pSlayer->setFlag(Effect::EFFECT_CLASS_SNIPING_MODE);

        // The effect was attached, so recompute the stats.
        SLAYER_RECORD prev;
        pSlayer->getSlayerRecord(prev);
        pSlayer->initAllStat();
        pSlayer->sendRealWearingInfo();
        pSlayer->sendModifyInfo(prev);

        addSnipingModeCreature(pZone, pCreature, x, y);
    }
    // Caused by the Vampire skill Invisibility.
    else {
        // Only a Vampire or a monster applies.
        Assert(pCreature->isVampire() || pCreature->isMonster());
        Assert(pCreature->isFlag(Effect::EFFECT_CLASS_INVISIBILITY) == false);
        Assert(pCreature->findEffect(Effect::EFFECT_CLASS_INVISIBILITY) == NULL);

        EffectInvisibility* pEffect = new EffectInvisibility(pCreature);
        pEffect->setNextTime(10);
        pEffect->setDeadline(99999999); // Effectively unlimited.
        pCreature->addEffect(pEffect);

        addInvisibleCreature(pZone, pCreature, x, y);
    }
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectFadeOut::unaffect()

{
    Creature* pCreature = dynamic_cast<Creature*>(m_pTarget);
    unaffect(pCreature);
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectFadeOut::unaffect(Zone* pZone, ZoneCoord_t x, ZoneCoord_t y, Object* pObject)

{}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
string EffectFadeOut::toString() const {
    StringStream msg;
    msg << "EffectFadeOut(" << "ObjectID:" << getObjectID() << ")";
    return msg.toString();
}
