////////////////////////////////////////////////////////////////////////////////
// Filename    : MasterLairManager.h
// Description :
////////////////////////////////////////////////////////////////////////////////


#include "MasterLairManager.h"

#include <stdio.h>

#include "Assert.h"
#include "EffectContinualGroundAttack.h"
#include "EffectMasterLairPass.h"
#include "GCAddEffect.h"
#include "GCCreateItem.h"
#include "GCNoticeEvent.h"
#include "GCSay.h"
#include "GCSystemMessage.h"
#include "GameContext.h"
#include "Inventory.h"
#include "Item.h"
#include "ItemFactoryManager.h"
#include "ItemUtil.h"
#include "MasterLairInfoManager.h"
#include "Monster.h"
#include "MonsterCorpse.h"
#include "MonsterManager.h"
#include "PCManager.h"
#include "PacketUtil.h"
#include "Player.h"
#include "PlayerCreature.h"
#include "StringPool.h"
#include "Timeval.h"
#include "VariableManager.h"
#include "Zone.h"
#include "ZoneGroupManager.h"
#include "ZoneInfoManager.h"

////////////////////////////////////////////////////////////////////////////////
//
// constructor
//
////////////////////////////////////////////////////////////////////////////////
MasterLairManager::MasterLairManager(Zone* pZone)

{
    __BEGIN_TRY

    Assert(pZone != NULL);
    m_pZone = pZone;

    MasterLairInfo* pInfo = de::gameContext().masterLairInfos().getMasterLairInfo(m_pZone->getZoneID());
    Assert(pInfo != NULL);

    m_MasterID = 0; // a single master
    m_MasterX = 0;
    m_MasterY = 0;

    m_bMasterReady = false; // is the master ready to fight?

    // m_nMaxSummonMonster = pInfo->getMaxSummonMonster(); // max monsters the master summons
    // m_nSummonedMonster = 0;

    m_nMaxPassPlayer = pInfo->getMaxPassPlayer(); // maximum number of players allowed in
    m_nPassPlayer = 0;

    m_Event = EVENT_WAITING_REGEN;
    m_EventValue = 0;

    Timeval currentTime;
    getCurrentTime(currentTime);

    // Has no meaning.
    m_EventTime.tv_sec = currentTime.tv_sec + pInfo->getFirstRegenDelay();
    m_EventTime.tv_usec = 0;

    m_RegenTime.tv_sec = currentTime.tv_sec + pInfo->getFirstRegenDelay();
    m_RegenTime.tv_usec = 0;

    m_Mutex.setName("MasterLairManager");

    // cout << "Init MasterLairManager: zoneID=" << (int)m_pZone->getZoneID() << endl;

    __END_CATCH
}

////////////////////////////////////////////////////////////////////////////////
//
// destructor
//
////////////////////////////////////////////////////////////////////////////////
MasterLairManager::~MasterLairManager()

{
    __BEGIN_TRY

    __END_CATCH_NO_RETHROW
}

////////////////////////////////////////////////////////////////////////////////
//
// enterCreature ( Creature* )
//
////////////////////////////////////////////////////////////////////////////////
//
// Check whether a creature may enter this zone (MasterLair) and, if it may,
// record it as having entered.
//
// [Conditions]
//   - EVENT_WAITING_PLAYER,
//     EVENT_MINION_COMBAT,
//     EVENT_MASTER_COMBAT are the only states in which entry is possible.
//   - A creature holding an EffectMasterLairPass for this MasterLair always enters.
//   - Entry is refused unless the state is EVENT_WAITING_PLAYER.
//   - With no master, entry is refused when m_nPassPlayer >= m_nMaxPassPlayer.
//
// For a character allowed in that has no EffectMasterLairPass:
//   - m_nPassPlayer is increased by 1 and an EffectMasterLairPass is attached.
//   - The EffectMasterLairPass lasts until EVENT_MASTER_COMBAT ends.
//
////////////////////////////////////////////////////////////////////////////////
bool MasterLairManager::enterCreature(Creature* pCreature)

