//////////////////////////////////////////////////////////////////////////////
// Filename    : EffectHPRecovery.cpp
// Written by  : elca
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "EffectHPRecovery.h"

#include "Assert.h"
#include "Creature.h"
#include "DB.h"
#include "GCHPRecoveryEndToOthers.h"
#include "GCHPRecoveryEndToSelf.h"
#include "GCModifyInformation.h"
#include "Ousters.h"
#include "Player.h"
#include "Slayer.h"
#include "Vampire.h"
#include "Zone.h"

EffectHPRecovery::EffectHPRecovery()

{
    // Server-only Effect.
    m_bBroadcastingEffect = false;
}

EffectHPRecovery::EffectHPRecovery(Zone* pZone, ZoneCoord_t x, ZoneCoord_t y, Creature* pCreature, Turn_t delay)

    : Effect(pZone, x, y, pCreature, delay) {
    Assert(getZone() != NULL);
    Assert(getTarget() != NULL);

    // Server-only Effect.
    m_bBroadcastingEffect = false;
}

EffectHPRecovery::~EffectHPRecovery()

{}

void EffectHPRecovery::affect()

{
    setNextTime(m_Delay);
    Creature* pCreature = dynamic_cast<Creature*>(m_pTarget);
    affect(pCreature);
}

void EffectHPRecovery::affect(Creature* pCreature)

{
    Timeval CurrentTime;

    getCurrentTime(CurrentTime);

    if (pCreature->isSlayer()) {
        Turn_t timegapSec = m_Deadline.tv_sec - CurrentTime.tv_sec;
        Turn_t timegapUSec = m_Deadline.tv_usec - CurrentTime.tv_usec;

        Turn_t timegap = timegapSec * 1000000 + timegapUSec;

        int RecoveryPeriod = (timegap / (m_Delay * 100000));

        Slayer* pSlayer = dynamic_cast<Slayer*>(pCreature);

        bool bHPCheck = pSlayer->getHP(ATTR_CURRENT) < pSlayer->getHP(ATTR_MAX);
        bool bPeriodCheck = m_Period != 0;
        bool bAliveCheck = pSlayer->isAlive();
        bool bComaCheck = pSlayer->isFlag(Effect::EFFECT_CLASS_COMA);

        if (bHPCheck && bPeriodCheck && bAliveCheck && !bComaCheck) {
            // Set the flag.
            pSlayer->setFlag(Effect::EFFECT_CLASS_HP_RECOVERY);

            // How much to recover in one turn.
            HP_t CurrentHP = pSlayer->getHP(ATTR_CURRENT);
            HP_t NewHP =
                min((int)(pSlayer->getHP(ATTR_MAX)), (int)(CurrentHP + m_HPQuantity * (m_Period - RecoveryPeriod)));

            pSlayer->setHP(NewHP, ATTR_CURRENT);
        } else {
            // The packet goes out during unaffect.
            setDeadline(0);
        }

        m_Period = RecoveryPeriod;
    } else if (pCreature->isVampire()) {
        Turn_t timegapSec = m_Deadline.tv_sec - CurrentTime.tv_sec;
        Turn_t timegapUSec = m_Deadline.tv_usec - CurrentTime.tv_usec;

        Turn_t timegap = timegapSec * 1000000 + timegapUSec;

        int RecoveryPeriod = (timegap / (m_Delay * 100000));

        Vampire* pVampire = dynamic_cast<Vampire*>(pCreature);

        bool bHPCheck = pVampire->getHP(ATTR_CURRENT) < pVampire->getHP(ATTR_MAX);
        bool bPeriodCheck = m_Period != 0;
        bool bAliveCheck = pVampire->isAlive();
        bool bComaCheck = pVampire->isFlag(Effect::EFFECT_CLASS_COMA);

        if (bHPCheck && bPeriodCheck && bAliveCheck && !bComaCheck) {
            // Set the flag.
            pVampire->setFlag(Effect::EFFECT_CLASS_HP_RECOVERY);

            // How much to recover in one turn.
            HP_t CurrentHP = pVampire->getHP(ATTR_CURRENT);
            HP_t NewHP = min((int)(pVampire->getHP(ATTR_MAX)), CurrentHP + m_HPQuantity);

            pVampire->setHP(NewHP, ATTR_CURRENT);

            // Serum also heals silver damage.
            if (pVampire->getSilverDamage() > 0) {
                Silver_t silverRecovery = max(1, m_HPQuantity / 2);
                Silver_t newSilverDamage = max(0, (int)(pVampire->getSilverDamage() - silverRecovery));

                pVampire->saveSilverDamage(newSilverDamage);

                GCModifyInformation GCMI;
                GCMI.addShortData(MODIFY_SILVER_DAMAGE, newSilverDamage);

                pVampire->getPlayer()->sendPacket(&GCMI);
            }
        } else {
            // The packet goes out during unaffect.
            setDeadline(0);
        }

        m_Period = RecoveryPeriod;
    } else if (pCreature->isOusters()) {
        Turn_t timegapSec = m_Deadline.tv_sec - CurrentTime.tv_sec;
        Turn_t timegapUSec = m_Deadline.tv_usec - CurrentTime.tv_usec;

        Turn_t timegap = timegapSec * 1000000 + timegapUSec;

        int RecoveryPeriod = (timegap / (m_Delay * 100000));

        Ousters* pOusters = dynamic_cast<Ousters*>(pCreature);

        bool bHPCheck = pOusters->getHP(ATTR_CURRENT) < pOusters->getHP(ATTR_MAX);
        bool bPeriodCheck = m_Period != 0;
        bool bAliveCheck = pOusters->isAlive();
        bool bComaCheck = pOusters->isFlag(Effect::EFFECT_CLASS_COMA);

        if (bHPCheck && bPeriodCheck && bAliveCheck && !bComaCheck) {
            // Set the flag.
            pOusters->setFlag(Effect::EFFECT_CLASS_HP_RECOVERY);

            // How much to recover in one turn.
            HP_t CurrentHP = pOusters->getHP(ATTR_CURRENT);
            HP_t NewHP = min((int)(pOusters->getHP(ATTR_MAX) - pOusters->getSilverDamage()),
                             (int)(CurrentHP + (m_HPQuantity * (m_Period - RecoveryPeriod))));

            pOusters->setHP(NewHP, ATTR_CURRENT);

            // Heal silver damage.
        } else {
            // The packet goes out during unaffect.
            setDeadline(0);
        }

        m_Period = RecoveryPeriod;
    }
}

