////////////////////////////////////////////////////////////////////////////////
// Filename    : ConditionCanEnterQuestZone.cpp
// Written By  :
// Description :
////////////////////////////////////////////////////////////////////////////////

#include "ConditionCanEnterQuestZone.h"

#include "DynamicZoneGroup.h"
#include "DynamicZoneInfo.h"
#include "DynamicZoneManager.h"
#include "GQuestManager.h"
#include "GameContext.h"
#include "PlayerCreature.h"


////////////////////////////////////////////////////////////////////////////////
// is satisfied?
////////////////////////////////////////////////////////////////////////////////
bool ConditionCanEnterQuestZone::isSatisfied(Creature* pCreature1, Creature* pCreature2, void* pParam) const

{
    Assert(pCreature2 != NULL);
    Assert(pCreature2->isPC());

    PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pCreature2);

    bool bQuestCondition = pPC->getGQuestManager()->canEnterDynamicZone(m_QuestZoneID);
    bool bDynamicZoneAvailable = true;

    // Check whether it is a dynamic zone.
    int targetDynamicZoneType = de::gameContext().dynamicZoneInfos().getDynamicZoneTypeByZoneID(m_QuestZoneID);

    if (targetDynamicZoneType != DYNAMIC_ZONE_MAX) {
        DynamicZoneGroup* pDynamicZoneGroup =
            de::gameContext().dynamicZones().getDynamicZoneGroup(targetDynamicZoneType);
        Assert(pDynamicZoneGroup != NULL);

        bDynamicZoneAvailable = pDynamicZoneGroup->canEnter();
    }

    cout << "QuestZoneID: " << m_QuestZoneID << endl;
    cout << "Quest: " << (bQuestCondition ? "true" : "false") << " Zone: " << (bDynamicZoneAvailable ? "true" : "false")
         << endl;

    return bQuestCondition && bDynamicZoneAvailable;
}

////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////
void ConditionCanEnterQuestZone::read(PropertyBuffer& propertyBuffer)

{
    m_QuestZoneID = propertyBuffer.getPropertyInt("ZoneID");
}

////////////////////////////////////////////////////////////////////////////////
// get debug string
////////////////////////////////////////////////////////////////////////////////
string ConditionCanEnterQuestZone::toString() const

{
    StringStream msg;
    msg << "ConditionCanEnterQuestZone(" << "ZoneID:" << (int)m_QuestZoneID << ")";
    return msg.toString();
}
