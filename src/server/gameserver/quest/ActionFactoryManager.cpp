////////////////////////////////////////////////////////////////////////////////
// Filename    : ActionFactoryManager.cpp
// Written By  :
// Description :
////////////////////////////////////////////////////////////////////////////////

#include "ActionFactoryManager.h"

#include "ActionAcceptApartCouple.h"
#include "ActionAcceptCoupleRequest.h"
#include "ActionAcceptReinforce.h"
#include "ActionActivateMazeEnter.h"
#include "ActionActivateMazeExit.h"
#include "ActionActivateMazeReturn.h"
#include "ActionActivatePortal.h"
#include "ActionAdvanceEventQuest.h"
#include "ActionAsk.h"
#include "ActionAskByQuestLevel.h"
#include "ActionAskDynamic.h"
#include "ActionAskVariable.h"
#include "ActionBuy.h"
#include "ActionCallHelicopter.h"
#include "ActionCancelEventQuestScript.h"
#include "ActionCancelQuest.h"
#include "ActionCheckPartnerName.h"
#include "ActionClearBloodBible.h"
#include "ActionClearRankBonus.h"
#include "ActionContractGnomesHorn.h"
#include "ActionCreateGuild.h"
#include "ActionDenyReinforce.h"
#include "ActionDestroyGuild.h"
#include "ActionDownSkill.h"
#include "ActionEnterCastleWithFee.h"
#include "ActionEnterEventZone.h"
#include "ActionEnterGDRLair.h"
#include "ActionEnterPKZone.h"
#include "ActionEnterQuestZone.h"
#include "ActionEnterSiege.h"
#include "ActionEventMeet.h"
#include "ActionForceApartCouple.h"
#include "ActionGiveAccountEventItem.h"
#include "ActionGiveEventItem.h"
#include "ActionGiveEventQuest.h"
#include "ActionGiveEventQuestScript.h"
#include "ActionGiveFinalLottoScript.h"
#include "ActionGiveItem.h"
#include "ActionGiveLotto.h"
#include "ActionGiveNewbieItem.h"
#include "ActionGiveQuestItem.h"
#include "ActionGiveSpecialEventItem.h"
#include "ActionGiveTestServerReward.h"
#include "ActionHeal.h"
#include "ActionInitEventQuest.h"
#include "ActionInitPartnerWaitingManager.h"
#include "ActionInitSimpleQuest.h"
#include "ActionJoinRaceWar.h"
#include "ActionMiniGame.h"
#include "ActionModifyTaxRatio.h"
#include "ActionPetDeposit.h"
#include "ActionPetWithdraw.h"
#include "ActionPrepareShop.h"
#include "ActionQuestReward.h"
#include "ActionQuitDialogue.h"
#include "ActionRandomSay.h"
#include "ActionRecallSiege.h"
#include "ActionRedeemMotorcycle.h"
#include "ActionRedistributeAttr.h"
#include "ActionRegenEventShop.h"
#include "ActionRegenShop.h"
#include "ActionRegisterReinforce.h"
#include "ActionRegisterSiege.h"
#include "ActionRepair.h"
#include "ActionRestore.h"
#include "ActionRewardEventQuest.h"
#include "ActionSay.h"
#include "ActionSayDynamic.h"
#include "ActionSearchMotorcycle.h"
#include "ActionSelectBloodBible.h"
#include "ActionSelectQuest.h"
#include "ActionSell.h"
#include "ActionSetPosition.h"
#include "ActionSetResurrectZone.h"
#include "ActionShowConfirmGetEventItemDialog.h"
#include "ActionShowDonationDialog.h"
#include "ActionShowGuildDialog.h"
#include "ActionShowTaxBalance.h"
#include "ActionShowWarSchedule.h"
#include "ActionSilverCoating.h"
#include "ActionSimpleQuestComplete.h"
#include "ActionSimpleQuestList.h"
#include "ActionSimpleQuestRegen.h"
#include "ActionStartEventQuest.h"
#include "ActionStartPetQuest.h"
#include "ActionStashOpen.h"
#include "ActionStashSell.h"
#include "ActionSwapAdvancementItem.h"
#include "ActionSystemMessage.h"
#include "ActionSystemMessagePerRace.h"
#include "ActionTakeDonation.h"
#include "ActionTakeOutGoods.h"
#include "ActionTeachSkill.h"
#include "ActionTradeEventItem.h"
#include "ActionTradeGQuestEventItem.h"
#include "ActionTradeGiftBox.h"
#include "ActionTradeLairItem.h"
#include "ActionTurnOffFlag.h"
#include "ActionTurnOnFlag.h"
#include "ActionTutorial.h"
#include "ActionWaitForApartCouple.h"
#include "ActionWaitForMeetCouple.h"
#include "ActionWander.h"
#include "ActionWanderZone.h"
#include "ActionWarRegistration.h"
#include "ActionWarpInZone.h"
#include "ActionWarpLevelWarZone.h"
#include "ActionWarpToNoviceZone.h"
#include "ActionWarpToResurrectPosition.h"
#include "Assert.h"
#include "StringStream.h"

