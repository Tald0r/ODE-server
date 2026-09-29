//////////////////////////////////////////////////////////////////////////////
// Filename    : Slayer.cpp
// Written By  : Elca
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "Slayer.h"

#include "EffectManager.h"
#include "Player.h"
#include "repository/CharacterRepository.h"
#include "repository/GoldRepository.h"
#include "repository/SkillSaveRepository.h"
#include "repository/StashRepository.h"
// #include <algo.h>
#include <stdio.h>
#include <stdlib.h>

#include <algorithm>

#include "AbilityBalance.h"
#include "CastleInfoManager.h"
#include "CastleSkillInfo.h"
#include "CreatureUtil.h"
#include "EffectLoaderManager.h"
#include "FlagSet.h"
#include "GameContext.h"
#include "GamePlayer.h"
#include "ItemInfoManager.h"
#include "OptionInfo.h"
#include "PKZoneInfoManager.h"
#include "ParkingCenter.h"
#include "ResurrectLocationManager.h"
#include "SkillDomainInfoManager.h"
#include "SkillHandlerManager.h"
#include "SkillInfo.h"
#include "Stash.h"
#include "TradeManager.h"
#include "WarSystem.h"
// #include "AttrBalanceInfo.h"
#include "ItemUtil.h"
#include "PacketUtil.h"
#include "Party.h"
#include "Shape.h"
#include "VariableManager.h"
// #include "RankEXPInfo.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include "EffectGrandMasterSlayer.h"
#include "GCAddEffect.h"
#include "GCChangeShape.h"
#include "GCModifyInformation.h"
#include "GCOtherModifyInfo.h"
#include "GCPetStashList.h"
#include "GCRealWearingInfo.h"
#include "GCRemoveEffect.h"
#include "GCSkillInfo.h"
#include "GCStatusCurrentHP.h"
#include "GCTakeOff.h"
#include "GCTradeFinish.h"
#include "LoginServerManager.h"
#include "Properties.h"
#include "item/AR.h"
#include "item/Belt.h"
#include "item/Motorcycle.h"
#include "item/SG.h"
#include "item/SMG.h"
#include "item/SR.h"
#include "item/Skull.h"
#include "skill/CastleSkillSlot.h"
#include "skill/EffectBless.h"
#include "skill/EffectChargingPower.h"
#include "skill/EffectDancingSword.h"
#include "skill/EffectDoom.h"
#include "skill/EffectGhostBlade.h"
#include "skill/EffectParalyze.h"
#include "skill/EffectPotentialExplosion.h"
#include "skill/EffectProtectionFromAcid.h"
#include "skill/EffectProtectionFromCurse.h"
#include "skill/EffectProtectionFromPoison.h"
// #include "RankEXPInfo.h"
#include "AdvancementClassExpTable.h"
#include "DynamicZone.h"
#include "ExpFwd.h"
#include "GuildUnion.h"
#include "MonsterInfo.h"
#include "RaceWarLimiter.h"
#include "RankExpTable.h"
#include "SlayerAttrExpTable.h"
#include "Store.h"
#include "SystemAvailabilitiesManager.h"
#include "TimeLimitItemManager.h"
#include "domain/EquipRequirement.h"
#include "skill/SkillUtil.h"
#include "types/ServerType.h"

const Color_t UNIQUE_OPTION = 0xFFFF;

const Color_t QUEST_COLOR = 0xFFFE;
const Color_t QUEST_OPTION = 0xFFFE;

Slayer::Slayer()

    : PlayerCreature(0, NULL), m_pRealWearingCheck{} {
    __BEGIN_TRY

    m_CClass = CREATURE_CLASS_SLAYER;

    m_Mutex.setName("Slayer");

    // Insert the basic skills such as AttackMelee.
    for (int i = 0; i < SKILL_DOUBLE_IMPACT; i++) {
        SkillSlot* pSkillSlot = new SkillSlot;
        // pSkillSlot = new SkillSlot;	// 2002.1.16  by sigi
        pSkillSlot->setName(m_Name);

        pSkillSlot->setSkillType(i);
        pSkillSlot->setInterval(5);
        pSkillSlot->setExpLevel(1);
        pSkillSlot->setExp(1);
        pSkillSlot->setRunTime();
        addSkill(pSkillSlot);
    }

    for (int i = 0; i < WEAR_MAX; i++)
        m_pWearItem[i] = NULL;

    // Set the motorcycle pointer to NULL.
    m_pMotorcycle = NULL;

    for (int i = 0; i < MAX_PHONE_SLOT; i++) {
        m_PhoneSlot[i] = 0;
    }

    // Initialize the MP regeneration time.
    getCurrentTime(m_MPRegenTime);

    // Initialize the experience save counts.
    m_DomainExpSaveCount = 0;
    m_AttrExpSaveCount = 0;
    m_SkillExpSaveCount = 0;
    m_FameSaveCount = 0;
    m_AlignmentSaveCount = 0;
    //	m_RankExpSaveCount   = 0;
    m_Gold = 0;

    //	m_pRank = NULL;

    __END_CATCH
}

Slayer::~Slayer()

{
    __BEGIN_TRY

    try {
        if (m_pMotorcycle != NULL) {
            // getOffMotorcycle();

            // Doing this in IncomingPlayerManager causes problems.
            // So just get rid of the motorcycle here.
            if (de::gameContext().parking().hasMotorcycleBox(m_pMotorcycle->getItemID())) {
                de::gameContext().parking().deleteMotorcycleBox(m_pMotorcycle->getItemID());
            }

            m_pMotorcycle = NULL;
        }

        // Build the outfit information in advance.
        DWORD flag;
        Color_t color[PCSlayerInfo::SLAYER_COLOR_MAX];
        getShapeInfo(flag, color);

        char pField[128];
        snprintf(pField, sizeof(pField),
                 "Shape=%u, HelmetColor=%d, JacketColor=%d, PantsColor=%d, WeaponColor=%d, ShieldColor=%d", flag,
                 color[PCSlayerInfo::SLAYER_COLOR_HELMET], color[PCSlayerInfo::SLAYER_COLOR_JACKET],
                 color[PCSlayerInfo::SLAYER_COLOR_PANTS], color[PCSlayerInfo::SLAYER_COLOR_WEAPON],
                 color[PCSlayerInfo::SLAYER_COLOR_SHIELD]);

        // cout << "SAVE = " << pField << endl;

        tinysave(pField);


        // Save the worn items' durability, the experience and the alignment.
        saveGears();
        saveExps();
        saveSkills();

        // Delete the equipped items from memory.
        destroyGears();

        // When this class is deleted the matching trade information must be deleted,
        // and the trade partner must be told about it.
        TradeManager* pTradeManager = m_pZone->getTradeManager();
        TradeInfo* pInfo = pTradeManager->getTradeInfo(getName());
        if (pInfo != NULL) {
            // Delete the trade information.
            pTradeManager->cancelTrade(this);
        }

        // Remove from the global party.
        // On a normal logout CGLogoutHandler calls
        // Zone::deleteCreature(), and even in an abnormal case
        // GamePlayer::disconnect() calls Zone::deleteCreature(),
        // so the local party, the party invitations and the trade
        // information need no attention here.
        deleteAllPartyInfo(this);

        // Delete the skills.
        unordered_map<SkillType_t, SkillSlot*>::iterator itr = m_SkillSlot.begin();
        for (; itr != m_SkillSlot.end(); itr++) {
            SkillSlot* pSkillSlot = itr->second;
            SAFE_DELETE(pSkillSlot);
        }

        m_SkillSlot.clear();

        //		SAFE_DELETE( m_pRank );

    } catch (Throwable& t) {
        filelog("slayerDestructor.txt", "%s", t.toString().c_str());
        throw;
    } catch (exception& e) {
        filelog("slayerDestructor.txt", "Unknown std::exception");
        throw;
    } catch (...) {
        filelog("slayerDestructor.txt", "Unknown ... exception");
        throw;
    }

    m_bDeriveDestructed = true;

    __END_CATCH_NO_RETHROW
}

// Allocate ObjectIDs for the Slayer and its owned items from the
// zone's ObjectRegistry.
void Slayer::registerObject()

{
    __BEGIN_TRY

    Assert(getZone() != NULL);

    // Access the zone's object registry.
    ObjectRegistry& OR = getZone()->getObjectRegistry();

    __ENTER_CRITICAL_SECTION(OR)

    // Every item gets a new OID, so the time-limit item manager's OID map must be cleared.
    if (m_pTimeLimitItemManager != NULL)
        m_pTimeLimitItemManager->clear();

    // Register the Slayer's own OID first.
    OR.registerObject_NOLOCKED(this);

    // Register the OIDs of the inventory items.
    registerInventory(OR);

    // Register the OIDs of the Goods Inventory items.
    registerGoodsInventory(OR);

    // Register the OIDs of the equipped items.
    for (int i = 0; i < WEAR_MAX; i++) {
        Item* pItem = m_pWearItem[i];

        if (pItem != NULL) {
            bool bCheck = true;

            // A two-handed weapon was already registered under WEAR_LEFTHAND,
            // so it does not need registering again.
            if (i == WEAR_RIGHTHAND && isTwohandWeapon(pItem))
                bCheck = false;

            if (bCheck)
                registerItem(pItem, OR);
        }
    }

    // Register the OID of the item held on the mouse cursor.
    Item* pSlotItem = m_pExtraInventorySlot->getItem();
    if (pSlotItem != NULL)
        registerItem(pSlotItem, OR);

    // Register the OID of the motorcycle.
    if (m_pMotorcycle != NULL)
        OR.registerObject_NOLOCKED(m_pMotorcycle);

    m_Garbage.registerObject(OR);

    for (int i = 0; i < MAX_PET_STASH; ++i) {
        Item* pItem = getPetStashItem(i);
        if (pItem != NULL)
            registerItem(pItem, OR);
    }

    __LEAVE_CRITICAL_SECTION(OR)

    m_SlayerInfo.setObjectID(m_ObjectID);
    m_pStore->updateStoreInfo();

    __END_CATCH
}

// Allocate ObjectIDs for the Slayer and its owned items from the
// zone's ObjectRegistry. Split out separately for the initial ItemTrace.
void Slayer::registerInitObject()

{
    __BEGIN_TRY

    Assert(getZone() != NULL);

    // Access the zone's object registry.
    ObjectRegistry& OR = getZone()->getObjectRegistry();

    __ENTER_CRITICAL_SECTION(OR)

    // Every item gets a new OID, so the time-limit item manager's OID map must be cleared.
    if (m_pTimeLimitItemManager != NULL)
        m_pTimeLimitItemManager->clear();

    // Register the Slayer's own OID first.
    OR.registerObject_NOLOCKED(this);

    // Register the OIDs of the inventory items.
    registerInitInventory(OR);

    // Register the OIDs of the Goods Inventory items.
    registerGoodsInventory(OR);

    // Register the OIDs of the equipped items.
    for (int i = 0; i < WEAR_MAX; i++) {
        Item* pItem = m_pWearItem[i];

        if (pItem != NULL) {
            // Decide whether to leave an ItemTrace.
            pItem->setTraceItem(bTraceLog(pItem));

            bool bCheck = true;

            // A two-handed weapon was already registered under WEAR_LEFTHAND,
            // so it does not need registering again.
            if (i == WEAR_RIGHTHAND && isTwohandWeapon(pItem))
                bCheck = false;

            if (bCheck)
                registerItem(pItem, OR);
        }
    }

    // Register the OID of the item held on the mouse cursor.
    Item* pSlotItem = m_pExtraInventorySlot->getItem();
    if (pSlotItem != NULL) {
        // Decide whether to leave an ItemTrace.
        pSlotItem->setTraceItem(bTraceLog(pSlotItem));
        registerItem(pSlotItem, OR);
    }

    // Register the OID of the motorcycle.
    if (m_pMotorcycle != NULL)
        OR.registerObject_NOLOCKED(m_pMotorcycle);

    m_Garbage.registerObject(OR);

    __LEAVE_CRITICAL_SECTION(OR)

    m_SlayerInfo.setObjectID(m_ObjectID);

    __END_CATCH
}


