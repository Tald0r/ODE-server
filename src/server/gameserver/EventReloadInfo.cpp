//////////////////////////////////////////////////////////////////////////////
// Filename    : EventReloadInfo.cpp
// Written by  : Reiot
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "EventReloadInfo.h"

#include <stdio.h>

#include "Directive.h"
#include "GCSystemMessage.h"
#include "GameContext.h"
#include "GamePlayer.h"
#include "GoodsInfoManager.h"
#include "IncomingPlayerManager.h"
#include "ItemInfoManager.h"
#include "LogNameManager.h"
#include "MasterLairInfoManager.h"
#include "MonsterInfo.h"
#include "OptionInfo.h"
#include "PlayerMailbox.h"
#include "RaceWarLimiter.h"
#include "RankBonusInfo.h"
#include "ShrineInfoManager.h"
#include "StringPool.h"
#include "SweeperBonusManager.h"
#include "WarScheduler.h"
#include "Zone.h"
#include "ZoneGroup.h"
#include "ZoneGroupManager.h"
#include "ZoneInfo.h"
#include "ZoneInfoManager.h"
#include "ZonePlayerManager.h"
#include "ZoneUtil.h"
#include "mission/QuestInfoManager.h"

//////////////////////////////////////////////////////////////////////////////
// class EventReloadInfo member methods
//////////////////////////////////////////////////////////////////////////////

EventReloadInfo::EventReloadInfo(GamePlayer* pGamePlayer, InfoType infoType, int infoValue)

    : Event(pGamePlayer), m_InfoType(infoType), m_InfoValue(infoValue) {}

EventReloadInfo::~EventReloadInfo()

{}

void EventReloadInfo::activate()

{
    __BEGIN_DEBUG

    StringPool& strings = de::gameContext().strings();

    //(m_pGamePlayer != NULL);// may be NULL.

    switch (m_InfoType) {
    case MASTER_LAIR_INFO:
        de::gameContext().masterLairInfos().reload();
        break;

    case MONSTER_INFO: {
        MonsterInfoManager& monsterInfos = de::gameContext().monsterInfos();
        // m_InfoValue is the SpriteType of the Monster to load.
        if (m_InfoValue == 0) {
            // Load them all.
            monsterInfos.reload(0);
        } else {
            // Find the monsters tied to this SpriteType.
            const vector<MonsterType_t>& monsters = monsterInfos.getMonsterTypeBySprite(m_InfoValue);

            vector<MonsterType_t>::const_iterator itr = monsters.begin();

            for (; itr != monsters.end(); itr++) {
                monsterInfos.reload(*itr);
            }
        }
    } break;

    case MONSTER_AI:
        de::gameContext().directiveSets().load();
        break;

    case ZONE_INFO:
        de::gameContext().zoneInfos().load();
        break;

    case ZONE:
        break;

    case ITEM_INFO: {
        // m_InfoValue is the Class of the ItemInfo to load.
        de::gameContext().itemInfos().getInfoManager((Item::ItemClass)m_InfoValue)->reload();
    } break;

    case LOG_USER_INFO:
        LogNameManager::getInstance().init();
        break;

    case RANK_BONUS_INFO: {
        de::gameContext().rankBonuses().load();
    } break;

    case OPTION_INFO: {
        de::gameContext().optionInfos().load();
    } break;

    case STRING_POOL: {
        strings.load();
    } break;

    case WAR_SCHEDULE_INFO: {
        ZoneID_t zoneID = (ZoneID_t)m_InfoValue;

        // The GM who asked is answered by name: the answer is sent from the
        // castle's thread after the reload, and by then a name is all that
        // still means something if the GM has logged out.
        string requester;
        if (m_pGamePlayer != NULL && m_pGamePlayer->getCreature() != NULL)
            requester = m_pGamePlayer->getCreature()->getName();

        // An unknown zone id is thrown as an Error by the lookup.
        Zone* pZone = NULL;
        try {
            pZone = getZoneByZoneID(zoneID);
        } catch (Error&) {
            pZone = NULL;
        }

        ZoneGroup* pZoneGroup = (pZone != NULL) ? pZone->getZoneGroup() : NULL;

        if (pZoneGroup == NULL) {
            if (m_pGamePlayer != NULL) {
                GCSystemMessage gcSystemMessage;
                gcSystemMessage.setMessage(strings.getString(STRID_NO_SUCH_ZONE));
                m_pGamePlayer->sendPacket(&gcSystemMessage);
            }
            break;
        }

        // A castle's scheduler belongs to its zone's group, whose thread runs
        // its heartbeat and executes the wars it hands out, and the reload
        // frees every war the scheduler holds. So the reload is posted to that
        // group and runs under the group mutex, never here on the main thread.
        // The scheduler is looked up there, because a zone reload replaces it;
        // the zone itself is never freed.
        pZoneGroup->post([pZone, zoneID, requester] {
            StringPool& strings = de::gameContext().strings();
            string message;

            WarScheduler* pWarScheduler = pZone->getWarScheduler();
            if (pWarScheduler != NULL) {
                pWarScheduler->load();

                char msg[100];
                snprintf(msg, sizeof(msg), strings.c_str(STRID_WAR_SCHEDULE_INFO), (int)zoneID,
                         pWarScheduler->getSize());
                message = msg;
            } else {
                message = strings.getString(STRID_THIS_ZONE_IS_NOT_CASTLE);
            }

            if (!requester.empty()) {
                de::postToPlayer(
                    requester,
                    [message](PlayerCreature&, Player& player) {
                        GCSystemMessage gcSystemMessage;
                        gcSystemMessage.setMessage(message);
                        player.sendPacket(&gcSystemMessage);
                    },
                    nullptr, de::Scope::Player);
            }
        });
    } break;

    case BLOOD_BIBLE_OWNER: {
        de::gameContext().shrines().reloadOwner();

        if (m_pGamePlayer != NULL) {
            GCSystemMessage gcSystemMessage;
            gcSystemMessage.setMessage(strings.getString(STRID_LOAD_BLOOD_BIBLE_OWNER_INFO));
            m_pGamePlayer->sendPacket(&gcSystemMessage);
        }
    } break;

    case RACE_WAR_PC_LIMIT:
        RaceWarLimiter::getInstance()->load();
        break;

    case GOODS_LIST_INFO:
        de::gameContext().goodsInfos().load();
        break;

    case SWEEPER_OWNER:
        de::gameContext().sweeperBonuses().reloadOwner(m_InfoValue);
        break;

    default:
        break;
    }

    __END_DEBUG
}

string EventReloadInfo::toString() const

{
    StringStream msg;
    msg << "EventReloadInfo(" << ")";
    return msg.toString();
}
