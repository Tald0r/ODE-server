//////////////////////////////////////////////////////////////////////////////
// Filename    : EffectMPRecovery.cpp
// Written by  : elca
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "EffectMPRecovery.h"

#include "Assert.h"
#include "Creature.h"
#include "GCMPRecoveryEnd.h"
#include "Ousters.h"
#include "Player.h"
#include "Slayer.h"
#include "Zone.h"

EffectMPRecovery::EffectMPRecovery()

{
    // Server-only effect.
    m_bBroadcastingEffect = false;
}

EffectMPRecovery::EffectMPRecovery(Zone* pZone, ZoneCoord_t x, ZoneCoord_t y, Creature* pCreature, Turn_t delay)

    : Effect(pZone, x, y, pCreature, delay) {
    Assert(getZone() != NULL);
    Assert(getTarget() != NULL);

    // Server-only effect.
    m_bBroadcastingEffect = false;
}

EffectMPRecovery::~EffectMPRecovery()

{}

void EffectMPRecovery::affect()

{
    // MP recovery according to the delay.
    setNextTime(m_Delay);

    Creature* pCreature = dynamic_cast<Creature*>(m_pTarget);

    affect(pCreature);
}

void EffectMPRecovery::affect(Creature* pCreature)

{
    if (pCreature->isFlag(Effect::EFFECT_CLASS_PLEASURE_EXPLOSION)) {
        cout << "No MP recovery under Pleasure Explosion." << endl;
        return;
    }

    Timeval CurrentTime;

    getCurrentTime(CurrentTime);

    if (pCreature->isSlayer()) {
        Turn_t timegapSec = m_Deadline.tv_sec - CurrentTime.tv_sec;
        Turn_t timegapUSec = m_Deadline.tv_usec - CurrentTime.tv_usec;

        Turn_t timegap = timegapSec * 1000000 + timegapUSec;

        int RecoveryPeriod = (timegap / (m_Delay * 100000));

        Slayer* pSlayer = dynamic_cast<Slayer*>(pCreature);

        if (pSlayer->getMP(ATTR_CURRENT) != pSlayer->getMP(ATTR_MAX) && m_Period != 0) {
            // Set the flag
            pSlayer->setFlag(Effect::EFFECT_CLASS_MP_RECOVERY);

            // How much to recover in one turn.
            MP_t CurrentMP = pSlayer->getMP(ATTR_CURRENT);
            MP_t NewMP =
                min((int)(pSlayer->getMP(ATTR_MAX)), (int)(CurrentMP + m_MPQuantity * (m_Period - RecoveryPeriod)));

            pSlayer->setMP(NewMP, ATTR_CURRENT);
        } else {
            setDeadline(0);
        }

        m_Period = RecoveryPeriod;
    } else if (pCreature->isOusters()) {
        Turn_t timegapSec = m_Deadline.tv_sec - CurrentTime.tv_sec;
        Turn_t timegapUSec = m_Deadline.tv_usec - CurrentTime.tv_usec;

        Turn_t timegap = timegapSec * 1000000 + timegapUSec;

        int RecoveryPeriod = (timegap / (m_Delay * 100000));

        Ousters* pOusters = dynamic_cast<Ousters*>(pCreature);

        if (pOusters->getMP(ATTR_CURRENT) <= pOusters->getMP(ATTR_MAX) && m_Period != 0) {
            // Set the flag
            pOusters->setFlag(Effect::EFFECT_CLASS_MP_RECOVERY);

            // How much to recover in one turn.
            MP_t CurrentMP = pOusters->getMP(ATTR_CURRENT);
            MP_t NewMP =
                min((int)(pOusters->getMP(ATTR_MAX)), (int)(CurrentMP + m_MPQuantity * (m_Period - RecoveryPeriod)));

            pOusters->setMP(NewMP, ATTR_CURRENT);
        } else {
            setDeadline(0);
        }

        m_Period = RecoveryPeriod;
    }
}

void EffectMPRecovery::affect(Zone* pZone, ZoneCoord_t x, ZoneCoord_t y, Object* pTarget)

{
    throw UnsupportedError();
}

void EffectMPRecovery::unaffect()

{
    Creature* pCreature = dynamic_cast<Creature*>(m_pTarget);

    unaffect(pCreature);
}

void EffectMPRecovery::unaffect(Creature* pCreature)

{
    if (pCreature->isSlayer()) {
        Slayer* pSlayer = dynamic_cast<Slayer*>(pCreature);

        if (m_Period != 0) {
            MP_t CurrentMP = pSlayer->getMP(ATTR_CURRENT);
            MP_t NewMP = min((int)(pSlayer->getMP(ATTR_MAX)), (int)(CurrentMP + m_MPQuantity * m_Period));

            pSlayer->setMP(NewMP, ATTR_CURRENT);
        }

        // Broadcast the current MP.
        // Announce that the recovery has now finished.
        // To the player itself first
        GCMPRecoveryEnd gcEffectMPRecoveryEnd;
        gcEffectMPRecoveryEnd.setCurrentMP(pSlayer->getMP(ATTR_CURRENT));
        pSlayer->getPlayer()->sendPacket(&gcEffectMPRecoveryEnd);

        pSlayer->removeFlag(Effect::EFFECT_CLASS_MP_RECOVERY);
    } else if (pCreature->isOusters()) {
        Ousters* pOusters = dynamic_cast<Ousters*>(pCreature);

        if (pOusters->getMP(ATTR_CURRENT) <= pOusters->getMP(ATTR_MAX) && m_Period != 0) {
            MP_t CurrentMP = pOusters->getMP(ATTR_CURRENT);
            MP_t NewMP = min((int)(pOusters->getMP(ATTR_MAX)), (int)(CurrentMP + m_MPQuantity * m_Period));

            pOusters->setMP(NewMP, ATTR_CURRENT);
        }

        // Broadcast the current MP.
        // Announce that the recovery has now finished.
        // To the player itself first
        GCMPRecoveryEnd gcEffectMPRecoveryEnd;
        gcEffectMPRecoveryEnd.setCurrentMP(pOusters->getMP(ATTR_CURRENT));
        pOusters->getPlayer()->sendPacket(&gcEffectMPRecoveryEnd);

        pOusters->removeFlag(Effect::EFFECT_CLASS_MP_RECOVERY);
    }
}

void EffectMPRecovery::unaffect(Zone* pZone, ZoneCoord_t x, ZoneCoord_t y, Object* pTarget)

{}

string EffectMPRecovery::toString() const

{
    StringStream msg;

    if (m_pZone) {
        msg << "EffectMPRecovery(" << "ZoneID:" << (int)m_pZone->getZoneID() << ",X:" << (int)getX()
            << ",Y:" << (int)getY();
    }

    if (m_pTarget) {
        msg << ",Target:" << m_pTarget->toString();
    } else {
        msg << ",Target:NULL";
    }

    msg << ",Deadline:" << (int)m_Deadline.tv_sec << "." << (int)m_Deadline.tv_usec << ")";

    return msg.toString();
}
