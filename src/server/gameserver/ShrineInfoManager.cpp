#include "ShrineInfoManager.h"

#include <stdio.h>

#include <algorithm>
#include <memory>
#include <vector>

#include "BloodBible.h"
#include "BloodBibleBonusManager.h"
#include "CastleInfoManager.h"
#include "ClientManager.h"
#include "CreatureUtil.h"
#include "EffectHasRelic.h"
#include "EffectShrineGuard.h"
#include "EffectShrineHoly.h"
#include "EffectShrineShield.h"
#include "EventRefreshHolyLandPlayer.h"
#include "GCAddEffect.h"
#include "GCAddEffectToTile.h"
#include "GCBloodBibleStatus.h"
#include "GCDeleteInventoryItem.h"
#include "GCRemoveEffect.h"
#include "GCSystemMessage.h"
#include "GameContext.h"
#include "GlobalItemPosition.h"
#include "GlobalItemPositionLoader.h"
#include "HolyLandManager.h"
#include "Item.h"
#include "ItemFactoryManager.h"
#include "MonsterCorpse.h"
#include "Player.h"
#include "PlayerCreature.h"
#include "RelicUtil.h"
#include "StringPool.h"
#include "StringStream.h"
#include "War.h"
#include "WarSystem.h"
#include "WarZoneWork.h"
#include "Zone.h"
#include "ZoneGroup.h"
#include "ZoneGroupManager.h"
#include "ZoneItemPosition.h"
#include "ZoneUtil.h"
#include "repository/WarInfoRepository.h"

bool AddBible[] = {
    true,  // SHRINE_ARMEGA,      // 0
    false, // SHRINE_MIHOLE,      // 1
    false, // SHRINE_KIRO,        // 2
    false, // SHRINE_INI,         // 3
    false, // SHRINE_GREGORI,     // 4
    false, // SHRINE_CONCILIA,    // 5
    false, // SHRINE_LEGIOS,      // 6
    false, // SHRINE_HILLEL,      // 7
    false, // SHRINE_JAVE,        // 8
    true,  // SHRINE_NEMA,        // 9
    false, // SHRINE_AROSA,       // 10
    false, // SHRINE_CHASPA       // 11
};

string ShrineInfo::toString() const

{
    StringStream msg;

    msg << "ShrineInfo(" << "MonsterType:" << (int)m_MonsterType
        << ",ShrineType:" << (m_ShrineType == SHRINE_GUARD ? "GUARD" : "HOLY") << ",ZoneID:" << (int)m_ZoneID
        << ",X:" << (int)m_X << ",Y:" << (int)m_Y << ",Name:" << m_Name << ")";

    return msg.toString();
}

ShrineInfo& ShrineSet::getReturnGuardShrine()

{
    __BEGIN_TRY

    switch (m_OwnerRace) {
    case RACE_SLAYER:
        return m_SlayerGuardShrine;
        break;
    case RACE_VAMPIRE:
        return m_VampireGuardShrine;
        break;
    case RACE_OUSTERS:
        return m_OustersGuardShrine;
        break;
    }

    // cannot reach here
    Assert(false);
    return m_SlayerGuardShrine;

    __END_CATCH
}

ShrineSet::ShrineSet() {
    m_Mutex.setName("ShrineSet");
    m_pGCBBS = NULL;
}

ShrineSet::~ShrineSet() {
    SAFE_DELETE(m_pGCBBS);
}

Item* ShrineSet::createBloodBibleInGuardShrine()

{
    __BEGIN_TRY

    ShrineInfo* pShrineInfo = &getReturnGuardShrine();
    Zone* pZone = getZoneByZoneID(pShrineInfo->getZoneID());
    Assert(pZone != NULL);

    MonsterCorpse* pShrine = dynamic_cast<MonsterCorpse*>(pZone->getItem(pShrineInfo->getObjectID()));
    Assert(pShrine != NULL);

    list<OptionType_t> optionNULL;
    Item* pItem = de::gameContext().itemFactories().createItem(Item::ITEM_CLASS_BLOOD_BIBLE, m_ItemType, optionNULL);
    Assert(pItem != NULL);

    char strZoneID[10];
    snprintf(strZoneID, sizeof(strZoneID), "%d", (int)pZone->getZoneID());

    pZone->registerObject(pItem);
    pItem->create(strZoneID, STORAGE_CORPSE, pShrine->getObjectID(), 0, 0);

    pShrine->addTreasure(pItem);

    return pItem;

    __END_CATCH
}

