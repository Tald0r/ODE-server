//////////////////////////////////////////////////////////////////////////////
// Filename    : EffectHeavenGround.h
// Written by  : bezz
//////////////////////////////////////////////////////////////////////////////

#ifndef __EFFECT_HEAVEN_GROUND__
#define __EFFECT_HEAVEN_GROUND__

#include "Effect.h"
#include "EffectLoader.h"

//////////////////////////////////////////////////////////////////////////////
// class EffectHeavenGround
//////////////////////////////////////////////////////////////////////////////

class EffectHeavenGround : public Effect {
public:
    EffectHeavenGround(Zone* pZone, ZoneCoord_t X, ZoneCoord_t Y);

public:
    EffectClass getEffectClass() const {
        return EFFECT_CLASS_HEAVEN_GROUND;
    }

    using Effect::affect;
    void affect();
    void affect(Zone* pZone, ZoneCoord_t X, ZoneCoord_t Y);

    void unaffect();

    string toString() const;

public:
    int getDamage(void) const {
        return m_Damage;
    }
    void setDamage(int damage) {
        m_Damage = damage;
    }

    Turn_t getTick() const {
        return m_Tick;
    }
    void setTick(Turn_t Tick) {
        m_Tick = Tick;
    }

    int getLevel(void) const {
        return m_Level;
    }
    void setLevel(int level) {
        m_Level = level;
    }

    void setUserObjectID(ObjectID_t oid) {
        m_UserObjectID = oid;
    }
    ObjectID_t getUserObjectID() const {
        return m_UserObjectID;
    }

    void setSlayer(bool bSlayer = true) {
        m_bSlayer = bSlayer;
    }
    bool isSlayer() const {
        return m_bSlayer;
    }

private:
    int m_Damage;                                // EffectAcidStorm Damage;
    Turn_t m_Tick;                               // EffectAcidStorm turn;
    int m_Level;                                 // EffectAcidStorm level;
    [[maybe_unused]] Duration_t m_Duration;      // EffectAcidStorm Duration;
    [[maybe_unused]] Duration_t m_StormDuration; // AcidStorm effect duration
    ObjectID_t m_UserObjectID;
    bool m_bSlayer;
};

#endif
