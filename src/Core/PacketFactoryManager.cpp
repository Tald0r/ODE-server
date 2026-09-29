//--------------------------------------------------------------------------------
//
// Filename    : PacketFactoryManager.cpp
// Written By  : Reiot
//
//--------------------------------------------------------------------------------

// include files
#include "PacketFactoryManager.h"

#include <algorithm>
#include <array>
#include <span>

#include <type_traits>

#include "Assert.h"
#include "CGAddGearToMouse.h"
#include "CGAddInventoryToMouse.h"
#include "CGAddMouseToGear.h"
#include "CGAddMouseToInventory.h"
#include "CGAddMouseToQuickSlot.h"
#include "CGAddMouseToZone.h"
#include "CGAddQuickSlotToMouse.h"
#include "CGAddZoneToInventory.h"
#include "CGAddZoneToMouse.h"
#include "CGAttack.h"
#include "CGBloodDrain.h"
#include "CGCastingSkill.h"
#include "CGConnect.h"
#include "CGDissectionCorpse.h"
#include "CGDropMoney.h"
#include "CGGetOffMotorCycle.h"
#include "CGGlobalChat.h"
#include "CGLearnSkill.h"
#include "CGLogout.h"
#include "CGMakeItem.h"
#include "CGMove.h"
#include "CGNPCAskAnswer.h"
#include "CGNPCTalk.h"
#include "CGPickupMoney.h"
#include "CGReady.h"
#include "CGReloadFromInventory.h"
#include "CGReloadFromQuickSlot.h"
#include "CGRequestNewbieItem.h"
#include "CGRequestRepair.h"
#include "CGRideMotorCycle.h"
#include "CGSay.h"
#include "CGShopRequestBuy.h"
#include "CGShopRequestList.h"
#include "CGShopRequestSell.h"
#include "CGSkillToInventory.h"
#include "CGThrowBomb.h"
#include "CGThrowItem.h"
#include "CGTypeStringList.h"
#include "CGUnburrow.h"
#include "CGUntransform.h"
#include "CGUseBonusPoint.h"
#include "CGUseItemFromGear.h"
#include "CGUsePotionFromInventory.h"
#include "CGUsePotionFromQuickSlot.h"
#include "CGVerifyTime.h"
#include "CGVisible.h"
#include "CGWithdrawTax.h"
#include "CLChangeServer.h"
#include "CLCreatePC.h"
#include "CLDeletePC.h"
#include "CLGetPCList.h"
#include "CLGetServerList.h"
#include "CLGetWorldList.h"
#include "CLLogin.h"
#include "CLLogout.h"
#include "CLQueryCharacterName.h"
#include "CLQueryPlayerID.h"
#include "CLReconnectLogin.h"
#include "CLRegisterPlayer.h"
#include "CLSelectPC.h"
#include "CLSelectServer.h"
#include "CLSelectWorld.h"
#include "CLVersionCheck.h"
#include "GCAddBat.h"
#include "GCAddBurrowingCreature.h"
#include "GCAddEffect.h"
#include "GCAddEffectToTile.h"
#include "GCAddGearToInventory.h"
#include "GCAddGearToZone.h"
#include "GCAddInjuriousCreature.h"
#include "GCAddInstalledMineToZone.h"
#include "GCAddMonster.h"
#include "GCAddMonsterCorpse.h"
#include "GCAddMonsterFromBurrowing.h"
#include "GCAddMonsterFromTransformation.h"
#include "GCAddNPC.h"
#include "GCAddNewItemToZone.h"
#include "GCAddSlayer.h"
#include "GCAddSlayerCorpse.h"
#include "GCAddVampire.h"
#include "GCAddVampireCorpse.h"
#include "GCAddVampireFromBurrowing.h"
#include "GCAddVampireFromTransformation.h"
#include "GCAddWolf.h"
#include "GCAttack.h"
#include "GCAttackArmsOK1.h"
#include "GCAttackArmsOK2.h"
#include "GCAttackArmsOK3.h"
#include "GCAttackArmsOK4.h"
#include "GCAttackArmsOK5.h"
#include "GCAttackMeleeOK1.h"
#include "GCAttackMeleeOK2.h"
#include "GCAttackMeleeOK3.h"
#include "GCBloodDrainOK1.h"
#include "GCBloodDrainOK2.h"
#include "GCBloodDrainOK3.h"
#include "GCCannotAdd.h"
#include "GCCannotUse.h"
#include "GCCastingSkill.h"
#include "GCChangeDarkLight.h"
#include "GCChangeShape.h"
#include "GCChangeWeather.h"
#include "GCCreateItem.h"
#include "GCCreatureDied.h"
#include "GCCrossCounterOK1.h"
#include "GCCrossCounterOK2.h"
#include "GCCrossCounterOK3.h"
#include "GCDeleteEffectFromTile.h"
#include "GCDeleteInventoryItem.h"
#include "GCDeleteObject.h"
#include "GCDeleteandPickUpOK.h"
#include "GCDisconnect.h"
#include "GCDropItemToZone.h"
#include "GCFakeMove.h"
#include "GCFastMove.h"
#include "GCGetDamage.h"
#include "GCGetOffMotorCycle.h"
#include "GCGetOffMotorCycleFailed.h"
#include "GCGetOffMotorCycleOK.h"
#include "GCGlobalChat.h"
#include "GCHPRecoveryEndToOthers.h"
#include "GCHPRecoveryEndToSelf.h"
#include "GCHPRecoveryStartToOthers.h"
#include "GCHPRecoveryStartToSelf.h"
#include "GCKnockBack.h"
#include "GCKnocksTargetBackOK1.h"
#include "GCKnocksTargetBackOK2.h"
#include "GCKnocksTargetBackOK4.h"
#include "GCKnocksTargetBackOK5.h"
#include "GCLearnSkillFailed.h"
#include "GCLearnSkillOK.h"
#include "GCLearnSkillReady.h"
#include "GCLightning.h"
#include "GCMPRecoveryEnd.h"
#include "GCMPRecoveryStart.h"
#include "GCMakeItemFail.h"
#include "GCMakeItemOK.h"
#include "GCMineExplosionOK1.h"
#include "GCMineExplosionOK2.h"
#include "GCModifyInformation.h"
#include "GCMorph1.h"
#include "GCMorphSlayer2.h"
#include "GCMorphVampire2.h"
#include "GCMove.h"
#include "GCMoveError.h"
#include "GCMoveOK.h"
#include "GCNPCAsk.h"
#include "GCNPCSay.h"
#include "GCRealWearingInfo.h"
#include "GCReconnect.h"
#include "GCReconnectLogin.h"
#include "GCReloadOK.h"
#include "GCRemoveCorpseHead.h"
#include "GCRemoveEffect.h"
#include "GCRemoveFromGear.h"
#include "GCRemoveInjuriousCreature.h"
#include "GCRideMotorCycle.h"
#include "GCRideMotorCycleFailed.h"
#include "GCRideMotorCycleOK.h"
#include "GCSay.h"
#include "GCSearchMotorcycleFail.h"
#include "GCSearchMotorcycleOK.h"
#include "GCSetPosition.h"
#include "GCThrowBombOK1.h"
#include "GCThrowBombOK2.h"
#include "GCThrowBombOK3.h"
#include "PacketMeta.h"
#include "StringStream.h"
// Shop Interface
#include "GCNPCAskVariable.h"
#include "GCNPCInfo.h"
#include "GCShopBought.h"
#include "GCShopBuyFail.h"
#include "GCShopBuyOK.h"
#include "GCShopList.h"
#include "GCShopListMysterious.h"
#include "GCShopMarketCondition.h"
#include "GCShopSellFail.h"
#include "GCShopSellOK.h"
#include "GCShopSold.h"
#include "GCShopVersion.h"
#include "GCSkillFailed1.h"
#include "GCSkillFailed2.h"
#include "GCSkillToInventoryOK1.h"
#include "GCSkillToInventoryOK2.h"
#include "GCSkillToObjectOK1.h"
#include "GCSkillToObjectOK2.h"
#include "GCSkillToObjectOK3.h"
#include "GCSkillToObjectOK4.h"
#include "GCSkillToObjectOK5.h"
#include "GCSkillToObjectOK6.h"
#include "GCSkillToSelfOK1.h"
#include "GCSkillToSelfOK2.h"
#include "GCSkillToSelfOK3.h"
#include "GCSkillToTileOK1.h"
#include "GCSkillToTileOK2.h"
#include "GCSkillToTileOK3.h"
#include "GCSkillToTileOK4.h"
#include "GCSkillToTileOK5.h"
#include "GCSkillToTileOK6.h"
#include "GCSystemMessage.h"
#include "GCTakeOff.h"
#include "GCTeachSkillInfo.h"
#include "GCThrowItemOK1.h"
#include "GCThrowItemOK2.h"
#include "GCThrowItemOK3.h"
#include "GCUnburrowFail.h"
#include "GCUnburrowOK.h"
#include "GCUntransformFail.h"
#include "GCUntransformOK.h"
#include "GCUpdateInfo.h"
#include "GCUseBonusPointFail.h"
#include "GCUseBonusPointOK.h"
#include "GCUseOK.h"
#include "GCVisibleFail.h"
#include "GCVisibleOK.h"
#include "GCWarList.h"
#include "GCWarScheduleList.h"
#include "GLIncomingConnection.h"
#include "GLIncomingConnectionError.h"
#include "GLIncomingConnectionOK.h"
#include "GLKickVerify.h"
#include "GMServerInfo.h"
#include "GameClientLink.h"
#include "LCCreatePCError.h"
#include "LCCreatePCOK.h"
#include "LCDeletePCError.h"
#include "LCDeletePCOK.h"
#include "LCLoginError.h"
#include "LCLoginOK.h"
#include "LCPCList.h"
#include "LCQueryResultCharacterName.h"
#include "LCQueryResultPlayerID.h"
#include "LCReconnect.h"
#include "LCRegisterPlayerError.h"
#include "LCRegisterPlayerOK.h"
#include "LCSelectPCError.h"
#include "LCServerList.h"
#include "LCVersionCheckError.h"
#include "LCVersionCheckOK.h"
#include "LCWorldList.h"
#include "LGIncomingConnection.h"
#include "LGIncomingConnectionError.h"
#include "LGIncomingConnectionOK.h"
#include "LGKickCharacter.h"