void ShrineSet::setOwnerRace(Race_t race)

{
    __BEGIN_TRY

    de::gameContext().bloodBibleBonuses().setBloodBibleBonusRace(m_ShrineID, race);

    m_OwnerRace = race;
    saveBloodBibleOwner();

    __END_CATCH
}

void ShrineSet::setBloodBibleStatus(GCBloodBibleStatus* pGCBBS)

{
    __BEGIN_TRY
    __ENTER_CRITICAL_SECTION(m_Mutex)

    SAFE_DELETE(m_pGCBBS);
    m_pGCBBS = pGCBBS;

    __LEAVE_CRITICAL_SECTION(m_Mutex);
    __END_CATCH
}

void ShrineSet::sendBloodBibleStatus(PlayerCreature* pPC)

{
    __BEGIN_TRY
    __ENTER_CRITICAL_SECTION(m_Mutex)

    if (m_pGCBBS != NULL)
        pPC->getPlayer()->sendPacket(m_pGCBBS);

    __LEAVE_CRITICAL_SECTION(m_Mutex)
    __END_CATCH
}

void ShrineSet::broadcastBloodBibleStatus()

{
    __BEGIN_TRY

    __ENTER_CRITICAL_SECTION(m_Mutex)

    //	if ( m_pGCBBS != NULL ) g_pHolyLandManager->broadcast( m_pGCBBS );
    if (m_pGCBBS != NULL)
        de::gameContext().zoneGroups().broadcast(m_pGCBBS);

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    __END_CATCH
}

string ShrineSet::toString() const

{
    __BEGIN_TRY

    StringStream msg;

    msg << "ShrineSet(" << "ShrineID:" << (int)m_ShrineID << "," << m_SlayerGuardShrine.toString() << ","
        << m_VampireGuardShrine.toString() << "," << m_OustersGuardShrine.toString() << "," << m_HolyShrine.toString()
        << ",ItemType:" << (int)m_ItemType << ")";

    return msg.toString();

    __END_CATCH
}

ShrineInfoManager::~ShrineInfoManager() {
    clear();
}

void ShrineInfoManager::clear() {
    HashMapShrineSetItor itr = m_ShrineSets.begin();
    for (; itr != m_ShrineSets.end(); itr++) {
        SAFE_DELETE(itr->second);
    }
    m_ShrineSets.clear();
}

void ShrineInfoManager::init()

{
    __BEGIN_TRY

    load();

    addAllShrineToZone();

    __END_CATCH
}

void ShrineInfoManager::load()

{
    __BEGIN_TRY

    vector<ShrineRow> rows = defaultWarInfoRepository().loadShrines();

    for (size_t r = 0; r < rows.size(); r++) {
        const ShrineRow& row = rows[r];

        ShrineSet* pShrineSet = new ShrineSet();

        ShrineInfo& SlayerGuardShrine = pShrineSet->getSlayerGuardShrine();
        ShrineInfo& VampireGuardShrine = pShrineSet->getVampireGuardShrine();
        ShrineInfo& OustersGuardShrine = pShrineSet->getOustersGuardShrine();
        ShrineInfo& HolyShrine = pShrineSet->getHolyShrine();

        pShrineSet->setShrineID(row.id);

        SlayerGuardShrine.setName(row.name);
        VampireGuardShrine.setName(SlayerGuardShrine.getName());
        OustersGuardShrine.setName(SlayerGuardShrine.getName());
        HolyShrine.setName(SlayerGuardShrine.getName());

        pShrineSet->setBloodBibleItemType(row.itemType);
        SlayerGuardShrine.setZoneID(row.slayerGuardZoneID);
        SlayerGuardShrine.setX(row.slayerGuardX);
        SlayerGuardShrine.setY(row.slayerGuardY);
        SlayerGuardShrine.setMonsterType(row.slayerGuardMonsterType);
        VampireGuardShrine.setZoneID(row.vampireGuardZoneID);
        VampireGuardShrine.setX(row.vampireGuardX);
        VampireGuardShrine.setY(row.vampireGuardY);
        VampireGuardShrine.setMonsterType(row.vampireGuardMonsterType);
        OustersGuardShrine.setZoneID(row.oustersGuardZoneID);
        OustersGuardShrine.setX(row.oustersGuardX);
        OustersGuardShrine.setY(row.oustersGuardY);
        OustersGuardShrine.setMonsterType(row.oustersGuardMonsterType);
        HolyShrine.setZoneID(row.holyZoneID);
        HolyShrine.setX(row.holyX);
        HolyShrine.setY(row.holyY);
        HolyShrine.setMonsterType(row.holyMonsterType);

        pShrineSet->setOwnerRace((Race_t)row.ownerRace);

        SlayerGuardShrine.setShrineType(ShrineInfo::SHRINE_GUARD);
        VampireGuardShrine.setShrineType(ShrineInfo::SHRINE_GUARD);
        OustersGuardShrine.setShrineType(ShrineInfo::SHRINE_GUARD);
        HolyShrine.setShrineType(ShrineInfo::SHRINE_HOLY);

        // ItemType and the shrine ID must match; a mismatch is a DB configuration error and stops the load.
        if (pShrineSet->getBloodBibleItemType() != pShrineSet->getShrineID()) {
            cout << "ShrineID and ItemType do not match. Check the DB settings." << endl;
            Assert(false);
        }

        addShrineSet(pShrineSet);
    }

    __END_CATCH
}

