//////////////////////////////////////////////////////////////////////////////
// Filename    : GamePacketDispatch.cpp
// Description : the gameserver composition root: every packet id the
//               gameserver receives from clients is bound to its handler
//               here. All CG
//               packets (keep alphabetical), plus the handful of GC
//               packets the live client sends server-ward.
//////////////////////////////////////////////////////////////////////////////

#include "GamePacketDispatch.h"

#include "CGAbsorbSoul.h"
#include "CGAcceptUnion.h"
#include "CGAddGearToMouse.h"
#include "CGAddInventoryToMouse.h"
#include "CGAddItemToCodeSheet.h"
#include "CGAddItemToItem.h"
#include "CGAddMouseToGear.h"
#include "CGAddMouseToInventory.h"
#include "CGAddMouseToQuickSlot.h"
#include "CGAddMouseToZone.h"
#include "CGAddQuickSlotToMouse.h"
#include "CGAddSMSAddress.h"
#include "CGAddZoneToInventory.h"
#include "CGAddZoneToMouse.h"
#include "CGAppointSubmaster.h"
#include "CGAttack.h"
#include "CGAuthKey.h"
#include "CGBloodDrain.h"
#include "CGBuyStoreItem.h"
#include "CGCastingSkill.h"
#include "CGConnect.h"
#include "CGConnectSetKey.h"
#include "CGCrashReport.h"
#include "CGDeleteSMSAddress.h"
#include "CGDenyUnion.h"
#include "CGDepositPet.h"
#include "CGDisplayItem.h"
#include "CGDissectionCorpse.h"
#include "CGDonationMoney.h"
#include "CGDownSkill.h"
#include "CGDropMoney.h"
#include "CGExchangeBuy.h"
#include "CGExchangeList.h"
#include "CGExpelGuild.h"
#include "CGExpelGuildMember.h"
#include "CGFailQuest.h"
#include "CGGQuestAccept.h"
#include "CGGQuestCancel.h"
#include "CGGetEventItem.h"
#include "CGGetOffMotorCycle.h"
#include "CGGlobalChat.h"
#include "CGGuildChat.h"
#include "CGJoinGuild.h"
#include "CGLearnSkill.h"
#include "CGLogout.h"
#include "CGLotterySelect.h"
#include "CGMakeItem.h"
#include "CGMixItem.h"
#include "CGModifyGuildIntro.h"
#include "CGModifyGuildMember.h"
#include "CGModifyGuildMemberIntro.h"
#include "CGModifyNickname.h"
#include "CGModifyTaxRatio.h"
#include "CGMouseToStash.h"
#include "CGMove.h"
#include "CGNPCAskAnswer.h"
#include "CGNPCTalk.h"
#include "CGPartyInvite.h"
#include "CGPartyLeave.h"
#include "CGPartyPosition.h"
#include "CGPartySay.h"
#include "CGPetGamble.h"
#include "CGPickupMoney.h"
#include "CGPortCheck.h"
#include "CGQuitGuild.h"
#include "CGQuitUnion.h"
#include "CGQuitUnionAccept.h"
#include "CGQuitUnionDeny.h"
#include "CGRangerSay.h"
#include "CGReady.h"
#include "CGRegistGuild.h"
#include "CGRelicToObject.h"
#include "CGReloadFromInventory.h"
#include "CGReloadFromQuickSlot.h"
#include "CGRequestGuildList.h"
#include "CGRequestGuildMemberList.h"
#include "CGRequestIP.h"
#include "CGRequestInfo.h"
#include "CGRequestNewbieItem.h"
#include "CGRequestPowerPoint.h"
#include "CGRequestRepair.h"
#include "CGRequestStoreInfo.h"
#include "CGRequestUnion.h"
#include "CGRequestUnionInfo.h"
#include "CGResurrect.h"
#include "CGRideMotorCycle.h"
#include "CGSMSAddressList.h"
#include "CGSMSSend.h"
#include "CGSay.h"
#include "CGSelectBloodBible.h"
#include "CGSelectGuild.h"
#include "CGSelectGuildMember.h"
#include "CGSelectNickname.h"
#include "CGSelectPortal.h"
#include "CGSelectQuest.h"
#include "CGSelectRankBonus.h"
#include "CGSelectRegenZone.h"
#include "CGSelectTileEffect.h"
#include "CGSelectWayPoint.h"
#include "CGSetSlayerHotKey.h"
#include "CGSetVampireHotKey.h"
#include "CGShopRequestBuy.h"
#include "CGShopRequestList.h"
#include "CGShopRequestSell.h"
#include "CGSilverCoating.h"
#include "CGSkillToInventory.h"
#include "CGSkillToNamed.h"
#include "CGSkillToObject.h"
#include "CGSkillToSelf.h"
#include "CGSkillToTile.h"
#include "CGStashDeposit.h"
#include "CGStashList.h"
#include "CGStashRequestBuy.h"
#include "CGStashToMouse.h"
#include "CGStashWithdraw.h"
#include "CGStoreClose.h"
#include "CGStoreOpen.h"
#include "CGStoreSign.h"
#include "CGSubmitScore.h"
#include "CGTakeOutGood.h"
#include "CGTameMonster.h"
#include "CGThrowBomb.h"
#include "CGThrowItem.h"
#include "CGTradeAddItem.h"
#include "CGTradeFinish.h"
#include "CGTradeMoney.h"
#include "CGTradePrepare.h"
#include "CGTradeRemoveItem.h"
#include "CGTryJoinGuild.h"
#include "CGTypeStringList.h"
#include "CGUnburrow.h"
#include "CGUndisplayItem.h"
#include "CGUntransform.h"
#include "CGUseBonusPoint.h"
#include "CGUseItemFromGQuestInventory.h"
#include "CGUseItemFromGear.h"
#include "CGUseItemFromInventory.h"
#include "CGUseMessageItemFromInventory.h"
#include "CGUsePotionFromInventory.h"
#include "CGUsePotionFromQuickSlot.h"
#include "CGUsePowerPoint.h"
#include "CGVerifyTime.h"
#include "CGVisible.h"
#include "CGWhisper.h"
#include "CGWithdrawPet.h"
#include "CGWithdrawTax.h"
#include "GCAddStoreItem.h"
#include "GCFriendChatting.h"
#include "GCRemoveStoreItem.h"
#include "GGCommand.h"
#include "GGGuildChat.h"
#include "GGServerChat.h"
#include "GameClientLink.h"
#include "LGIncomingConnection.h"
#include "LGIncomingConnectionError.h"
#include "LGIncomingConnectionOK.h"
#include "LGKickCharacter.h"
#include "PacketDispatcher.h"
#include "SGAddGuildMemberOK.h"
#include "SGAddGuildOK.h"
#include "SGDeleteGuildOK.h"
#include "SGExpelGuildMemberOK.h"
#include "SGGuildInfo.h"
#include "SGGuildMemberLogOnOK.h"
#include "SGModifyGuildIntroOK.h"
#include "SGModifyGuildMemberOK.h"
#include "SGModifyGuildOK.h"
#include "SGQuitGuildOK.h"