// added by elcastle 2000-11-29
// #include "CGDialUp.h"
// #include "CGPhoneDisconnect.h"
// #include "CGPhoneSay.h"
#include "CGWhisper.h"

// #include "GCPhoneConnected.h"
// #include "GCRing.h"
// #include "GCPhoneDisconnected.h"
// #include "GCPhoneConnectionFailed.h"
// #include "GCPhoneSay.h"
#include "GCWhisper.h"
#include "GCWhisperFailed.h"

// added by elca 2000-11-29
#include "GCSkillInfo.h"

// added by elca 2000-12-09
#include "GCStatusCurrentHP.h"

// added by elca 2000-12-09
#include "CGSetSlayerHotKey.h"
#include "CGSetVampireHotKey.h"


// added by elca 2001-06-26
#include "CGSelectPortal.h"

// 2001-01-08 stash feature
#include "CGMouseToStash.h"
#include "CGPartyInvite.h"
#include "CGPartyLeave.h"
#include "CGRequestIP.h"
#include "CGResurrect.h"
#include "CGSelectTileEffect.h"
#include "CGSelectWayPoint.h"
#include "CGSilverCoating.h"
#include "CGSkillToObject.h"
#include "CGSkillToSelf.h"
#include "CGSkillToTile.h"
#include "CGStashDeposit.h"
#include "CGStashList.h"
#include "CGStashRequestBuy.h"
#include "CGStashToMouse.h"
#include "CGStashWithdraw.h"
#include "CGTradeAddItem.h"
#include "CGTradeFinish.h"
#include "CGTradeMoney.h"
#include "CGTradePrepare.h"
#include "CGTradeRemoveItem.h"
#include "CGUseItemFromInventory.h"
#include "GCAddHelicopter.h"
#include "GCAddVampirePortal.h"
#include "GCEnterVampirePortal.h"
#include "GCNPCAskDynamic.h"
#include "GCNPCResponse.h"
#include "GCNPCSayDynamic.h"
#include "GCOtherModifyInfo.h"
#include "GCPartyError.h"
#include "GCPartyInvite.h"
#include "GCPartyJoined.h"
#include "GCPartyLeave.h"
#include "GCRequestFailed.h"
#include "GCRequestedIP.h"
#include "GCStashList.h"
#include "GCStashSell.h"
#include "GCTradeAddItem.h"
#include "GCTradeError.h"
#include "GCTradeFinish.h"
#include "GCTradeMoney.h"
#include "GCTradePrepare.h"
#include "GCTradeRemoveItem.h"
#include "GCTradeVerify.h"
#include "GGCommand.h"