{
    __BEGIN_TRY

    __ENTER_CRITICAL_SECTION(m_Mutex)

    if (pCreature->isDM() || pCreature->isGOD()) {
        goto ENTER_OK;
    }

    if (m_Event != EVENT_WAITING_PLAYER && m_Event != EVENT_MINION_COMBAT && m_Event != EVENT_MASTER_COMBAT) {
        // cout << "[" << (int)m_pZone->getZoneID() << "] MasterLairManager: cannot enter now" << endl;
        return false;
    }

    EffectMasterLairPass* pPassEffect = NULL;

    // Does the creature hold an EffectMasterLairPass for the current zone?
    if (pCreature->isFlag(Effect::EFFECT_CLASS_MASTER_LAIR_PASS)) {
        if (de::gameContext().variables().isRetryMasterLair()) {
            Effect* pEffect = pCreature->getEffectManager()->findEffect(Effect::EFFECT_CLASS_MASTER_LAIR_PASS);
            Assert(pEffect != NULL);

            pPassEffect = dynamic_cast<EffectMasterLairPass*>(pEffect);

            if (pPassEffect->getZoneID() == m_pZone->getZoneID()) {
                // cout << "[" << (int)m_pZone->getZoneID() << "] MasterLairManager: " << pCreature->getName().c_str()
                // << " has EffectPass" << endl;

                goto ENTER_OK;
            }

            // The pass belongs to a different lair.
            // cout << "[" << (int)m_pZone->getZoneID() << "] MMasterLairManager: " << pCreature->getName().c_str() << "
            // has Wrong EffectPass" << endl;
        } else {
            // cout << "[" << (int)m_pZone->getZoneID() << "] MMasterLairManager: " << pCreature->getName().c_str() << "
            // can't enter more" << endl;

            return false;
        }
    }

    // Entry is refused.
    if (m_Event != EVENT_WAITING_PLAYER) {
        // cout << "[" << (int)m_pZone->getZoneID() << "] MasterLairManager: Not WAITING_PLAYER: "
        //	<< m_pZone->getPCManager()->getSize() << " / " << m_nPassPlayer << "/" << m_nMaxPassPlayer << endl;

        return false;
    }

    // if (m_nPassPlayer >= m_nMaxPassPlayer)
    if (m_nPassPlayer >= de::gameContext().variables().getVariable(MASTER_LAIR_PLAYER_NUM)) // by sigi. 2002.12.31
    {
        // cout << "[" << (int)m_pZone->getZoneID() << "] MasterLairManager: Already Maximum Players: "
        //<< m_pZone->getPCManager()->getSize() << " / " << m_nPassPlayer << "/" << m_nMaxPassPlayer << endl;

        return false;
    }

    // Entry is allowed.
    m_nPassPlayer++;

    if (pPassEffect == NULL) {
        pPassEffect = new EffectMasterLairPass(pCreature, m_pZone->getZoneID());

        // cout << "[" << (int)m_pZone->getZoneID() << "] MasterLairManager: " << pCreature->getName().c_str() << "
        // received EffectPass: "
        //	<< m_pZone->getPCManager()->getSize() << " / " << m_nPassPlayer << "/" << m_nMaxPassPlayer << endl;
    } else {
        pPassEffect->setZoneID(m_pZone->getZoneID());
    }

    pCreature->getEffectManager()->addEffect(pPassEffect);
    pCreature->setFlag(Effect::EFFECT_CLASS_MASTER_LAIR_PASS);

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    __END_CATCH

ENTER_OK:

    if (m_Event == EVENT_MINION_COMBAT || m_Event == EVENT_MASTER_COMBAT) {
        Timeval currentTime;
        getCurrentTime(currentTime);

        int timeGap = m_EventTime.tv_sec - currentTime.tv_sec;

        GCNoticeEvent gcNoticeEvent;
        gcNoticeEvent.setCode(NOTICE_EVENT_MASTER_COMBAT_TIME);
        gcNoticeEvent.setParameter(timeGap);
        // m_pZone->broadcastPacket( &gcNoticeEvent );
        pCreature->getPlayer()->sendPacket(&gcNoticeEvent);
    }

    return true;
}

////////////////////////////////////////////////////////////////////////////////
//
// leaveCreature ( Creature* )
//
////////////////////////////////////////////////////////////////////////////////
//
// In the WaitingPlayer state, decrease PassPlayer by one.
//
// If re-entering the master lair is not allowed, remove the EffectPass.
//
////////////////////////////////////////////////////////////////////////////////
bool MasterLairManager::leaveCreature(Creature* pCreature)