// Called on the ClientManager thread. It must not be called from anywhere else.
void ShrineInfoManager::reloadOwner()

{
    __BEGIN_TRY

    bool bOwnerChanged = false;

    vector<ShrineOwnerRow> owners = defaultWarInfoRepository().loadShrineOwners();

    for (size_t r = 0; r < owners.size(); r++) {
        ShrineID_t shrineID = owners[r].id;
        Race_t OwnerRace = (Race_t)owners[r].ownerRace;

        ShrineSet* pShrineSet = getShrineSet(shrineID);

        if (pShrineSet->getOwnerRace() != OwnerRace) {
            pShrineSet->setOwnerRace(OwnerRace);
            postBloodBibleReturn(shrineID);

            bOwnerChanged = true;
        }
    }

    if (bOwnerChanged) {
        EventRefreshHolyLandPlayer* pEvent = new EventRefreshHolyLandPlayer(NULL);
        pEvent->setDeadline(0);

        de::gameContext().clients().addEvent_LOCKED(pEvent);
    }

    __END_CATCH
}


void ShrineInfoManager::addAllShrineToZone()

{
    __BEGIN_TRY

    HashMapShrineSetItor itr = m_ShrineSets.begin();
    for (; itr != m_ShrineSets.end(); itr++) {
        ShrineSet* pShrineSet = itr->second;

        if (pShrineSet == NULL)
            continue;

        addShrineToZone(pShrineSet->getSlayerGuardShrine(), pShrineSet->getBloodBibleItemType());
        addShrineToZone(pShrineSet->getVampireGuardShrine(), pShrineSet->getBloodBibleItemType());
        addShrineToZone(pShrineSet->getOustersGuardShrine(), pShrineSet->getBloodBibleItemType());

        Item* pItem = pShrineSet->createBloodBibleInGuardShrine();
        pShrineSet->setBloodBibleItemID(pItem->getItemID());

        addShrineToZone(pShrineSet->getHolyShrine(), pShrineSet->getBloodBibleItemType());
    }

    __END_CATCH
}

void ShrineInfoManager::addShrineToZone(ShrineInfo& shrineInfo, ItemType_t itemType)

