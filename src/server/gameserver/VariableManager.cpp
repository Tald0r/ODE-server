#include "VariableManager.h"

#include <stdlib.h>

#include "DiagnosticTrace.h"
#include "KernelContext.h"
#include "Properties.h"
#include "SystemAvailabilitiesManager.h"
#include "ZoneInternal.h"
#include "repository/ContentInfoRepository.h"


//////////////////////////////////////////////////////////////////////////////
// constructor
//////////////////////////////////////////////////////////////////////////////
VariableManager::VariableManager()

{
    __BEGIN_TRY

    m_Variables = VARIABLE(VARIABLE_MAX);

    m_Variables[STAR_RATIO] = 1000;
    m_Variables[EVENT_ACTIVE] = 1;
    m_Variables[EVENT_RATIO] = 1000;
    m_Variables[ITEM_PROBE_RATIO] = 1;
    m_Variables[EXP_RATIO] = 1;
    m_Variables[COMBAT_BONUS_TIME] = 30; // 30 minutes
    m_Variables[COMBAT_SLAYER_HP_BONUS_RATIO] = 0;
    m_Variables[COMBAT_VAMPIRE_HP_BONUS_RATIO] = 0;
    m_Variables[COMBAT_SLAYER_DAMAGE_BONUS] = 0;
    m_Variables[COMBAT_VAMPIRE_DAMAGE_BONUS] = 0;
    m_Variables[PREMIUM_EXP_BONUS_PERCENT] = 150;               // 150%
    m_Variables[PREMIUM_ITEM_PROBE_PERCENT] = 220;              // 220%
    m_Variables[ZONE_GROUP_BALANCING_MINUTE] = 4 * 7 * 24 * 60; // 4 weeks
    m_Variables[GAMBLE_ITEM_TYPE_RATIO] = 100;                  // 100%
    m_Variables[GAMBLE_ITEM_OPTION_RATIO] = 100;                // 100%
    m_Variables[SUMMON_MOTORCYCLE] = 0;
    m_Variables[ENEMY_LIMIT_TIME] = 300;  // 5 minutes
    m_Variables[RARE_ITEM_RATIO] = 100;   // 100%
    m_Variables[UNIQUE_ITEM_RATIO] = 100; // 100%
    m_Variables[ACTIVE_MASTER_LAIR] = 0;
    m_Variables[RETRY_MASTER_LAIR] = 0;
    m_Variables[HARVEST_FESTIVAL_ITEM_RATIO] = 100; // 100%

    // [Master blood drain scheme]
    // At 70% HP or above it does not drain,
    // at 70% HP it drains with a 10% chance,
    //      50%     20%
    // and at 30% HP it drains with a 30% chance.
    // At 30% HP or below it drains with a 30% chance.
    m_Variables[MASTER_BLOOD_DRAIN_START_HP] = 70;
    m_Variables[MASTER_BLOOD_DRAIN_START_BD] = 10;
    m_Variables[MASTER_BLOOD_DRAIN_END_HP] = 30;
    m_Variables[MASTER_BLOOD_DRAIN_END_BD] = 30;

    m_Variables[ACTIVE_CHIEF_MONSTER] = 0;
    m_Variables[CHIEF_MONSTER_RARE_ITEM_RATIO] = 0;

    m_Variables[NEWBIE_TRANSPORT_TO_GUILD] = 1;

    // 2002.11.19
    m_Variables[FREE_PLAY_SLAYER_DOMAIN_SUM] = 30;
    m_Variables[FREE_PLAY_VAMPIRE_LEVEL] = 30;

    // 2002.12.10
    m_Variables[LOG_INCOMING_CONNECTION] = false;

    // 2002.12.11
    m_Variables[CHRISTMAS_FIRE_CRACKER_RATIO] = 0;
    m_Variables[CHRISTMAS_TREE_PART_RATIO] = 0;

    // 2002.12.12
    m_Variables[CHRISTMAS_GIFT_BOX_RATIO] = 0;

    // 2002.12.17
    m_Variables[CHRISTMAS_TREE_DECAY_TIME] = 6 * 60 * 60 * 10;

    // 2002.12.31
    m_Variables[MASTER_LAIR_PLAYER_NUM] = 15;
    m_Variables[RANK_EXP_GAIN_PERCENT] = 100; // 100 leaves it unchanged
    m_Variables[ITEM_LUCK_PERCENT] = 100;     // 200 means double (200%)

    // For the jackpot event ( code name : lotto event )
    // 2003. 1. 12 by bezz. Sequoia. dew
    m_Variables[LOTTO_ITEM_RATIO] = 0;
    m_Variables[LOTTO_ITEM_BONUS_NUM] = 0;
    m_Variables[LOTTO_SKULL_RATIO] = 0;
    m_Variables[PREMIUM_HALF_EVENT] = false;

    // 2003. 1. 20 by bezz, Sequoia
    m_Variables[COMMON_CASTLE_ENTRANCE_FEE] = 1000;
    m_Variables[GUILD_CASTLE_ENTRANCE_FEE] = 10000;

    m_Variables[COMMON_CASTLE_ITEM_TAX_RATIO] = 110;
    m_Variables[GUILD_CASTLE_ITEM_TAX_RATIO] = 150;
    m_Variables[WAR_REGISTRATION_FEE] = 50000000;

    m_Variables[GUILD_WAR_TIME] = 1 * 60 * 60; // 1 hour
    m_Variables[RACE_WAR_TIME] = 3 * 60 * 60;  // 3 hours

    m_Variables[WAR_PERIOD_WEEK] = true; // Wars run on a one-week cycle.

    m_Variables[WAR_ACTIVE] = false; // The war feature is not running.

    m_Variables[RACE_WAR_TIMEBAND] = 2; // Timeband of the race war

    m_Variables[AUTO_START_RACE_WAR] = false; // Is the race war started automatically?

    m_Variables[ACTIVATE_COUPLE] = false; // Is this a period in which couples can be formed?

    m_Variables[ACTIVE_RACE_WAR_LIMITER] = true; // Limit on the number of race war participants

    m_Variables[CHIEF_ITEM_BONUS_NUM] = 0; // Number of extra items from a chief monster

    m_Variables[ACTIVE_EVENT_GIVE_ITEM] = false; // Item giveaway event

    m_Variables[VAMPIRE_REDISTRIBUTE_ATTR_PRICE] = 1000000; // Cost of redistributing attributes

    m_Variables[ACTIVATE_COUPLE] = true; // Couple feature

    m_Variables[CAN_RECOUPLE] = false; // Whether a couple can be formed again

    m_Variables[GUILD_WAR_ACTIVE] = false; // Are guild wars run?

    m_Variables[FREE_PLAY_OUSTERS_LEVEL] = 30; // Level up to which Ousters can be played for free

    m_Variables[CAN_APPLY_QUEST] = true;  // Can quests be applied for?
    m_Variables[SEND_QUEST_INFO] = false; // Must the quest information be sent?
    m_Variables[CAN_BUY_SHOP] = true;

    m_Variables[PCROOM_EXP_BONUS] = 100;
    m_Variables[PCROOM_ITEM_RATIO_BONUS] = 100;

    m_Variables[MONEY_TRACE_LOG_LIMIT] = 5000000;

    m_Variables[PC_ROOM_LOTTO_EVENT] = false; // Whether the PC room lottery event is on
    m_Variables[ACTIVE_FLAG_WAR] = false;     // Whether capture the flag is running

    m_Variables[KILL_DAEMONCTL] = false;  // Whether daemonctl2 is killed
    m_Variables[EVENT_MOON_CARD] = false; // Whether the moon card event is on

    m_Variables[FULL_MOON_CARD_RATIO] = 3000;
    m_Variables[HALF_MOON_CARD_RATIO] = 100;
    m_Variables[NEW_MOON_CARD_RATIO] = 200;
    m_Variables[OLD_MOON_CARD_RATIO] = 1000;

    m_Variables[ACTIVE_LEVEL_WAR] = 1000;
    m_Variables[RACE_WAR_HP_BONUS] = 0;
    m_Variables[HEAD_COUNT] = 0;

    m_Variables[HEAD_PRICE_BONUS] = 100;

    m_Variables[LEVEL_WAR_ZONE_FREE_ENTER] = false;
    m_Variables[PET_EXP_RATIO] = 100;

    m_Variables[EVENT_LUCKY_BAG] = false;
    m_Variables[GREEN_LUCKY_BAG_RATIO] = 150;
    m_Variables[BLUE_LUCKY_BAG_RATIO] = 1000;
    m_Variables[GOLD_LUCKY_BAG_RATIO] = 5000;
    m_Variables[RED_LUCKY_BAG_RATIO] = 20000;

    m_Variables[EVENT_GIFT_BOX] = false;
    m_Variables[RED_GIFT_BOX_RATIO] = 10000;
    m_Variables[BLUE_GIFT_BOX_RATIO] = 10000;
    m_Variables[GREEN_GIFT_BOX_RATIO] = 10000;
    m_Variables[YELLOW_GIFT_BOX_RATIO] = 10000;

    m_Variables[NETMARBLE_CARD_RATIO] = 0;
    m_Variables[NETMARBLE_CARD_EVENT] = 0;

    m_Variables[PET_FOOD_EVENT] = 0;

    m_Variables[HIGHER_PET_FOOD_RATIO] = 30;

    m_Variables[PET_FOOD_RATIO] = 125;
    m_Variables[RACE_PET_FOOD_RATIO] = 30;

    m_Variables[SLAYER_HP_RATIO] = 100;
    m_Variables[VAMPIRE_HP_RATIO] = 100;
    m_Variables[OUSTERS_HP_RATIO] = 100;
    m_Variables[MONSTER_HP_RATIO] = 100;
    m_Variables[MONSTER_EXP_RATIO] = 100;

    m_Variables[CLOVER_EVENT] = 0;
    m_Variables[CLOVER_RATIO] = 0;

    m_Variables[MONSTER_DAMAGE_RATIO] = 100;
    m_Variables[RACE_PET_QUEST] = 0;

    m_Variables[NICKNAME_PEN_EVENT] = 0;
    m_Variables[NICKNAME_PEN_RATIO] = 0;
    m_Variables[GOLD_MEDAL_RATIO] = 0;
    m_Variables[PET_DAY_EVENT] = 0;

    m_Variables[OLYMPIC_ITEM_RATIO] = 0;
    m_Variables[OLYMPIC_EVENT] = 0;

    m_Variables[CROWN_PRICE] = 0;
    m_Variables[CHOBO_EVENT] = 0;

    m_Variables[QUIT_GUILD_PENALTY_TERM] = 0;
    m_Variables[GUILD_UNION_MAX] = 0;

    m_Variables[LUCK_CHARM_RATIO] = 0;
    m_Variables[GDR_LAIR_PC_LIMIT] = 36;

    m_Variables[DONATION_EVENT_200501] = 0;

    m_Variables[EVENT_NEW_YEAR_2005] = 0;
    m_Variables[BOKJORY_RATIO] = 80000;
    m_Variables[EVENT_RED_RICE_CAKE_SOUP_RATIO] = 1000;
    m_Variables[EVENT_GREEN_RICE_CAKE_SOUP_RATIO] = 1000;
    m_Variables[EVENT_BLUE_RICE_CAKE_SOUP_RATIO] = 1000;
    m_Variables[EVENT_BLACK_RICE_CAKE_SOUP_RATIO] = 80000;
    m_Variables[EVENT_MUGWORT_RICE_CAKE_SOUP_RATIO] = 80000;

    m_Variables[TIME_PERIOD_EXP_2X] = 0;

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////////////
// destructor
//////////////////////////////////////////////////////////////////////////////
VariableManager::~VariableManager()

{
    __BEGIN_TRY
    __END_CATCH_NO_RETHROW
}

void VariableManager::init()

{
    __BEGIN_TRY

    Properties& config = de::kernelContext().config();

    m_VariableNames.clear();

    for (int i = 0; i < VARIABLE_MAX; i++) {
        char str[256];
        strcpy(str, VariableType2String[i].c_str());
        strlwr(str);
        m_VariableNames[str] = (VariableType)i;
    }

    load();


    // Cases where a different initial value is set.
    setVariable(AUTO_START_RACE_WAR, config.getPropertyInt("ActiveRaceWar"));
    setVariable(GUILD_WAR_ACTIVE, config.getPropertyInt("ActiveGuildWar"));
    setVariable(ACTIVE_FLAG_WAR, config.getPropertyInt("ActiveFlagWar"));
    setVariable(ACTIVE_LEVEL_WAR, config.getPropertyInt("ActiveLevelWar"));
    setVariable(HEAD_COUNT, config.getPropertyInt("HeadCount"));
    setVariable(KILL_DAEMONCTL, 0);

    if (!SystemAvailabilitiesManager::getInstance()->isAvailable(SystemAvailabilitiesManager::SYSTEM_HOLY_LAND_WAR))
        setVariable(WAR_ACTIVE, 0);

    if (!SystemAvailabilitiesManager::getInstance()->isAvailable(SystemAvailabilitiesManager::SYSTEM_GUILD_WAR))
        setVariable(GUILD_WAR_ACTIVE, 0);

    if (!SystemAvailabilitiesManager::getInstance()->isAvailable(SystemAvailabilitiesManager::SYSTEM_RACE_WAR))
        setVariable(AUTO_START_RACE_WAR, 0);

    if (!SystemAvailabilitiesManager::getInstance()->isAvailable(SystemAvailabilitiesManager::SYSTEM_FLAG_WAR))
        setVariable(ACTIVE_FLAG_WAR, 0);

    if (!SystemAvailabilitiesManager::getInstance()->isAvailable(SystemAvailabilitiesManager::SYSTEM_MASTER_LAIR))
        setVariable(ACTIVE_MASTER_LAIR, 0);

    __END_CATCH
}


void VariableManager::load()

{
    __BEGIN_TRY

    ContentInfoRepository& repository = defaultContentInfoRepository();

    int maxAttr = 0;
    if (!repository.loadMaxAttrID(maxAttr)) {
        throw Error("VariableManager::load(): cannot obtain the maximum attrID.");
    }

    vector<VariableRow> rows = repository.loadVariables();

    if (rows.empty()) {
        throw Error("VariableManager::load(): no values exist");
    }

    // The constructor's defaults with the stored rows over them; a variable
    // the table has no row for keeps its default. The rows are not written
    // back: each value came from its own row.
    vector<int> defaults(m_Variables.size());
    for (size_t i = 0; i < defaults.size(); i++)
        defaults[i] = m_Variables[i];

    vector<int> values = overlayStoredVariables(defaults, maxAttr, rows);

    // A fresh table rather than a resize of the one in place: the slots are
    // atomics, which a vector cannot move, and this runs once at startup.
    VARIABLE loaded(values.size());
    for (size_t i = 0; i < values.size(); i++)
        loaded[i] = values[i];
    m_Variables.swap(loaded);

    for (size_t r = 0; r < rows.size(); r++) {
        if (rows[r].attrID >= 0 && rows[r].attrID < (int)m_Variables.size())
            de::diagnosticTrace(
                [&](std::ostream& output) { output << toString((VariableType)rows[r].attrID).c_str(); });
    }

    __END_CATCH
}

VariableType VariableManager::getVariableType(const string& variableName) const {
    char str[256];
    strcpy(str, variableName.c_str());
    strlwr(str);

    VARIABLE_NAME::const_iterator itr = m_VariableNames.find(str);

    if (itr == m_VariableNames.end()) {
        return VARIABLE_MAX;
    }

    return itr->second;
}

void VariableManager::setVariable(VariableType vt, const string& strVariable) {
    int value = atoi(strVariable.c_str());

    if (value == 0) {
        if (strcasecmp(strVariable.c_str(), "on") == 0) {
            setVariable(vt, 1);
        } else if (strcasecmp(strVariable.c_str(), "off") == 0) {
            setVariable(vt, 0);
        } else {
            setVariable(vt, 0);
        }
    } else {
        setVariable(vt, value);
    }
}

void VariableManager::setVariable(VariableType vt, int value) {
    try {
        if (!isAcceptedVariableValue(vt, value))
            return;

        m_Variables[vt] = value;

        defaultContentInfoRepository().saveVariable(value, (int)vt);

    } catch (Throwable& t) {
        filelog("variableManagerError.log", "%s", t.toString().c_str());
    }
}

string VariableManager::toString(VariableType vt) const {
    StringStream msg;

    int attr1 = m_Variables[vt];

    switch (vt) {
    case EVENT_ACTIVE:
        msg << "Event ACTIVE: " << attr1;
        break;
    case EVENT_RATIO:
        msg << "Event RATIO: " << attr1;
        break;
    case ITEM_PROBE_RATIO:
        msg << "Item PROBE RATIO:  " << attr1;
        break;
    case EXP_RATIO:
        msg << "EXP RATIO: " << attr1;
        break;
    case COMBAT_BONUS_TIME:
        msg << "COMBAT BONUM TIME: " << attr1;
        break;

    case COMBAT_SLAYER_HP_BONUS_RATIO:
        msg << "COMBAT SLAYER HP BONUS RATIO: " << attr1;
        break;

    case COMBAT_VAMPIRE_HP_BONUS_RATIO:
        msg << "COMBAT VAMPIREE  HP BONUS RATIO: " << attr1;
        break;

    case PREMIUM_EXP_BONUS_PERCENT:
        msg << "PREMIUM EXP BONUS PERCENT: " << attr1;
        break;

    case PREMIUM_ITEM_PROBE_PERCENT:
        msg << "PREMIUM ITEM PROBE PERCENT: " << attr1;
        break;

    case ZONE_GROUP_BALANCING_MINUTE:
        msg << "ZoneGroupBalancing minute: " << attr1 << " minutes";
        break;

    case GAMBLE_ITEM_TYPE_RATIO:
        msg << "GAMBLE_ITEM_TYPE_RATIO: " << attr1;
        break;

    case GAMBLE_ITEM_OPTION_RATIO:
        msg << "GAMBLE_ITEM_OPTION_RATIO: " << attr1;
        break;

    case SUMMON_MOTORCYCLE:
        msg << "SUMMON_MOTORCYCLE: " << (attr1 ? "ON" : "OFF");
        break;

    case ENEMY_LIMIT_TIME:
        msg << "ENEMY_LIMIT_TIME: " << attr1 << " seconds";
        break;

    case COMBAT_SLAYER_DAMAGE_BONUS:
        msg << "COMBAT_SLAYER_DAMAGE_BONUS: " << attr1;
        break;

    case COMBAT_VAMPIRE_DAMAGE_BONUS:
        msg << "COMBAT_VAMPIRE_DAMAGE_BONUS: " << attr1;
        break;

    case RARE_ITEM_RATIO:
        msg << "RARE_ITEM_RATIO: " << attr1;
        break;

    case UNIQUE_ITEM_RATIO:
        msg << "UNIQUE_ITEM_RATIO: " << attr1;
        break;

    case ACTIVE_MASTER_LAIR:
        msg << "ACTIVE_MASTER_LAIR: " << (attr1 ? "ON" : "OFF");
        break;

    case RETRY_MASTER_LAIR:
        msg << "RETRY_MASTER_LAIR: " << (attr1 ? "ON" : "OFF");
        break;


    case HARVEST_FESTIVAL_ITEM_RATIO:
        if (attr1 == 0) {
            msg << "HARVEST_FESTIVAL_ITEM_RATIO: 0";
        } else {
            msg << "HARVEST_FESTIVAL_ITEM_RATIO: 1/" << attr1;
        }
        break;

    case MASTER_BLOOD_DRAIN_START_HP:
        msg << "MASTER_BLOOD_DRAIN_START_HP: " << attr1 << "%";
        break;
    case MASTER_BLOOD_DRAIN_START_BD:
        msg << "MASTER_BLOOD_DRAIN_START_BD: " << attr1 << "%";
        break;
    case MASTER_BLOOD_DRAIN_END_HP:
        msg << "MASTER_BLOOD_DRAIN_END_HP: " << attr1 << "%";
        break;
    case MASTER_BLOOD_DRAIN_END_BD:
        msg << "MASTER_BLOOD_DRAIN_END_BD: " << attr1 << "%";
        break;

    case ACTIVE_CHIEF_MONSTER:
        msg << "ACTIVE_CHIEF_MONSTER: " << (attr1 ? "ON" : "OFF");
        break;
    case CHIEF_MONSTER_RARE_ITEM_RATIO:
        msg << "CHIEF_MONSTER_RARE_ITEM_RATIO: " << attr1 << "%";
        break;

    case NEWBIE_TRANSPORT_TO_GUILD:
        msg << "NEWBIE_TRANSPORT_TO_GUILD: " << (attr1 ? "ON" : "OFF");
        break;

    case FREE_PLAY_SLAYER_DOMAIN_SUM:
        msg << "FREE_PLAY_SLAYER_DOMAIN_SUM: " << attr1;
        break;

    case FREE_PLAY_VAMPIRE_LEVEL:
        msg << "FREE_PLAY_VAMPIRE_LEVEL: " << attr1;
        break;

    case LOG_INCOMING_CONNECTION:
        msg << "LOG_INCOMING_CONNECTION: " << (attr1 ? "ON" : "OFF");
        break;

    case CHRISTMAS_FIRE_CRACKER_RATIO:
        msg << "CHRISTMAS_FIRE_CRACKER_RATIO: " << attr1 << " / 10000 ";
        break;

    case CHRISTMAS_TREE_PART_RATIO:
        msg << "CHRISTMAS_TREE_PART_RATIO: " << attr1 << " / 10000 ";
        break;

    case CHRISTMAS_GIFT_BOX_RATIO:
        msg << "christmas_gift_box_ratio: " << attr1 << " / 10000 ";
        break;

    case CHRISTMAS_TREE_DECAY_TIME:
        msg << "CHRISTMAS_TREE_DECAY_TIME: " << attr1 / 600 << " minutes";
        break;

    case MASTER_LAIR_PLAYER_NUM:
        msg << "MASTER_LAIR_PLAYER_NUM: " << attr1 << " players";
        break;

    case RANK_EXP_GAIN_PERCENT:
        msg << "RANK_EXP_GAIN_PERCENT: " << attr1 << "%";
        break;

    case ITEM_LUCK_PERCENT:
        msg << "ITEM_LUCK_PERCENT: " << attr1 << "%";
        break;

    case LOTTO_ITEM_RATIO:
        msg << "LOTTO_ITEM_RATIO: " << attr1 << " / 10000";
        break;

    case LOTTO_ITEM_BONUS_NUM:
        msg << "LOTTO_ITEM_BONUS_NUM: " << attr1;
        break;

    case LOTTO_SKULL_RATIO:
        msg << "LOTTO_SKULL_RATIO: " << attr1 << " / 10000";
        break;

    case PREMIUM_HALF_EVENT:
        msg << "PREMIUM_HALF_EVENT: " << (attr1 ? "ON" : "OFF");
        break;

    case COMMON_CASTLE_ENTRANCE_FEE:
        msg << "COMMON_CASTLE_ENTRANCE_FEE: " << attr1;
        break;

    case GUILD_CASTLE_ENTRANCE_FEE:
        msg << "GUILD_CASTLE_ENTRANCE_FEE: " << attr1;
        break;

    case COMMON_CASTLE_ITEM_TAX_RATIO:
        msg << "COMMON_CASTLE_ITEM_TAX_RATIO: " << attr1;
        break;

    case GUILD_CASTLE_ITEM_TAX_RATIO:
        msg << "GUILD_CASTLE_ITEM_TAX_RATIO: " << attr1;
        break;

    case WAR_REGISTRATION_FEE:
        msg << "WAR_REGISTRATION_FEE: " << attr1;
        break;

    case GUILD_WAR_TIME:
        msg << "GUILD_WAR_TIME : " << attr1 << " seconds == " << attr1 / 60 << " minutes";
        break;

    case RACE_WAR_TIME:
        msg << "RACE_WAR_TIME :" << attr1 << " seconds == " << attr1 / 60 << " minutes";
        break;

    case WAR_PERIOD_WEEK:
        msg << "WAR_PERIOD_WEEK :" << (attr1 ? "weekly" : "daily");
        break;

    case WAR_ACTIVE:
        msg << "WAR_ACTIVE :" << (attr1 ? "ON" : "OFF");
        break;

    case RACE_WAR_TIMEBAND:
        msg << "RACE_WAR_TIMEBAND:";
        if (attr1 == 0)
            msg << "dawn";
        else if (attr1 == 1)
            msg << "day";
        else if (attr1 == 2)
            msg << "evening";
        else if (attr1 == 3)
            msg << "night";
        else
            msg << "unknown";
        break;

    case AUTO_START_RACE_WAR:
        msg << "AUTO_START_RACE_WAR:" << (attr1 ? "ON" : "OFF");
        break;

    case ACTIVE_RACE_WAR_LIMITER:
        msg << "ACTIVE_RACE_WAR_LIMITER :" << (attr1 ? "ON" : "OFF");
        break;

    case CHIEF_ITEM_BONUS_NUM:
        msg << "CHIEF_ITEM_BONUS_NUM:" << attr1 << " items";
        break;

    case ACTIVE_EVENT_GIVE_ITEM:
        msg << "ACTIVE_EVENT_GIVE_ITEM :" << (attr1 ? "ON" : "OFF");
        break;

    case VAMPIRE_REDISTRIBUTE_ATTR_PRICE:
        msg << "VAMPIRE_REDISTRIBUTE_ATTR_PRICE:" << attr1;
        break;

    case ACTIVATE_COUPLE:
        msg << "ACTIVATE_COUPLE:" << (attr1 ? "ON" : "OFF");
        break;

    case CAN_RECOUPLE:
        msg << "CAN_RECOUPLE:" << (attr1 ? "Y" : "N");
        break;

    case GUILD_WAR_ACTIVE:
        msg << "GUILD_WAR_ACTIVE:" << (attr1 ? "Yes" : "No");
        break;

    case FREE_PLAY_OUSTERS_LEVEL:
        msg << "FREE_PLAY_OUSTERS_LEVEL: " << attr1;
        break;

    case CAN_APPLY_QUEST:
        msg << "CAN_APPLY_QUEST:" << (attr1 ? "Yes" : "No");
        break;

    case SEND_QUEST_INFO:
        msg << "SEND_QUEST_INFO:" << (attr1 ? "Yes" : "No");
        break;

    case CAN_BUY_SHOP:
        msg << "CAN_BUY_SHOP:" << (attr1 ? "Yes" : "No");
        break;
    case PCROOM_EXP_BONUS:
        msg << "PCROOM_EXP_BONUS:" << attr1;
        break;
    case PCROOM_ITEM_RATIO_BONUS:
        msg << "PCROOM_ITEM_RATIO_BONUS:" << attr1;
        break;
    case MONEY_TRACE_LOG_LIMIT:
        msg << "MONEY_TRACE_LOG_LIMIT:" << attr1;
        break;
    case PC_ROOM_LOTTO_EVENT:
        msg << "PC room lottery event: " << attr1;
        break;
    case ACTIVE_FLAG_WAR:
        msg << "Capture-the-flag game running: " << (attr1 ? "yes" : "no");
        break;
    case KILL_DAEMONCTL:
        msg << "DAEMONCTL kill: " << (attr1 ? "yes" : "no");
        break;

    case EVENT_MOON_CARD:
        if (attr1)
            msg << "The moon card event is running.";
        else
            msg << "The moon card event has been stopped.";
        break;

    case FULL_MOON_CARD_RATIO:
        msg << "Full moon card chance: 1 / " << attr1;
        break;

    case HALF_MOON_CARD_RATIO:
        msg << "Half moon card chance: 1 / " << attr1;
        break;

    case NEW_MOON_CARD_RATIO:
        msg << "NEW_MOON_CARD_RATIO:" << attr1;
        break;

    case OLD_MOON_CARD_RATIO:
        msg << "Old moon (blank) card chance: 1 / " << attr1;
        break;

    case ACTIVE_LEVEL_WAR:
        msg << "Per-level war running: " << (attr1 ? "yes" : "no");
        break;

    case RACE_WAR_HP_BONUS:
        msg << "Race war HP bonus: " << attr1;
        break;

    case HEAD_COUNT:
        msg << "Head count: " << (attr1 ? "yes" : "no");
        break;

    case HEAD_PRICE_BONUS:
        msg << "HEAD_PRICE_BONUS:" << attr1;
        break;

    case LEVEL_WAR_ZONE_FREE_ENTER:
        msg << "LEVEL_WAR_ZONE_FREE_ENTER:" << (attr1 ? "yes" : "no");
        break;

    case PET_EXP_RATIO:
        msg << "Pet exp ratio : " << attr1;
        break;

    case EVENT_LUCKY_BAG:
        if (attr1)
            msg << "The lucky bag event is running.";
        else
            msg << "The lucky bag event is not running.";
        break;

    case GREEN_LUCKY_BAG_RATIO:
        msg << "Green lucky bag chance : 1 / " << attr1;
        break;

    case BLUE_LUCKY_BAG_RATIO:
        msg << "Blue lucky bag chance : 1 / " << attr1;
        break;

    case GOLD_LUCKY_BAG_RATIO:
        msg << "Gold lucky bag chance : 1 / " << attr1;
        break;

    case RED_LUCKY_BAG_RATIO:
        msg << "Red lucky bag chance : 1 / " << attr1;
        break;

    case EVENT_GIFT_BOX:
        if (attr1)
            msg << "The gift box exchange event is running.";
        else
            msg << "The gift box exchange event is not running.";
        break;

    case RED_GIFT_BOX_RATIO:
        msg << "Red gift box chance : 1 / " << attr1;
        break;

    case BLUE_GIFT_BOX_RATIO:
        msg << "Blue gift box chance : 1 / " << attr1;
        break;

    case GREEN_GIFT_BOX_RATIO:
        msg << "Green gift box chance : 1 / " << attr1;
        break;

    case YELLOW_GIFT_BOX_RATIO:
        msg << "YELLOW_GIFT_BOX_RATIO:" << attr1;
        break;

    case NETMARBLE_CARD_RATIO:
        msg << "Netmarble card chance : " << attr1 << " / 100000";
        break;

    case NETMARBLE_CARD_EVENT:
        msg << "Netmarble card event : " << ((attr1) ? "Yes" : "No");
        break;

    case PREMIUM_TRIAL_EVENT:
        msg << "Premium trial event : " << ((attr1) ? "Yes" : "No");
        break;

    case PET_FOOD_EVENT:
        msg << "Pet food event : " << ((attr1) ? "Yes" : "No");
        break;

    case HIGHER_PET_FOOD_RATIO:
        msg << "High-grade pet food chance : " << attr1 << "%";
        break;

    case RACE_PET_FOOD_RATIO:
        msg << "RACE_PET_FOOD_RATIO:" << attr1 << "%";
        break;

    case PET_FOOD_RATIO:
        msg << "PET_FOOD_RATIO:" << attr1 << " / 100000";
        break;

    case REVIVAL_SET_RATIO:
        msg << "Revival set chance : " << attr1 << "%";
        break;

    case TODAY_IS_HOLYDAY:
        if (attr1)
            msg << "Today is a national holiday.";
        else
            msg << "Today is not a national holiday.";
        break;

    case SLAYER_HP_RATIO:
        msg << "Slayer HP ratio : " << attr1 << "%";
        break;
    case VAMPIRE_HP_RATIO:
        msg << "Vampire HP ratio : " << attr1 << "%";
        break;
    case OUSTERS_HP_RATIO:
        msg << "Ousters HP ratio : " << attr1 << "%";
        break;
    case MONSTER_HP_RATIO:
        msg << "Monster HP ratio : " << attr1 << "%";
        break;

    case MONSTER_EXP_RATIO:
        msg << "Monster EXP ratio : " << attr1 << "%";
        break;

    case CLOVER_EVENT:
        msg << "Four-leaf clover event : " << ((attr1) ? "on" : "off");
        break;

    case CLOVER_RATIO:
        msg << "Four-leaf clover chance : " << attr1 << "/100000";
        break;

    case MONSTER_DAMAGE_RATIO:
        msg << "Monster damage ratio : " << attr1 << "%";
        break;

    case RACE_PET_QUEST:
        msg << "Second pet quest : " << ((attr1) ? "on" : "off");
        break;

    case NICKNAME_PEN_EVENT:
        msg << "Naming pen event : " << ((attr1) ? "on" : "off");
        break;

    case NICKNAME_PEN_RATIO:
        msg << "Naming pen chance : " << attr1 << "/100000";
        break;

    case GOLD_MEDAL_RATIO:
        msg << "Gold medal chance : " << attr1 << "/100000";
        break;

    case PET_DAY_EVENT:
        msg << "Dog days event : " << ((attr1) ? "on" : "off");
        break;

    case OLYMPIC_ITEM_RATIO:
        msg << "Olympic event item loot chance : " << attr1 << "/100000";
        break;

    case OLYMPIC_EVENT:
        msg << "DarkEden Olympic event : " << ((attr1) ? "on" : "off");
        break;

    case CROWN_PRICE:
        msg << "Laurel crown price : " << attr1;
        break;

    case CHOBO_EVENT:
        msg << "Beginner event : " << ((attr1) ? "on" : "off");
        break;

    case QUIT_GUILD_PENALTY_TERM:
        msg << "Guild rejoin ban after leaving : " << attr1;
        break;

    case GUILD_UNION_MAX:
        msg << "GUILD_UNION_MAX:" << attr1;
        break;

    case LUCK_CHARM_RATIO:
        msg << "LUCK_CHARM_RATIO:" << attr1;
        break;

    case GDR_LAIR_PC_LIMIT:
        msg << "GDR_LAIR_PC_LIMIT:" << attr1;
        break;

    case DONATION_EVENT_200501:
        msg << "DONATION_EVENT_200501:" << ((attr1) ? "on" : "off");
        break;

    case EVENT_NEW_YEAR_2005:
        msg << "EVENT_NEW_YEAR_2005: " << ((attr1) ? "on" : "off");
        break;

    case BOKJORY_RATIO:
        msg << "BOKJORY_RATIO:" << attr1;
        break;

    case EVENT_RED_RICE_CAKE_SOUP_RATIO:
        msg << "EVENT_RED_RICE_CAKE_SOUP_RATIO:" << attr1;
        break;

    case EVENT_GREEN_RICE_CAKE_SOUP_RATIO:
        msg << "EVENT_GREEN_RICE_CAKE_SOUP_RATIO:" << attr1;
        break;

    case EVENT_BLUE_RICE_CAKE_SOUP_RATIO:
        msg << "EVENT_BLUE_RICE_CAKE_SOUP_RATIO:" << attr1;
        break;

    case EVENT_BLACK_RICE_CAKE_SOUP_RATIO:
        msg << "EVENT_BLACK_RICE_CAKE_SOUP_RATIO:" << attr1;
        break;

    case EVENT_MUGWORT_RICE_CAKE_SOUP_RATIO:
        msg << "EVENT_MUGWORT_RICE_CAKE_SOUP_RATIO:" << attr1;
        break;

    case TIME_PERIOD_EXP_2X:
        msg << "TIME_PERIOD_EXP_2X:" << ((attr1) ? "on" : "off");
        break;

    default:
        if (vt < VARIABLE_MAX) // add by viva
            msg << VariableType2String[vt] << ": " << attr1;
        //			msg << VariableType2String[vt] << ": " << attr1;
        break;
    }

    return msg.toString();
}
