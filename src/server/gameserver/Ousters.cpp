//////////////////////////////////////////////////////////////////////////////
// Filename    : Ousters.cpp
// Written By  : Elca
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "Ousters.h"

#include "AbilityBalance.h"
#include "CreatureUtil.h"
#include "EffectLoaderManager.h"
#include "FlagSet.h"
#include "GameContext.h"
#include "GamePlayer.h"
#include "ItemInfoManager.h"
#include "ItemLoaderManager.h"
#include "ItemUtil.h"
#include "OptionInfo.h"
#include "OustersEXPInfo.h"
#include "PacketUtil.h"
#include "Party.h"
#include "Player.h"
#include "Shape.h"
#include "SkillInfo.h"
#include "SkillParentInfo.h"
#include "SkillUtil.h"
#include "Stash.h"
#include "TradeManager.h"
#include "repository/CharacterRepository.h"
#include "repository/GoldRepository.h"
#include "repository/SkillSaveRepository.h"
#include "repository/StashRepository.h"
// #include "RankEXPInfo.h"
#include <stdio.h>

#include "AdvancementClassExpTable.h"
#include "CastleSkillInfo.h"
#include "DynamicZone.h"
#include "EffectGrandMasterOusters.h"
#include "GCChangeShape.h"
#include "GCModifyInformation.h"
#include "GCOtherModifyInfo.h"
#include "GCPetStashList.h"
#include "GCRealWearingInfo.h"
#include "GCSkillInfo.h"
#include "GCStatusCurrentHP.h"
#include "GCTakeOff.h"
#include "GuildUnion.h"
#include "MonsterInfo.h"
#include "PKZoneInfoManager.h"
#include "RaceWarLimiter.h"
#include "RankExpTable.h"
#include "ResurrectLocationManager.h"
#include "Store.h"
#include "SystemAvailabilitiesManager.h"
#include "TimeLimitItemManager.h"
#include "VariableManager.h"
#include "WarSystem.h"
#include "item/AR.h"
#include "item/Belt.h"
#include "item/OustersArmsband.h"
#include "item/OustersStone.h"
#include "item/OustersWristlet.h"
#include "item/SG.h"
#include "item/SMG.h"
#include "item/SR.h"
#include "item/Skull.h"
#include "skill/EffectBless.h"
#include "skill/EffectDoom.h"
#include "skill/EffectParalyze.h"
#include "skill/EffectTransformToBat.h"
#include "skill/EffectTransformToWolf.h"
#include "skill/OustersCastleSkillSlot.h"

const Color_t QUEST_COLOR = 0xFFFE;

const Level_t MAX_OUSTERS_LEVEL = 150;

Ousters::Ousters()

    : PlayerCreature(0, NULL), m_pRealWearingCheck{} {
    __BEGIN_TRY

    m_CClass = CREATURE_CLASS_OUSTERS;

    m_Mutex.setName("Ousters");

    // Initialize melee skill slots except double impact.
    for (int i = 0; i < SKILL_DOUBLE_IMPACT; i++) {
        OustersSkillSlot* pOustersSkillSlot = new OustersSkillSlot;
        pOustersSkillSlot->setName(m_Name);
        pOustersSkillSlot->setSkillType(i);
        pOustersSkillSlot->setInterval(5);
        pOustersSkillSlot->setRunTime();

        addSkill(pOustersSkillSlot);
    }

    //////////////////////////////////////
    //////////////////////////////////////
    // Default absorb/sylph skills
    //////////////////////////////////////
    //////////////////////////////////////
    {
        OustersSkillSlot* pOustersSkillSlot = new OustersSkillSlot;
        pOustersSkillSlot->setName(m_Name);
        pOustersSkillSlot->setSkillType(SKILL_ABSORB_SOUL);
        pOustersSkillSlot->setExpLevel(1);
        pOustersSkillSlot->setInterval(5);
        pOustersSkillSlot->setRunTime();

        addSkill(pOustersSkillSlot);
    }
    {
        OustersSkillSlot* pOustersSkillSlot = new OustersSkillSlot;
        pOustersSkillSlot->setName(m_Name);
        pOustersSkillSlot->setSkillType(SKILL_SUMMON_SYLPH);
        pOustersSkillSlot->setExpLevel(1);
        pOustersSkillSlot->setInterval(5);
        pOustersSkillSlot->setRunTime();

        addSkill(pOustersSkillSlot);
    }

    for (int i = 0; i < OUSTERS_WEAR_MAX; i++)
        m_pWearItem[i] = NULL;

    getCurrentTime(m_MPRegenTime);

    // Reset save counters.
    //	m_RankExpSaveCount		= 0;
    m_ExpSaveCount = 0;
    m_FameSaveCount = 0;
    m_AlignmentSaveCount = 0;

    __END_CATCH
}

Ousters::~Ousters()

{
    __BEGIN_TRY

    // Persist outlook info before destruction. by sigi. 2002.6.18
    char pField[128];
    snprintf(pField, sizeof(pField), "CoatType=%d,ArmType=%d,CoatColor=%d,ArmColor=%d,BootsColor=%d",
             m_OustersInfo.getCoatType(), m_OustersInfo.getArmType(), m_OustersInfo.getCoatColor(),
             m_OustersInfo.getArmColor(), m_OustersInfo.getBootsColor());

    tinysave(pField);

    // Persist equipment/exp/skills before destruction.
    saveGears();
    saveExps(m_GoalExp, m_SilverDamage);
    saveSkills();

    // Destroy equipped items (ownership transfers to garbage collector).
    destroyGears();

    // Cancel any pending trade tied to this character before cleanup.
    TradeManager* pTradeManager = m_pZone->getTradeManager();
    TradeInfo* pInfo = pTradeManager->getTradeInfo(getName());
    if (pInfo != NULL) {
        // Cancel the live trade session if one exists.
        pTradeManager->cancelTrade(this);
    }

    // Clear party info to avoid stale references in zone cleanup flows.
    deleteAllPartyInfo(this);

    unordered_map<SkillType_t, OustersSkillSlot*>::iterator itr = m_SkillSlot.begin();
    for (; itr != m_SkillSlot.end(); itr++) {
        OustersSkillSlot* pOustersSkillSlot = itr->second;
        SAFE_DELETE(pOustersSkillSlot);
    }

    __END_CATCH_NO_RETHROW
}

// registerObject
// Register this Ousters in the zone object registry and assign object IDs.
void Ousters::registerObject()

{
    __BEGIN_TRY

    Assert(getZone() != NULL);

    // Register within the zone object registry.
    ObjectRegistry& OR = getZone()->getObjectRegistry();

    __ENTER_CRITICAL_SECTION(OR)

    // Clear any time-limit manager state before registering items.
    if (m_pTimeLimitItemManager != NULL)
        m_pTimeLimitItemManager->clear();

    // Register self without locking (caller holds the lock).
    OR.registerObject_NOLOCKED(this);

    // Register inventory items.
    registerInventory(OR);

    // Register goods inventory items.
    registerGoodsInventory(OR);

    // Register equipped items.
    for (int i = 0; i < OUSTERS_WEAR_MAX; i++) {
        Item* pItem = m_pWearItem[i];

        if (pItem != NULL) {
            bool bCheck = true;

            // Skip registering the left hand if a two-hand weapon occupies the right.
            if (i == WEAR_RIGHTHAND && isTwohandWeapon(pItem))
                bCheck = false;

            if (bCheck)
                registerItem(pItem, OR);
        }
    }

    // Register extra-slot item if present.
    Item* pSlotItem = m_pExtraInventorySlot->getItem();
    if (pSlotItem != NULL)
        registerItem(pSlotItem, OR);

    m_Garbage.registerObject(OR);

    for (int i = 0; i < MAX_PET_STASH; ++i) {
        Item* pItem = getPetStashItem(i);
        if (pItem != NULL)
            registerItem(pItem, OR);
    }

    __LEAVE_CRITICAL_SECTION(OR)

    m_OustersInfo.setObjectID(m_ObjectID);
    m_pStore->updateStoreInfo();

    __END_CATCH
}

// Register this Ousters and items during initial load (with trace logging where needed).
void Ousters::registerInitObject()