// Check the time-limited items.
// Every item must already be registered.
void Slayer::checkItemTimeLimit() {
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
                    // Find the current item in the list of checked items.
                    list<Item*>::iterator itr = find(ItemList.begin(), ItemList.end(), pItem);

                    if (itr == ItemList.end()) {
                        i += pItem->getVolumeWidth() - 1;

                        if (wasteIfTimeLimitExpired(pItem)) {
                            m_pInventory->deleteItem(pItem->getObjectID());
                            SAFE_DELETE(pItem);
                        } else {
                            // If the item is not in the list, put it into
                            // the list so that the same item is not
                            // checked twice.
                            ItemList.push_back(pItem);
                        }
                    }
                }
            }
        }
    }

    // Search among the equipped items.
    {
        for (int i = 0; i < WEAR_MAX; i++) {
            Item* pItem = m_pWearItem[i];

            if (pItem != NULL) {
                bool bCheck = true;

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

    // Check the item held on the mouse cursor.
    {
        Item* pSlotItem = m_pExtraInventorySlot->getItem();
        if (pSlotItem != NULL && wasteIfTimeLimitExpired(pSlotItem)) {
            deleteItemFromExtraInventorySlot();
            SAFE_DELETE(pSlotItem);
        }
    }

    // The motorcycle is left out of the sweep: it carries no time limit.

    __END_CATCH
}

void Slayer::updateEventItemTime(DWORD time) {
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
                    // Find the current item in the list of checked items.
                    list<Item*>::iterator itr = find(ItemList.begin(), ItemList.end(), pItem);

                    if (itr == ItemList.end()) {
                        i += pItem->getVolumeWidth() - 1;

                        updateItemTimeLimit(pItem, time);

                        // If the item is not in the list, put it into
                        // the list so that the same item is not
                        // checked twice.
                        ItemList.push_back(pItem);
                    }
                }
            }
        }
    }

    // Search among the equipped items.
    {
        for (int i = 0; i < WEAR_MAX; i++) {
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

    // Check the item held on the mouse cursor.
    {
        Item* pSlotItem = m_pExtraInventorySlot->getItem();
        if (pSlotItem != NULL) {
            updateItemTimeLimit(pSlotItem, time);
        }
    }

    __END_CATCH
}

void Slayer::loadOwnedItems() {
    de::gameContext().itemLoaders().load(this);
}

// A slayer carries the newbie flag turned on until its starting set has
// been given.
void Slayer::giveNewbieItems() {
    if (m_pFlagSet->isOn(FLAGSET_RECEIVE_NEWBIE_ITEM_AUTO)) {
        addNewbieItemToInventory(this);
        addNewbieGoldToInventory(this);
        addNewbieItemToGear(this);
        m_pFlagSet->turnOff(FLAGSET_RECEIVE_NEWBIE_ITEM_AUTO);
        m_pFlagSet->save(getName());
    }
}

bool Slayer::load()

{
    __BEGIN_TRY

    if (!PlayerCreature::load())
        return false;

    for (int i = 0; i < SKILL_DOMAIN_VAMPIRE; i++) {
        m_SkillDomainLevels[i] = 0;
        //		m_SkillDomainExps[i]   = 0;
    }

    SlayerLoadRecord record;
    if (!defaultCharacterRepository().loadSlayer(m_Name, record))
        return false;

    setName(record.name);

    Level_t advLevel = record.advancementClass;
    Exp_t advGoalExp = record.advancementGoalExp;

    m_pAdvancementClass =
        new AdvancementClass(advLevel, advGoalExp, AdvancementClassExpTable::s_AdvancementClassExpTable);
    if (getAdvancementClassLevel() > 0)
        m_bAdvanced = true;

    int competence = record.competence;
    if (competence >= 4)
        competence = 3;

    setCompetence(competence);
    setCompetenceShape(record.competenceShape);
    setSex(record.sex);
    setMasterEffectColor(record.masterEffectColor);
    setHairStyle(record.hairStyle);
    setHairColor(record.hairColor);
    setSkinColor(record.skinColor);
    setPhoneNumber(atoi(record.phone.c_str()));

    m_STR[ATTR_BASIC] = record.str;
    m_STR[ATTR_MAX] = m_STR[ATTR_BASIC];
    m_STR[ATTR_CURRENT] = m_STR[ATTR_BASIC];
    //		m_STRExp            = pResult->getInt(++i);
    Exp_t STRGoalExp = record.strGoalExp;

    m_pAttrs[ATTR_KIND_STR] =
        new Attr(m_STR[ATTR_BASIC], STRGoalExp, SlayerAttrExpTable::s_SlayerAttrExpTable[ATTR_KIND_STR]);

    m_DEX[ATTR_BASIC] = record.dex;
    m_DEX[ATTR_MAX] = m_DEX[ATTR_BASIC];
    m_DEX[ATTR_CURRENT] = m_DEX[ATTR_BASIC];
    //		m_DEXExp            = pResult->getInt(++i);
    Exp_t DEXGoalExp = record.dexGoalExp;

    m_pAttrs[ATTR_KIND_DEX] =
        new Attr(m_DEX[ATTR_BASIC], DEXGoalExp, SlayerAttrExpTable::s_SlayerAttrExpTable[ATTR_KIND_DEX]);

    m_INT[ATTR_BASIC] = record.inte;
    m_INT[ATTR_MAX] = m_INT[ATTR_BASIC];
    m_INT[ATTR_CURRENT] = m_INT[ATTR_BASIC];
    //		m_INTExp            = pResult->getInt(++i);
    Exp_t INTGoalExp = record.intGoalExp;

    m_pAttrs[ATTR_KIND_INT] =
        new Attr(m_INT[ATTR_BASIC], INTGoalExp, SlayerAttrExpTable::s_SlayerAttrExpTable[ATTR_KIND_INT]);

    m_AdvancedSTR = record.advancedSTR;
    m_AdvancedDEX = record.advancedDEX;
    m_AdvancedINT = record.advancedINT;
    m_AdvancedAttrBonus = record.bonus;

    Rank_t CurRank = record.rank;
    //		RankExp_t RankExp            = pResult->getInt(++i);
    RankExp_t RankGoalExp = record.rankGoalExp;

    m_pRank = new Rank(CurRank, RankGoalExp, RankExpTable::s_RankExpTables[RANK_TYPE_SLAYER]);
    //		cout << getRankGoalExp() << endl;

    m_HP[ATTR_CURRENT] = record.currentHP;
    m_HP[ATTR_MAX] = record.maxHP;
    m_HP[ATTR_BASIC] = 0;

    m_MP[ATTR_CURRENT] = record.currentMP;
    m_MP[ATTR_MAX] = record.maxMP;
    m_MP[ATTR_BASIC] = 0;

    setFame(record.fame);
    setGold(record.gold);
    setGuildID(record.guildID);
    // setInMagics(pResult->getString(++i));

    setSkillDomainLevel(SKILL_DOMAIN_BLADE, record.bladeLevel);
    //		setSkillDomainExp  (SKILL_DOMAIN_BLADE,   pResult->getInt(++i));
    setGoalExp(SKILL_DOMAIN_BLADE, record.bladeGoalExp);
    setSkillDomainLevel(SKILL_DOMAIN_SWORD, record.swordLevel);
    //		setSkillDomainExp  (SKILL_DOMAIN_SWORD,   pResult->getInt(++i));
    setGoalExp(SKILL_DOMAIN_SWORD, record.swordGoalExp);
    setSkillDomainLevel(SKILL_DOMAIN_GUN, record.gunLevel);
    //		setSkillDomainExp  (SKILL_DOMAIN_GUN,     pResult->getInt(++i));
    setGoalExp(SKILL_DOMAIN_GUN, record.gunGoalExp);
    setSkillDomainLevel(SKILL_DOMAIN_ENCHANT, record.enchantLevel);
    //		setSkillDomainExp  (SKILL_DOMAIN_ENCHANT, pResult->getInt(++i));
    setGoalExp(SKILL_DOMAIN_ENCHANT, record.enchantGoalExp);
    setSkillDomainLevel(SKILL_DOMAIN_HEAL, record.healLevel);
    //		setSkillDomainExp  (SKILL_DOMAIN_HEAL,    pResult->getInt(++i));
    setGoalExp(SKILL_DOMAIN_HEAL, record.healGoalExp);
    setSkillDomainLevel(SKILL_DOMAIN_ETC, record.etcLevel);
    //		setSkillDomainExp  (SKILL_DOMAIN_ETC,     pResult->getInt(++i));
    setGoalExp(SKILL_DOMAIN_ETC, record.etcGoalExp);

    //		setZoneID(pResult->getInt(++i));
    ZoneID_t zoneID = record.zoneID;
    setX(record.x);
    setY(record.y);

    setSight(record.sight);
    setSight(13);
    setGunBonusExp(record.gunBonusExp);
    setRifleBonusExp(record.rifleBonusExp);
    setAlignment(record.alignment);

    //		for (int j = 0; j < 4; j++) setHotKey(j, pResult->getInt(++i));

    setStashGold(record.stashGold);
    setStashNum(record.stashNum);
    setResurrectZoneID(record.resurrectZone);

    // record.reward is not consumed: the reward flow that read it is dead.
    setSMSCharge(record.smsCharge);

    // Just recompute it. 2002.7.15 by sigi
    // If the formula changes, computeHP in AbilityBalance.cpp must change too.
    //		m_HP[ATTR_MAX]      = m_STR[ATTR_CURRENT]*2;

    try {
        setZoneID(zoneID);
    } catch (Error& e) {
        // Treated as the guild-hideout problem:
        // a guild hideout exists on one game server only, so a character
        // connecting to another game server cannot enter it.
        ZONE_COORD ResurrectCoord;
        de::gameContext().resurrectLocations().getSlayerPosition(12, ResurrectCoord);
        setZoneID(ResurrectCoord.id);
        setX(ResurrectCoord.x);
        setY(ResurrectCoord.y);
    }

    // Access the zone's object registry.
    ObjectRegistry& OR = getZone()->getObjectRegistry();
    OR.registerObject(this);

    // Build the Slayer Outlook Information.
    m_SlayerInfo.setObjectID(m_ObjectID);
    m_SlayerInfo.setName(m_Name);
    m_SlayerInfo.setX(m_X);
    m_SlayerInfo.setY(m_Y);
    m_SlayerInfo.setDir(m_Dir);
    m_SlayerInfo.setSex(m_Sex);
    m_SlayerInfo.setHairStyle(m_HairStyle);

    // A competence of 0 or 1 must be drawn with the
    // game master sprite.
    m_SlayerInfo.setCompetence(m_CompetenceShape);

    // Load the learned skills.
    vector<SlayerSkillRow> skillRows = defaultSkillSaveRepository().loadSlayerSkills(m_Name);
    for (size_t r = 0; r < skillRows.size(); r++) {
        const SlayerSkillRow& row = skillRows[r];
        SkillSlot* pSkillSlot = new SkillSlot();

        pSkillSlot->setName(m_Name);
        pSkillSlot->setSkillType(row.skillType);
        pSkillSlot->setExpLevel(row.skillLevel);
        pSkillSlot->setExp(row.skillExp);

        pSkillSlot->setInterval(row.delay);

        pSkillSlot->setCastingTime(row.castingTime);
        // pSkillSlot->setRunTime (pResult->getInt(++i));
        pSkillSlot->setRunTime();

        // Check whether this skill can be used: fetch its SkillInfo.
        SkillInfo* pSkillInfo = de::gameContext().skillInfos().getSkillInfo(pSkillSlot->getSkillType());
        Assert(pSkillInfo != NULL);

        // If the current domain level is below the level the skill is
        // learned at, it obviously cannot be used.
        if (pSkillInfo->getLevel() > m_SkillDomainLevels[pSkillInfo->getDomainType()] &&
            pSkillInfo->getDomainType() != SKILL_DOMAIN_ETC) {
            pSkillSlot->setDisable();
        }

        addSkill(pSkillSlot);
    }

    // Load the effects.
    de::gameContext().effectLoaders().load(this);

    // Load the Rank Bonus.
    loadRankBonus();

    // Attach an Effect for a GrandMaster.
    // by sigi. 2002.11.8
    if (getHighestSkillDomainLevel() >= 100 && SystemAvailabilitiesManager::getInstance()->isAvailable(
                                                   SystemAvailabilitiesManager::SYSTEM_GRAND_MASTER_EFFECT)) {
        if (!isFlag(Effect::EFFECT_CLASS_GRAND_MASTER_SLAYER)) {
            EffectGrandMasterSlayer* pEffect = new EffectGrandMasterSlayer(this);
            pEffect->setDeadline(999999);
            getEffectManager()->addEffect(pEffect);
            setFlag(Effect::EFFECT_CLASS_GRAND_MASTER_SLAYER);
        }
    }


    // Load the flag set.
    m_pFlagSet->load(getName());

    // Build the Slayer Outlook Information.
    m_SlayerInfo.setHelmetType(HELMET_NONE);
    m_SlayerInfo.setJacketType(JACKET_BASIC);
    m_SlayerInfo.setPantsType(PANTS_BASIC);
    m_SlayerInfo.setWeaponType(WEAPON_NONE);
    m_SlayerInfo.setShieldType(SHIELD_NONE);
    m_SlayerInfo.setMotorcycleType(MOTORCYCLE_NONE);
    m_SlayerInfo.setMasterEffectColor(m_HairColor);
    m_SlayerInfo.setHairColor(m_HairColor);
    m_SlayerInfo.setSkinColor(m_SkinColor);
    m_SlayerInfo.setShoulderType(0);

    m_SlayerInfo.setAdvancementLevel(getAdvancementClassLevel());

    // A rank of 0 means the initial value has not been set yet.
    if (getRank() == 0) {
        saveInitialRank();
    }


    // All attributes are loaded, so initialize the derived
    // attributes from them.
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

void Slayer::save() const

{
    __BEGIN_TRY

    __ENTER_CRITICAL_SECTION(m_Mutex)

    // Normally, when no data has changed at all, #Affected Rows == 0.
    // In most cases the data changes at least a little, but it may
    // not - so affected rows must not be checked.
    SlayerVitalsRecord record;
    record.currentHP = m_HP[ATTR_CURRENT];
    record.maxHP = m_HP[ATTR_MAX];
    record.currentMP = m_MP[ATTR_CURRENT];
    record.maxMP = m_MP[ATTR_MAX];
    record.zoneID = getZoneID();
    record.x = (int)m_X;
    record.y = (int)m_Y;
    defaultCharacterRepository().saveSlayerVitals(m_Name, record);

    // Save the effects.
    m_pEffectManager->save(m_Name);

    // Save the motorcycle.
    if (m_pMotorcycle != NULL) {
        // m_pMotorcycle->save("", STORAGE_ZONE, m_pZone->getZoneID(), m_X, m_Y);
        //  by sigi. 2002.5.15
        char pField[80];

        snprintf(pField, sizeof(pField), "OwnerID='', Storage=%d, StorageID=%u, X=%d, Y=%d", STORAGE_ZONE,
                 m_pZone->getZoneID(), m_X, m_Y);
        m_pMotorcycle->tinysave(pField);
    }

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    __END_CATCH
}

PhoneNumber_t Slayer::getPhoneSlotNumber(SlotID_t SlotID)

{
    __BEGIN_TRY

    Assert(SlotID <= MAX_PHONE_SLOT);

    return m_PhoneSlot[SlotID];

    __END_CATCH
}

void Slayer::setPhoneSlotNumber(SlotID_t SlotID, PhoneNumber_t PhoneNumber)

{
    __BEGIN_TRY

    __ENTER_CRITICAL_SECTION(m_Mutex)

    m_PhoneSlot[SlotID] = PhoneNumber;

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    __END_CATCH
}

bool Slayer::isSlotByPhoneNumber(PhoneNumber_t PhoneNumber)

{
    __BEGIN_TRY

    bool isFind = false;

    for (int i = 0; i < MAX_PHONE_SLOT; i++) {
        if (m_PhoneSlot[i] == PhoneNumber)
            isFind = true;
    }

    return isFind;

    __END_CATCH
}

SlotID_t Slayer::getSlotWithPhoneNumber(PhoneNumber_t PhoneNumber)

{
    __BEGIN_TRY

    for (int i = 0; i < MAX_PHONE_SLOT; i++) {
        if (m_PhoneSlot[i] == PhoneNumber)
            return i;
    }

    return MAX_PHONE_SLOT;

    __END_CATCH
}

SlotID_t Slayer::findEmptyPhoneSlot()

{
    __BEGIN_TRY

    for (int i = 0; i < MAX_PHONE_SLOT; i++) {
        if (m_PhoneSlot[i] == 0) {
            return i;
        }
    }

    return MAX_PHONE_SLOT;

    __END_CATCH
}

bool Slayer::isEmptyPhoneSlot()

{
    __BEGIN_TRY

    bool Success = false;

    for (int i = 0; i < MAX_PHONE_SLOT; i++) {
        if (m_PhoneSlot[i] == 0) {
            // Found an empty slot.
            Success = true;
        }
    }

    return Success;

    __END_CATCH
}

// Look up whether a given Skill exists and return its SkillSlot.
SkillSlot* Slayer::getSkill(SkillType_t SkillType) const {
    return findSkillSlot(m_SkillSlot, SkillType);
}

// Register a SkillSlot under its skill type; a duplicate slot is deleted.
void Slayer::addSkill(SkillSlot* pSkillSlot)

{
    __BEGIN_TRY

    SkillType_t SkillType = pSkillSlot->getSkillType();
    switch (SkillType) {
    case SKILL_UN_BURROW:
    case SKILL_UN_TRANSFORM:
    case SKILL_UN_INVISIBILITY:
    case SKILL_THROW_HOLY_WATER:
    case SKILL_EAT_CORPSE:
    case SKILL_HOWL:
        filelog("SlayerError.log", "SkillType[%d], %s", SkillType, toString().c_str());
        Assert(false);
        break;
    default:
        break;
    }

    unordered_map<SkillType_t, SkillSlot*>::iterator itr = m_SkillSlot.find(pSkillSlot->getSkillType());

    if (itr == m_SkillSlot.end()) {
        m_SkillSlot[pSkillSlot->getSkillType()] = pSkillSlot;
    }
    // 2002.1.16 by sigi
    else {
        if (pSkillSlot != itr->second)
            SAFE_DELETE(pSkillSlot);
    }

    __END_CATCH
}

// Only for learning a skill. Calling it anywhere else breaks things.
void Slayer::addSkill(SkillType_t SkillType)

{
    __BEGIN_TRY

    switch (SkillType) {
    case SKILL_UN_BURROW:
    case SKILL_UN_TRANSFORM:
    case SKILL_UN_INVISIBILITY:
    case SKILL_THROW_HOLY_WATER:
    case SKILL_EAT_CORPSE:
    case SKILL_HOWL:
        filelog("SlayerError.log", "2 SkillType[%d], %s", SkillType, toString().c_str());
        Assert(false);
        break;
    default:
        break;
    }

    unordered_map<SkillType_t, SkillSlot*>::iterator itr = m_SkillSlot.find(SkillType);

    if (itr == m_SkillSlot.end()) {
        SkillSlot* pSkillSlot = new SkillSlot();
        pSkillSlot->setName(m_Name);
        pSkillSlot->setSkillType(SkillType);
        // A freshly learned skill starts with no run-time lock and a zero
        // interval -- the seeded MaxDelay leaked to the client as a sticky
        // per-cast cooldown via the skill-info refresh; the first
        // successful cast installs the real formula delay. See
        // Vampire::addSkill for the full story.
        pSkillSlot->setRunTime(0, false);
        pSkillSlot->setInterval(0);
        pSkillSlot->setExpLevel(0);
        pSkillSlot->setExp(1);
        pSkillSlot->create(m_Name);

        m_SkillSlot[SkillType] = pSkillSlot;
    }

    __END_CATCH
}

void Slayer::removeCastleSkill(SkillType_t SkillType) {
    removeCastleSkillSlot<SkillSlot, CastleSkillSlot>(m_SkillSlot, SkillType);
}

void Slayer::removeAllCastleSkill() {
    removeAllCastleSkillSlots(m_SkillSlot);
}

// Slayer::wearItem()
// Equip an Item in the gear window and compute the attributes.
// This wearItem is the one used while loading Items at connection time.
// It is better not to broadcast inside this method.
void Slayer::wearItem(WearPart Part, Item* pItem)

{
    __BEGIN_TRY
    __BEGIN_DEBUG

    Assert(pItem != NULL);

    Item::ItemClass IClass = pItem->getItemClass();
    Item* pLeft = NULL;
    Item* pRight = NULL;
    Item* pPrevItem = NULL;
    OptionInfo* pOptionInfo = NULL;

    // Take the colour of the first option.
    if (pItem->getFirstOptionType() != 0)
        pOptionInfo = de::gameContext().optionInfos().getOptionInfo(pItem->getFirstOptionType());

    // By the current design an item can always be used even when the
    // attributes are too low, but then the item's attribute bonuses do
    // not apply. So the item is put into the matching gear slot first.
    // A two-handed weapon puts one item pointer into both hand slots.
    if (isTwohandWeapon(pItem)) {
        // Both hands hold an item.
        if (isWear(WEAR_RIGHTHAND) && isWear(WEAR_LEFTHAND)) {
            pLeft = getWearItem(WEAR_RIGHTHAND);
            pRight = getWearItem(WEAR_LEFTHAND);

            // A two-handed weapon is held.
            if (pLeft == pRight) {
                // Put the requested item into the gear slot,
                m_pWearItem[WEAR_RIGHTHAND] = pItem;
                m_pWearItem[WEAR_LEFTHAND] = pItem;

                // by sigi. 2002.5.15
                char pField[80];
                // pItem->save(m_Name, STORAGE_GEAR, 0, Part, 0);
                snprintf(pField, sizeof(pField), "Storage=%d, X=%d", STORAGE_GEAR, Part);
                pItem->tinysave(pField);

                // and hang the previous item on the mouse cursor.
                addItemToExtraInventorySlot(pLeft);
                // pLeft->save(m_Name, STORAGE_EXTRASLOT, 0, 0, 0);
                snprintf(pField, sizeof(pField), "Storage=%d", STORAGE_EXTRASLOT);
                pLeft->tinysave(pField);
            }
            // A sword and a shield are held.
            else {
                // With a sword and a shield in both hands and a two-handed
                // weapon incoming, the sword can go on the mouse cursor but
                // there is nowhere to put the shield. It would have to go
                // into the inventory; for now just refuse to equip it.
                cerr << "Holding a sword and a shield, so a two-handed weapon cannot be equipped." << endl;
                return;
            }
        }
        // Not both hands hold an item.
        else {
            char pField[80];

            // The right hand holds an item.
            if (isWear(WEAR_RIGHTHAND)) {
                pRight = getWearItem(WEAR_RIGHTHAND);
                // Put the requested item into the gear slot.
                m_pWearItem[WEAR_RIGHTHAND] = pItem;
                m_pWearItem[WEAR_LEFTHAND] = pItem;

                // by sigi. 2002.5.15
                // pItem->save(m_Name, STORAGE_GEAR, 0, Part, 0);
                snprintf(pField, sizeof(pField), "Storage=%d, X=%d", STORAGE_GEAR, Part);
                pItem->tinysave(pField);

                // Hang the previous item on the mouse cursor.
                addItemToExtraInventorySlot(pRight);
                // pRight->save(m_Name, STORAGE_EXTRASLOT, 0, 0, 0);
                snprintf(pField, sizeof(pField), "Storage=%d", STORAGE_EXTRASLOT);
                pRight->tinysave(pField);
            }
            // The left hand holds an item.
            else if (isWear(WEAR_LEFTHAND)) {
                pLeft = getWearItem(WEAR_LEFTHAND);
                // Put the requested item into the gear slot.
                m_pWearItem[WEAR_RIGHTHAND] = pItem;
                m_pWearItem[WEAR_LEFTHAND] = pItem;

                // by sigi. 2002.5.15
                // pItem->save(m_Name, STORAGE_GEAR, 0, Part, 0);
                snprintf(pField, sizeof(pField), "Storage=%d, X=%d", STORAGE_GEAR, Part);
                pItem->tinysave(pField);

                // Hang the previous item on the mouse cursor.
                addItemToExtraInventorySlot(pLeft);
                // pLeft->save(m_Name, STORAGE_EXTRASLOT, 0, 0, 0);
                snprintf(pField, sizeof(pField), "Storage=%d", STORAGE_EXTRASLOT);
                pLeft->tinysave(pField);
            }
            // Neither hand holds an item.
            else {
                // Put the requested item into the gear slot.
                m_pWearItem[WEAR_RIGHTHAND] = pItem;
                m_pWearItem[WEAR_LEFTHAND] = pItem;

                // by sigi. 2002.5.15
                // pItem->save(m_Name, STORAGE_GEAR, 0, Part, 0);
                snprintf(pField, sizeof(pField), "Storage=%d, X=%d", STORAGE_GEAR, Part);
                pItem->tinysave(pField);
            }
        }
    } else {
        char pField[80];

        if (isWear(Part)) {
            pPrevItem = getWearItem(Part);
            // Put the requested item into the gear slot.
            m_pWearItem[Part] = pItem;

            // by sigi. 2002.5.15
            // pItem->save(m_Name, STORAGE_GEAR, 0, Part, 0);
            snprintf(pField, sizeof(pField), "Storage=%d, X=%d", STORAGE_GEAR, Part);
            pItem->tinysave(pField);

            // Hang the previous item on the mouse cursor.
            addItemToExtraInventorySlot(pPrevItem);

            // pPrevItem->save(m_Name, STORAGE_EXTRASLOT, 0, 0, 0);
            snprintf(pField, sizeof(pField), "Storage=%d", STORAGE_EXTRASLOT);
            pPrevItem->tinysave(pField);
        } else {
            // Put the requested item into the gear slot.
            m_pWearItem[Part] = pItem;
            // pItem->save(m_Name, STORAGE_GEAR, 0, Part, 0);
            snprintf(pField, sizeof(pField), "Storage=%d, X=%d", STORAGE_GEAR, Part);
            pItem->tinysave(pField);
        }
    }

    ItemType_t IType = pItem->getItemType();

    // Record which weapon is being held.
    // It must be set in SlayerInfo so the next viewer is sent it.
    Color_t color = getItemShapeColor(pItem, pOptionInfo);

    switch (IClass) {
    case Item::ITEM_CLASS_MACE:
        // m_SlayerInfo.setWeaponType(WEAPON_MACE);
        m_SlayerInfo.setWeaponType(slayerWeaponShape(WEAPON_MACE, IType));
        m_SlayerInfo.setWeaponColor(color);
        break;
    case Item::ITEM_CLASS_CROSS:
        m_SlayerInfo.setWeaponType(slayerWeaponShape(WEAPON_CROSS, IType));
        m_SlayerInfo.setWeaponColor(color);
        break;
    case Item::ITEM_CLASS_BLADE:
        m_SlayerInfo.setWeaponType(slayerWeaponShape(WEAPON_BLADE, IType));
        m_SlayerInfo.setWeaponColor(color);
        break;
    case Item::ITEM_CLASS_AR:
        m_SlayerInfo.setWeaponType(slayerWeaponShape(WEAPON_AR, IType));
        m_SlayerInfo.setWeaponColor(color);
        break;
    case Item::ITEM_CLASS_SR:
        m_SlayerInfo.setWeaponType(slayerWeaponShape(WEAPON_SR, IType));
        m_SlayerInfo.setWeaponColor(color);
        break;
    case Item::ITEM_CLASS_SMG:
        m_SlayerInfo.setWeaponType(slayerWeaponShape(WEAPON_SMG, IType));
        m_SlayerInfo.setWeaponColor(color);
        break;
    case Item::ITEM_CLASS_SG:
        m_SlayerInfo.setWeaponType(slayerWeaponShape(WEAPON_SG, IType));
        m_SlayerInfo.setWeaponColor(color);
        break;
    case Item::ITEM_CLASS_HELM:
        m_SlayerInfo.setHelmetType(getHelmetType(IType));
        m_SlayerInfo.setHelmetColor(color);
        break;
    case Item::ITEM_CLASS_SHIELD:
        m_SlayerInfo.setShieldType(getShieldType(IType));
        m_SlayerInfo.setShieldColor(color);
        break;
    case Item::ITEM_CLASS_SWORD:
        m_SlayerInfo.setWeaponType(slayerWeaponShape(WEAPON_SWORD, IType));
        m_SlayerInfo.setWeaponColor(color);
        break;
    case Item::ITEM_CLASS_COAT:
        m_SlayerInfo.setJacketType(getJacketType(IType));
        // cout << "Jacket: ItemType=" << (int)IType << ", JacketType=" <<
        // JacketType2String[m_SlayerInfo.getJacketType()] << endl;
        m_SlayerInfo.setJacketColor(color);
        break;
    case Item::ITEM_CLASS_TROUSER:
        m_SlayerInfo.setPantsType(getPantsType(IType));
        // cout << "Pants: ItemType=" << (int)IType << ", PantsType=" << PantsType2String[m_SlayerInfo.getPantsType()]
        // << endl;
        m_SlayerInfo.setPantsColor(color);
        break;
    case Item::ITEM_CLASS_SHOULDER_ARMOR:
        m_SlayerInfo.setShoulderType(getShoulderType(IType));
        // cout << "Pants: ItemType=" << (int)IType << ", PantsType=" << PantsType2String[m_SlayerInfo.getPantsType()]
        // << endl;
        m_SlayerInfo.setShoulderColor(color);
        break;
    default:
        break;
    }

    __END_DEBUG
    __END_CATCH
}

// Slayer::wearItem()
// Equip an Item in the gear window and compute the attributes.
void Slayer::wearItem(WearPart Part)

{
    __BEGIN_TRY
    __BEGIN_DEBUG

    // Take the item queued for equipping.
    Item* pItem = getExtraInventorySlotItem();
    Assert(pItem != NULL);

    Item::ItemClass IClass = pItem->getItemClass();
    OptionInfo* pOptionInfo = NULL;
    Item* pLeft = NULL;
    Item* pRight = NULL;
    Item* pPrevItem = NULL;
    GCTakeOff _GCTakeOff;

    // Take the colour of the first option.
    if (pItem->getFirstOptionType() != 0)
        pOptionInfo = de::gameContext().optionInfos().getOptionInfo(pItem->getFirstOptionType());


    if (IClass == Item::ITEM_CLASS_SHIELD)
        Part = WEAR_LEFTHAND;
    if (IClass == Item::ITEM_CLASS_SWORD)
        Part = WEAR_RIGHTHAND;

    // Save the current attributes into a buffer before putting on or
    // taking off gear, so that only the changed ones are sent later.
    SLAYER_RECORD prev;
    getSlayerRecord(prev);

    // By the current design an item can always be used even when the
    // attributes are too low, but then the item's attribute bonuses do
    // not apply. So the item is put into the matching gear slot first.
    // A two-handed weapon puts one item pointer into both hand slots.
    if (isTwohandWeapon(pItem)) {
        // Both hands hold an item.
        if (isWear(WEAR_RIGHTHAND) && isWear(WEAR_LEFTHAND)) {
            pLeft = getWearItem(WEAR_RIGHTHAND);
            pRight = getWearItem(WEAR_LEFTHAND);

            // A two-handed weapon is held.
            if (pLeft == pRight) {
                char pField[80];

                takeOffItem(WEAR_LEFTHAND, false, false);

                // Put the requested item into the gear slot,
                m_pWearItem[WEAR_RIGHTHAND] = pItem;
                m_pWearItem[WEAR_LEFTHAND] = pItem;
                // by sigi. 2002.5.15
                // pItem->save(m_Name, STORAGE_GEAR, 0, Part, 0);
                snprintf(pField, sizeof(pField), "Storage=%d, X=%d", STORAGE_GEAR, Part);
                pItem->tinysave(pField);

                // Remove the requested item from the mouse cursor.
                deleteItemFromExtraInventorySlot();
                // Hang the previous item on the mouse cursor.
                addItemToExtraInventorySlot(pLeft);
                // pLeft->save(m_Name, STORAGE_EXTRASLOT, 0, 0, 0);
                snprintf(pField, sizeof(pField), "Storage=%d", STORAGE_EXTRASLOT);
                pLeft->tinysave(pField);

            }
            // A sword and a shield are held.
            else {
                // With a sword and a shield in both hands and a two-handed
                // weapon incoming, the sword can go on the mouse cursor but
                // there is nowhere to put the shield. It would have to go
                // into the inventory; for now just refuse to equip it.
                return;
            }
        }
        // Not both hands hold an item.
        else {
            // by sigi. 2002.5.15
            char pField[80];

            // The right hand holds an item.
            if (isWear(WEAR_RIGHTHAND)) {
                pRight = getWearItem(WEAR_RIGHTHAND);

                takeOffItem(WEAR_RIGHTHAND, false, false);

                // Put the requested item into the gear slot.
                m_pWearItem[WEAR_RIGHTHAND] = pItem;
                m_pWearItem[WEAR_LEFTHAND] = pItem;
                // pItem->save(m_Name, STORAGE_GEAR, 0, Part, 0);

                // by sigi. 2002.5.15
                snprintf(pField, sizeof(pField), "Storage=%d, X=%d", STORAGE_GEAR, Part);
                pItem->tinysave(pField);

                // Remove the requested item from the mouse cursor.
                deleteItemFromExtraInventorySlot();
                // Hang the previous item on the mouse cursor.
                addItemToExtraInventorySlot(pRight);
                // pRight->save(m_Name, STORAGE_EXTRASLOT, 0, 0, 0);
                snprintf(pField, sizeof(pField), "Storage=%d", STORAGE_EXTRASLOT);
                pRight->tinysave(pField);

            }
            // The left hand holds an item.
            else if (isWear(WEAR_LEFTHAND)) {
                pLeft = getWearItem(WEAR_LEFTHAND);

                takeOffItem(WEAR_LEFTHAND, false, false);

                // Put the requested item into the gear slot.
                m_pWearItem[WEAR_RIGHTHAND] = pItem;
                m_pWearItem[WEAR_LEFTHAND] = pItem;

                // by sigi. 2002.5.15
                // pItem->save(m_Name, STORAGE_GEAR, 0, Part, 0);
                snprintf(pField, sizeof(pField), "Storage=%d, X=%d", STORAGE_GEAR, Part);
                pItem->tinysave(pField);

                // Remove the requested item from the mouse cursor.
                deleteItemFromExtraInventorySlot();
                // Hang the previous item on the mouse cursor.
                addItemToExtraInventorySlot(pLeft);
                // pLeft->save(m_Name, STORAGE_EXTRASLOT, 0, 0, 0);
                snprintf(pField, sizeof(pField), "Storage=%d", STORAGE_EXTRASLOT);
                pLeft->tinysave(pField);
            }
            // Neither hand holds an item.
            else {
                // Put the requested item into the gear slot.
                m_pWearItem[WEAR_RIGHTHAND] = pItem;
                m_pWearItem[WEAR_LEFTHAND] = pItem;

                pItem->save(m_Name, STORAGE_GEAR, 0, Part, 0);
                // Remove the requested item from the mouse cursor.
                deleteItemFromExtraInventorySlot();
            }
        }
    } else {
        char pField[80];

        if (isWear(Part)) {
            pPrevItem = getWearItem(Part);

            takeOffItem(Part, false, false);

            // Put the requested item into the gear slot.
            m_pWearItem[Part] = pItem;
            // by sigi. 2002.5.15
            // pItem->save(m_Name, STORAGE_GEAR, 0, Part, 0);
            snprintf(pField, sizeof(pField), "Storage=%d, X=%d", STORAGE_GEAR, Part);
            pItem->tinysave(pField);

            // Remove the requested item from the mouse cursor.
            deleteItemFromExtraInventorySlot();
            // Hang the previous item on the mouse cursor.
            addItemToExtraInventorySlot(pPrevItem);
            // pPrevItem->save(m_Name, STORAGE_EXTRASLOT, 0, 0, 0);
            snprintf(pField, sizeof(pField), "Storage=%d", STORAGE_EXTRASLOT);
            pPrevItem->tinysave(pField);
        } else {
            // Put the requested item into the gear slot.
            m_pWearItem[Part] = pItem;

            // by sigi. 2002.5.15
            // pItem->save(m_Name, STORAGE_GEAR, 0, Part, 0);
            snprintf(pField, sizeof(pField), "Storage=%d, X=%d", STORAGE_GEAR, Part);
            pItem->tinysave(pField);
            // Remove the requested item from the mouse cursor.
            deleteItemFromExtraInventorySlot();
        }
    }

    // Mark it as worn for now.
    // by sigi. 2002.10.31
    m_pRealWearingCheck[Part] = true;

    initAllStat();
    sendRealWearingInfo();
    sendModifyInfo(prev); // Send the attributes that changed.

    // bool bisWeapon = false;
    // ItemType_t IType = pItem->getItemType();

    Color_t color = getItemShapeColor(pItem, pOptionInfo);

    changeShape(pItem, color);

    // Change the outfit if the item can really be worn.
    if (m_pRealWearingCheck[Part])
    // if (bisChange)
    {
        GCChangeShape _GCChangeShape;
        _GCChangeShape.setObjectID(getObjectID());
        _GCChangeShape.setItemClass(IClass);
        _GCChangeShape.setItemType(pItem->getItemType());
        _GCChangeShape.setOptionType(pItem->getFirstOptionType());
        _GCChangeShape.setAttackSpeed(m_AttackSpeed[ATTR_CURRENT]);

        if (color == QUEST_COLOR)
            _GCChangeShape.setFlag(SHAPE_FLAG_QUEST);

        Zone* pZone = m_pZone;
        pZone->broadcastPacket(m_X, m_Y, &_GCChangeShape, this);

        // cout << _GCChangeShape.toString().c_str()  << endl;
    }

    if (m_pZone != NULL) {
        GCOtherModifyInfo gcOtherModifyInfo;
        makeGCOtherModifyInfo(&gcOtherModifyInfo, this, &prev);

        if (gcOtherModifyInfo.getShortCount() != 0 || gcOtherModifyInfo.getLongCount() != 0) {
            m_pZone->broadcastPacket(m_X, m_Y, &gcOtherModifyInfo, this);
        }
    }

    __END_DEBUG
    __END_CATCH
}

// Slayer::takeOffItem()
// *NOTE : a bool parameter is passed in for now.
//         The code gets messy, so change it later.
void Slayer::takeOffItem(WearPart Part, bool bAddOnMouse, bool bSendModifyInfo)

{
    __BEGIN_TRY
    __BEGIN_DEBUG

    SLAYER_RECORD prev;

    // Take the item in the gear slot.
    Item* pItem = m_pWearItem[Part];
    Assert(pItem != NULL);
    Item::ItemClass IClass = pItem->getItemClass();

    if (Part == WEAR_LEFTHAND || Part == WEAR_RIGHTHAND) {
        if (m_pWearItem[WEAR_RIGHTHAND] && m_pWearItem[WEAR_LEFTHAND]) {
            if (m_pWearItem[WEAR_RIGHTHAND] == m_pWearItem[WEAR_LEFTHAND]) {
                m_pWearItem[WEAR_RIGHTHAND] = NULL;
                m_pWearItem[WEAR_LEFTHAND] = NULL;
            }
        }
    }

    // Remove the item from the gear point.
    if (isTwohandWeapon(pItem)) {
        m_pWearItem[WEAR_RIGHTHAND] = NULL;
        m_pWearItem[WEAR_LEFTHAND] = NULL;
    } else
        m_pWearItem[Part] = NULL;

    // When wearItem finds the given slot already occupied it takes the
    // old item off and puts the new one on, which would send one packet
    // for the removal and another for the wear. To avoid the second
    // packet a bool variable was added.
    if (bSendModifyInfo) {
        getSlayerRecord(prev);
        initAllStat();
        sendRealWearingInfo();
        sendModifyInfo(prev);
    } else {
        initAllStat();
    }

    // A check that should not be needed; a temporary patch.
    // Hang the item on the mouse cursor.
    if (bAddOnMouse) {
        addItemToExtraInventorySlot(pItem);

        // Optimized item save.
        // pItem->save(m_Name, STORAGE_EXTRASLOT, 0, 0, 0);
        char pField[80];

        if (pItem->isSilverWeapon()) {
            if (pItem->isGun()) {
                //				Gun* pGun = dynamic_cast<Gun*>(pItem);
                snprintf(pField, sizeof(pField), "Storage=%d, Durability=%d, BulletCount=%d, Silver=%d",
                         STORAGE_EXTRASLOT, pItem->getDurability(), pItem->getBulletCount(), pItem->getSilver());
            } else {
                snprintf(pField, sizeof(pField), "Storage=%d, Durability=%d, Silver=%d", STORAGE_EXTRASLOT,
                         pItem->getDurability(), pItem->getSilver());
            }
        } else {
            snprintf(pField, sizeof(pField), "Storage=%d, Durability=%d", STORAGE_EXTRASLOT, pItem->getDurability());
        }

        pItem->tinysave(pField);
    }

    GCTakeOff _GCTakeOff;

    switch (IClass) {
    case Item::ITEM_CLASS_MACE:
    case Item::ITEM_CLASS_CROSS:
    case Item::ITEM_CLASS_BLADE:
    case Item::ITEM_CLASS_AR:
    case Item::ITEM_CLASS_SR:
    case Item::ITEM_CLASS_SMG:
    case Item::ITEM_CLASS_SG:
        m_SlayerInfo.setWeaponType(WEAPON_NONE);
        _GCTakeOff.setObjectID(getObjectID());
        _GCTakeOff.setSlotID((SlotID_t)ADDON_RIGHTHAND);
        m_pZone->broadcastPacket(getX(), getY(), &_GCTakeOff, this);
        break;
    case Item::ITEM_CLASS_HELM:
        m_SlayerInfo.setHelmetType(HELMET_NONE);
        _GCTakeOff.setObjectID(getObjectID());
        _GCTakeOff.setSlotID((SlotID_t)ADDON_HELM);
        m_pZone->broadcastPacket(getX(), getY(), &_GCTakeOff, this);
        break;
    case Item::ITEM_CLASS_SHIELD:
        m_SlayerInfo.setShieldType(SHIELD_NONE);
        _GCTakeOff.setObjectID(getObjectID());
        _GCTakeOff.setSlotID((SlotID_t)ADDON_LEFTHAND);
        m_pZone->broadcastPacket(getX(), getY(), &_GCTakeOff, this);
        break;
    case Item::ITEM_CLASS_SWORD:
        m_SlayerInfo.setWeaponType(WEAPON_NONE);
        _GCTakeOff.setObjectID(getObjectID());
        _GCTakeOff.setSlotID((SlotID_t)ADDON_RIGHTHAND);
        m_pZone->broadcastPacket(getX(), getY(), &_GCTakeOff, this);
        break;
    case Item::ITEM_CLASS_COAT:
        m_SlayerInfo.setJacketType(JACKET_BASIC);
        _GCTakeOff.setObjectID(getObjectID());
        _GCTakeOff.setSlotID((SlotID_t)ADDON_COAT);
        m_pZone->broadcastPacket(getX(), getY(), &_GCTakeOff, this);
        break;
    case Item::ITEM_CLASS_TROUSER:
        m_SlayerInfo.setPantsType(PANTS_BASIC);
        _GCTakeOff.setObjectID(getObjectID());
        _GCTakeOff.setSlotID((SlotID_t)ADDON_TROUSER);
        m_pZone->broadcastPacket(getX(), getY(), &_GCTakeOff, this);
        break;
    case Item::ITEM_CLASS_SHOULDER_ARMOR:
        m_SlayerInfo.setShoulderType(0);
        _GCTakeOff.setObjectID(getObjectID());
        _GCTakeOff.setSlotID((SlotID_t)ADDON_SHOULDER);
        m_pZone->broadcastPacket(getX(), getY(), &_GCTakeOff, this);
        break;
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

    __END_DEBUG
    __END_CATCH
}

// destroyGears
// Delete the equipped items.
void Slayer::destroyGears()

{
    __BEGIN_DEBUG

    for (int j = 0; j < WEAR_MAX; j++) {
        Item* pItem = m_pWearItem[j];
        if (pItem != NULL) {
            // Check whether it is a two-handed weapon so that deleting one
            // item clears both hands.
            if (isTwohandWeapon(pItem)) {
                m_pWearItem[WEAR_RIGHTHAND] = NULL;
                m_pWearItem[WEAR_LEFTHAND] = NULL;
            } else
                m_pWearItem[j] = NULL;

            SAFE_DELETE(pItem);
        }
    }

    __END_DEBUG
}

bool Slayer::isRealWearing(WearPart part) const

{
    __BEGIN_TRY

    if (part >= WEAR_MAX)
        throw Error("Slayer::isRealWearing() : invalid wear point!");
    if (m_pWearItem[part] == NULL)
        return false;
    if (part >= WEAR_ZAP1 && part <= WEAR_ZAP4) {
        // A ring must also be worn at the matching position.
        if (m_pWearItem[part - WEAR_ZAP1 + WEAR_FINGER1] == NULL)
            return false;
    }

    return isRealWearing(m_pWearItem[part]);

    __END_CATCH
}

bool Slayer::isRealWearing(Item* pItem) const

{
    __BEGIN_TRY

    if (pItem == NULL)
        return false;

    ItemInfo* pItemInfo = de::gameContext().itemInfos().getItemInfo(pItem->getItemClass(), pItem->getItemType());

    Level_t ReqAdvancedLevel = pItemInfo->getReqAdvancedLevel();
    if (ReqAdvancedLevel > 0 && (!isAdvanced() || getAdvancementClassLevel() < ReqAdvancedLevel))
        return false;

    if (isSlayerWeapon(pItem->getItemClass()) || pItem->getItemClass() == Item::ITEM_CLASS_COAT ||
        pItem->getItemClass() == Item::ITEM_CLASS_TROUSER) {
        if (ReqAdvancedLevel <= 0 && isAdvanced())
            return false;
    }

    // Time-limited items work for free users too, rare or unique.
    if (pItem->isTimeLimitItem())
        return decore::genderAllows(m_Sex, pItemInfo->getReqGender());

    // In a premium zone only paying users get unique/rare items applied.
    // A couple ring is also only usable by paying users.
    if (getZone()->isPremiumZone() &&
        (pItem->isUnique() || pItem->getOptionTypeSize() > 1 || pItem->getItemClass() == Item::ITEM_CLASS_COUPLE_RING ||
         pItem->getItemClass() == Item::ITEM_CLASS_VAMPIRE_COUPLE_RING)) {
        GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(getPlayer());
        if (!pGamePlayer->isPayPlaying() && !pGamePlayer->isPremiumPlay()) {
            // cout << "Premium Item :" << pItem->getItemClassName().c_str() << endl;
            return false;
        }
    }

    if (isCoupleRing(pItem)) {
        return true;
    }

    // The item's requirement raised by its options and capped, against
    // the slayer's current STR, DEX, INT and their sum, and sex. A slayer
    // has no level requirement.
    const decore::EquipRequirement required = computeEquipRequirement(decore::EquipRace::Slayer, pItem, pItemInfo);
    const decore::EquipStats current = {m_STR[ATTR_CURRENT], m_DEX[ATTR_CURRENT], m_INT[ATTR_CURRENT], 0, m_Sex};
    return decore::meetsRequirement(decore::EquipRace::Slayer, required, current);

    __END_CATCH
}

bool Slayer::isRealWearingEx(WearPart part) const {
    if (part >= WEAR_MAX)
        return false;
    return m_pRealWearingCheck[part];
}

DWORD Slayer::sendRealWearingInfo(void) const

{
    __BEGIN_TRY

    DWORD info = 0;
    DWORD flag = 1;

    for (int i = 0; i < WEAR_MAX; i++) {
        if (isRealWearing((Slayer::WearPart)i))
            info |= flag;
        flag <<= 1;
    }

    GCRealWearingInfo pkt;
    pkt.setInfo(info);
    m_pPlayer->sendPacket(&pkt);

    return info;

    __END_CATCH
}

void Slayer::setMotorcycle(Motorcycle* pMotorcycle)

{
    __BEGIN_DEBUG

    // Set the motorcycle.
    m_pMotorcycle = pMotorcycle;

    // Record in SlayerInfo that a motorcycle is being ridden.
    // m_SlayerInfo.setMotorcycleType(MOTORCYCLE1);
    // by sigi.2002.6.22
    m_SlayerInfo.setMotorcycleType(getMotorcycleType(pMotorcycle->getItemType()));

    if (!pMotorcycle->hasOptionType()) {
        m_SlayerInfo.setMotorcycleColor(388);
    } else {
        OptionType_t option = pMotorcycle->getFirstOptionType();
        OptionInfo* pOptionInfo = de::gameContext().optionInfos().getOptionInfo(option);

        if (pOptionInfo != NULL) {
            m_SlayerInfo.setMotorcycleColor(pOptionInfo->getColor());
        } else {
            m_SlayerInfo.setMotorcycleColor(388);
        }
    }

    __END_DEBUG
}

void Slayer::getOffMotorcycle()

{
    __BEGIN_DEBUG

    // Drop the motorcycle into the zone.
    TPOINT pt = m_pZone->addItem((Item*)m_pMotorcycle, m_X, m_Y);

    if (pt.x != -1) {
        // m_pMotorcycle->save("", STORAGE_ZONE, m_pZone->getZoneID(), pt.x, pt.y);

        // Optimized item save.
        char pField[80];
        snprintf(pField, sizeof(pField), "OwnerID='', Storage=%d, StorageID=%u, X=%d, Y=%d", STORAGE_ZONE,
                 m_pZone->getZoneID(), (int)pt.x, (int)pt.y);
        m_pMotorcycle->tinysave(pField);

        MotorcycleBox* pMotorcycleBox = de::gameContext().parking().getMotorcycleBox(m_pMotorcycle->getItemID());

        if (pMotorcycleBox != NULL) {
            pMotorcycleBox->setZone(m_pZone);
            pMotorcycleBox->setX(pt.x);
            pMotorcycleBox->setY(pt.y);
        } else {
            // cout << "Slayer::getOffMotorcycle() - pMotorcycleBox is NULL" << endl;
            filelog("errorLog.txt", "Slayer::getOffMotorcycle() - No MotorcycleBox: %d",
                    (int)m_pMotorcycle->getItemID());
            // throw Error("Getting off the motorcycle but ParkingCenter has no MotorcycleBox.");
        }
    } else {
        // If too many other items are lying around, just delete the Box itself.
        // It has to be claimed again.
        if (de::gameContext().parking().hasMotorcycleBox(m_pMotorcycle->getItemID())) {
            de::gameContext().parking().deleteMotorcycleBox(m_pMotorcycle->getItemID());
        }
    }

    // Take the motorcycle off the Slayer.
    m_pMotorcycle = NULL;

    // Record that the Slayer is not riding a motorcycle.
    m_SlayerInfo.setMotorcycleType(MOTORCYCLE_NONE);

    __END_DEBUG
}

PCSlayerInfo2* Slayer::getSlayerInfo2() const

{
    __BEGIN_DEBUG

    PCSlayerInfo2* pInfo = new PCSlayerInfo2();

    pInfo->setObjectID(m_ObjectID);
    pInfo->setName(m_Name);
    pInfo->setSex(m_Sex);
    pInfo->setHairStyle(m_HairStyle);
    pInfo->setHairColor(m_HairColor);
    pInfo->setSkinColor(m_SkinColor);
    pInfo->setMasterEffectColor(m_MasterEffectColor);

    // cout << "PCSlayerInfo2: HairStyle = " << HairStyle2String[pInfo->getHairStyle()] << endl;

    // pInfo->setPhoneNumber(m_PhoneNumber);

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

    // Attribute experience
    pInfo->setSTRExp(getSTRGoalExp());
    pInfo->setDEXExp(getDEXGoalExp());
    pInfo->setINTExp(getINTGoalExp());

    // Rank
    pInfo->setRank(getRank());
    pInfo->setRankExp(getRankGoalExp());

    //	cout << getRankGoalExp() << endl;

    pInfo->setHP(m_HP[ATTR_CURRENT], m_HP[ATTR_MAX]);
    pInfo->setMP(m_MP[ATTR_CURRENT], m_MP[ATTR_MAX]);
    pInfo->setFame(m_Fame);
    pInfo->setGold(m_Gold);
    pInfo->setSkillDomain(SKILL_DOMAIN_BLADE, m_SkillDomainLevels[SKILL_DOMAIN_BLADE], m_GoalExp[SKILL_DOMAIN_BLADE]);
    pInfo->setSkillDomain(SKILL_DOMAIN_SWORD, m_SkillDomainLevels[SKILL_DOMAIN_SWORD], m_GoalExp[SKILL_DOMAIN_SWORD]);
    pInfo->setSkillDomain(SKILL_DOMAIN_GUN, m_SkillDomainLevels[SKILL_DOMAIN_GUN], m_GoalExp[SKILL_DOMAIN_GUN]);
    pInfo->setSkillDomain(SKILL_DOMAIN_ETC, m_SkillDomainLevels[SKILL_DOMAIN_ETC], m_GoalExp[SKILL_DOMAIN_ETC]);
    pInfo->setSkillDomain(SKILL_DOMAIN_ENCHANT, m_SkillDomainLevels[SKILL_DOMAIN_ENCHANT],
                          m_GoalExp[SKILL_DOMAIN_ENCHANT]);
    pInfo->setSkillDomain(SKILL_DOMAIN_HEAL, m_SkillDomainLevels[SKILL_DOMAIN_HEAL], m_GoalExp[SKILL_DOMAIN_HEAL]);
    pInfo->setSight(m_Sight);

    // A competence of 0 or 1 must be drawn with the
    // game master sprite.
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
    pInfo->setAttrBonus(getBonus());

    return pInfo;

    __END_DEBUG
}

PCSlayerInfo3 Slayer::getSlayerInfo3() const

{
    __BEGIN_DEBUG

    // The coordinates and the direction change very often, so they are
    // only set when this function is called.
    m_SlayerInfo.setObjectID(m_ObjectID); // The object ID changes on morph.
    m_SlayerInfo.setX(m_X);
    m_SlayerInfo.setY(m_Y);
    m_SlayerInfo.setDir(m_Dir);
    m_SlayerInfo.setCurrentHP(m_HP[ATTR_CURRENT]);
    m_SlayerInfo.setMaxHP(m_HP[ATTR_MAX]);
    m_SlayerInfo.setAlignment(m_Alignment);
    m_SlayerInfo.setGuildID(m_GuildID);

    // Attack speed
    m_SlayerInfo.setAttackSpeed(m_AttackSpeed[ATTR_CURRENT]);

    // by sigi. 2002.9.10
    m_SlayerInfo.setRank(getRank());

    // For hair dye
    m_SlayerInfo.setHairColor(m_HairColor);
    m_SlayerInfo.setSkinColor(m_SkinColor);
    m_SlayerInfo.setMasterEffectColor(m_MasterEffectColor);

    GuildUnion* pUnion = GuildUnionManager::Instance().getGuildUnion(m_GuildID);
    if (pUnion == NULL)
        m_SlayerInfo.setUnionID(0);
    else
        m_SlayerInfo.setUnionID(pUnion->getUnionID());

    m_SlayerInfo.setAdvancementLevel(getAdvancementClassLevel());

    return m_SlayerInfo;

    __END_DEBUG
}

GearInfo* Slayer::getGearInfo() const

{
    __BEGIN_DEBUG

    GearInfo* pGearInfo = new GearInfo();

    for (int i = 0; i < WEAR_MAX; i++) {
        Item* pItem = m_pWearItem[i];

        if (pItem != NULL) {
            //			Item::ItemClass IClass = pItem->getItemClass();

            GearSlotInfo* pGearSlotInfo = new GearSlotInfo();
            pItem->makePCItemInfo(*pGearSlotInfo);

            pGearSlotInfo->setSlotID(i);

            pGearInfo->addListElement(pGearSlotInfo);
        }
    }

    return pGearInfo;

    __END_DEBUG
}

RideMotorcycleInfo* Slayer::getRideMotorcycleInfo() const

{
    __BEGIN_DEBUG

    RideMotorcycleInfo* pRideMotorcycleInfo = new RideMotorcycleInfo();

    pRideMotorcycleInfo->setObjectID(m_pMotorcycle->getObjectID());
    pRideMotorcycleInfo->setItemType(m_pMotorcycle->getItemType());
    pRideMotorcycleInfo->setOptionType(m_pMotorcycle->getOptionTypeList());

    m_SlayerInfo.setMotorcycleColor(388);

    return pRideMotorcycleInfo;

    __END_DEBUG
}

void Slayer::sendSlayerSkillInfo()

{
    __BEGIN_TRY
    __BEGIN_DEBUG

    try {
        SlayerSkillInfo* pSlayerSkillInfo[SKILL_DOMAIN_VAMPIRE];

        for (int i = 0; i < SKILL_DOMAIN_VAMPIRE; i++) {
            pSlayerSkillInfo[i] = new SlayerSkillInfo();
            pSlayerSkillInfo[i]->setDomainType((SkillDomainType_t)i);
        }

        SkillInfo* pSkillInfo = NULL;
        SkillDomainType_t SDomainType = 0;

        // To work out the current time and the remaining casting time.
        Timeval currentTime;
        getCurrentTime(currentTime);

        unordered_map<SkillType_t, SkillSlot*>::const_iterator itr = m_SkillSlot.begin();
        for (; itr != m_SkillSlot.end(); itr++) {
            SkillSlot* pSkillSlot = itr->second;
            Assert(pSkillSlot != NULL);

            // If it is not a basic attack skill...
            if (pSkillSlot->getSkillType() >= SKILL_DOUBLE_IMPACT) {
                // Take the skill info.
                pSkillInfo = de::gameContext().skillInfos().getSkillInfo(pSkillSlot->getSkillType());

                // Take the current skill's domain from the skill info.
                SDomainType = pSkillInfo->getDomainType();

                // Build the sub skill info.
                SubSlayerSkillInfo* pSubSlayerSkillInfo = new SubSlayerSkillInfo();
                pSubSlayerSkillInfo->setSkillType(pSkillSlot->getSkillType());
                pSubSlayerSkillInfo->setSkillExp(pSkillSlot->getExp());
                pSubSlayerSkillInfo->setSkillExpLevel(pSkillSlot->getExpLevel());
                pSubSlayerSkillInfo->setSkillTurn(pSkillSlot->getInterval());

                //				cout << pSkillInfo->getName() << "skill delay " << pSkillSlot->getInterval() << endl;

                // Use the remaining time until the next cast as the casting time field.
                // pSubSlayerSkillInfo->setCastingTime(pSkillSlot->getCastingTime());
                pSubSlayerSkillInfo->setCastingTime(pSkillSlot->getRemainTurn(currentTime));
                pSubSlayerSkillInfo->setEnable(pSkillSlot->canUse());

                // Add the sub skill info to the Slayer skill info.
                pSlayerSkillInfo[SDomainType]->addListElement(pSubSlayerSkillInfo);
            }
        }

        GCSkillInfo gcSkillInfo;
        gcSkillInfo.setPCType(PC_SLAYER);

        for (int i = 0; i < SKILL_DOMAIN_VAMPIRE; i++) {
            SkillType_t LearnSkillType = de::gameContext().skillInfos().getSkillTypeByLevel(i, m_SkillDomainLevels[i]);

            // Check whether a skill can be learned at the current level.
            if (LearnSkillType != 0) {
                // If a learnable skill is not learned yet, tell the player to learn it.
                if (hasSkill(LearnSkillType) == NULL) {
                    pSlayerSkillInfo[i]->setLearnNewSkill(true);
                }
            }

            if (pSlayerSkillInfo[i]->isLearnNewSkill() || pSlayerSkillInfo[i]->getListNum() > 0) {
                gcSkillInfo.addListElement(pSlayerSkillInfo[i]);
            } else {
                SAFE_DELETE(pSlayerSkillInfo[i]);
            }
        }

        m_pPlayer->sendPacket(&gcSkillInfo);
    } catch (Throwable& t) {
        filelog("slayerBug.log", "%s", t.toString().c_str());
    }


    __END_DEBUG
    __END_CATCH
}


EffectInfo* Slayer::getEffectInfo() const {
    EffectInfo* pEffectInfo = m_pEffectManager->getEffectInfo();
    return pEffectInfo;
}

void Slayer::setGoldEx(Gold_t gold)

{
    __BEGIN_TRY
    __BEGIN_DEBUG

    setGold(gold);

    char pField[80];
    snprintf(pField, sizeof(pField), "Gold = %u", m_Gold);
    tinysave(pField);

    __END_DEBUG
    __END_CATCH
}

void Slayer::heartbeat(const Timeval& currentTime)

{
    __BEGIN_DEBUG

    PlayerCreature::heartbeat(currentTime);

    // The MP regeneration for Prayer and Meditation is done here.
    // A separate heartbeat function would be the right place.
    Item* pWeapon = getWearItem(Slayer::WEAR_RIGHTHAND);
    if (pWeapon != NULL) {
        Item::ItemClass IClass = pWeapon->getItemClass();

        SkillSlot* pPrayer = hasSkill(SKILL_PRAYER);
        SkillSlot* pMeditation = hasSkill(SKILL_MEDITATION);

        if (IClass == Item::ITEM_CLASS_CROSS && pPrayer != NULL && pPrayer->canUse() &&
            !isFlag(Effect::EFFECT_CLASS_PLEASURE_EXPLOSION)) {
            Timeval currentTime;
            getCurrentTime(currentTime);

            if (m_MPRegenTime < currentTime) {
                MP_t MPMax = getMP(ATTR_MAX);
                int MPRegenPercent = 3 + getINT(ATTR_CURRENT) / 20;
                MP_t MPQuantity = max(1, getPercentValue(MPMax, MPRegenPercent));
                MP_t oldMP = getMP(ATTR_CURRENT);
                MP_t newMP = min((int)MPMax, (int)(oldMP + MPQuantity));

                if (oldMP != newMP) {
                    setMP(newMP, ATTR_CURRENT);

                    GCModifyInformation gcMI;
                    gcMI.addShortData(MODIFY_CURRENT_MP, newMP);
                    m_pPlayer->sendPacket(&gcMI);
                }

                // Heartbeat every 5 seconds.
                m_MPRegenTime.tv_sec = currentTime.tv_sec + 5;
                m_MPRegenTime.tv_usec = currentTime.tv_usec;
            }
        } else if (IClass == Item::ITEM_CLASS_MACE && pMeditation != NULL && pMeditation->canUse() &&
                   !isFlag(Effect::EFFECT_CLASS_PLEASURE_EXPLOSION)) {
            Timeval currentTime;
            getCurrentTime(currentTime);

            if (m_MPRegenTime < currentTime) {
                MP_t MPMax = getMP(ATTR_MAX);
                int MPRegenPercent = 3 + getINT(ATTR_CURRENT) / 20;
                MP_t MPQuantity = max(1, getPercentValue(MPMax, MPRegenPercent));
                MP_t oldMP = getMP(ATTR_CURRENT);
                MP_t newMP = min((int)MPMax, (int)(oldMP + MPQuantity));

                if (oldMP != newMP) {
                    setMP(newMP, ATTR_CURRENT);

                    GCModifyInformation gcMI;
                    gcMI.addShortData(MODIFY_CURRENT_MP, newMP);
                    m_pPlayer->sendPacket(&gcMI);
                }

                // Heartbeat every 5 seconds.
                m_MPRegenTime.tv_sec = (currentTime.tv_sec + 5);
                m_MPRegenTime.tv_usec = currentTime.tv_usec;
            }
        }
    }

    __END_DEBUG
}

void Slayer::getSlayerRecord(SLAYER_RECORD& record) const

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

    record.Rank = getRank();

    record.pDamage[0] = m_Damage[0];
    record.pDamage[1] = m_Damage[1];

    record.Defense = m_Defense[0];
    record.Protection = m_Protection[0];
    record.ToHit = m_ToHit[0];
    record.AttackSpeed = m_AttackSpeed[0];

    __END_CATCH
}

uint Slayer::getSlayerLevel(void) const

{
    __BEGIN_TRY

    uint SumAttr = 0, SumDomain = 0;

    SumAttr += m_STR[ATTR_BASIC];
    SumAttr += m_DEX[ATTR_BASIC];
    SumAttr += m_INT[ATTR_BASIC];

    SumDomain += m_SkillDomainLevels[SKILL_DOMAIN_SWORD];
    SumDomain += m_SkillDomainLevels[SKILL_DOMAIN_BLADE];
    SumDomain += m_SkillDomainLevels[SKILL_DOMAIN_GUN];
    SumDomain += m_SkillDomainLevels[SKILL_DOMAIN_HEAL];
    SumDomain += m_SkillDomainLevels[SKILL_DOMAIN_ENCHANT];

    return (uint)(SumAttr / 4 + SumDomain / 2);

    __END_CATCH
}

string Slayer::toString() const

{
    __BEGIN_DEBUG

    StringStream msg;
    msg << "Slayer("
        //<< "ObjectID:"     << (int)getObjectID()
        << ",Name:" << m_Name << ",Sex:" << Sex2String[m_Sex] << ",HairStyle:" << HairStyle2String[m_HairStyle]
        << ",HairColor:" << (int)m_HairColor << ",SkinColor:" << (int)m_SkinColor << ",Rank:"
        << (int)getRank()
        //		<< ",RankExp:"     << (int)m_RankExp
        << ",RankGoalExp:" << (int)getRankGoalExp() << ",STR:" << (int)m_STR[ATTR_CURRENT] << "/"
        << (int)m_STR[ATTR_MAX] << ",DEX:" << (int)m_DEX[ATTR_CURRENT] << "/" << (int)m_DEX[ATTR_MAX]
        << ",INT:" << (int)m_INT[ATTR_CURRENT] << "/"
        << (int)m_INT[ATTR_MAX]
        //		<< ",STRExp:"      << (int)m_STRExp
        << ",STRGoalExp :"
        << (int)getSTRGoalExp()
        //		<< ",DEXExp:"      << (int)m_DEXExp
        //		<< ",INTExp:"      << (int)m_INTExp
        << ",HP:" << (int)m_HP[ATTR_CURRENT] << "/" << (int)m_HP[ATTR_MAX] << ",MP:" << (int)m_MP[ATTR_CURRENT] << "/"
        << (int)m_MP[ATTR_MAX] << ",Fame:" << (int)m_Fame << ",Gold:" << (int)m_Gold << ",GuildID:" << (int)m_GuildID
        << ",ZoneID:" << (int)getZoneID() << ",X:" << (int)m_X << ",Y:" << (int)m_Y << ",Sight :" << (int)m_Sight
        << ")";
    return msg.toString();

    __END_DEBUG
}

SkillLevel_t Slayer::getSkillDomainLevelSum() const

{
    __BEGIN_TRY

    SkillLevel_t sum = 0;
    sum += m_SkillDomainLevels[SKILL_DOMAIN_BLADE];
    sum += m_SkillDomainLevels[SKILL_DOMAIN_SWORD];
    sum += m_SkillDomainLevels[SKILL_DOMAIN_GUN];
    sum += m_SkillDomainLevels[SKILL_DOMAIN_HEAL];
    sum += m_SkillDomainLevels[SKILL_DOMAIN_ENCHANT];
    return sum;

    __END_CATCH
}


SkillLevel_t Slayer::getHighestSkillDomainLevel() const

{
    __BEGIN_TRY

    SkillLevel_t highest;

    highest = max(m_SkillDomainLevels[SKILL_DOMAIN_BLADE], m_SkillDomainLevels[SKILL_DOMAIN_SWORD]);
    highest = max(highest, m_SkillDomainLevels[SKILL_DOMAIN_GUN]);
    highest = max(highest, m_SkillDomainLevels[SKILL_DOMAIN_HEAL]);
    highest = max(highest, m_SkillDomainLevels[SKILL_DOMAIN_ENCHANT]);

    return highest;

    __END_CATCH
}

SkillDomainType_t Slayer::getHighestSkillDomain() const

{
    __BEGIN_TRY

    SkillDomainType_t highest;

    if (m_SkillDomainLevels[SKILL_DOMAIN_BLADE] > m_SkillDomainLevels[SKILL_DOMAIN_SWORD])
        highest = SKILL_DOMAIN_BLADE;
    else
        highest = SKILL_DOMAIN_SWORD;

    if (m_SkillDomainLevels[SKILL_DOMAIN_GUN] > m_SkillDomainLevels[highest])
        highest = SKILL_DOMAIN_GUN;

    if (m_SkillDomainLevels[SKILL_DOMAIN_HEAL] > m_SkillDomainLevels[highest])
        highest = SKILL_DOMAIN_HEAL;

    if (m_SkillDomainLevels[SKILL_DOMAIN_ENCHANT] > m_SkillDomainLevels[highest])
        highest = SKILL_DOMAIN_ENCHANT;

    return highest;

    __END_CATCH
}

void Slayer::saveSkills(void) const {
    saveSkillSlots(m_SkillSlot);
}

void Slayer::saveGears(void) const

{
    __BEGIN_TRY

    // Save the equipped items.
    char pField[80];

    for (int i = 0; i < Slayer::WEAR_MAX; i++) {
        Item* pItem = m_pWearItem[i];
        if (pItem != NULL) {
            Durability_t maxDurability = computeMaxDurability(pItem);
            if (pItem->getDurability() < maxDurability) {
                // For a weapon, save the bullet count.
                if (i == Slayer::WEAR_RIGHTHAND) // Checked first to keep it quick.
                {
                    if (pItem->isGun()) {
                        //						Gun* pGun = dynamic_cast<Gun*>(pItem);

                        if (pItem != NULL) {
                            // pItem->saveBullet();
                            snprintf(pField, sizeof(pField), "Durability=%d, BulletCount=%d, Silver=%d",
                                     pItem->getDurability(), pItem->getBulletCount(), pItem->getSilver());
                            pItem->tinysave(pField);
                        }
                    }
                    // All current weapons are silver-plated.
                    else // if (pItem->isSilverWeapon())
                    {
                        snprintf(pField, sizeof(pField), "Durability=%d, Silver=%d", pItem->getDurability(),
                                 pItem->getSilver());
                        pItem->tinysave(pField);
                    }
                } else {
                    // pItem->save(m_Name, STORAGE_GEAR, 0, i, 0);
                    //  Optimized item save.
                    snprintf(pField, sizeof(pField), "Durability=%d", pItem->getDurability());
                    pItem->tinysave(pField);
                }
            }
        }
    }

    __END_CATCH
}

void Slayer::saveExps(void) const

{
    __BEGIN_TRY

    // Divide by 10 to reduce the number of queries in the skill handler,
    // If the server is not down and you log out normally
    // If you don't explicitly save, the part that goes up below 10 will be blown away.
    // So save here.

    SlayerExpsRecord record;
    record.strGoalExp = getSTRGoalExp();
    record.dexGoalExp = getDEXGoalExp();
    record.intGoalExp = getINTGoalExp();
    record.bladeGoalExp = m_GoalExp[SKILL_DOMAIN_BLADE];
    record.swordGoalExp = m_GoalExp[SKILL_DOMAIN_SWORD];
    record.gunGoalExp = m_GoalExp[SKILL_DOMAIN_GUN];
    record.enchantGoalExp = m_GoalExp[SKILL_DOMAIN_ENCHANT];
    record.healGoalExp = m_GoalExp[SKILL_DOMAIN_HEAL];
    record.etcGoalExp = m_GoalExp[SKILL_DOMAIN_ETC];
    record.alignment = m_Alignment;
    record.fame = m_Fame;
    record.rank = getRank();
    record.rankGoalExp = getRankGoalExp();
    record.advancementClass = getAdvancementClassLevel();
    record.advancementGoalExp = getAdvancementClassGoalExp();
    record.advancedSTR = m_AdvancedSTR;
    record.advancedDEX = m_AdvancedDEX;
    record.advancedINT = m_AdvancedINT;
    record.advancedAttrBonus = m_AdvancedAttrBonus;
    defaultCharacterRepository().saveSlayerExps(m_Name, record);

    __END_CATCH
}

//----------------------------------------------------------------------
// getShapeInfo
//----------------------------------------------------------------------
// Build the outfit flag/color information from the slayer's current outfit.
// This is to speed up processing at login.
//----------------------------------------------------------------------
// 32 bits for 32 kinds is considered enough for now.
// If it ever overflows a bitset will be needed.
//
// (!) The color is not an index color value but the optionType.
//     The client looks the color value up from the option.
//----------------------------------------------------------------------
void Slayer::getShapeInfo(DWORD& flag, Color_t colors[PCSlayerInfo::SLAYER_COLOR_MAX]) const
//
{
    __BEGIN_DEBUG

    Item* pItem;
    // OptionInfo* 				pOptionInfo;
    PCSlayerInfo::SlayerBits slayerBit;
    PCSlayerInfo::SlayerColors slayerColor;

    WearPart Part;

    // Initialize
    flag = 0;

    //-----------------------------------------------------------------
    // Sex
    //-----------------------------------------------------------------
    slayerBit = PCSlayerInfo::SLAYER_BIT_SEX;
    flag |= ((m_Sex ? 1 : 0) << slayerBit);

    //-----------------------------------------------------------------
    // HairStyle
    //-----------------------------------------------------------------
    slayerBit = PCSlayerInfo::SLAYER_BIT_HAIRSTYLE1;
    flag |= (m_HairStyle << slayerBit);

    //-----------------------------------------------------------------
    // Trousers
    //-----------------------------------------------------------------
    Part = WEAR_LEG;
    pItem = m_pWearItem[Part];
    slayerBit = PCSlayerInfo::SLAYER_BIT_PANTS1;
    slayerColor = PCSlayerInfo::SLAYER_COLOR_PANTS;
    if (pItem != NULL && m_pRealWearingCheck[Part]) {
        ItemType_t IType = pItem->getItemType();

        //		colors[slayerColor] = (pItem->isUnique()? UNIQUE_OPTION : pItem->getFirstOptionType());
        if (pItem->isTimeLimitItem())
            colors[slayerColor] = QUEST_OPTION;
        else if (pItem->isUnique())
            colors[slayerColor] = UNIQUE_OPTION;
        else
            colors[slayerColor] = pItem->getFirstOptionType();

        flag |= (getPantsType(IType) << slayerBit);
    } else {
        colors[slayerColor] = 0;
        flag |= (PANTS_BASIC << slayerBit);
    }
    //-----------------------------------------------------------------
    // Jacket
    //-----------------------------------------------------------------
    Part = WEAR_BODY;
    pItem = m_pWearItem[Part];
    slayerBit = PCSlayerInfo::SLAYER_BIT_JACKET1;
    slayerColor = PCSlayerInfo::SLAYER_COLOR_JACKET;
    if (pItem != NULL && m_pRealWearingCheck[Part]) {
        ItemType_t IType = pItem->getItemType();

        //		colors[slayerColor] = (pItem->isUnique()? UNIQUE_OPTION : pItem->getFirstOptionType());

        if (pItem->isTimeLimitItem())
            colors[slayerColor] = QUEST_OPTION;
        else if (pItem->isUnique())
            colors[slayerColor] = UNIQUE_OPTION;
        else
            colors[slayerColor] = pItem->getFirstOptionType();

        flag |= (getJacketType(IType) << slayerBit);
    } else {
        colors[slayerColor] = 0;
        flag |= (JACKET_BASIC << slayerBit);
    }

    //-----------------------------------------------------------------
    // Helmet
    //-----------------------------------------------------------------
    Part = WEAR_HEAD;
    pItem = m_pWearItem[Part];
    slayerBit = PCSlayerInfo::SLAYER_BIT_HELMET1;
    slayerColor = PCSlayerInfo::SLAYER_COLOR_HELMET;
    if (pItem != NULL && m_pRealWearingCheck[Part]) {
        ItemType_t IType = pItem->getItemType();

        //		colors[slayerColor] = (pItem->isUnique()? UNIQUE_OPTION : pItem->getFirstOptionType());

        if (pItem->isTimeLimitItem())
            colors[slayerColor] = QUEST_OPTION;
        else if (pItem->isUnique())
            colors[slayerColor] = UNIQUE_OPTION;
        else
            colors[slayerColor] = pItem->getFirstOptionType();

        flag |= (getHelmetType(IType) << slayerBit);

    } else {
        colors[slayerColor] = 0;
        // none
    }

    //-----------------------------------------------------------------
    // Shield
    //-----------------------------------------------------------------
    Part = WEAR_LEFTHAND;
    pItem = m_pWearItem[Part];
    slayerBit = PCSlayerInfo::SLAYER_BIT_SHIELD1;
    slayerColor = PCSlayerInfo::SLAYER_COLOR_SHIELD;
    if (pItem != NULL && m_pRealWearingCheck[Part] && pItem->getItemClass() == Item::ITEM_CLASS_SHIELD) {
        ItemType_t IType = pItem->getItemType();

        //		colors[slayerColor] = (pItem->isUnique()? UNIQUE_OPTION : pItem->getFirstOptionType());

        //		if ( pItem->isUnique() ) colors[slayerColor] = UNIQUE_OPTION;
        if (pItem->isTimeLimitItem())
            colors[slayerColor] = QUEST_OPTION;
        else if (pItem->isUnique())
            colors[slayerColor] = UNIQUE_OPTION;
        else
            colors[slayerColor] = pItem->getFirstOptionType();

        flag |= (getShieldType(IType) << slayerBit);
    } else {
        colors[slayerColor] = 0;
        // none
    }

    //-----------------------------------------------------------------
    // Weapon
    //-----------------------------------------------------------------
    Part = WEAR_RIGHTHAND;
    pItem = m_pWearItem[Part];
    slayerBit = PCSlayerInfo::SLAYER_BIT_WEAPON1;
    slayerColor = PCSlayerInfo::SLAYER_COLOR_WEAPON;

    if (pItem != NULL && m_pRealWearingCheck[Part]) {
        DWORD weaponType = 0;

        if (pItem->getItemClass() == Item::ITEM_CLASS_SWORD)
            weaponType = slayerWeaponListShape(slayerWeaponShape(WEAPON_SWORD, pItem->getItemType()));
        else if (pItem->getItemClass() == Item::ITEM_CLASS_BLADE)
            weaponType = slayerWeaponListShape(slayerWeaponShape(WEAPON_BLADE, pItem->getItemType()));
        else if (pItem->getItemClass() == Item::ITEM_CLASS_SR)
            weaponType = slayerWeaponListShape(slayerWeaponShape(WEAPON_SR, pItem->getItemType()));
        else if (pItem->getItemClass() == Item::ITEM_CLASS_AR)
            weaponType = slayerWeaponListShape(slayerWeaponShape(WEAPON_AR, pItem->getItemType()));
        else if (pItem->getItemClass() == Item::ITEM_CLASS_SG)
            weaponType = slayerWeaponListShape(slayerWeaponShape(WEAPON_SG, pItem->getItemType()));
        else if (pItem->getItemClass() == Item::ITEM_CLASS_SMG)
            weaponType = slayerWeaponListShape(slayerWeaponShape(WEAPON_SMG, pItem->getItemType()));
        else if (pItem->getItemClass() == Item::ITEM_CLASS_CROSS)
            weaponType = slayerWeaponListShape(slayerWeaponShape(WEAPON_CROSS, pItem->getItemType()));
        else if (pItem->getItemClass() == Item::ITEM_CLASS_MACE)
            weaponType = slayerWeaponListShape(slayerWeaponShape(WEAPON_MACE, pItem->getItemType()));

        // colors[slayerColor] = (pItem->isUnique()? UNIQUE_OPTION : pItem->getFirstOptionType());

        if (pItem->isUnique())
            colors[slayerColor] = UNIQUE_OPTION;
        else if (pItem->isTimeLimitItem())
            colors[slayerColor] = QUEST_OPTION;
        else
            colors[slayerColor] = pItem->getFirstOptionType();

        flag |= (weaponType << slayerBit);
    } else {
        colors[slayerColor] = 0;
    }

    __END_DEBUG
}


Slayer::WearPart Slayer::getWearPart(Item::ItemClass IClass) const {
    switch (IClass) {
    case Item::ITEM_CLASS_COAT:
        return WEAR_BODY;
    case Item::ITEM_CLASS_TROUSER:
        return WEAR_LEG;

    case Item::ITEM_CLASS_SWORD:
    case Item::ITEM_CLASS_BLADE:
    case Item::ITEM_CLASS_AR:
    case Item::ITEM_CLASS_SR:
    case Item::ITEM_CLASS_SG:
    case Item::ITEM_CLASS_SMG:
    case Item::ITEM_CLASS_MACE:
    case Item::ITEM_CLASS_CROSS:
        return WEAR_RIGHTHAND;

    case Item::ITEM_CLASS_HELM:
        return WEAR_HEAD;

    case Item::ITEM_CLASS_SHIELD:
        return WEAR_LEFTHAND;

    case Item::ITEM_CLASS_SHOULDER_ARMOR:
        return WEAR_SHOULDER;

    default:
        return WEAR_MAX;
    }

    return WEAR_MAX;
}

bool Slayer::changeShape(Item* pItem, Color_t color, bool bSendPacket) {
    Item::ItemClass IClass = pItem->getItemClass();
    ItemType_t IType = pItem->getItemType();

    WearPart Part = getWearPart(IClass);

    if (Part == WEAR_MAX)
        return false;

    bool bRealWear = m_pRealWearingCheck[Part];

    if (bRealWear)
        return addShape(IClass, IType, color);

    return removeShape(IClass, bSendPacket);
}

bool Slayer::addShape(Item::ItemClass IClass, ItemType_t IType, Color_t color) {
    bool bisChange = false;

    switch (IClass) {
    case Item::ITEM_CLASS_MACE:
        bisChange = true;
        // m_SlayerInfo.setWeaponType(WEAPON_MACE);

        m_SlayerInfo.setWeaponType(slayerWeaponShape(WEAPON_MACE, IType));
        m_SlayerInfo.setWeaponColor(color);
        break;
    case Item::ITEM_CLASS_CROSS:
        bisChange = true;

        m_SlayerInfo.setWeaponType(slayerWeaponShape(WEAPON_CROSS, IType));
        m_SlayerInfo.setWeaponColor(color);
        break;
    case Item::ITEM_CLASS_BLADE:
        bisChange = true;

        m_SlayerInfo.setWeaponType(slayerWeaponShape(WEAPON_BLADE, IType));
        m_SlayerInfo.setWeaponColor(color);
        break;
    case Item::ITEM_CLASS_AR:
        bisChange = true;

        m_SlayerInfo.setWeaponType(slayerWeaponShape(WEAPON_AR, IType));
        m_SlayerInfo.setWeaponColor(color);
        break;
    case Item::ITEM_CLASS_SR:
        bisChange = true;

        m_SlayerInfo.setWeaponType(slayerWeaponShape(WEAPON_SR, IType));
        m_SlayerInfo.setWeaponColor(color);
        break;
    case Item::ITEM_CLASS_SMG:
        bisChange = true;

        m_SlayerInfo.setWeaponType(slayerWeaponShape(WEAPON_SMG, IType));
        m_SlayerInfo.setWeaponColor(color);
        break;
    case Item::ITEM_CLASS_SG:
        bisChange = true;

        m_SlayerInfo.setWeaponType(slayerWeaponShape(WEAPON_SG, IType));
        m_SlayerInfo.setWeaponColor(color);
        break;
    case Item::ITEM_CLASS_HELM:
        bisChange = true;

        m_SlayerInfo.setHelmetType(getHelmetType(IType));
        m_SlayerInfo.setHelmetColor(color);
        break;
    case Item::ITEM_CLASS_SHIELD:
        bisChange = true;

        m_SlayerInfo.setShieldType(getShieldType(IType));
        m_SlayerInfo.setShieldColor(color);
        break;
    case Item::ITEM_CLASS_SWORD:
        bisChange = true;

        m_SlayerInfo.setWeaponType(slayerWeaponShape(WEAPON_SWORD, IType));
        m_SlayerInfo.setWeaponColor(color);
        break;
    case Item::ITEM_CLASS_COAT:
        bisChange = true;

        m_SlayerInfo.setJacketType(getJacketType(IType));
        m_SlayerInfo.setJacketColor(color);
        break;
    case Item::ITEM_CLASS_TROUSER:
        bisChange = true;

        m_SlayerInfo.setPantsType(getPantsType(IType));
        m_SlayerInfo.setPantsColor(color);
        break;

    case Item::ITEM_CLASS_SHOULDER_ARMOR:
        bisChange = true;

        m_SlayerInfo.setShoulderType(getShoulderType(IType));
        m_SlayerInfo.setShoulderColor(color);
        break;

    default:
        break;
    }

    return bisChange;
}

bool Slayer::removeShape(Item::ItemClass IClass, bool bSendPacket) {
    bool bisWeapon = false;

    // When items are dropped on death the packet must go to the owner too.
    // It would be better taken as a parameter, but the header is left alone.
    // by sigi. 2002.11.7
    Creature* pOwner = (isDead() ? NULL : this);

    switch (IClass) {
    case Item::ITEM_CLASS_MACE:
    case Item::ITEM_CLASS_CROSS:
    case Item::ITEM_CLASS_BLADE:
    case Item::ITEM_CLASS_AR:
    case Item::ITEM_CLASS_SR:
    case Item::ITEM_CLASS_SMG:
    case Item::ITEM_CLASS_SG: {
        bisWeapon = true;
        m_SlayerInfo.setWeaponType(WEAPON_NONE);

        if (bSendPacket) // by sigi. 2002.11.6
        {
            GCTakeOff _GCTakeOff;
            _GCTakeOff.setObjectID(getObjectID());
            _GCTakeOff.setSlotID((SlotID_t)ADDON_RIGHTHAND);
            m_pZone->broadcastPacket(getX(), getY(), &_GCTakeOff, pOwner);
        }
    } break;

    case Item::ITEM_CLASS_HELM: {
        m_SlayerInfo.setHelmetType(HELMET_NONE);

        if (bSendPacket) // by sigi. 2002.11.6
        {
            GCTakeOff _GCTakeOff;
            _GCTakeOff.setObjectID(getObjectID());
            _GCTakeOff.setSlotID((SlotID_t)ADDON_HELM);
            m_pZone->broadcastPacket(getX(), getY(), &_GCTakeOff, pOwner);
        }
    } break;

    case Item::ITEM_CLASS_SHIELD: {
        m_SlayerInfo.setShieldType(SHIELD_NONE);

        if (bSendPacket) // by sigi. 2002.11.6
        {
            GCTakeOff _GCTakeOff;
            _GCTakeOff.setObjectID(getObjectID());
            _GCTakeOff.setSlotID((SlotID_t)ADDON_LEFTHAND);
            m_pZone->broadcastPacket(getX(), getY(), &_GCTakeOff, pOwner);
        }
    } break;

    case Item::ITEM_CLASS_SWORD: {
        bisWeapon = true;
        m_SlayerInfo.setWeaponType(WEAPON_NONE);

        if (bSendPacket) // by sigi. 2002.11.6
        {
            GCTakeOff _GCTakeOff;
            _GCTakeOff.setObjectID(getObjectID());
            _GCTakeOff.setSlotID((SlotID_t)ADDON_RIGHTHAND);
            m_pZone->broadcastPacket(getX(), getY(), &_GCTakeOff, pOwner);
        }
    } break;

    case Item::ITEM_CLASS_COAT: {
        m_SlayerInfo.setJacketType(JACKET_BASIC);

        if (bSendPacket) // by sigi. 2002.11.6
        {
            GCTakeOff _GCTakeOff;
            _GCTakeOff.setObjectID(getObjectID());
            _GCTakeOff.setSlotID((SlotID_t)ADDON_COAT);
            m_pZone->broadcastPacket(getX(), getY(), &_GCTakeOff, pOwner);
        }
    } break;

    case Item::ITEM_CLASS_TROUSER: {
        m_SlayerInfo.setPantsType(PANTS_BASIC);

        if (bSendPacket) // by sigi. 2002.11.6
        {
            GCTakeOff _GCTakeOff;
            _GCTakeOff.setObjectID(getObjectID());
            _GCTakeOff.setSlotID((SlotID_t)ADDON_TROUSER);
            m_pZone->broadcastPacket(getX(), getY(), &_GCTakeOff, pOwner);
        }
    } break;

    case Item::ITEM_CLASS_SHOULDER_ARMOR: {
        m_SlayerInfo.setShoulderType(0);

        if (bSendPacket) // by sigi. 2002.11.6
        {
            GCTakeOff _GCTakeOff;
            _GCTakeOff.setObjectID(getObjectID());
            _GCTakeOff.setSlotID((SlotID_t)ADDON_TROUSER);
            m_pZone->broadcastPacket(getX(), getY(), &_GCTakeOff, pOwner);
        }
    } break;

    default:
        break;
    }

    return bisWeapon;
}

QuestGrade_t Slayer::getQuestGrade() const {
    return getTotalAttr(ATTR_BASIC) - getSkillDomainLevel(SKILL_DOMAIN_HEAL) * 1.5 -
           getSkillDomainLevel(SKILL_DOMAIN_ENCHANT) * 1.5;
}

// A pure attribute sum of 40 or less makes the character a Novice.
bool Slayer::isNovice() const {
    return (m_STR[ATTR_BASIC] + m_DEX[ATTR_BASIC] + m_INT[ATTR_BASIC]) <= 40;
}

void Slayer::divideAttrExp(AttrKind kind, Damage_t damage, ModifyInfo& modifyInfo) {
    SLAYER_RECORD prev;
    getSlayerRecord(prev);

    damage = (Damage_t)getPercentValue(damage, AttrExpTimebandFactor[getZoneTimeband(m_pZone)]);

    if (de::gameContext().variables().getExpRatio() > 100 && de::gameContext().variables().getEventActivate() == 1)
        damage = getPercentValue(damage, de::gameContext().variables().getExpRatio());

    // Double experience depending on the time of day.
    if (isAffectExp2X())
        damage <<= 1;

    if (isFlag(Effect::EFFECT_CLASS_BONUS_EXP))
        damage *= 2;

    Exp_t MainPoint = max(1, damage * 8 / 10);
    Exp_t SubPoint = max(1, damage / 10);

    // Slayer attributes are capped at a total of 300 below domain level 100 (50, 200, 50 as before), and
    // experience beyond that does not accumulate. Once a domain level passes 100 the attribute experience
    // accumulates again. Dropping back below 100 does not re-impose the 300 cap on a total already past it.

    SkillLevel_t MaxDomainLevel = getHighestSkillDomainLevel();
    Attr_t TotalAttr = getTotalAttr(ATTR_BASIC);
    Attr_t TotalAttrBound = 0;  // Total attribute cap
    Attr_t AttrBound = 0;       // Single attribute cap
    Attr_t OneAttrExpBound = 0; // Total-attribute bound above which only one attribute gains exp
    Attr_t SubAttrMax = 0;      // Sub attribute maximum

    if (MaxDomainLevel <= SLAYER_BOUND_LEVEL && TotalAttr <= SLAYER_BOUND_ATTR_SUM) {
        TotalAttrBound = SLAYER_BOUND_ATTR_SUM;      // 300
        AttrBound = SLAYER_BOUND_ATTR;               // 200
        OneAttrExpBound = SLAYER_BOUND_ONE_EXP_ATTR; // 200
        SubAttrMax = SLAYER_BOUND_SUB_ATTR;          // 50
    } else {
        TotalAttrBound = SLAYER_MAX_ATTR_SUM;  // 435
        AttrBound = SLAYER_MAX_ATTR;           // 295
        OneAttrExpBound = SLAYER_ONE_EXP_ATTR; // 400
        SubAttrMax = SLAYER_MAX_SUB_ATTR;      // 70
    }

    Attr* pMainAttr = m_pAttrs[kind];
    Attr* pSubAttrs[2];
    int count = 0;

    bool levelUpSubAttrs[2] = {false, false};

    for (int itr = ATTR_KIND_STR; itr != ATTR_KIND_MAX; ++itr) {
        if (m_pAttrs[itr] != pMainAttr)
            pSubAttrs[count++] = m_pAttrs[itr];
    }

    if (pSubAttrs[0]->getLevel() < pSubAttrs[1]->getLevel()) {
        Attr* pTemp;
        SWAP(pSubAttrs[0], pSubAttrs[1], pTemp);
    }

    bool downOtherLevel = TotalAttr >= TotalAttrBound;
    bool upOtherLevel = TotalAttr < OneAttrExpBound;
    bool canLevelUp = pMainAttr->getLevel() < AttrBound;
    bool levelUpMainAttr = pMainAttr->increaseExp(MainPoint, canLevelUp);

    if (levelUpMainAttr && downOtherLevel) {
        pSubAttrs[0]->levelDown();
        levelUpSubAttrs[0] = true;
    }

    if (upOtherLevel) {
        levelUpSubAttrs[0] = pSubAttrs[0]->increaseExp(SubPoint) || levelUpSubAttrs[0];
        levelUpSubAttrs[1] = pSubAttrs[1]->increaseExp(SubPoint);
    } else {
        if (pSubAttrs[0]->getLevel() < SubAttrMax)
            levelUpSubAttrs[0] = pSubAttrs[0]->increaseExp(SubPoint) || levelUpSubAttrs[0];
        if (pSubAttrs[1]->getLevel() < SubAttrMax)
            levelUpSubAttrs[1] = pSubAttrs[1]->increaseExp(SubPoint) || levelUpSubAttrs[1];
    }

    if (++m_AttrExpSaveCount > ATTR_EXP_SAVE_PERIOD) {
        char pField[256];
        snprintf(pField, sizeof(pField), "STRGoalExp=%u, DEXGoalExp=%u, INTGoalExp=%u", getSTRGoalExp(),
                 getDEXGoalExp(), getINTGoalExp());

        tinysave(pField);

        m_AttrExpSaveCount = 0;
    }

    if (levelUpMainAttr || levelUpSubAttrs[0] || levelUpSubAttrs[1]) {
        healCreatureForLevelUp(this, modifyInfo, &prev);
        sendEffectLevelUp(this);
        if (de::gameContext().variables().isNewbieTransportToGuild())
            checkNewbieTransportToGuild(this);

        char pField[256];
        snprintf(
            pField, sizeof(pField), "STR=%d, DEX=%d, INTE=%d, STRGoalExp=%u, DEXGoalExp=%u, INTGoalExp=%u",
            //							getSTR(ATTR_BASIC), getDEX(ATTR_BASIC), getINT(ATTR_BASIC), getSTRGoalExp(),
            // getDEXGoalExp(), getINTGoalExp();
            m_pAttrs[ATTR_KIND_STR]->getLevel(), m_pAttrs[ATTR_KIND_DEX]->getLevel(),
            m_pAttrs[ATTR_KIND_INT]->getLevel(), getSTRGoalExp(), getDEXGoalExp(), getINTGoalExp());

        tinysave(pField);
    }

    modifyInfo.addLongData(MODIFY_STR_EXP, getSTRGoalExp());
    modifyInfo.addLongData(MODIFY_DEX_EXP, getDEXGoalExp());
    modifyInfo.addLongData(MODIFY_INT_EXP, getINTGoalExp());
}

void Slayer::setLastTarget(ObjectID_t value) {
    if (getPet() != NULL && value != getLastTarget()) {
        GCOtherModifyInfo gcOMI;
        gcOMI.setObjectID(getObjectID());
        gcOMI.addLongData(MODIFY_LAST_TARGET, value);

        m_pZone->broadcastPacket(getX(), getY(), &gcOMI, this);

        GCModifyInformation gcMI;
        gcMI.addLongData(MODIFY_LAST_TARGET, value);

        getPlayer()->sendPacket(&gcMI);
    }

    Creature::setLastTarget(value);
}

void Slayer::initPetQuestTarget() {
    QuestGrade_t grade = getQuestGrade();

    int minClass = 1, maxClass = 1;

    if (grade <= 60) {
        minClass = maxClass = 2;
    } else if (grade <= 95) {
        minClass = maxClass = 3;
    } else if (grade <= 130) {
        minClass = 4;
        maxClass = 5;
    } else if (grade <= 170) {
        minClass = maxClass = 6;
    } else if (grade <= 210) {
        minClass = maxClass = 7;
    } else if (grade <= 240) {
        minClass = 7;
        maxClass = 8;
    } else if (grade <= 270) {
        minClass = 8;
        maxClass = 9;
    } else if (grade <= 290) {
        minClass = 9;
        maxClass = 10;
    } else if (grade <= 300) {
        minClass = maxClass = 10;
    } else if (grade <= 320) {
        minClass = 10;
        maxClass = 11;
    } else if (grade <= 360) {
        minClass = 10;
        maxClass = 11;
    } else {
        minClass = 11;
        maxClass = 12;
    }

    m_TargetMonster = de::gameContext().monsterInfos().getRandomMonsterByClass(minClass, maxClass);
    m_TargetNum = 80;
    m_TimeLimit = 3600;
}
