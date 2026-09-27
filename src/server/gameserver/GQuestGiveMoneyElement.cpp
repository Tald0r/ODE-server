#include "GQuestGiveMoneyElement.h"

#include <cstdio>

#include "GCModifyInformation.h"
#include "GCSystemMessage.h"
#include "Player.h"
#include "PlayerCreature.h"

GQuestElement::ResultType GQuestGiveMoneyElement::checkCondition(PlayerCreature* pPC) const {
    GCModifyInformation gcMI;
    pPC->increaseGoldEx(m_Amount);
    gcMI.addLongData(MODIFY_GOLD, pPC->getGold());
    pPC->getPlayer()->sendPacket(&gcMI);

    GCSystemMessage gcSM;
    char buffer[256];
    if (pPC->isSlayer()) {
        snprintf(buffer, sizeof(buffer), "Received %u gold.", m_Amount);
        gcSM.setMessage(buffer);
        pPC->getPlayer()->sendPacket(&gcSM);
    } else if (pPC->isVampire()) {
        snprintf(buffer, sizeof(buffer), "Received %u gold.", m_Amount);
        gcSM.setMessage(buffer);
        pPC->getPlayer()->sendPacket(&gcSM);
    } else if (pPC->isOusters()) {
        snprintf(buffer, sizeof(buffer), "Received %u gold.", m_Amount);
        gcSM.setMessage(buffer);
        pPC->getPlayer()->sendPacket(&gcSM);
    }

    return OK;
}

GQuestGiveMoneyElement* GQuestGiveMoneyElement::makeElement(XMLTree* pTree) {
    GQuestGiveMoneyElement* pRet = new GQuestGiveMoneyElement;

    pTree->GetAttribute("amount", pRet->m_Amount);

    return pRet;
}

GQuestGiveMoneyElement g_GiveMoneyElement;