namespace {

// The links the gameserver accepts, enforced on every registration below.
// CG is the client link. GC is here because a handful of packets named for
// the server -> client direction travel client -> server on the live wire
// (GCFriendChatting and the personal-store pair; see the registrations and
// the thunks below). GC is narrowed to those packets, the ones the client
// link admits (GameClientLink.h), so a GC handler the link would never
// deliver does not compile. LG, SG and GG arrive from the
// loginserver, the sharedserver and the other gameservers.
constexpr de::packet::DirectionSet kReceivedDirections =
    de::packet::DirectionSet{de::packet::Direction::CG, de::packet::Direction::GC, de::packet::Direction::GG,
                             de::packet::Direction::LG, de::packet::Direction::SG}
        .narrowed(de::packet::Direction::GC, &de::packet::sentOnGameClientLink);

// CGPortCheckHandler::execute takes no player.
void dispatchCGPortCheck(Packet* pPacket, Player*) {
    CGPortCheckHandler::execute(static_cast<CGPortCheck*>(pPacket));
}

// CGStashList kept a __BEGIN_DEBUG/__END_DEBUG wrapper around its handler.
void dispatchCGStashList(Packet* pPacket, Player* pPlayer) {
    __BEGIN_DEBUG
    CGStashListHandler::execute(static_cast<CGStashList*>(pPacket), pPlayer);
    __END_DEBUG
}

// The live client's personal-store UI sends GCAddStoreItem /
// GCRemoveStoreItem to the server (the store flow is disabled server-side:
// GamePlayer force-disconnects the GC_MY/OTHER_STORE_INFO requests that
// precede them). Their handlers were server-side no-ops, so keep the
// silent ignore instead of letting the unregistered-id default disconnect
// a legitimate client.
void dispatchIgnore(Packet*, Player*) {}

} // namespace

