//////////////////////////////////////////////////////////////////////////////
// Filename    : Vampire.cpp
// Written By  : Elca
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "Vampire.h"

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
#include "PacketUtil.h"
#include "Party.h"
#include "Player.h"
#include "Shape.h"
#include "SkillInfo.h"
#include "SkillParentInfo.h"
#include "SkillUtil.h"
#include "Stash.h"
#include "TradeManager.h"
#include "VampEXPInfo.h"
#include "repository/CharacterRepository.h"
#include "repository/GoldRepository.h"
#include "repository/SkillSaveRepository.h"
#include "repository/StashRepository.h"
// #include "RankEXPInfo.h"
#include <stdio.h>

#include "AdvancementClassExpTable.h"
#include "CastleSkillInfo.h"
#include "DynamicZone.h"
#include "EffectGrandMasterVampire.h"
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
#include "PetInfo.h"
#include "RaceWarLimiter.h"
#include "RankExpTable.h"
#include "ResurrectLocationManager.h"
#include "Store.h"
#include "SystemAvailabilitiesManager.h"
#include "TimeLimitItemManager.h"
#include "VariableManager.h"
#include "WarSystem.h"
#include "domain/EquipRequirement.h"
#include "item/AR.h"
#include "item/Belt.h"
#include "item/PetItem.h"
#include "item/SG.h"
#include "item/SMG.h"
#include "item/SR.h"
#include "item/Skull.h"
#include "skill/EffectBless.h"
#include "skill/EffectDoom.h"
#include "skill/EffectParalyze.h"
#include "skill/EffectTransformToBat.h"
#include "skill/EffectTransformToWolf.h"
#include "skill/VampireCastleSkillSlot.h"

const Color_t QUEST_COLOR = 0xFFFE;

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////


Vampire::Vampire()

    : PlayerCreature(0, NULL), m_pRealWearingCheck{} {
    __BEGIN_TRY

    m_CClass = CREATURE_CLASS_VAMPIRE;

    m_Mutex.setName("Vampire");

    // Insert the basic attacks such as AttackMelee.
    for (int i = 0; i < SKILL_DOUBLE_IMPACT; i++) {
        VampireSkillSlot* pVampireSkillSlot = new VampireSkillSlot;
        // pVampireSkillSlot = new VampireSkillSlot;	// 2002.1.16 by sigi
        pVampireSkillSlot->setName(m_Name);
        pVampireSkillSlot->setSkillType(i);
        pVampireSkillSlot->setInterval(5);
        pVampireSkillSlot->setRunTime();

        addSkill(pVampireSkillSlot);
    }

    for (int i = 0; i < VAMPIRE_WEAR_MAX; i++)
        m_pWearItem[i] = NULL;

    m_ClanType = 0;

    // Initialize the HP regeneration time.
    getCurrentTime(m_HPRegenTime);

    // Initialize the experience save count.
    //	m_RankExpSaveCount       = 0;
    m_ExpSaveCount = 0;
    m_FameSaveCount = 0;
    m_AlignmentSaveCount = 0;

    __END_CATCH
}

Vampire::~Vampire()

{
    __BEGIN_TRY

    try {
        // Build the outfit information.
        DWORD flag;
        Color_t color[PCVampireInfo::VAMPIRE_COLOR_MAX];
        getShapeInfo(flag, color);

        char pField[128];
        snprintf(pField, sizeof(pField), "Shape=%u, CoatColor=%d", flag, color[PCVampireInfo::VAMPIRE_COLOR_COAT]);

        // cout << "SAVE = " << pField << endl;

        tinysave(pField);


        // Save the items' remaining durability, the experience and the alignment.
        saveGears();
        saveExps(m_GoalExp, m_SilverDamage);
        saveSkills();

        // Delete the items being worn from memory.
        destroyGears();

        // When the class is deleted the matching trade information must be deleted,
        // and the trade partner must be told as well.
        TradeManager* pTradeManager = m_pZone->getTradeManager();
        TradeInfo* pInfo = pTradeManager->getTradeInfo(getName());
        if (pInfo != NULL) {
            // Delete the trade information.
            pTradeManager->cancelTrade(this);
        }

        // Delete the global party information.
        // On a normal logout
        // CGLogoutHandler calls Zone::deleteCreature(), and
        // even in an abnormal case
        // GamePlayer::disconnect() calls Zone::deleteCreature(), so
        // the local party, party invitation and trade information need no care here.
        deleteAllPartyInfo(this);

        // Delete the skills.
        unordered_map<SkillType_t, VampireSkillSlot*>::iterator itr = m_SkillSlot.begin();
        for (; itr != m_SkillSlot.end(); itr++) {
            VampireSkillSlot* pVampireSkillSlot = itr->second;
            SAFE_DELETE(pVampireSkillSlot);
        }
    } catch (Throwable& t) {
        filelog("vampireDestructor.txt", "%s", t.toString().c_str());
    } catch (exception& e) {
        filelog("vampireDestructor.txt", "Unknown std::exception");
    } catch (...) {
        filelog("vampireDestructor.txt", "Unknown ... exception");
    }

    m_bDeriveDestructed = true;

    __END_CATCH_NO_RETHROW
}

// registerObject
// Use the ObjectRegistry owned by the Zone to allocate ObjectIDs for the
// Vampire and the items it owns.
void Vampire::registerObject()

{
    __BEGIN_TRY

    Assert(getZone() != NULL);

    // Access the zone's object registry.
    ObjectRegistry& OR = getZone()->getObjectRegistry();

    __ENTER_CRITICAL_SECTION(OR)

    // Every item's OID changes, so the OID map of the time-limit item manager must be cleared.
    if (m_pTimeLimitItemManager != NULL)
        m_pTimeLimitItemManager->clear();

    // Register the vampire's own OID first.
    OR.registerObject_NOLOCKED(this);

    // Register the OIDs of the inventory items.
    registerInventory(OR);

    // Register the OIDs of the Goods Inventory items.
    registerGoodsInventory(OR);

    // Register the OIDs of the items being worn.
    for (int i = 0; i < VAMPIRE_WEAR_MAX; i++) {
        Item* pItem = m_pWearItem[i];

        if (pItem != NULL) {
            bool bCheck = true;

            // A two-handed weapon was already registered under WEAR_LEFTHAND,
            // so there is no need to register it again.
            if (i == WEAR_RIGHTHAND && isTwohandWeapon(pItem))
                bCheck = false;

            if (bCheck)
                registerItem(pItem, OR);
        }
    }

    // Register the OID of the item held on the mouse.
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

    m_VampireInfo.setObjectID(m_ObjectID);
    m_pStore->updateStoreInfo();

    __END_CATCH
}

// Use the ObjectRegistry owned by the Zone to allocate ObjectIDs for the
// Vampire and the items it owns. Kept separate to decide whether to leave an ItemTrace.
void Vampire::registerInitObject()