{
    __BEGIN_TRY

    Assert(getZone() != NULL);

    ObjectRegistry& OR = getZone()->getObjectRegistry();

    __ENTER_CRITICAL_SECTION(OR)

    // Clear any time-limit manager state before registering items.
    if (m_pTimeLimitItemManager != NULL)
        m_pTimeLimitItemManager->clear();

    // Register self without locking (caller holds the lock).
    OR.registerObject_NOLOCKED(this);

    // Register inventory items (initial load path).
    registerInitInventory(OR);

    // Register goods inventory items (initial load path).
    registerGoodsInventory(OR);

    // Register equipped items (initial load path).
    for (int i = 0; i < OUSTERS_WEAR_MAX; i++) {
        Item* pItem = m_pWearItem[i];

        if (pItem != NULL) {
            // Enable trace logging for registered item.
            pItem->setTraceItem(bTraceLog(pItem));

            bool bCheck = true;

            // Skip registering the left hand if a two-hand weapon occupies the right.
            if (i == WEAR_RIGHTHAND && isTwohandWeapon(pItem))
                bCheck = false;

            if (bCheck)
                registerItem(pItem, OR);
        }
    }

    // Sweep equipped items for expiration.
    Item* pSlotItem = m_pExtraInventorySlot->getItem();
    if (pSlotItem != NULL) {
        pSlotItem->setTraceItem(bTraceLog(pSlotItem));
        registerItem(pSlotItem, OR);
    }

    m_Garbage.registerObject(OR);

    __LEAVE_CRITICAL_SECTION(OR)

    m_OustersInfo.setObjectID(m_ObjectID);

    __END_CATCH
}

void Ousters::checkItemTimeLimit() {
    __BEGIN_TRY

    {
        list<Item*> ItemList;
        int height = m_pInventory->getHeight();
        int width = m_pInventory->getWidth();

        for (int j = 0; j < height; j++) {
            for (int i = 0; i < width; i++) {
                Item* pItem = m_pInventory->getItem(i, j);
                if (pItem != NULL) {
                    list<Item*>::iterator itr = find(ItemList.begin(), ItemList.end(), pItem);

                    if (itr == ItemList.end()) {
                        i += pItem->getVolumeWidth() - 1;

                        if (wasteIfTimeLimitExpired(pItem)) {
                            m_pInventory->deleteItem(pItem->getObjectID());
                            SAFE_DELETE(pItem);
                        } else {
                            ItemList.push_back(pItem);
                        }
                    }
                }
            }
        }
    }

    // Sweep equipped items for expiration.
    {
        for (int i = 0; i < OUSTERS_WEAR_MAX; i++) {
            Item* pItem = m_pWearItem[i];

            if (pItem != NULL) {
                bool bCheck = true;

                // Skip left hand check if right hand holds a two-hand weapon.
                if (i == WEAR_RIGHTHAND && isTwohandWeapon(pItem))
                    bCheck = false;

                if (bCheck) {
                    if (wasteIfTimeLimitExpired(pItem)) {
                        deleteWearItem((WearPart)i);
                        if (i == WEAR_LEFTHAND && isTwohandWeapon(pItem))
                            deleteWearItem(WEAR_RIGHTHAND);
                        SAFE_DELETE(pItem);
                    }
                }
            }
        }
    }

    // Sweep extra inventory slot for expiration.
    {
        Item* pSlotItem = m_pExtraInventorySlot->getItem();
        if (pSlotItem != NULL && wasteIfTimeLimitExpired(pSlotItem)) {
            deleteItemFromExtraInventorySlot();
            SAFE_DELETE(pSlotItem);
        }
    }

    __END_CATCH
}

void Ousters::updateEventItemTime(DWORD time) {
    __BEGIN_TRY

    {
        list<Item*> ItemList;
        int height = m_pInventory->getHeight();
        int width = m_pInventory->getWidth();

        for (int j = 0; j < height; j++) {
            for (int i = 0; i < width; i++) {
                Item* pItem = m_pInventory->getItem(i, j);
                if (pItem != NULL) {
                    list<Item*>::iterator itr = find(ItemList.begin(), ItemList.end(), pItem);

                    if (itr == ItemList.end()) {
                        i += pItem->getVolumeWidth() - 1;
                        updateItemTimeLimit(pItem, time);
                        ItemList.push_back(pItem);
                    }
                }
            }
        }
    }

    {
        for (int i = 0; i < OUSTERS_WEAR_MAX; i++) {
            Item* pItem = m_pWearItem[i];

            if (pItem != NULL) {
                bool bCheck = true;

                if (i == WEAR_RIGHTHAND && isTwohandWeapon(pItem))
                    bCheck = false;

                if (bCheck) {
                    updateItemTimeLimit(pItem, time);
                }
            }
        }
    }

    {
        Item* pSlotItem = m_pExtraInventorySlot->getItem();
        if (pSlotItem != NULL) {
            updateItemTimeLimit(pSlotItem, time);
        }
    }

    __END_CATCH
}

void Ousters::loadOwnedItems() {
    de::gameContext().itemLoaders().load(this);
}

// An ousters reads the newbie flag the other way round from a slayer: it
// is off until the starting set has been given, and turned on after.
void Ousters::giveNewbieItems() {
    if (!m_pFlagSet->isOn(FLAGSET_RECEIVE_NEWBIE_ITEM_AUTO)) {
        addNewbieItemToInventory(this);
        addNewbieGoldToInventory(this);
        addNewbieItemToGear(this);
        m_pFlagSet->turnOn(FLAGSET_RECEIVE_NEWBIE_ITEM_AUTO);
        m_pFlagSet->save(getName());
    }
}


//----------------------------------------------------------------------
//----------------------------------------------------------------------
bool Ousters::load()

