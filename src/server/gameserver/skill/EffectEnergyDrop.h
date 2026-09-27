///////////////////////////////////////////////////////////////////////////
// Project     : DARKEDEN
// Module      : Skill - Effect
// File Name   : EffectEnergyDrop.h
// Date        : 2002.3.28
// Description :
//               This effect implements what happens when an Energy Drop
//               falls to the ground.
//
// History
//     DATE      WRITER         DESCRIPTION
// =========== =========== =====================================================
//
//

#ifndef __EFFECT_ENERGY_DROP__
#define __EFFECT_ENERGY_DROP__

#include "Effect.h"
#include "EffectLoader.h"

class EffectEnergyDrop : public Effect {
public:
    EffectEnergyDrop(Zone* pZone, ZoneCoord_t zoneX, ZoneCoord_t zoneY);

    EffectClass getEffectClass() const {
        return EFFECT_CLASS_ENERGY_DROP;
    }

    void affect();
    void affect(Creature* pCreature);
    void affect(Zone* pZone, ZoneCoord_t x, ZoneCoord_t y, Object* pObject);

    void unaffect();
    void unaffect(Creature* pCreature);
    void unaffect(Item* pItem) {}
    void unaffect(Zone* pZone, ZoneCoord_t x, ZoneCoord_t y, Object* pObject);

    string toString() const;

public:
    ObjectID_t getUserObjectID() const {
        return m_UserObjectID;
    }
    void setUserObjectID(ObjectID_t UserObjectID) {
        m_UserObjectID = UserObjectID;
    }

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

    bool affectCreature(Creature* pCreature, bool bAffectByMove);

private:
    ObjectID_t m_UserObjectID;
    int m_Damage;                                // EffectEnergyDrop Damage;
    Turn_t m_Tick;                               // EffectEnergyDrop turn;
    int m_Level;                                 // EffectEnergyDrop level;
    Duration_t m_Duration;                       // EffectEnergyDrop Duration;
    [[maybe_unused]] Duration_t m_StormDuration; // EnergyDrop effect duration
};

class EffectEnergyDropLoader : public EffectLoader {
public:
    virtual Effect::EffectClass getEffectClass() const {
        return Effect::EFFECT_CLASS_ENERGY_DROP;
    }
    virtual string getEffectClassName() const {
        return "EffectEnergyDrop";
    }

public:
    virtual void load(Creature* pCreature) {}
};

#endif