{
    __BEGIN_TRY

    // A Holy Shrine is not added to the zone.
    if (shrineInfo.getShrineType() == ShrineInfo::SHRINE_HOLY)
        return;

    // Get the zone the shrine goes into.
    Zone* pZone = getZoneByZoneID(shrineInfo.getZoneID());
    Assert(pZone != NULL);

    MonsterCorpse* pShrine = new MonsterCorpse(shrineInfo.getMonsterType(), shrineInfo.getName(), 2);
    Assert(pShrine != NULL);

    pShrine->setShrine(true);
    pShrine->setZone(pZone);

    pZone->getObjectRegistry().registerObject(pShrine);
    shrineInfo.setObjectID(pShrine->getObjectID());

    cout << "AddShrine[" << (int)shrineInfo.getZoneID() << "] "
         << (shrineInfo.getShrineType() == ShrineInfo::SHRINE_GUARD ? "Guard" : "Holy")
         << ", mtype=" << shrineInfo.getMonsterType() << ", oid=" << pShrine->getObjectID() << endl;

    if (shrineInfo.getShrineType() == ShrineInfo::SHRINE_GUARD) {
        pShrine->setFlag(Effect::EFFECT_CLASS_SHRINE_GUARD);

        EffectShrineGuard* pEffect = new EffectShrineGuard(pShrine);
        pEffect->setShrineID(itemType);
        pEffect->setTick(60 * 10);

        pShrine->getEffectManager().addEffect(pEffect);
    }

    TPOINT tp = pZone->addItem(pShrine, shrineInfo.getX(), shrineInfo.getY(), true);
    Assert(tp.x != -1);

    if (shrineInfo.getShrineType() == ShrineInfo::SHRINE_GUARD) {
        // Attach the Shield Effect to every guard shrine.
        pShrine->setFlag(Effect::EFFECT_CLASS_SHRINE_SHIELD);

        EffectShrineShield* pEffect = new EffectShrineShield(pShrine);
        pEffect->setShrineID(itemType);
        pEffect->setTick(60 * 10);

        pShrine->getEffectManager().addEffect(pEffect);
    }

    forbidDarkness(pZone, tp.x, tp.y, 2);

    // Set the shrine coordinates anew.
    shrineInfo.setX(tp.x);
    shrineInfo.setY(tp.y);

    __END_CATCH
}

void ShrineInfoManager::addShrineSet(ShrineSet* pShrineSet)

{
    __BEGIN_TRY

    if (pShrineSet == NULL)
        return;

    ShrineID_t shrineID = pShrineSet->getShrineID();

    HashMapShrineSetItor itr = m_ShrineSets.find(shrineID);

    if (itr != m_ShrineSets.end()) {
        filelog("ShrineInfoError.log", "ShrineInfoManager::addShrineSet DuplicatedException : %d", (int)shrineID);
        return;
    }

    m_ShrineSets[shrineID] = pShrineSet;

    __END_CATCH
}

void ShrineInfoManager::deleteShrineSet(ShrineID_t shrineID)

{
    __BEGIN_TRY

    HashMapShrineSetItor itr = m_ShrineSets.find(shrineID);

    if (itr != m_ShrineSets.end()) {
        SAFE_DELETE(itr->second);
        m_ShrineSets.erase(shrineID);
    }

    __END_CATCH
}

ShrineSet* ShrineInfoManager::getShrineSet(ShrineID_t shrineID) const

{
    __BEGIN_TRY

    HashMapShrineSetConstItor itr = m_ShrineSets.find(shrineID);

    if (itr != m_ShrineSets.end()) {
        return itr->second;
    }

    return NULL;

    __END_CATCH
}

bool ShrineInfoManager::isMatchGuardShrine(Item* pItem, MonsterCorpse* pMonsterCorpse, PlayerCreature* pPC) const

{
    __BEGIN_TRY

    if (pItem->getItemClass() != Item::ITEM_CLASS_BLOOD_BIBLE)
        return false;

    ItemType_t itemType = pItem->getItemType();
    ShrineID_t shrineID = itemType; // ShrineID = ItemType(of BloodBible)

    ShrineSet* pShrineSet = getShrineSet(shrineID);

    if (pShrineSet == NULL) {
        // There is no Shrine Set for this blood bible.
        return false;
    }

    // True when the MonsterType of the guard shrine of the Shrine set for this blood
    // bible equals the MonsterType of the MonsterCorpse passed in.
    if (pPC->isSlayer()) {
        return pShrineSet->getSlayerGuardShrine().getMonsterType() == pMonsterCorpse->getMonsterType();
    } else if (pPC->isVampire()) {
        return pShrineSet->getVampireGuardShrine().getMonsterType() == pMonsterCorpse->getMonsterType();
    } else if (pPC->isOusters()) {
        return pShrineSet->getOustersGuardShrine().getMonsterType() == pMonsterCorpse->getMonsterType();
    }

    return false;

    __END_CATCH
}


bool ShrineInfoManager::isMatchHolyShrine(Item* pItem, MonsterCorpse* pMonsterCorpse) const