// Guild feature update - 2002.05.31 (bezz)
// #include "GCShowGuildRegist.h"
#include "CGJoinGuild.h"
#include "CGRegistGuild.h"
#include "CGSelectGuild.h"
#include "CGTryJoinGuild.h"
#include "GCShowGuildInfo.h"
#include "GCShowGuildJoin.h"
#include "GCShowMessageBox.h"
#include "GCShowWaitGuildInfo.h"
#include "GCWaitGuildList.h"
// #include "GCModifyMoney.h"
// #include "CGQuitGuild.h"
#include "CGRequestGuildMemberList.h"
#include "CGSelectGuildMember.h"
// #include "CGExpelGuildMember.h"
#include "CGAbsorbSoul.h"
#include "CGAcceptUnion.h"
#include "CGAddItemToCodeSheet.h"
#include "CGAddItemToItem.h"
#include "CGAddSMSAddress.h"
#include "CGAppointSubmaster.h"
#include "CGAuthKey.h"
#include "CGBuyStoreItem.h"
#include "CGCrashReport.h"
#include "CGDeleteSMSAddress.h"
#include "CGDenyUnion.h"
#include "CGDepositPet.h"
#include "CGDisplayItem.h"
#include "CGDonationMoney.h"
#include "CGDownSkill.h"
#include "CGExpelGuild.h"
#include "CGGuildChat.h"
#include "CGModifyGuildIntro.h"
#include "CGModifyGuildMember.h"
#include "CGModifyGuildMemberIntro.h"
#include "CGPortCheck.h"
#include "CGRelicToObject.h"
#include "CGRequestInfo.h"
#include "GCActiveGuildList.h"
#include "GCAddItemToItemVerify.h"
#include "GCGuildChat.h"
#include "GCGuildMemberList.h"
#include "GCModifyGuildMemberInfo.h"
#include "GCNoticeEvent.h"
#include "GCShowGuildMemberInfo.h"
#include "GGGuildChat.h"
#include "GSAddGuild.h"
#include "GSAddGuildMember.h"
#include "GSExpelGuildMember.h"
#include "GSGuildMemberLogOn.h"
#include "GSModifyGuildIntro.h"
#include "GSModifyGuildMember.h"
#include "GSQuitGuild.h"
#include "GSRequestGuildInfo.h"
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

