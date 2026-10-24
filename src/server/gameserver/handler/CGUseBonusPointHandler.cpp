//////////////////////////////////////////////////////////////////////////////
// Filename    : CGUseBonusPointHandler.cpp
// Written By  : elca@ewestsoft.com
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "CGUseBonusPoint.h"

#ifdef __GAME_SERVER__
#include "Creature.h"
#include "GCLearnSkillReady.h"
#include "GCUseBonusPointFail.h"
#include "GCUseBonusPointOK.h"
#include "GamePlayer.h"
#include "SkillHandlerManager.h"
#include "SkillInfo.h"
#include "SkillParentInfo.h"
#include "Slayer.h"
#endif

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void CGUseBonusPointHandler::execute(CGUseBonusPoint* pPacket, Player* pPlayer)

{
    __BEGIN_DEBUG_EX __BEGIN_DEBUG

#ifdef __GAME_SERVER__

        Assert(pPacket != NULL);
    Assert(pPlayer != NULL);

    // Return if the state is not normal
    GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(pPlayer);
    if (pGamePlayer->getPlayerStatus() != GPS_NORMAL)
        return;

    Creature* pCreature = pGamePlayer->getCreature();
    BYTE which = pPacket->getWhich();
    Attr_t cur = 0;

    if (pCreature->isSlayer()) {
        Slayer* pSlayer = dynamic_cast<Slayer*>(pCreature);
        if (pSlayer->getBonus() <= 0) {
            GCUseBonusPointFail failPkt;
            pPlayer->sendPacket(&failPkt);
            return;
        }

        bool success = false;
        if (which == INC_INT) {
            success = pSlayer->putAdvancedBonusToINT();
        } else if (which == INC_STR) {
            success = pSlayer->putAdvancedBonusToSTR();
        } else if (which == INC_DEX) {
            success = pSlayer->putAdvancedBonusToDEX();
        }

        if (success) {
            GCUseBonusPointOK okpkt;
            pGamePlayer->sendPacket(&okpkt);

            pSlayer->saveExps();
            pSlayer->initAllStatAndSend();
        } else {
            GCUseBonusPointFail failPkt;
            pPlayer->sendPacket(&failPkt);
            return;
        }
    } else if (pCreature->isVampire()) {
        // Return if there is no bonus point
        Vampire* pVampire = dynamic_cast<Vampire*>(pCreature);
        if (pVampire->getBonus() <= 0) {
            GCUseBonusPointFail failPkt;
            pPlayer->sendPacket(&failPkt);
            return;
        }

        VAMPIRE_RECORD oldRecord;
        // Save the existing stats before raising them.
        pVampire->getVampireRecord(oldRecord);

        if (which == INC_INT) {
            cur = pVampire->getINT(ATTR_BASIC) + 1;
            pVampire->setINT(cur, ATTR_BASIC);

            StringStream sst;
            sst << "INTE = " << (int)cur;
            pVampire->tinysave(sst.toString());
        } else if (which == INC_STR) {
            cur = pVampire->getSTR(ATTR_BASIC) + 1;
            pVampire->setSTR(cur, ATTR_BASIC);

            StringStream sst;
            sst << "STR = " << (int)cur;
            pVampire->tinysave(sst.toString());
        } else if (which == INC_DEX) {
            cur = pVampire->getDEX(ATTR_BASIC) + 1;
            pVampire->setDEX(cur, ATTR_BASIC);

            StringStream sst;
            sst << "DEX = " << (int)cur;
            pVampire->tinysave(sst.toString());
        }

        // Save the changed bonus points.
        Bonus_t OldBonus = pVampire->getBonus();
        pVampire->setBonus(OldBonus - 1);
        StringStream sst;
        sst << "Bonus = " << (int)(OldBonus - 1);
        pVampire->tinysave(sst.toString());

        // The stats changed, so refresh stat.
        pVampire->initAllStat();

        // Because of a bug caused by the client's computation order,
        // the verify packet is sent first and the changed stats after it.
        // Later the changed stat information should be carried
        // in CGUseBonusPointOK itself.

        // Send the OK packet.
        GCUseBonusPointOK okpkt;
        pGamePlayer->sendPacket(&okpkt);

        // Send the information about the changed stats.
        pVampire->sendModifyInfo(oldRecord);
        pVampire->sendRealWearingInfo();
    } else if (pCreature->isOusters()) {
        // Return if there is no bonus point
        Ousters* pOusters = dynamic_cast<Ousters*>(pCreature);
        if (pOusters->getBonus() <= 0) {
            GCUseBonusPointFail failPkt;
            pPlayer->sendPacket(&failPkt);
            return;
        }

        OUSTERS_RECORD oldRecord;
        // Save the existing stats before raising them.
        pOusters->getOustersRecord(oldRecord);

        if (which == INC_INT) {
            cur = pOusters->getINT(ATTR_BASIC) + 1;
            pOusters->setINT(cur, ATTR_BASIC);

            StringStream sst;
            sst << "INTE = " << (int)cur;
            pOusters->tinysave(sst.toString());
        } else if (which == INC_STR) {
            cur = pOusters->getSTR(ATTR_BASIC) + 1;
            pOusters->setSTR(cur, ATTR_BASIC);

            StringStream sst;
            sst << "STR = " << (int)cur;
            pOusters->tinysave(sst.toString());
        } else if (which == INC_DEX) {
            cur = pOusters->getDEX(ATTR_BASIC) + 1;
            pOusters->setDEX(cur, ATTR_BASIC);

            StringStream sst;
            sst << "DEX = " << (int)cur;
            pOusters->tinysave(sst.toString());
        }

        // Save the changed bonus points.
        Bonus_t OldBonus = pOusters->getBonus();
        pOusters->setBonus(OldBonus - 1);
        StringStream sst;
        sst << "Bonus = " << (int)(OldBonus - 1);
        pOusters->tinysave(sst.toString());

        // The stats changed, so refresh stat.
        pOusters->initAllStat();

        // Send the OK packet.
        GCUseBonusPointOK okpkt;
        pGamePlayer->sendPacket(&okpkt);

        // Send the information about the changed stats.
        pOusters->sendModifyInfo(oldRecord);
        pOusters->sendRealWearingInfo();
    } else {
        GCUseBonusPointFail failPkt;
        pPlayer->sendPacket(&failPkt);
        return;
    }


#endif // __GAME_SERVER__

    __END_DEBUG
    __END_DEBUG_EX
}