{
    __BEGIN_TRY

    __ENTER_CRITICAL_SECTION(m_Mutex)

    if (pCreature->isDM() || pCreature->isGOD()) {
        return true;
    }

    // The count is decreased only in the waiting player state.
    if (m_Event == EVENT_WAITING_PLAYER) {
        if (m_nPassPlayer > 0)
            m_nPassPlayer--;
    }

    // When re-entry after leaving (or dying) is not allowed, remove the
    // EffectMasterLairPass on the way out.
    if (!de::gameContext().variables().isRetryMasterLair()) {
        if (pCreature->isFlag(Effect::EFFECT_CLASS_MASTER_LAIR_PASS)) {
            pCreature->getEffectManager()->deleteEffect(Effect::EFFECT_CLASS_MASTER_LAIR_PASS);
            pCreature->removeFlag(Effect::EFFECT_CLASS_MASTER_LAIR_PASS);
        }
    }

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    __END_CATCH

    // cout << "[" << (int)m_pZone->getZoneID() << "] MasterLairManager: " << pCreature->getName().c_str() << " leaved:
    // "
    //		<< m_pZone->getPCManager()->getSize() << " / " << m_nPassPlayer << "/" << m_nMaxPassPlayer << endl;
    return true;
}


////////////////////////////////////////////////////////////////////////////////
//
// heartbeat
//
////////////////////////////////////////////////////////////////////////////////
bool MasterLairManager::heartbeat()

{
    __BEGIN_TRY

    __ENTER_CRITICAL_SECTION(m_Mutex)

    switch (m_Event) {
    case EVENT_WAITING_PLAYER:
        processEventWaitingPlayer();
        break;

    case EVENT_MINION_COMBAT:
        processEventMinionCombat();
        break;

    case EVENT_MASTER_COMBAT:
        processEventMasterCombat();
        break;

    case EVENT_WAITING_KICK_OUT:
        processEventWaitingKickOut();
        break;

    case EVENT_WAITING_REGEN:
        processEventWaitingRegen();
        break;

    default:
        break;
    };

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    __END_CATCH

    return true;
}

////////////////////////////////////////////////////////////////////////////////
//
// process EventWaitingPlayer
//
////////////////////////////////////////////////////////////////////////////////
void MasterLairManager::processEventWaitingPlayer()

{
    __BEGIN_TRY

    Timeval currentTime;
    getCurrentTime(currentTime);

    // When the waiting time is over,
    // the master starts summoning monsters.
    if (currentTime >= m_EventTime) {
        // Tell everyone the master lair is closed.

        GCNoticeEvent gcNoticeEvent;

        gcNoticeEvent.setCode(NOTICE_EVENT_MASTER_LAIR_CLOSED);
        gcNoticeEvent.setParameter(m_pZone->getZoneID());

        de::gameContext().zoneGroups().broadcast(&gcNoticeEvent);

        // Start the fight with the minions.
        activeEventMinionCombat();
    } else {
        int remainSec = m_EventTime.tv_sec - currentTime.tv_sec;

        // Announce once a minute.
        if (remainSec != m_EventValue && remainSec != 0 && remainSec % 60 == 0) {
            // Tell everyone how many minutes are left before the master lair closes.

            GCNoticeEvent gcNoticeEvent;

            gcNoticeEvent.setCode(NOTICE_EVENT_MASTER_LAIR_COUNT);

            int remainMin = remainSec / 60;
            uint param = (remainMin << 16) | ((int)m_pZone->getZoneID());
            gcNoticeEvent.setParameter(param);

            de::gameContext().zoneGroups().broadcast(&gcNoticeEvent);


            m_EventValue = remainSec;
        }
    }


    __END_CATCH
}

////////////////////////////////////////////////////////////////////////////////
//
// process EventWaitingPlayer
//
////////////////////////////////////////////////////////////////////////////////
void MasterLairManager::processEventMinionCombat()

{
    __BEGIN_TRY

    Timeval currentTime;
    getCurrentTime(currentTime);

    // When the waiting time is over,
    // not enough monsters were killed, so
    // the players are forcibly kicked out.
    if (currentTime >= m_EventTime) {
        GCNoticeEvent gcNoticeEvent;
        gcNoticeEvent.setCode(NOTICE_EVENT_MASTER_COMBAT_END);
        m_pZone->broadcastPacket(&gcNoticeEvent);

        activeEventWaitingKickOut();
    }

    // When every summoned monster is dead,
    // the master comes out and fights.
    // if (m_nSummonedMonster >= m_nMaxSummonMonster
    if (m_bMasterReady
        // Only the master is left in the zone.
        && m_pZone->getMonsterManager()->getSize() == 1) {
        activeEventMasterCombat();
    }

    // Every player is dead.
    if (m_pZone->getPCManager()->getSize() == 0) {
        activeEventWaitingRegen();
    }

    __END_CATCH
}