{
    __BEGIN_TRY

    if (pItem->getItemClass() != Item::ITEM_CLASS_BLOOD_BIBLE)
        return false;

    ItemType_t itemType = pItem->getItemType();
    ShrineID_t shrineID = itemType; // ShrineID = ItemType(of BloodBible)

    ShrineSet* pShrineSet = getShrineSet(shrineID);

    if (pShrineSet == NULL) {
        // There is no Shrine Set for this blood bible.
        return false;
    }

    // True when the MonsterType of the holy shrine of the Shrine set for this blood
    // bible equals the MonsterType of the MonsterCorpse passed in.
    return pShrineSet->getHolyShrine().getMonsterType() == pMonsterCorpse->getMonsterType();

    __END_CATCH
}

bool ShrineInfoManager::isDefenderOfGuardShrine(PlayerCreature* pPC, MonsterCorpse* pShrine) const

{
    __BEGIN_TRY

    Zone* pZone = pShrine->getZone();
    Assert(pZone != NULL);

    // Not a castle -- fail.
    if (!pZone->isCastle()) {
        return false;
    }

    ZoneID_t castleZoneID = pZone->getZoneID();

    CastleInfo* pCastleInfo = de::gameContext().castleInfos().getCastleInfo(castleZoneID);
    if (pCastleInfo == NULL)
        return false;

    // During a race war a player of the same race as the castle owner is a defender.
    if (pPC->getRace() == pCastleInfo->getRace())
        return true;

    return false;

    __END_CATCH
}

// Can this race pick up a blood bible fragment?
bool ShrineInfoManager::canPickupBloodBible(Race_t race, BloodBible* pBloodBible) const

{
    __BEGIN_TRY

    // The blood bible is used only in race wars.
    return true;

    __END_CATCH
}

bool ShrineInfoManager::getMatchGuardShrinePosition(Item* pItem, ZoneItemPosition& zip) const

{
    __BEGIN_TRY

    if (pItem->getItemClass() != Item::ITEM_CLASS_BLOOD_BIBLE)
        return false;

    ItemType_t itemType = pItem->getItemType();
    ShrineID_t shrineID = itemType; // ShrineID = ItemType(of BloodBible)

    ShrineSet* pShrineSet = getShrineSet(shrineID);

    if (pShrineSet == NULL) {
        // There is no Shrine Set for this blood bible.
        return false;
    }

    ShrineInfo& GuardShrine = pShrineSet->getReturnGuardShrine();

    zip.setZoneID(GuardShrine.getZoneID());
    zip.setZoneX(GuardShrine.getX());
    zip.setZoneY(GuardShrine.getY());

    return true;

    __END_CATCH
}

// Called from putBloodBible, on the zone thread of the zone holding the shrine
// the bible was just laid on, so the bible is taken without the zone's mutex.
bool ShrineInfoManager::returnBloodBible(ShrineID_t shrineID) const

{
    __BEGIN_TRY

    // Find the BloodBible related to shrineID using the DB information.
    ShrineSet* pShrineSet = getShrineSet(shrineID);

    if (pShrineSet == NULL)
        return false;

    Item::ItemClass ItemClass = Item::ITEM_CLASS_BLOOD_BIBLE;
    ItemID_t ItemID = pShrineSet->getBloodBibleItemID();

    if (ItemID == 0)
        return false;

    std::unique_ptr<GlobalItemPosition> pItemPosition(GlobalItemPositionLoader::getInstance()->load(ItemClass, ItemID));

    if (pItemPosition == nullptr)
        return false;

    pItemPosition->expectItem(ItemClass, ItemID);
    Item* pItem = pItemPosition->popItem(false);

    if (pItem != NULL && pItem->getItemClass() == Item::ITEM_CLASS_BLOOD_BIBLE) {
        Zone* pZone = pItemPosition->getZone();
        Assert(pZone != NULL);

        BloodBible* pBloodBible = dynamic_cast<BloodBible*>(pItem);
        Assert(pBloodBible != NULL);

        return returnBloodBible(pZone, pBloodBible);
    }

    return false;

    __END_CATCH
}

bool ShrineInfoManager::postBloodBibleReturn(ShrineID_t shrineID) const {
    ShrineSet* pShrineSet = getShrineSet(shrineID);
    if (pShrineSet == NULL)
        return false;

    ItemID_t itemID = pShrineSet->getBloodBibleItemID();
    if (itemID == 0)
        return false;

    return de::war::postItemReturn(Item::ITEM_CLASS_BLOOD_BIBLE, itemID, [](Zone& from, Item* pItem) {
        BloodBible* pBloodBible = dynamic_cast<BloodBible*>(pItem);
        Assert(pBloodBible != NULL);

        de::gameContext().shrines().returnBloodBible(&from, pBloodBible);
    });
}

