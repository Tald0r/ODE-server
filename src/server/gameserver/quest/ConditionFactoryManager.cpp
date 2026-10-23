////////////////////////////////////////////////////////////////////////////////
// Filename    : ConditionFactoryManager.cpp
// Written By  :
// Description :
////////////////////////////////////////////////////////////////////////////////

#include "ConditionFactoryManager.h"

#include "Assert.h"
#include "ConditionAnsweredBy.h"
#include "ConditionAtFirst.h"
#include "ConditionAttrComp.h"
#include "ConditionCanEnterBeginnerZone.h"
#include "ConditionCanEnterEventZone.h"
#include "ConditionCanEnterGDRLair.h"
#include "ConditionCanEnterLevelWarZone.h"
#include "ConditionCanEnterPayZone.h"
#include "ConditionCanEnterQuestZone.h"
#include "ConditionCanPetQuest.h"
#include "ConditionCanWarpGate.h"
#include "ConditionEffectFlag.h"
#include "ConditionEnterCastle.h"
#include "ConditionEnterCastleDungeon.h"
#include "ConditionEnterHolyLand.h"
#include "ConditionEnterMasterLair.h"
#include "ConditionEveryTime.h"
#include "ConditionExistReinforce.h"
#include "ConditionFlagOff.h"
#include "ConditionFlagOn.h"
#include "ConditionHasInvenSpace.h"
#include "ConditionHasQuest.h"
#include "ConditionIdle.h"
#include "ConditionIsGuildMember.h"
#include "ConditionNotGuildMember.h"
#include "ConditionPayPlay.h"
#include "ConditionRaceComp.h"
#include "ConditionSameClan.h"
#include "ConditionSiegeAttackerSide.h"
#include "ConditionSiegeDefenderSide.h"
#include "ConditionTalkedBy.h"
#include "StringStream.h"

////////////////////////////////////////////////////////////////////////////////
// constructor
////////////////////////////////////////////////////////////////////////////////
ConditionFactoryManager::ConditionFactoryManager()

    : m_Factories(NULL), m_Size(Condition::CONDITION_MAX) {
    Assert(m_Size > 0);

    // Create the condition factory array.
    m_Factories = new ConditionFactory*[m_Size];

    // Initialize the factory pointers to NULL.
    for (int i = 0; i < m_Size; i++)
        m_Factories[i] = NULL;
}


////////////////////////////////////////////////////////////////////////////////
// destructor
////////////////////////////////////////////////////////////////////////////////
ConditionFactoryManager::~ConditionFactoryManager()

{
    try {
        Assert(m_Factories != NULL);

        // Delete each condition factory.
        for (int i = 0; i < m_Size; i++) {
            if (m_Factories[i] != NULL) {
                delete m_Factories[i];
                m_Factories[i] = NULL;
            }
        }

        // Delete the condition factory array.
        delete[] m_Factories;
        m_Factories = NULL;
    } catch (Throwable&) {
        // A Throwable from the try block is dropped here and goes no further.
    }
}


////////////////////////////////////////////////////////////////////////////////
// Adds every defined condition factory here.
////////////////////////////////////////////////////////////////////////////////
void ConditionFactoryManager::init()

