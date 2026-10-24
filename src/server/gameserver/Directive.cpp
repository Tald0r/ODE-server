////////////////////////////////////////////////////////////////////////
// Filename    : Directive.cpp
// Description :
// Implements the Directive and DirectiveSet that decide the behaviour
// used by MonsterAI.
////////////////////////////////////////////////////////////////////////

#include "Directive.h"

#include <stdio.h>
#include <string.h>

#include "Assert.h"
#include "StringStream.h"
#include "repository/ContentInfoRepository.h"
#include "skill/Skill.h"

// Constants.
#define MODE_CONDITION 0
#define MODE_ACTION 1

////////////////////////////////////////////////////////////////////////
// class Directive member methods
////////////////////////////////////////////////////////////////////////

Directive::Directive()

{
    m_Action = 0;
    m_Parameter = 0;
    m_Ratio = 0;
    m_Weight = 0;
}

Directive::~Directive()

{
    m_Conditions.clear();
}

void Directive::addCondition(int condition)

{
    // Check that the same condition is not already present.
    list<int>::iterator itr = m_Conditions.begin();
    for (; itr != m_Conditions.end(); itr++) {
        if (condition == *itr)
            return;
    }

    m_Conditions.push_back(condition);
}

string Directive::toString()

{
    StringStream msg;

    // header
    msg << "Directive(";

    // condition output
    msg << "Condition : ";
    list<int>::const_iterator itr = m_Conditions.begin();
    for (; itr != m_Conditions.end(); itr++)
        msg << DirectiveCondition2String[*itr] << ",";
    msg << " ";

    // action output
    msg << "Action : " << DirectiveAction2String[m_Action] << " ";

    // parameter output
    if (m_Parameter != 0)
        msg << "Parameter : " << SkillTypes2String[m_Parameter] << " ";

    // ratio output
    if (m_Ratio != 0)
        msg << "Ratio : " << m_Ratio;

    // tail
    msg << ")";

    return msg.toString();
}

////////////////////////////////////////////////////////////////////////
// class DirectiveSet member methods
////////////////////////////////////////////////////////////////////////

DirectiveSet::DirectiveSet()

{}

DirectiveSet::~DirectiveSet()

{
    // Walk the list and delete the Directive objects.
    list<Directive*>::iterator itr = m_Directives.begin();
    for (; itr != m_Directives.end(); itr++) {
        Directive* pDirective = *itr;
        SAFE_DELETE(pDirective);
    }

    m_Directives.clear();
}

void DirectiveSet::clear()

{
    // Walk the list and delete the Directive objects.
    list<Directive*>::iterator itr = m_Directives.begin();
    for (; itr != m_Directives.end(); itr++) {
        Directive* pDirective = *itr;
        SAFE_DELETE(pDirective);
    }

    m_Directives.clear();
}

bool DirectiveSet::hasCondition(int condition) const {
    list<Directive*>::const_iterator itr = m_Directives.begin();
    list<Directive*>::const_iterator endItr = m_Directives.end();

    for (; itr != endItr; ++itr) {
        if ((*itr)->hasCondition(condition))
            return true;
    }

    return false;
}

void DirectiveSet::addDirective(Directive* pDirective)

{
    m_Directives.push_back(pDirective);
}

void DirectiveSet::addDeadDirective(Directive* pDirective)

{
    m_DeadDirectives.push_back(pDirective);
}


////////////////////////////////////////////////////////////////////////
// Constructor
////////////////////////////////////////////////////////////////////////
DirectiveSetManager::DirectiveSetManager()

{
    m_nSetCount = 0;
    m_ppSet = NULL;
}

////////////////////////////////////////////////////////////////////////
// Destructor
////////////////////////////////////////////////////////////////////////
DirectiveSetManager::~DirectiveSetManager()

{
    if (m_ppSet) {
        for (uint i = 0; i < m_nSetCount; i++) {
            SAFE_DELETE(m_ppSet[i]);
        }

        SAFE_DELETE_ARRAY(m_ppSet); // This is unlikely ever to be reached.
    }
}

////////////////////////////////////////////////////////////////////////
// init()
// Initializes the object. Initialization happens in ObjectManager.
////////////////////////////////////////////////////////////////////////
void DirectiveSetManager::init()

