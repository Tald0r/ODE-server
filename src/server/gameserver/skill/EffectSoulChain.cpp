//////////////////////////////////////////////////////////////////////////////
// Filename    : EffectSoulChain.cpp
// Written by  : elca
// Description :
// Effect that, when it expires, transports the player it is on to the zone
// and position of the creature it is chained to, if that zone allows it.
//////////////////////////////////////////////////////////////////////////////

#include "EffectSoulChain.h"

#include "GCRemoveEffect.h"
#include "GCSkillFailed1.h"
#include "GCSkillFailed2.h"
#include "GDRLairManager.h"
#include "GQuestManager.h"
#include "GameContext.h"
#include "GamePlayer.h"
#include "PCFinder.h"
#include "Slayer.h"
#include "Store.h"
#include "Vampire.h"
#include "ZoneInfoManager.h"
#include "ZoneUtil.h"

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
EffectSoulChain::EffectSoulChain(Creature* pCreature)

{
    m_OwnerOID = pCreature->getObjectID();
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectSoulChain::affect(Creature* pCreature)

{}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectSoulChain::unaffect(Creature* pCreature)

{
    Assert(pCreature != NULL);

    // A missing effect flag means the caster died and will not be transported.
    if (!pCreature->isFlag(Effect::EFFECT_CLASS_SOUL_CHAIN))
        return;

    Zone* pZone = pCreature->getZone();
    Assert(pZone != NULL);

    Player* pPlayer = pCreature->getPlayer();
    Assert(pPlayer != NULL);

    GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(pPlayer);
    Assert(pGamePlayer != NULL);

    PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pCreature);
    Assert(pPC != NULL);

    ZoneCoord_t x = pCreature->getX();
    ZoneCoord_t y = pCreature->getY();
    pCreature->removeFlag(Effect::EFFECT_CLASS_SOUL_CHAIN);

    // Announces that the effect is gone.
    GCRemoveEffect gcRemoveEffect;
    gcRemoveEffect.setObjectID(pCreature->getObjectID());
    gcRemoveEffect.addEffectList(Effect::EFFECT_CLASS_SOUL_CHAIN);
    pZone->broadcastPacket(x, y, &gcRemoveEffect);

    // Checks the target and transports when the transport is possible.
    bool bValid = false;

    if (pPC->hasRelicItem() || pPC->isFlag(Effect::EFFECT_CLASS_HAS_FLAG) ||
        pPC->isFlag(Effect::EFFECT_CLASS_HAS_SWEEPER)) {
    } else if (pPC->isSlayer() && dynamic_cast<Slayer*>(pPC)->hasRideMotorcycle()) {
    } else if (pPC->getStore()->isOpen()) {
    } else {
        PCFinder& pcFinder = de::gameContext().playerCreatures();

        __ENTER_CRITICAL_SECTION(pcFinder)

        Creature* pTargetCreature = pcFinder.getCreature_LOCKED(m_TargetName);
        if (pTargetCreature != NULL) {
            Zone* pTargetZone = pTargetCreature->getZone();
            if (pTargetZone != NULL) {
                // Moving to a master lair is not allowed.
                if (!pTargetZone->isMasterLair() &&
                    !GDRLairManager::Instance().isGDRLairZone(pTargetZone->getZoneID())) {
                    // Is the pay service available?
                    if (pGamePlayer->loginPayPlay(pGamePlayer->getSocket()->getHost(), pGamePlayer->getID()) ||
                        pGamePlayer->isFamilyFreePass() ||
                        !(de::gameContext().zoneInfos().getZoneInfo(pTargetZone->getZoneID())->isPayPlay())) {
                        // The field headquarters, outskirts, event arena and event OX zones are off limits.
                        // The Temerie sanctuary is off limits as well.
                        if (pTargetZone->getZoneID() != 2101 && pTargetZone->getZoneID() != 2102 &&
                            pTargetZone->getZoneID() != 1005 && pTargetZone->getZoneID() != 1006 &&
                            pTargetZone->getZoneID() != 1122 && pTargetZone->getZoneID() != 1131 &&
                            pTargetZone->getZoneID() != 1132 && pTargetZone->getZoneID() != 1133 &&
                            pTargetZone->getZoneID() != 1134 && !pTargetZone->isCastleZone() &&
                            !pTargetZone->isDynamicZone()) {
                            bValid = true;
                            pPC->getGQuestManager()->illegalWarp();
                            transportCreature(pCreature, pTargetZone->getZoneID(), pTargetCreature->getX(),
                                              pTargetCreature->getY(), false);
                        }
                    }
                }
            }
        }

        __LEAVE_CRITICAL_SECTION(pcFinder)
    }

    if (!bValid) {
        GCSkillFailed1 gcSkillFailed1;
        gcSkillFailed1.setSkillType(SKILL_SOUL_CHAIN);
        pPlayer->sendPacket(&gcSkillFailed1);

        GCSkillFailed2 gcSkillFailed2;
        gcSkillFailed2.setSkillType(SKILL_SOUL_CHAIN);
        gcSkillFailed2.setObjectID(pCreature->getObjectID());

        pZone->broadcastPacket(pCreature->getX(), pCreature->getY(), &gcSkillFailed2, pCreature);
    }
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectSoulChain::unaffect()

{
    if (m_pZone != NULL) {
        Creature* pCreature = m_pZone->getCreature(m_OwnerOID);
        if (pCreature != NULL)
            unaffect(pCreature);
    }
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
string EffectSoulChain::toString() const {
    StringStream msg;
    msg << "EffectSoulChain(" << "ObjectID:" << getObjectID() << "TargetName:" << m_TargetName << ")";
    return msg.toString();
}
