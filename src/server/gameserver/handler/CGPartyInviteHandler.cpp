//////////////////////////////////////////////////////////////////////////////
// Filename    : CGPartyInviteHandler.cpp
// Written By  : excel96
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "CGPartyInvite.h"

#ifdef __GAME_SERVER__
#include "Creature.h"
#include "CreatureUtil.h"
#include "GCPartyError.h"
#include "GCPartyInvite.h"
#include "GQuestManager.h"
#include "GameContext.h"
#include "GamePlayer.h"
#include "Party.h"
#include "PlayerCreature.h"
#include "SystemAvailabilitiesManager.h"
#include "Zone.h"
#include "party/PartyInviteDecision.h"

namespace {

// The zone's invite records and the global party manager, as the decision
// asks about them.
class ZonePartyInviteTopology : public PartyInviteTopology {
public:
    ZonePartyInviteTopology(PartyInviteInfoManager* pInfoManager, Creature* pRequester, Creature* pTarget)
        : m_pInfoManager(pInfoManager), m_pRequester(pRequester), m_pTarget(pTarget) {}

    bool targetHasInviteInfo() override {
        return m_pInfoManager->getInviteInfo(m_pTarget->getName()) != NULL;
    }

    bool isInviting() override {
        return m_pInfoManager->isInviting(m_pRequester, m_pTarget);
    }

    bool canAddMember(int partyID) override {
        return de::gameContext().parties().canAddMember(partyID);
    }

private:
    PartyInviteInfoManager* m_pInfoManager;
    Creature* m_pRequester;
    Creature* m_pTarget;
};

} // namespace

#endif

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void CGPartyInviteHandler::execute(CGPartyInvite* pPacket, Player* pPlayer)

{
    __BEGIN_DEBUG_EX

#ifdef __GAME_SERVER__

    Assert(pPacket != NULL);
    Assert(pPlayer != NULL);

    SYSTEM_ASSERT(SYSTEM_PARTY);

    GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(pPlayer);
    Assert(pGamePlayer != NULL);

    Creature* pCreature = pGamePlayer->getCreature();
    Assert(pCreature != NULL);

    Zone* pZone = pCreature->getZone();
    Assert(pZone != NULL);

    PartyInviteInfoManager* pPIIM = pZone->getPartyInviteInfoManager();
    Creature* pTargetCreature = pZone->getCreature(pPacket->getTargetObjectID());

    PartyInviteRequest request;
    request.code = pPacket->getCode();
    request.requesterObjectID = pCreature->getObjectID();
    request.targetObjectID = pPacket->getTargetObjectID();
    request.targetExists = pTargetCreature != NULL;

    if (pTargetCreature != NULL) {
        request.targetIsSameRacePC = pTargetCreature->isPC() && isSameRace(pCreature, pTargetCreature);
        request.requesterPartyID = pCreature->getPartyID();
        request.targetPartyID = pTargetCreature->getPartyID();
        request.requesterLevel = pCreature->getLevel();
        request.targetLevel = pTargetCreature->getLevel();
    }

    ZonePartyInviteTopology topology(pPIIM, pCreature, pTargetCreature);
    Outcome<PartyInviteEvents, PartyInviteRejection> outcome = decidePartyInvite(request, topology);

    if (outcome.isRejected()) {
        const PartyInviteRejection& rejection = outcome.rejection();

        if (rejection.reason == PartyInviteReason::UnknownCode)
            throw ProtocolException("CGPartyInvite::execute() : Unknown Code");

        if (rejection.cancelRequesterInvite)
            pPIIM->cancelInvite(pCreature);

        if (rejection.isPartyError) {
            executeError(pPacket, pPlayer, rejection.code);
        } else {
            GCPartyInvite gcPartyInvite;
            gcPartyInvite.setTargetObjectID(pPacket->getTargetObjectID());
            gcPartyInvite.setCode(rejection.code);
            pPlayer->sendPacket(&gcPartyInvite);
        }

        return;
    }

    const PartyInviteEvents& events = outcome.events();

    if (events.sendInvite) {
        GCPartyInvite gcPartyInvite;
        gcPartyInvite.setTargetObjectID(events.sendObjectID);
        gcPartyInvite.setCode(events.sendCode);

        if (events.sendTo == PartyInvitePeer::Target)
            pTargetCreature->getPlayer()->sendPacket(&gcPartyInvite);
        else
            pPlayer->sendPacket(&gcPartyInvite);
    }

    if (events.initInvite)
        pPIIM->initInviteInfo(pCreature, pTargetCreature);

    // A newcomer reaches the zone's local party only when the global party
    // manager took him.
    if (events.join == PartyJoin::RequesterJoinsTargetParty) {
        if (de::gameContext().parties().addPartyMember(events.joinPartyID, pCreature))
            pZone->getLocalPartyManager()->addPartyMember(events.joinPartyID, pCreature);
    } else if (events.join == PartyJoin::TargetJoinsRequesterParty) {
        if (de::gameContext().parties().addPartyMember(events.joinPartyID, pTargetCreature))
            pZone->getLocalPartyManager()->addPartyMember(events.joinPartyID, pTargetCreature);
    } else if (events.join == PartyJoin::CreateParty) {
        int NewPartyID = de::gameContext().parties().registerParty();

        de::gameContext().parties().createParty(NewPartyID, pTargetCreature->getCreatureClass());
        de::gameContext().parties().addPartyMember(NewPartyID, pCreature);
        de::gameContext().parties().addPartyMember(NewPartyID, pTargetCreature);

        LocalPartyManager* pLocalPartyManager = pZone->getLocalPartyManager();
        pLocalPartyManager->createParty(NewPartyID, pTargetCreature->getCreatureClass());
        pLocalPartyManager->addPartyMember(NewPartyID, pCreature);
        pLocalPartyManager->addPartyMember(NewPartyID, pTargetCreature);
    }

    if (events.questEvent != PartyQuestEvent::None) {
        Creature* pEventCreature = events.questEvent == PartyQuestEvent::Requester ? pCreature : pTargetCreature;
        PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pEventCreature);
        pPC->getGQuestManager()->eventParty();
    }

    if (events.cancelInvite)
        pPIIM->cancelInvite(pCreature, pTargetCreature);

#endif

    __END_DEBUG_EX
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void CGPartyInviteHandler::executeError(CGPartyInvite* pPacket, Player* pPlayer, BYTE ErrorCode)

{
    __BEGIN_DEBUG_EX

#ifdef __GAME_SERVER__

    GCPartyError gcPartyError;
    gcPartyError.setTargetObjectID(pPacket->getTargetObjectID());
    gcPartyError.setCode(ErrorCode);
    pPlayer->sendPacket(&gcPartyError);

#endif

    __END_DEBUG_EX
}