////////////////////////////////////////////////////////////////////////////////
//
// process EventWaitingPlayer
//
////////////////////////////////////////////////////////////////////////////////
void MasterLairManager::processEventMasterCombat()

{
    __BEGIN_TRY

    Timeval currentTime;
    getCurrentTime(currentTime);

    Creature* pMaster = m_pZone->getMonsterManager()->getCreature(m_MasterID);

    if (pMaster == NULL) {
        // The master is missing.
        StringStream msg;
        msg << "The master is gone. zoneID = " << (int)m_pZone->getZoneID();

        filelog("masterLairBug.txt", "%s", msg.toString().c_str());

        // throw Error(msg.toString());
    } else {
        // The master's current position.
        m_MasterX = pMaster->getX();
        m_MasterY = pMaster->getY();
    }

    // When the master is dead or
    // the waiting time is over,
    // switch to the forced kick-out mode.
    if (pMaster == NULL || pMaster->isDead()) {
        killAllMonsters();
        giveKillingReward();
        activeEventWaitingKickOut();
    }

    else if (currentTime >= m_EventTime) {
        activeEventWaitingKickOut();
    }

    // Every player is dead.
    if (m_pZone->getPCManager()->getSize() == 0) {
        activeEventWaitingRegen();
    }

    __END_CATCH
}

////////////////////////////////////////////////////////////////////////////////
//
// process EventWaitingPlayer
//
////////////////////////////////////////////////////////////////////////////////
void MasterLairManager::processEventWaitingKickOut()

{
    __BEGIN_TRY

    Timeval currentTime;
    getCurrentTime(currentTime);

    // When the waiting time is over,
    //   kick the players out and
    //   wait for the regen.
    if (currentTime >= m_EventTime) {
        kickOutPlayers();
        activeEventWaitingRegen();
    }

    __END_CATCH
}
////////////////////////////////////////////////////////////////////////////////
//
// process EventWaitingPlayer
//
////////////////////////////////////////////////////////////////////////////////
void MasterLairManager::processEventWaitingRegen()

{
    __BEGIN_TRY

    Timeval currentTime;
    getCurrentTime(currentTime);

    // When the regen time arrives,
    //   wait for players.
    if (currentTime >= m_RegenTime) {
        if (de::gameContext().variables().isActiveMasterLair()) {
            activeEventWaitingPlayer();
        } else {
            // Otherwise wait until the next regen time.
            MasterLairInfo* pInfo = de::gameContext().masterLairInfos().getMasterLairInfo(m_pZone->getZoneID());
            Assert(pInfo != NULL);

            m_RegenTime.tv_sec += pInfo->getRegenDelay();
        }
    }

    __END_CATCH
}

////////////////////////////////////////////////////////////////////////////////
//
// active EventWaitingPlayer
//
////////////////////////////////////////////////////////////////////////////////
void MasterLairManager::activeEventWaitingPlayer()