{
    addFactory(new ConditionAtFirstFactory());
    addFactory(new ConditionTalkedByFactory());
    addFactory(new ConditionIdleFactory());
    addFactory(new ConditionEveryTimeFactory());
    addFactory(new ConditionAnsweredByFactory());
    addFactory(new ConditionFlagOnFactory());
    addFactory(new ConditionFlagOffFactory());
    addFactory(new ConditionAttrCompFactory());
    addFactory(new ConditionRaceCompFactory());
    addFactory(new ConditionSameClanFactory());
    addFactory(new ConditionPayPlayFactory());
    addFactory(new ConditionEnterMasterLairFactory());
    addFactory(new ConditionEnterCastleFactory());
    addFactory(new ConditionEnterHolyLandFactory());
    addFactory(new ConditionEnterCastleDungeonFactory());
    addFactory(new ConditionHasQuestFactory());
    addFactory(new ConditionHasInvenSpaceFactory());
    addFactory(new ConditionCanEnterPayZoneFactory());
    addFactory(new ConditionCanEnterBeginnerZoneFactory());
    addFactory(new ConditionCanWarpGateFactory());
    addFactory(new ConditionCanEnterLevelWarZoneFactory());
    addFactory(new ConditionCanPetQuestFactory());
    addFactory(new ConditionCanEnterEventZoneFactory());
    addFactory(new ConditionEffectFlagFactory());
    addFactory(new ConditionCanEnterGDRLairFactory());
    addFactory(new ConditionExistReinforceFactory());
    addFactory(new ConditionSiegeDefenderSideFactory());
    addFactory(new ConditionSiegeAttackerSideFactory());
    addFactory(new ConditionNotGuildMemberFactory());
    addFactory(new ConditionIsGuildMemberFactory());
    addFactory(new ConditionCanEnterQuestZoneFactory());
}


////////////////////////////////////////////////////////////////////////////////
// add condition factory to factories array
////////////////////////////////////////////////////////////////////////////////
void ConditionFactoryManager::addFactory(ConditionFactory* pFactory)

{
    Assert(pFactory != NULL);

    if (m_Factories[pFactory->getConditionType()] != NULL) {
        StringStream msg;
        msg << "duplicate condition factories, " << pFactory->getConditionName();
        cout << msg.toString() << endl;
        throw Error(msg.toString());
    }

    // Register the condition factory.
    m_Factories[pFactory->getConditionType()] = pFactory;
}


////////////////////////////////////////////////////////////////////////////////
// create condition object with condition type
////////////////////////////////////////////////////////////////////////////////
Condition* ConditionFactoryManager::createCondition(ConditionType_t conditionType) const

{
    if (conditionType >= m_Size || m_Factories[conditionType] == NULL) {
        StringStream msg;
        msg << "condition factory [" << conditionType << "] not exist.";
        cout << msg.toString() << endl;
        throw Error(msg.toString());
    }

    return m_Factories[conditionType]->createCondition();
}


////////////////////////////////////////////////////////////////////////////////
// get condition name with condition type
////////////////////////////////////////////////////////////////////////////////
string ConditionFactoryManager::getConditionName(ConditionType_t conditionType) const

{
    // Guard against a segfault from a condition type that is out of range.
    // An unknown type is logged and throws an Error.
    if (conditionType >= m_Size || m_Factories[conditionType] == NULL) {
        StringStream msg;
        msg << "invalid condition type (" << conditionType << ")";
        cout << msg.toString() << endl;
        throw Error(msg.toString());
    }

    return m_Factories[conditionType]->getConditionName();
}


////////////////////////////////////////////////////////////////////////////////
// get debug string
////////////////////////////////////////////////////////////////////////////////
ConditionType_t ConditionFactoryManager::getConditionType(const string& conditionname) const

{
#ifdef __OUTPUT_INIT__
    cout << "Condition[" << conditionname << "]...";
#endif

    for (int i = 0; i < m_Size; i++) {
        if (m_Factories[i] != NULL) {
            if (m_Factories[i]->getConditionName() == conditionname) {
                return i;
            }
        }
    }

    string msg = "no such condition type : " + conditionname;
    cout << msg << endl;
    throw Error(msg);
}


////////////////////////////////////////////////////////////////////////////////
// get debug string
////////////////////////////////////////////////////////////////////////////////
string ConditionFactoryManager::toString() const

{
    StringStream msg;

    msg << "ConditionFactoryManager(\n";

    for (int i = 0; i < m_Size; i++) {
        msg << "ConditionFactories[" << i << "] == ";
        msg << (m_Factories[i] == NULL ? "NULL" : m_Factories[i]->getConditionName());
        msg << "\n";
    }

    msg << ")";

    return msg.toString();
}