{
    load();
}


////////////////////////////////////////////////////////////////////////
// load()
// Loads every DirectiveSet from the DB.
// MonsterAI objects only hold pointers to the loaded DirectiveSets.
////////////////////////////////////////////////////////////////////////
void DirectiveSetManager::load()

{
    ContentInfoRepository& repository = defaultContentInfoRepository();

    // The number of sets.
    int maxID = 0;
    if (!repository.loadMaxDirectiveSetID(maxID))
        throw Error("DirectiveSetManager::load() : not in the table");

    m_nSetCount = maxID + 1;

    // Allocate that many slots...
    if (m_ppSet == NULL) {
        m_ppSet = new DirectiveSet*[m_nSetCount];
        Assert(m_ppSet != NULL);
        for (uint i = 0; i < m_nSetCount; i++) {
            m_ppSet[i] = NULL;
        }
    }

    // ...and read each directive set.
    vector<DirectiveSetRow> rows = repository.loadDirectiveSets();

    for (size_t r = 0; r < rows.size(); r++) {
        uint index = rows[r].id;
        string name = rows[r].name;
        string text = rows[r].content;
        string deadtext = rows[r].deadContent;

        printf("ID[%d] Directive Loading Begin >> ", index);

        ////////////////
        /// filelog("Directive.txt", "===============================================");
        // filelog("Directive.txt", "Name:%s", name.c_str());
        ////////////////

        createDirectiveSet(index, name, text, deadtext);

        printf("Loading End\n");
    }
}

////////////////////////////////////////////////////////////////////////
// getDirectiveSet()
// Returns the pointer to the DirectiveSet of the given type.
////////////////////////////////////////////////////////////////////////
DirectiveSet* DirectiveSetManager::getDirectiveSet(uint index)

{
    if (index >= m_nSetCount) {
        cerr << "DirectiveSetManager::getDirectiveSet() : Out of bounds, value = [" << index << "]" << endl;
        throw Error("DirectiveSetManager::getDirectiveSet() : Out of bounds!");
    }

    return m_ppSet[index];
}

////////////////////////////////////////////////////////////////////////
// createDirectiveSet()
// Parses the string and creates a DirectiveSet at the given index.
////////////////////////////////////////////////////////////////////////
void DirectiveSetManager::createDirectiveSet(uint index, const string& name, const string& text, const string& deadtext)

