//////////////////////////////////////////////////////////////////////////////
// Filename    : PlayerCreature.cpp
// Written by  : excel96
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "PlayerCreature.h"

#include "Belt.h"
#include "CastleSkillInfo.h"
#include "EventKick.h"
#include "ExtraInfo.h"
#include "ExtraSlotInfo.h"
#include "FlagSet.h"
#include "GCModifyInformation.h"
#include "GCModifyNickname.h"
#include "GCMonsterKillQuestInfo.h"
#include "GCNoticeEvent.h"
#include "GCOtherModifyInfo.h"
#include "GCPetStashList.h"
#include "GCRankBonusInfo.h"
#include "GCSystemMessage.h"
#include "GCTimeLimitItemInfo.h"
#include "GameContext.h"
#include "GamePlayer.h"
#include "GameServerInfoManager.h"
#include "GoodsInventory.h"
#include "Guild.h"
#include "GuildManager.h"
#include "Inventory.h"
#include "InventoryInfo.h"
#include "InventorySlotInfo.h"
#include "Item.h"
#include "ItemUtil.h"
#include "Key.h"
#include "PKZoneInfoManager.h"
#include "ParkingCenter.h"
#include "Player.h"
#include "Properties.h"
#include "RankBonusInfo.h"
#include "RankExpTable.h"
#include "Stash.h"
#include "StringStream.h"
#include "TimeLimitItemManager.h"
#include "Zone.h"
#include "mission/QuestManager.h"
// #include "QuestManager.h"
// #include "QuestEvent.h"
// #include "Quest.h"
// #include "SimpleQuestLoader.h"
#include <stdio.h>

#include <algorithm>
#include <list>

#include "AdvancementClassExpTable.h"
#include "BloodBibleSignInfo.h"
#include "CreatureUtil.h"
#include "DefaultOptionSetInfo.h"
#include "GQuestManager.h"
#include "KernelContext.h"
#include "NicknameBook.h"
#include "Pet.h"
#include "PlayerRace.h"
#include "SMSAddressBook.h"
#include "ServerContext.h"
#include "Socket.h"
#include "Store.h"
#include "VariableManager.h"
#include "mofus/Mofus.h"
#include "repository/BloodBibleSignRepository.h"
#include "repository/CharacterRepository.h"
#include "repository/GoldRepository.h"
#include "repository/GoodsRepository.h"
#include "repository/RankBonusRepository.h"
#include "repository/StashRepository.h"
#include "skill/CastleSkillSlot.h"
#include "skill/OustersCastleSkillSlot.h"
#include "skill/OustersSkillSlot.h"
#include "skill/SkillSlot.h"
#include "skill/VampireCastleSkillSlot.h"
#include "skill/VampireSkillSlot.h"

