////////////////////////////////////////////////////////////////////////////////
// Filename    : TriggerManager.cpp
// Written By  :
// Description :
////////////////////////////////////////////////////////////////////////////////

#include "TriggerManager.h"

#include <stdio.h>

#include "Assert.h"
#include "GameContext.h"
#include "Properties.h"
#include "ScriptManager.h"
#include "TriggerParser.h"
#include "repository/ContentInfoRepository.h"
#include "repository/ZoneInfoRepository.h"

class isSameTriggerID {
public:
    isSameTriggerID(TriggerID_t triggerID) : m_TriggerID(triggerID) {}
    bool operator()(Trigger* pTrigger) {
        return pTrigger->getTriggerID() == m_TriggerID;
    }

private:
    TriggerID_t m_TriggerID;
};

////////////////////////////////////////////////////////////////////////////////
// constructor
////////////////////////////////////////////////////////////////////////////////
TriggerManager::TriggerManager()

{}


////////////////////////////////////////////////////////////////////////////////
// destructor
////////////////////////////////////////////////////////////////////////////////
TriggerManager::~TriggerManager()

{
    __BEGIN_TRY
    __END_CATCH_NO_RETHROW
}


////////////////////////////////////////////////////////////////////////////////
// Loads the triggers related to the given NPC name from the database.
////////////////////////////////////////////////////////////////////////////////
void TriggerManager::load(const string& name)

{
    vector<NPCTriggerRow> rows = defaultContentInfoRepository().loadNPCTriggers(name);

    for (vector<NPCTriggerRow>::const_iterator it = rows.begin(); it != rows.end(); ++it) {
        Trigger* pTrigger = new Trigger(de::gameContext());

        pTrigger->setTriggerID(it->triggerID);


        pTrigger->setTriggerType(trim(it->triggerType));
        pTrigger->setConditions(trim(it->conditions));
        pTrigger->setActions(trim(it->actions));

        addTrigger(pTrigger);
    }
}

////////////////////////////////////////////////////////////////////////////////
// Loads the triggers related to the given zone coordinates from the database.
////////////////////////////////////////////////////////////////////////////////
void TriggerManager::load(ZoneID_t zoneid, int left, int top, int right, int bottom) {
    vector<ZoneTriggerRow> rows = defaultZoneInfoRepository().loadZoneTriggers((int)zoneid, left, top, right, bottom);

    for (vector<ZoneTriggerRow>::const_iterator it = rows.begin(); it != rows.end(); ++it) {
        Trigger* pTrigger = new Trigger(de::gameContext());

        pTrigger->setTriggerID(it->triggerID);


        pTrigger->setTriggerType(trim(it->triggerType));
        pTrigger->setConditions(trim(it->conditions));
        pTrigger->setActions(trim(it->actions));
        pTrigger->setCounterActions(trim(it->counterActions));


        addTrigger(pTrigger);
    }
}


////////////////////////////////////////////////////////////////////////////////
// refresh condition set
////////////////////////////////////////////////////////////////////////////////
void TriggerManager::refresh() {
    // OR the ConditionSet of every trigger held here into m_ConditionSet.
    for (list<Trigger*>::const_iterator itr = m_Triggers.begin(); itr != m_Triggers.end(); itr++) {
        m_ConditionSet |= (*itr)->getConditionSet();
    }
}


////////////////////////////////////////////////////////////////////////////////
// add trigger
////////////////////////////////////////////////////////////////////////////////
void TriggerManager::addTrigger(Trigger* pTrigger) {
    Assert(pTrigger != NULL);

    list<Trigger*>::iterator itr = m_Triggers.begin();
    for (; itr != m_Triggers.end(); itr++) {
        if ((*itr) == pTrigger) {
            break;
        }
    }

    if (itr != m_Triggers.end())
        throw DuplicatedException("duplicated trigger");

    m_Triggers.push_back(pTrigger);

    m_ConditionSet |= pTrigger->getConditionSet();
}


////////////////////////////////////////////////////////////////////////////////
// delete trigger
////////////////////////////////////////////////////////////////////////////////
void TriggerManager::deleteTrigger(TriggerID_t triggerID) {
    list<Trigger*>::iterator itr;
    for (itr = m_Triggers.begin(); itr != m_Triggers.end(); itr++) {
        if ((*itr)->getTriggerID() == triggerID) {
            break;
        }
    }

    if (itr != m_Triggers.end())
        throw NoSuchElementException();

    // delete trigger object
    delete *itr;

    // delete node
    m_Triggers.erase(itr);

    // Rebuild the condition set.
    refresh();
}


////////////////////////////////////////////////////////////////////////////////
// get trigger
////////////////////////////////////////////////////////////////////////////////
Trigger* TriggerManager::getTrigger(TriggerID_t triggerID) {
    list<Trigger*>::iterator itr;
    for (itr = m_Triggers.begin(); itr != m_Triggers.end(); itr++) {
        if ((*itr)->getTriggerID() == triggerID) {
            break;
        }
    }

    if (itr != m_Triggers.end())
        throw NoSuchElementException();

    return *itr;
}


////////////////////////////////////////////////////////////////////////////////
// get debug string
////////////////////////////////////////////////////////////////////////////////
string TriggerManager::toString() const {
    StringStream msg;
    return msg.toString();
}