// Called when the race war ends.
bool ShrineInfoManager::returnAllBloodBible() const

{
    __BEGIN_TRY

    bool bReturned = false;

    HashMapShrineSetConstItor itr = m_ShrineSets.begin();

    // The shrineID for castleZoneID cannot be looked up, so compare them one by one.
    for (; itr != m_ShrineSets.end(); itr++) {
        ShrineSet* pShrineSet = itr->second;

        bReturned = postBloodBibleReturn(pShrineSet->getShrineID()) || bReturned;
    }

    return bReturned;

    __END_CATCH
}


bool ShrineInfoManager::returnBloodBible(Zone* pZone, BloodBible* pBloodBible) const

{
    __BEGIN_TRY

    StringPool& strings = de::gameContext().strings();

    Assert(pZone != NULL);
    Assert(pBloodBible != NULL);

    // Find the TargetZone and Shrine.
    ShrineID_t shrineID = pBloodBible->getItemType();
    ShrineSet* pShrineSet = getShrineSet(shrineID);

    if (pShrineSet == NULL)
        return false;

    ShrineInfo& GuardShrine = pShrineSet->getReturnGuardShrine();

    Zone* pTargetZone = getZoneByZoneID(GuardShrine.getZoneID());
    Assert(pTargetZone != NULL);

    ObjectID_t CorpseObjectID = GuardShrine.getObjectID();

    pZone->transportItemToCorpse(pBloodBible, pTargetZone, CorpseObjectID);

    char msg[300];

    const char* race = "";
    if (pShrineSet->getOwnerRace() == RACE_SLAYER) {
        race = strings.c_str(STRID_SLAYER);
    } else if (pShrineSet->getOwnerRace() == RACE_VAMPIRE) {
        race = strings.c_str(STRID_VAMPIRE);
    } else if (pShrineSet->getOwnerRace() == RACE_OUSTERS) {
        race = strings.c_str(STRID_OUSTERS);
    }

    snprintf(msg, sizeof(msg), strings.c_str(STRID_RETURN_TO_GUARD_SHRINE_BLOOD_BIBLE), GuardShrine.getName().c_str(),
             race,
             //					(pShrineSet->getOwnerRace()==RACE_SLAYER? g_pStringPool->c_str( STRID_SLAYER ) :
             // g_pStringPool->c_str( STRID_VAMPIRE ) ),
             GuardShrine.getName().c_str());
    GCSystemMessage msgPkt;
    msgPkt.setMessage(msg);

    de::gameContext().holyLands().broadcast(&msgPkt);

    return true;

    __END_CATCH
}

bool ShrineInfoManager::putBloodBible(PlayerCreature* pPC, Item* pItem, MonsterCorpse* pCorpse) const

{
    __BEGIN_TRY

    Assert(pPC != NULL);
    Assert(pItem != NULL);
    Assert(pCorpse != NULL);

    ShrineID_t shrineID = pItem->getItemType();

    filelog("WarLog.txt", "%s put the Blood Bible[%u] on the holy shrine[%s].", pPC->getName().c_str(), (uint)shrineID,
            pCorpse->getName().c_str());

    // Attach the effect showing the blood bible flying back from the shrine it was put into.
    //	sendBloodBibleEffect( pCorpse, Effect::EFFECT_CLASS_SHRINE_HOLY_WARP );

    // Take the bible from the PC and put it inside the shrine.
    Assert(pItem->getObjectID() == pPC->getExtraInventorySlotItem()->getObjectID());
    pPC->deleteItemFromExtraInventorySlot();

    GCDeleteInventoryItem gcDeleteInventoryItem;
    gcDeleteInventoryItem.setObjectID(pItem->getObjectID());

    pPC->getPlayer()->sendPacket(&gcDeleteInventoryItem);

    deleteRelicEffect(pPC, pItem);

    pCorpse->addTreasure(pItem);

    ShrineSet* pShrineSet = getShrineSet(shrineID);
    if (pShrineSet == NULL)
        return false;

    // Placing it in the matching holy shrine changes the owner race, but only
    // for a player the running war lets take a shrine: the shrines of Adam's
    // holy land are fought over in the race war, so outside one, and for anyone
    // the war is not open to, the bible only travels back.
    if ((isMatchHolyShrine(pItem, pCorpse) && de::gameContext().warSystem().mayModifyShrineOwner(pPC))
        // Placing it in the GuardShrine is allowed when the castle's race and the player's race match.
        || (isDefenderOfGuardShrine(pPC, pCorpse) && isMatchGuardShrine(pItem, pCorpse, pPC))) {
        pShrineSet->setOwnerRace(pPC->getRace());
    }

    // The bible goes back to its guard shrine either way, so it can be carried
    // and contested again while the war lasts; the owner races the shrine sets
    // ended up with are tallied when the war ends.
    returnBloodBible(shrineID);

    return false;

    __END_CATCH
}

