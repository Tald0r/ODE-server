#ifndef __EFFECT_ACID_STORM__
#define __EFFECT_ACID_STORM__

#include "Effect.h"
#include "EffectLoader.h"

class EffectAcidStorm : public Effect {
public:
    EffectAcidStorm(Zone* pZone, ZoneCoord_t zoneX, ZoneCoord_t zoneY);

    EffectClass getEffectClass() const {
        return EFFECT_CLASS_ACID_STORM;
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

    void setVampire(bool bVampire = true) {
        m_bVampire = bVampire;
    }
    bool isVampire() const {
        return m_bVampire;
    }

    bool affectCreature(Creature* pCreature, bool bAffectByMove);

private:
    int m_Damage;                                // EffectAcidStorm Damage;
    Turn_t m_Tick;                               // EffectAcidStorm turn;
    int m_Level;                                 // EffectAcidStorm level;
    Duration_t m_Duration;                       // EffectAcidStorm Duration;
    [[maybe_unused]] Duration_t m_StormDuration; // AcidStorm effect duration
    ObjectID_t m_UserObjectID;
    bool m_bVampire;
};

class EffectAcidStormLoader : public EffectLoader {
public:
    virtual Effect::EffectClass getEffectClass() const {
        return Effect::EFFECT_CLASS_ACID_STORM;
    }
    virtual string getEffectClassName() const {
        return "EffectAcidStorm";
    }

public:
    virtual void load(Creature* pCreature) {}
    virtual void load(Zone* pZone) {}
};

#endif