{
    if (index >= m_nSetCount) {
        cerr << "DirectiveSetManager::createDirectiveSet() : Out of bounds!" << endl;
        throw Error("DirectiveSetManager::createDirectiveSet() : Out of bounds!");
    }

    // Delete the previous DirectiveSet and make a new one.
    if (m_ppSet[index] == NULL) // by sigi. 2002.9.26
    {
        // SAFE_DELETE(m_ppSet[index]);
        m_ppSet[index] = new DirectiveSet;
    } else {
        // Clear whatever was there before.
        m_ppSet[index]->clear();
    }

    m_ppSet[index]->setName(name);

    string beginToken = "DIRECTIVE BEGIN";
    string endToken = "DIRECTIVE END";
    uint start = 0;
    uint end = 0;

    while (end < text.size() - 1) {
        ////////////////////////////////////////////////////////////
        // ex)
        // DIRECTIVE BEGIN
        //   (CONDITION:ImDying)
        //   (ACTION:Flee)
        // DIRECTIVE END
        // DIRECTIVE BEGIN
        //   (CONDITION:EnemyRangeMelee)
        //   (CONDITION:EnemyDying)
        //   (CONDITION:EnemyNotBloodDrained)
        //   (ACTION:UseSkill,BloodDrain,30)
        // DIRECTIVE END
        ////////////////////////////////////////////////////////////

        start = text.find(beginToken, end);
        end = text.find(endToken, start + 1);

        if (start > end)
            break;

        Directive* pDirective = new Directive;
        string directive = trim(text.substr(start + beginToken.size() + 1, end - start - beginToken.size() - 1));

        Assert(pDirective != NULL);

        size_t i = 0, j = 0, k = 0;

        while (k < directive.size()) {
            // Read one of the actions or conditions that
            // make up the Directive.
            i = directive.find_first_of('(', k);
            j = directive.find_first_of(':', i + 1);
            k = directive.find_first_of(')', j + 1);

            // Stop parsing when the end of the string is passed.
            if (i > j || j > k)
                break;

            // identifier says whether it is an action or a condition.
            // parameter is the argument that follows it.
            string identifier = directive.substr(i + 1, j - i - 1);
            string parameter = directive.substr(j + 1, k - j - 1);

            if (identifier == "CONDITION") {
                parseDirectiveParameter(pDirective, parameter, MODE_CONDITION);
            } else if (identifier == "ACTION") {
                parseDirectiveParameter(pDirective, parameter, MODE_ACTION);
            } else {
                cerr << "DirectiveSetManager::createDirectiveSet() : unknown IDENTIFIER" << endl;
                throw Error("DirectiveSetManager::createDirectiveSet() : unknown IDENTIFIER");
            }
        }

        m_ppSet[index]->addDirective(pDirective);
    }

    // Read the behaviour performed on death. The number 5 has no special meaning.
    // With no action at all the length is usually 0, but once an action is present
    // it is expected to be roughly 50. The check exists so that nothing is read
    // when there is no action.
    if (deadtext.size() > 5) {
        start = 0;
        end = 0;

        while (end < deadtext.size() - 1) {
            ////////////////////////////////////////////////////////////
            // ex)
            // DIRECTIVE BEGIN
            //   (CONDITION:ImDying)
            //   (ACTION:Flee)
            // DIRECTIVE END
            // DIRECTIVE BEGIN
            //   (CONDITION:EnemyRangeMelee)
            //   (CONDITION:EnemyDying)
            //   (CONDITION:EnemyNotBloodDrained)
            //   (ACTION:UseSkill,BloodDrain,30)
            // DIRECTIVE END
            ////////////////////////////////////////////////////////////

            start = deadtext.find(beginToken, end);
            end = deadtext.find(endToken, start + 1);

            if (start > end)
                break;

            Directive* pDirective = new Directive;
            string directive =
                trim(deadtext.substr(start + beginToken.size() + 1, end - start - beginToken.size() - 1));

            Assert(pDirective != NULL);

            size_t i = 0, j = 0, k = 0;

            while (k < directive.size()) {
                // Read one of the actions or conditions that
                // make up the Directive.
                i = directive.find_first_of('(', k);
                j = directive.find_first_of(':', i + 1);
                k = directive.find_first_of(')', j + 1);

                // Stop parsing when the end of the string is passed.
                if (i > j || j > k)
                    break;

                // identifier says whether it is an action or a condition.
                // parameter is the argument that follows it.
                string identifier = directive.substr(i + 1, j - i - 1);
                string parameter = directive.substr(j + 1, k - j - 1);

                if (identifier == "CONDITION") {
                    parseDirectiveParameter(pDirective, parameter, MODE_CONDITION);
                } else if (identifier == "ACTION") {
                    parseDirectiveParameter(pDirective, parameter, MODE_ACTION);
                } else {
                    cerr << "DirectiveSetManager::createDirectiveSet() : unknown IDENTIFIER" << endl;
                    throw Error("DirectiveSetManager::createDirectiveSet() : unknown IDENTIFIER");
                }
            }

            m_ppSet[index]->addDeadDirective(pDirective);
        }
    }

    m_ppSet[index]->setAttackAir(m_ppSet[index]->hasCondition(DIRECTIVE_COND_ENEMY_ON_AIR));
    m_ppSet[index]->setSeeSafeZone(m_ppSet[index]->hasCondition(DIRECTIVE_COND_ENEMY_ON_SAFE_ZONE));

    if (m_ppSet[index]->canSeeSafeZone())
        cout << index << " : can see safe zone" << endl;
}

////////////////////////////////////////////////////////////////////////
// parseDirectiveParameter()
// Parses the string, builds a condition or an action and sets it on the Directive.
////////////////////////////////////////////////////////////////////////
void DirectiveSetManager::parseDirectiveParameter(Directive* pDirective, const string& text, int mode)