{
    __BEGIN_TRY

    MasterLairInfo* pInfo = de::gameContext().masterLairInfos().getMasterLairInfo(m_pZone->getZoneID());
    Assert(pInfo != NULL);

    deleteAllMonsters();

    m_bMasterReady = false;
    // m_nSummonedMonster = 0;

    m_nPassPlayer = 0;

    // Five-minute waiting time.
    getCurrentTime(m_RegenTime);
    m_EventTime.tv_sec = m_RegenTime.tv_sec + pInfo->getStartDelay();
    m_EventTime.tv_usec = m_RegenTime.tv_usec;
    m_EventValue = 0;

    // Flames keep rising from the ground,
    // every three seconds.
    int lairAttackTick = pInfo->getLairAttackTick();
    int lairAttackMinNumber = pInfo->getLairAttackMinNumber();
    int lairAttackMaxNumber = pInfo->getLairAttackMaxNumber();

    // cout << "EffectCon: " << (int)m_pZone->getZoneID() << ", " << lairAttackTick << ", " << lairAttackMinNumber << ",
    // " << lairAttackMaxNumber << endl;

    if (lairAttackMinNumber > 0 && lairAttackMaxNumber > 0) {
        // Delete every existing attack effect.
        for (int i = 0; i < 10; i++) // guard against an infinite loop
        {
            Effect* pOldEffect = m_pZone->findEffect(Effect::EFFECT_CLASS_CONTINUAL_GROUND_ATTACK);
            if (pOldEffect == NULL)
                break;
            m_pZone->deleteEffect(pOldEffect->getObjectID());
        }

        EffectContinualGroundAttack* pEffect =
            new EffectContinualGroundAttack(m_pZone, Effect::EFFECT_CLASS_GROUND_ATTACK, lairAttackTick);
        // EffectContinualGroundAttack* pEffect = new EffectContinualGroundAttack(m_pZone,
        // Effect::EFFECT_CLASS_METEOR_STRIKE, lairAttackTick);
        pEffect->setDeadline(pInfo->getStartDelay() * 10);
        pEffect->setNumber(lairAttackMinNumber, lairAttackMaxNumber);

        ObjectRegistry& objectregister = m_pZone->getObjectRegistry();
        objectregister.registerObject(pEffect);

        // Add the effect to the zone.
        m_pZone->addEffect(pEffect);

        // Pillar of fire.
        GCNoticeEvent gcNoticeEvent;
        gcNoticeEvent.setCode(NOTICE_EVENT_CONTINUAL_GROUND_ATTACK);
        gcNoticeEvent.setParameter(pInfo->getStartDelay()); // seconds

        m_pZone->broadcastPacket(&gcNoticeEvent);
    }

    // Tell everyone the master lair has opened.

    GCNoticeEvent gcNoticeEvent;

    gcNoticeEvent.setCode(NOTICE_EVENT_MASTER_LAIR_OPEN);
    gcNoticeEvent.setParameter(m_pZone->getZoneID());

    de::gameContext().zoneGroups().broadcast(&gcNoticeEvent);

    // Set the next regen time.
    m_RegenTime.tv_sec += pInfo->getRegenDelay();

    m_Event = EVENT_WAITING_PLAYER;

    // cout << "[" << (int)m_pZone->getZoneID() << "] MasterLairManager::activeEventWaitingPlayer" << endl;

    __END_CATCH
}

////////////////////////////////////////////////////////////////////////////////
//
// active EventWaitingPlayer
//
////////////////////////////////////////////////////////////////////////////////
void MasterLairManager::activeEventMinionCombat()

{
    __BEGIN_TRY

    MasterLairInfo* pInfo = de::gameContext().masterLairInfos().getMasterLairInfo(m_pZone->getZoneID());
    Assert(pInfo != NULL);

    // Signal that the pillar of fire has ended.
    GCNoticeEvent gcNoticeEvent;
    gcNoticeEvent.setCode(NOTICE_EVENT_CONTINUAL_GROUND_ATTACK_END);
    m_pZone->broadcastPacket(&gcNoticeEvent);

    gcNoticeEvent.setCode(NOTICE_EVENT_MASTER_COMBAT_TIME);
    gcNoticeEvent.setParameter(pInfo->getEndDelay());
    m_pZone->broadcastPacket(&gcNoticeEvent);


    // Remove from the tile without sending a packet.
    deleteAllMonsters();

    // Create the master.
    Monster* pMaster = new Monster(pInfo->getMasterNotReadyMonsterType());
    Assert(pMaster != NULL);

    // No items come out of the corpse.
    pMaster->setTreasure(false);

    // Set the invulnerable state.
    pMaster->setFlag(Effect::EFFECT_CLASS_NO_DAMAGE);

    // Once the master exists it summons
    // monsters on its own.

    try {
        m_pZone->addCreature(pMaster, pInfo->getMasterX(), pInfo->getMasterY(), pInfo->getMasterDir());

        // Remember the ObjectID and read the master through it.
        m_MasterID = pMaster->getObjectID();
    } catch (EmptyTileNotExistException&) {
        // There is no free tile for the master.
        SAFE_DELETE(pMaster);
    }

    // m_nSummonedMonster = 0;  // number of monsters the master summoned

    m_Event = EVENT_MINION_COMBAT;
    m_EventValue = 0;

    // How long the fight lasts.
    getCurrentTime(m_EventTime);
    m_EventTime.tv_sec += pInfo->getEndDelay();

    // cout << "[" << (int)m_pZone->getZoneID() << "] MasterLairManager::activeEventMinionCombat" << endl;

    __END_CATCH
}

////////////////////////////////////////////////////////////////////////////////
//
// active EventWaitingPlayer
//
////////////////////////////////////////////////////////////////////////////////
void MasterLairManager::activeEventMasterCombat()