void EffectHPRecovery::affect(Zone* pZone, ZoneCoord_t x, ZoneCoord_t y, Object* pTarget)

{
    throw UnsupportedError();
}

void EffectHPRecovery::unaffect()

{
    Creature* pCreature = dynamic_cast<Creature*>(m_pTarget);
    unaffect(pCreature);
}

void EffectHPRecovery::unaffect(Creature* pCreature)

{
    if (pCreature->isSlayer()) {
        Slayer* pSlayer = dynamic_cast<Slayer*>(pCreature);
        Zone* pZone = pSlayer->getZone();

        // Recover HP only while turns remain and the creature is alive.
        if (m_Period != 0 && pCreature->isAlive()) {
            HP_t CurrentHP = pSlayer->getHP(ATTR_CURRENT);
            HP_t NewHP = min((int)(pSlayer->getHP(ATTR_MAX)), (int)(CurrentHP + m_HPQuantity * m_Period));

            pSlayer->setHP(NewHP, ATTR_CURRENT);
        }

        // Broadcast the current HP.
        // Announce that the recovery has ended.
        // To the player first.
        GCHPRecoveryEndToSelf gcEffectHPRecoveryEndToSelf;
        gcEffectHPRecoveryEndToSelf.setCurrentHP(pSlayer->getHP(ATTR_CURRENT));
        pSlayer->getPlayer()->sendPacket(&gcEffectHPRecoveryEndToSelf);

        // And to nearby players.
        GCHPRecoveryEndToOthers gcEffectHPRecoveryEndToOthers;
        gcEffectHPRecoveryEndToOthers.setObjectID(pSlayer->getObjectID());
        gcEffectHPRecoveryEndToOthers.setCurrentHP(pSlayer->getHP(ATTR_CURRENT));
        pZone->broadcastPacket(pSlayer->getX(), pSlayer->getY(), &gcEffectHPRecoveryEndToOthers, pSlayer);

        pSlayer->removeFlag(Effect::EFFECT_CLASS_HP_RECOVERY);
    } else if (pCreature->isVampire()) {
        Vampire* pVampire = dynamic_cast<Vampire*>(pCreature);
        Zone* pZone = pVampire->getZone();

        // Recover HP only while turns remain and the creature is alive.
        if (m_Period != 0 && pCreature->isAlive()) {
            HP_t CurrentHP = pVampire->getHP(ATTR_CURRENT);
            HP_t NewHP = min((int)(pVampire->getHP(ATTR_MAX)), (int)(CurrentHP + m_HPQuantity * m_Period));

            pVampire->setHP(NewHP, ATTR_CURRENT);
        }

        // Broadcast the current HP.
        // Announce that the recovery has ended.
        // To the player first.
        GCHPRecoveryEndToSelf gcEffectHPRecoveryEndToSelf;
        gcEffectHPRecoveryEndToSelf.setCurrentHP(pVampire->getHP(ATTR_CURRENT));
        pVampire->getPlayer()->sendPacket(&gcEffectHPRecoveryEndToSelf);

        // And to nearby players.
        GCHPRecoveryEndToOthers gcEffectHPRecoveryEndToOthers;
        gcEffectHPRecoveryEndToOthers.setObjectID(pVampire->getObjectID());
        gcEffectHPRecoveryEndToOthers.setCurrentHP(pVampire->getHP(ATTR_CURRENT));
        pZone->broadcastPacket(pVampire->getX(), pVampire->getY(), &gcEffectHPRecoveryEndToOthers, pVampire);

        pVampire->removeFlag(Effect::EFFECT_CLASS_HP_RECOVERY);
    } else if (pCreature->isOusters()) {
        Ousters* pOusters = dynamic_cast<Ousters*>(pCreature);
        Zone* pZone = pOusters->getZone();

        // Recover HP only while turns remain and the creature is alive.
        if (m_Period != 0 && pCreature->isAlive()) {
            HP_t CurrentHP = pOusters->getHP(ATTR_CURRENT);
            HP_t NewHP = min((int)(pOusters->getHP(ATTR_MAX)), (int)(CurrentHP + m_HPQuantity * m_Period));

            pOusters->setHP(NewHP, ATTR_CURRENT);
        }

        // Broadcast the current HP.
        // Announce that the recovery has ended.
        // To the player first.
        GCHPRecoveryEndToSelf gcEffectHPRecoveryEndToSelf;
        gcEffectHPRecoveryEndToSelf.setCurrentHP(pOusters->getHP(ATTR_CURRENT));
        pOusters->getPlayer()->sendPacket(&gcEffectHPRecoveryEndToSelf);

        // And to nearby players.
        GCHPRecoveryEndToOthers gcEffectHPRecoveryEndToOthers;
        gcEffectHPRecoveryEndToOthers.setObjectID(pOusters->getObjectID());
        gcEffectHPRecoveryEndToOthers.setCurrentHP(pOusters->getHP(ATTR_CURRENT));
        pZone->broadcastPacket(pOusters->getX(), pOusters->getY(), &gcEffectHPRecoveryEndToOthers, pOusters);

        pOusters->removeFlag(Effect::EFFECT_CLASS_HP_RECOVERY);
    }
}

void EffectHPRecovery::unaffect(Zone* pZone, ZoneCoord_t x, ZoneCoord_t y, Object* pTarget)

{}

string EffectHPRecovery::toString() const

{
    StringStream msg;

    if (m_pZone != NULL) {
        msg << "EffectHPRecovery(" << "ZoneID:" << (int)m_pZone->getZoneID() << ",X:" << (int)getX()
            << ",Y:" << (int)getY();
    }

    if (m_pTarget != NULL) {
        msg << ",Target:" << m_pTarget->toString();
    } else {
        msg << ",Target:NULL";
    }

    msg << ",Deadline:" << (int)m_Deadline.tv_sec << "." << (int)m_Deadline.tv_usec << ")";

    return msg.toString();
}