{
    __BEGIN_TRY

    if (!PlayerCreature::load())
        return false;

    OustersLoadRecord record;
    if (!defaultCharacterRepository().loadOusters(m_Name, record))
        return false;

    setName(record.name);

    Level_t advLevel = record.advancementClass;
    Exp_t advGoalExp = record.advancementGoalExp;

    m_pAdvancementClass =
        new AdvancementClass(advLevel, advGoalExp, AdvancementClassExpTable::s_AdvancementClassExpTable);
    if (getAdvancementClassLevel() > 0)
        m_bAdvanced = true;

    setSex(record.sex);
    setMasterEffectColor(record.masterEffectColor);

    m_STR[ATTR_BASIC] = record.str;
    m_STR[ATTR_CURRENT] = m_STR[ATTR_BASIC];
    m_STR[ATTR_MAX] = m_STR[ATTR_BASIC];

    m_DEX[ATTR_BASIC] = record.dex;
    m_DEX[ATTR_CURRENT] = m_DEX[ATTR_BASIC];
    m_DEX[ATTR_MAX] = m_DEX[ATTR_BASIC];

    m_INT[ATTR_BASIC] = record.inte;
    m_INT[ATTR_CURRENT] = m_INT[ATTR_BASIC];
    m_INT[ATTR_MAX] = m_INT[ATTR_BASIC];

    setHP(record.maxHP, ATTR_MAX);
    setHP(getHP(ATTR_MAX), ATTR_BASIC);
    setHP(record.currentHP, ATTR_CURRENT);

    setMP(record.maxMP, ATTR_MAX);
    setMP(getMP(ATTR_MAX), ATTR_BASIC);
    setMP(record.currentMP, ATTR_CURRENT);

    setFame(record.fame);

    //		setExp(pResult->getInt(++i));
    setGoalExp(record.goalExp);
    //		setExpOffset(pResult->getInt(++i));
    setLevel(record.level);
    setBonus(record.bonus);
    setSkillBonus(record.skillBonus);

    setGold(record.gold);
    setGuildID(record.guildID);

    ZoneID_t zoneID = record.zoneID;
    setX(record.x);
    setY(record.y);

    setSight(record.sight);

    setAlignment(record.alignment);

    setStashGold(record.stashGold);
    setStashNum(record.stashNum);

    m_Competence = record.competence;

    if (m_Competence >= 4)
        m_Competence = 3;

    m_CompetenceShape = record.competenceShape;

    setResurrectZoneID(record.resurrectZone);
    setSilverDamage(record.silverDamage);

    setSMSCharge(record.smsCharge);

    Rank_t CurRank = record.rank;
    RankExp_t RankGoalExp = record.rankGoalExp;

    m_pRank = new Rank(CurRank, RankGoalExp, RankExpTable::s_RankExpTables[RANK_TYPE_OUSTERS]);

    setHairColor(record.hairColor);

    // 2002.7.15 by sigi
    int maxHP = m_STR[ATTR_CURRENT] * 2 + m_INT[ATTR_CURRENT] + m_DEX[ATTR_CURRENT] + m_Level;
    maxHP = min((int)maxHP, OUSTERS_MAX_HP);
    setHP(maxHP, ATTR_MAX);

    try {
        setZoneID(zoneID);
    } catch (Error& e) {
        ZONE_COORD ResurrectCoord;
        de::gameContext().resurrectLocations().getOustersPosition(1311, ResurrectCoord);
        setZoneID(ResurrectCoord.id);
        setX(ResurrectCoord.x);
        setY(ResurrectCoord.y);
    }

    //----------------------------------------------------------------------
    //----------------------------------------------------------------------
    m_OustersInfo.setObjectID(m_ObjectID);
    m_OustersInfo.setName(m_Name);
    m_OustersInfo.setSex(m_Sex);
    m_OustersInfo.setHairColor(m_HairColor);
    m_OustersInfo.setMasterEffectColor(m_MasterEffectColor);

    m_OustersInfo.setCompetence(m_CompetenceShape);

    //----------------------------------------------------------------------
    //----------------------------------------------------------------------
    vector<OustersSkillRow> skillRows = defaultSkillSaveRepository().loadOustersSkills(m_Name);
    for (size_t r = 0; r < skillRows.size(); r++) {
        const OustersSkillRow& row = skillRows[r];
        SkillType_t SkillType = row.skillType;

        if (hasSkill(SkillType) == NULL) {
            OustersSkillSlot* pOustersSkillSlot = new OustersSkillSlot();

            pOustersSkillSlot->setName(m_Name);
            pOustersSkillSlot->setSkillType(SkillType);
            pOustersSkillSlot->setExpLevel(row.skillLevel);
            pOustersSkillSlot->setInterval(row.delay);
            pOustersSkillSlot->setCastingTime(row.castingTime);
            pOustersSkillSlot->setRunTime();

            addSkill(pOustersSkillSlot);
        }
    }

    //----------------------------------------------------------------------
    //----------------------------------------------------------------------
    loadRankBonus();

    //----------------------------------------------------------------------
    //----------------------------------------------------------------------
    de::gameContext().effectLoaders().load(this);

    //----------------------------------------------------------------------
    //----------------------------------------------------------------------
    // by sigi. 2002.11.8
    if (m_Level >= 100 && SystemAvailabilitiesManager::getInstance()->isAvailable(
                              SystemAvailabilitiesManager::SYSTEM_GRAND_MASTER_EFFECT)) {
        if (!isFlag(Effect::EFFECT_CLASS_GRAND_MASTER_OUSTERS)) {
            EffectGrandMasterOusters* pEffect = new EffectGrandMasterOusters(this);
            pEffect->setDeadline(999999);
            getEffectManager()->addEffect(pEffect);
            setFlag(Effect::EFFECT_CLASS_GRAND_MASTER_OUSTERS);
        }
    }

    //----------------------------------------------------------------------
    //----------------------------------------------------------------------
    m_pFlagSet->load(getName());

    //----------------------------------------------------------------------
    //----------------------------------------------------------------------
    m_OustersInfo.setCoatType(OUSTERS_COAT_BASIC);
    m_OustersInfo.setArmType(OUSTERS_ARM_GAUNTLET);
    m_OustersInfo.setSylphType(OUSTERS_SYLPH_NONE);
    m_OustersInfo.setHairColor(m_HairColor);

    m_OustersInfo.setCoatColor(377);
    m_OustersInfo.setAdvancementLevel(getAdvancementClassLevel());

    if (getRank() == 0) {
        saveInitialRank();
    }


    initAllStat();

    if (RaceWarLimiter::isInPCList(this)) {
        setFlag(Effect::EFFECT_CLASS_RACE_WAR_JOIN_TICKET);
    }

    if (m_pZone->isHolyLand() && de::gameContext().warSystem().hasActiveRaceWar() &&
        !isFlag(Effect::EFFECT_CLASS_RACE_WAR_JOIN_TICKET)) {
        ZONE_COORD ResurrectCoord;
        de::gameContext().resurrectLocations().getPosition(this, ResurrectCoord);
        setZoneID(ResurrectCoord.id);
        setX(ResurrectCoord.x);
        setY(ResurrectCoord.y);
    }

    return true;

    __END_CATCH
}

//----------------------------------------------------------------------
//----------------------------------------------------------------------
void Ousters::save() const