{
    size_t oldpos = 0;
    size_t pos = 0;
    size_t paramCount = 0;

    while (oldpos < text.size() && pos < text.size()) {
        // Consecutive parameters are separated by ",".
        oldpos = pos;
        pos = text.find_first_of(',', oldpos);

        // A condition has only one parameter, so searching for ","
        // always yields the max value. That cannot be used to call the
        // substring function properly, so pos is set to the length of
        // the string.
        if (pos == string::npos)
            pos = text.size();
        pos++;

        string token = text.substr(oldpos, (pos - 1) - oldpos);

        switch (mode) {
        case MODE_CONDITION:
            if (paramCount != 0) {
                cerr << "DirectiveSetManager::parseParameter() : There's two or more parameters in condition" << endl;
                cerr << "ParsingText: " << text.c_str() << endl;
                throw Error("DirectiveSetManager::parseParameter() : There's two or more parameters in condition");
            }
            pDirective->addCondition(getCondition(token));
            break;
        case MODE_ACTION:
            if (paramCount == 0)
                pDirective->setAction(getAction(token));
            else if (paramCount == 1) {
                switch (pDirective->getAction()) {
                case DIRECTIVE_ACTION_USE_SKILL:
                    pDirective->setParameter(getParameter(token));
                    break;

                case DIRECTIVE_ACTION_CHANGE_ENEMY:
                case DIRECTIVE_ACTION_MOVE_RANDOM:
                case DIRECTIVE_ACTION_SAY:
                    pDirective->setParameter(atoi(token.c_str()));
                    break;


                default:
                    cerr << "DirectiveSetManager::parseParameter() : There's two or more parameters in action" << endl;
                    cerr << "ParsingText: " << text.c_str() << endl;
                    throw Error("No Need Parameter!");
                }

                ////////////////
                // if (pDirective->getAction() == DIRECTIVE_ACTION_USE_SKILL)
                //{
                //	filelog("Directive.txt", "%s", token.c_str());
                // }
                ////////////////
            } else if (paramCount == 2)
                pDirective->setRatio(getRatio(token));
            break;
        default:
            break;
        }
        paramCount++;
    }
}

////////////////////////////////////////////////////////////////////////
// getCondition()
// Takes a string and returns the matching condition constant.
////////////////////////////////////////////////////////////////////////
int DirectiveSetManager::getCondition(const string& token)

{
    for (int i = 0; i < DIRECTIVE_COND_MAX; i++)
        if (DirectiveCondition2String[i] == token)
            return i;

    StringStream msg;
    msg << "DirectiveSetManager::getCondition() : Invalid condition string[" << token << "]";

    cerr << msg.toString() << endl;
    throw msg.toString();
    return DIRECTIVE_COND_MAX;
}

////////////////////////////////////////////////////////////////////////
// getAction()
// Takes a string and returns the matching action constant.
////////////////////////////////////////////////////////////////////////
int DirectiveSetManager::getAction(const string& token)

{
    for (int i = 0; i < DIRECTIVE_ACTION_MAX; i++)
        if (DirectiveAction2String[i] == token)
            return i;

    cerr << "DirectiveSetManager::getAction() : Invalid action[" << token << "]" << endl;
    throw Error("DirectiveSetManager::getAction() : Invalid action");
    return DIRECTIVE_COND_MAX;
}

////////////////////////////////////////////////////////////////////////
// getParameter()
// Takes a string and returns the matching parameter.
// For now that is only the skill type a monster uses.
////////////////////////////////////////////////////////////////////////
int DirectiveSetManager::getParameter(const string& token)

{
    for (int i = 0; i < SKILL_MAX; i++) {
        if (strcasecmp(token.c_str(), SkillTypes2String[i].c_str()) == 0)
            return i;
    }

    cerr << "DirectiveSetManager::getParameter() : Invalid parameter[" << token << "]" << endl;
    throw Error("DirectiveSetManager::getParameter() : Invalid parameter");

    return 0;
}

////////////////////////////////////////////////////////////////////////
// getRatio()
// Takes a string and returns the matching probability value, as a percentage.
////////////////////////////////////////////////////////////////////////
int DirectiveSetManager::getRatio(const string& token)

{
    int rValue = atoi(token.c_str());

    if (rValue < 0 || rValue > 100) {
        cerr << "DirectiveSetManager::getRatio() : Invalid ratio value" << endl;
        throw Error("DirectiveSetManager::getRatio() : Invalid ratio value");
    }

    return rValue;
}