// Exchange System Packets
#include "CGExchangeBuy.h"
#include "CGExchangeList.h"
#include "CGFailQuest.h"
#include "CGGQuestAccept.h"
#include "CGGQuestCancel.h"
#include "CGGetEventItem.h"
#include "CGLotterySelect.h"
#include "CGMixItem.h"
#include "CGModifyNickname.h"
#include "CGModifyTaxRatio.h"
#include "CGPartyPosition.h"
#include "CGPartySay.h"
#include "CGPetGamble.h"
#include "CGQuitUnion.h"
#include "CGQuitUnionAccept.h"
#include "CGQuitUnionDeny.h"
#include "CGRangerSay.h"
#include "CGRequestGuildList.h"
#include "CGRequestPowerPoint.h"
#include "CGRequestStoreInfo.h"
#include "CGRequestUnion.h"
#include "CGRequestUnionInfo.h"
#include "CGSMSAddressList.h"
#include "CGSMSSend.h"
#include "CGSelectBloodBible.h"
#include "CGSelectNickname.h"
#include "CGSelectQuest.h"
#include "CGSelectRankBonus.h"
#include "CGSelectRegenZone.h"
#include "CGSkillToNamed.h"
#include "CGStoreClose.h"
#include "CGStoreOpen.h"
#include "CGStoreSign.h"
#include "CGSubmitScore.h"
#include "CGTakeOutGood.h"
#include "CGTameMonster.h"
#include "CGUndisplayItem.h"
#include "CGUseItemFromGQuestInventory.h"
#include "CGUseMessageItemFromInventory.h"
#include "CGUsePowerPoint.h"
#include "CGWithdrawPet.h"
#include "GCAddNickname.h"
#include "GCAddOusters.h"
#include "GCAddOustersCorpse.h"
#include "GCAddStoreItem.h"
#include "GCAddressListVerify.h"
#include "GCAuthKey.h"
#include "GCBloodBibleList.h"
#include "GCBloodBibleSignInfo.h"
#include "GCBloodBibleStatus.h"
#include "GCDownSkillFailed.h"
#include "GCDownSkillOK.h"
#include "GCExchangeBuy.h"
#include "GCExchangeList.h"
#include "GCExecuteElement.h"
#include "GCFlagWarStatus.h"
#include "GCGQuestInventory.h"
#include "GCGQuestStatusInfo.h"
#include "GCGQuestStatusModify.h"
#include "GCGoodsList.h"
#include "GCGuildResponse.h"
#include "GCHolyLandBonusInfo.h"
#include "GCKickMessage.h"
#include "GCMiniGameScores.h"
#include "GCModifyNickname.h"
#include "GCMonsterKillQuestInfo.h"
#include "GCMyStoreInfo.h"
#include "GCNicknameList.h"
#include "GCNicknameVerify.h"
#include "GCNotifyWin.h"
#include "GCOtherGuildName.h"
#include "GCOtherStoreInfo.h"
#include "GCPartyPosition.h"
#include "GCPartySay.h"
#include "GCPetInfo.h"
#include "GCPetStashList.h"
#include "GCPetStashVerify.h"
#include "GCPetUseSkill.h"
#include "GCQuestStatus.h"
#include "GCRankBonusInfo.h"
#include "GCRegenZoneStatus.h"
#include "GCRemoveStoreItem.h"
#include "GCRequestPowerPointResult.h"
#include "GCSMSAddressList.h"
#include "GCSelectQuestID.h"
#include "GCSelectRankBonusFailed.h"
#include "GCSelectRankBonusOK.h"
#include "GCSweeperBonusInfo.h"
#include "GCSystemAvailabilities.h"
#include "GCTakeOutFail.h"
#include "GCTakeOutOK.h"
#include "GCTimeLimitItemInfo.h"
#include "GCUnionOfferList.h"
#include "GCUsePowerPointResult.h"
#include "GGServerChat.h"
// add by viva 2008-12-31
#include "CGConnectSetKey.h"
#include "GCFriendChatting.h"
// end
// #include "GSGuildAction.h"
// #include "SGGuildResponse.h"

