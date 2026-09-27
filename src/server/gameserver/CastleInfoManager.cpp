////////////////////////////////////////////////////////////////////////////////
// Filename    : CastleInfoManager.cpp
// Written By  :
// Description : Information about the castles near Adam's sanctuary
////////////////////////////////////////////////////////////////////////////////

#include "CastleInfoManager.h"

#include <stdio.h>

#include <algorithm>

#include "Guild.h"
#include "GuildManager.h"
#include "ItemUtil.h"
#include "PlayerCreature.h"
#include "Properties.h"
#include "VariableManager.h"
#include "ZoneGroupManager.h"
#include "ZoneInfoManager.h"
// #include "HolyLandRaceBonus.h"
#include "CastleOwnerDecision.h"
#include "CastleSkillInfo.h"
#include "ClientManager.h"
#include "EffectHasBloodBible.h"
#include "EventRefreshHolyLandPlayer.h"
#include "GCModifyInformation.h"
#include "GCNoticeEvent.h"
#include "GCSystemMessage.h"
#include "GameContext.h"
#include "GamePlayer.h"
#include "KernelContext.h"
#include "NPC.h"
#include "NPCManager.h"
#include "PCManager.h"
#include "ShrineInfoManager.h"
#include "Skill.h"
#include "StringPool.h"
#include "War.h"
#include "WarScheduler.h"
#include "WarSystem.h"
#include "WarZoneWork.h"
#include "Zone.h"
#include "ZoneGroup.h"
#include "ZoneUtil.h"
#include "repository/WarInfoRepository.h"

CastleInfo::CastleInfo() : m_Name(""), m_BonusOptionList(), m_CastleZoneIDList() {
    m_ZoneID = 0;
    m_ShrineID = 0;
    m_GuildID = 0;
    m_ItemTaxRatio = 0;
    m_EntranceFee = 0;
    m_TaxBalance = 0;
}

CastleInfo::~CastleInfo() {}

void CastleInfo::setOptionTypeList(const string& options)

{
    __BEGIN_TRY

    makeOptionList(options, m_BonusOptionList);

    __END_CATCH
}

void CastleInfo::setZoneIDList(const string& zoneIDs)

{
    __BEGIN_TRY

    makeZoneIDList(zoneIDs, m_CastleZoneIDList);

    __END_CATCH
}

void CastleInfo::setResurrectPosition(ResurrectPriority resurrectPriority, const ZONE_COORD& zoneCoord) {
    m_ResurrectPosition[resurrectPriority].set(zoneCoord.id, zoneCoord.x, zoneCoord.y);
}

void CastleInfo::getResurrectPosition(ResurrectPriority resurrectPriority, ZONE_COORD& zoneCoord) {
    zoneCoord.set(m_ResurrectPosition[resurrectPriority].id, m_ResurrectPosition[resurrectPriority].x,
                  m_ResurrectPosition[resurrectPriority].y);
}

void CastleInfo::broadcast(Packet* pPacket) const {
    Assert(pPacket != NULL);

    de::war::postBroadcast(vector<ZoneID_t>(m_CastleZoneIDList.begin(), m_CastleZoneIDList.end()), *pPacket);
}

bool CastleInfo::isCastleZone(ZoneID_t targetZoneID) const

{
    __BEGIN_TRY

    list<ZoneID_t>::const_iterator itr = m_CastleZoneIDList.begin();

    for (; itr != m_CastleZoneIDList.end(); itr++) {
        ZoneID_t zoneID = *itr;

        if (zoneID == targetZoneID)
            return true;
    }

    return false;

    __END_CATCH
}


string CastleInfo::toString() const