////////////////////////////////////////////////////////////////////////////////
// constructor
////////////////////////////////////////////////////////////////////////////////
ActionFactoryManager::ActionFactoryManager(de::GameContext& context)

    : m_Context(context), m_Factories(NULL), m_Size(Action::ACTION_MAX) {
    Assert(m_Size > 0);

    // Create the action factory array.
    m_Factories = new ActionFactory*[m_Size];

    // Initialize the factory pointers to NULL.
    for (int i = 0; i < m_Size; i++)
        m_Factories[i] = NULL;
}


////////////////////////////////////////////////////////////////////////////////
// destructor
////////////////////////////////////////////////////////////////////////////////
ActionFactoryManager::~ActionFactoryManager()

{
    try {
        Assert(m_Factories != NULL);

        // Delete each action factory.
        for (int i = 0; i < m_Size; i++) {
            if (m_Factories[i] != NULL) {
                delete m_Factories[i];
                m_Factories[i] = NULL;
            }
        }

        // Delete the action factory array.
        delete[] m_Factories;
        m_Factories = NULL;
    } catch (Throwable&) {
        // A Throwable from the try block is dropped here and goes no further.
    }
}


////////////////////////////////////////////////////////////////////////////////
// Adds every defined action factory here.
////////////////////////////////////////////////////////////////////////////////
void ActionFactoryManager::init()