//////////////////////////////////////////////////////////////////////
//
// constructor
//
//////////////////////////////////////////////////////////////////////
PacketFactoryManager::PacketFactoryManager() : m_Factories(NULL), m_Size(Packet::PACKET_MAX) {
    __BEGIN_TRY

    Assert(m_Size > 0);

    // Allocate the packet factory array.
    m_Factories = new PacketFactory*[m_Size];

    // Initialize every slot to NULL.
    for (int i = 0; i < m_Size; i++)
        m_Factories[i] = NULL;

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////
//
// destructor
//
//////////////////////////////////////////////////////////////////////
PacketFactoryManager::~PacketFactoryManager() noexcept {
    Assert(m_Factories != NULL);

    // Delete any instantiated packet factories.
    for (int i = 0; i < m_Size; i++) {
#ifdef __GAME_CLIENT__
        if (m_Factories[i] != NULL) {
            delete m_Factories[i];
            m_Factories[i] = NULL;
        }
#else
        SAFE_DELETE(m_Factories[i]);
#endif
    }

    // Release the packet factory array itself.
#ifdef __GAME_CLIENT__
    if (m_Factories != NULL) {
        delete[] m_Factories;
        m_Factories = NULL;
    }
#else
    SAFE_DELETE_ARRAY(m_Factories);
#endif
}


//////////////////////////////////////////////////////////////////////
//
// Register every supported packet factory here.
//
// Each list is a de::packet::FactoryList (PacketMeta.h): its packet ids
// are folded into a constexpr table while this file compiles, and a
// duplicate or out-of-range id is a compile error naming the id. The
// per-server set is the Concat of the lists below, validated as one
// table, so a clash across lists is rejected the same way. The
// membership is the hand-written addFactory() sequence it replaced,
// pinned per server in tests/ratchet/factory_registrations.txt: adding
// or dropping an entry here fails tests/ratchet/ratchets.sh until that
// file is regenerated (tests/tools/factory_registrations.pl).
//
//////////////////////////////////////////////////////////////////////
namespace {

using de::packet::Concat;
using de::packet::FactoryList;

// clang-format off
// Client traffic both the gameserver and the loginserver accept.
using ClientLinkFactories = FactoryList<
    CGConnectSetKeyFactory,
    GGCommandFactory,
    GLIncomingConnectionErrorFactory,
    GLIncomingConnectionFactory,
    GLIncomingConnectionOKFactory,
    GLKickVerifyFactory,
    GMServerInfoFactory,
    LGIncomingConnectionErrorFactory,
    LGIncomingConnectionFactory,
    LGIncomingConnectionOKFactory,
    LGKickCharacterFactory>;

// Guild traffic the gameserver and the sharedserver exchange.
using GuildLinkFactories = FactoryList<
    GGGuildChatFactory,
    GSAddGuildFactory,
    GSAddGuildMemberFactory,
    GSExpelGuildMemberFactory,
    GSGuildMemberLogOnFactory,
    GSModifyGuildIntroFactory,
    GSModifyGuildMemberFactory,
    GSQuitGuildFactory,
    GSRequestGuildInfoFactory,
    SGAddGuildMemberOKFactory,
    SGAddGuildOKFactory,
    SGDeleteGuildOKFactory,
    SGExpelGuildMemberOKFactory,
    SGGuildInfoFactory,
    SGGuildMemberLogOnOKFactory,
    SGModifyGuildIntroOKFactory,
    SGModifyGuildMemberOKFactory,
    SGModifyGuildOKFactory,
    SGQuitGuildOKFactory>;

// Login-phase packets: CL/LC, and the GL/LG link seen from the loginserver.
using LoginOnlyFactories = FactoryList<
    CLChangeServerFactory,
    CLCreatePCFactory,
    CLDeletePCFactory,
    CLGetPCListFactory,
    CLGetServerListFactory,
    CLGetWorldListFactory,
    CLLoginFactory,
    CLLogoutFactory,
    CLQueryCharacterNameFactory,
    CLQueryPlayerIDFactory,
    CLReconnectLoginFactory,
    CLRegisterPlayerFactory,
    CLSelectPCFactory,
    CLSelectServerFactory,
    CLSelectWorldFactory,
    CLVersionCheckFactory,
    LCCreatePCErrorFactory,
    LCCreatePCOKFactory,
    LCDeletePCErrorFactory,
    LCDeletePCOKFactory,
    LCLoginErrorFactory,
    LCLoginOKFactory,
    LCPCListFactory,
    LCQueryResultCharacterNameFactory,
    LCQueryResultPlayerIDFactory,
    LCReconnectFactory,
    LCRegisterPlayerErrorFactory,
    LCRegisterPlayerOKFactory,
    LCSelectPCErrorFactory,
    LCServerListFactory,
    LCVersionCheckErrorFactory,
    LCVersionCheckOKFactory,
    LCWorldListFactory>;

// Everything else the gameserver speaks: CG/GC, GG, and its GL/LG, GS/SG ends.
using GameOnlyFactories = FactoryList<
    CGAbsorbSoulFactory,
    CGAcceptUnionFactory,
    CGAddGearToMouseFactory,
    CGAddInventoryToMouseFactory,
    CGAddItemToCodeSheetFactory,
    CGAddItemToItemFactory,
    CGAddMouseToGearFactory,
    CGAddMouseToInventoryFactory,
    CGAddMouseToQuickSlotFactory,
    CGAddMouseToZoneFactory,
    CGAddQuickSlotToMouseFactory,
    CGAddSMSAddressFactory,
    CGAddZoneToInventoryFactory,
    CGAddZoneToMouseFactory,
    CGAppointSubmasterFactory,
    CGAttackFactory,
    CGAuthKeyFactory,
    CGBloodDrainFactory,
    CGBuyStoreItemFactory,
    CGCastingSkillFactory,
    CGConnectFactory,
    CGCrashReportFactory,
    CGDeleteSMSAddressFactory,
    CGDenyUnionFactory,
    CGDepositPetFactory,
    CGDisplayItemFactory,
    CGDissectionCorpseFactory,
    CGDonationMoneyFactory,
    CGDownSkillFactory,
    CGDropMoneyFactory,
    CGExchangeBuyFactory,
    CGExchangeListFactory,
    CGExpelGuildFactory,
    CGFailQuestFactory,
    CGGQuestAcceptFactory,
    CGGQuestCancelFactory,
    CGGetEventItemFactory,
    CGGetOffMotorCycleFactory,
    CGGlobalChatFactory,
    CGGuildChatFactory,
    CGJoinGuildFactory,
    CGLearnSkillFactory,
    CGLogoutFactory,
    CGLotterySelectFactory,
    CGMakeItemFactory,
    CGMixItemFactory,
    CGModifyGuildIntroFactory,
    CGModifyGuildMemberFactory,
    CGModifyGuildMemberIntroFactory,
    CGModifyNicknameFactory,
    CGModifyTaxRatioFactory,
    CGMouseToStashFactory,
    CGMoveFactory,
    CGNPCAskAnswerFactory,
    CGNPCTalkFactory,
    CGPartyInviteFactory,
    CGPartyLeaveFactory,
    CGPartyPositionFactory,
    CGPartySayFactory,
    CGPetGambleFactory,
    CGPickupMoneyFactory,
    CGPortCheckFactory,
    CGQuitUnionAcceptFactory,
    CGQuitUnionDenyFactory,
    CGQuitUnionFactory,
    CGRangerSayFactory,
    CGReadyFactory,
    CGRegistGuildFactory,
    CGRelicToObjectFactory,
    CGReloadFromInventoryFactory,
    CGReloadFromQuickSlotFactory,
    CGRequestGuildListFactory,
    CGRequestGuildMemberListFactory,
    CGRequestIPFactory,
    CGRequestInfoFactory,
    CGRequestNewbieItemFactory,
    CGRequestPowerPointFactory,
    CGRequestRepairFactory,
    CGRequestStoreInfoFactory,
    CGRequestUnionFactory,
    CGRequestUnionInfoFactory,
    CGResurrectFactory,
    CGRideMotorCycleFactory,
    CGSMSAddressListFactory,
    CGSMSSendFactory,
    CGSayFactory,
    CGSelectBloodBibleFactory,
    CGSelectGuildFactory,
    CGSelectGuildMemberFactory,
    CGSelectNicknameFactory,
    CGSelectPortalFactory,
    CGSelectQuestFactory,
    CGSelectRankBonusFactory,
    CGSelectRegenZoneFactory,
    CGSelectTileEffectFactory,
    CGSelectWayPointFactory,
    CGSetSlayerHotKeyFactory,
    CGSetVampireHotKeyFactory,
    CGShopRequestBuyFactory,
    CGShopRequestListFactory,
    CGShopRequestSellFactory,
    CGSilverCoatingFactory,
    CGSkillToInventoryFactory,
    CGSkillToNamedFactory,
    CGSkillToObjectFactory,
    CGSkillToSelfFactory,
    CGSkillToTileFactory,
    CGStashDepositFactory,
    CGStashListFactory,
    CGStashRequestBuyFactory,
    CGStashToMouseFactory,
    CGStashWithdrawFactory,
    CGStoreCloseFactory,
    CGStoreOpenFactory,
    CGStoreSignFactory,
    CGSubmitScoreFactory,
    CGTakeOutGoodFactory,
    CGTameMonsterFactory,
    CGThrowBombFactory,
    CGThrowItemFactory,
    CGTradeAddItemFactory,
    CGTradeFinishFactory,
    CGTradeMoneyFactory,
    CGTradePrepareFactory,
    CGTradeRemoveItemFactory,
    CGTryJoinGuildFactory,
    CGTypeStringListFactory,
    CGUnburrowFactory,
    CGUndisplayItemFactory,
    CGUntransformFactory,
    CGUseBonusPointFactory,
    CGUseItemFromGQuestInventoryFactory,
    CGUseItemFromGearFactory,
    CGUseItemFromInventoryFactory,
    CGUseMessageItemFromInventoryFactory,
    CGUsePotionFromInventoryFactory,
    CGUsePotionFromQuickSlotFactory,
    CGUsePowerPointFactory,
    CGVerifyTimeFactory,
    CGVisibleFactory,
    CGWhisperFactory,
    CGWithdrawPetFactory,
    CGWithdrawTaxFactory,
    GCActiveGuildListFactory,
    GCAddBatFactory,
    GCAddBurrowingCreatureFactory,
    GCAddEffectFactory,
    GCAddEffectToTileFactory,
    GCAddGearToInventoryFactory,
    GCAddGearToZoneFactory,
    GCAddHelicopterFactory,
    GCAddInjuriousCreatureFactory,
    GCAddInstalledMineToZoneFactory,
    GCAddItemToItemVerifyFactory,
    GCAddMonsterCorpseFactory,
    GCAddMonsterFactory,
    GCAddMonsterFromBurrowingFactory,
    GCAddMonsterFromTransformationFactory,
    GCAddNPCFactory,
    GCAddNewItemToZoneFactory,
    GCAddNicknameFactory,
    GCAddOustersCorpseFactory,
    GCAddOustersFactory,
    GCAddSlayerCorpseFactory,
    GCAddSlayerFactory,
    GCAddStoreItemFactory,
    GCAddVampireCorpseFactory,
    GCAddVampireFactory,
    GCAddVampireFromBurrowingFactory,
    GCAddVampireFromTransformationFactory,
    GCAddVampirePortalFactory,
    GCAddWolfFactory,
    GCAddressListVerifyFactory,
    GCAttackArmsOK1Factory,
    GCAttackArmsOK2Factory,
    GCAttackArmsOK3Factory,
    GCAttackArmsOK4Factory,
    GCAttackArmsOK5Factory,
    GCAttackFactory,
    GCAttackMeleeOK1Factory,
    GCAttackMeleeOK2Factory,
    GCAttackMeleeOK3Factory,
    GCAuthKeyFactory,
    GCBloodBibleListFactory,
    GCBloodBibleSignInfoFactory,
    GCBloodBibleStatusFactory,
    GCBloodDrainOK1Factory,
    GCBloodDrainOK2Factory,
    GCBloodDrainOK3Factory,
    GCCannotAddFactory,
    GCCannotUseFactory,
    GCCastingSkillFactory,
    GCChangeDarkLightFactory,
    GCChangeShapeFactory,
    GCChangeWeatherFactory,
    GCCreateItemFactory,
    GCCreatureDiedFactory,
    GCCrossCounterOK1Factory,
    GCCrossCounterOK2Factory,
    GCCrossCounterOK3Factory,
    GCDeleteEffectFromTileFactory,
    GCDeleteInventoryItemFactory,
    GCDeleteObjectFactory,
    GCDeleteandPickUpOKFactory,
    GCDisconnectFactory,
    GCDownSkillFailedFactory,
    GCDownSkillOKFactory,
    GCDropItemToZoneFactory,
    GCEnterVampirePortalFactory,
    GCExecuteElementFactory,
    GCFakeMoveFactory,
    GCFastMoveFactory,
    GCFlagWarStatusFactory,
    GCFriendChattingFactory,
    GCGQuestInventoryFactory,
    GCGQuestStatusInfoFactory,
    GCGQuestStatusModifyFactory,
    GCGetDamageFactory,
    GCGetOffMotorCycleFactory,
    GCGetOffMotorCycleFailedFactory,
    GCGetOffMotorCycleOKFactory,
    GCGlobalChatFactory,
    GCGoodsListFactory,
    GCGuildChatFactory,
    GCGuildMemberListFactory,
    GCGuildResponseFactory,
    GCHPRecoveryEndToOthersFactory,
    GCHPRecoveryEndToSelfFactory,
    GCHPRecoveryStartToOthersFactory,
    GCHPRecoveryStartToSelfFactory,
    GCHolyLandBonusInfoFactory,
    GCKickMessageFactory,
    GCKnockBackFactory,
    GCKnocksTargetBackOK1Factory,
    GCKnocksTargetBackOK2Factory,
    GCKnocksTargetBackOK4Factory,
    GCKnocksTargetBackOK5Factory,
    GCLearnSkillFailedFactory,
    GCLearnSkillOKFactory,
    GCLearnSkillReadyFactory,
    GCLightningFactory,
    GCMPRecoveryEndFactory,
    GCMPRecoveryStartFactory,
    GCMakeItemFailFactory,
    GCMakeItemOKFactory,
    GCMineExplosionOK1Factory,
    GCMineExplosionOK2Factory,
    GCMiniGameScoresFactory,
    GCModifyGuildMemberInfoFactory,
    GCModifyInformationFactory,
    GCModifyNicknameFactory,
    GCMonsterKillQuestInfoFactory,
    GCMorph1Factory,
    GCMorphSlayer2Factory,
    GCMorphVampire2Factory,
    GCMoveErrorFactory,
    GCMoveFactory,
    GCMoveOKFactory,
    GCMyStoreInfoFactory,
    GCNPCAskDynamicFactory,
    GCNPCAskFactory,
    GCNPCAskVariableFactory,
    GCNPCInfoFactory,
    GCNPCResponseFactory,
    GCNPCSayDynamicFactory,
    GCNPCSayFactory,
    GCNicknameListFactory,
    GCNicknameVerifyFactory,
    GCNoticeEventFactory,
    GCNotifyWinFactory,
    GCOtherGuildNameFactory,
    GCOtherModifyInfoFactory,
    GCOtherStoreInfoFactory,
    GCPartyErrorFactory,
    GCPartyInviteFactory,
    GCPartyJoinedFactory,
    GCPartyLeaveFactory,
    GCPartyPositionFactory,
    GCPartySayFactory,
    GCPetInfoFactory,
    GCPetStashListFactory,
    GCPetStashVerifyFactory,
    GCPetUseSkillFactory,
    GCQuestStatusFactory,
    GCRankBonusInfoFactory,
    GCRealWearingInfoFactory,
    GCReconnectFactory,
    GCReconnectLoginFactory,
    GCRegenZoneStatusFactory,
    GCReloadOKFactory,
    GCRemoveCorpseHeadFactory,
    GCRemoveEffectFactory,
    GCRemoveFromGearFactory,
    GCRemoveInjuriousCreatureFactory,
    GCRemoveStoreItemFactory,
    GCRequestFailedFactory,
    GCRequestPowerPointResultFactory,
    GCRequestedIPFactory,
    GCRideMotorCycleFactory,
    GCRideMotorCycleFailedFactory,
    GCRideMotorCycleOKFactory,
    GCSMSAddressListFactory,
    GCSayFactory,
    GCSearchMotorcycleFailFactory,
    GCSearchMotorcycleOKFactory,
    GCSelectQuestIDFactory,
    GCSelectRankBonusFailedFactory,
    GCSelectRankBonusOKFactory,
    GCSetPositionFactory,
    GCShopBoughtFactory,
    GCShopBuyFailFactory,
    GCShopBuyOKFactory,
    GCShopListFactory,
    GCShopListMysteriousFactory,
    GCShopMarketConditionFactory,
    GCShopSellFailFactory,
    GCShopSellOKFactory,
    GCShopSoldFactory,
    GCShopVersionFactory,
    GCShowGuildInfoFactory,
    GCShowGuildJoinFactory,
    GCShowGuildMemberInfoFactory,
    GCShowMessageBoxFactory,
    GCShowWaitGuildInfoFactory,
    GCSkillFailed1Factory,
    GCSkillFailed2Factory,
    GCSkillInfoFactory,
    GCSkillToInventoryOK1Factory,
    GCSkillToInventoryOK2Factory,
    GCSkillToObjectOK1Factory,
    GCSkillToObjectOK2Factory,
    GCSkillToObjectOK3Factory,
    GCSkillToObjectOK4Factory,
    GCSkillToObjectOK5Factory,
    GCSkillToObjectOK6Factory,
    GCSkillToSelfOK1Factory,
    GCSkillToSelfOK2Factory,
    GCSkillToSelfOK3Factory,
    GCSkillToTileOK1Factory,
    GCSkillToTileOK2Factory,
    GCSkillToTileOK3Factory,
    GCSkillToTileOK4Factory,
    GCSkillToTileOK5Factory,
    GCSkillToTileOK6Factory,
    GCStashListFactory,
    GCStashSellFactory,
    GCStatusCurrentHPFactory,
    GCSweeperBonusInfoFactory,
    GCSystemAvailabilitiesFactory,
    GCSystemMessageFactory,
    GCTakeOffFactory,
    GCTakeOutFailFactory,
    GCTakeOutOKFactory,
    GCTeachSkillInfoFactory,
    GCThrowBombOK1Factory,
    GCThrowBombOK2Factory,
    GCThrowBombOK3Factory,
    GCThrowItemOK1Factory,
    GCThrowItemOK2Factory,
    GCThrowItemOK3Factory,
    GCTimeLimitItemInfoFactory,
    GCTradeAddItemFactory,
    GCTradeErrorFactory,
    GCTradeFinishFactory,
    GCTradeMoneyFactory,
    GCTradePrepareFactory,
    GCTradeRemoveItemFactory,
    GCTradeVerifyFactory,
    GCUnburrowFailFactory,
    GCUnburrowOKFactory,
    GCUnionOfferListFactory,
    GCUntransformFailFactory,
    GCUntransformOKFactory,
    GCUpdateInfoFactory,
    GCUseBonusPointFailFactory,
    GCUseBonusPointOKFactory,
    GCUseOKFactory,
    GCUsePowerPointResultFactory,
    GCVisibleFailFactory,
    GCVisibleOKFactory,
    GCWaitGuildListFactory,
    GCWarListFactory,
    GCWarScheduleListFactory,
    GCWhisperFactory,
    GCWhisperFailedFactory,
    GGServerChatFactory>;

// clang-format on

#if defined(__GAME_SERVER__)
using ServerFactories = Concat<GameOnlyFactories, ClientLinkFactories, GuildLinkFactories>;
#elif defined(__LOGIN_SERVER__)
using ServerFactories = Concat<LoginOnlyFactories, ClientLinkFactories>;
#elif defined(__SHARED_SERVER__)
using ServerFactories = GuildLinkFactories;
#else
// Define-free (TestPackets): nothing is registered, exactly as before.
using ServerFactories = FactoryList<>;
#endif

#if defined(__GAME_SERVER__)
// The gameserver's registered ids that a game client sends on the game
// connection (GameClientLink.h), folded from the factory table while
// compiling: a factory added to the lists above is admitted in GPS_NORMAL
// exactly when the rule says a client sends it.
consteval std::size_t countGameClientLinkIDs() {
    std::size_t count = 0;
    for (const de::packet::Meta& meta : ServerFactories::kMeta)
        count += de::packet::sentOnGameClientLink(meta) ? 1 : 0;
    return count;
}

consteval std::array<PacketID_t, countGameClientLinkIDs()> gameClientLinkIDs() {
    std::array<PacketID_t, countGameClientLinkIDs()> ids{};
    std::size_t next = 0;
    for (const de::packet::Meta& meta : ServerFactories::kMeta)
        if (de::packet::sentOnGameClientLink(meta))
            ids[next++] = meta.id;
    std::sort(ids.begin(), ids.end());
    return ids;
}

constexpr auto kGameClientLinkIDs = gameClientLinkIDs();
#endif

} // namespace

#if defined(__GAME_SERVER__)
std::span<const PacketID_t> de::packet::gameClientLinkPacketIDs() {
    return kGameClientLinkIDs;
}
#endif

void PacketFactoryManager::init() {
    __BEGIN_TRY

    ServerFactories::forEach([this]<typename Factory>(std::type_identity<Factory>) { addFactory(new Factory()); });

#if __OUTPUT_INIT__
    cout << toString() << endl;
#endif

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////
//
// Insert the factory into its slot using the packet ID as the index.
//
//////////////////////////////////////////////////////////////////////
void PacketFactoryManager::addFactory(PacketFactory* pFactory) {
    __BEGIN_TRY

    if (m_Factories[pFactory->getPacketID()] != NULL) {
        StringStream msg;
#ifdef __GAME_CLIENT__
#ifdef __DEBUG_OUTPUT__
        msg << "duplicate packet factories, " << pFactory->getPacketName();
#else
        msg << "duplicate packet factories ";
#endif
#else
        msg << "duplicate packet factories, " << pFactory->getPacketName();
#endif
        throw Error(msg.toString());
    }

    // Store the factory in the lookup table.
    m_Factories[pFactory->getPacketID()] = pFactory;

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////
//
// Create a packet instance for the given packet ID.
//
//////////////////////////////////////////////////////////////////////
Packet* PacketFactoryManager::createPacket(PacketID_t packetID) {
    __BEGIN_TRY

    // Guard against invalid IDs to avoid accessing an empty slot and crashing.
    if (packetID >= m_Size || m_Factories[packetID] == NULL) {
        StringStream msg;
        msg << "packet factory [" << packetID << "] not exist.";
        throw InvalidProtocolException(msg.toString());
    }

    return m_Factories[packetID]->createPacket();

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////
//
// Return the max packet size for the given packet ID.
//
//////////////////////////////////////////////////////////////////////
PacketSize_t PacketFactoryManager::getPacketMaxSize(PacketID_t packetID) {
    __BEGIN_TRY

    // Guard against invalid IDs to avoid accessing an empty slot and crashing.
    if (packetID >= m_Size || m_Factories[packetID] == NULL) {
        StringStream msg;
        msg << "invalid packet id(" << packetID << ")";
        throw InvalidProtocolException(msg.toString());
    }

    return m_Factories[packetID]->getPacketMaxSize();

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////
//
// Return the packet name for the given packet ID.
//
//////////////////////////////////////////////////////////////////////
#if !defined(__GAME_CLIENT__) || defined(__GAME_CLIENT__) && defined(__DEBUG_OUTPUT__)
string PacketFactoryManager::getPacketName(PacketID_t packetID) {
    __BEGIN_TRY

    // Guard against invalid IDs to avoid accessing an empty slot and crashing.
    if (packetID >= m_Size || m_Factories[packetID] == NULL) {
        StringStream msg;
        msg << "invalid packet id(" << packetID << ")";
        throw InvalidProtocolException(msg.toString());
    }

    return m_Factories[packetID]->getPacketName();

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////
// get debug string
//////////////////////////////////////////////////////////////////////
string PacketFactoryManager::toString() const {
    __BEGIN_TRY

    StringStream msg;

    msg << "PacketFactoryManager(\n";

    for (int i = 0; i < m_Size; i++)
        msg << "PacketFactories[" << i << "] == " << (m_Factories[i] == NULL ? "NULL" : m_Factories[i]->getPacketName())
            << "\n";

    msg << ")";

    return msg.toString();

    __END_CATCH
}
#endif