void registerGameServerPacketHandlers() {
    DE_REGISTER_PACKET_HANDLER(CGAbsorbSoul);
    DE_REGISTER_PACKET_HANDLER(CGAcceptUnion);
    DE_REGISTER_PACKET_HANDLER(CGAddGearToMouse);
    DE_REGISTER_PACKET_HANDLER(CGAddInventoryToMouse);
    DE_REGISTER_PACKET_HANDLER(CGAddItemToCodeSheet);
    DE_REGISTER_PACKET_HANDLER(CGAddItemToItem);
    DE_REGISTER_PACKET_HANDLER(CGAddMouseToGear);
    DE_REGISTER_PACKET_HANDLER(CGAddMouseToInventory);
    DE_REGISTER_PACKET_HANDLER(CGAddMouseToQuickSlot);
    DE_REGISTER_PACKET_HANDLER(CGAddMouseToZone);
    DE_REGISTER_PACKET_HANDLER(CGAddQuickSlotToMouse);
    DE_REGISTER_PACKET_HANDLER(CGAddSMSAddress);
    DE_REGISTER_PACKET_HANDLER(CGAddZoneToInventory);
    DE_REGISTER_PACKET_HANDLER(CGAddZoneToMouse);
    DE_REGISTER_PACKET_HANDLER(CGAppointSubmaster);
    DE_REGISTER_PACKET_HANDLER(CGAttack);
    DE_REGISTER_PACKET_HANDLER(CGAuthKey);
    DE_REGISTER_PACKET_HANDLER(CGBloodDrain);
    DE_REGISTER_PACKET_HANDLER(CGBuyStoreItem);
    DE_REGISTER_PACKET_HANDLER(CGCastingSkill);
    DE_REGISTER_PACKET_HANDLER(CGConnect);
    DE_REGISTER_PACKET_HANDLER(CGConnectSetKey);
    DE_REGISTER_PACKET_HANDLER(CGCrashReport);
    DE_REGISTER_PACKET_HANDLER(CGDeleteSMSAddress);
    DE_REGISTER_PACKET_HANDLER(CGDenyUnion);
    DE_REGISTER_PACKET_HANDLER(CGDepositPet);
    DE_REGISTER_PACKET_HANDLER(CGDisplayItem);
    DE_REGISTER_PACKET_HANDLER(CGDissectionCorpse);
    DE_REGISTER_PACKET_HANDLER(CGDonationMoney);
    DE_REGISTER_PACKET_HANDLER(CGDownSkill);
    DE_REGISTER_PACKET_HANDLER(CGDropMoney);
    DE_REGISTER_PACKET_HANDLER(CGExchangeBuy);
    DE_REGISTER_PACKET_HANDLER(CGExchangeList);
    DE_REGISTER_PACKET_HANDLER(CGExpelGuild);
    DE_REGISTER_PACKET_HANDLER(CGExpelGuildMember);
    DE_REGISTER_PACKET_HANDLER(CGFailQuest);
    DE_REGISTER_PACKET_HANDLER(CGGQuestAccept);
    DE_REGISTER_PACKET_HANDLER(CGGQuestCancel);
    DE_REGISTER_PACKET_HANDLER(CGGetEventItem);
    DE_REGISTER_PACKET_HANDLER(CGGetOffMotorCycle);
    DE_REGISTER_PACKET_HANDLER(CGGlobalChat);
    DE_REGISTER_PACKET_HANDLER(CGGuildChat);
    DE_REGISTER_PACKET_HANDLER(CGJoinGuild);
    DE_REGISTER_PACKET_HANDLER(CGLearnSkill);
    DE_REGISTER_PACKET_HANDLER(CGLogout);
    DE_REGISTER_PACKET_HANDLER(CGLotterySelect);
    DE_REGISTER_PACKET_HANDLER(CGMakeItem);
    DE_REGISTER_PACKET_HANDLER(CGMixItem);
    DE_REGISTER_PACKET_HANDLER(CGModifyGuildIntro);
    DE_REGISTER_PACKET_HANDLER(CGModifyGuildMember);
    DE_REGISTER_PACKET_HANDLER(CGModifyGuildMemberIntro);
    DE_REGISTER_PACKET_HANDLER(CGModifyNickname);
    DE_REGISTER_PACKET_HANDLER(CGModifyTaxRatio);
    DE_REGISTER_PACKET_HANDLER(CGMouseToStash);
    DE_REGISTER_PACKET_HANDLER(CGMove);
    DE_REGISTER_PACKET_HANDLER(CGNPCAskAnswer);
    DE_REGISTER_PACKET_HANDLER(CGNPCTalk);
    DE_REGISTER_PACKET_HANDLER(CGPartyInvite);
    DE_REGISTER_PACKET_HANDLER(CGPartyLeave);
    DE_REGISTER_PACKET_HANDLER(CGPartyPosition);
    DE_REGISTER_PACKET_HANDLER(CGPartySay);
    DE_REGISTER_PACKET_HANDLER(CGPetGamble);
    DE_REGISTER_PACKET_HANDLER(CGPickupMoney);
    DE_REGISTER_PACKET_HANDLER(CGQuitGuild);
    DE_REGISTER_PACKET_HANDLER(CGQuitUnion);
    DE_REGISTER_PACKET_HANDLER(CGQuitUnionAccept);
    DE_REGISTER_PACKET_HANDLER(CGQuitUnionDeny);
    DE_REGISTER_PACKET_HANDLER(CGRangerSay);
    DE_REGISTER_PACKET_HANDLER(CGReady);
    DE_REGISTER_PACKET_HANDLER(CGRegistGuild);
    DE_REGISTER_PACKET_HANDLER(CGRelicToObject);
    DE_REGISTER_PACKET_HANDLER(CGReloadFromInventory);
    DE_REGISTER_PACKET_HANDLER(CGReloadFromQuickSlot);
    DE_REGISTER_PACKET_HANDLER(CGRequestGuildList);
    DE_REGISTER_PACKET_HANDLER(CGRequestGuildMemberList);
    DE_REGISTER_PACKET_HANDLER(CGRequestIP);
    DE_REGISTER_PACKET_HANDLER(CGRequestInfo);
    DE_REGISTER_PACKET_HANDLER(CGRequestNewbieItem);
    DE_REGISTER_PACKET_HANDLER(CGRequestPowerPoint);
    DE_REGISTER_PACKET_HANDLER(CGRequestRepair);
    DE_REGISTER_PACKET_HANDLER(CGRequestStoreInfo);
    DE_REGISTER_PACKET_HANDLER(CGRequestUnion);
    DE_REGISTER_PACKET_HANDLER(CGRequestUnionInfo);
    DE_REGISTER_PACKET_HANDLER(CGResurrect);
    DE_REGISTER_PACKET_HANDLER(CGRideMotorCycle);
    DE_REGISTER_PACKET_HANDLER(CGSMSAddressList);
    DE_REGISTER_PACKET_HANDLER(CGSMSSend);
    DE_REGISTER_PACKET_HANDLER(CGSay);
    DE_REGISTER_PACKET_HANDLER(CGSelectBloodBible);
    DE_REGISTER_PACKET_HANDLER(CGSelectGuild);
    DE_REGISTER_PACKET_HANDLER(CGSelectGuildMember);
    DE_REGISTER_PACKET_HANDLER(CGSelectNickname);
    DE_REGISTER_PACKET_HANDLER(CGSelectPortal);
    DE_REGISTER_PACKET_HANDLER(CGSelectQuest);
    DE_REGISTER_PACKET_HANDLER(CGSelectRankBonus);
    DE_REGISTER_PACKET_HANDLER(CGSelectRegenZone);
    DE_REGISTER_PACKET_HANDLER(CGSelectTileEffect);
    DE_REGISTER_PACKET_HANDLER(CGSelectWayPoint);
    DE_REGISTER_PACKET_HANDLER(CGSetSlayerHotKey);
    DE_REGISTER_PACKET_HANDLER(CGSetVampireHotKey);
    DE_REGISTER_PACKET_HANDLER(CGShopRequestBuy);
    DE_REGISTER_PACKET_HANDLER(CGShopRequestList);
    DE_REGISTER_PACKET_HANDLER(CGShopRequestSell);
    DE_REGISTER_PACKET_HANDLER(CGSilverCoating);
    DE_REGISTER_PACKET_HANDLER(CGSkillToInventory);
    DE_REGISTER_PACKET_HANDLER(CGSkillToNamed);
    DE_REGISTER_PACKET_HANDLER(CGSkillToObject);
    DE_REGISTER_PACKET_HANDLER(CGSkillToSelf);
    DE_REGISTER_PACKET_HANDLER(CGSkillToTile);
    DE_REGISTER_PACKET_HANDLER(CGStashDeposit);
    DE_REGISTER_PACKET_HANDLER(CGStashRequestBuy);
    DE_REGISTER_PACKET_HANDLER(CGStashToMouse);
    DE_REGISTER_PACKET_HANDLER(CGStashWithdraw);
    DE_REGISTER_PACKET_HANDLER(CGStoreClose);
    DE_REGISTER_PACKET_HANDLER(CGStoreOpen);
    DE_REGISTER_PACKET_HANDLER(CGStoreSign);
    DE_REGISTER_PACKET_HANDLER(CGSubmitScore);
    DE_REGISTER_PACKET_HANDLER(CGTakeOutGood);
    DE_REGISTER_PACKET_HANDLER(CGTameMonster);
    DE_REGISTER_PACKET_HANDLER(CGThrowBomb);
    DE_REGISTER_PACKET_HANDLER(CGThrowItem);
    DE_REGISTER_PACKET_HANDLER(CGTradeAddItem);
    DE_REGISTER_PACKET_HANDLER(CGTradeFinish);
    DE_REGISTER_PACKET_HANDLER(CGTradeMoney);
    DE_REGISTER_PACKET_HANDLER(CGTradePrepare);
    DE_REGISTER_PACKET_HANDLER(CGTradeRemoveItem);
    DE_REGISTER_PACKET_HANDLER(CGTryJoinGuild);
    DE_REGISTER_PACKET_HANDLER(CGTypeStringList);
    DE_REGISTER_PACKET_HANDLER(CGUnburrow);
    DE_REGISTER_PACKET_HANDLER(CGUndisplayItem);
    DE_REGISTER_PACKET_HANDLER(CGUntransform);
    DE_REGISTER_PACKET_HANDLER(CGUseBonusPoint);
    DE_REGISTER_PACKET_HANDLER(CGUseItemFromGQuestInventory);
    DE_REGISTER_PACKET_HANDLER(CGUseItemFromGear);
    DE_REGISTER_PACKET_HANDLER(CGUseItemFromInventory);
    DE_REGISTER_PACKET_HANDLER(CGUseMessageItemFromInventory);
    DE_REGISTER_PACKET_HANDLER(CGUsePotionFromInventory);
    DE_REGISTER_PACKET_HANDLER(CGUsePotionFromQuickSlot);
    DE_REGISTER_PACKET_HANDLER(CGUsePowerPoint);
    DE_REGISTER_PACKET_HANDLER(CGVerifyTime);
    DE_REGISTER_PACKET_HANDLER(CGVisible);
    DE_REGISTER_PACKET_HANDLER(CGWhisper);
    DE_REGISTER_PACKET_HANDLER(CGWithdrawPet);
    DE_REGISTER_PACKET_HANDLER(CGWithdrawTax);

    DE_REGISTER_PACKET_HANDLER_FN(CGPortCheck, dispatchCGPortCheck);
    DE_REGISTER_PACKET_HANDLER_FN(CGStashList, dispatchCGStashList);

    // GC packets the gameserver really receives (see the thunks above).
    // GCFriendChatting is the one GC packet with a live server handler:
    // the friend system's requests ride it client -> server. The client
    // link admits a GC packet only when GameClientLink.h lists it, and
    // kReceivedDirections refuses the registration of any other.
    DE_REGISTER_PACKET_HANDLER(GCFriendChatting);
    DE_REGISTER_PACKET_HANDLER_FN(GCAddStoreItem, dispatchIgnore);
    DE_REGISTER_PACKET_HANDLER_FN(GCRemoveStoreItem, dispatchIgnore);

    // SG (shared -> game), received on the SharedServerClient link.
    // SGModifyGuildMemberOK never ran before 2.3: its execute() was
    // guarded by a misspelled #ifdef __GAME_SERER__ (docs/FIXES.md).
    DE_REGISTER_PACKET_HANDLER_NOPLAYER(SGAddGuildMemberOK);
    DE_REGISTER_PACKET_HANDLER_NOPLAYER(SGAddGuildOK);
    DE_REGISTER_PACKET_HANDLER_NOPLAYER(SGDeleteGuildOK);
    DE_REGISTER_PACKET_HANDLER_NOPLAYER(SGExpelGuildMemberOK);
    DE_REGISTER_PACKET_HANDLER_NOPLAYER(SGGuildInfo);
    DE_REGISTER_PACKET_HANDLER_NOPLAYER(SGGuildMemberLogOnOK);
    DE_REGISTER_PACKET_HANDLER_NOPLAYER(SGModifyGuildIntroOK);
    DE_REGISTER_PACKET_HANDLER_NOPLAYER(SGModifyGuildMemberOK);
    DE_REGISTER_PACKET_HANDLER_NOPLAYER(SGModifyGuildOK);
    DE_REGISTER_PACKET_HANDLER_NOPLAYER(SGQuitGuildOK);

    // LG (login -> game) and GG (game -> game), both received on the
    // LoginServerManager datagram socket.
    DE_REGISTER_PACKET_HANDLER_NOPLAYER(LGIncomingConnection);
    DE_REGISTER_PACKET_HANDLER_NOPLAYER(LGIncomingConnectionError);
    DE_REGISTER_PACKET_HANDLER_NOPLAYER(LGIncomingConnectionOK);
    DE_REGISTER_PACKET_HANDLER_NOPLAYER(LGKickCharacter);
    DE_REGISTER_PACKET_HANDLER_NOPLAYER(GGCommand);
    DE_REGISTER_PACKET_HANDLER_NOPLAYER(GGGuildChat);
    DE_REGISTER_PACKET_HANDLER_NOPLAYER(GGServerChat);
}