{
    addFactory(new ActionSetPositionFactory());
    addFactory(new ActionWanderFactory());
    addFactory(new ActionSayFactory());
    addFactory(new ActionRandomSayFactory());
    addFactory(new ActionAskFactory());
    addFactory(new ActionQuitDialogueFactory());
    addFactory(new ActionPrepareShopFactory());
    addFactory(new ActionSellFactory());
    addFactory(new ActionBuyFactory());
    addFactory(new ActionRegenShopFactory());
    addFactory(new ActionTeachSkillFactory());
    addFactory(new ActionHealFactory());
    addFactory(new ActionRedeemMotorcycleFactory());
    addFactory(new ActionSearchMotorcycleFactory());
    addFactory(new ActionRepairFactory());
    addFactory(new ActionStashSellFactory());
    addFactory(new ActionStashOpenFactory());
    addFactory(new ActionRestoreFactory());
    addFactory(new ActionSetResurrectZoneFactory());
    addFactory(new ActionSilverCoatingFactory());
    addFactory(new ActionCreateGuildFactory());
    addFactory(new ActionDestroyGuildFactory());
    addFactory(new ActionTutorialFactory());
    addFactory(new ActionGiveNewbieItemFactory());
    addFactory(new ActionActivatePortalFactory());
    addFactory(new ActionTurnOnFlagFactory());
    addFactory(new ActionTurnOffFlagFactory());
    addFactory(new ActionSystemMessageFactory());
    addFactory(new ActionTakeDonationFactory());
    addFactory(new ActionCallHelicopterFactory());
    addFactory(new ActionRegenEventShopFactory());
    addFactory(new ActionAskDynamicFactory());
    addFactory(new ActionSayDynamicFactory());
    addFactory(new ActionGiveSpecialEventItemFactory());
    addFactory(new ActionRedistributeAttrFactory());
    addFactory(new ActionTradeEventItemFactory());
    addFactory(new ActionTradeLairItemFactory());
    addFactory(new ActionShowGuildDialogFactory());
    addFactory(new ActionSimpleQuestListFactory());
    addFactory(new ActionSimpleQuestCompleteFactory());
    addFactory(new ActionSimpleQuestRegenFactory());
    addFactory(new ActionTradeGiftBoxFactory());
    addFactory(new ActionGiveTestServerRewardFactory());
    addFactory(new ActionEnterCastleWithFeeFactory());
    addFactory(new ActionShowTaxBalanceFactory());
    addFactory(new ActionAskVariableFactory());
    addFactory(new ActionWarRegistrationFactory());
    addFactory(new ActionShowWarScheduleFactory());
    addFactory(new ActionJoinRaceWarFactory());
    addFactory(new ActionGiveEventItemFactory());
    addFactory(new ActionGiveAccountEventItemFactory());

    addFactory(new ActionInitPartnerWaitingManagerFactory());
    addFactory(new ActionWaitForMeetCoupleFactory());
    addFactory(new ActionWaitForApartCoupleFactory());
    addFactory(new ActionAcceptCoupleRequestFactory());
    addFactory(new ActionAcceptApartCoupleFactory());
    addFactory(new ActionForceApartCoupleFactory());
    addFactory(new ActionEnterPKZoneFactory());
    addFactory(new ActionWarpToResurrectPositionFactory());
    addFactory(new ActionSelectQuestFactory());
    addFactory(new ActionQuestRewardFactory());
    addFactory(new ActionWarpToNoviceZoneFactory());
    addFactory(new ActionCancelQuestFactory());
    addFactory(new ActionInitSimpleQuestFactory());
    addFactory(new ActionWanderZoneFactory());
    addFactory(new ActionInitEventQuestFactory());
    addFactory(new ActionGiveEventQuestFactory());
    addFactory(new ActionAdvanceEventQuestFactory());
    addFactory(new ActionRewardEventQuestFactory());
    addFactory(new ActionCancelEventQuestScriptFactory());
    addFactory(new ActionGiveLottoFactory());
    addFactory(new ActionAskByQuestLevelFactory());
    addFactory(new ActionGiveEventQuestScriptFactory());
    addFactory(new ActionStartEventQuestFactory());
    addFactory(new ActionWarpInZoneFactory());
    addFactory(new ActionEventMeetFactory());
    addFactory(new ActionGiveFinalLottoScriptFactory());
    addFactory(new ActionGiveQuestItemFactory());
    addFactory(new ActionTakeOutGoodsFactory());
    addFactory(new ActionClearRankBonusFactory());
    addFactory(new ActionContractGnomesHornFactory());
    addFactory(new ActionDownSkillFactory());
    addFactory(new ActionMiniGameFactory());
    addFactory(new ActionGiveItemFactory());
    addFactory(new ActionActivateMazeExitFactory());
    addFactory(new ActionActivateMazeEnterFactory());
    addFactory(new ActionActivateMazeReturnFactory());
    addFactory(new ActionSystemMessagePerRaceFactory());
    addFactory(new ActionWarpLevelWarZoneFactory());
    addFactory(new ActionCheckPartnerNameFactory());
    addFactory(new ActionStartPetQuestFactory());

    addFactory(new ActionPetDepositFactory());
    /* Pet stash: only PetDeposit is used; Withdraw is not. */
    /* addFactory(new ActionPetWithdrawFactory());	*/

    addFactory(new ActionEnterEventZoneFactory());
    addFactory(new ActionEnterGDRLairFactory());
    addFactory(new ActionTradeGQuestEventItemFactory());
    addFactory(new ActionRegisterSiegeFactory());
    addFactory(new ActionRegisterReinforceFactory());
    addFactory(new ActionAcceptReinforceFactory());
    addFactory(new ActionDenyReinforceFactory());
    addFactory(new ActionEnterSiegeFactory());
    addFactory(new ActionRecallSiegeFactory());
    addFactory(new ActionSelectBloodBibleFactory());
    addFactory(new ActionClearBloodBibleFactory());
    addFactory(new ActionModifyTaxRatioFactory());
    addFactory(new ActionSwapAdvancementItemFactory());
    addFactory(new ActionShowDonationDialogFactory());
    addFactory(new ActionEnterQuestZoneFactory());
    addFactory(new ActionShowConfirmGetEventItemDialogFactory());
}