// The guard shrines of a shrine set, one per race.
static std::vector<ShrineInfo*> guardShrinesOf(ShrineSet* pShrineSet) {
    return {&pShrineSet->getSlayerGuardShrine(), &pShrineSet->getVampireGuardShrine(),
            &pShrineSet->getOustersGuardShrine()};
}

std::vector<ZoneID_t> ShrineInfoManager::getGuardShrineZoneIDs() const {
    std::vector<ZoneID_t> zoneIDs;

    for (const auto& entry : m_ShrineSets) {
        for (ShrineInfo* pGuard : guardShrinesOf(entry.second)) {
            ZoneID_t zoneID = pGuard->getZoneID();
            if (std::find(zoneIDs.begin(), zoneIDs.end(), zoneID) == zoneIDs.end())
                zoneIDs.push_back(zoneID);
        }
    }

    return zoneIDs;
}

std::vector<ShrineInfo*> ShrineInfoManager::getGuardShrinesIn(ZoneID_t zoneID) const {
    std::vector<ShrineInfo*> guards;

    for (const auto& entry : m_ShrineSets) {
        for (ShrineInfo* pGuard : guardShrinesOf(entry.second)) {
            if (pGuard->getZoneID() == zoneID)
                guards.push_back(pGuard);
        }
    }

    return guards;
}

bool ShrineInfoManager::removeAllShrineShield() {
    de::war::postToZones(getGuardShrineZoneIDs(), [](Zone& zone) {
        ShrineInfoManager& shrines = de::gameContext().shrines();
        for (ShrineInfo* pGuard : shrines.getGuardShrinesIn(zone.getZoneID()))
            shrines.removeShrineShield(pGuard);
    });

    return true;
}

bool ShrineInfoManager::removeShrineShield(ShrineInfo* pShrineInfo)

{
    __BEGIN_TRY

    ZoneID_t guardZoneID = pShrineInfo->getZoneID();

    Zone* pZone = getZoneByZoneID(guardZoneID);
    Assert(pZone != NULL);
    pZone->getZoneGroup()->assertOwned();

    Item* pItem = pZone->getItem(pShrineInfo->getObjectID());

    if (pItem != NULL &&
        pItem->getItemClass() == Item::ITEM_CLASS_CORPSE
        //		&& pItem->isFlag(Effect::EFFECT_CLASS_HAS_BLOOD_BIBLE)
        && pItem->isFlag(Effect::EFFECT_CLASS_SHRINE_SHIELD)) {
        pItem->removeFlag(Effect::EFFECT_CLASS_SHRINE_SHIELD);

        Corpse* pCorpse = dynamic_cast<Corpse*>(pItem);

        EffectManager& EM = pItem->getEffectManager();
        EM.deleteEffect(Effect::EFFECT_CLASS_SHRINE_SHIELD);

        GCRemoveEffect gcRemoveEffect;
        gcRemoveEffect.setObjectID(pItem->getObjectID());
        gcRemoveEffect.addEffectList(Effect::EFFECT_CLASS_SHRINE_SHIELD);
        pZone->broadcastPacket(pCorpse->getX(), pCorpse->getY(), &gcRemoveEffect);

        // Report the position of the blood bible on the shrine.
        if (pItem->isFlag(Effect::EFFECT_CLASS_HAS_BLOOD_BIBLE)) {
            Effect* pEffect = EM.findEffect(Effect::EFFECT_CLASS_HAS_BLOOD_BIBLE);
            Assert(pEffect != NULL);

            EffectHasRelic* pHasRelicEffect = dynamic_cast<EffectHasRelic*>(pEffect);
            Assert(pHasRelicEffect != NULL);

            pHasRelicEffect->affect();
        }

        return true;
    }

    return false;

    __END_CATCH
}

