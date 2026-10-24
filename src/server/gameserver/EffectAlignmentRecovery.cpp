//////////////////////////////////////////////////////////////////////////////
// Filename    : EffectAlignmentRecovery.cpp
// Written by  : elca
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "EffectAlignmentRecovery.h"

#include "AlignmentManager.h"
#include "Assert.h"
#include "DB.h"
#include "GCModifyInformation.h"
#include "GCOtherModifyInfo.h"
#include "GameContext.h"
#include "Ousters.h"
#include "Player.h"
#include "Slayer.h"
#include "Vampire.h"
#include "Zone.h"

//////////////////////////////////////////////////////////////////////////////
// class EffectAlignmentRecovery member methods
//////////////////////////////////////////////////////////////////////////////

EffectAlignmentRecovery::EffectAlignmentRecovery()

{
    // Server-only Effect.
    m_bBroadcastingEffect = false;
}

EffectAlignmentRecovery::EffectAlignmentRecovery(Zone* pZone, ZoneCoord_t x, ZoneCoord_t y, Creature* pCreature,
                                                 Turn_t delay)

    : Effect(pZone, x, y, pCreature, delay) {
    Assert(pZone != NULL);
    Assert(pCreature != NULL);

    Assert(getZone() != NULL);
    Assert(getTarget() != NULL);

    // Server-only Effect.
    m_bBroadcastingEffect = false;
}

EffectAlignmentRecovery::~EffectAlignmentRecovery()

{}

void EffectAlignmentRecovery::affect()

{
    setNextTime(m_Delay);

    Creature* pCreature = dynamic_cast<Creature*>(m_pTarget);
    Assert(pCreature != NULL);

    affect(pCreature);
}

void EffectAlignmentRecovery::affect(Creature* pCreature)