{
    __BEGIN_TRY

    Assert(getZone() != NULL);

    // Access the zone's object registry.
    ObjectRegistry& OR = getZone()->getObjectRegistry();

    __ENTER_CRITICAL_SECTION(OR)

    // Every item's OID changes, so the OID map of the time-limit item manager must be cleared.
    if (m_pTimeLimitItemManager != NULL)
        m_pTimeLimitItemManager->clear();

    // Register the vampire's own OID first.
    OR.registerObject_NOLOCKED(this);

    // Register the OIDs of the inventory items.
    registerInitInventory(OR);

    // Register the OIDs of the Goods Inventory items.
    registerGoodsInventory(OR);

    // Register the OIDs of the items being worn.
    for (int i = 0; i < VAMPIRE_WEAR_MAX; i++) {
        Item* pItem = m_pWearItem[i];

        if (pItem != NULL) {
            // Decide whether to leave an ItemTrace.
            pItem->setTraceItem(bTraceLog(pItem));

            bool bCheck = true;

            // A two-handed weapon was already registered under WEAR_LEFTHAND,
            // so there is no need to register it again.
            if (i == WEAR_RIGHTHAND && isTwohandWeapon(pItem))
                bCheck = false;

            if (bCheck)
                registerItem(pItem, OR);
        }
    }

    // Register the OID of the item held on the mouse.
    Item* pSlotItem = m_pExtraInventorySlot->getItem();
    if (pSlotItem != NULL) {
        // Decide whether to leave an ItemTrace.
        pSlotItem->setTraceItem(bTraceLog(pSlotItem));
        registerItem(pSlotItem, OR);
    }

    m_Garbage.registerObject(OR);

    __LEAVE_CRITICAL_SECTION(OR)

    m_VampireInfo.setObjectID(m_ObjectID);

    __END_CATCH
}