void ShrineInfoManager::addAllShrineShield() {
    de::war::postToZones(getGuardShrineZoneIDs(), [](Zone& zone) {
        ShrineInfoManager& shrines = de::gameContext().shrines();
        for (ShrineInfo* pGuard : shrines.getGuardShrinesIn(zone.getZoneID()))
            shrines.addShrineShield(*pGuard);
    });
}

bool ShrineInfoManager::addShrineShield(ShrineInfo& shrineInfo)

{
    __BEGIN_TRY

    Zone* pZone = getZoneByZoneID(shrineInfo.getZoneID());
    Assert(pZone != NULL);
    pZone->getZoneGroup()->assertOwned();

    Item* pItem = pZone->getItem(shrineInfo.getObjectID());

    if (pItem != NULL && pItem->getItemClass() == Item::ITEM_CLASS_CORPSE &&
        !pItem->isFlag(Effect::EFFECT_CLASS_SHRINE_SHIELD)) {
        pItem->setFlag(Effect::EFFECT_CLASS_SHRINE_SHIELD);

        Corpse* pCorpse = dynamic_cast<Corpse*>(pItem);
        Assert(pCorpse != NULL);

        EffectManager& EM = pItem->getEffectManager();
        EffectShrineShield* pEffect = new EffectShrineShield(pCorpse);
        pEffect->setShrineID(pCorpse->getItemType());
        pEffect->setTick(60 * 10);
        EM.addEffect(pEffect);

        GCAddEffect gcAddEffect;
        gcAddEffect.setObjectID(pItem->getObjectID());
        gcAddEffect.setEffectID(Effect::EFFECT_CLASS_SHRINE_SHIELD);
        gcAddEffect.setDuration(65000);

        pZone->broadcastPacket(pCorpse->getX(), pCorpse->getY(), &gcAddEffect);

        return true;
    }

    return false;

    __END_CATCH
}

bool ShrineInfoManager::saveBloodBibleOwner()

{
    __BEGIN_TRY

    // The original wrapped this loop in a BEGIN_DB block that created a
    // Statement it never used; each ShrineSet does its own write through
    // the repository.
    HashMapShrineSetConstItor itr = m_ShrineSets.begin();

    for (; itr != m_ShrineSets.end(); itr++) {
        ShrineSet* pShrineSet = itr->second;

        pShrineSet->saveBloodBibleOwner();
    }

    return true;

    __END_CATCH
}

bool ShrineSet::saveBloodBibleOwner()

{
    __BEGIN_TRY

    defaultWarInfoRepository().saveShrineOwner((int)getOwnerRace(), (int)getShrineID());

    return true;

    __END_CATCH
}

void ShrineInfoManager::registerBloodBibleStatus(ItemType_t m_Part, GCBloodBibleStatus* pGCBBS)

{
    __BEGIN_TRY

    ShrineSet* pShrineSet = getShrineSet(m_Part);
    Assert(pShrineSet != NULL);

    pShrineSet->setBloodBibleStatus(pGCBBS);

    __END_CATCH
}

void ShrineInfoManager::sendBloodBibleStatus(PlayerCreature* pPC)

{
    __BEGIN_TRY

    Assert(pPC != NULL);

    HashMapShrineSetConstItor itr = m_ShrineSets.begin();

    for (; itr != m_ShrineSets.end(); itr++) {
        ShrineSet* pShrineSet = itr->second;

        pShrineSet->sendBloodBibleStatus(pPC);
    }

    __END_CATCH
}

void ShrineInfoManager::broadcastBloodBibleStatus()

{
    __BEGIN_TRY

    HashMapShrineSetConstItor itr = m_ShrineSets.begin();

    for (; itr != m_ShrineSets.end(); itr++) {
        ShrineSet* pShrineSet = itr->second;

        pShrineSet->broadcastBloodBibleStatus();
    }

    __END_CATCH
}

string ShrineInfoManager::toString() const

{
    __BEGIN_TRY

    StringStream msg;

    msg << "ShrineInfoManager(" << "Size:" << size() << ",(\n";

    HashMapShrineSetConstItor itr = m_ShrineSets.begin();

    for (; itr != m_ShrineSets.end(); itr++) {
        msg << "\t" << itr->second->toString() << "\n";
    }

    msg << "))";

    return msg.toString();

    __END_CATCH
}
