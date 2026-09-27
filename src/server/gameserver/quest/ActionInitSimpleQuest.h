//////////////////////////////////////////////////////////////////////////////
// Filename    : ActionInitSimpleQuest.h
// Written By  : excel96
// Description :
// The creature asks the PC a question. The line is printed in the NPC dialogue window.
//////////////////////////////////////////////////////////////////////////////

#ifndef __ACTION_INIT_SIMPLE_QUEST_H__
#define __ACTION_INIT_SIMPLE_QUEST_H__

#include "Action.h"
#include "ActionFactory.h"
#include "Exception.h"
#include "Types.h"

//////////////////////////////////////////////////////////////////////////////
// class ActionInitSimpleQuest
//////////////////////////////////////////////////////////////////////////////

class ActionInitSimpleQuest : public Action {
public:
    virtual ActionType_t getActionType() const {
        return ACTION_INIT_SIMPLE_QUEST;
    }
    virtual void read(PropertyBuffer& propertyBuffer);
    virtual void execute(Creature* pCreature1, Creature* pCreature2 = NULL);
    virtual string toString() const;

public:
private:
    [[maybe_unused]] Race_t m_Race;
};


//////////////////////////////////////////////////////////////////////////////
// class ActionInitSimpleQuestFactory;
//////////////////////////////////////////////////////////////////////////////

class ActionInitSimpleQuestFactory : public ActionFactory {
public:
    virtual ActionType_t getActionType() const {
        return Action::ACTION_INIT_SIMPLE_QUEST;
    }
    virtual string getActionName() const {
        return "InitSimpleQuest";
    }
    virtual Action* createAction() const {
        return new ActionInitSimpleQuest();
    }
};
#endif