// Check the time-limit items.
// Every item must already be registered.
void Vampire::checkItemTimeLimit() {
    __BEGIN_TRY

    // Search the inventory.
    {
        list<Item*> ItemList;
        int height = m_pInventory->getHeight();
        int width = m_pInventory->getWidth();

        for (int j = 0; j < height; j++) {
            for (int i = 0; i < width; i++) {
                Item* pItem = m_pInventory->getItem(i, j);
                if (pItem != NULL) {
                    // Find the current item in the list of items already checked.
                    list<Item*>::iterator itr = find(ItemList.begin(), ItemList.end(), pItem);

                    if (itr == ItemList.end()) {
                        i += pItem->getVolumeWidth() - 1;

                        if (wasteIfTimeLimitExpired(pItem)) {
                            m_pInventory->deleteItem(pItem->getObjectID());
                            SAFE_DELETE(pItem);
                        } else {
                            // If the item is not in the list,
                            // put the item into the list so that
                            // the same item is not checked twice.
                            ItemList.push_back(pItem);
                        }
                    }
                }
            }
        }
    }

    // Search among the items being worn.
    {
        for (int i = 0; i < VAMPIRE_WEAR_MAX; i++) {
            Item* pItem = m_pWearItem[i];

            if (pItem != NULL) {
                bool bCheck = true;

                // A two-handed weapon fills both hand slots with the same item,
                // so it is handled only once, under WEAR_LEFTHAND.
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

    // Check the item held on the mouse.
    {
        Item* pSlotItem = m_pExtraInventorySlot->getItem();
        if (pSlotItem != NULL && wasteIfTimeLimitExpired(pSlotItem)) {
            deleteItemFromExtraInventorySlot();
            SAFE_DELETE(pSlotItem);
        }
    }

    __END_CATCH
}

void Vampire::updateEventItemTime(DWORD time) {
    __BEGIN_TRY

    // Search the inventory.
    {
        list<Item*> ItemList;
        int height = m_pInventory->getHeight();
        int width = m_pInventory->getWidth();

        for (int j = 0; j < height; j++) {
            for (int i = 0; i < width; i++) {
                Item* pItem = m_pInventory->getItem(i, j);
                if (pItem != NULL) {
                    // Find the current item in the list of items already checked.
                    list<Item*>::iterator itr = find(ItemList.begin(), ItemList.end(), pItem);

                    if (itr == ItemList.end()) {
                        i += pItem->getVolumeWidth() - 1;

                        updateItemTimeLimit(pItem, time);

                        // If the item is not in the list,
                        // put the item into the list so that
                        // the same item is not checked twice.
                        ItemList.push_back(pItem);
                    }
                }
            }
        }
    }

    // Search among the items being worn.
    {
        for (int i = 0; i < VAMPIRE_WEAR_MAX; i++) {
            Item* pItem = m_pWearItem[i];

            if (pItem != NULL) {
                bool bCheck = true;

                // A two-handed weapon fills both hand slots with the same item,
                // so it is handled only once, under WEAR_LEFTHAND.
                if (i == WEAR_RIGHTHAND && isTwohandWeapon(pItem))
                    bCheck = false;

                if (bCheck) {
                    updateItemTimeLimit(pItem, time);
                }
            }
        }
    }

    // Check the item held on the mouse.
    {
        Item* pSlotItem = m_pExtraInventorySlot->getItem();
        if (pSlotItem != NULL) {
            updateItemTimeLimit(pSlotItem, time);
        }
    }

    __END_CATCH
}

// A vampire is made by transforming a slayer, so it is never given a
// newbie set; it inherits the empty hook.
void Vampire::loadOwnedItems() {
    de::gameContext().itemLoaders().load(this);
}


//----------------------------------------------------------------------
//----------------------------------------------------------------------
bool Vampire::load()

{
    __BEGIN_TRY

    if (!PlayerCreature::load())
        return false;

    VampireLoadRecord record;
    if (!defaultCharacterRepository().loadVampire(m_Name, record))
        return false;

    setName(record.name);

    Level_t advLevel = record.advancementClass;
    Exp_t advGoalExp = record.advancementGoalExp;

    m_pAdvancementClass =
        new AdvancementClass(advLevel, advGoalExp, AdvancementClassExpTable::s_AdvancementClassExpTable);
    if (getAdvancementClassLevel() > 0)
        m_bAdvanced = true;

    setSex(record.sex);
    // edit by sonic 2006.10.28
    setMasterEffectColor(record.masterEffectColor);
    // end by sonic
    setBatColor(record.batColor);
    setSkinColor(record.skinColor);

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

    setFame(record.fame);

    //		setExp(pResult->getInt(++i));
    setGoalExp(record.goalExp);
    //		setExpOffset(pResult->getInt(++i));
    setLevel(record.level);
    setBonus(record.bonus);

    // setInMagics(pResult->getString(++i));
    setGold(record.gold);
    setGuildID(record.guildID);

    //		setZoneID(pResult->getInt(++i));
    ZoneID_t zoneID = record.zoneID;
    setX(record.x);
    setY(record.y);

    setSight(record.sight);

    setAlignment(record.alignment);

    //		for (int j = 0; j < 8; j++)
    //			setHotKey(j, pResult->getInt(++i));

    setStashGold(record.stashGold);
    setStashNum(record.stashNum);

    m_Competence = record.competence;

    if (m_Competence >= 4)
        m_Competence = 3;

    m_CompetenceShape = record.competenceShape;

    setResurrectZoneID(record.resurrectZone);

    m_SilverDamage = record.silverDamage;

    // record.reward is not consumed: the reward flow that read it is dead.
    setSMSCharge(record.smsCharge);

    Rank_t CurRank = record.rank;
    RankExp_t RankGoalExp = record.rankGoalExp;

    m_pRank = new Rank(CurRank, RankGoalExp, RankExpTable::s_RankExpTables[RANK_TYPE_VAMPIRE]);

    // Recompute and set maxHP.
    // 2002.7.15 by sigi
    // If the formula changes, computeHP in AbilityBalance.cpp must change too.
    int maxHP = m_STR[ATTR_CURRENT] * 2 + m_INT[ATTR_CURRENT] + m_DEX[ATTR_CURRENT] + m_Level;
    maxHP = min((int)maxHP, VAMPIRE_MAX_HP);
    setHP(maxHP, ATTR_MAX);

    try {
        setZoneID(zoneID);
    } catch (Error& e) {
        // Treated as the guild-hideout problem:
        // a guild hideout exists on one game server only, so a character
        // connecting to another game server cannot enter it.
        // Move to the hideout entrance instead.
        ZONE_COORD ResurrectCoord;
        de::gameContext().resurrectLocations().getVampirePosition(1003, ResurrectCoord);
        setZoneID(ResurrectCoord.id);
        setX(ResurrectCoord.x);
        setY(ResurrectCoord.y);
    }

    //----------------------------------------------------------------------
    // Build the Vampire Outlook Information.
    //----------------------------------------------------------------------
    // The vampire sets its ObjectID at load time. What about at connect time?
    m_VampireInfo.setObjectID(m_ObjectID);
    m_VampireInfo.setName(m_Name);
    m_VampireInfo.setSex(m_Sex);
    m_VampireInfo.setBatColor(m_BatColor);
    m_VampireInfo.setSkinColor(m_SkinColor);
    m_VampireInfo.setMasterEffectColor(m_MasterEffectColor);

    m_VampireInfo.setCompetence(m_CompetenceShape);

    //----------------------------------------------------------------------
    // Load the learned skills.
    //----------------------------------------------------------------------
    vector<VampireSkillRow> skillRows = defaultSkillSaveRepository().loadVampireSkills(m_Name);
    for (size_t r = 0; r < skillRows.size(); r++) {
        const VampireSkillRow& row = skillRows[r];
        SkillType_t SkillType = row.skillType;

        if (hasSkill(SkillType) == NULL) {
            VampireSkillSlot* pVampireSkillSlot = new VampireSkillSlot();

            pVampireSkillSlot->setName(m_Name);
            pVampireSkillSlot->setSkillType(SkillType);
            pVampireSkillSlot->setInterval(row.delay);
            pVampireSkillSlot->setCastingTime(row.castingTime);
            // pVampireSkillSlot->setRunTime (pResult->getInt(++i));
            pVampireSkillSlot->setRunTime();

            addSkill(pVampireSkillSlot);
        }
    }

    //----------------------------------------------------------------------
    // Load the Rank Bonus.
    //----------------------------------------------------------------------
    loadRankBonus();

    //----------------------------------------------------------------------
    // Load the effects.
    //----------------------------------------------------------------------
    de::gameContext().effectLoaders().load(this);

    //----------------------------------------------------------------------
    // Attach the effect for a GrandMaster.
    //----------------------------------------------------------------------
    // by sigi. 2002.11.8
    if (m_Level >= 100 && SystemAvailabilitiesManager::getInstance()->isAvailable(
                              SystemAvailabilitiesManager::SYSTEM_GRAND_MASTER_EFFECT)) {
        if (!isFlag(Effect::EFFECT_CLASS_GRAND_MASTER_VAMPIRE)) {
            EffectGrandMasterVampire* pEffect = new EffectGrandMasterVampire(this);
            pEffect->setDeadline(999999);
            getEffectManager()->addEffect(pEffect);
            setFlag(Effect::EFFECT_CLASS_GRAND_MASTER_VAMPIRE);
        }
    }

    //----------------------------------------------------------------------
    // Load the flag set.
    //----------------------------------------------------------------------
    m_pFlagSet->load(getName());

    //----------------------------------------------------------------------
    // Initialize the Vampire Outlook Information.
    //----------------------------------------------------------------------

    m_VampireInfo.setCoatType(0);
    m_VampireInfo.setCoatColor(JACKET_BASIC);
    m_VampireInfo.setCoatColor(377);
    m_VampireInfo.setAdvancementLevel(getAdvancementClassLevel());
    // m_VampireInfo.setCoatColor(2 , SUB_COLOR);


    // A rank of 0 means the initial values have not been set.
    if (getRank() == 0) {
        saveInitialRank();
    }


    initAllStat();

    // Check the war participation flag.
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
void Vampire::save() const

{
    __BEGIN_TRY

    __ENTER_CRITICAL_SECTION(m_Mutex)

    // Save the vampire information.
    VampireVitalsRecord record;
    record.currentHP = (int)m_HP[ATTR_CURRENT];
    record.maxHP = (int)m_HP[ATTR_MAX];
    record.silverDamage = (int)m_SilverDamage;
    record.zoneID = (int)getZoneID();
    record.x = (int)m_X;
    record.y = (int)m_Y;
    defaultCharacterRepository().saveVampireVitals(m_Name, record);


    //--------------------------------------------------
    // Save the effects.
    //--------------------------------------------------
    m_pEffectManager->save(m_Name);

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    __END_CATCH
}

////////////////////////////////////////////////////////////////////////////////
//
//
// Skill related functions
//
//
////////////////////////////////////////////////////////////////////////////////

// Return a specific Skill.
VampireSkillSlot* Vampire::getSkill(SkillType_t SkillType) const {
    return findSkillSlot(m_SkillSlot, SkillType);
}

// Add a specific Skill.
void Vampire::addSkill(SkillType_t SkillType)

{
    __BEGIN_TRY

    switch (SkillType) {
    case SKILL_UN_BURROW:
    case SKILL_UN_TRANSFORM:
    case SKILL_UN_INVISIBILITY:
    case SKILL_THROW_HOLY_WATER:
    case SKILL_EAT_CORPSE:
        // case SKILL_HOWL:
        filelog("VampireError.log", "SkillType[%d], %s", SkillType, toString().c_str());
        Assert(false);
        break;
    default:
        break;
    }

    unordered_map<SkillType_t, VampireSkillSlot*>::iterator itr = m_SkillSlot.find(SkillType);

    if (itr == m_SkillSlot.end()) {
        VampireSkillSlot* pVampireSkillSlot = new VampireSkillSlot;

        pVampireSkillSlot->setName(m_Name);
        pVampireSkillSlot->setSkillType(SkillType);
        // A freshly learned skill starts with no run-time lock and a ZERO
        // interval. Do not seed it from SkillBalance's MaxDelay (2.0 s for
        // e.g. Bloody Nail and Violent Phantom): GCSkillInfo sends the slot
        // interval on every login and zone change, and the client keeps any
        // delay of 1.8 s or more as a per-cast cooldown until the next
        // refresh, so a skill learned mid-session would stutter for the rest
        // of the session. The first successful cast installs the real
        // per-cast formula delay (setRunTime(delay)) and persists it.
        pVampireSkillSlot->setRunTime(0);
        pVampireSkillSlot->setInterval(0);
        pVampireSkillSlot->create(m_Name);

        m_SkillSlot[SkillType] = pVampireSkillSlot;
    }

    __END_CATCH
}

// Put a SkillSlot into an empty slot found automatically.
void Vampire::addSkill(VampireSkillSlot* pVampireSkillSlot)

{
    __BEGIN_TRY

    SkillType_t SkillType = pVampireSkillSlot->getSkillType();
    switch (SkillType) {
    case SKILL_UN_BURROW:
    case SKILL_UN_TRANSFORM:
    case SKILL_UN_INVISIBILITY:
    case SKILL_THROW_HOLY_WATER:
    case SKILL_EAT_CORPSE:
        //		case SKILL_HOWL:
        filelog("VampireError.log", "SkillType[%d], %s", SkillType, toString().c_str());
        Assert(false);
        break;
    default:
        break;
    }

    unordered_map<SkillType_t, VampireSkillSlot*>::iterator itr = m_SkillSlot.find(pVampireSkillSlot->getSkillType());

    if (itr == m_SkillSlot.end()) {
        m_SkillSlot[pVampireSkillSlot->getSkillType()] = pVampireSkillSlot;
    }
    // 2002.1.16 by sigi
    else {
        delete pVampireSkillSlot;
    }

    __END_CATCH
}

void Vampire::removeCastleSkill(SkillType_t SkillType) {
    removeCastleSkillSlot<VampireSkillSlot, VampireCastleSkillSlot>(m_SkillSlot, SkillType);
}

void Vampire::removeAllCastleSkill() {
    removeAllCastleSkillSlots(m_SkillSlot);
}


////////////////////////////////////////////////////////////////////////////////
//
//
// Item wear/take-off related functions
//
//
////////////////////////////////////////////////////////////////////////////////

//----------------------------------------------------------------------
//
// Vampire::WearItem()
//
// Put an Item into the wear slot and compute the attributes.
//
//----------------------------------------------------------------------
void Vampire::wearItem(WearPart Part, Item* pItem)

{
    __BEGIN_TRY

    Assert(pItem != NULL);

    Item* pPrevItem = NULL;
    Item* pLeft = NULL;
    Item* pRight = NULL;

    // Under the current design an item can be used even when the attributes
    // fall short. The item's attribute bonus is simply not applied.
    // So the item is put into the matching wear slot first.

    // A two-handed weapon puts one item pointer into both hand slots...
    if (isTwohandWeapon(pItem)) {
        // Holding an item in both hands.
        if (isWear(WEAR_RIGHTHAND) && isWear(WEAR_LEFTHAND)) {
            pLeft = getWearItem(WEAR_LEFTHAND);
            pRight = getWearItem(WEAR_RIGHTHAND);

            // Holding a two-handed weapon.
            if (pLeft == pRight) {
                // Put the requested item into the wear point, and
                m_pWearItem[WEAR_RIGHTHAND] = pItem;
                m_pWearItem[WEAR_LEFTHAND] = pItem;

                // by sigi. 2002.5.15
                char pField[80];
                // pItem->save(m_Name, STORAGE_GEAR, 0, Part, 0);
                snprintf(pField, sizeof(pField), "Storage=%d, X=%d", STORAGE_GEAR, Part);
                pItem->tinysave(pField);

                // Hand the item that was there back to the mouse pointer.
                addItemToExtraInventorySlot(pLeft);
                // pLeft->save(m_Name, STORAGE_EXTRASLOT, 0, 0, 0);
                snprintf(pField, sizeof(pField), "Storage=%d", STORAGE_EXTRASLOT);
                pLeft->tinysave(pField);
            }
            // Holding a sword and a shield.
            else {
                // A sword and a shield are held in both hands and a two-handed weapon is
                // requested: the sword can go onto the mouse pointer, but the shield cannot.
                // It would have to go into the inventory, but there is no way to do that yet...
                // So just send a packet saying the item cannot be worn...
                // cerr << "A sword and a shield are held, so a two-handed weapon cannot be worn." << endl;
                return;
            }
        }
        // Not holding an item in both hands.
        else {
            char pField[80];

            // Holding an item in the right hand.
            if (isWear(WEAR_RIGHTHAND)) {
                pRight = getWearItem(WEAR_RIGHTHAND);
                // Put the requested item into the wear point.
                m_pWearItem[WEAR_RIGHTHAND] = pItem;
                m_pWearItem[WEAR_LEFTHAND] = pItem;

                // by sigi. 2002.5.15
                // pItem->save(m_Name, STORAGE_GEAR, 0, Part, 0);
                snprintf(pField, sizeof(pField), "Storage=%d, X=%d", STORAGE_GEAR, Part);
                pItem->tinysave(pField);

                // Hand the item that was there back to the mouse pointer.
                addItemToExtraInventorySlot(pRight);
                // pRight->save(m_Name, STORAGE_EXTRASLOT, 0, 0, 0);
                snprintf(pField, sizeof(pField), "Storage=%d", STORAGE_EXTRASLOT);
                pRight->tinysave(pField);
            }
            // Holding an item in the left hand.
            else if (isWear(WEAR_LEFTHAND)) {
                pLeft = getWearItem(WEAR_LEFTHAND);
                // Put the requested item into the wear point.
                m_pWearItem[WEAR_RIGHTHAND] = pItem;
                m_pWearItem[WEAR_LEFTHAND] = pItem;

                // by sigi. 2002.5.15
                // pItem->save(m_Name, STORAGE_GEAR, 0, Part, 0);
                snprintf(pField, sizeof(pField), "Storage=%d, X=%d", STORAGE_GEAR, Part);
                pItem->tinysave(pField);

                // Hand the item that was there back to the mouse pointer.
                addItemToExtraInventorySlot(pLeft);
                // pLeft->save(m_Name, STORAGE_EXTRASLOT, 0, 0, 0);
                snprintf(pField, sizeof(pField), "Storage=%d", STORAGE_EXTRASLOT);
                pLeft->tinysave(pField);
            }
            // Holding an item in neither hand.
            else {
                // Put the requested item into the wear point.
                m_pWearItem[WEAR_RIGHTHAND] = pItem;
                m_pWearItem[WEAR_LEFTHAND] = pItem;

                // by sigi. 2002.5.15
                // pItem->save(m_Name, STORAGE_GEAR, 0, Part, 0);
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
            // pItem->save(m_Name, STORAGE_GEAR, 0, Part, 0);
            snprintf(pField, sizeof(pField), "Storage=%d, X=%d", STORAGE_GEAR, Part);
            pItem->tinysave(pField);

            addItemToExtraInventorySlot(pPrevItem);
            // pPrevItem->save(m_Name, STORAGE_EXTRASLOT, 0, 0, 0);
            snprintf(pField, sizeof(pField), "Storage=%d", STORAGE_EXTRASLOT);
            pPrevItem->tinysave(pField);
        } else {
            // Put the requested item into the wear point.
            m_pWearItem[Part] = pItem;

            // by sigi. 2002.5.15
            char pField[80];
            // pItem->save(m_Name, STORAGE_GEAR, 0, Part, 0);
            snprintf(pField, sizeof(pField), "Storage=%d, X=%d", STORAGE_GEAR, Part);
            pItem->tinysave(pField);
        }
    }

    // For a coat, set the color that goes with it.
    // There may be several coat types later, but for now there is only
    // one, so only the color is set.
    if (pItem->getItemClass() == Item::ITEM_CLASS_VAMPIRE_COAT) {
        m_VampireInfo.setCoatColor(getItemShapeColor(pItem));

        // Set the item type.
        m_VampireInfo.setCoatType(pItem->getItemType());
    }

    __END_CATCH
}


//----------------------------------------------------------------------
// Vampire::WearItem()
// Put an Item into the wear slot and compute the attributes.
//----------------------------------------------------------------------
void Vampire::wearItem(WearPart Part)

{
    __BEGIN_TRY

    // Get the item that is about to be worn.
    Item* pItem = getExtraInventorySlotItem();
    Assert(pItem != NULL);

    Item* pPrevItem = NULL;
    Item* pLeft = NULL;
    Item* pRight = NULL;

    // Save the current attributes into a buffer before clothing is put on or
    // taken off, so that only the changed attributes are sent later.
    VAMPIRE_RECORD prev;
    getVampireRecord(prev);

    // Under the current design an item can be used even when the attributes
    // fall short. The item's attribute bonus is simply not applied.
    // So the item is put into the matching wear slot first.
    char pField[80];

    // A two-handed weapon puts one item pointer into both hand slots...
    if (isTwohandWeapon(pItem)) {
        // Holding an item in both hands.
        if (isWear(WEAR_RIGHTHAND) && isWear(WEAR_LEFTHAND)) {
            pLeft = getWearItem(WEAR_LEFTHAND);
            pRight = getWearItem(WEAR_RIGHTHAND);

            // Holding a two-handed weapon.
            if (pLeft == pRight) {
                takeOffItem(WEAR_LEFTHAND, false, false);

                // Put the requested item into the wear point, and
                m_pWearItem[WEAR_RIGHTHAND] = pItem;
                m_pWearItem[WEAR_LEFTHAND] = pItem;
                // by sigi. 2002.5.15
                // pItem->save(m_Name, STORAGE_GEAR, 0, Part, 0);
                snprintf(pField, sizeof(pField), "Storage=%d, X=%d", STORAGE_GEAR, Part);
                pItem->tinysave(pField);

                // Remove the requested item from the mouse pointer.
                deleteItemFromExtraInventorySlot();
                // Hand the item that was there back to the mouse pointer.
                addItemToExtraInventorySlot(pLeft);
                // pLeft->save(m_Name, STORAGE_EXTRASLOT, 0, 0, 0);
                snprintf(pField, sizeof(pField), "Storage=%d", STORAGE_EXTRASLOT);
                pLeft->tinysave(pField);

            }
            // Holding a sword and a shield.
            else {
                // A sword and a shield are held in both hands and a two-handed weapon is
                // requested: the sword can go onto the mouse pointer, but the shield cannot.
                // It would have to go into the inventory, but there is no way to do that yet...
                // So just send a packet saying the item cannot be worn...
                return;
            }
        }
        // Not holding an item in both hands.
        else {
            // by sigi. 2002.5.15
            // Holding an item in the right hand.
            if (isWear(WEAR_RIGHTHAND)) {
                pRight = getWearItem(WEAR_RIGHTHAND);

                takeOffItem(WEAR_RIGHTHAND, false, false);

                // Put the requested item into the wear point.
                m_pWearItem[WEAR_RIGHTHAND] = pItem;
                m_pWearItem[WEAR_LEFTHAND] = pItem;
                // pItem->save(m_Name, STORAGE_GEAR, 0, Part, 0);

                // by sigi. 2002.5.15
                snprintf(pField, sizeof(pField), "Storage=%d, X=%d", STORAGE_GEAR, Part);
                pItem->tinysave(pField);

                // Remove the requested item from the mouse pointer.
                deleteItemFromExtraInventorySlot();
                // Hand the item that was there back to the mouse pointer.
                addItemToExtraInventorySlot(pRight);
                // pRight->save(m_Name, STORAGE_EXTRASLOT, 0, 0, 0);
                snprintf(pField, sizeof(pField), "Storage=%d", STORAGE_EXTRASLOT);
                pRight->tinysave(pField);

            }
            // Holding an item in the left hand.
            else if (isWear(WEAR_LEFTHAND)) {
                pLeft = getWearItem(WEAR_LEFTHAND);

                takeOffItem(WEAR_LEFTHAND, false, false);

                // Put the requested item into the wear point.
                m_pWearItem[WEAR_RIGHTHAND] = pItem;
                m_pWearItem[WEAR_LEFTHAND] = pItem;

                // by sigi. 2002.5.15
                // pItem->save(m_Name, STORAGE_GEAR, 0, Part, 0);
                snprintf(pField, sizeof(pField), "Storage=%d, X=%d", STORAGE_GEAR, Part);
                pItem->tinysave(pField);

                // Remove the requested item from the mouse pointer.
                deleteItemFromExtraInventorySlot();
                // Hand the item that was there back to the mouse pointer.
                addItemToExtraInventorySlot(pLeft);
                // pLeft->save(m_Name, STORAGE_EXTRASLOT, 0, 0, 0);
                snprintf(pField, sizeof(pField), "Storage=%d", STORAGE_EXTRASLOT);
                pLeft->tinysave(pField);
            }
            // Holding an item in neither hand.
            else {
                // Put the requested item into the wear point.
                m_pWearItem[WEAR_RIGHTHAND] = pItem;
                m_pWearItem[WEAR_LEFTHAND] = pItem;

                pItem->save(m_Name, STORAGE_GEAR, 0, Part, 0);
                // Remove the requested item from the mouse pointer.
                deleteItemFromExtraInventorySlot();
            }
        }
    } else {
        if (isWear(Part)) {
            pPrevItem = getWearItem(Part);
            takeOffItem(Part, false, false);
            m_pWearItem[Part] = pItem;

            // by sigi. 2002.5.15
            // pItem->save(m_Name, STORAGE_GEAR, 0, Part, 0);
            snprintf(pField, sizeof(pField), "Storage=%d, X=%d", STORAGE_GEAR, Part);
            pItem->tinysave(pField);

            deleteItemFromExtraInventorySlot();
            addItemToExtraInventorySlot(pPrevItem);

            // pPrevItem->save(m_Name, STORAGE_EXTRASLOT, 0, 0, 0);
            snprintf(pField, sizeof(pField), "Storage=%d", STORAGE_EXTRASLOT);
            pPrevItem->tinysave(pField);
        } else {
            m_pWearItem[Part] = pItem;
            deleteItemFromExtraInventorySlot();

            // by sigi. 2002.5.15
            // pItem->save(m_Name, STORAGE_GEAR, 0, Part, 0);
            snprintf(pField, sizeof(pField), "Storage=%d, X=%d", STORAGE_GEAR, Part);
            pItem->tinysave(pField);
        }
    }

    initAllStat();
    sendRealWearingInfo();
    sendModifyInfo(prev);

    // For a coat, set the color that goes with it.
    // There may be several coat types later, but for now there is only
    // one, so only the color is set.

    // Only an item that is really applied changes the outfit.
    if (m_pRealWearingCheck[Part]) {
        if (pItem->getItemClass() == Item::ITEM_CLASS_VAMPIRE_COAT) {
            Color_t color = getItemShapeColor(pItem);
            m_VampireInfo.setCoatColor(color);
            m_VampireInfo.setCoatType(pItem->getItemType());

            // The clothing changed, so tell the surroundings about it.
            GCChangeShape pkt;
            pkt.setObjectID(getObjectID());
            pkt.setItemClass(Item::ITEM_CLASS_VAMPIRE_COAT);
            pkt.setItemType(pItem->getItemType());
            pkt.setOptionType(pItem->getFirstOptionType());
            pkt.setAttackSpeed(m_AttackSpeed[ATTR_CURRENT]);

            if (color == QUEST_COLOR)
                pkt.setFlag(SHAPE_FLAG_QUEST);

            Zone* pZone = getZone();
            pZone->broadcastPacket(m_X, m_Y, &pkt, this);
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
// Vampire::takeOffItem()
//
//----------------------------------------------------------------------
void Vampire::takeOffItem(WearPart Part, bool bAddOnMouse, bool bSendModifyInfo)

{
    __BEGIN_TRY

    VAMPIRE_RECORD prev;

    // Get the item in the wear slot.
    Item* pItem = m_pWearItem[Part];
    Assert(pItem != NULL);

    // m_pWearItem[Part] = NULL;

    // Get the item in the wear slot.
    // Item::ItemClass IClass = pItem->getItemClass();

    if (Part == WEAR_LEFTHAND || Part == WEAR_RIGHTHAND) {
        if (m_pWearItem[WEAR_RIGHTHAND] && m_pWearItem[WEAR_LEFTHAND]) {
            if (m_pWearItem[WEAR_RIGHTHAND] == m_pWearItem[WEAR_LEFTHAND]) {
                m_pWearItem[WEAR_RIGHTHAND] = NULL;
                m_pWearItem[WEAR_LEFTHAND] = NULL;
            }
        }
    }

    // Remove the item from the wear point.
    if (isTwohandWeapon(pItem)) {
        m_pWearItem[WEAR_RIGHTHAND] = NULL;
        m_pWearItem[WEAR_LEFTHAND] = NULL;
    } else
        m_pWearItem[Part] = NULL;

    // wearItem takes off the clothing already in the given slot and puts the
    // new one on, which would send one packet when it comes off and another
    // when it goes on, two packets in total. A bool parameter was added to
    // prevent that.
    if (bSendModifyInfo) {
        getVampireRecord(prev);
        initAllStat();
        sendRealWearingInfo();
        sendModifyInfo(prev);
    } else {
        initAllStat();
    }

    //---------------------------------------------
    // A check that should not be needed; a temporary patch.
    // Attach the item to the mouse cursor.
    //---------------------------------------------
    if (bAddOnMouse) {
        addItemToExtraInventorySlot(pItem);
        // pItem->save(m_Name, STORAGE_EXTRASLOT, 0, 0, 0);
        //  Item save optimization.
        char pField[80];
        snprintf(pField, sizeof(pField), "Storage=%d, Durability=%d", STORAGE_EXTRASLOT, pItem->getDurability());
        pItem->tinysave(pField);
    }

    if (pItem->getItemClass() == Item::ITEM_CLASS_VAMPIRE_COAT) {
        m_VampireInfo.setCoatColor(377);
        m_VampireInfo.setCoatType(0);

        GCTakeOff pkt;
        pkt.setObjectID(getObjectID());
        pkt.setSlotID((SlotID_t)ADDON_COAT);
        m_pZone->broadcastPacket(getX(), getY(), &pkt, this);
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
// Delete the worn items.
//----------------------------------------------------------------------
void Vampire::destroyGears()

{
    __BEGIN_TRY
    __BEGIN_DEBUG

    for (int j = 0; j < VAMPIRE_WEAR_MAX; j++) {
        Item* pItem = m_pWearItem[j];
        if (pItem != NULL) {
            Item::ItemClass IClass = pItem->getItemClass();

            //-------------------------------------------------------------
            // Assert on anyone wearing a Slayer-only item.
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

            // Check for a two-handed weapon so that deleting the one item
            // empties both hands.
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
bool Vampire::isRealWearing(WearPart part) const

{
    __BEGIN_TRY

    if (part >= VAMPIRE_WEAR_MAX)
        throw Error("Vampire::isRealWearing() : invalid wear point!");

    if (m_pWearItem[part] == NULL)
        return false;
    if (part >= WEAR_ZAP1 && part <= WEAR_ZAP4) {
        // A ring must also be worn in the matching position.
        if (m_pWearItem[part - WEAR_ZAP1 + WEAR_FINGER1] == NULL)
            return false;
    }

    return isRealWearing(m_pWearItem[part]);

    __END_CATCH
}

//----------------------------------------------------------------------
//----------------------------------------------------------------------
bool Vampire::isRealWearing(Item* pItem) const

{
    __BEGIN_TRY

    if (pItem == NULL)
        return false;

    ItemInfo* pItemInfo = de::gameContext().itemInfos().getItemInfo(pItem->getItemClass(), pItem->getItemType());

    Level_t ReqAdvancedLevel = pItemInfo->getReqAdvancedLevel();
    if (ReqAdvancedLevel > 0 && (!isAdvanced() || getAdvancementClassLevel() < ReqAdvancedLevel))
        return false;

    if (pItem->getItemClass() == Item::ITEM_CLASS_VAMPIRE_COAT ||
        pItem->getItemClass() == Item::ITEM_CLASS_VAMPIRE_WEAPON) {
        if (ReqAdvancedLevel <= 0 && isAdvanced())
            return false;
    }

    if (pItem->isTimeLimitItem())
        return decore::genderAllows(m_Sex, pItemInfo->getReqGender());

    // In a premium zone only paying users get unique/rare items applied.
    // Couple rings are usable only by paying users as well.
    if (getZone()->isPremiumZone() &&
        (pItem->isUnique() || pItem->getOptionTypeSize() > 1 || pItem->getItemClass() == Item::ITEM_CLASS_COUPLE_RING ||
         pItem->getItemClass() == Item::ITEM_CLASS_VAMPIRE_COUPLE_RING)) {
        GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(getPlayer());
        if (!pGamePlayer->isPayPlaying() && !pGamePlayer->isPremiumPlay()) {
            return false;
        }
    }

    if (isCoupleRing(pItem))
        return true;

    // The item's requirement raised by its options and capped, against
    // the vampire's level and sex.
    const decore::EquipRequirement required = computeEquipRequirement(decore::EquipRace::Vampire, pItem, pItemInfo);
    const decore::EquipStats current = {0, 0, 0, m_Level, m_Sex};
    return decore::meetsRequirement(decore::EquipRace::Vampire, required, current);

    __END_CATCH
}

//----------------------------------------------------------------------
//----------------------------------------------------------------------
bool Vampire::isRealWearingEx(WearPart part) const {
    if (part >= VAMPIRE_WEAR_MAX)
        return false;
    return m_pRealWearingCheck[part];
}

DWORD Vampire::sendRealWearingInfo(void) const

{
    __BEGIN_TRY

    DWORD info = 0;
    DWORD flag = 1;

    for (int i = 0; i < VAMPIRE_WEAR_MAX; i++) {
        if (isRealWearing((Vampire::WearPart)i))
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
// Info related functions
//
//
////////////////////////////////////////////////////////////////////////////////

PCVampireInfo2* Vampire::getVampireInfo2()

{
    __BEGIN_TRY
    __BEGIN_DEBUG

    PCVampireInfo2* pInfo = new PCVampireInfo2();

    pInfo->setObjectID(m_ObjectID);
    pInfo->setName(m_Name);
    pInfo->setLevel(m_Level);
    pInfo->setSex(m_Sex);
    pInfo->setBatColor(m_BatColor);
    pInfo->setSkinColor(m_SkinColor);
    pInfo->setMasterEffectColor(m_MasterEffectColor);

    // Alignment
    pInfo->setAlignment(m_Alignment);

    // Attributes
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
    pInfo->setFame(m_Fame);
    pInfo->setExp(m_GoalExp);
    //	pInfo->setExp(m_Exp);
    pInfo->setGold(m_Gold);
    pInfo->setSight(m_Sight);
    pInfo->setBonus(m_Bonus);
    pInfo->setSilverDamage(m_SilverDamage);

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
// Vampire Outlook Information
//----------------------------------------------------------------------
PCVampireInfo3 Vampire::getVampireInfo3() const

{
    __BEGIN_TRY
    __BEGIN_DEBUG

    m_VampireInfo.setX(m_X);
    m_VampireInfo.setY(m_Y);
    m_VampireInfo.setDir(m_Dir);
    m_VampireInfo.setCurrentHP(m_HP[ATTR_CURRENT]);
    m_VampireInfo.setMaxHP(m_HP[ATTR_MAX]);
    m_VampireInfo.setAttackSpeed(m_AttackSpeed[ATTR_CURRENT]);
    m_VampireInfo.setAlignment(m_Alignment);
    m_VampireInfo.setGuildID(m_GuildID);

    // by sigi. 2002.9.10
    m_VampireInfo.setRank(getRank());


    if (m_Flag.test(Effect::EFFECT_CLASS_TRANSFORM_TO_WOLF)) {
        m_VampireInfo.setShape(SHAPE_WOLF);
    } else if (m_Flag.test(Effect::EFFECT_CLASS_TRANSFORM_TO_BAT)) {
        m_VampireInfo.setShape(SHAPE_BAT);
    } else if (m_Flag.test(Effect::EFFECT_CLASS_TRANSFORM_TO_WERWOLF)) {
        m_VampireInfo.setShape(SHAPE_WERWOLF);
    } else {
        m_VampireInfo.setShape(SHAPE_NORMAL);
    }

    // For the dye item.
    m_VampireInfo.setBatColor(m_BatColor);
    m_VampireInfo.setSkinColor(m_SkinColor);
    m_VampireInfo.setMasterEffectColor(m_MasterEffectColor);

    GuildUnion* pUnion = GuildUnionManager::Instance().getGuildUnion(m_GuildID);
    if (pUnion == NULL)
        m_VampireInfo.setUnionID(0);
    else
        m_VampireInfo.setUnionID(pUnion->getUnionID());

    m_VampireInfo.setAdvancementLevel(getAdvancementClassLevel());

    return m_VampireInfo;

    __END_DEBUG
    __END_CATCH
}

//----------------------------------------------------------------------
//
// get Gear Info
//
//----------------------------------------------------------------------
GearInfo* Vampire::getGearInfo() const

{
    __BEGIN_TRY
    __BEGIN_DEBUG

    GearInfo* pGearInfo = new GearInfo();

    for (int i = 0; i < VAMPIRE_WEAR_MAX; i++) {
        Item* pItem = m_pWearItem[i];

        if (pItem != NULL) {
            // Item::ItemClass IClass = pItem->getItemClass();

            GearSlotInfo* pGearSlotInfo = new GearSlotInfo();
            pItem->makePCItemInfo(*pGearSlotInfo);

            pGearSlotInfo->setSlotID(i);

            // Main Color of the top and the bottom is simply set to 0 for now.
            //			pGearSlotInfo->setMainColor(0);

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
void Vampire::sendVampireSkillInfo()

{
    __BEGIN_TRY
    __BEGIN_DEBUG

    VampireSkillInfo* pVampireSkillInfo = new VampireSkillInfo();

    // Current time, used to compute the remaining casting time.
    Timeval currentTime;
    getCurrentTime(currentTime);

    unordered_map<SkillType_t, VampireSkillSlot*>::const_iterator itr = m_SkillSlot.begin();
    for (; itr != m_SkillSlot.end(); itr++) {
        VampireSkillSlot* pVampireSkillSlot = itr->second;
        Assert(pVampireSkillSlot != NULL);

        // Information about basic attack skills such as AttackMelee must not be sent.
        if (pVampireSkillSlot->getSkillType() >= SKILL_DOUBLE_IMPACT) {
            SubVampireSkillInfo* pSubVampireSkillInfo = new SubVampireSkillInfo();
            pSubVampireSkillInfo->setSkillType(pVampireSkillSlot->getSkillType());
            pSubVampireSkillInfo->setSkillTurn(pVampireSkillSlot->getInterval());
            // The casting time field holds the time left until the next casting.
            // pSubVampireSkillInfo->setCastingTime(pVampireSkillSlot->getCastingTime());
            pSubVampireSkillInfo->setCastingTime(pVampireSkillSlot->getRemainTurn(currentTime));

            pVampireSkillInfo->addListElement(pSubVampireSkillInfo);
        }
    }

    GCSkillInfo gcSkillInfo;
    gcSkillInfo.setPCType(PC_VAMPIRE);
    SkillType_t LearnSkillType = de::gameContext().skillInfos().getSkillTypeByLevel(SKILL_DOMAIN_VAMPIRE, m_Level);

    // Check whether there is a skill learnable at the current level.
    if (LearnSkillType != 0) {
        // If a learnable skill exists and has not been learned, say so.
        if (hasSkill(LearnSkillType) == NULL) {
            pVampireSkillInfo->setLearnNewSkill(true);
        }
    }

    gcSkillInfo.addListElement(pVampireSkillInfo);

    m_pPlayer->sendPacket(&gcSkillInfo);

    __END_DEBUG
    __END_CATCH
}


////////////////////////////////////////////////////////////////////////////////
//
//
// Miscellaneous functions
//
//
////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////
// Heartbeat for the items the vampire owns
//////////////////////////////////////////////////////////////////////////////
void Vampire::heartbeat(const Timeval& currentTime)

{
    __BEGIN_TRY
    __BEGIN_DEBUG

    PlayerCreature::heartbeat(currentTime);

    // Recover HP periodically.
    if (m_HPRegenTime < currentTime) {
        Timeval diffTime = timediff(currentTime, m_HPRegenTime);

        if (diffTime.tv_sec > 0) {
            // 1. alive (current HP above 0), and
            // 2. no Coma effect attached, and
            // 3. no Mephisto effect attached.
            if (isAlive() && !isFlag(Effect::EFFECT_CLASS_COMA) &&
                (!isFlag(Effect::EFFECT_CLASS_MEPHISTO) || isFlag(Effect::EFFECT_CLASS_CASKET))) {
                // by sigi. 2002.6.19
                bool bInCasket = isFlag(Effect::EFFECT_CLASS_CASKET);

                HP_t CurHP = m_HP[ATTR_CURRENT];
                HP_t NewHP = 0;

                // While inside a casket,
                // SilverDamage is healed first.
                if (bInCasket && m_SilverDamage > 0) {
                    NewHP = (10 + m_HPRegenBonus) * diffTime.tv_sec;
                    if (isFlag(Effect::EFFECT_CLASS_HAS_BLOOD_BIBLE))
                        NewHP /= 2;

                    int remainSilver = (int)m_SilverDamage - (int)NewHP;

                    // SilverDamage is fully healed and HP is healed as well.
                    if (remainSilver < 0) {
                        m_SilverDamage = 0;
                        NewHP = -remainSilver;

                        HP_t MaxHP = m_HP[ATTR_MAX];
                        m_HP[ATTR_CURRENT] = min((int)MaxHP, (int)(CurHP + NewHP));
                    }
                    // Only SilverDamage is reduced.
                    else {
                        m_SilverDamage = remainSilver;
                    }
                } else {
                    HP_t MaxHP = m_HP[ATTR_MAX] - getSilverDamage();

                    // Normal       : 2
                    // Burrow(Hide) : 4
                    // Casket       : 6
                    // Wolf         : 2 (treated as the normal state)
                    // Bat          : 0
                    if (isFlag(Effect::EFFECT_CLASS_HIDE)) {
                        NewHP = (4 + m_HPRegenBonus) * diffTime.tv_sec;
                    } else if (isFlag(Effect::EFFECT_CLASS_TRANSFORM_TO_BAT)) {
                        NewHP = 0;
                    }
                    // by sigi. 2002.6.19
                    else if (isFlag(Effect::EFFECT_CLASS_CASKET)) {
                        NewHP = (10 + m_HPRegenBonus) * diffTime.tv_sec;
                    } else {
                        NewHP = (2 + m_HPRegenBonus) * diffTime.tv_sec;
                    }

                    if (isFlag(Effect::EFFECT_CLASS_HAS_BLOOD_BIBLE))
                        NewHP /= 2;
                    m_HP[ATTR_CURRENT] = min((int)MaxHP, (int)(CurHP + NewHP));
                }
            }

            m_HPRegenTime.tv_sec = m_HPRegenTime.tv_sec + diffTime.tv_sec;
            m_HPRegenTime.tv_usec = m_HPRegenTime.tv_usec;
        }
    }

    __END_DEBUG
    __END_CATCH
}

void Vampire::getVampireRecord(VAMPIRE_RECORD& record) const

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
string Vampire::toString() const

{
    __BEGIN_TRY

    StringStream msg;

    msg << "Vampire("
        //<< "ObjectID:"   << (int)getObjectID()
        << ",Name:" << m_Name << ",BatColor:" << (int)m_BatColor << ",SkinColor:" << (int)m_SkinColor
        << ",STR:" << (int)m_STR[ATTR_CURRENT] << "/" << (int)m_STR[ATTR_MAX] << ",DEX:" << (int)m_DEX[ATTR_CURRENT]
        << "/" << (int)m_DEX[ATTR_MAX] << ",INT:" << (int)m_INT[ATTR_CURRENT] << "/" << (int)m_INT[ATTR_MAX]
        << ",HP:" << (int)m_HP[ATTR_CURRENT] << "/" << (int)m_HP[ATTR_MAX] << ",Fame:"
        << (int)m_Fame
        //		<< ",Exp:"       << (int)m_Exp
        //		<< ",ExpOffset:" << (int)m_ExpOffset
        << ",Rank:" << (int)getRank() << ",RankGoalExp:" << (int)getRankGoalExp() << ",Level:" << (int)m_Level
        << ",Bonus:"
        << (int)m_Bonus
        //<< ",InMagics:'" << ??? << "'"
        << ",Gold:" << (int)m_Gold << ",ZoneID:" << (int)getZoneID() << ",XCoord:" << (int)m_X << ",YCoord:" << (int)m_Y
        << ",Sight:" << (int)m_Sight << ")";

    return msg.toString();

    __END_CATCH
}

void Vampire::saveSkills(void) const {
    saveSkillSlots(m_SkillSlot);
}

void Vampire::saveGears(void) const

{
    __BEGIN_TRY

    // Save the items being worn.
    char pField[80];

    for (int i = 0; i < Vampire::VAMPIRE_WEAR_MAX; i++) {
        Item* pItem = m_pWearItem[i];
        if (pItem != NULL) {
            Durability_t maxDurability = computeMaxDurability(pItem);
            if (pItem->getDurability() < maxDurability) {
                // pItem->save(m_Name, STORAGE_GEAR, 0, i, 0);
                //  Item save optimization.
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
// Makes login processing faster.
//----------------------------------------------------------------------
// Representing 32 kinds in 32 bits is considered enough for now.
// If that ever overflows, a bitset will be needed.
//
// (!) The color holds an optionType, not an index color value.
//     The client looks the color value up from the option.
//
// colors[1] holds coatColor only.
//----------------------------------------------------------------------
void Vampire::getShapeInfo(DWORD& flag, Color_t colors[PCVampireInfo::VAMPIRE_COLOR_MAX]) const
//
{
    __BEGIN_DEBUG

    Item* pItem;
    // OptionInfo* 				pOptionInfo;
    int vampireColor;
    WearPart Part;

    // Initialize.
    flag = 0;

    //-----------------------------------------------------------------
    // Outfit
    //-----------------------------------------------------------------
    Part = WEAR_BODY;
    pItem = m_pWearItem[Part];
    vampireColor = 0;

    if (pItem != NULL && m_pRealWearingCheck[Part]) {
        ItemType_t IType = pItem->getItemType();

        colors[vampireColor] = getItemShapeColor(pItem);

        // colors[vampireColor] = pItem->getOptionType();
        // flag |= (getVampireCoatType(IType) << vampireBit);

        // Store the itemType.
        flag = IType;
    } else {
        colors[vampireColor] = 377;
        // flag |= (VAMPIRE_COAT_BASIC << vampireBit);
        //  Default outfit :  male is 0, female is 1
        flag = (m_Sex ? 0 : 1);
    }

    __END_DEBUG
}


bool Vampire::addShape(Item::ItemClass IClass, ItemType_t IType, Color_t color) {
    bool bisChange = false;

    switch (IClass) {
    case Item::ITEM_CLASS_VAMPIRE_COAT: {
        bisChange = true;

        m_VampireInfo.setCoatColor(color);
        m_VampireInfo.setCoatType(IType);
    } break;

    default:
        break;
    }

    return bisChange;
}


bool Vampire::removeShape(Item::ItemClass IClass, bool bSendPacket) {
    bool bisChange = false;

    switch (IClass) {
    case Item::ITEM_CLASS_VAMPIRE_COAT: {
        m_VampireInfo.setCoatColor(377);
        m_VampireInfo.setCoatType(0);

        if (bSendPacket) // by sigi. 2002.11.6
        {
            GCTakeOff pkt;
            pkt.setObjectID(getObjectID());
            pkt.setSlotID((SlotID_t)ADDON_COAT);
            m_pZone->broadcastPacket(getX(), getY(), &pkt, this);
        }
    } break;

    default:
        return false;
    }

    return bisChange;
}


void Vampire::initPetQuestTarget() {
    int minClass = 1, maxClass = 1;

    if (getLevel() <= 50) {
        minClass = 8;
        maxClass = 9;
    } else if (getLevel() <= 60) {
        minClass = maxClass = 9;
    } else if (getLevel() <= 70) {
        minClass = maxClass = 10;
    } else if (getLevel() <= 80) {
        minClass = 10;
        maxClass = 11;
    } else if (getLevel() <= 90) {
        minClass = 10;
        maxClass = 11;
    } else if (getLevel() <= 110) {
        minClass = 11;
        maxClass = 12;
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
