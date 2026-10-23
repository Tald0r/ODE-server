//////////////////////////////////////////////////////////////////////////////
// Filename    : EffectLoveChain.cpp
// Written by  : elca
// Description :
// Effect that, when it expires, moves the creature to the zone and
// position of the partner it is chained to.
//////////////////////////////////////////////////////////////////////////////

#include "EffectLoveChain.h"

#include "CreatureUtil.h"
#include "GCCannotUse.h"
#include "GCRemoveEffect.h"
#include "GCUseOK.h"
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
EffectLoveChain::EffectLoveChain(Creature* pCreature)

{
    // setTarget(pCreature);

    m_OwnerOID = pCreature->getObjectID();
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectLoveChain::affect(Creature* pCreature)

{}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectLoveChain::unaffect(Creature* pCreature)

{
    // cout << "EffectLoveChain " << "unaffect BEGIN" << endl;

    Assert(pCreature != NULL);

    // No effect flag means the creature died or similar, so do not transport it.
    if (!pCreature->isFlag(Effect::EFFECT_CLASS_LOVE_CHAIN))
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
    pCreature->removeFlag(Effect::EFFECT_CLASS_LOVE_CHAIN);

    // Announce that the effect is gone.
    GCRemoveEffect gcRemoveEffect;
    gcRemoveEffect.setObjectID(pCreature->getObjectID());
    gcRemoveEffect.addEffectList(Effect::EFFECT_CLASS_LOVE_CHAIN);
    pZone->broadcastPacket(x, y, &gcRemoveEffect);

    // Check the target and transport if it is allowed.
    bool bValid = false;

    PCFinder& pcFinder = de::gameContext().playerCreatures();

    __ENTER_CRITICAL_SECTION(pcFinder)

    Creature* pTargetCreature = pcFinder.getCreature_LOCKED(m_TargetName);
    if (pTargetCreature != NULL) {
        Zone* pTargetZone = pTargetCreature->getZone();
        if (pTargetZone != NULL) {
            // Cannot move into a master lair.
            if (!pTargetZone->isMasterLair() && !GDRLairManager::Instance().isGDRLairZone(pTargetZone->getZoneID())) {
                // Is the pay service available?
                if (pGamePlayer->loginPayPlay(pGamePlayer->getSocket()->getHost(), pGamePlayer->getID()) ||
                    !(de::gameContext().zoneInfos().getZoneInfo(pTargetZone->getZoneID())->isPayPlay())) {
                    if (!pTargetCreature->isPC() ||
                        !(dynamic_cast<PlayerCreature*>(pTargetCreature)->getStore()->isOpen())) {
                        // Cannot go to the field headquarters, outskirts, event stadium or event OX zones.
                        if (pTargetZone->getZoneID() != 2101 && pTargetZone->getZoneID() != 2102 &&
                            pTargetZone->getZoneID() != 1005 && pTargetZone->getZoneID() != 1006 &&
                            pTargetZone->getZoneID() != 1131 && pTargetZone->getZoneID() != 1132 &&
                            pTargetZone->getZoneID() != 1133 && pTargetZone->getZoneID() != 1134 &&
                            (pTargetZone->getZoneID() != 1122 || canEnterBeginnerZone(pCreature)) &&
                            !pTargetZone->isCastleZone()) {
                            bValid = true;

                            if (pCreature->isSlayer()) {
                                Slayer* pSlayer = dynamic_cast<Slayer*>(pCreature);
                                Assert(pSlayer != NULL);

                                if (pSlayer->hasRideMotorcycle()) {
                                    pSlayer->getOffMotorcycle();
                                }
                            }

                            pPC->getGQuestManager()->illegalWarp();
                            transportCreature(pCreature, pTargetZone->getZoneID(), pTargetCreature->getX(),
                                              pTargetCreature->getY(), false);
                        }
                    }
                }
            }
        }
    }

    __LEAVE_CRITICAL_SECTION(pcFinder)

    if (!bValid) {
        GCCannotUse _GCCannotUse;
        _GCCannotUse.setObjectID(m_ItemObjectID);
        pGamePlayer->sendPacket(&_GCCannotUse);
    }
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectLoveChain::unaffect()

{
    if (m_pZone != NULL) {
        Creature* pCreature = m_pZone->getCreature(m_OwnerOID);
        if (pCreature != NULL)
            unaffect(pCreature);
    }
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
string EffectLoveChain::toString() const

{
    StringStream msg;
    msg << "EffectLoveChain(" << "ObjectID:" << getObjectID() << "TargetName:" << m_TargetName << ")";
    return msg.toString();
}