{
    __BEGIN_TRY

    StringStream msg;
    msg << "CastleInfo(" << "ZoneID:" << m_ZoneID << ",Item Tax Ratie:" << m_ItemTaxRatio.load()
        << ",Entrance Fee:" << m_EntranceFee.load() << ",Tax Balance:" << m_TaxBalance.load() << ")";
    return msg.toString();

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
// constructor
//////////////////////////////////////////////////////////////////////////////
CastleInfoManager::CastleInfoManager(){__BEGIN_TRY __END_CATCH}


//////////////////////////////////////////////////////////////////////////////
// destructor
//////////////////////////////////////////////////////////////////////////////
CastleInfoManager::~CastleInfoManager() {
    __BEGIN_TRY

    unordered_map<ZoneID_t, CastleInfo*>::iterator itr = m_CastleInfos.begin();
    for (; itr != m_CastleInfos.end(); itr++) {
        CastleInfo* pInfo = itr->second;
        SAFE_DELETE(pInfo);
    }

    // Remove all pairs from the hashmap.
    m_CastleInfos.clear();

    __END_CATCH_NO_RETHROW
}


//////////////////////////////////////////////////////////////////////////////
// initialize zone info manager
//////////////////////////////////////////////////////////////////////////////
void CastleInfoManager::init()

{
    __BEGIN_TRY

    // init == load
    load();

    __END_CATCH
}


// void testMaxMemory();

//////////////////////////////////////////////////////////////////////////////
// load from database
//////////////////////////////////////////////////////////////////////////////
void CastleInfoManager::load()

{
    __BEGIN_TRY

    clearCastleZoneIDs();

    vector<CastleRow> rows =
        defaultWarInfoRepository().loadCastles(de::kernelContext().config().getPropertyInt("ServerID"));

    ZoneCoord_t x, y;
    ZONE_COORD zoneCoord;

    for (size_t r = 0; r < rows.size(); r++) {
        const CastleRow& row = rows[r];

        ZoneID_t zoneID = row.zoneID;

        ZoneInfo* pZoneInfo = de::gameContext().zoneInfos().getZoneInfo(zoneID);
        Assert(pZoneInfo != NULL);

        pZoneInfo->setCastle();

        CastleInfo* pCastleInfo = new CastleInfo();
        // cout << "new OK" << endl;

        pCastleInfo->setZoneID(zoneID);
        pCastleInfo->setShrineID(row.shrineID);
        pCastleInfo->setGuildID(row.guildID);
        pCastleInfo->setName(row.name);
        pCastleInfo->setRace(row.race);
        pCastleInfo->setItemTaxRatio(row.itemTaxRatio);
        pCastleInfo->setEntranceFee(row.entranceFee);
        // Relative saves that never landed (see increaseTaxBalance) can leave
        // the row outside the balance's range; the balance starts inside it.
        pCastleInfo->setTaxBalance((Gold_t)std::clamp<int64_t>(row.taxBalance, 0, GUILD_TAX_BALANCE_MAX));
        pCastleInfo->setOptionTypeList(row.bonusOptionType);

        zoneID = row.firstResurrectZoneID;
        x = row.firstResurrectX;
        y = row.firstResurrectY;

        zoneCoord.set(zoneID, x, y);
        pCastleInfo->setResurrectPosition(CastleInfo::CASTLE_RESURRECT_PRIORITY_FIRST, zoneCoord);

        zoneID = row.secondResurrectZoneID;
        x = row.secondResurrectX;
        y = row.secondResurrectY;

        zoneCoord.set(zoneID, x, y);
        pCastleInfo->setResurrectPosition(CastleInfo::CASTLE_RESURRECT_PRIORITY_SECOND, zoneCoord);

        zoneID = row.thirdResurrectZoneID;
        x = row.thirdResurrectX;
        y = row.thirdResurrectY;

        zoneCoord.set(zoneID, x, y);
        pCastleInfo->setResurrectPosition(CastleInfo::CASTLE_RESURRECT_PRIORITY_THIRD, zoneCoord);

        pCastleInfo->setZoneIDList(row.zoneIDList);

        addCastleInfo(pCastleInfo);

        cout << pCastleInfo->toString().c_str() << endl;
    }

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
// add zone info to zone info manager
//////////////////////////////////////////////////////////////////////////////
void CastleInfoManager::addCastleInfo(CastleInfo* pCastleInfo)

{
    __BEGIN_TRY

    // First check if a zone with the same ID already exists.
    unordered_map<ZoneID_t, CastleInfo*>::iterator itr = m_CastleInfos.find(pCastleInfo->getZoneID());

    if (itr != m_CastleInfos.end())
        // Duplicate zone ID.
        throw Error("duplicated zone id");

    m_CastleInfos[pCastleInfo->getZoneID()] = pCastleInfo;

    // Register every zone ID that belongs to this castle.
    const list<ZoneID_t>& zoneIDs = pCastleInfo->getZoneIDList();
    list<ZoneID_t>::const_iterator iZoneID = zoneIDs.begin();

    for (; iZoneID != zoneIDs.end(); iZoneID++) {
        ZoneID_t zoneID = *iZoneID;
        setCastleZoneID(zoneID, pCastleInfo->getZoneID());
    }

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////////////
// Delete zone info from zone info manager
//////////////////////////////////////////////////////////////////////////////
void CastleInfoManager::deleteCastleInfo(ZoneID_t zoneID)

{
    __BEGIN_TRY

    unordered_map<ZoneID_t, CastleInfo*>::iterator itr = m_CastleInfos.find(zoneID);

    if (itr != m_CastleInfos.end()) {
        // Delete the zone.
        SAFE_DELETE(itr->second);

        // Remove the pair entry.
        m_CastleInfos.erase(itr);
    } else {
        // If no such zone ID is found
        return;
    }

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
// get zone from zone info manager
//////////////////////////////////////////////////////////////////////////////
CastleInfo* CastleInfoManager::getCastleInfo(ZoneID_t zoneID) const

{
    __BEGIN_TRY

    CastleInfo* pCastleInfo = NULL;

    unordered_map<ZoneID_t, CastleInfo*>::const_iterator itr = m_CastleInfos.find(zoneID);

    if (itr != m_CastleInfos.end()) {
        pCastleInfo = itr->second;

    } else {
        // If no such zone ID exists

        return NULL;
    }

    return pCastleInfo;

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
// get debug string
//////////////////////////////////////////////////////////////////////////////
string CastleInfoManager::toString() const

{
    __BEGIN_TRY

    StringStream msg;

    msg << "CastleInfoManager(";

    if (m_CastleInfos.empty())
        msg << "EMPTY";
    else {
        for (unordered_map<ZoneID_t, CastleInfo*>::const_iterator itr = m_CastleInfos.begin();
             itr != m_CastleInfos.end(); itr++) {
            msg << itr->second->toString();
        }
    }

    msg << ")";

    return msg.toString();

    __END_CATCH
}

bool CastleInfoManager::modifyCastleOwner(ZoneID_t zoneID, PlayerCreature* pPC)

{
    __BEGIN_TRY

    Assert(pPC != NULL);

    Race_t race = pPC->getRace();
    GuildID_t guildID = pPC->getGuildID();

    return modifyCastleOwner(zoneID, race, guildID);

    __END_CATCH
}

bool CastleInfoManager::modifyCastleOwner(ZoneID_t zoneID, Race_t race, GuildID_t guildID)

{
    __BEGIN_TRY

    StringPool& strings = de::gameContext().strings();
    VariableManager& variables = de::gameContext().variables();

    CastleInfo* pCastleInfo = getCastleInfo(zoneID);
    if (pCastleInfo == NULL)
        return false;

    Zone* pZone = getZoneByZoneID(zoneID);

    // The castle's own group owns its owner change (see the header).
    pZone->getZoneGroup()->assertOwned();

    Race_t oldRace = pCastleInfo->getRace();

    pCastleInfo->setGuildID(guildID);
    pCastleInfo->setRace(race);
    // The reset is saved as the debit of what it took, like any other change
    // (see increaseTaxBalance in the header): a credit another thread made
    // just before it is taken with it, one made just after stays, and the
    // row agrees whichever save lands first.
    TaxBalanceChange reset = pCastleInfo->takeTaxBalance();

    if (pCastleInfo->isCommon()) {
        pCastleInfo->setEntranceFee(variables.getVariable(COMMON_CASTLE_ENTRANCE_FEE));
        setItemTaxRatio(pZone, variables.getVariable(COMMON_CASTLE_ITEM_TAX_RATIO));
    } else {
        pCastleInfo->setEntranceFee(variables.getVariable(GUILD_CASTLE_ENTRANCE_FEE));
        setItemTaxRatio(pZone, variables.getVariable(GUILD_CASTLE_ITEM_TAX_RATIO));
    }

    CastleOwnerRecord record;
    record.guildID = (int)pCastleInfo->getGuildID();
    record.race = (int)pCastleInfo->getRace();
    record.itemTaxRatio = (int)pCastleInfo->getItemTaxRatio();
    record.entranceFee = (int)pCastleInfo->getEntranceFee();
    record.taxBalanceDelta = -(int64_t)reset.applied;

    if (defaultWarInfoRepository().saveCastleOwner(de::kernelContext().config().getPropertyInt("ServerID"), (int)zoneID,
                                                   record)) {
        //		StringStream msg;
        char msg[100];
        if (guildID == SlayerCommon) {
            // msg << pCastleInfo->getName() << " castle became a Slayer common castle.";
            snprintf(msg, sizeof(msg), strings.c_str(STRID_BECOME_SLAYER_COMMON_CASTLE),
                     pCastleInfo->getName().c_str());
        } else if (guildID == VampireCommon) {
            // msg << pCastleInfo->getName() << " castle became a Vampire common castle.";
            snprintf(msg, sizeof(msg), strings.c_str(STRID_BECOME_VAMPIRE_COMMON_CASTLE),
                     pCastleInfo->getName().c_str());
        } else if (guildID == OustersCommon) {
            snprintf(msg, sizeof(msg), "%s castle became an Ousters common castle.", pCastleInfo->getName().c_str());
        } else {
            Guild* pGuild = de::gameContext().guilds().getGuild(guildID);

            if (pGuild == NULL) {
                filelog("CastleError.log", "Unknown guildID : %d", (int)guildID);
            } else {
                if (pGuild->getRace() == Guild::GUILD_RACE_SLAYER) {
                    // msg << pGuild->getName() << " team conquered the castle.";
                    snprintf(msg, sizeof(msg), strings.c_str(STRID_BECOME_SLAYER_GUILD_CASTLE),
                             pGuild->getName().c_str(), pCastleInfo->getName().c_str());
                } else {
                    // msg << pGuild->getName() << " clan conquered the castle.";
                    snprintf(msg, sizeof(msg), strings.c_str(STRID_BECOME_VAMPIRE_GUILD_CASTLE),
                             pGuild->getName().c_str(), pCastleInfo->getName().c_str());
                }
            }

            //			msg << pCastleInfo->getName() << " castle was captured.";
        }

        // Handle when the owning race of the castle changes
        if (oldRace != race) {
            // Re-apply castle-related bonuses
            // g_pHolyLandRaceBonus->refresh();

            // [NPC reset] --> handled in War

            // Cancel every war schedule for this castle.
            WarScheduler* pWarScheduler = pZone->getWarScheduler();
            Assert(pWarScheduler != NULL);

            // Cancel all guild war schedules.
            pWarScheduler->cancelGuildSchedules();
        }

        filelog("WarLog.txt", "[CastleZoneID:%u]%s", (uint)pCastleInfo->getZoneID(), msg);

        // Apply the Holy Land Race Bonus to players immediately.
        EventRefreshHolyLandPlayer* pEvent = new EventRefreshHolyLandPlayer(NULL);
        pEvent->setDeadline(0);
        de::gameContext().clients().addEvent(pEvent);

        GCSystemMessage gcSystemMessage;
        gcSystemMessage.setType(SYSTEM_MESSAGE_HOLY_LAND);
        gcSystemMessage.setMessage(msg);
        de::gameContext().zoneGroups().broadcast(&gcSystemMessage);
    }

    return true;

    __END_CATCH
}

void CastleInfoManager::postCastleWarEnd(ZoneID_t castleZoneID, bool bChangeOwner, Race_t winnerRace,
                                         GuildID_t winnerGuildID, Gold_t registrationFee) {
    Zone* pZone = getZoneByZoneID(castleZoneID);
    Assert(pZone != NULL);

    pZone->getZoneGroup()->post([castleZoneID, bChangeOwner, winnerRace, winnerGuildID, registrationFee] {
        CastleInfoManager& castles = de::gameContext().castleInfos();

        if (bChangeOwner) {
            bool bWinnerExists =
                isCommonGuildID(winnerGuildID) || de::gameContext().guilds().getGuild(winnerGuildID) != NULL;
            CastleOwner owner = castleWarWinnerOwner(winnerRace, winnerGuildID, bWinnerExists);
            castles.modifyCastleOwner(castleZoneID, owner.race, owner.guildID);
        }

        // The fee goes to whoever holds the castle now; the owner change
        // above resets the balance, so the fee is credited after it.
        castles.increaseTaxBalance(castleZoneID, registrationFee);
    });
}

void CastleInfoManager::settleGuildDeletion(ZoneID_t castleZoneID, GuildID_t deletedGuildID) {
    const CastleInfo* pCastleInfo = getCastleInfo(castleZoneID);
    if (pCastleInfo == NULL)
        return;

    std::optional<CastleOwner> owner =
        castleOwnerAfterGuildDeleted(pCastleInfo->getRace(), pCastleInfo->getGuildID(), deletedGuildID);
    if (owner)
        modifyCastleOwner(castleZoneID, owner->race, owner->guildID);
}

TaxBalanceChange CastleInfoManager::increaseTaxBalance(ZoneID_t zoneID, Gold_t tax) {
    CastleInfo* pCastleInfo = getCastleInfo(zoneID);
    if (pCastleInfo == NULL)
        return TaxBalanceChange{0, 0};

    TaxBalanceChange change = pCastleInfo->increaseTaxBalance(tax);
    saveTaxBalanceChange(zoneID, (int64_t)change.applied);

    return change;
}

TaxBalanceChange CastleInfoManager::decreaseTaxBalance(ZoneID_t zoneID, Gold_t tax) {
    CastleInfo* pCastleInfo = getCastleInfo(zoneID);
    if (pCastleInfo == NULL)
        return TaxBalanceChange{0, 0};

    TaxBalanceChange change = pCastleInfo->decreaseTaxBalance(tax);
    saveTaxBalanceChange(zoneID, -(int64_t)change.applied);

    return change;
}

void CastleInfoManager::saveTaxBalanceChange(ZoneID_t zoneID, int64_t delta) {
    if (delta == 0)
        return;

    defaultWarInfoRepository().addCastleTaxBalance(de::kernelContext().config().getPropertyInt("ServerID"), (int)zoneID,
                                                   delta);
}

bool CastleInfoManager::setItemTaxRatio(Zone* pZone, int itemTaxRatio)

{
    __BEGIN_TRY

    Assert(pZone != NULL);
    CastleInfo* pCastleInfo = getCastleInfo(pZone->getZoneID());
    if (pCastleInfo == NULL)
        return false;

    pCastleInfo->setItemTaxRatio(itemTaxRatio);

    char str[40];
    snprintf(str, sizeof(str), "ItemTaxRatio=%d", (int)itemTaxRatio);

    tinysave(pZone->getZoneID(), str);

    GCNoticeEvent gcNoticeEvent;
    gcNoticeEvent.setCode(NOTICE_EVENT_SHOP_TAX_CHANGE);
    gcNoticeEvent.setParameter((uint)itemTaxRatio);

    pZone->broadcastPacket(&gcNoticeEvent);

    return true;

    __END_CATCH
}

int CastleInfoManager::getItemTaxRatio(const PlayerCreature* pPC, const NPC* pNPC) const

{
    __BEGIN_TRY

    if (pNPC == NULL)
        return 100;

    ZoneID_t castleZoneID = pNPC->getTaxingCastleZoneID();
    if (castleZoneID == 0)
        return 100;

    Assert(pPC != NULL);
    //	Zone* pZone = pPC->getZone();
    const CastleInfo* pCastleInfo = getCastleInfo(castleZoneID);

    if (pCastleInfo != NULL) {
        GuildID_t OwnerGuildID = pCastleInfo->getGuildID();
        GuildID_t PlayerGuildID = pPC->getGuildID();
        int ItemTaxRatio = pCastleInfo->getItemTaxRatio();


        if (PlayerGuildID == SlayerCommon || PlayerGuildID == VampireCommon || PlayerGuildID == OustersCommon ||
            PlayerGuildID != OwnerGuildID) {
            return ItemTaxRatio;
        }
    }

    return 100;

    __END_CATCH
}

Gold_t CastleInfoManager::getEntranceFee(ZoneID_t zoneID, PlayerCreature* pPC) const

{
    __BEGIN_TRY

    Assert(pPC != NULL);
    const CastleInfo* pCastleInfo = getCastleInfo(zoneID);

    // If the castle is in war, entrance fee is waived.
    if (pCastleInfo != NULL && !de::gameContext().warSystem().hasCastleActiveWar(zoneID) &&
        !de::gameContext().warSystem().hasActiveRaceWar()) {
        GuildID_t OwnerGuildID = pCastleInfo->getGuildID();
        GuildID_t PlayerGuildID = pPC->getGuildID();
        Gold_t EntranceFee = pCastleInfo->getEntranceFee();

        if (PlayerGuildID == SlayerCommon || PlayerGuildID == VampireCommon || PlayerGuildID == OustersCommon ||
            PlayerGuildID != OwnerGuildID) {
            return EntranceFee;
        }
    }

    return 0;

    __END_CATCH
}

bool CastleInfoManager::isCastleMember(PlayerCreature* pPC) const

{
    __BEGIN_TRY

    Assert(pPC != NULL);
    Zone* pZone = pPC->getZone();
    const CastleInfo* pCastleInfo = getCastleInfo(pZone->getZoneID());

    if (pCastleInfo != NULL) {
        GuildID_t OwnerGuildID = pCastleInfo->getGuildID();
        GuildID_t PlayerGuildID = pPC->getGuildID();

        if (PlayerGuildID == SlayerCommon || PlayerGuildID == VampireCommon || PlayerGuildID == OustersCommon ||
            PlayerGuildID != OwnerGuildID) {
            return false;
        }
    }

    return true;

    __END_CATCH
}

bool CastleInfoManager::isCastleMember(ZoneID_t zoneID, PlayerCreature* pPC) const

{
    __BEGIN_TRY

    Assert(pPC != NULL);
    const CastleInfo* pCastleInfo = getCastleInfo(zoneID);

    if (pCastleInfo != NULL) {
        GuildID_t OwnerGuildID = pCastleInfo->getGuildID();
        GuildID_t PlayerGuildID = pPC->getGuildID();

        if (PlayerGuildID == SlayerCommon || PlayerGuildID == VampireCommon || PlayerGuildID == OustersCommon ||
            PlayerGuildID != OwnerGuildID) {
            return false;
        }
    } else
        return false;

    return true;

    __END_CATCH
}

bool CastleInfoManager::hasOtherBloodBible(ZoneID_t zoneID, PlayerCreature* pPC) const

{
    __BEGIN_TRY

    if (pPC->isFlag(Effect::EFFECT_CLASS_HAS_BLOOD_BIBLE)) {
        // Block entry if carrying a Blood Bible that does not belong to this castle.
        EffectHasBloodBible* pEffect =
            dynamic_cast<EffectHasBloodBible*>(pPC->findEffect(Effect::EFFECT_CLASS_HAS_BLOOD_BIBLE));
        Assert(pEffect != NULL);

        int part = pEffect->getPart();
        ShrineSet* pShrineSet = de::gameContext().shrines().getShrineSet(part);
        if (pShrineSet->getVampireGuardShrine().getZoneID() == zoneID ||
            pShrineSet->getSlayerGuardShrine().getZoneID() == zoneID ||
            pShrineSet->getOustersGuardShrine().getZoneID() == zoneID) {
            return false;
        }

        return true;
    } else {
        return false;
    }

    __END_CATCH
}

bool CastleInfoManager::isPossibleEnter(ZoneID_t zoneID, PlayerCreature* pPC) const

{
    __BEGIN_TRY

    Assert(pPC != NULL);
    const CastleInfo* pCastleInfo = getCastleInfo(zoneID);

    if (pCastleInfo != NULL) {
        Race_t OwnerRace = pCastleInfo->getRace();
        Race_t PlayerRace = pPC->getRace(); //(pPC->isSlayer()? RACE_SLAYER : RACE_VAMPIRE);

        if (de::gameContext().warSystem().hasActiveRaceWar()) {
            if (hasOtherBloodBible(zoneID, pPC)) {
                return false;
            } else {
                return true;
            }
        } else if (OwnerRace == PlayerRace) {
            return true;
        }
        return false;
    }

    return true;

    __END_CATCH
}

bool CastleInfoManager::canPortalActivate(ZoneID_t zoneID, PlayerCreature* pPC) const

{
    __BEGIN_TRY

    WarSystem& warSystem = de::gameContext().warSystem();

    const CastleInfo* pCastleInfo = getCastleInfo(zoneID);

    if (pCastleInfo == NULL) {
        filelog("CastleError.log", "CastleInfoManager::canPortalActivate() CastleInfo(%d) is missing.", (int)zoneID);
        Assert(false);
    }

    if (warSystem.hasActiveRaceWar()) {
        if (hasOtherBloodBible(zoneID, pPC)) {
            return false;
        } else {
            return true;
        }
    }

    if (warSystem.hasCastleActiveWar(zoneID)) {
        return pPC->getRace() == pCastleInfo->getRace();
    }

    return false;

    __END_CATCH
}

CastleInfo* CastleInfoManager::getGuildCastleInfo(GuildID_t guildID) const

{
    __BEGIN_TRY

    if (guildID == SlayerCommon || guildID == VampireCommon || guildID == OustersCommon) {
        return NULL;
    }

    unordered_map<ZoneID_t, CastleInfo*>::const_iterator itr = m_CastleInfos.begin();

    for (; itr != m_CastleInfos.end(); itr++) {
        CastleInfo* pCastleInfo = itr->second;

        if (pCastleInfo->getGuildID() == guildID) {
            return pCastleInfo;
        }
    }

    return NULL;

    __END_CATCH
}

list<CastleInfo*> CastleInfoManager::getGuildCastleInfos(GuildID_t guildID) const

{
    __BEGIN_TRY

    list<CastleInfo*> castleList;

    if (guildID == SlayerCommon || guildID == VampireCommon || guildID == OustersCommon) {
        return castleList;
    }

    unordered_map<ZoneID_t, CastleInfo*>::const_iterator itr = m_CastleInfos.begin();

    for (; itr != m_CastleInfos.end(); itr++) {
        CastleInfo* pCastleInfo = itr->second;

        if (pCastleInfo->getGuildID() == guildID) {
            castleList.push_back(pCastleInfo);
        }
    }

    return castleList;

    __END_CATCH
}

bool CastleInfoManager::getResurrectPosition(PlayerCreature* pPC, ZONE_COORD& zoneCoord)

{
    CastleInfo::ResurrectPriority resurrectPriority = CastleInfo::CASTLE_RESURRECT_PRIORITY_SECOND;

    ZoneID_t castleZoneID;
    bool isCastleZone = de::gameContext().castleInfos().getCastleZoneID(pPC->getResurrectZoneID(), castleZoneID);

    if (!isCastleZone)
        return false;

    CastleInfo* pCastleInfo = getCastleInfo(castleZoneID);

    if (pCastleInfo != NULL) {
        if (pCastleInfo->getZoneID() != pPC->getResurrectZoneID() ||
            de::gameContext().warSystem().hasCastleActiveWar(castleZoneID)) {
            // Resurrect outside the castle.
            // If the resurrect point is a castle dungeon, treat it as outside.
            // Hard-coded because each zone only supports one resurrect point.
            // Example: west/east of Adam's sanctuary has two nearby castles; cannot set both outside points.
            // Also, if the castle is at war, resurrect outside.
            if (isCastleMember(castleZoneID, pPC))
                resurrectPriority = CastleInfo::CASTLE_RESURRECT_PRIORITY_SECOND;
            else
                resurrectPriority = CastleInfo::CASTLE_RESURRECT_PRIORITY_THIRD;
        } else {
            // Resurrect inside the castle.
            // Non-guild members pay a fee; if short on gold, resurrect outside.
            if (isCastleMember(castleZoneID, pPC))
                resurrectPriority = CastleInfo::CASTLE_RESURRECT_PRIORITY_FIRST;
            else {
                Gold_t fee = pCastleInfo->getEntranceFee();
                Gold_t remain = pPC->getGold();

                if (remain < fee) {
                    // Not enough fee: resurrect outside.
                    resurrectPriority = CastleInfo::CASTLE_RESURRECT_PRIORITY_THIRD;
                } else {
                    // Charge the fee and resurrect inside.
                    pPC->decreaseGoldEx(fee);
                    increaseTaxBalance(pCastleInfo->getZoneID(), fee);

                    GCModifyInformation gcMI;
                    gcMI.addLongData(MODIFY_GOLD, pPC->getGold());

                    GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(pPC->getPlayer());
                    if (pGamePlayer->getPlayerStatus() == GPS_NORMAL)
                        pGamePlayer->sendPacket(&gcMI);

                    resurrectPriority = CastleInfo::CASTLE_RESURRECT_PRIORITY_FIRST;
                }
            }
        }
    } else {
        return false;
    }

    pCastleInfo->getResurrectPosition(resurrectPriority, zoneCoord);

    return true;
}

bool CastleInfoManager::tinysave(ZoneID_t zoneID, const string& query)

{
    __BEGIN_TRY

    CastleInfo* pCastleInfo = getCastleInfo(zoneID);
    if (pCastleInfo == NULL)
        return false;

    bool isAffected = defaultWarInfoRepository().tinysaveCastle(
        query, pCastleInfo->getZoneID(), de::kernelContext().config().getPropertyInt("ServerID"));

    return isAffected;

    __END_CATCH
}

void CastleInfoManager::deleteAllNPCs()

{
    __BEGIN_TRY

    unordered_map<ZoneID_t, CastleInfo*>::const_iterator itr = m_CastleInfos.begin();

    for (; itr != m_CastleInfos.end(); itr++) {
        CastleInfo* pCastleInfo = itr->second;

        Zone* pZone = getZoneByZoneID(pCastleInfo->getZoneID());
        Assert(pZone != NULL);

        __ENTER_CRITICAL_SECTION((*pZone))

        // Remove all NPCs.
        pZone->deleteNPCs(RACE_SLAYER);
        pZone->deleteNPCs(RACE_VAMPIRE);

        __LEAVE_CRITICAL_SECTION((*pZone))
    }

    __END_CATCH
}

// The castles' own zones, whose safe zones the race war lifts while it runs.
static vector<ZoneID_t> castleZoneIDsOf(const unordered_map<ZoneID_t, CastleInfo*>& castleInfos) {
    vector<ZoneID_t> zoneIDs;
    for (const auto& castle : castleInfos)
        zoneIDs.push_back(castle.second->getZoneID());
    return zoneIDs;
}

void CastleInfoManager::releaseAllSafeZone() {
    de::war::postToZones(castleZoneIDsOf(m_CastleInfos), [](Zone& zone) {
        __ENTER_CRITICAL_SECTION(zone)

        zone.releaseSafeZone();

        __LEAVE_CRITICAL_SECTION(zone)
    });
}

void CastleInfoManager::resetAllSafeZone() {
    de::war::postToZones(castleZoneIDsOf(m_CastleInfos), [](Zone& zone) {
        __ENTER_CRITICAL_SECTION(zone)

        zone.resetSafeZone();

        __LEAVE_CRITICAL_SECTION(zone)
    });
}

void CastleInfoManager::transportAllOtherRace() {
    for (const auto& castle : m_CastleInfos) {
        ZoneID_t castleZoneID = castle.first;

        // Every zone of the castle, each on its own group's thread; the castle's
        // race and resurrect point are read there, when the transport is made.
        const list<ZoneID_t>& zoneIDList = castle.second->getZoneIDList();
        vector<ZoneID_t> zoneIDs(zoneIDList.begin(), zoneIDList.end());

        de::war::postToZones(zoneIDs, [castleZoneID](Zone& zone) {
            CastleInfo* pCastleInfo = de::gameContext().castleInfos().getCastleInfo(castleZoneID);
            Assert(pCastleInfo != NULL);

            __ENTER_CRITICAL_SECTION(zone)

            // Send the losing race of the race war to the resurrect point.
            Race_t otherRace = (pCastleInfo->getRace() == RACE_SLAYER ? RACE_VAMPIRE : RACE_SLAYER);

            ZONE_COORD zoneCoord;
            pCastleInfo->getResurrectPosition(CastleInfo::CASTLE_RESURRECT_PRIORITY_SECOND, zoneCoord);

            PCManager* pPCManager = (PCManager*)(zone.getPCManager());
            pPCManager->transportAllCreatures(zoneCoord.id, zoneCoord.x, zoneCoord.y, otherRace);

            __LEAVE_CRITICAL_SECTION(zone)
        });
    }
}

void CastleInfoManager::loadAllNPCs()

{
    __BEGIN_TRY

    unordered_map<ZoneID_t, CastleInfo*>::const_iterator itr = m_CastleInfos.begin();

    for (; itr != m_CastleInfos.end(); itr++) {
        CastleInfo* pCastleInfo = itr->second;

        Zone* pZone = getZoneByZoneID(pCastleInfo->getZoneID());
        Assert(pZone != NULL);

        __ENTER_CRITICAL_SECTION((*pZone))

        // Load NPCs
        pZone->loadNPCs(pCastleInfo->getRace());

        __LEAVE_CRITICAL_SECTION((*pZone))
    }

    __END_CATCH
}

ZoneID_t CastleInfoManager::getCastleZoneID(ShrineID_t shrineID) const

{
    __BEGIN_TRY

    unordered_map<ZoneID_t, CastleInfo*>::const_iterator itr = m_CastleInfos.begin();

    for (; itr != m_CastleInfos.end(); itr++) {
        CastleInfo* pCastleInfo = itr->second;

        if (pCastleInfo->getShrineID() == shrineID) {
            return pCastleInfo->getZoneID();
        }
    }

    StringStream msg;
    msg << "No such ShrineID(" << (int)shrineID << ").";

    throw Error(msg.toString());

    return 0;

    __END_CATCH
}

void CastleInfoManager::broadcastShrinePacket(ShrineID_t shrineID, Packet* pPacket) const

{
    __BEGIN_TRY

    ZoneID_t castleZoneID = getCastleZoneID(shrineID);

    const CastleInfo* pCastleInfo = getCastleInfo(castleZoneID);
    Assert(pCastleInfo != NULL);

    pCastleInfo->broadcast(pPacket);

    __END_CATCH
}

bool CastleInfoManager::isCastleZone(ZoneID_t castleZoneID, ZoneID_t targetZoneID) const

{
    __BEGIN_TRY

    const CastleInfo* pCastleInfo = getCastleInfo(castleZoneID);
    Assert(pCastleInfo != NULL);

    return pCastleInfo->isCastleZone(targetZoneID);

    __END_CATCH
}

bool CastleInfoManager::isCastleZone(ZoneID_t zoneID) const {
    __BEGIN_TRY

    unordered_map<ZoneID_t, CastleInfo*>::const_iterator itr = m_CastleInfos.begin();

    for (; itr != m_CastleInfos.end(); ++itr) {
        if (itr->second->isCastleZone(zoneID))
            return true;
    }

    return false;

    __END_CATCH
}

void CastleInfoManager::clearCastleZoneIDs()

{
    __BEGIN_TRY

    m_CastleZoneIDs.clear();

    __END_CATCH
}

bool CastleInfoManager::getCastleZoneID(ZoneID_t zoneID, ZoneID_t& castleZoneID) const

{
    __BEGIN_TRY

    unordered_map<ZoneID_t, ZoneID_t>::const_iterator itr = m_CastleZoneIDs.find(zoneID);

    if (itr != m_CastleZoneIDs.end()) {
        castleZoneID = itr->second;
        return true;
    }

    return false;

    __END_CATCH
}

void CastleInfoManager::setCastleZoneID(ZoneID_t zoneID, ZoneID_t castleZoneID)

{
    __BEGIN_TRY

    m_CastleZoneIDs[zoneID] = castleZoneID;

    __END_CATCH
}

bool CastleInfoManager::isSameCastleZone(ZoneID_t zoneID1, ZoneID_t zoneID2) const

{
    __BEGIN_TRY

    ZoneID_t castleZoneID1, castleZoneID2;

    bool isCastle1 = getCastleZoneID(zoneID1, castleZoneID1);
    bool isCastle2 = getCastleZoneID(zoneID2, castleZoneID2);

    // If either zone is not a castle, they are not in the same castle group.
    if (!isCastle1 || !isCastle2)
        return false;

    return castleZoneID1 == castleZoneID2;

    __END_CATCH
}

SkillType_t CastleInfoManager::getCastleSkillType(ZoneID_t zoneID, GuildID_t guildID) const

{
    __BEGIN_TRY

    const CastleInfo* pCastleInfo = getCastleInfo(zoneID);

    if (pCastleInfo != NULL) {
        GuildID_t OwnerGuildID = pCastleInfo->getGuildID();

        // If the guild does not own the castle, return none.
        if (guildID == SlayerCommon || guildID == VampireCommon || guildID == OustersCommon ||
            guildID != OwnerGuildID) {
            return SKILL_MAX;
        }

        // If the castle is in a guild war, none is available.
        if (de::gameContext().warSystem().hasCastleActiveWar(zoneID)) {
            return SKILL_MAX;
        }

        return de::gameContext().castleSkills().getSkillType(zoneID);
    }

    return SKILL_MAX;

    __END_CATCH
}