{
    __BEGIN_TRY

    Creature* pMaster = m_pZone->getMonsterManager()->getCreature(m_MasterID);
    // Master-specific hardcoding can go here.

    if (pMaster != NULL) {
        MasterLairInfo* pInfo = de::gameContext().masterLairInfos().getMasterLairInfo(m_pZone->getZoneID());
        Assert(pInfo != NULL);

        Monster* pMasterMonster = dynamic_cast<Monster*>(pMaster);

        // Replace the summoning-stage master with the master monster that fights.
        if (pInfo->getMasterMonsterType() != pMasterMonster->getMonsterType()) {
            // Create the master.
            Monster* pNewMaster = new Monster(pInfo->getMasterMonsterType());
            Assert(pNewMaster != NULL);

            // No items come out of the corpse.
            pNewMaster->setTreasure(false);

            try {
                m_pZone->addCreature(pNewMaster, pInfo->getSummonX(), pInfo->getSummonY(), pMaster->getDir());

                // Remember the ObjectID and read the master through it.
                m_MasterID = pNewMaster->getObjectID();
            } catch (EmptyTileNotExistException&) {
                m_MasterID = 0;

                // There is no free tile for the master.
                SAFE_DELETE(pNewMaster);
            }

            // The NotReady master is left in place.
            if (pInfo->isMasterRemainNotReady()) {
                ZoneCoord_t cx = pMasterMonster->getX();
                ZoneCoord_t cy = pMasterMonster->getY();

                // Broadcast the effect that lays it on the ground first.
                GCAddEffect gcAddEffect;
                gcAddEffect.setObjectID(pMasterMonster->getObjectID());
                gcAddEffect.setEffectID(Effect::EFFECT_CLASS_COMA);
                gcAddEffect.setDuration(0);
                m_pZone->broadcastPacket(cx, cy, &gcAddEffect);

                // Only the AI is removed; the monster stays.
                pMasterMonster->removeBrain();

            } else {
                m_pZone->deleteCreature(pMaster, pMaster->getX(), pMaster->getY());

                SAFE_DELETE(pMaster);
            }
        } else {
            // Clear the invulnerable state.
            // When the original master is not replaced and fights directly it was
            // invulnerable until now, so the flag must be cleared. If a new master
            // fights instead, the original master must stay NO_DAMAGE. In Tepez lair
            // the original master is the Tepez casket; clearing this too early let
            // players hit the casket for experience.
            pMaster->removeFlag(Effect::EFFECT_CLASS_NO_DAMAGE);
        }
    }

    m_Event = EVENT_MASTER_COMBAT;
    m_EventValue = 0;

    // cout << "[" << (int)m_pZone->getZoneID() << "[ MasterLairManager::activeEventMasterCombat" << endl;

    __END_CATCH
}

////////////////////////////////////////////////////////////////////////////////
//
// active EventWaitingPlayer
//
////////////////////////////////////////////////////////////////////////////////
void MasterLairManager::activeEventWaitingKickOut()

{
    __BEGIN_TRY

    MasterLairInfo* pInfo = de::gameContext().masterLairInfos().getMasterLairInfo(m_pZone->getZoneID());
    Assert(pInfo != NULL);

    // Print a message if the master is not dead.
    Creature* pMaster = m_pZone->getMonsterManager()->getCreature(m_MasterID);

    if (pMaster != NULL && pMaster->isAlive()) {
        GCSay gcSay;
        gcSay.setObjectID(pMaster->getObjectID());
        gcSay.setColor(MASTER_SAY_COLOR);
        gcSay.setMessage(pInfo->getRandomMasterNotDeadSay());
        if (!gcSay.getMessage().empty())
            m_pZone->broadcastPacket(pMaster->getX(), pMaster->getY(), &gcSay);
    }

    // Time given to move the players inside back out.
    m_Event = EVENT_WAITING_KICK_OUT;
    m_EventValue = 0;

    getCurrentTime(m_EventTime);
    m_EventTime.tv_sec += pInfo->getKickOutDelay();

    // Send the end time to the users in the lair.
    GCNoticeEvent gcNoticeEvent;
    gcNoticeEvent.setCode(NOTICE_EVENT_KICK_OUT_FROM_ZONE);
    gcNoticeEvent.setParameter(pInfo->getKickOutDelay());

    m_pZone->broadcastPacket(&gcNoticeEvent);

    // cout << "[" << (int)m_pZone->getZoneID() << "] MasterLairManager::activeEventKickOut" << endl;

    __END_CATCH
}

////////////////////////////////////////////////////////////////////////////////
//
// active EventWaitingPlayer
//
////////////////////////////////////////////////////////////////////////////////
void MasterLairManager::activeEventWaitingRegen()

