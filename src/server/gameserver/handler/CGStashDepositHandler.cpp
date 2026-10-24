//////////////////////////////////////////////////////////////////////////////
// Filename    : CGStashDepositHandler.cpp
// Written By  : excel96
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "CGStashDeposit.h"

#ifdef __GAME_SERVER__
#include "GCDeleteObject.h"
#include "GCDeleteandPickUpOK.h"
#include "GamePlayer.h"
#include "Item.h"
#include "Ousters.h"
#include "Slayer.h"
#include "Utility.h"
#include "Vampire.h"
#include "Zone.h"
#include "item/Money.h"
#endif

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void CGStashDepositHandler::execute(CGStashDeposit* pPacket, Player* pPlayer) {
    __BEGIN_DEBUG_EX

#ifdef __GAME_SERVER__

        Assert(pPacket != NULL);
    Assert(pPlayer != NULL);

    GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(pPlayer);
    PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pGamePlayer->getCreature());
    Gold_t amount = pPacket->getAmount();

    if (!pPC->checkGoldIntegrity() || !pPC->checkStashGoldIntegrity()) {
        filelog("GoldBug.log", "CGStashDeposit : gold does not match the DB! [%s:%s]", pGamePlayer->getID().c_str(),
                pPC->getName().c_str());
        throw DisconnectException("CGStashDeposit : money does not match the database!");
    }


    if (amount == 0)
        return;

    if (pPC->isSlayer()) {
        Slayer* pSlayer = dynamic_cast<Slayer*>(pPC);

        if (pSlayer->getGold() < amount)
            return;

        // When more money than the stash can hold is put in,
        // only part goes in and the rest stays with the player.
        if (pSlayer->getStashGold() + amount > MAX_MONEY) {
            Gold_t margin = MAX_MONEY - pSlayer->getStashGold();
            pSlayer->decreaseGoldEx(margin);
            pSlayer->increaseStashGoldEx(margin);
        } else {
            pSlayer->decreaseGoldEx(amount);
            pSlayer->increaseStashGoldEx(amount);
        }
    } else if (pPC->isVampire()) {
        Vampire* pVampire = dynamic_cast<Vampire*>(pPC);

        if (pVampire->getGold() < amount)
            return;

        if (pVampire->getStashGold() + amount > MAX_MONEY) {
            Gold_t margin = MAX_MONEY - pVampire->getStashGold();
            pVampire->decreaseGoldEx(margin);
            pVampire->increaseStashGoldEx(margin);
        } else {
            pVampire->decreaseGoldEx(amount);
            pVampire->increaseStashGoldEx(amount);
        }
    } else if (pPC->isOusters()) {
        Ousters* pOusters = dynamic_cast<Ousters*>(pPC);

        if (pOusters->getGold() < amount)
            return;

        if (pOusters->getStashGold() + amount > MAX_MONEY) {
            Gold_t margin = MAX_MONEY - pOusters->getStashGold();
            pOusters->decreaseGoldEx(margin);
            pOusters->increaseStashGoldEx(margin);
        } else {
            pOusters->decreaseGoldEx(amount);
            pOusters->increaseStashGoldEx(amount);
        }
    }

#endif // __GAME_SERVER__

    __END_DEBUG_EX
}
