///////////////////////////////////////////////////////////////////////////
// Project     : DARKEDEN
// Module      : Skill - Effect
// File Name   : EffectVigorDrop.h
// Date        : 2002.3.28
// Description :
//               This effect implements what happens when a Vigor Drop
//               falls to the ground.
//
// History
//     DATE      WRITER         DESCRIPTION
// =========== =========== =====================================================
//
//

#ifndef __EFFECT_VIGOR_DROP__
#define __EFFECT_VIGOR_DROP__

#include "Effect.h"
#include "EffectLoader.h"

class EffectVigorDrop : public Effect {
public:
    EffectVigorDrop(Zone* pZone, ZoneCoord_t zoneX, ZoneCoord_t zoneY);

    EffectClass getEffectClass() const {
        return EFFECT_CLASS_VIGOR_DROP;
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
    int m_Damage;                                // EffectVigorDrop Damage;
    Turn_t m_Tick;                               // EffectVigorDrop turn;
    int m_Level;                                 // EffectVigorDrop level;
    Duration_t m_Duration;                       // EffectVigorDrop Duration;
    [[maybe_unused]] Duration_t m_StormDuration; // VigorDrop effect duration
};

class EffectVigorDropLoader : public EffectLoader {
public:
    virtual Effect::EffectClass getEffectClass() const {
        return Effect::EFFECT_CLASS_VIGOR_DROP;
    }
    virtual string getEffectClassName() const {
        return "EffectVigorDrop";
    }

public:
    virtual void load(Creature* pCreature) {}
};

#endif
