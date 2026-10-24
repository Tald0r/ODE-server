//////////////////////////////////////////////////////////////////////
//
// Filename    : GCFriendChattingHandle.cpp
// Written By  : aliveviva@gmail.com
// Description :
//
//////////////////////////////////////////////////////////////////////

// include files
#include "GCFriendChatting.h"

#ifdef __GAME_SERVER__
#include "GameContext.h"
#include "GamePlayer.h"
#include "PCFinder.h"
#include "repository/FriendRepository.h"
#endif

void GCFriendChattingHandler::execute(GCFriendChatting* pPacket, Player* pPlayer) {
    __BEGIN_DEBUG_EX

#ifdef __GAME_SERVER__
    Assert(pPacket != NULL);
    Assert(pPlayer != NULL);

    GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(pPlayer);
    Creature* pCreature = pGamePlayer->getCreature();

    DWORD Command = pPacket->getCommand();

    if (Command > MAX_CG)
        throw InvalidProtocolException("Command Error");
    switch (Command) {
        /////////////////////////////////////////////////CG_ADD_FRIEND_AGREE/////////////////////////////////////////
    case CG_ADD_FRIEND_AGREE: {
        {
            FriendRepository& friends = defaultFriendRepository();

            // One row per direction: the roster is mutual by construction.
            friends.insertFriend(pCreature->getName(), pPacket->getPlayerName());
            friends.insertFriend(pPacket->getPlayerName(), pCreature->getName());
        }

        Creature* pTargetCreature = NULL;
        PCFinder& pcFinder = de::gameContext().playerCreatures();

        __ENTER_CRITICAL_SECTION(pcFinder)
        pTargetCreature = pcFinder.getCreature_LOCKED(pPacket->getPlayerName());
        __LEAVE_CRITICAL_SECTION(pcFinder)

        if (pTargetCreature != NULL) {
            Player* pTargetPlayer = pTargetCreature->getPlayer();
            if (pTargetPlayer == NULL)
                break;
            GamePlayer* pTargetGamePlayer = dynamic_cast<GamePlayer*>(pTargetPlayer);
            if (pTargetGamePlayer == NULL)
                break;

            GCFriendChatting gcFriend;
            gcFriend.setCommand(GC_ADD_FRIEND_OK);
            gcFriend.setPlayerName(pCreature->getName());
            pTargetGamePlayer->sendPacket(&gcFriend);

            GCFriendChatting gcFriend2;
            gcFriend2.setCommand(GC_ADD_FRIEND_OK);
            gcFriend2.setPlayerName(pPacket->getPlayerName());
            pGamePlayer->sendPacket(&gcFriend2);
        }
        break;
    }
        ///////////////////////////////////////////////CG_ADD_FRIEND///////////////////////////////////////////////////
    case CG_ADD_FRIEND: {
        Creature* pTargetCreature = NULL;
        PCFinder& pcFinder = de::gameContext().playerCreatures();

        __ENTER_CRITICAL_SECTION(pcFinder)
        pTargetCreature = pcFinder.getCreature_LOCKED(pPacket->getPlayerName());
        __LEAVE_CRITICAL_SECTION(pcFinder)
        bool blResult = true;
        if (pTargetCreature != NULL) {
            Player* pTargetPlayer = pTargetCreature->getPlayer();
            if (pTargetPlayer == NULL)
                break;
            GamePlayer* pTargetGamePlayer = dynamic_cast<GamePlayer*>(pTargetPlayer);
            if (pTargetGamePlayer == NULL)
                break;
            {
                FriendRepository& friends = defaultFriendRepository();
                ////////////////////GC_ADD_FRIEND_EXIST//////////////////
                if (friends.friendExists(pCreature->getName(), pPacket->getPlayerName())) {
                    blResult = false;
                    GCFriendChatting gcFriend3;
                    gcFriend3.setCommand(GC_ADD_FRIEND_EXIST);
                    gcFriend3.setPlayerName(pPacket->getPlayerName());
                    pGamePlayer->sendPacket(&gcFriend3);
                }
                /////////////////GC_ADD_FRIEND_BLACK///////////////////////
                // Asked with the OTHER character as owner: "has the person
                // I am adding blacklisted ME?"
                if (friends.hasBlacklisted(pPacket->getPlayerName(), pCreature->getName())) {
                    blResult = false;
                    GCFriendChatting gcFriend4;
                    gcFriend4.setCommand(GC_ADD_FRIEND_BLACK);
                    gcFriend4.setPlayerName(pPacket->getPlayerName());
                    pGamePlayer->sendPacket(&gcFriend4);
                }
            }

            if (blResult) {
                GCFriendChatting gcFriend;
                gcFriend.setCommand(GC_ADD_FRIEND_REQUEST);
                gcFriend.setPlayerName(pCreature->getName());
                pTargetGamePlayer->sendPacket(&gcFriend);

                GCFriendChatting gcFriend2;
                gcFriend2.setCommand(GC_ADD_FRIEND_WAIT);
                gcFriend2.setPlayerName(pPacket->getPlayerName());
                pGamePlayer->sendPacket(&gcFriend2);
            }
        }
        break;
    }
        ////////////////////////////////////////////////////CG_MESSAGE//////////////////////////////////////////////
    case CG_MESSAGE: {
        Creature* pTargetCreature = NULL;
        PCFinder& pcFinder = de::gameContext().playerCreatures();

        __ENTER_CRITICAL_SECTION(pcFinder)
        pTargetCreature = pcFinder.getCreature_LOCKED(pPacket->getPlayerName());
        __LEAVE_CRITICAL_SECTION(pcFinder)

        if (pTargetCreature != NULL) {
            Player* pTargetPlayer = pTargetCreature->getPlayer();
            if (pTargetPlayer == NULL)
                break;
            GamePlayer* pTargetGamePlayer = dynamic_cast<GamePlayer*>(pTargetPlayer);
            if (pTargetGamePlayer == NULL)
                break;
            GCFriendChatting gcFriend;
            gcFriend.setCommand(GC_MESSAGE);
            gcFriend.setPlayerName(pCreature->getName());
            gcFriend.setMessage(pPacket->getMessage());
            pTargetGamePlayer->sendPacket(&gcFriend);
        } else {
            defaultFriendRepository().insertMessage(pPacket->getMessage(), pPacket->getPlayerName(),
                                                    pCreature->getName());
        }
        break;
    }
    ///////////////////////////////////////////CG_GETSTATE//////////////////////////////////////////////////////////
    case CG_UPDATE: {
        {
            FriendRepository& friends = defaultFriendRepository();

            // Both rosters are read in full before anything is sent. The
            // inline version walked each Result while sending packets, so
            // a Throwable out of sendPacket escaped mid-iteration and
            // leaked the Statement -- END_DB catches only
            // SQLQueryException.
            vector<FriendListRow> roster = friends.loadFriends(pCreature->getName());

            for (size_t r = 0; r < roster.size(); r++) {
                GCFriendChatting gcFriend;
                gcFriend.setCommand(GC_UPDATE);
                gcFriend.setPlayerName(roster[r].friendName);
                gcFriend.setIsBlack(roster[r].isBlack);

                Creature* pTargetCreature = NULL;
                PCFinder& pcFinder = de::gameContext().playerCreatures();

                __ENTER_CRITICAL_SECTION(pcFinder)
                pTargetCreature = pcFinder.getCreature_LOCKED(gcFriend.getPlayerName());
                __LEAVE_CRITICAL_SECTION(pcFinder)

                if (pTargetCreature == NULL)
                    gcFriend.setIsOnLine(0);
                else
                    gcFriend.setIsOnLine(1);

                pGamePlayer->sendPacket(&gcFriend);
            }

            vector<FriendMessageRow> spool = friends.loadMessages(pCreature->getName());
            bool IsHave = false;
            for (size_t m = 0; m < spool.size(); m++) {
                IsHave = true;
                GCFriendChatting gcFriend2;
                gcFriend2.setCommand(GC_MESSAGE);
                gcFriend2.setMessage(spool[m].message);
                gcFriend2.setPlayerName(spool[m].friendName);

                pGamePlayer->sendPacket(&gcFriend2);
            }
            // IsHave rather than !spool.empty(): the flag is the inline
            // code's, and it is set inside the loop, so the two agree.
            if (IsHave)
                friends.deleteMessages(pCreature->getName());
        }

        break;
    }
        ////////////////////////////////////////CG_ADD_FRIEND_REFUSE/////////////////////////////
    case CG_ADD_FRIEND_REFUSE: {
        Creature* pTargetCreature = NULL;
        PCFinder& pcFinder = de::gameContext().playerCreatures();

        __ENTER_CRITICAL_SECTION(pcFinder)
        pTargetCreature = pcFinder.getCreature_LOCKED(pPacket->getPlayerName());
        __LEAVE_CRITICAL_SECTION(pcFinder)

        if (pTargetCreature == NULL) {
            GCFriendChatting gcFriend;
            gcFriend.setCommand(GC_ADD_FRIEND_ERROR);
            pGamePlayer->sendPacket(&gcFriend);
        } else {
            Player* pTargetPlayer = pTargetCreature->getPlayer();
            if (pTargetPlayer == NULL)
                break;
            GamePlayer* pTargetGamePlayer = dynamic_cast<GamePlayer*>(pTargetPlayer);
            if (pTargetGamePlayer == NULL)
                break;
            GCFriendChatting gcFriend;
            gcFriend.setCommand(GC_ADD_FRIEND_REFUSE);
            gcFriend.setPlayerName(pCreature->getName());
            pTargetGamePlayer->sendPacket(&gcFriend);
        }
        break;
    }
        /////////////////////////////////////CG_ADD_FRIEND_BLACK///////////////////////////////////////
    case CG_ADD_FRIEND_BLACK: {
        defaultFriendRepository().insertBlacklisted(pPacket->getPlayerName(), pCreature->getName());

        Creature* pTargetCreature = NULL;
        PCFinder& pcFinder = de::gameContext().playerCreatures();

        __ENTER_CRITICAL_SECTION(pcFinder)
        pTargetCreature = pcFinder.getCreature_LOCKED(pPacket->getPlayerName());
        __LEAVE_CRITICAL_SECTION(pcFinder)

        if (pTargetCreature == NULL) {
            GCFriendChatting gcFriend;
            gcFriend.setCommand(GC_ADD_FRIEND_ERROR);
            pGamePlayer->sendPacket(&gcFriend);
        } else {
            Player* pTargetPlayer = pTargetCreature->getPlayer();
            if (pTargetPlayer == NULL)
                break;
            GamePlayer* pTargetGamePlayer = dynamic_cast<GamePlayer*>(pTargetPlayer);
            if (pTargetGamePlayer == NULL)
                break;
            GCFriendChatting gcFriend;
            gcFriend.setCommand(GC_ADD_FRIEND_REFUSE);
            gcFriend.setPlayerName(pCreature->getName());
            pTargetGamePlayer->sendPacket(&gcFriend);
        }
        break;
    }
        //////////////////////////////////////////////////////CG_FRIEND_DELTET//////////////////////////////////
    case CG_FRIEND_DELETE: {
        {
            FriendRepository& friends = defaultFriendRepository();

            // One direction each, like the insert pair -- but NOT in the
            // same parameter order: the inserts take (friend, owner)
            // and these take (owner, friend), mirroring the statements.
            friends.deleteFriend(pCreature->getName(), pPacket->getPlayerName());
            friends.deleteFriend(pPacket->getPlayerName(), pCreature->getName());
        }

        GCFriendChatting gcFriend;
        gcFriend.setCommand(GC_FRIEND_DELETE);
        gcFriend.setPlayerName(pPacket->getPlayerName());
        pGamePlayer->sendPacket(&gcFriend);

        Creature* pTargetCreature = NULL;
        PCFinder& pcFinder = de::gameContext().playerCreatures();

        __ENTER_CRITICAL_SECTION(pcFinder)
        pTargetCreature = pcFinder.getCreature_LOCKED(pPacket->getPlayerName());
        __LEAVE_CRITICAL_SECTION(pcFinder)

        if (pTargetCreature) {
            Player* pTargetPlayer = pTargetCreature->getPlayer();
            if (pTargetPlayer == NULL)
                break;
            GamePlayer* pTargetGamePlayer = dynamic_cast<GamePlayer*>(pTargetPlayer);
            if (pTargetGamePlayer == NULL)
                break;
            GCFriendChatting gcFriend2;
            gcFriend2.setCommand(GC_FRIEND_DELETE);
            gcFriend2.setPlayerName(pCreature->getName());
            pTargetGamePlayer->sendPacket(&gcFriend2);
        }
        break;
    }
        ////////////////////////////////////////////DEFAULT/////////////////////////////////////////////////////////////////
    default:
        break;
    }

#endif // __GAME_SERVER__

    __END_DEBUG_EX
}
