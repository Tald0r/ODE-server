////////////////////////////////////////////////////////////////////////////////
// Filename    : ConditionEnterCastleDungeon.cpp
// Written By  :
// Description :
////////////////////////////////////////////////////////////////////////////////

#include "ConditionEnterCastleDungeon.h"

#include "CastleInfoManager.h"
#include "FlagSet.h"
#include "GameContext.h"
#include "GamePlayer.h"
#include "PacketUtil.h"
#include "PaySystem.h"
#include "PlayerCreature.h"
#include "WarSystem.h"
#include "Zone.h"

////////////////////////////////////////////////////////////////////////////////
// is satisfied?
////////////////////////////////////////////////////////////////////////////////
bool ConditionEnterCastleDungeon::isSatisfied(Creature* pCreature1, Creature* pCreature2, void* pParam) const

{
    Assert(pCreature2 != NULL);
    Assert(pCreature2->isPC());


    bool bPayPlay = false;

    GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(pCreature2->getPlayer());
    Assert(pGamePlayer != NULL);

    bPayPlay = true;

    // The pay-to-play gate is always open: bPayPlay is set true just above.
    if (bPayPlay) {
        bool hasGuildWar = de::gameContext().warSystem().hasCastleActiveWar(m_CastleZoneID);

        CastleInfo* pCastleInfo = de::gameContext().castleInfos().getCastleInfo(m_CastleZoneID);
        Assert(pCastleInfo != NULL);

        PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pCreature2);
        Assert(pPC != NULL);

        GuildID_t GuildID = pPC->getGuildID();
        GuildID_t OwnerGuildID = pCastleInfo->getGuildID();

        // No war --> only the guild that owns the castle can enter.
        // Guild war --> the attacking guild can enter as well.
        if (OwnerGuildID != SlayerCommon && OwnerGuildID != VampireCommon && OwnerGuildID != OustersCommon &&
            GuildID == OwnerGuildID) {
            return true;
        }

        if (hasGuildWar) {
            GuildID_t AttackGuildID;
            de::gameContext().warSystem().getAttackGuildID(m_CastleZoneID, AttackGuildID);
            if (GuildID == AttackGuildID) {
                return true;
            }
        }
    }

    return false;
}

////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////
void ConditionEnterCastleDungeon::read(PropertyBuffer& propertyBuffer)

{
    try {
        // read turn
        m_CastleZoneID = propertyBuffer.getPropertyInt("CastleZoneID");
    } catch (NoSuchElementException& nsee) {
        throw Error(nsee.toString());
    }
}

////////////////////////////////////////////////////////////////////////////////
// get debug string
////////////////////////////////////////////////////////////////////////////////
string ConditionEnterCastleDungeon::toString() const

{
    StringStream msg;
    msg << "ConditionEnterCastleDungeon(" << "CastleZoneID:" << (int)m_CastleZoneID << ")";
    return msg.toString();
}