{
    __BEGIN_TRY

    deleteAllMonsters();

    // Turn off EffectContinualGroundAttack.

    // m_nSummonedMonster = 0;
    m_nPassPlayer = 0;
    m_Event = EVENT_WAITING_REGEN;
    m_EventValue = 0;

    m_bMasterReady = false;

    // cout << "[" << (int)m_pZone->getZoneID() << "] MasterLairManager::activeEventWaitingRegen" << endl;


    __END_CATCH
}

////////////////////////////////////////////////////////////////////////////////
//
// delete All Monsters
//
////////////////////////////////////////////////////////////////////////////////
void MasterLairManager::deleteAllMonsters()

{
    __BEGIN_TRY

    // Remove it from the zone's MonsterManager, then delete it.
    // m_pZone->getMonsterManager()->deleteCreature( m_pMaster->getObjectID() );
    // SAFE_DELETE(m_pMaster);
    bool bDeleteFromZone = true;
    m_pZone->getMonsterManager()->deleteAllMonsters(bDeleteFromZone);

    m_MasterID = 0;
    m_MasterX = 0;
    m_MasterY = 0;

    __END_CATCH
}

////////////////////////////////////////////////////////////////////////////////
//
// kill All Monsters
//
////////////////////////////////////////////////////////////////////////////////
void MasterLairManager::killAllMonsters()

{
    __BEGIN_TRY

    // Removed because something seems wrong with this part.

    __END_CATCH
}
////////////////////////////////////////////////////////////////////////////////
//
// increase SummonedMonster Number
//
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
//
// start Event
//
////////////////////////////////////////////////////////////////////////////////
void MasterLairManager::startEvent()

{
    __BEGIN_TRY

    activeEventWaitingPlayer();

    __END_CATCH
}

////////////////////////////////////////////////////////////////////////////////
//
// start Event
//
////////////////////////////////////////////////////////////////////////////////
void MasterLairManager::stopEvent()

{
    __BEGIN_TRY

    kickOutPlayers();
    activeEventWaitingRegen();

    __END_CATCH
}

////////////////////////////////////////////////////////////////////////////////
//
// kickOut Players
//
////////////////////////////////////////////////////////////////////////////////
void MasterLairManager::kickOutPlayers()

{
    __BEGIN_TRY

    MasterLairInfo* pInfo = de::gameContext().masterLairInfos().getMasterLairInfo(m_pZone->getZoneID());
    Assert(pInfo != NULL);


    // After the kick-out time, a meteor attack.
    int lairAttackTick = pInfo->getLairAttackTick();
    int lairAttackMinNumber = pInfo->getLairAttackMinNumber();
    int lairAttackMaxNumber = pInfo->getLairAttackMaxNumber();

    EffectContinualGroundAttack* pEffect =
        new EffectContinualGroundAttack(m_pZone, Effect::EFFECT_CLASS_METEOR_STRIKE, lairAttackTick);
    pEffect->setDeadline(pInfo->getStartDelay() * 10);
    pEffect->setNumber(lairAttackMinNumber, lairAttackMaxNumber);

    ObjectRegistry& objectregister = m_pZone->getObjectRegistry();
    objectregister.registerObject(pEffect);

    // Add the effect to the zone.
    m_pZone->addEffect(pEffect);

    // Meteor attack.
    GCNoticeEvent gcNoticeEvent;
    gcNoticeEvent.setCode(NOTICE_EVENT_CONTINUAL_GROUND_ATTACK);
    gcNoticeEvent.setParameter(pInfo->getStartDelay()); // seconds

    m_pZone->broadcastPacket(&gcNoticeEvent);

    __END_CATCH
}

////////////////////////////////////////////////////////////////////////////////
//
// give Killing Reward
//
////////////////////////////////////////////////////////////////////////////////
// Reward for killing the master.
// A QuestItem is put into the inventory of each player in the zone.
// If there is no room in the inventory it is dropped on the ground, where a
// player who already holds one cannot pick it up.
////////////////////////////////////////////////////////////////////////////////
void MasterLairManager::giveKillingReward()

