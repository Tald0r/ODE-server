#include "CoupleManager.h"

#include "PlayerCreature.h"
#include "repository/CoupleRepository.h"

bool CoupleManager::isCouple(PlayerCreature* pPC1, string name2) {
    Assert(pPC1 != NULL);

    return defaultCoupleRepository().countPairingWithPartner(pPC1->getSex(), pPC1->getName(), name2) >= 1;
}

bool CoupleManager::isCouple(PlayerCreature* pPC1, PlayerCreature* pPC2) {
    Assert(pPC1 != NULL);
    Assert(pPC2 != NULL);

    if (pPC1->getSex() == pPC2->getSex())
        return false;

    return defaultCoupleRepository().countPairing(pPC1->getSex(), pPC1->getName(), pPC2->getSex(), pPC2->getName()) >=
           1;
}

bool CoupleManager::hasCouple(PlayerCreature* pPC) {
    Assert(pPC != NULL);

    return defaultCoupleRepository().countPairingsOf(pPC->getSex(), pPC->getName()) >= 1;
}

bool CoupleManager::getPartnerName(PlayerCreature* pPC, string& partnerName) {
    Assert(pPC != NULL);

    return defaultCoupleRepository().loadPartnerName(pPC->getSex(), pPC->getName(), partnerName);
}

void CoupleManager::makeCouple(PlayerCreature* pPC1, PlayerCreature* pPC2) {
    Assert(pPC1 != NULL);
    Assert(pPC2 != NULL);

    Assert(pPC1->getRace() == pPC2->getRace());
    Assert(pPC1->getSex() != pPC2->getSex());

    defaultCoupleRepository().insertCouple(pPC1->getSex(), pPC1->getName(), pPC2->getSex(), pPC2->getName(),
                                           (uint)pPC1->getRace());
}

void CoupleManager::removeCouple(PlayerCreature* pPC1, PlayerCreature* pPC2) {
    Assert(pPC1 != NULL);
    Assert(pPC2 != NULL);

    Assert(pPC1->getRace() == pPC2->getRace());
    Assert(pPC1->getSex() != pPC2->getSex());

    defaultCoupleRepository().deletePairing(pPC1->getSex(), pPC1->getName(), pPC2->getSex(), pPC2->getName(),
                                            (uint)pPC1->getRace());
}

void CoupleManager::removeCoupleForce(PlayerCreature* pPC1, string strPC2) {
    Assert(pPC1 != NULL);

    defaultCoupleRepository().deletePairingWithPartner(pPC1->getSex(), pPC1->getName(), strPC2,
                                                       (uint)(pPC1->getRace()));
}

void CoupleManager::removeCoupleForce(PlayerCreature* pPC1) {
    Assert(pPC1 != NULL);

    defaultCoupleRepository().deletePairingsOf(pPC1->getSex(), pPC1->getName(), (uint)(pPC1->getRace()));
}
