//////////////////////////////////////////////////////////////////////////////
// Filename    : EffectHasCastleSymbol.cpp
// Written by  : elca
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "EffectHasCastleSymbol.h"

#include <stdio.h>

#include "CastleInfoManager.h"
#include "CastleSymbol.h"
#include "Creature.h"
#include "GCAddEffect.h"
#include "GCModifyInformation.h"
#include "GCRemoveEffect.h"
#include "GCStatusCurrentHP.h"
#include "GCSystemMessage.h"
#include "GameContext.h"
#include "GuildManager.h"
#include "ItemInfoManager.h"
#include "Monster.h"
#include "MonsterCorpse.h"
#include "Player.h"
#include "PlayerCreature.h"
#include "Slayer.h"
#include "StringPool.h"
#include "Vampire.h"
#include "WarSystem.h"
#include "ZoneGroupManager.h"
#include "ZoneInfoManager.h"
#include "ZoneUtil.h"

const Effect::EffectClass EffectHasCastleSymbol::EffectClasses[6] = {
    Effect::EFFECT_CLASS_HAS_CASTLE_SYMBOL,   Effect::EFFECT_CLASS_HAS_CASTLE_SYMBOL_2,
    Effect::EFFECT_CLASS_HAS_CASTLE_SYMBOL_3, Effect::EFFECT_CLASS_HAS_CASTLE_SYMBOL_4,
    Effect::EFFECT_CLASS_HAS_CASTLE_SYMBOL_5, Effect::EFFECT_CLASS_HAS_CASTLE_SYMBOL_6,
};

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
EffectHasCastleSymbol::EffectHasCastleSymbol(Creature* pCreature)

    : EffectHasRelic(pCreature){__BEGIN_TRY

                                    __END_CATCH}

      //////////////////////////////////////////////////////////////////////////////
      //////////////////////////////////////////////////////////////////////////////
      EffectHasCastleSymbol::EffectHasCastleSymbol(Item * pItem)

    : EffectHasRelic(pItem) {
    __BEGIN_TRY

    __END_CATCH
}
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectHasCastleSymbol::affect(Creature* pCreature)

{
    __BEGIN_TRY

    // Get the zone.
    Zone* pZone = pCreature->getZone();
    Assert(pZone != NULL);

    ZoneInfo* pZoneInfo = de::gameContext().zoneInfos().getZoneInfo(pZone->getZoneID());
    Assert(pZoneInfo != NULL);

    PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pCreature);
    Assert(pPC != NULL);

    // Announce the location.

    char msg[300];
    snprintf(msg, sizeof(msg), de::gameContext().strings().c_str(STRID_BROADCAST_CASTLE_SYMBOL_POSITION),
             pCreature->getName().c_str(), de::gameContext().guilds().getGuildName(pPC->getGuildID()).c_str(),
             pZoneInfo->getFullName().c_str(), (int)pCreature->getX(), (int)pCreature->getY(), m_PartName.c_str());

    GCSystemMessage gcSystemMessage;
    gcSystemMessage.setMessage(msg);


    de::gameContext().castleInfos().broadcastShrinePacket(m_Part, &gcSystemMessage);
    //	g_pZoneGroupManager->broadcast( &gcSystemMessage );

    setNextTime(m_Tick);

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectHasCastleSymbol::affect(Item* pItem)

{
    __BEGIN_TRY

    if (m_pZone == NULL)
        return;

    // Send the message only while a war is running at the castle.
    ZoneID_t castleZoneID;
    bool isCastle;

    isCastle = de::gameContext().castleInfos().getCastleZoneID(m_pZone->getZoneID(), castleZoneID);

    if (isCastle && de::gameContext().warSystem().hasCastleActiveWar(castleZoneID)) {
        ZoneInfo* pZoneInfo = de::gameContext().zoneInfos().getZoneInfo(m_pZone->getZoneID());
        Assert(pZoneInfo != NULL);

        // Announce the location.

        char msg[200];
        snprintf(msg, sizeof(msg), de::gameContext().strings().c_str(STRID_BROADCAST_CASTLE_SYMBOL_POSITION_2),
                 pZoneInfo->getFullName().c_str(), (int)m_X, (int)m_Y, m_PartName.c_str());

        GCSystemMessage gcSystemMessage;
        gcSystemMessage.setMessage(msg);

        de::gameContext().castleInfos().broadcastShrinePacket(m_Part, &gcSystemMessage);
        //		g_pZoneGroupManager->broadcast( &gcSystemMessage );
    }

    setNextTime(m_Tick);

    __END_CATCH
}

void EffectHasCastleSymbol::setPart(int part)

{
    __BEGIN_TRY

    const CastleSymbolInfo* pCastleSymbolInfo = dynamic_cast<const CastleSymbolInfo*>(
        de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_CASTLE_SYMBOL, part));

    if (pCastleSymbolInfo != NULL) {
        m_Part = part;
        m_PartName = pCastleSymbolInfo->getName();
    }

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
string EffectHasCastleSymbol::toString() const {
    __BEGIN_TRY

    StringStream msg;
    msg << "EffectHasCastleSymbol(" << "ObjectID:" << getObjectID() << ")";
    return msg.toString();

    __END_CATCH
}
