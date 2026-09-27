////////////////////////////////////////////////////////////////////////////////
// Filename    : ActionRedistributeAttr.cpp
// Written By  :
// Description :
////////////////////////////////////////////////////////////////////////////////

#include "ActionRedistributeAttr.h"

#include <stdio.h>

#include "GCModifyInformation.h"
#include "GCNPCResponse.h"
#include "GCSystemMessage.h"
#include "GameContext.h"
#include "GamePlayer.h"
#include "StringPool.h"
#include "Vampire.h"
#include "VariableManager.h"
#include "Zone.h"
#include "repository/CharacterRepository.h"

////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////
void ActionRedistributeAttr::read(PropertyBuffer& propertyBuffer)

{
    __BEGIN_TRY

    try {
        // read script type
        string AttrType = propertyBuffer.getProperty("AttrType");

        if (AttrType == "STR")
            m_AttrType = 0;
        else if (AttrType == "DEX")
            m_AttrType = 1;
        else if (AttrType == "INT")
            m_AttrType = 2;
        else {
            cout << "ActionRedistributeAttr::read() : Unknown ATTR type" << endl;
            throw Error("ActionRedistributeAttr::read() : Unknown ATTR type");
        }
    } catch (NoSuchElementException& nsee) {
        cout << nsee.toString() << endl;
        throw Error(nsee.toString());
    }

    __END_CATCH
}


////////////////////////////////////////////////////////////////////////////////
// Execute the action.
////////////////////////////////////////////////////////////////////////////////
void ActionRedistributeAttr::execute(Creature* pCreature1, Creature* pCreature2)

