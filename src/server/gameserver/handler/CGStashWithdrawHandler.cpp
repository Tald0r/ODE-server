//////////////////////////////////////////////////////////////////////////////
// Filename    : CGStashWithdrawHandler.cpp
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "CGStashWithdraw.h"

#ifdef __GAME_SERVER__
#include "GCDeleteObject.h"
#include "GCDeleteandPickUpOK.h"
#include "GamePlayer.h"
#include "Item.h"
#include "Ousters.h"
#include "Slayer.h"
#include "Vampire.h"
#include "Zone.h"
#include "item/Money.h"
#endif

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void CGStashWithdrawHandler::execute(CGStashWithdraw* pPacket, Player* pPlayer)

{
    __BEGIN_DEBUG_EX __BEGIN_DEBUG

#ifdef __GAME_SERVER__

        Assert(pPacket != NULL);
    Assert(pPlayer != NULL);

    GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(pPlayer);
    PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pGamePlayer->getCreature());
    Gold_t amount = pPacket->getAmount();

    if (!pPC->checkGoldIntegrity() || !pPC->checkStashGoldIntegrity()) {
        filelog("GoldBug.log", "CGStashWithdraw : gold does not match the DB! [%s:%s]", pGamePlayer->getID().c_str(),
                pPC->getName().c_str());
        throw DisconnectException("CGStashWithdraw : money does not match the database!");
    }

    if (amount == 0)
        return;

    if (pPC->isSlayer()) {
        Slayer* pSlayer = dynamic_cast<Slayer*>(pPC);

        if (pSlayer->getStashGold() < amount)
            return;

        if (pSlayer->getGold() + amount > MAX_MONEY) {
            Gold_t margin = MAX_MONEY - pSlayer->getGold();

            // by sigi. 2002.9.4
            pSlayer->decreaseStashGoldEx(margin);
            pSlayer->increaseGoldEx(margin);
        } else {
            // by sigi. 2002.9.4
            pSlayer->decreaseStashGoldEx(amount);
            pSlayer->increaseGoldEx(amount);
        }
    } else if (pPC->isVampire()) {
        Vampire* pVampire = dynamic_cast<Vampire*>(pPC);

        if (pVampire->getStashGold() < amount)
            return;

        if (pVampire->getGold() + amount > MAX_MONEY) {
            Gold_t margin = MAX_MONEY - pVampire->getGold();

            // by sigi. 2002.9.18
            pVampire->decreaseStashGoldEx(margin);
            pVampire->increaseGoldEx(margin);
        } else {
            // by sigi. 2002.9.18
            pVampire->decreaseStashGoldEx(amount);
            pVampire->increaseGoldEx(amount);
        }
    } else if (pPC->isOusters()) {
        Ousters* pOusters = dynamic_cast<Ousters*>(pPC);

        if (pOusters->getStashGold() < amount)
            return;

        if (pOusters->getGold() + amount > MAX_MONEY) {
            Gold_t margin = MAX_MONEY - pOusters->getGold();

            // by sigi. 2002.9.18
            pOusters->decreaseStashGoldEx(margin);
            pOusters->increaseGoldEx(margin);
        } else {
            // by sigi. 2002.9.18
            pOusters->decreaseStashGoldEx(amount);
            pOusters->increaseGoldEx(amount);
        }
    }

#endif // __GAME_SERVER__

    __END_DEBUG
    __END_DEBUG_EX
}