{
    __BEGIN_TRY

    const PCManager* pPCManager = m_pZone->getPCManager();
    const unordered_map<ObjectID_t, Creature*>& creatures = pPCManager->getCreatures();
    unordered_map<ObjectID_t, Creature*>::const_iterator itr;

    if (creatures.empty())
        return;

    int goodOneIndex = rand() % creatures.size(); // who gets the pendant

    ItemType_t itemType;
    int i;
    for (i = 0, itr = creatures.begin(); itr != creatures.end(); i++, itr++) {
        Creature* pCreature = itr->second;

        if (pCreature->isPC()) {
            PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pCreature);
            Inventory* pInventory = pPC->getInventory();

            //------------------------------------------------------------
            // Raise the rank experience.
            //------------------------------------------------------------
            // Only within 7 tiles of the master's position.
            //
            if (pPC->getDistance(m_MasterX, m_MasterY) <= 7) {
                pPC->increaseRankExp(MASTER_KILL_RANK_EXP);
            }

            //------------------------------------------------------------
            // Create the reward item.
            //------------------------------------------------------------
            // Hardcoded.
            switch (m_pZone->getZoneID()) {
            // Bathory lair and its clone.
            case 1104:
            case 1106:
                itemType = ((goodOneIndex == i) ? 1 : 0);
                break;

            // Tepez lair and its clone.
            case 1114:
            case 1115:
                itemType = ((goodOneIndex == i) ? 3 : 2);
                break;

            default:
                filelog("MasterLairBUG.txt", "Invalid ZoneID");
                return;
            }

            list<OptionType_t> nullList;
            Item* pItem = de::gameContext().itemFactories().createItem(Item::ITEM_CLASS_QUEST_ITEM, itemType, nullList);

            (m_pZone->getObjectRegistry()).registerObject(pItem);

            // Find an empty slot in the inventory.
            _TPOINT p;
            if (pInventory->getEmptySlot(pItem, p)) {
                // Add it to the inventory.
                pInventory->addItem(p.x, p.y, pItem);

                pItem->create(pCreature->getName(), STORAGE_INVENTORY, 0, p.x, p.y);

                // Write an entry to the ItemTrace log.
                if (pItem != NULL && pItem->isTraceItem()) {
                    remainTraceLog(pItem, "LairMaster", pCreature->getName(), ITEM_LOG_CREATE, DETAIL_EVENTNPC);
                    remainTraceLogNew(pItem, pCreature->getName(), ITL_GET, ITLD_EVENTNPC, m_pZone->getZoneID());
                }

                // Send the inventory item creation packet.
                GCCreateItem gcCreateItem;

                makeGCCreateItem(&gcCreateItem, pItem, p.x, p.y);

                pCreature->getPlayer()->sendPacket(&gcCreateItem);
            } else {
                // No room in the inventory, so drop it on the ground.

                TPOINT p = m_pZone->addItem(pItem, pCreature->getX(), pCreature->getY());
                if (p.x != -1) {
                    pItem->create("", STORAGE_ZONE, m_pZone->getZoneID(), p.x, p.y);

                    // Write an entry to the ItemTrace log.
                    if (pItem != NULL && pItem->isTraceItem()) {
                        char zoneName[15];
                        snprintf(zoneName, sizeof(zoneName), "%4d%3d%3d", m_pZone->getZoneID(), p.x, p.y);
                        remainTraceLog(pItem, "LairMaster", zoneName, ITEM_LOG_CREATE, DETAIL_EVENTNPC);
                        remainTraceLogNew(pItem, zoneName, ITL_GET, ITLD_EVENTNPC, m_pZone->getZoneID(), p.x, p.y);
                    }
                } else {
                    SAFE_DELETE(pItem);
                }
            }
        } else {
            throw Error("PCManager holds a creature that is not a PC");
        }
    }

    __END_CATCH
}

string MasterLairManager::toString() const

{
    StringStream msg;

    int eventSec = m_EventTime.tv_sec;

    switch (m_Event) {
    case EVENT_WAITING_PLAYER: // waiting for players to enter
        msg << "WAITING_PLAYER, ";
        break;

    case EVENT_MINION_COMBAT: // fighting the summoned monsters
        msg << "MINION_COMBAT, ";
        break;

    case EVENT_MASTER_COMBAT:
        msg << "MASTER_COMBAT, ";
        break;

    case EVENT_WAITING_KICK_OUT: // waiting to kick users out (cleanup after the master is killed)
        msg << "WAITING_KICK_OUT, ";
        break;

    case EVENT_WAITING_REGEN: // waiting for the regen
        msg << "WAITING_REGEN, ";

        eventSec = m_RegenTime.tv_sec;
        break;

    default:
        break;
    }

    Timeval currentTime;
    getCurrentTime(currentTime);

    int timeGap = eventSec - currentTime.tv_sec;

    msg << timeGap << " sec remain, " << (int)m_pZone->getPCManager()->getSize() << " players";

    return msg.toString();
}
