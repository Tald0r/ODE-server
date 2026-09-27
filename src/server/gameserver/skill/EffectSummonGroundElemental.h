//////////////////////////////////////////////////////////////////////////////
// Filename    : EffectSummonGroundElemental.h
// Written by  : bezz
//////////////////////////////////////////////////////////////////////////////

#ifndef __EFFECT_SUMMON_GROUND_ELEMENTAL__
#define __EFFECT_SUMMON_GROUND_ELEMENTAL__

#include "Effect.h"
#include "EffectLoader.h"

//////////////////////////////////////////////////////////////////////////////
// class EffectSummonGroundElemental
//////////////////////////////////////////////////////////////////////////////

class EffectSummonGroundElemental : public Effect {
public:
    EffectSummonGroundElemental(Zone* pZone, ZoneCoord_t X, ZoneCoord_t Y);

public:
    EffectClass getEffectClass() const {
        return EFFECT_CLASS_GROUND_ELEMENTAL_AURA;
    }

    using Effect::affect;
    void affect();
    void affect(Zone* pZone, ZoneCoord_t X, ZoneCoord_t Y);

    void unaffect();

    string toString() const;

public:
private:
};

#endif