{
    __BEGIN_TRY

    __ENTER_CRITICAL_SECTION(m_Mutex)

    OustersVitalsRecord record;
    record.currentHP = (int)m_HP[ATTR_CURRENT];
    record.maxHP = (int)m_HP[ATTR_MAX];
    record.currentMP = (int)m_MP[ATTR_CURRENT];
    record.maxMP = (int)m_MP[ATTR_MAX];
    record.zoneID = (int)getZoneID();
    record.x = (int)m_X;
    record.y = (int)m_Y;
    defaultCharacterRepository().saveOustersVitals(m_Name, record);

    //--------------------------------------------------
    // Save the effects.
    //--------------------------------------------------
    m_pEffectManager->save(m_Name);

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    __END_CATCH
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
SkillBonus_t Ousters::getSumOfUsedSkillBonus() const

{
    __BEGIN_TRY

    return 0;

    __END_CATCH
}

////////////////////////////////////////////////////////////////////////////////
//
//
//
//
////////////////////////////////////////////////////////////////////////////////

OustersSkillSlot* Ousters::getSkill(SkillType_t SkillType) const {
    return findSkillSlot(m_SkillSlot, SkillType);
}

void Ousters::addSkill(SkillType_t SkillType)

{
    __BEGIN_TRY

    switch (SkillType) {
    case SKILL_UN_BURROW:
    case SKILL_UN_TRANSFORM:
    case SKILL_UN_INVISIBILITY:
    case SKILL_THROW_HOLY_WATER:
    case SKILL_EAT_CORPSE:
    case SKILL_HOWL:
        filelog("OustersError.log", "SkillType[%d], %s", SkillType, toString().c_str());
        Assert(false);
        break;
    default:
        break;
    }

    unordered_map<SkillType_t, OustersSkillSlot*>::iterator itr = m_SkillSlot.find(SkillType);

    if (itr == m_SkillSlot.end()) {
        OustersSkillSlot* pOustersSkillSlot = new OustersSkillSlot;

        pOustersSkillSlot->setName(m_Name);
        pOustersSkillSlot->setSkillType(SkillType);
        // A freshly learned skill starts with no run-time lock and a zero
        // interval -- the seeded MaxDelay leaked to the client as a sticky
        // per-cast cooldown via the skill-info refresh; the first
        // successful cast installs the real formula delay. See
        // Vampire::addSkill for the full story.
        pOustersSkillSlot->setRunTime(0);
        pOustersSkillSlot->setInterval(0);
        pOustersSkillSlot->setExpLevel(1);
        pOustersSkillSlot->create(m_Name);

        m_SkillSlot[SkillType] = pOustersSkillSlot;
    }

    __END_CATCH
}

void Ousters::addSkill(OustersSkillSlot* pOustersSkillSlot)

{
    __BEGIN_TRY

    SkillType_t SkillType = pOustersSkillSlot->getSkillType();
    switch (SkillType) {
    case SKILL_UN_BURROW:
    case SKILL_UN_TRANSFORM:
    case SKILL_UN_INVISIBILITY:
    case SKILL_THROW_HOLY_WATER:
    case SKILL_EAT_CORPSE:
    case SKILL_HOWL:
        filelog("OustersError.log", "SkillType[%d], %s", SkillType, toString().c_str());
        Assert(false);
        break;
    default:
        break;
    }

    unordered_map<SkillType_t, OustersSkillSlot*>::iterator itr = m_SkillSlot.find(pOustersSkillSlot->getSkillType());

    if (itr == m_SkillSlot.end()) {
        m_SkillSlot[pOustersSkillSlot->getSkillType()] = pOustersSkillSlot;
    }
    // 2002.1.16 by sigi
    else {
        delete pOustersSkillSlot;
    }

    __END_CATCH
}

void Ousters::removeSkill(SkillType_t SkillType)

{
    __BEGIN_TRY

    unordered_map<SkillType_t, OustersSkillSlot*>::iterator itr = m_SkillSlot.find(SkillType);

    if (itr != m_SkillSlot.end()) {
        OustersSkillSlot* pSkillSlot = itr->second;

        SAFE_DELETE(pSkillSlot);

        m_SkillSlot.erase(itr);
    }

    __END_CATCH
}

void Ousters::removeCastleSkill(SkillType_t SkillType) {
    removeCastleSkillSlot<OustersSkillSlot, OustersCastleSkillSlot>(m_SkillSlot, SkillType);
}

void Ousters::removeAllCastleSkill() {
    removeAllCastleSkillSlots(m_SkillSlot);
}


////////////////////////////////////////////////////////////////////////////////
//
//
//
//
////////////////////////////////////////////////////////////////////////////////

//----------------------------------------------------------------------
//
// Ousters::WearItem()
//
//
//----------------------------------------------------------------------
void Ousters::wearItem(WearPart Part, Item* pItem)

{
    __BEGIN_TRY

    Assert(pItem != NULL);

    Item* pPrevItem = NULL;
    Item* pLeft = NULL;
    Item* pRight = NULL;

    if (isTwohandWeapon(pItem)) {
        if (isWear(WEAR_RIGHTHAND) && isWear(WEAR_LEFTHAND)) {
            pLeft = getWearItem(WEAR_RIGHTHAND);
            pRight = getWearItem(WEAR_LEFTHAND);

            if (pLeft == pRight) {
                m_pWearItem[WEAR_RIGHTHAND] = pItem;
                m_pWearItem[WEAR_LEFTHAND] = pItem;

                // by sigi. 2002.5.15
                char pField[80];
                snprintf(pField, sizeof(pField), "Storage=%d, X=%d", STORAGE_GEAR, Part);
                pItem->tinysave(pField);

                addItemToExtraInventorySlot(pLeft);
                snprintf(pField, sizeof(pField), "Storage=%d", STORAGE_EXTRASLOT);
                pLeft->tinysave(pField);
            } else {
                return;
            }
        } else {
            char pField[80];

            if (isWear(WEAR_RIGHTHAND)) {
                pRight = getWearItem(WEAR_RIGHTHAND);
                m_pWearItem[WEAR_RIGHTHAND] = pItem;
                m_pWearItem[WEAR_LEFTHAND] = pItem;

                // by sigi. 2002.5.15
                snprintf(pField, sizeof(pField), "Storage=%d, X=%d", STORAGE_GEAR, Part);
                pItem->tinysave(pField);

                addItemToExtraInventorySlot(pRight);
                snprintf(pField, sizeof(pField), "Storage=%d", STORAGE_EXTRASLOT);
                pRight->tinysave(pField);
            } else if (isWear(WEAR_LEFTHAND)) {
                pLeft = getWearItem(WEAR_LEFTHAND);
                m_pWearItem[WEAR_RIGHTHAND] = pItem;
                m_pWearItem[WEAR_LEFTHAND] = pItem;

                // by sigi. 2002.5.15
                snprintf(pField, sizeof(pField), "Storage=%d, X=%d", STORAGE_GEAR, Part);
                pItem->tinysave(pField);

                addItemToExtraInventorySlot(pLeft);
                snprintf(pField, sizeof(pField), "Storage=%d", STORAGE_EXTRASLOT);
                pLeft->tinysave(pField);
            } else {
                m_pWearItem[WEAR_RIGHTHAND] = pItem;
                m_pWearItem[WEAR_LEFTHAND] = pItem;

                // by sigi. 2002.5.15
                snprintf(pField, sizeof(pField), "Storage=%d, X=%d", STORAGE_GEAR, Part);
                pItem->tinysave(pField);
            }
        }
    } else {
        if (isWear(Part)) {
            pPrevItem = getWearItem(Part);
            m_pWearItem[Part] = pItem;

            // by sigi. 2002.5.15
            char pField[80];
            snprintf(pField, sizeof(pField), "Storage=%d, X=%d", STORAGE_GEAR, Part);
            pItem->tinysave(pField);

            addItemToExtraInventorySlot(pPrevItem);
            snprintf(pField, sizeof(pField), "Storage=%d", STORAGE_EXTRASLOT);
            pPrevItem->tinysave(pField);
        } else {
            m_pWearItem[Part] = pItem;

            // by sigi. 2002.5.15
            char pField[80];
            snprintf(pField, sizeof(pField), "Storage=%d, X=%d", STORAGE_GEAR, Part);
            pItem->tinysave(pField);
        }
    }

    switch (pItem->getItemClass()) {
    case Item::ITEM_CLASS_OUSTERS_COAT:
        m_OustersInfo.setCoatType(getOustersCoatType(pItem->getItemType()));
        m_OustersInfo.setCoatColor(getItemShapeColor(pItem));
        break;
    case Item::ITEM_CLASS_OUSTERS_CHAKRAM:
        m_OustersInfo.setArmType(OUSTERS_ARM_CHAKRAM);
        m_OustersInfo.setArmColor(getItemShapeColor(pItem));
        break;
    case Item::ITEM_CLASS_OUSTERS_WRISTLET:
        m_OustersInfo.setArmType(OUSTERS_ARM_GAUNTLET);
        m_OustersInfo.setArmColor(getItemShapeColor(pItem));
        break;
    case Item::ITEM_CLASS_OUSTERS_BOOTS:
        m_OustersInfo.setBootsColor(getItemShapeColor(pItem));
        break;
    default:
        break;
    }

    __END_CATCH
}


//----------------------------------------------------------------------
// Ousters::WearItem()
//----------------------------------------------------------------------
void Ousters::wearItem(WearPart Part)

{
    __BEGIN_TRY

    Item* pItem = getExtraInventorySlotItem();
    Assert(pItem != NULL);

    Item* pPrevItem = NULL;
    Item* pLeft = NULL;
    Item* pRight = NULL;

    OUSTERS_RECORD prev;
    getOustersRecord(prev);

    char pField[80];

    if (isTwohandWeapon(pItem)) {
        if (isWear(WEAR_RIGHTHAND) && isWear(WEAR_LEFTHAND)) {
            pLeft = getWearItem(WEAR_RIGHTHAND);
            pRight = getWearItem(WEAR_LEFTHAND);

            if (pLeft == pRight) {
                takeOffItem(WEAR_LEFTHAND, false, false);

                m_pWearItem[WEAR_RIGHTHAND] = pItem;
                m_pWearItem[WEAR_LEFTHAND] = pItem;
                // by sigi. 2002.5.15
                snprintf(pField, sizeof(pField), "Storage=%d, X=%d", STORAGE_GEAR, Part);
                pItem->tinysave(pField);

                deleteItemFromExtraInventorySlot();
                addItemToExtraInventorySlot(pLeft);
                snprintf(pField, sizeof(pField), "Storage=%d", STORAGE_EXTRASLOT);
                pLeft->tinysave(pField);

            } else {
                return;
            }
        } else {
            // by sigi. 2002.5.15
            if (isWear(WEAR_RIGHTHAND)) {
                pRight = getWearItem(WEAR_RIGHTHAND);

                takeOffItem(WEAR_RIGHTHAND, false, false);

                m_pWearItem[WEAR_RIGHTHAND] = pItem;
                m_pWearItem[WEAR_LEFTHAND] = pItem;

                // by sigi. 2002.5.15
                snprintf(pField, sizeof(pField), "Storage=%d, X=%d", STORAGE_GEAR, Part);
                pItem->tinysave(pField);

                deleteItemFromExtraInventorySlot();
                addItemToExtraInventorySlot(pRight);
                snprintf(pField, sizeof(pField), "Storage=%d", STORAGE_EXTRASLOT);
                pRight->tinysave(pField);

            } else if (isWear(WEAR_LEFTHAND)) {
                pLeft = getWearItem(WEAR_LEFTHAND);

                takeOffItem(WEAR_LEFTHAND, false, false);

                m_pWearItem[WEAR_RIGHTHAND] = pItem;
                m_pWearItem[WEAR_LEFTHAND] = pItem;

                // by sigi. 2002.5.15
                snprintf(pField, sizeof(pField), "Storage=%d, X=%d", STORAGE_GEAR, Part);
                pItem->tinysave(pField);

                deleteItemFromExtraInventorySlot();
                addItemToExtraInventorySlot(pLeft);
                snprintf(pField, sizeof(pField), "Storage=%d", STORAGE_EXTRASLOT);
                pLeft->tinysave(pField);
            } else {
                m_pWearItem[WEAR_RIGHTHAND] = pItem;
                m_pWearItem[WEAR_LEFTHAND] = pItem;

                pItem->save(m_Name, STORAGE_GEAR, 0, Part, 0);
                deleteItemFromExtraInventorySlot();
            }
        }
    } else {
        if (isWear(Part)) {
            pPrevItem = getWearItem(Part);
            takeOffItem(Part, false, false);
            m_pWearItem[Part] = pItem;

            // by sigi. 2002.5.15
            snprintf(pField, sizeof(pField), "Storage=%d, X=%d", STORAGE_GEAR, Part);
            pItem->tinysave(pField);

            deleteItemFromExtraInventorySlot();
            addItemToExtraInventorySlot(pPrevItem);

            snprintf(pField, sizeof(pField), "Storage=%d", STORAGE_EXTRASLOT);
            pPrevItem->tinysave(pField);
        } else {
            m_pWearItem[Part] = pItem;
            deleteItemFromExtraInventorySlot();

            // by sigi. 2002.5.15
            snprintf(pField, sizeof(pField), "Storage=%d, X=%d", STORAGE_GEAR, Part);
            pItem->tinysave(pField);
        }
    }

    initAllStat();
    sendRealWearingInfo();
    sendModifyInfo(prev);

    if (m_pRealWearingCheck[Part]) {
        if (pItem->getItemClass() == Item::ITEM_CLASS_OUSTERS_COAT ||
            pItem->getItemClass() == Item::ITEM_CLASS_OUSTERS_WRISTLET ||
            pItem->getItemClass() == Item::ITEM_CLASS_OUSTERS_CHAKRAM ||
            pItem->getItemClass() == Item::ITEM_CLASS_OUSTERS_BOOTS) {
            Color_t color = getItemShapeColor(pItem);

            GCChangeShape pkt;
            pkt.setObjectID(getObjectID());
            pkt.setItemClass(pItem->getItemClass());
            pkt.setItemType(pItem->getItemType());
            pkt.setOptionType(pItem->getFirstOptionType());
            pkt.setAttackSpeed(m_AttackSpeed[ATTR_CURRENT]);

            if (color == QUEST_COLOR)
                pkt.setFlag(SHAPE_FLAG_QUEST);

            Zone* pZone = getZone();
            pZone->broadcastPacket(m_X, m_Y, &pkt, this);

            switch (pItem->getItemClass()) {
            case Item::ITEM_CLASS_OUSTERS_COAT:
                m_OustersInfo.setCoatType(getOustersCoatType(pItem->getItemType()));
                m_OustersInfo.setCoatColor(color);
                break;
            case Item::ITEM_CLASS_OUSTERS_CHAKRAM:
                m_OustersInfo.setArmType(OUSTERS_ARM_CHAKRAM);
                m_OustersInfo.setArmColor(color);
                break;
            case Item::ITEM_CLASS_OUSTERS_WRISTLET:
                m_OustersInfo.setArmType(OUSTERS_ARM_GAUNTLET);
                m_OustersInfo.setArmColor(color);
                break;
            case Item::ITEM_CLASS_OUSTERS_BOOTS:
                m_OustersInfo.setBootsColor(color);
                break;
            default:
                break;
            }
        }
    }

    if (m_pZone != NULL) {
        GCOtherModifyInfo gcOtherModifyInfo;
        makeGCOtherModifyInfo(&gcOtherModifyInfo, this, &prev);

        if (gcOtherModifyInfo.getShortCount() != 0 || gcOtherModifyInfo.getLongCount() != 0) {
            m_pZone->broadcastPacket(m_X, m_Y, &gcOtherModifyInfo, this);
        }
    }

    __END_CATCH
}


//----------------------------------------------------------------------
//
// Ousters::takeOffItem()
//
//----------------------------------------------------------------------
void Ousters::takeOffItem(WearPart Part, bool bAddOnMouse, bool bSendModifyInfo)

{
    __BEGIN_TRY

    OUSTERS_RECORD prev;

    Item* pItem = m_pWearItem[Part];
    Assert(pItem != NULL);

    // Item::ItemClass IClass = pItem->getItemClass();

    if (Part == WEAR_LEFTHAND || Part == WEAR_RIGHTHAND) {
        if (m_pWearItem[WEAR_RIGHTHAND] && m_pWearItem[WEAR_LEFTHAND]) {
            if (m_pWearItem[WEAR_RIGHTHAND] == m_pWearItem[WEAR_LEFTHAND]) {
                m_pWearItem[WEAR_RIGHTHAND] = NULL;
                m_pWearItem[WEAR_LEFTHAND] = NULL;
            }
        }
    }

    if (isTwohandWeapon(pItem)) {
        m_pWearItem[WEAR_RIGHTHAND] = NULL;
        m_pWearItem[WEAR_LEFTHAND] = NULL;
    } else
        m_pWearItem[Part] = NULL;

    if (bSendModifyInfo) {
        getOustersRecord(prev);
        initAllStat();
        sendRealWearingInfo();
        sendModifyInfo(prev);
    } else {
        initAllStat();
    }

    //---------------------------------------------
    //---------------------------------------------
    if (bAddOnMouse) {
        addItemToExtraInventorySlot(pItem);
        char pField[80];
        snprintf(pField, sizeof(pField), "Storage=%d, Durability=%d", STORAGE_EXTRASLOT, pItem->getDurability());
        pItem->tinysave(pField);
    }

    switch (pItem->getItemClass()) {
    case Item::ITEM_CLASS_OUSTERS_COAT: {
        m_OustersInfo.setCoatType(OUSTERS_COAT_BASIC);
        m_OustersInfo.setCoatColor(377);
        GCTakeOff pkt;
        pkt.setObjectID(getObjectID());
        pkt.setSlotID((SlotID_t)ADDON_COAT);
        m_pZone->broadcastPacket(getX(), getY(), &pkt, this);
    } break;
    case Item::ITEM_CLASS_OUSTERS_CHAKRAM: {
        m_OustersInfo.setArmType(OUSTERS_ARM_GAUNTLET);
        m_OustersInfo.setArmColor(377);
        GCTakeOff pkt;
        pkt.setObjectID(getObjectID());
        pkt.setSlotID((SlotID_t)ADDON_LEFTHAND);
        m_pZone->broadcastPacket(getX(), getY(), &pkt, this);
    } break;
    case Item::ITEM_CLASS_OUSTERS_WRISTLET: {
        m_OustersInfo.setArmType(OUSTERS_ARM_GAUNTLET);
        m_OustersInfo.setArmColor(377);
        GCTakeOff pkt;
        pkt.setObjectID(getObjectID());
        pkt.setSlotID((SlotID_t)ADDON_LEFTHAND);
        m_pZone->broadcastPacket(getX(), getY(), &pkt, this);
    } break;
    case Item::ITEM_CLASS_OUSTERS_BOOTS: {
        m_OustersInfo.setBootsColor(377);
        GCTakeOff pkt;
        pkt.setObjectID(getObjectID());
        pkt.setSlotID((SlotID_t)ADDON_TROUSER);
        m_pZone->broadcastPacket(getX(), getY(), &pkt, this);
    } break;
    default:
        break;
    }

    if (m_pZone != NULL) {
        GCOtherModifyInfo gcOtherModifyInfo;
        makeGCOtherModifyInfo(&gcOtherModifyInfo, this, &prev);

        if (gcOtherModifyInfo.getShortCount() != 0 || gcOtherModifyInfo.getLongCount() != 0) {
            m_pZone->broadcastPacket(m_X, m_Y, &gcOtherModifyInfo, this);
        }
    }

    __END_CATCH
}


//----------------------------------------------------------------------
// destroyGears
//----------------------------------------------------------------------
void Ousters::destroyGears()

{
    __BEGIN_TRY
    __BEGIN_DEBUG

    for (int j = 0; j < OUSTERS_WEAR_MAX; j++) {
        Item* pItem = m_pWearItem[j];
        if (pItem != NULL) {
            Item::ItemClass IClass = pItem->getItemClass();

            //-------------------------------------------------------------
            //-------------------------------------------------------------
            Assert(IClass != Item::ITEM_CLASS_AR);
            Assert(IClass != Item::ITEM_CLASS_SR);
            Assert(IClass != Item::ITEM_CLASS_SG);
            Assert(IClass != Item::ITEM_CLASS_SMG);
            Assert(IClass != Item::ITEM_CLASS_SWORD);
            Assert(IClass != Item::ITEM_CLASS_BLADE);
            Assert(IClass != Item::ITEM_CLASS_SHIELD);
            Assert(IClass != Item::ITEM_CLASS_CROSS);
            Assert(IClass != Item::ITEM_CLASS_MACE);
            Assert(IClass != Item::ITEM_CLASS_HELM);
            Assert(IClass != Item::ITEM_CLASS_GLOVE);
            Assert(IClass != Item::ITEM_CLASS_TROUSER);
            Assert(IClass != Item::ITEM_CLASS_COAT);

            if (isTwohandWeapon(pItem)) {
                m_pWearItem[WEAR_RIGHTHAND] = NULL;
                m_pWearItem[WEAR_LEFTHAND] = NULL;
            } else
                m_pWearItem[j] = NULL;

            SAFE_DELETE(pItem);
        }
    }

    __END_DEBUG
    __END_CATCH
}

//----------------------------------------------------------------------
//----------------------------------------------------------------------
bool Ousters::isRealWearing(WearPart part) const

{
    __BEGIN_TRY

    if (part >= OUSTERS_WEAR_MAX)
        throw Error("Ousters::isRealWearing() : invalid wear point!");

    if (m_pWearItem[part] == NULL)
        return false;
    if (part >= WEAR_ZAP1 && part <= WEAR_ZAP4) {
        if (m_pWearItem[part - WEAR_ZAP1 + WEAR_STONE1] == NULL)
            return false;
    }

    return isRealWearing(m_pWearItem[part]);

    __END_CATCH
}

//----------------------------------------------------------------------
//----------------------------------------------------------------------
bool Ousters::isRealWearing(Item* pItem) const

{
    __BEGIN_TRY

    if (pItem == NULL)
        return false;

    if (pItem->isTimeLimitItem()) {
        return true;
    }

    if (getZone()->isPremiumZone() && (pItem->isUnique() || pItem->getOptionTypeSize() > 1))
    // pItem->getItemClass() == Item::ITEM_CLASS_COUPLE_RING || pItem->getItemClass() ==
    // Item::ITEM_CLASS_OUSTERS_COUPLE_RING))
    {
        GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(getPlayer());
        if (!pGamePlayer->isPayPlaying() && !pGamePlayer->isPremiumPlay()) {
            return false;
        }
    }

    Item::ItemClass IClass = pItem->getItemClass();
    ItemInfo* pItemInfo = de::gameContext().itemInfos().getItemInfo(IClass, pItem->getItemType());

    Level_t ReqAdvancedLevel = pItemInfo->getReqAdvancedLevel();
    if (ReqAdvancedLevel > 0 && (!isAdvanced() || getAdvancementClassLevel() < ReqAdvancedLevel))
        return false;

    if (pItem->getItemClass() == Item::ITEM_CLASS_OUSTERS_COAT ||
        pItem->getItemClass() == Item::ITEM_CLASS_OUSTERS_BOOTS || isOustersWeapon(pItem->getItemClass())) {
        if (ReqAdvancedLevel <= 0 && isAdvanced())
            return false;
    }

    Level_t ReqLevel = pItemInfo->getReqLevel();
    Attr_t ReqSTR = pItemInfo->getReqSTR();
    Attr_t ReqDEX = pItemInfo->getReqDEX();
    Attr_t ReqINT = pItemInfo->getReqINT();
    Attr_t ReqSum = pItemInfo->getReqSum();

    const list<OptionType_t>& optionTypes = pItem->getOptionTypeList();
    list<OptionType_t>::const_iterator itr;

    for (itr = optionTypes.begin(); itr != optionTypes.end(); itr++) {
        OptionInfo* pOptionInfo = de::gameContext().optionInfos().getOptionInfo(*itr);
        if (ReqLevel != 0)
            ReqLevel += pOptionInfo->getReqLevel();
        if (ReqSTR != 0)
            ReqSTR += (pOptionInfo->getReqSum() * 2);
        if (ReqDEX != 0)
            ReqDEX += (pOptionInfo->getReqSum() * 2);
        if (ReqINT != 0)
            ReqINT += (pOptionInfo->getReqSum() * 2);
        if (ReqSum != 0)
            ReqSum += pOptionInfo->getReqSum();
    }

    ReqLevel = min(ReqLevel, MAX_OUSTERS_LEVEL);

    Attr_t CSTR = m_STR[ATTR_CURRENT];
    Attr_t CDEX = m_DEX[ATTR_CURRENT];
    Attr_t CINT = m_INT[ATTR_CURRENT];
    Attr_t CSUM = CSTR + CDEX + CINT;

    if (CSTR < ReqSTR || CDEX < ReqDEX || CINT < ReqINT || CSUM < ReqSum || m_Level < ReqLevel) {
        return false;
    }

    return true;

    __END_CATCH
}

//----------------------------------------------------------------------
//----------------------------------------------------------------------
bool Ousters::isRealWearingEx(WearPart part) const {
    if (part >= OUSTERS_WEAR_MAX)
        return false;
    return m_pRealWearingCheck[part];
}

DWORD Ousters::sendRealWearingInfo(void) const

{
    __BEGIN_TRY

    DWORD info = 0;
    DWORD flag = 1;

    for (int i = 0; i < OUSTERS_WEAR_MAX; i++) {
        if (isRealWearing((Ousters::WearPart)i))
            info |= flag;
        flag <<= 1;
    }

    GCRealWearingInfo pkt;
    pkt.setInfo(info);
    m_pPlayer->sendPacket(&pkt);

    return info;

    __END_CATCH
}

////////////////////////////////////////////////////////////////////////////////
//
//
//
//
////////////////////////////////////////////////////////////////////////////////

PCOustersInfo2* Ousters::getOustersInfo2()

{
    __BEGIN_TRY
    __BEGIN_DEBUG

    PCOustersInfo2* pInfo = new PCOustersInfo2();

    pInfo->setObjectID(m_ObjectID);
    pInfo->setName(m_Name);
    pInfo->setLevel(m_Level);
    pInfo->setSex(m_Sex);
    pInfo->setHairColor(m_HairColor);
    pInfo->setMasterEffectColor(m_MasterEffectColor);

    pInfo->setAlignment(m_Alignment);

    pInfo->setSTR(m_STR[ATTR_CURRENT], ATTR_CURRENT);
    pInfo->setSTR(m_STR[ATTR_MAX], ATTR_MAX);
    pInfo->setSTR(m_STR[ATTR_BASIC], ATTR_BASIC);
    pInfo->setDEX(m_DEX[ATTR_CURRENT], ATTR_CURRENT);
    pInfo->setDEX(m_DEX[ATTR_MAX], ATTR_MAX);
    pInfo->setDEX(m_DEX[ATTR_BASIC], ATTR_BASIC);
    pInfo->setINT(m_INT[ATTR_CURRENT], ATTR_CURRENT);
    pInfo->setINT(m_INT[ATTR_MAX], ATTR_MAX);
    pInfo->setINT(m_INT[ATTR_BASIC], ATTR_BASIC);

    pInfo->setHP(m_HP[ATTR_CURRENT], m_HP[ATTR_MAX]);
    pInfo->setMP(m_MP[ATTR_CURRENT], m_MP[ATTR_MAX]);
    pInfo->setSilverDamage(m_SilverDamage);

    pInfo->setFame(m_Fame);
    pInfo->setExp(m_GoalExp);
    pInfo->setGold(m_Gold);
    pInfo->setSight(m_Sight);
    pInfo->setBonus(m_Bonus);
    pInfo->setSkillBonus(m_SkillBonus);

    // by sigi. 2002.8.30
    pInfo->setRank(getRank());
    pInfo->setRankExp(getRankGoalExp());

    pInfo->setCompetence(m_CompetenceShape);
    pInfo->setGuildID(m_GuildID);
    pInfo->setGuildName(getGuildName());
    pInfo->setGuildMemberRank(getGuildMemberRank());

    GuildUnion* pUnion = GuildUnionManager::Instance().getGuildUnion(m_GuildID);
    if (pUnion == NULL)
        pInfo->setUnionID(0);
    else
        pInfo->setUnionID(pUnion->getUnionID());

    pInfo->setAdvancementLevel(getAdvancementClassLevel());
    pInfo->setAdvancementGoalExp(getAdvancementClassGoalExp());

    return pInfo;

    __END_DEBUG
    __END_CATCH
}


//----------------------------------------------------------------------
// Ousters Outlook Information
//----------------------------------------------------------------------
PCOustersInfo3 Ousters::getOustersInfo3() const

{
    __BEGIN_TRY
    __BEGIN_DEBUG

    m_OustersInfo.setObjectID(m_ObjectID);
    m_OustersInfo.setX(m_X);
    m_OustersInfo.setY(m_Y);
    m_OustersInfo.setDir(m_Dir);
    m_OustersInfo.setCurrentHP(m_HP[ATTR_CURRENT]);
    m_OustersInfo.setMaxHP(m_HP[ATTR_MAX]);
    m_OustersInfo.setAttackSpeed(m_AttackSpeed[ATTR_CURRENT]);
    m_OustersInfo.setAlignment(m_Alignment);
    m_OustersInfo.setGuildID(m_GuildID);

    // by sigi. 2002.9.10
    m_OustersInfo.setRank(getRank());

    m_OustersInfo.setHairColor(m_HairColor);
    m_OustersInfo.setMasterEffectColor(m_MasterEffectColor);

    GuildUnion* pUnion = GuildUnionManager::Instance().getGuildUnion(m_GuildID);
    if (pUnion == NULL)
        m_OustersInfo.setUnionID(0);
    else
        m_OustersInfo.setUnionID(pUnion->getUnionID());

    m_OustersInfo.setAdvancementLevel(getAdvancementClassLevel());

    return m_OustersInfo;

    __END_DEBUG
    __END_CATCH
}

//----------------------------------------------------------------------
//
// get Gear Info
//
//----------------------------------------------------------------------
GearInfo* Ousters::getGearInfo() const

{
    __BEGIN_TRY
    __BEGIN_DEBUG

    GearInfo* pGearInfo = new GearInfo();

    for (int i = 0; i < OUSTERS_WEAR_MAX; i++) {
        Item* pItem = m_pWearItem[i];

        if (pItem != NULL) {
            // Item::ItemClass IClass = pItem->getItemClass();

            GearSlotInfo* pGearSlotInfo = new GearSlotInfo();
            pItem->makePCItemInfo(*pGearSlotInfo);
            pGearSlotInfo->setSlotID(i);

            pGearInfo->addListElement(pGearSlotInfo);
        }
    }

    return pGearInfo;

    __END_DEBUG
    __END_CATCH
}

//----------------------------------------------------------------------
// getSkillInfo
//----------------------------------------------------------------------
void Ousters::sendOustersSkillInfo()

{
    __BEGIN_TRY
    __BEGIN_DEBUG

    OustersSkillInfo* pOustersSkillInfo = new OustersSkillInfo();

    Timeval currentTime;
    getCurrentTime(currentTime);

    unordered_map<SkillType_t, OustersSkillSlot*>::const_iterator itr = m_SkillSlot.begin();
    for (; itr != m_SkillSlot.end(); itr++) {
        OustersSkillSlot* pOustersSkillSlot = itr->second;
        Assert(pOustersSkillSlot != NULL);

        if (pOustersSkillSlot->getSkillType() >= SKILL_DOUBLE_IMPACT) {
            SubOustersSkillInfo* pSubOustersSkillInfo = new SubOustersSkillInfo();
            pSubOustersSkillInfo->setSkillType(pOustersSkillSlot->getSkillType());
            pSubOustersSkillInfo->setExpLevel(pOustersSkillSlot->getExpLevel());
            pSubOustersSkillInfo->setSkillTurn(pOustersSkillSlot->getInterval());
            pSubOustersSkillInfo->setCastingTime(pOustersSkillSlot->getRemainTurn(currentTime));

            pOustersSkillInfo->addListElement(pSubOustersSkillInfo);
        }
    }

    GCSkillInfo gcSkillInfo;
    gcSkillInfo.setPCType(PC_OUSTERS);
    SkillType_t LearnSkillType = de::gameContext().skillInfos().getSkillTypeByLevel(SKILL_DOMAIN_OUSTERS, m_Level);

    if (LearnSkillType != 0) {
        if (hasSkill(LearnSkillType) == NULL) {
            pOustersSkillInfo->setLearnNewSkill(true);
        }
    }

    gcSkillInfo.addListElement(pOustersSkillInfo);

    m_pPlayer->sendPacket(&gcSkillInfo);

    __END_DEBUG
    __END_CATCH
}


////////////////////////////////////////////////////////////////////////////////
//
//
//
//
////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void Ousters::heartbeat(const Timeval& currentTime)

{
    __BEGIN_TRY
    __BEGIN_DEBUG

    PlayerCreature::heartbeat(currentTime);

    OustersSkillSlot* pDriftingSoul = hasSkill(SKILL_DRIFTING_SOUL);

    if (pDriftingSoul != NULL && m_MPRegenTime < currentTime && !isFlag(Effect::EFFECT_CLASS_PLEASURE_EXPLOSION)) {
        SkillLevel_t level = pDriftingSoul->getExpLevel();
        int bonus = 0;

        if (getMP() < getMP(ATTR_MAX)) {
            if (level <= 15)
                bonus += 1;
            else if (level <= 25)
                bonus += 2;
            else if (level <= 29)
                bonus += 3;
            else if (level <= 30)
                bonus += 4;

            MP_t oldMP = getMP();
            MP_t newMP = min((int)getMP(ATTR_MAX), oldMP + bonus);

            if (oldMP != newMP) {
                setMP(newMP);

                GCModifyInformation gcMI;
                gcMI.addShortData(MODIFY_CURRENT_MP, newMP);
                m_pPlayer->sendPacket(&gcMI);
            }
        }

        m_MPRegenTime.tv_sec = currentTime.tv_sec + 2;
        m_MPRegenTime.tv_usec = currentTime.tv_usec;
    }

    __END_DEBUG
    __END_CATCH
}

void Ousters::getOustersRecord(OUSTERS_RECORD& record) const

{
    __BEGIN_TRY

    record.pSTR[0] = m_STR[0];
    record.pSTR[1] = m_STR[1];
    record.pSTR[2] = m_STR[2];

    record.pDEX[0] = m_DEX[0];
    record.pDEX[1] = m_DEX[1];
    record.pDEX[2] = m_DEX[2];

    record.pINT[0] = m_INT[0];
    record.pINT[1] = m_INT[1];
    record.pINT[2] = m_INT[2];

    record.pHP[0] = m_HP[0];
    record.pHP[1] = m_HP[1];

    record.pMP[0] = m_MP[0];
    record.pMP[1] = m_MP[1];

    record.pDamage[0] = m_Damage[0];
    record.pDamage[1] = m_Damage[1];

    record.Rank = getRank();

    record.Defense = m_Defense[0];
    record.ToHit = m_ToHit[0];
    record.Protection = m_Protection[0];
    record.AttackSpeed = m_AttackSpeed[0];

    __END_CATCH
}

//----------------------------------------------------------------------
// get debug string
//----------------------------------------------------------------------
string Ousters::toString() const

{
    __BEGIN_TRY

    StringStream msg;

    msg << "Ousters("
        //<< "ObjectID:"   << (int)getObjectID()
        << ",Name:" << m_Name << ",HairColor:" << (int)m_HairColor << ",STR:" << (int)m_STR[ATTR_CURRENT] << "/"
        << (int)m_STR[ATTR_MAX] << ",DEX:" << (int)m_DEX[ATTR_CURRENT] << "/" << (int)m_DEX[ATTR_MAX]
        << ",INT:" << (int)m_INT[ATTR_CURRENT] << "/" << (int)m_INT[ATTR_MAX] << ",HP:" << (int)m_HP[ATTR_CURRENT]
        << "/" << (int)m_HP[ATTR_MAX] << ",MP:" << (int)m_MP[ATTR_CURRENT] << "/" << (int)m_MP[ATTR_MAX] << ",Fame:"
        << (int)m_Fame
        //		<< ",Exp:"       << (int)m_Exp
        //		<< ",ExpOffset:" << (int)m_ExpOffset
        << ",Rank:" << (int)getRank() << ",RankGoalExp:" << (int)getRankGoalExp() << ",Level:" << (int)m_Level
        << ",Bonus:" << (int)m_Bonus << ",Gold:" << (int)m_Gold << ",ZoneID:" << (int)getZoneID()
        << ",XCoord:" << (int)m_X << ",YCoord:" << (int)m_Y << ",Sight:" << (int)m_Sight << ")";

    return msg.toString();

    __END_CATCH
}

void Ousters::saveSkills(void) const {
    saveSkillSlots(m_SkillSlot);
}

Sight_t Ousters::getEffectedSight() {
    __BEGIN_TRY

    return Creature::getEffectedSight();

    __END_CATCH
}

void Ousters::saveGears(void) const

{
    __BEGIN_TRY

    char pField[80];

    for (int i = 0; i < Ousters::OUSTERS_WEAR_MAX; i++) {
        Item* pItem = m_pWearItem[i];
        if (pItem != NULL) {
            Durability_t maxDurability = computeMaxDurability(pItem);
            if (pItem->getDurability() < maxDurability) {
                // pItem->save(m_Name, STORAGE_GEAR, 0, i, 0);
                snprintf(pField, sizeof(pField), "Durability=%d", pItem->getDurability());
                pItem->tinysave(pField);
            }
        }
    }

    __END_CATCH
}


//----------------------------------------------------------------------
// getShapeInfo
//----------------------------------------------------------------------
//----------------------------------------------------------------------
//
//
//----------------------------------------------------------------------


bool Ousters::addShape(Item::ItemClass IClass, ItemType_t IType, Color_t color) {
    bool bisChange = false;

    switch (IClass) {
    default:
        break;
    }

    return bisChange;
}


bool Ousters::removeShape(Item::ItemClass IClass, bool bSendPacket) {
    bool bisChange = false;

    switch (IClass) {
    default:
        return false;
    }

    return bisChange;
}

bool Ousters::satisfySkillRequire(SkillInfo* pSkillInfo) {
    if (isFlag(Effect::EFFECT_CLASS_SUMMON_SYLPH))
        return false;
    if (pSkillInfo->getRequireFire() > m_ElementalFire)
        return false;
    if (pSkillInfo->getRequireWater() > m_ElementalWater)
        return false;
    if (pSkillInfo->getRequireEarth() > m_ElementalEarth)
        return false;
    if (pSkillInfo->getRequireWind() > m_ElementalWind)
        return false;
    if (pSkillInfo->getRequireSum() > getElementalSum())
        return false;

    if (pSkillInfo->getRequireWristletElemental() != ELEMENTAL_ANY) {
        if (!isRealWearing(WEAR_LEFTHAND) ||
            m_pWearItem[WEAR_LEFTHAND]->getItemClass() != Item::ITEM_CLASS_OUSTERS_WRISTLET)
            return false;

        OustersWristlet* pWristlet = dynamic_cast<OustersWristlet*>(m_pWearItem[WEAR_LEFTHAND]);
        Assert(pWristlet != NULL);

        if (pSkillInfo->getRequireWristletElemental() != pWristlet->getElementalType())
            return false;
    }

    if (pSkillInfo->getRequireStone1Elemental() != ELEMENTAL_ANY) {
        if (!isRealWearing(WEAR_STONE1) || m_pWearItem[WEAR_STONE1]->getItemClass() != Item::ITEM_CLASS_OUSTERS_STONE)
            return false;

        OustersStone* pStone = dynamic_cast<OustersStone*>(m_pWearItem[WEAR_STONE1]);
        Assert(pStone != NULL);

        if (pSkillInfo->getRequireStone1Elemental() != pStone->getElementalType())
            return false;
    }

    if (pSkillInfo->getRequireStone2Elemental() != ELEMENTAL_ANY) {
        if (!isRealWearing(WEAR_STONE2) || m_pWearItem[WEAR_STONE2]->getItemClass() != Item::ITEM_CLASS_OUSTERS_STONE)
            return false;

        OustersStone* pStone = dynamic_cast<OustersStone*>(m_pWearItem[WEAR_STONE2]);
        Assert(pStone != NULL);

        if (pSkillInfo->getRequireStone2Elemental() != pStone->getElementalType())
            return false;
    }

    if (pSkillInfo->getRequireStone3Elemental() != ELEMENTAL_ANY) {
        if (!isRealWearing(WEAR_STONE3) || m_pWearItem[WEAR_STONE3]->getItemClass() != Item::ITEM_CLASS_OUSTERS_STONE)
            return false;

        OustersStone* pStone = dynamic_cast<OustersStone*>(m_pWearItem[WEAR_STONE3]);
        Assert(pStone != NULL);

        if (pSkillInfo->getRequireStone3Elemental() != pStone->getElementalType())
            return false;
    }

    if (pSkillInfo->getRequireStone4Elemental() != ELEMENTAL_ANY) {
        if (!isRealWearing(WEAR_STONE4) || m_pWearItem[WEAR_STONE4]->getItemClass() != Item::ITEM_CLASS_OUSTERS_STONE)
            return false;

        OustersStone* pStone = dynamic_cast<OustersStone*>(m_pWearItem[WEAR_STONE4]);
        Assert(pStone != NULL);

        if (pSkillInfo->getRequireStone4Elemental() != pStone->getElementalType())
            return false;
    }

    return true;
}

SkillBonus_t Ousters::getSkillPointCount(ElementalDomain eDomain) {
    unordered_map<SkillType_t, OustersSkillSlot*>::iterator itr = m_SkillSlot.begin();
    SkillBonus_t ret = 0;

    for (; itr != m_SkillSlot.end(); ++itr) {
        OustersSkillSlot* pSlot = itr->second;
        if (pSlot->getSkillType() < SKILL_DOUBLE_IMPACT)
            continue;
        SkillInfo* pInfo = de::gameContext().skillInfos().getSkillInfo(pSlot->getSkillType());
        if (pInfo == NULL)
            continue;
        if (pInfo->getElementalDomain() == eDomain) {
            ret += (pInfo->getSkillPoint() + pInfo->getLevelUpPoint() * (pSlot->getExpLevel() - 1));
        }
    }

    cout << "Elemental domain " << static_cast<int>(eDomain) << " points: " << ret << endl;

    return ret;
}

void Ousters::initPetQuestTarget() {
    int minClass = 1, maxClass = 1;

    if (getLevel() <= 50) {
        minClass = 6;
        maxClass = 7;
    } else if (getLevel() <= 60) {
        minClass = 7;
        maxClass = 8;
    } else if (getLevel() <= 70) {
        minClass = maxClass = 9;
    } else if (getLevel() <= 80) {
        minClass = 9;
        maxClass = 10;
    } else if (getLevel() <= 90) {
        minClass = maxClass = 10;
    } else if (getLevel() <= 110) {
        minClass = 10;
        maxClass = 11;
    } else if (getLevel() <= 130) {
        minClass = 11;
        maxClass = 12;
    } else {
        minClass = 12;
        maxClass = 13;
    }

    m_TargetMonster = de::gameContext().monsterInfos().getRandomMonsterByClass(minClass, maxClass);
    m_TargetNum = 80;
    m_TimeLimit = 3600;
}

bool Ousters::canLearnSkill(SkillType_t skill) {
    SkillInfo* pSkillInfo = de::gameContext().skillInfos().getSkillInfo(skill);
    if (pSkillInfo == NULL)
        return false;

    if (pSkillInfo->getLevel() >= 150) {
        unordered_map<SkillType_t, OustersSkillSlot*>::iterator itr = m_SkillSlot.begin();

        for (; itr != m_SkillSlot.end(); ++itr) {
            if (itr->first < SKILL_DOUBLE_IMPACT)
                continue;
            SkillInfo* pHasSkillInfo = de::gameContext().skillInfos().getSkillInfo(itr->first);
            if (pHasSkillInfo == NULL)
                continue;
            // if ( pHasSkillInfo->getLevel() == pSkillInfo->getLevel() && !(pSkillInfo->getType() > 400 )) return
            // false;
            if ((skill >= 370 && skill <= 373) &&
                (pHasSkillInfo->getType() >= 370 && pHasSkillInfo->getType() <= 373)) {
                return false;
            }
        }
    }

    return true;
}