{
    Assert(pCreature != NULL);
    Assert(pCreature->isPC());

    Timeval CurrentTime;
    getCurrentTime(CurrentTime);

    GCModifyInformation gcModifyInformation;

    Alignment_t CurrentAlignment = 0;
    Alignment_t NewAlignment = 0;

    if (pCreature->isSlayer()) {
        Slayer* pSlayer = dynamic_cast<Slayer*>(pCreature);
        Assert(pSlayer != NULL);

        if (m_Period != 0) {
            // Set the flag.
            pSlayer->setFlag(Effect::EFFECT_CLASS_ALIGNMENT_RECOVERY);

            // How much to recover in one turn.
            CurrentAlignment = pSlayer->getAlignment();
            NewAlignment = min(10000, (int)(CurrentAlignment + m_AlignmentQuantity));

            pSlayer->setAlignment(NewAlignment);

            gcModifyInformation.addLongData(MODIFY_ALIGNMENT, NewAlignment);
            pSlayer->getPlayer()->sendPacket(&gcModifyInformation);

            WORD AlignmentSaveCount = pSlayer->getAlignmentSaveCount();
            if (AlignmentSaveCount > ALIGNMENT_SAVE_PERIOD) {
                StringStream msg;
                msg << "Alignment = " << NewAlignment;
                pSlayer->tinysave(msg.toString());
                AlignmentSaveCount = 0;
            } else
                AlignmentSaveCount++;
            pSlayer->setAlignmentSaveCount(AlignmentSaveCount);
        } else {
            // The packet goes out during unaffect.
            setDeadline(0);
        }

        m_Period--;
    } else if (pCreature->isVampire()) {
        Vampire* pVampire = dynamic_cast<Vampire*>(pCreature);
        Assert(pVampire != NULL);

        if (m_Period != 0) {
            // Set the flag.
            pVampire->setFlag(Effect::EFFECT_CLASS_ALIGNMENT_RECOVERY);

            // How much to recover in one turn.
            CurrentAlignment = pVampire->getAlignment();
            NewAlignment = min(10000, CurrentAlignment + m_AlignmentQuantity);

            pVampire->setAlignment(NewAlignment);

            gcModifyInformation.addLongData(MODIFY_ALIGNMENT, NewAlignment);
            pVampire->getPlayer()->sendPacket(&gcModifyInformation);

            WORD AlignmentSaveCount = pVampire->getAlignmentSaveCount();
            if (AlignmentSaveCount > ALIGNMENT_SAVE_PERIOD) {
                StringStream msg;
                msg << "Alignment = " << NewAlignment;
                pVampire->tinysave(msg.toString());
                AlignmentSaveCount = 0;
            } else
                AlignmentSaveCount++;
            pVampire->setAlignmentSaveCount(AlignmentSaveCount);
        } else {
            // The packet goes out during unaffect.
            setDeadline(0);
        }

        m_Period--;
    } else if (pCreature->isOusters()) {
        Ousters* pOusters = dynamic_cast<Ousters*>(pCreature);
        Assert(pOusters != NULL);

        if (m_Period != 0) {
            // Set the flag.
            pOusters->setFlag(Effect::EFFECT_CLASS_ALIGNMENT_RECOVERY);

            // How much to recover in one turn.
            CurrentAlignment = pOusters->getAlignment();
            NewAlignment = min(10000, CurrentAlignment + m_AlignmentQuantity);

            pOusters->setAlignment(NewAlignment);

            gcModifyInformation.addLongData(MODIFY_ALIGNMENT, NewAlignment);
            pOusters->getPlayer()->sendPacket(&gcModifyInformation);

            WORD AlignmentSaveCount = pOusters->getAlignmentSaveCount();
            if (AlignmentSaveCount > ALIGNMENT_SAVE_PERIOD) {
                StringStream msg;
                msg << "Alignment = " << NewAlignment;
                pOusters->tinysave(msg.toString());
                AlignmentSaveCount = 0;
            } else
                AlignmentSaveCount++;
            pOusters->setAlignmentSaveCount(AlignmentSaveCount);
        } else {
            // The packet goes out during unaffect.
            setDeadline(0);
        }

        m_Period--;
    } else {
        return; // not that it means much
    }

    // A change of alignment step has to be announced to other players too.

    Alignment beforeAlignment = de::gameContext().alignments().getAlignmentType(CurrentAlignment);
    Alignment afterAlignment = de::gameContext().alignments().getAlignmentType(NewAlignment);

    if (beforeAlignment != afterAlignment) {
        GCOtherModifyInfo gcOtherModifyInfo;
        gcOtherModifyInfo.setObjectID(pCreature->getObjectID());
        gcOtherModifyInfo.addShortData(MODIFY_ALIGNMENT, NewAlignment);

        Zone* pZone = pCreature->getZone(); // 2003.1.10
        Assert(pZone != NULL);

        pZone->broadcastPacket(pCreature->getX(), pCreature->getY(), &gcOtherModifyInfo, pCreature);
    }
}

void EffectAlignmentRecovery::affect(Zone* pZone, ZoneCoord_t x, ZoneCoord_t y, Object* pTarget)

{
    throw UnsupportedError();
}

void EffectAlignmentRecovery::unaffect()

{
    Creature* pCreature = dynamic_cast<Creature*>(m_pTarget);
    Assert(pCreature != NULL);
    unaffect(pCreature);
}

void EffectAlignmentRecovery::unaffect(Creature* pCreature)