////////////////////////////////////////////////////////////////////////////////
// add condition factory to factories array
////////////////////////////////////////////////////////////////////////////////
void ActionFactoryManager::addFactory(ActionFactory* pFactory)

{
    Assert(pFactory != NULL);

    if (m_Factories[pFactory->getActionType()] != NULL) {
        StringStream msg;
        msg << "duplicate action factories, " << pFactory->getActionName();
        throw Error(msg.toString());
    }

    // Register the action factory.
    m_Factories[pFactory->getActionType()] = pFactory;
}


////////////////////////////////////////////////////////////////////////////////
// create condition object with condition type
////////////////////////////////////////////////////////////////////////////////
Action* ActionFactoryManager::createAction(ActionType_t actionType) const

{
    // Guard against a segfault from a type outside the factory array.
    // An unknown type is logged and throws an Error.
    if (actionType >= m_Size || m_Factories[actionType] == NULL) {
        StringStream msg;
        msg << "action factory [" << actionType << "] not exist.";
        cout << msg.toString() << endl;
        throw Error(msg.toString());
    }

    Action* pAction = m_Factories[actionType]->createAction();
    pAction->setContext(m_Context);

    return pAction;
}


////////////////////////////////////////////////////////////////////////////////
// get condition name with condition type
////////////////////////////////////////////////////////////////////////////////
string ActionFactoryManager::getActionName(ActionType_t actionType) const

{
    // Guard against a segfault from a type outside the factory array.
    // An unknown type is logged and throws an Error.
    if (actionType >= m_Size || m_Factories[actionType] == NULL) {
        StringStream msg;
        msg << "invalid action type (" << actionType << ")";
        cout << msg.toString() << endl;
        throw Error(msg.toString());
    }

    return m_Factories[actionType]->getActionName();
}


////////////////////////////////////////////////////////////////////////////////
// get action type with action type name
////////////////////////////////////////////////////////////////////////////////
ActionType_t ActionFactoryManager::getActionType(const string& actionname) const

{
#ifdef __OUTPUT_INIT__
    cout << "Action[" << actionname << "]..." << endl;
#endif

    for (int i = 0; i < m_Size; i++) {
        if (m_Factories[i] != NULL) {
            if (m_Factories[i]->getActionName() == actionname) {
                return i;
            }
        }
    }

    string msg = "no such action type : " + actionname;
    cout << msg << endl;
    throw Error(msg);
}


////////////////////////////////////////////////////////////////////////////////
// get debug string
////////////////////////////////////////////////////////////////////////////////
string ActionFactoryManager::toString() const

{
    StringStream msg;

    msg << "ActionFactoryManager(\n";

    for (int i = 0; i < m_Size; i++) {
        msg << "ActionFactories[" << i << "] == ";
        msg << (m_Factories[i] == NULL ? "NULL" : m_Factories[i]->getActionName());
        msg << "\n";
    }

    msg << ")";

    return msg.toString();
}
