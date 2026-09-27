//////////////////////////////////////////////////////////////////////////////
// Filename    : EffectGrayDarkness.h
// Written by  : crazydog
// Description :
//////////////////////////////////////////////////////////////////////////////

#ifndef __EFFECT_GRAY_DARKNESS__
#define __EFFECT_GRAY_DARKNESS__

#include "Effect.h"
#include "EffectLoader.h"
#include "Tile.h"

//////////////////////////////////////////////////////////////////////////////
// class EffectGrayDarkness
//////////////////////////////////////////////////////////////////////////////

class EffectGrayDarkness : public Effect {
public:
    EffectGrayDarkness(Zone* pZone, ZoneCoord_t ZoneX, ZoneCoord_t ZoneY);

public:
    EffectClass getEffectClass() const {
        return EFFECT_CLASS_GRAY_DARKNESS;
    }

    void affect() {}

    void unaffect();

    string toString() const;

public:
    void setLevel(Attr_t l) {
        m_Level = l;
    }
    Attr_t getLevel() const {
        return m_Level;
    }

    void setDuration(Duration_t d) {
        m_Duration = d;
    }
    Duration_t getDuration() {
        return m_Duration;
    }


private:
    Attr_t m_Level;
    Duration_t m_Duration;
    [[maybe_unused]] Timeval m_StartTime; // Time the skill started
};

#endif // __EFFECT_GRAY_DARKNESS__