{
    __BEGIN_TRY

    StringPool& strings = context().strings();

    Assert(pCreature1 != NULL);
    Assert(pCreature2 != NULL);
    Assert(pCreature1->isNPC());
    Assert(pCreature2->isVampire());

    Player* pPlayer = pCreature2->getPlayer();
    Assert(pPlayer != NULL);

    // Send GCNPCResponse to the client first.
    GCNPCResponse okpkt;
    pPlayer->sendPacket(&okpkt);

    Vampire* pVampire = dynamic_cast<Vampire*>(pCreature2);

    Gold_t ATTR_PRICE = context().variables().getVariable(VAMPIRE_REDISTRIBUTE_ATTR_PRICE);

    // Not having the money is an error.
    if (pVampire->getGold() < ATTR_PRICE) {
        // Close the dialogue window first.
        GCNPCResponse gcNPCResponse;
        gcNPCResponse.setCode(NPC_RESPONSE_QUIT_DIALOGUE);
        pPlayer->sendPacket(&gcNPCResponse);


        char msg[100];
        snprintf(msg, sizeof(msg), strings.c_str(STRID_NOT_ENOUGH_MONEY), pVampire->getName().c_str());

        GCSystemMessage gcSM;
        gcSM.setMessage(msg);
        pPlayer->sendPacket(&gcSM);
        return;
    }

    // A vampire may turn at most as many stat points into bonus points as it
    // has levels, so check whether that limit has already been reached.
    int RedistributedAttr = 0;
    {
        if (!defaultCharacterRepository().loadVampireRedistributeAttr(pVampire->getName(), RedistributedAttr)) {
            cerr << "ActionRedistributeAttr : No Vampire Record On Table" << endl;
            throw Error("ActionRedistributeAttr : No Vampire Record On Table");
        }

        if (RedistributedAttr >= pVampire->getLevel()) {
            // Close the dialogue window first.
            GCNPCResponse gcNPCResponse;
            gcNPCResponse.setCode(NPC_RESPONSE_QUIT_DIALOGUE);
            pPlayer->sendPacket(&gcNPCResponse);


            GCSystemMessage gcSM;
            gcSM.setMessage(strings.getString(STRID_TRANS_BONUS_POINT));
            pPlayer->sendPacket(&gcSM);
            return;
        }
    }

    // Save the previous stats before changing them.
    VAMPIRE_RECORD prev;
    pVampire->getVampireRecord(prev);

    StringStream sql;
    StringStream sql2;

    // STR redistribution
    if (m_AttrType == 0) {
        // Cannot redistribute any further once base STR is 20 or less.
        if (pVampire->getSTR(ATTR_BASIC) <= 20) {
            // Close the dialogue window first.
            GCNPCResponse gcNPCResponse;
            gcNPCResponse.setCode(NPC_RESPONSE_QUIT_DIALOGUE);
            pPlayer->sendPacket(&gcNPCResponse);


            GCSystemMessage gcSM;
            gcSM.setMessage(strings.getString(STRID_STR_LOW_LIMIT));
            pPlayer->sendPacket(&gcSM);
            return;
        }

        pVampire->setSTR(pVampire->getSTR(ATTR_BASIC) - 1, ATTR_BASIC);
        sql << "STR = " << (int)pVampire->getSTR(ATTR_BASIC);
    }
    // DEX redistribution
    else if (m_AttrType == 1) {
        if (pVampire->getDEX(ATTR_BASIC) <= 20) {
            // Close the dialogue window first.
            GCNPCResponse gcNPCResponse;
            gcNPCResponse.setCode(NPC_RESPONSE_QUIT_DIALOGUE);
            pPlayer->sendPacket(&gcNPCResponse);


            GCSystemMessage gcSM;
            gcSM.setMessage(strings.getString(STRID_DEX_LOW_LIMIT));
            pPlayer->sendPacket(&gcSM);
            return;
        }

        pVampire->setDEX(pVampire->getDEX(ATTR_BASIC) - 1, ATTR_BASIC);
        sql << "DEX = " << (int)pVampire->getDEX(ATTR_BASIC);
    }
    // INT redistribution
    else if (m_AttrType == 2) {
        if (pVampire->getINT(ATTR_BASIC) <= 20) {
            // Close the dialogue window first.
            GCNPCResponse gcNPCResponse;
            gcNPCResponse.setCode(NPC_RESPONSE_QUIT_DIALOGUE);
            pPlayer->sendPacket(&gcNPCResponse);


            GCSystemMessage gcSM;
            gcSM.setMessage(strings.getString(STRID_INT_LOW_LIMIT));
            pPlayer->sendPacket(&gcSM);
            return;
        }

        pVampire->setINT(pVampire->getINT(ATTR_BASIC) - 1, ATTR_BASIC);
        sql << "INTE = " << (int)pVampire->getINT(ATTR_BASIC);
    } else {
        Assert(false);
    }

    // Save the reduced stat,
    // raise the bonus since the stat went down,
    // and take the gold.
    pVampire->tinysave(sql.toString());
    pVampire->setBonus(pVampire->getBonus() + 1);
    sql2 << "Bonus = " << (int)pVampire->getBonus();
    pVampire->tinysave(sql2.toString());
    pVampire->decreaseGoldEx(ATTR_PRICE);

    GCModifyInformation gcMI;
    gcMI.addShortData(MODIFY_BONUS_POINT, pVampire->getBonus());
    gcMI.addLongData(MODIFY_GOLD, pVampire->getGold());

    pVampire->initAllStat();
    pVampire->addModifyInfo(prev, gcMI);
    pVampire->sendRealWearingInfo();
    pPlayer->sendPacket(&gcMI);

    // Store the amount of stats converted.
    defaultCharacterRepository().saveVampireRedistributeAttr(RedistributedAttr + 1, pVampire->getName());

    __END_CATCH
}


////////////////////////////////////////////////////////////////////////////////
// get debug string
////////////////////////////////////////////////////////////////////////////////
string ActionRedistributeAttr::toString() const

{
    __BEGIN_TRY

    StringStream msg;
    msg << "ActionRedistributeAttr(" << ")";
    return msg.toString();

    __END_CATCH
}
