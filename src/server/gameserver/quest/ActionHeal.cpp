////////////////////////////////////////////////////////////////////////////////
// Filename    : ActionHeal.cpp
// Written By  :
// Description :
////////////////////////////////////////////////////////////////////////////////

#include "ActionHeal.h"

#include "Creature.h"
#include "DB.h"
#include "EffectAftermath.h"
#include "GCModifyInformation.h"
#include "GCNPCResponse.h"
#include "GCRemoveEffect.h"
#include "GCStatusCurrentHP.h"
#include "GamePlayer.h"
#include "Ousters.h"
#include "Slayer.h"
#include "Vampire.h"
#include "Zone.h"

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
void ActionHeal::read(PropertyBuffer& propertyBuffer)

{}


////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
void ActionHeal::execute(Creature* pCreature1, Creature* pCreature2)

{
    Assert(pCreature1 != NULL);
    Assert(pCreature2 != NULL);
    Assert(pCreature1->isNPC());
    Assert(pCreature2->isPC());

    Player* pPlayer = pCreature2->getPlayer();
    Zone* pZone = pCreature2->getZone();

    Assert(pPlayer != NULL);

    // Send an OK packet to the client first.
    GCNPCResponse okpkt;
    okpkt.setCode(NPC_RESPONSE_HEAL);
    pPlayer->sendPacket(&okpkt);

    // No healing while dead or in a coma.
    if (pCreature2->isDead() || pCreature2->isFlag(Effect::EFFECT_CLASS_COMA)) {
        return;
    }

    // For a Slayer.
    if (pCreature2->isSlayer()) {
        Slayer* pSlayer = dynamic_cast<Slayer*>(pCreature2);
        EffectManager* pEffectManager = pSlayer->getEffectManager();
        GCModifyInformation modifyPkt;
        GCRemoveEffect removePkt;
        GCStatusCurrentHP hpPkt;

        // Fill HP and MP up first.
        if (pSlayer->getHP(ATTR_CURRENT) < pSlayer->getHP(ATTR_MAX)) {
            pSlayer->setHP(pSlayer->getHP(ATTR_MAX), ATTR_CURRENT);
            modifyPkt.addShortData(MODIFY_CURRENT_HP, pSlayer->getHP(ATTR_CURRENT));

            hpPkt.setObjectID(pSlayer->getObjectID());
            hpPkt.setCurrentHP(pSlayer->getHP(ATTR_CURRENT));
        }
        if (pSlayer->getMP(ATTR_CURRENT) < pSlayer->getMP(ATTR_MAX)) {
            pSlayer->setMP(pSlayer->getMP(ATTR_MAX), ATTR_CURRENT);
            modifyPkt.addShortData(MODIFY_CURRENT_MP, pSlayer->getMP(ATTR_CURRENT));
        }

        // Remove the blood drain effect.
        Effect* pBloodDrainEffect = pEffectManager->findEffect(Effect::EFFECT_CLASS_BLOOD_DRAIN);
        if (pBloodDrainEffect != NULL) {
            // Delete it from the database and from the effect manager.
            pBloodDrainEffect->destroy(pSlayer->getName());
            pEffectManager->deleteEffect(pSlayer, Effect::EFFECT_CLASS_BLOOD_DRAIN);

            // Attach an aftermath effect to prevent repeated blood drain healing.
            if (pSlayer->isFlag(Effect::EFFECT_CLASS_AFTERMATH)) {
                Effect* pEffect = pEffectManager->findEffect(Effect::EFFECT_CLASS_AFTERMATH);
                EffectAftermath* pEffectAftermath = dynamic_cast<EffectAftermath*>(pEffect);
                pEffectAftermath->setDeadline(5 * 600); // Lasts for 5 minutes.
            } else {
                EffectAftermath* pEffectAftermath = new EffectAftermath(pSlayer);
                pEffectAftermath->setDeadline(5 * 600); // Lasts for 5 minutes.
                pEffectManager->addEffect(pEffectAftermath);
                pSlayer->setFlag(Effect::EFFECT_CLASS_AFTERMATH);
                pEffectAftermath->create(pSlayer->getName());
            }

            // Add the information to the packet.
            removePkt.addEffectList((EffectID_t)Effect::EFFECT_CLASS_BLOOD_DRAIN);

            // Curing blood drain changes the stats.
            SLAYER_RECORD prev;
            pSlayer->getSlayerRecord(prev);
            pSlayer->initAllStat();
            pSlayer->sendModifyInfo(prev);
            pSlayer->sendRealWearingInfo();
        }

        // Remove the poison effect.
        Effect* pEffectPoison = pEffectManager->findEffect(Effect::EFFECT_CLASS_POISON);
        if (pEffectPoison != NULL) {
            // Delete it from the effect manager.
            pEffectManager->deleteEffect(pSlayer, Effect::EFFECT_CLASS_POISON);

            // Add the information to the packet.
            removePkt.addEffectList((EffectID_t)Effect::EFFECT_CLASS_POISON);
        }

        // Remove the dark blue poison effect.
        Effect* pEffectDarkBluePoison = pEffectManager->findEffect(Effect::EFFECT_CLASS_DARKBLUE_POISON);
        if (pEffectDarkBluePoison != NULL) {
            // Delete it from the effect manager.
            pEffectManager->deleteEffect(pSlayer, Effect::EFFECT_CLASS_DARKBLUE_POISON);

            // Add the information to the packet.
            removePkt.addEffectList((EffectID_t)Effect::EFFECT_CLASS_DARKBLUE_POISON);
        }

        // Send the packets.
        removePkt.setObjectID(pSlayer->getObjectID());
        pPlayer->sendPacket(&modifyPkt);
        pZone->broadcastPacket(pSlayer->getX(), pSlayer->getY(), &removePkt);
        pZone->broadcastPacket(pSlayer->getX(), pSlayer->getY(), &hpPkt, pSlayer);
    } else if (pCreature2->isVampire()) {
        Vampire* pVampire = dynamic_cast<Vampire*>(pCreature2);
        GCModifyInformation modifyPkt;
        GCRemoveEffect removePkt;
        GCStatusCurrentHP hpPkt;

        // Fill HP up.
        if (pVampire->getHP(ATTR_CURRENT) < pVampire->getHP(ATTR_MAX)) {
            pVampire->setHP(pVampire->getHP(ATTR_MAX), ATTR_CURRENT);
            modifyPkt.addShortData(MODIFY_CURRENT_HP, pVampire->getHP(ATTR_CURRENT));

            hpPkt.setObjectID(pVampire->getObjectID());
            hpPkt.setCurrentHP(pVampire->getHP(ATTR_CURRENT));
        }

        // Send the packets.
        removePkt.setObjectID(pVampire->getObjectID());
        pPlayer->sendPacket(&modifyPkt);
        pZone->broadcastPacket(pVampire->getX(), pVampire->getY(), &removePkt);
        pZone->broadcastPacket(pVampire->getX(), pVampire->getY(), &hpPkt, pVampire);
    } else if (pCreature2->isOusters()) {
        Ousters* pOusters = dynamic_cast<Ousters*>(pCreature2);
        EffectManager* pEffectManager = pOusters->getEffectManager();
        GCModifyInformation modifyPkt;
        GCRemoveEffect removePkt;
        GCStatusCurrentHP hpPkt;

        // Fill HP and MP up first.
        if (pOusters->getHP(ATTR_CURRENT) < pOusters->getHP(ATTR_MAX) || pOusters->getSilverDamage() != 0) {
            Silver_t prev = pOusters->getSilverDamage();

            if (prev != 0) {
                pOusters->setSilverDamage(0);
                modifyPkt.addShortData(MODIFY_SILVER_DAMAGE, pOusters->getSilverDamage());
            }

            pOusters->setHP(pOusters->getHP(ATTR_MAX), ATTR_CURRENT);
            modifyPkt.addShortData(MODIFY_CURRENT_HP, pOusters->getHP(ATTR_CURRENT));

            hpPkt.setObjectID(pOusters->getObjectID());
            hpPkt.setCurrentHP(pOusters->getHP(ATTR_CURRENT));
        }
        if (pOusters->getMP(ATTR_CURRENT) < pOusters->getMP(ATTR_MAX)) {
            pOusters->setMP(pOusters->getMP(ATTR_MAX), ATTR_CURRENT);
            modifyPkt.addShortData(MODIFY_CURRENT_MP, pOusters->getMP(ATTR_CURRENT));
        }

        // Remove the poison effect.
        Effect* pEffectPoison = pEffectManager->findEffect(Effect::EFFECT_CLASS_POISON);
        if (pEffectPoison != NULL) {
            // Delete it from the effect manager.
            pEffectManager->deleteEffect(pOusters, Effect::EFFECT_CLASS_POISON);

            // Add the information to the packet.
            removePkt.addEffectList((EffectID_t)Effect::EFFECT_CLASS_POISON);
        }

        // Remove the dark blue poison effect.
        Effect* pEffectDarkBluePoison = pEffectManager->findEffect(Effect::EFFECT_CLASS_DARKBLUE_POISON);
        if (pEffectDarkBluePoison != NULL) {
            // Delete it from the effect manager.
            pEffectManager->deleteEffect(pOusters, Effect::EFFECT_CLASS_DARKBLUE_POISON);

            // Add the information to the packet.
            removePkt.addEffectList((EffectID_t)Effect::EFFECT_CLASS_DARKBLUE_POISON);
        }

        // Remove the blood drain effect.
        Effect* pBloodDrainEffect = pEffectManager->findEffect(Effect::EFFECT_CLASS_BLOOD_DRAIN);
        if (pBloodDrainEffect != NULL) {
            pBloodDrainEffect->setDeadline(0);

            // Attach an aftermath effect to prevent repeated blood drain healing.
            if (pOusters->isFlag(Effect::EFFECT_CLASS_AFTERMATH)) {
                Effect* pEffect = pEffectManager->findEffect(Effect::EFFECT_CLASS_AFTERMATH);
                EffectAftermath* pEffectAftermath = dynamic_cast<EffectAftermath*>(pEffect);
                pEffectAftermath->setDeadline(5 * 600); // Lasts for 5 minutes.
            } else {
                EffectAftermath* pEffectAftermath = new EffectAftermath(pOusters);
                pEffectAftermath->setDeadline(5 * 600); // Lasts for 5 minutes.
                pEffectManager->addEffect(pEffectAftermath);
                pOusters->setFlag(Effect::EFFECT_CLASS_AFTERMATH);
                pEffectAftermath->create(pOusters->getName());
            }

            // Add the information to the packet.
            removePkt.addEffectList((EffectID_t)Effect::EFFECT_CLASS_BLOOD_DRAIN);
        }


        // Send the packets.
        removePkt.setObjectID(pOusters->getObjectID());
        pPlayer->sendPacket(&modifyPkt);
        pZone->broadcastPacket(pOusters->getX(), pOusters->getY(), &removePkt);
        pZone->broadcastPacket(pOusters->getX(), pOusters->getY(), &hpPkt, pOusters);
    }
}


////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
string ActionHeal::toString() const

{
    StringStream msg;
    msg << "ActionHeal(" << ")";
    return msg.toString();
}