{
    Assert(pCreature != NULL);

    if (pCreature->isSlayer()) {
        Slayer* pSlayer = dynamic_cast<Slayer*>(pCreature);
        Assert(pSlayer != NULL);

        Zone* pZone = pSlayer->getZone();
        Assert(pZone != NULL);

        if (m_Period != 0) {
            // How much to recover in one turn.
            Alignment_t CurrentAlignment = pSlayer->getAlignment();
            Alignment_t NewAlignment = min(10000, (int)(CurrentAlignment + m_AlignmentQuantity * m_Period));

            pSlayer->setAlignment(NewAlignment);

            WORD AlignmentSaveCount = pSlayer->getAlignmentSaveCount();
            if (AlignmentSaveCount == 10) {
                StringStream msg;
                msg << "Alignment = " << NewAlignment;
                pSlayer->tinysave(msg.toString());

                AlignmentSaveCount = 0;
            } else
                AlignmentSaveCount++;
            pSlayer->setAlignmentSaveCount(AlignmentSaveCount);
        }

        // Broadcast the current Alignment.
        // Announce that the recovery has ended.
        // To the player first.
        GCModifyInformation gcModifyInformation;
        gcModifyInformation.addLongData(MODIFY_ALIGNMENT, pSlayer->getAlignment());
        pSlayer->getPlayer()->sendPacket(&gcModifyInformation);

        // Something should go out to nearby players as well.
        // A new packet would have to be made for that.
        pSlayer->removeFlag(Effect::EFFECT_CLASS_ALIGNMENT_RECOVERY);
    } else if (pCreature->isVampire()) {
        Vampire* pVampire = dynamic_cast<Vampire*>(pCreature);

        // Zone* pZone = pVampire->getZone();

        if (m_Period != 0) {
            // How much to recover in one turn.
            Alignment_t CurrentAlignment = pVampire->getAlignment();
            Alignment_t NewAlignment = min(10000, (int)(CurrentAlignment + m_AlignmentQuantity * m_Period));

            WORD AlignmentSaveCount = pVampire->getAlignmentSaveCount();
            if (AlignmentSaveCount == 10) {
                StringStream msg;
                msg << "Alignment = " << NewAlignment;
                pVampire->tinysave(msg.toString());

                AlignmentSaveCount = 0;
            } else
                AlignmentSaveCount++;
            pVampire->setAlignmentSaveCount(AlignmentSaveCount);
        }

        // Broadcast the current Alignment.
        // Announce that the recovery has ended.
        // To the player first.
        GCModifyInformation gcModifyInformation;
        gcModifyInformation.addLongData(MODIFY_ALIGNMENT, pVampire->getAlignment());
        pVampire->getPlayer()->sendPacket(&gcModifyInformation);

        // And to nearby players.
        pVampire->removeFlag(Effect::EFFECT_CLASS_ALIGNMENT_RECOVERY);
    } else if (pCreature->isOusters()) {
        Ousters* pOusters = dynamic_cast<Ousters*>(pCreature);

        // Zone* pZone = pOusters->getZone();

        if (m_Period != 0) {
            // How much to recover in one turn.
            Alignment_t CurrentAlignment = pOusters->getAlignment();
            Alignment_t NewAlignment = min(10000, (int)(CurrentAlignment + m_AlignmentQuantity * m_Period));

            WORD AlignmentSaveCount = pOusters->getAlignmentSaveCount();
            if (AlignmentSaveCount == 10) {
                StringStream msg;
                msg << "Alignment = " << NewAlignment;
                pOusters->tinysave(msg.toString());

                AlignmentSaveCount = 0;
            } else
                AlignmentSaveCount++;
            pOusters->setAlignmentSaveCount(AlignmentSaveCount);
        }

        // Broadcast the current Alignment.
        // Announce that the recovery has ended.
        // To the player first.
        GCModifyInformation gcModifyInformation;
        gcModifyInformation.addLongData(MODIFY_ALIGNMENT, pOusters->getAlignment());
        pOusters->getPlayer()->sendPacket(&gcModifyInformation);

        // And to nearby players.
        pOusters->removeFlag(Effect::EFFECT_CLASS_ALIGNMENT_RECOVERY);
    }
}

void EffectAlignmentRecovery::unaffect(Zone* pZone, ZoneCoord_t x, ZoneCoord_t y, Object* pTarget)

{}

string EffectAlignmentRecovery::toString() const

{
    StringStream msg;

    if (m_pZone) {
        msg << "EffectAlignmentRecovery(" << "ZoneID:" << (int)m_pZone->getZoneID() << ",X:" << (int)getX()
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
