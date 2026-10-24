//////////////////////////////////////////////////////////////////////////////
// Filename    : CGModifyNicknameHandler.cc
// Written By  : elca@ewestsoft.com
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "CGModifyNickname.h"

#ifdef __GAME_SERVER__
#include "CreatureUtil.h"
#include "GCModifyNickname.h"
#include "GCNicknameVerify.h"
#include "GCUseOK.h"
#include "GamePlayer.h"
#include "Inventory.h"
#include "NicknameBook.h"
#include "NicknameInfo.h"
#include "PetInfo.h"
#include "PlayerCreature.h"
#include "Zone.h"
#include "item/EventGiftBox.h"
#include "item/PetItem.h"
#include "repository/NicknameRepository.h"
#endif

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void CGModifyNicknameHandler::execute(CGModifyNickname* pPacket, Player* pPlayer)

{
    __BEGIN_DEBUG_EX

#ifdef __GAME_SERVER__

    Assert(pPacket != NULL);
    Assert(pPlayer != NULL);

    GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(pPlayer);
    Assert(pGamePlayer != NULL);

    PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pGamePlayer->getCreature());
    Assert(pPC != NULL);

    Inventory* pInventory = pPC->getInventory();
    Assert(pInventory != NULL);

    GCNicknameVerify gcNV;

    ObjectID_t itemOID = pPacket->getItemObjectID();
    EventGiftBox* pItem = NULL;

    if (itemOID != 0) {
        pItem = dynamic_cast<EventGiftBox*>(pInventory->getItemWithObjectID(itemOID));

        if (pItem == NULL) {
            filelog("Nickname.log", "[%s:%s] No item, or it is not an EventGiftBox.", pPlayer->getID().c_str(),
                    pPC->getName().c_str());

            gcNV.setCode(NICKNAME_MODIFY_FAIL_NO_ITEM);
            pGamePlayer->sendPacket(&gcNV);
            return;
        }
    } else {
        PetInfo* pPetInfo = pPC->getPetInfo();
        if (pPetInfo == NULL || pPetInfo->getPetType() != PET_PACKAGE) {
            gcNV.setCode(NICKNAME_MODIFY_FAIL_NO_ITEM);
            pGamePlayer->sendPacket(&gcNV);
            return;
        }

        pPetInfo->setNickname(pPacket->getNickname());
        pPetInfo->getPetItem()->savePetInfo();
        sendPetInfo(pGamePlayer, true);

        gcNV.setCode(NICKNAME_MODIFY_OK);
        pGamePlayer->sendPacket(&gcNV);
        return;
    }

    switch (pItem->getItemType()) {
    // Change the pet nickname
    case 23: {
        PetInfo* pPetInfo = pPC->getPetInfo();
        if (pPetInfo == NULL) {
            gcNV.setCode(NICKNAME_MODIFY_FAIL_NO_ITEM);
            pGamePlayer->sendPacket(&gcNV);
            return;
        }

        if (pPetInfo->getPetType() != PET_PACKAGE && pItem == NULL) {
            gcNV.setCode(NICKNAME_MODIFY_FAIL_NO_ITEM);
            pGamePlayer->sendPacket(&gcNV);
            return;
        }

        pPetInfo->setNickname(pPacket->getNickname());
        pPetInfo->getPetItem()->savePetInfo();
        sendPetInfo(pGamePlayer, true);

        gcNV.setCode(NICKNAME_MODIFY_OK);
        pGamePlayer->sendPacket(&gcNV);

        if (pPetInfo->getPetType() != PET_PACKAGE) {
            pInventory->deleteItem(itemOID);
            pItem->destroy();
            SAFE_DELETE(pItem);

            GCUseOK gcOK;
            pGamePlayer->sendPacket(&gcOK);
        }

        return;
    }

    // Change the custom nickname
    case 22:
    case 25: {
        NicknameInfo* pNickname = pPC->getNicknameBook()->getNicknameInfo(0);

        if (pNickname == NULL || pNickname->getNicknameType() != NicknameInfo::NICK_CUSTOM ||
            pPacket->getNickname() == "") {
            gcNV.setCode(NICKNAME_MODIFY_FAIL_NO_ITEM);
            pGamePlayer->sendPacket(&gcNV);
            return;
        }

        if (pItem == NULL) {
            gcNV.setCode(NICKNAME_MODIFY_FAIL_NO_ITEM);
            pGamePlayer->sendPacket(&gcNV);
            return;
        }

        pNickname->setNickname(pPacket->getNickname());

        pPC->getNicknameBook()->repository().updateNickname(pPC->getName(), pNickname->getNicknameID(),
                                                            pNickname->getNickname());

        gcNV.setCode(NICKNAME_MODIFY_OK);
        pGamePlayer->sendPacket(&gcNV);

        if (pItem->getItemType() == 22) {
            pInventory->deleteItem(itemOID);
            pItem->destroy();
            SAFE_DELETE(pItem);

            GCUseOK gcOK;
            pGamePlayer->sendPacket(&gcOK);
        }

        if (pPC->getNickname() == pNickname) {
            GCModifyNickname gcMN;
            gcMN.setObjectID(pPC->getObjectID());
            gcMN.setNicknameInfo(pNickname);
            pPC->getZone()->broadcastPacket(pPC->getX(), pPC->getY(), &gcMN);
        }

        break;
    }

    // Add a nickname
    case 24: {
        if (pItem == NULL) {
            gcNV.setCode(NICKNAME_MODIFY_FAIL_NO_ITEM);
            pGamePlayer->sendPacket(&gcNV);
            return;
        }

        NicknameInfo* pNickname = new NicknameInfo;
        pNickname->setNicknameID(pPC->getNicknameBook()->popNicknameID());
        pNickname->setNicknameType(NicknameInfo::NICK_CUSTOM);
        pNickname->setNickname(pPacket->getNickname());

        pPC->getNicknameBook()->setNicknameInfo(pNickname->getNicknameID(), pNickname);

        pPC->getNicknameBook()->repository().insert(pPC->getName(), pNickname->getNicknameID(),
                                                    pNickname->getNicknameType(), pNickname->getNickname());

        gcNV.setCode(NICKNAME_MODIFY_OK);
        pGamePlayer->sendPacket(&gcNV);

        pGamePlayer->sendPacket(pPC->getNicknameBook()->getNicknameBookListPacket().get());

        pInventory->deleteItem(itemOID);
        pItem->destroy();
        SAFE_DELETE(pItem);

        GCUseOK gcOK;
        pGamePlayer->sendPacket(&gcOK);

        break;
    }
    }

#endif // __GAME_SERVER__

    __END_DEBUG_EX
}
