//////////////////////////////////////////////////////////////////////////////
// Filename    : EffectRelicPosition.cpp
// Written by  :
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "EffectRelicPosition.h"

#include <stdio.h>

#include "CastleInfoManager.h"
#include "GCBloodBibleStatus.h"
#include "GCSystemMessage.h"
#include "GameContext.h"
#include "HolyLandManager.h"
#include "ItemInfoManager.h"
#include "Relic.h"
#include "ShrineInfoManager.h"
#include "StringPool.h"
#include "StringStream.h"
#include "ZoneGroupManager.h"
#include "ZoneInfoManager.h"
#include "ZoneUtil.h"

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
EffectRelicPosition::EffectRelicPosition(Item* pItem)

{
    __BEGIN_TRY

    setTarget(pItem);

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectRelicPosition::affect()

{
    __BEGIN_TRY

    Item* pItem = dynamic_cast<Item*>(m_pTarget);
    affect(pItem);

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectRelicPosition::affect(Item* pItem)

{
    __BEGIN_TRY

    // Get the zone info.
    ZoneInfo* pZoneInfo = de::gameContext().zoneInfos().getZoneInfo(m_ZoneID);
    Assert(pZoneInfo != NULL);

    if (pItem->getItemClass() == Item::ITEM_CLASS_RELIC) {
        // Get the relic info.
        ItemType_t relicIndex = pItem->getItemType();
        const RelicInfo* pRelicInfo =
            dynamic_cast<RelicInfo*>(de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_RELIC, relicIndex));
        Assert(pRelicInfo != NULL);

        //		StringStream msg;

        char msg[100];

        snprintf(msg, sizeof(msg), de::gameContext().strings().c_str(STRID_RELIC_IN_ZONE),
                 pRelicInfo->getName().c_str(), pZoneInfo->getFullName().c_str(), (int)m_X, (int)m_Y);

        GCSystemMessage gcSystemMessage;
        gcSystemMessage.setType(SYSTEM_MESSAGE_COMBAT);
        gcSystemMessage.setMessage(msg);

        de::gameContext().holyLands().broadcast(&gcSystemMessage);

        setNextTime(m_Tick);
    } else if (pItem->getItemClass() == Item::ITEM_CLASS_BLOOD_BIBLE) {
        ShrineSet* pShrineSet = de::gameContext().shrines().getShrineSet(m_Part);
        Assert(pShrineSet != NULL);

        GCBloodBibleStatus* pGCBBS = new GCBloodBibleStatus;
        pGCBBS->setItemType(m_Part);
        pGCBBS->setZoneID(m_ZoneID);
        pGCBBS->setStorage(STORAGE_ZONE);
        // pGCBBS->setRace( pShrineSet->getOwnerRace() );
        pGCBBS->setShrineRace(pShrineSet->getOwnerRace());
        pGCBBS->setX(m_X);
        pGCBBS->setY(m_Y);

        //		g_pHolyLandManager->broadcast( pGCBBS );
        de::gameContext().zoneGroups().broadcast(pGCBBS);
        de::gameContext().shrines().registerBloodBibleStatus(m_Part, pGCBBS);

        setNextTime(999999);
    } else if (pItem->getItemClass() == Item::ITEM_CLASS_CASTLE_SYMBOL) {
        //		StringStream msg;

        //		msg << "The castle symbol is lying at "
        //			<< pZoneInfo->getFullName() << "(" << (int)m_X << ", " << (int)m_Y << ").";

        char msg[200];
        snprintf(msg, sizeof(msg), de::gameContext().strings().c_str(STRID_BROADCAST_CASTLE_SYMBOL_POSITION_3),
                 pZoneInfo->getFullName().c_str(), (int)m_X, (int)m_Y);

        GCSystemMessage gcSystemMessage;
        gcSystemMessage.setType(SYSTEM_MESSAGE_HOLY_LAND);
        gcSystemMessage.setMessage(msg);

        de::gameContext().castleInfos().broadcastShrinePacket(m_Part, &gcSystemMessage);

        setNextTime(m_Tick);
        //		g_pZoneGroupManager->broadcast( &gcSystemMessage );
    } else {
        return;
    }


    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectRelicPosition::unaffect()

{
    __BEGIN_TRY

    Item* pItem = dynamic_cast<Item*>(m_pTarget);
    unaffect(pItem);

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectRelicPosition::unaffect(Item* pItem)

{
    __BEGIN_TRY

    Assert(pItem != NULL);

    pItem->removeFlag(Effect::EFFECT_CLASS_RELIC_POSITION);

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
string EffectRelicPosition::toString() const {
    __BEGIN_TRY

    StringStream msg;
    msg << "EffectRelicPosition(" << "ObjectID:" << getObjectID() << ")";
    return msg.toString();

    __END_CATCH
}