//////////////////////////////////////////////////////////////////////////////
// constructor
//////////////////////////////////////////////////////////////////////////////
PlayerCreature::PlayerCreature(ObjectID_t OID, Player* pPlayer)

    : Creature(OID, pPlayer), m_pAdvancementClass(NULL) {
    __BEGIN_TRY

    m_pInventory = new Inventory(10, 6);
    m_pExtraInventorySlot = new InventorySlot();

    m_pGoodsInventory = new GoodsInventory();

    m_pStash = new Stash;
    m_StashNum = 0;
    m_StashGold = 0;
    m_bStashStatus = false;

    m_pFlagSet = new FlagSet;
    m_isPK = false;
    m_GuildID = 0;

    m_pQuestManager = new QuestManager(this);
    m_pTimeLimitItemManager = new TimeLimitItemManager(this);

    m_bLotto = false;
    m_pQuestItem = NULL;
    m_pPetInfo = NULL;

    m_RankExpSaveCount = 0;
    m_pRank = NULL;
    m_pPet = NULL;
    //	m_pSMSAddressBook = NULL;
    m_pSMSAddressBook = new SMSAddressBook(this);
    m_SMSCharge = 0;
    //	m_pNicknameBook = NULL;
    m_pNicknameBook = new NicknameBook(this);
    m_pNickname = NULL;

    m_PetStash.reserve(MAX_PET_STASH);
    for (int i = 0; i < MAX_PET_STASH; ++i)
        m_PetStash.push_back(NULL);

    m_pGQuestManager = new GQuestManager(this);
    m_pBloodBibleSign = new BloodBibleSignInfo;
    m_BaseLuck = 0;
    m_pStore = new Store;
    m_bAdvanced = false;
    m_AdvancementClassExpSaveCount = 0;

    m_AdvancedAttrBonus = 0;
    m_AdvancedSTR = 0;
    m_AdvancedDEX = 0;
    m_AdvancedINT = 0;
    m_MasterEffectColor = 0;

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
// destructor
//////////////////////////////////////////////////////////////////////////////
PlayerCreature::~PlayerCreature()

{
    __BEGIN_TRY

    // Delete inventories and pointer-held item.
    SAFE_DELETE(m_pInventory);

    SAFE_DELETE(m_pGoodsInventory);

    // Delete the item attached to the cursor.
    if (m_pExtraInventorySlot != NULL) {
        Item* pItem = m_pExtraInventorySlot->getItem();
        if (pItem != NULL) {
            if (pItem->getItemClass() == Item::ITEM_CLASS_KEY) {
                Key* pKey = dynamic_cast<Key*>(pItem);
                // Remove the motorcycle box directly from the zone here.
                if (de::gameContext().parking().hasMotorcycleBox(pKey->getTarget())) {
                    de::gameContext().parking().deleteMotorcycleBox(pKey->getTarget());
                }
            }

            m_pExtraInventorySlot->deleteItem();
            SAFE_DELETE(pItem);
        }

        SAFE_DELETE(m_pExtraInventorySlot);
    }

    // Delete stash.
    SAFE_DELETE(m_pStash);

    // Delete flag set.
    SAFE_DELETE(m_pFlagSet);

    // Delete rank bonus map entries.
    for (HashMapRankBonusItor itr = m_RankBonuses.begin(); itr != m_RankBonuses.end(); itr++) {
        SAFE_DELETE(itr->second);
    }
    m_RankBonuses.clear();

    if (m_pTimeLimitItemManager != NULL)
        SAFE_DELETE(m_pTimeLimitItemManager);

    if (m_pQuestManager != NULL)
        SAFE_DELETE(m_pQuestManager);

    if (m_pQuestItem != NULL)
        SAFE_DELETE(m_pQuestItem);

    SAFE_DELETE(m_pRank);

    for (int i = 0; i < MAX_PET_STASH; ++i) {
        SAFE_DELETE(m_PetStash[i]);
    }

    SAFE_DELETE(m_pSMSAddressBook);
    SAFE_DELETE(m_pNicknameBook);
    SAFE_DELETE(m_pGQuestManager);
    SAFE_DELETE(m_pBloodBibleSign);
    SAFE_DELETE(m_pStore);
    SAFE_DELETE(m_pAdvancementClass);

    __END_CATCH_NO_RETHROW
}

bool PlayerCreature::load() {
    __BEGIN_TRY

    m_pSMSAddressBook->load();

    m_PowerPoint = loadPowerPoint(getName());

    return true;

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
// Register a single item into the object registry.
// NOTE: caller must lock the registry; this function does not lock internally.
//////////////////////////////////////////////////////////////////////////////
void PlayerCreature::registerItem(Item* pItem, ObjectRegistry& OR)

{
    __BEGIN_TRY

    Assert(pItem != NULL);

    // Assign object ID to the item.
    OR.registerObject_NOLOCKED(pItem);
    // Inform time-limit manager about the new OID.
    m_pTimeLimitItemManager->registerItem(pItem);

    // If the item is a belt, also register contained items.
    if (pItem->getItemClass() == Item::ITEM_CLASS_BELT) {
        Belt* pBelt = dynamic_cast<Belt*>(pItem);
        PocketNum_t PocketCount = pBelt->getPocketCount();
        Inventory* pInventory = pBelt->getInventory();

        for (int k = 0; k < PocketCount; k++) {
            Item* pBeltItem = pInventory->getItem(k, 0);
            if (pBeltItem != NULL) {
                OR.registerObject_NOLOCKED(pBeltItem);
                m_pTimeLimitItemManager->registerItem(pBeltItem);
            }
        }
    }

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
// Register all items inside the inventory.
// NOTE: caller must lock the registry; this function does not lock internally.
//////////////////////////////////////////////////////////////////////////////
void PlayerCreature::registerInventory(ObjectRegistry& OR)

{
    __BEGIN_TRY

    list<Item*> ItemList;
    int height = m_pInventory->getHeight();
    int width = m_pInventory->getWidth();

    for (int j = 0; j < height; j++) {
        for (int i = 0; i < width; i++) {
            Item* pItem = m_pInventory->getItem(i, j);
            if (pItem != NULL) {
                // Skip already-registered stacked items.
                list<Item*>::iterator itr = find(ItemList.begin(), ItemList.end(), pItem);

                if (itr == ItemList.end()) {
                    // Track to avoid double registration.
                    ItemList.push_back(pItem);

                    // Register and assign OID.
                    registerItem(pItem, OR);

                    i += pItem->getVolumeWidth() - 1;
                }
            }
        }
    }

    __END_CATCH
}

void PlayerCreature::registerInitInventory(ObjectRegistry& OR)

{
    __BEGIN_TRY

    list<Item*> ItemList;
    int height = m_pInventory->getHeight();
    int width = m_pInventory->getWidth();

    for (int j = 0; j < height; j++) {
        for (int i = 0; i < width; i++) {
            Item* pItem = m_pInventory->getItem(i, j);
            if (pItem != NULL) {
                list<Item*>::iterator itr = find(ItemList.begin(), ItemList.end(), pItem);

                if (itr == ItemList.end()) {
                    ItemList.push_back(pItem);

                    pItem->setTraceItem(bTraceLog(pItem));

                    registerItem(pItem, OR);

                    i += pItem->getVolumeWidth() - 1;
                }
            }
        }
    }

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void PlayerCreature::registerStash(void)

{
    __BEGIN_TRY

    Zone* pZone = getZone();
    ObjectRegistry& OR = pZone->getObjectRegistry();

    __ENTER_CRITICAL_SECTION(OR)

    for (int r = 0; r < STASH_RACK_MAX; r++) {
        for (int i = 0; i < STASH_INDEX_MAX; i++) {
            Item* pStashItem = m_pStash->get(r, i);
            if (pStashItem != NULL)
                registerItem(pStashItem, OR);
        }
    }

    __LEAVE_CRITICAL_SECTION(OR)

    m_bStashStatus = true;

    __END_CATCH
}

void PlayerCreature::registerGoodsInventory(ObjectRegistry& OR)

{
    __BEGIN_TRY

    GoodsInventory::ListItem& goods = m_pGoodsInventory->getGoods();
    GoodsInventory::ListItemItr itr = goods.begin();

    for (; itr != goods.end(); itr++) {
        registerItem((*itr).m_pItem, OR);
    }

    __END_CATCH
}


void PlayerCreature::loadTimeLimitItem() {
    __BEGIN_TRY

    Assert(m_pTimeLimitItemManager != NULL);

    m_pTimeLimitItemManager->load();

    __END_CATCH
}

void PlayerCreature::loadItem()

{
    __BEGIN_TRY

    loadTimeLimitItem();

    m_pNicknameBook->load();
    m_pGQuestManager->init();

    vector<ItemType_t> signs = defaultBloodBibleSignRepository().loadItemTypes(getName());
    for (size_t i = 0; i < signs.size(); ++i) {
        m_pBloodBibleSign->getList().push_back(signs[i]);
    }

    __END_CATCH
}

void PlayerCreature::loadItem(bool checkTimeLimit) {
    __BEGIN_TRY

    PlayerCreature::loadItem();

    // Create the inventory. Delete the previous one before creating it.
    SAFE_DELETE(m_pInventory);
    m_pInventory = new Inventory(10, 6);
    m_pInventory->setOwner(getName());

    loadOwnedItems();

    // Load the purchased items.
    loadGoods();

    // Register the loaded items.
    registerInitObject();

    giveNewbieItems();

    if (checkTimeLimit) {
        checkItemTimeLimit();
    }

    // Compute the attributes from the equipped gear.
    initAllStat(-1);

    __END_CATCH
}

bool PlayerCreature::wasteIfTimeLimitExpired(Item* pItem)

{
    __BEGIN_TRY

    if (pItem == NULL)
        return false;

    if (m_pTimeLimitItemManager->wasteIfTimeOver(pItem)) {
        pItem->waste(STORAGE_TIMEOVER);
        return true;
    }

    return false;

    __END_CATCH
}

void PlayerCreature::sendTimeLimitItemInfo()

{
    __BEGIN_TRY

    GCTimeLimitItemInfo gcTimeLimitItemInfo;

    if (m_pTimeLimitItemManager->makeTimeLimitItemInfo(gcTimeLimitItemInfo)) {
        getPlayer()->sendPacket(&gcTimeLimitItemInfo);
    }

    __END_CATCH
}

void PlayerCreature::addTimeLimitItem(Item* pItem, DWORD time) {
    m_pTimeLimitItemManager->addTimeLimitItem(pItem, time);
}

void PlayerCreature::sellItem(Item* pItem) {
    __BEGIN_TRY

    if (pItem->isTimeLimitItem())
        m_pTimeLimitItemManager->itemSold(pItem);

    __END_CATCH
}

void PlayerCreature::deleteItemByMorph(Item* pItem) {
    __BEGIN_TRY

    if (pItem->isTimeLimitItem())
        m_pTimeLimitItemManager->deleteItemByMorph(pItem);

    __END_CATCH
}

void PlayerCreature::updateItemTimeLimit(Item* pItem, DWORD time) {
    __BEGIN_TRY

    if (pItem->isTimeLimitItem())
        m_pTimeLimitItemManager->updateItemTimeLimit(pItem, time);

    __END_CATCH
}

void PlayerCreature::sendCurrentQuestInfo() const {
    m_pQuestManager->sendQuestInfo();
}

void PlayerCreature::whenQuestLevelUpgrade() {
    static const int serverID = de::kernelContext().config().getPropertyInt("ServerID");
    static const int worldID = de::kernelContext().config().getPropertyInt("WorldID");
    static bool bNonPK = de::serverContext().serverInfos().getGameServerInfo(1, serverID, worldID)->isNonPKServer();

    if (bNonPK && getLevel() > 80) {
        GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(m_pPlayer);
        Assert(pGamePlayer != NULL);

        GCSystemMessage gcSM;
        gcSM.setMessage("This non-PK server is only for characters up to level 80."
                        " You will be disconnected in 10 seconds.");
        pGamePlayer->sendPacket(&gcSM);

        bool newEvent = false;
        EventKick* pEvent = dynamic_cast<EventKick*>(pGamePlayer->getEvent(Event::EVENT_CLASS_KICK));
        if (pEvent == NULL) {
            pEvent = new EventKick(pGamePlayer);
            newEvent = true;
        }

        pEvent->setDeadline(100);
        if (newEvent)
            pGamePlayer->addEvent(pEvent);
    }

    if (de::gameContext().variables().getVariable(CHOBO_EVENT)) {
        Level_t level = getLevel();
        if (level < 40 && (level % 5) == 0) {
            getGQuestManager()->getGQuestInventory().saveOne(getName(), 13 + (level / 5));
            getPlayer()->sendPacket(getGQuestManager()->getGQuestInventory().getInventoryPacket());

            GCNoticeEvent gcNE;
            gcNE.setCode(NOTICE_EVENT_GIVE_PRESENT_1 + (level / 5));
            getPlayer()->sendPacket(&gcNE);
        }
        if (level == 40) {
            GCNoticeEvent gcNE;
            gcNE.setCode(NOTICE_EVENT_CAN_OPEN_PRESENT_8);
            getPlayer()->sendPacket(&gcNE);
        }
    }

    m_pGQuestManager->levelUp();

    if (getQuestLevel() == 40) {
        GCNoticeEvent gcNE;
        gcNE.setCode(NOTICE_EVENT_CAN_PET_QUEST);
        getPlayer()->sendPacket(&gcNE);
    }

    if (getLevel() >= 10) {
        NicknameInfo* pLevelNickInfo = m_pNicknameBook->getNicknameInfo(1);
        if (pLevelNickInfo == NULL) {
            pLevelNickInfo = new NicknameInfo;
            pLevelNickInfo->setNicknameID(1);
            pLevelNickInfo->setNicknameType(NicknameInfo::NICK_BUILT_IN);
            pLevelNickInfo->setNicknameIndex(0);
            m_pNicknameBook->setNicknameInfo(1, pLevelNickInfo);
        }

        if (pLevelNickInfo->getNicknameType() == NicknameInfo::NICK_BUILT_IN &&
            pLevelNickInfo->getNicknameIndex() != getLevel() / 10) {
            pLevelNickInfo->setNicknameIndex(getLevel() / 10);

            getPlayer()->sendPacket(m_pNicknameBook->getNicknameBookListPacket().get());

            if (pLevelNickInfo == m_pNickname) {
                GCModifyNickname gcMN;
                gcMN.setObjectID(getObjectID());
                gcMN.setNicknameInfo(m_pNickname);
                getZone()->broadcastPacket(getX(), getY(), &gcMN);
            }
        }
    }
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void PlayerCreature::setStashNumEx(BYTE num)

{
    __BEGIN_TRY

    setStashNum(num);

    defaultStashRepository().saveStashNum(getName(), isOusters(), num);

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void PlayerCreature::setStashGoldEx(Gold_t gold)

{
    __BEGIN_TRY

    // cout << "setStashGoldEx Called" << "Name:" << getName() << " Gold: " << (int)gold << endl;
    setStashGold(gold);

    defaultStashRepository().saveStashGold(getName(), isOusters(), gold);

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void PlayerCreature::increaseStashGoldEx(Gold_t gold)

{
    __BEGIN_TRY

    setStashGold(m_StashGold + gold);

    defaultStashRepository().saveStashGold(getName(), isOusters(), m_StashGold);

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void PlayerCreature::decreaseStashGoldEx(Gold_t gold)

{
    __BEGIN_TRY

    setStashGold(m_StashGold - gold);

    defaultStashRepository().saveStashGold(getName(), isOusters(), m_StashGold);

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
// Delete stash in memory.
//////////////////////////////////////////////////////////////////////////////
void PlayerCreature::deleteStash(void)

{
    __BEGIN_TRY

    SAFE_DELETE(m_pStash);
    m_StashNum = 0;

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
// Delete flag set in memory.
//////////////////////////////////////////////////////////////////////////////
void PlayerCreature::deleteFlagSet(void)

{
    __BEGIN_TRY

    SAFE_DELETE(m_pFlagSet);

    __END_CATCH
}

//----------------------------------------------------------------------
// Add an aggressor to the aggressor list.
//----------------------------------------------------------------------
void PlayerCreature::addEnemy(const string& Name)

{
    __BEGIN_DEBUG

    list<string>::iterator itr = find(m_Enemies.begin(), m_Enemies.end(), Name);

    // Add if not already present.
    if (itr == m_Enemies.end()) {
        m_Enemies.push_back(Name);
    }

    __END_DEBUG
}

//----------------------------------------------------------------------
// Remove an aggressor from the aggressor list.
//----------------------------------------------------------------------
void PlayerCreature::deleteEnemy(const string& Name)

{
    __BEGIN_DEBUG

    list<string>::iterator itr = find(m_Enemies.begin(), m_Enemies.end(), Name);
    if (itr != m_Enemies.end()) {
        m_Enemies.erase(itr);
    }

    __END_DEBUG
}

//----------------------------------------------------------------------
// Check if an aggressor with a given name exists.
//----------------------------------------------------------------------
bool PlayerCreature::hasEnemy(const string& Name) const {
    __BEGIN_DEBUG

    list<string>::const_iterator itr = find(m_Enemies.begin(), m_Enemies.end(), Name);
    if (itr != m_Enemies.end()) {
        return true;
    } else {
        return false;
    }

    __END_DEBUG
}

//----------------------------------------------------------------------
//----------------------------------------------------------------------
string PlayerCreature::getGuildName() const

{
    Guild* pGuild = de::gameContext().guilds().getGuild(m_GuildID);

    if (pGuild != NULL)
        return pGuild->getName();

    return "";
}

//----------------------------------------------------------------------
//----------------------------------------------------------------------
GuildMemberRank_t PlayerCreature::getGuildMemberRank() const

{
    Guild* pGuild = de::gameContext().guilds().getGuild(m_GuildID);

    if (pGuild != NULL) {
        // A member the guild let go is retired rather than freed, so the
        // pointer stays readable and says so; it gets the same answer as a
        // player the guild never had.
        GuildMember* pGuildMember = pGuild->getMember(getName());
        if (pGuildMember != NULL && !pGuildMember->isRetired()) {
            return pGuildMember->getRank();
        }
    }

    return GuildMember::GUILDMEMBER_RANK_DENY;
}

Rank_t PlayerCreature::getRank() const {
    return m_pRank->getLevel();
}
RankExp_t PlayerCreature::getRankExp() const {
    return m_pRank->getTotalExp();
}
RankExp_t PlayerCreature::getRankGoalExp() const {
    return m_pRank->getGoalExp();
}

RankBonus* PlayerCreature::getRankBonus(RankBonus::RankBonusType type) const

{
    __BEGIN_TRY

    HashMapRankBonusConstItor itr = m_RankBonuses.find(type);

    if (itr == m_RankBonuses.end()) {
        return NULL;
    }

    return itr->second;

    __END_CATCH
}

void PlayerCreature::addRankBonus(RankBonus* rankBonus)

{
    __BEGIN_TRY

    HashMapRankBonusItor itr = m_RankBonuses.find(rankBonus->getType());

    if (itr == m_RankBonuses.end()) {
        m_RankBonuses[rankBonus->getType()] = rankBonus;
        m_RankBonusFlag.set(rankBonus->getType());
    } else {
        SAFE_DELETE(rankBonus);
    }

    __END_CATCH
}

void PlayerCreature::clearRankBonus()

{
    __BEGIN_TRY

    HashMapRankBonusItor itr = m_RankBonuses.begin();
    for (; itr != m_RankBonuses.end(); itr++) {
        SAFE_DELETE(itr->second);
    }

    m_RankBonuses.clear();
    m_RankBonusFlag.reset();

    defaultRankBonusRepository().deleteAll(getName());

    __END_CATCH
}

RankBonus* PlayerCreature::getRankBonusByRank(Rank_t rank) const

{
    __BEGIN_TRY

    HashMapRankBonusConstItor itr = m_RankBonuses.begin();

    for (; itr != m_RankBonuses.end(); itr++) {
        RankBonus* pLearnedRankBonus = itr->second;

        if (rank == pLearnedRankBonus->getRank()) {
            return pLearnedRankBonus;
        }
    }

    return NULL;

    __END_CATCH
}

void PlayerCreature::clearRankBonus(Rank_t rank)

{
    __BEGIN_TRY

    RankBonus* pRankBonus = getRankBonusByRank(rank);
    if (pRankBonus == NULL)
        return;

    DWORD rankBonusType = pRankBonus->getType();

    HashMapRankBonusItor itr = m_RankBonuses.find(rankBonusType);
    if (itr != m_RankBonuses.end()) {
        m_RankBonusFlag.reset(rankBonusType);
        SAFE_DELETE(itr->second);
        m_RankBonuses.erase(itr);
    }

    defaultRankBonusRepository().deleteOne(getName(), rankBonusType);

    __END_CATCH
}

bool PlayerCreature::learnRankBonus(DWORD type)

{
    //__BEGIN_TRY
    try {
        HashMapRankBonusConstItor itr = m_RankBonuses.begin();

        RankBonusInfo* pRankBonusInfo = de::gameContext().rankBonuses().getRankBonusInfo(type);

        bool bValidRace = (isSlayer() && pRankBonusInfo->getRace() == 0) ||
                          (isVampire() && pRankBonusInfo->getRace() == 1) ||
                          (isOusters() && pRankBonusInfo->getRace() == 2);

        if (getRank() < pRankBonusInfo->getRank())
            return false;

        if (!bValidRace)
            return false;

        for (; itr != m_RankBonuses.end(); itr++) {
            RankBonus* pLearnedRankBonus = itr->second;

            if (pRankBonusInfo->getRank() == pLearnedRankBonus->getRank()) {
                DWORD type = pLearnedRankBonus->getType();

                RankBonusInfo* pLearnedRankBonusInfo = de::gameContext().rankBonuses().getRankBonusInfo(type);

                if (pRankBonusInfo->getRace() == pLearnedRankBonusInfo->getRace()) {
                    return false;
                }
            }
        }

        RankBonus* rankBonus =
            new RankBonus(pRankBonusInfo->getType(), pRankBonusInfo->getPoint(), pRankBonusInfo->getRank());

        addRankBonus(rankBonus);

        defaultRankBonusRepository().insert(getName(), type);

        return true;
    } catch (Throwable& t) {
        filelog("PlayerCreature.txt", "PlayerCreature.cpp INSERT INTO RankBonusData error!");
        return false;
    }

    //__END_CATCH
}

void PlayerCreature::sendRankBonusInfo()

{
    __BEGIN_TRY

    HashMapRankBonusConstItor itr = m_RankBonuses.begin();

    GCRankBonusInfo gcRankBonusInfo;

    for (; itr != m_RankBonuses.end(); itr++) {
        gcRankBonusInfo.addListElement(itr->second->getType());
    }

    m_pPlayer->sendPacket(&gcRankBonusInfo);

    __END_CATCH
}

void PlayerCreature::loadRankBonus()

{
    __BEGIN_TRY

    vector<DWORD> types = defaultRankBonusRepository().loadTypes(getName());

    for (size_t i = 0; i < types.size(); ++i) {
        DWORD rankBonusType = types[i];

        RankBonusInfo* pRankBonusInfo = de::gameContext().rankBonuses().getRankBonusInfo(rankBonusType);

        if (getRace() == pRankBonusInfo->getRace()) {
            RankBonus* pRankBonus = new RankBonus();

            pRankBonus->setType(rankBonusType);
            pRankBonus->setPoint(pRankBonusInfo->getPoint());
            pRankBonus->setRank(pRankBonusInfo->getRank());

            addRankBonus(pRankBonus);
        }
    }

    __END_CATCH
}

void PlayerCreature::increaseRankExp(RankExp_t Point) {
    if (Point <= 0)
        return;

    if (de::gameContext().pkZoneInfos().isPKZone(getZoneID()))
        return;

    if (m_pZone != NULL && m_pZone->isDynamicZone())
        return;

    if (isFlag(Effect::EFFECT_CLASS_BONUS_EXP))
        Point *= 2;

    if (m_pRank->increaseExp(Point)) {
        char pField[80];
        snprintf(pField, sizeof(pField), "`Rank`=%u, RankExp=%u, RankGoalExp=%u", getRank(), getRankExp(),
                 getRankGoalExp());
        tinysave(pField);
        setRankExpSaveCount(0);

        GCModifyInformation gcModifyInformation;
        gcModifyInformation.addLongData(MODIFY_RANK, getRank());
        m_pPlayer->sendPacket(&gcModifyInformation);

        if (m_pZone != NULL) {
            GCOtherModifyInfo gcOtherModifyInfo;
            gcOtherModifyInfo.setObjectID(getObjectID());
            gcOtherModifyInfo.addShortData(MODIFY_RANK, getRank());

            m_pZone->broadcastPacket(m_X, m_Y, &gcOtherModifyInfo, this);
        }
    } else {
        WORD rankExpSaveCount = getRankExpSaveCount();
        if (rankExpSaveCount > RANK_EXP_SAVE_PERIOD) {
            char pField[80];
            snprintf(pField, sizeof(pField), "RankExp=%u, RankGoalExp=%u", getRankExp(), getRankGoalExp());
            tinysave(pField);

            rankExpSaveCount = 0;
        } else
            rankExpSaveCount++;
        setRankExpSaveCount(rankExpSaveCount);

        GCModifyInformation gcModifyInformation;
        gcModifyInformation.addLongData(MODIFY_RANK_EXP, getRankGoalExp());
        m_pPlayer->sendPacket(&gcModifyInformation);
    }
}

bool PlayerCreature::isPayPlayAvaiable()

{
    __BEGIN_TRY

    if (m_pPlayer == NULL)
        return false;

    GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(m_pPlayer);

    return pGamePlayer->isPayPlaying();

    __END_CATCH
}


void PlayerCreature::loadGoods()

{
    __BEGIN_TRY

    if (m_pGoodsInventory->getNum() != 0) {
        filelog("GoodsReload.log", "  ? : %s", getName().c_str());
        m_pGoodsInventory->clear();
    }

    vector<GoodsRecord> records = defaultGoodsRepository().loadPending(
        de::kernelContext().config().getPropertyInt("WorldID"), getPlayer()->getID(), getName());

    for (size_t r = 0; r < records.size(); ++r) {
        const GoodsRecord& record = records[r];

        for (int i = 0; i < max(1, min(50, record.num)); i++) {
            Item* pItem = createItemByGoodsID(record.goodsID);
            if (pItem != NULL) {
                m_pGoodsInventory->addItem(record.id, pItem);
            }
        }
    }

    __END_CATCH
}

void PlayerCreature::addDefaultOptionSet(DefaultOptionSetType_t type)

{
    forward_list<DefaultOptionSetType_t>::iterator itr = m_DefaultOptionSet.begin();
    for (; itr != m_DefaultOptionSet.end(); itr++) {
        if ((*itr) == type)
            return;
    }

    m_DefaultOptionSet.push_front(type);
}

void PlayerCreature::removeDefaultOptionSet(DefaultOptionSetType_t type)

{
    forward_list<DefaultOptionSetType_t>::iterator before = m_DefaultOptionSet.end();
    forward_list<DefaultOptionSetType_t>::iterator current = m_DefaultOptionSet.begin();

    for (; current != m_DefaultOptionSet.end(); before = current++) {
        if ((*current) == type) {
            if (before == m_DefaultOptionSet.end()) {
                // delete first node
                m_DefaultOptionSet.pop_front();
            } else {
                m_DefaultOptionSet.erase_after(before);
            }

            return;
        }
    }
}

PetInfo* PlayerCreature::getPetInfo() const {
    return m_pPetInfo;
}

void PlayerCreature::setPetInfo(PetInfo* pPetInfo) {
    m_pPetInfo = pPetInfo;
    SAFE_DELETE(m_pPet);
    m_pPet = Pet::makePet(this, m_pPetInfo);
}

void PlayerCreature::heartbeat(const Timeval& currentTime) {
    if (m_pPet != NULL)
        m_pPet->heartbeat(currentTime);
    m_pGQuestManager->heartbeat();
}


GCMonsterKillQuestInfo::QuestInfo* PlayerCreature::getPetQuestInfo() const {
    GCMonsterKillQuestInfo::QuestInfo* pQI = new GCMonsterKillQuestInfo::QuestInfo;

    pQI->questID = PET_QUEST_ID;
    pQI->sType = m_TargetMonster;
    pQI->goal = m_TargetNum;
    pQI->timeLimit = m_TimeLimit;

    return pQI;
}

void PlayerCreature::addPetStashItem(int idx, Item* pPetItem) {
    Assert(pPetItem == NULL || pPetItem->getItemClass() == Item::ITEM_CLASS_PET_ITEM);
    Assert(idx >= 0 && idx <= MAX_PET_STASH);
    m_PetStash[idx] = pPetItem;
}

Item* PlayerCreature::getPetStashItem(int idx) {
    Assert(idx >= 0 && idx <= MAX_PET_STASH);
    return m_PetStash[idx];
}

bool PlayerCreature::canSee(Object* pObject) const {
    if (pObject->getObjectClass() == OBJECT_CLASS_CREATURE) {
        return ::canSee(this, dynamic_cast<Creature*>(pObject));
    } else {
        return true;
    }
}

Level_t PlayerCreature::getAdvancementClassLevel() const {
    return m_pAdvancementClass->getLevel();
}
Exp_t PlayerCreature::getAdvancementClassGoalExp() const {
    return m_pAdvancementClass->getGoalExp();
}
bool PlayerCreature::increaseAdvancementClassExp(Exp_t exp, bool bApplyExpBonus) {
    VariableManager& variables = de::gameContext().variables();

    GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(m_pPlayer);
    if (pGamePlayer != NULL) {
        if (bApplyExpBonus && (pGamePlayer->isPremiumPlay() || pGamePlayer->isFamilyFreePass())) {
            exp = getPercentValue(exp, variables.getPremiumExpBonusPercent());
        }

        if (bApplyExpBonus && variables.getExpRatio() > 100 && variables.getEventActivate() == 1) {
            exp = getPercentValue(exp, variables.getExpRatio());
        }

        if (bApplyExpBonus && isAffectExp2X()) {
            // Double EXP effect
            exp = exp * 2;
        }
        // chengh modify 2006-07-21: bonus EXP effect.
        if (isFlag(Effect::EFFECT_CLASS_BONUS_EXP)) {
            exp *= 2;
        }
    }

    cout << getName() << " gains " << static_cast<int>(exp) << " advancement EXP." << endl;
    Level_t prevLevel = getAdvancementClassLevel();
    bool ret = m_pAdvancementClass->increaseExp(exp, true, true);
    if (getAdvancementClassLevel() > 0)
        m_bAdvanced = true;
    Level_t nextLevel = getAdvancementClassLevel();
    cout << getName() << " advanced from " << static_cast<int>(prevLevel) << " to " << static_cast<int>(nextLevel)
         << "." << endl;

    if (prevLevel != nextLevel) {
        Bonus_t bonus = getBonus();
        Level_t _4pointstart = min((int)prevLevel, 50);
        Level_t _4pointend = min((int)nextLevel, 50);
        Level_t _5pointstart = max((int)prevLevel, 50);
        Level_t _5pointend = max((int)nextLevel, 50);

        Bonus_t bonusdiff = ((_4pointend - _4pointstart) * 4) + ((_5pointend - _5pointstart) * 5);
        cout << getName() << " receives " << static_cast<int>(bonusdiff) << " attribute points." << endl;
        bonus += bonusdiff;
        setBonus(bonus);

        StringStream sav;
        sav << "AdvancementClass = " << (int)nextLevel << ",AdvancementGoalExp = " << (int)getAdvancementClassGoalExp()
            << ",Bonus = " << (int)getBonus();

        tinysave(sav.toString());
        initAllStatAndSend();
    } else {
        m_AdvancementClassExpSaveCount++;
        if (m_AdvancementClassExpSaveCount > 100) {
            StringStream sav;
            sav << "AdvancementClass = " << (int)getAdvancementClassLevel()
                << ",AdvancementGoalExp = " << (int)getAdvancementClassGoalExp();
            tinysave(sav.toString());
            m_AdvancementClassExpSaveCount = 0;
        }
    }

    GCModifyInformation gcMI;
    gcMI.addLongData(MODIFY_ADVANCEMENT_CLASS_GOAL_EXP, getAdvancementClassGoalExp());
    if (ret) {
        gcMI.addShortData(MODIFY_ADVANCEMENT_CLASS_LEVEL, getAdvancementClassLevel());
        gcMI.addShortData(MODIFY_BONUS_POINT, getBonus());
        m_pGQuestManager->advancementClassLevelUp();
    }

    getPlayer()->sendPacket(&gcMI);

    return ret;
}

bool PlayerCreature::putAdvancedBonusToSTR() {
    if (m_AdvancedAttrBonus <= 0)
        return false;
    m_AdvancedAttrBonus--;
    m_AdvancedSTR++;
    return true;
}

bool PlayerCreature::putAdvancedBonusToDEX() {
    if (m_AdvancedAttrBonus <= 0)
        return false;
    m_AdvancedAttrBonus--;
    m_AdvancedDEX++;
    return true;
}

bool PlayerCreature::putAdvancedBonusToINT() {
    if (m_AdvancedAttrBonus <= 0)
        return false;
    m_AdvancedAttrBonus--;
    m_AdvancedINT++;
    return true;
}
bool PlayerCreature::canChangeMasterEffectColor() {
    // MasterEffectColor 5 is reserved for guild war champions;
    // it cannot be changed via shop conversion items.
    // 2005.05.17 by bezz
    return m_MasterEffectColor != 5;
}

//////////////////////////////////////////////////////////////////////////////
// Behaviour shared by every player race.
//////////////////////////////////////////////////////////////////////////////

const Color_t UNIQUE_COLOR = 0xFFFF;
const Color_t QUEST_COLOR = 0xFFFE;

CharacterRace PlayerCreature::characterRace() const {
    return characterRaceOf(getRace());
}

void PlayerCreature::tinysave(const string& field) const {
    __BEGIN_TRY

    defaultCharacterRepository().tinysave(m_Name, characterRace(), field);

    __END_CATCH
}

void PlayerCreature::saveExps(Exp_t goalExp, Silver_t silverDamage) const {
    __BEGIN_TRY

    // The skill handlers persist experience only every tenth tick, so the
    // gain below that threshold lives in memory alone. A character that
    // logs out normally flushes it here.
    CharacterExpsRecord record;
    record.alignment = getAlignment();
    record.fame = getFame();
    record.goalExp = goalExp;
    record.silverDamage = silverDamage;
    record.rank = getRank();
    record.rankGoalExp = getRankGoalExp();
    record.advancementClass = getAdvancementClassLevel();
    record.advancementGoalExp = getAdvancementClassGoalExp();
    defaultCharacterRepository().saveExps(m_Name, characterRace(), record);

    __END_CATCH
}

void PlayerCreature::setGold(Gold_t gold) {
    __BEGIN_TRY

    // Never let the amount pass MAX_MONEY.
    m_Gold = min((Gold_t)MAX_MONEY, gold);

    __END_CATCH
}

void PlayerCreature::setGoldEx(Gold_t gold) {
    __BEGIN_TRY

    setGold(gold);

    char pField[80];
    snprintf(pField, sizeof(pField), "Gold=%u", m_Gold);
    tinysave(pField);

    __END_CATCH
}

void PlayerCreature::increaseGoldEx(Gold_t gold) {
    __BEGIN_TRY
    __BEGIN_DEBUG

    // Prevent going over MAX_MONEY
    if (m_Gold + gold > MAX_MONEY)
        gold = MAX_MONEY - m_Gold;

    setGold(m_Gold + gold);

    defaultGoldRepository().increaseGold(m_Name, characterRace(), gold);

    __END_DEBUG
    __END_CATCH
}

void PlayerCreature::decreaseGoldEx(Gold_t gold) {
    __BEGIN_TRY
    __BEGIN_DEBUG

    // Prevent going below 0. Below 0 it underflows and causes chaos.
    if (m_Gold < gold)
        gold = m_Gold;

    setGold(m_Gold - gold);

    defaultGoldRepository().decreaseGold(m_Name, characterRace(), gold);

    __END_DEBUG
    __END_CATCH
}

bool PlayerCreature::checkGoldIntegrity() {
    __BEGIN_TRY

    int gold = 0;
    if (!defaultGoldRepository().loadGold(m_Name, characterRace(), gold))
        return false;

    return gold == m_Gold;

    __END_CATCH
}

bool PlayerCreature::checkStashGoldIntegrity() {
    __BEGIN_TRY

    int gold = 0;
    if (!defaultStashRepository().loadStashGold(m_Name, characterRace(), gold))
        return false;

    return gold == m_StashGold;

    __END_CATCH
}

void PlayerCreature::setResurrectZoneIDEx(ZoneID_t id) {
    __BEGIN_TRY

    setResurrectZoneID(id);

    char pField[80];
    snprintf(pField, sizeof(pField), "ResurrectZone=%d", id);
    tinysave(pField);

    __END_CATCH
}

void PlayerCreature::saveAlignment(Alignment_t alignment) {
    __BEGIN_TRY

    setAlignment(alignment);

    char pField[80];
    snprintf(pField, sizeof(pField), "Alignment=%d", alignment);
    tinysave(pField);

    __END_CATCH
}

void PlayerCreature::saveSilverDamage(Silver_t damage) {
    __BEGIN_TRY

    // A slayer row has no SilverDamage column; the value stays at zero.
    if (getRace() == RACE_SLAYER)
        return;

    setSilverDamage(damage);

    char pField[80];
    snprintf(pField, sizeof(pField), "SilverDamage=%d", m_SilverDamage);
    tinysave(pField);

    __END_CATCH
}

void PlayerCreature::saveInitialRank() {
    int curRank = max(1, (getLevel() + 3) / 4);
    m_pRank->SET_LEVEL(curRank);

    char pField[80];
    snprintf(pField, sizeof(pField), "`Rank`=%d, RankExp=%u, RankGoalExp=%u", getRank(), getRankExp(),
             getRankGoalExp());
    tinysave(pField);
    setRankExpSaveCount(0);
}

IP_t PlayerCreature::getIP(void) const {
    Assert(m_pPlayer != NULL);
    Socket* pSocket = m_pPlayer->getSocket();
    Assert(pSocket != NULL);
    return pSocket->getHostIP();
}

Color_t PlayerCreature::getItemShapeColor(Item* pItem, OptionInfo* pOptionInfo) const {
    Color_t color;

    if (pItem->isTimeLimitItem()) {
        // Time-limited (quest) items get a colour of their own.
        color = QUEST_COLOR;
    } else if (pItem->isUnique()) {
        // So do unique items.
        color = UNIQUE_COLOR;
    }
    // The caller already looked the OptionInfo up.
    else if (pOptionInfo != NULL) {
        color = pOptionInfo->getColor();
    }
    // Otherwise take the colour of the item's first option.
    else if (pItem->getFirstOptionType() != 0) {
        OptionInfo* pOptionInfo = de::gameContext().optionInfos().getOptionInfo(pItem->getFirstOptionType());
        color = pOptionInfo->getColor();
    } else {
        // Default colour.
        color = 377;
    }

    return color;
}

// The item held on the mouse cursor, as the client is told to draw it.
ExtraInfo* PlayerCreature::getExtraInfo() const {
    __BEGIN_TRY
    __BEGIN_DEBUG

    ExtraInfo* pExtraInfo = new ExtraInfo();

    Item* pItem = m_pExtraInventorySlot->getItem();

    if (pItem != NULL) {
        ExtraSlotInfo* pExtraSlotInfo = new ExtraSlotInfo();
        pItem->makePCItemInfo(*pExtraSlotInfo);

        pExtraInfo->addListElement(pExtraSlotInfo);
    }

    return pExtraInfo;

    __END_DEBUG
    __END_CATCH
}

// The carried inventory, as the client is told to draw it.
InventoryInfo* PlayerCreature::getInventoryInfo() const {
    __BEGIN_TRY
    __BEGIN_DEBUG

    InventoryInfo* pInventoryInfo = new InventoryInfo();
    list<Item*> ItemList;
    VolumeHeight_t Height = m_pInventory->getHeight();
    VolumeWidth_t Width = m_pInventory->getWidth();

    for (int j = 0; j < Height; j++) {
        for (int i = 0; i < Width; i++) {
            if (m_pInventory->hasItem(i, j)) {
                Item* pItem = m_pInventory->getItem(i, j);
                VolumeWidth_t ItemWidth = pItem->getVolumeWidth();

                list<Item*>::iterator itr = find(ItemList.begin(), ItemList.end(), pItem);

                if (itr == ItemList.end()) {
                    // An item wider than one cell is met once per cell it
                    // covers; report it from the leftmost one only.
                    ItemList.push_back(pItem);

                    InventorySlotInfo* pInventorySlotInfo = new InventorySlotInfo();
                    pItem->makePCItemInfo(*pInventorySlotInfo);
                    pInventorySlotInfo->setInvenX(i);
                    pInventorySlotInfo->setInvenY(j);

                    pInventoryInfo->addListElement(pInventorySlotInfo);
                    i = i + ItemWidth - 1;
                }
            }
        }
    }

    return pInventoryInfo;

    __END_DEBUG
    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
// Skill slot table
//
// Each race keys its own slot class by skill type; the operations below do
// not depend on which class that is, so the race passes its map in and, for
// the castle removal, names the castle slot class whose instances may be
// deleted.
//////////////////////////////////////////////////////////////////////////////
template <class SlotType>
SlotType* PlayerCreature::findSkillSlot(const unordered_map<SkillType_t, SlotType*>& skillSlots,
                                        SkillType_t SkillType) const {
    __BEGIN_TRY

    typename unordered_map<SkillType_t, SlotType*>::const_iterator itr = skillSlots.find(SkillType);
    if (itr != skillSlots.end()) {
        return itr->second;
    }

    return NULL;

    __END_CATCH
}

// Removes a castle skill.
template <class SlotType, class CastleSlotType>
void PlayerCreature::removeCastleSkillSlot(unordered_map<SkillType_t, SlotType*>& skillSlots, SkillType_t SkillType) {
    __BEGIN_TRY

    // Only a castle skill may be removed.
    if (de::gameContext().castleSkills().getZoneID(SkillType) == 0)
        return;

    typename unordered_map<SkillType_t, SlotType*>::iterator itr = skillSlots.find(SkillType);

    if (itr != skillSlots.end()) {
        CastleSlotType* pCastleSkillSlot = dynamic_cast<CastleSlotType*>(itr->second);

        SAFE_DELETE(pCastleSkillSlot);

        skillSlots.erase(itr);
    }

    __END_CATCH
}

// Removes every castle skill the character has.
template <class SlotType>
void PlayerCreature::removeAllCastleSkillSlots(unordered_map<SkillType_t, SlotType*>& skillSlots) {
    __BEGIN_TRY

    typename unordered_map<SkillType_t, SlotType*>::iterator itr = skillSlots.begin();

    while (itr != skillSlots.end()) {
        if (itr->second != NULL) {
            SlotType* pSkillSlot = itr->second;
            if (de::gameContext().castleSkills().getZoneID(pSkillSlot->getSkillType()) == 0) {
                // Not a castle skill, move on to the next one.
                ++itr;
                continue;
            }

            // A castle skill is removed. Mind the iterator.
            SAFE_DELETE(pSkillSlot);
            typename unordered_map<SkillType_t, SlotType*>::iterator prevItr = itr;

            ++itr;
            skillSlots.erase(prevItr);
        } else {
            // A null skill slot should not be in the map.
            Assert(false);
        }
    }

    __END_CATCH
}

template <class SlotType>
void PlayerCreature::saveSkillSlots(const unordered_map<SkillType_t, SlotType*>& skillSlots) const {
    __BEGIN_TRY

    typename unordered_map<SkillType_t, SlotType*>::const_iterator itr = skillSlots.begin();
    for (; itr != skillSlots.end(); itr++) {
        SlotType* pSkillSlot = itr->second;
        Assert(pSkillSlot != NULL);

        // Not a basic attack skill...
        if (pSkillSlot->getSkillType() >= SKILL_DOUBLE_IMPACT) {
            pSkillSlot->save(m_Name);
        }
    }

    __END_CATCH
}

// The three races that keep such a table.
template SkillSlot* PlayerCreature::findSkillSlot(const unordered_map<SkillType_t, SkillSlot*>&, SkillType_t) const;
template VampireSkillSlot* PlayerCreature::findSkillSlot(const unordered_map<SkillType_t, VampireSkillSlot*>&,
                                                         SkillType_t) const;
template OustersSkillSlot* PlayerCreature::findSkillSlot(const unordered_map<SkillType_t, OustersSkillSlot*>&,
                                                         SkillType_t) const;

template void PlayerCreature::removeCastleSkillSlot<SkillSlot, CastleSkillSlot>(unordered_map<SkillType_t, SkillSlot*>&,
                                                                                SkillType_t);
template void PlayerCreature::removeCastleSkillSlot<VampireSkillSlot, VampireCastleSkillSlot>(
    unordered_map<SkillType_t, VampireSkillSlot*>&, SkillType_t);
template void PlayerCreature::removeCastleSkillSlot<OustersSkillSlot, OustersCastleSkillSlot>(
    unordered_map<SkillType_t, OustersSkillSlot*>&, SkillType_t);

template void PlayerCreature::removeAllCastleSkillSlots(unordered_map<SkillType_t, SkillSlot*>&);
template void PlayerCreature::removeAllCastleSkillSlots(unordered_map<SkillType_t, VampireSkillSlot*>&);
template void PlayerCreature::removeAllCastleSkillSlots(unordered_map<SkillType_t, OustersSkillSlot*>&);

template void PlayerCreature::saveSkillSlots(const unordered_map<SkillType_t, SkillSlot*>&) const;
template void PlayerCreature::saveSkillSlots(const unordered_map<SkillType_t, VampireSkillSlot*>&) const;
template void PlayerCreature::saveSkillSlots(const unordered_map<SkillType_t, OustersSkillSlot*>&) const;
