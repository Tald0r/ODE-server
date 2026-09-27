//////////////////////////////////////////////////////////////////////////////
// Filename     : CombatInfoManager.cpp
// Written by   : bezz
// Description  :
//////////////////////////////////////////////////////////////////////////////

#include "CombatInfoManager.h"

#include "GCSystemMessage.h"
#include "GameContext.h"
#include "StringPool.h"
#include "ZoneGroupManager.h"

//////////////////////////////////////////////////////////////////////////////
// class CombatInfoManager member methods
//////////////////////////////////////////////////////////////////////////////

CombatInfoManager::CombatInfoManager() {
    __BEGIN_TRY

    for (int i = 0; i < maxRelic; i++) {
        m_RelicOwner[i] = RELIC_OWNER_NULL;
    }

    m_SlayerHPModify = 0;
    m_SlayerToHitModify = 0;
    m_VampireHPModify = 0;
    m_VampireToHitModify = 0;

    m_bSlayerBonus = false;
    m_bVampireBonus = false;

    m_bCombat = false;

    __END_CATCH
}

void CombatInfoManager::initModify() {
    __BEGIN_TRY

    // computeModify();

    __END_CATCH
}

void CombatInfoManager::computeModify() {
    __BEGIN_TRY

    StringPool& strings = de::gameContext().strings();

    m_SlayerHPModify = 0;
    m_SlayerToHitModify = 0;
    m_VampireHPModify = 0;
    m_VampireToHitModify = 0;

    for (int i = 0; i < maxRelic; i++) {
        // Compute the bonus and penalty.
    }

    GCSystemMessage gcSystemMessage;

    bool bSlayerBonus = m_RelicOwner[0] == RELIC_OWNER_SLAYER && m_RelicOwner[1] == RELIC_OWNER_SLAYER;

    bool bVampireBonus = m_RelicOwner[0] == RELIC_OWNER_VAMPIRE && m_RelicOwner[1] == RELIC_OWNER_VAMPIRE;

    // Slayer victory
    if (bSlayerBonus) {
        // m_SlayerHPModify = g_pVariableManager->getCombatSlayerHPBonusRatio();
        //		gcSystemMessage.setMessage("What bonus should the Slayers get?");
        gcSystemMessage.setMessage(strings.getString(STRID_APPLICATE_COMBAT_SLAYER_BONUS));
    }
    // Vampire victory
    else if (bVampireBonus) {
        // m_VampireHPModify = g_pVariableManager->getCombatVampireHPBonusRatio();
        //		gcSystemMessage.setMessage("What bonus should the Vampires get?");
        gcSystemMessage.setMessage(strings.getString(STRID_APPLICATE_COMBAT_VAMPIRE_BONUS));
    }

    else if (!bSlayerBonus && !bVampireBonus) {
        // Was being applied and no longer is.
        if (m_bSlayerBonus && m_bVampireBonus) {
            gcSystemMessage.setMessage(strings.getString(STRID_NO_MORE_COMBAT_BONUS));
        }
        // The Slayer bonus goes away.
        else if (m_bSlayerBonus) {
            gcSystemMessage.setMessage(strings.getString(STRID_NO_MORE_COMBAT_SLAYER_BONUS));
        }
        // The Vampire bonus goes away.
        else if (m_bVampireBonus) {
            gcSystemMessage.setMessage(strings.getString(STRID_NO_MORE_COMBAT_VAMPIRE_BONUS));
        }
    }

    //	if (bSendMessage)
    //		g_pZoneGroupManager->broadcast(&gcSystemMessage);

    m_bSlayerBonus = bSlayerBonus;
    m_bVampireBonus = bVampireBonus;

    __END_CATCH
}

void CombatInfoManager::setRelicOwner(int index, RelicOwner relicOwner) {
    __BEGIN_TRY

    if (index < 0 || index >= maxRelic) {
        throw OutOfBoundException();
    }

    m_RelicOwner[index] = relicOwner;

    // computeModify();

    __END_CATCH
}
