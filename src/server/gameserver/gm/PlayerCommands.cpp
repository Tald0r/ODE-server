//////////////////////////////////////////////////////////////////////////////
// Filename    : PlayerCommands.cpp
// Description : GM commands that act on one character or account: the penalties, the inspection
//               commands, the payments and the grants.
//////////////////////////////////////////////////////////////////////////////

#include <stdio.h>

#include <list>

#include "CGSkillToNamed.h"
#include "CreatureUtil.h"
#include "DatabaseError.h"
#include "EffectMute.h"
#include "GCAddEffect.h"
#include "GCSystemMessage.h"
#include "GameContext.h"
#include "GamePlayer.h"
#include "GameServerGroupInfoManager.h"
#include "ItemInfoManager.h"
#include "ItemUtil.h"
#include "KernelContext.h"
#include "Mine.h"
#include "NPC.h"
#include "OptionInfo.h"
#include "Ousters.h"
#include "PCFinder.h"
#include "PacketUtil.h"
#include "Properties.h"
#include "RelicUtil.h"
#include "Slayer.h"
#include "StringPool.h"
#include "VSDateTime.h"
#include "Vampire.h"
#include "Zone.h"
#include "ZoneUtil.h"
#include "gm/GMCommands.h"
#include "repository/CharacterRepository.h"
#include "repository/SessionRepository.h"

namespace de::gm {
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void opkick(GamePlayer* pGamePlayer, string msg, int i) {
    __BEGIN_TRY __BEGIN_DEBUG_EX

        size_t j = msg.find_first_of(' ', i + 1);
    string Name = msg.substr(j + 1, msg.size() - j - 1).c_str();

    Creature* pTargetCreature = NULL;

    // NoSuch removed.
    PCFinder& pcFinder = de::gameContext().playerCreatures();

    __ENTER_CRITICAL_SECTION(pcFinder)

    pTargetCreature = pcFinder.getCreature_LOCKED(Name);
    if (pTargetCreature == NULL) {
        return;
    }

    Player* pTargetPlayer = pTargetCreature->getPlayer();
    GamePlayer* pTargetGamePlayer = dynamic_cast<GamePlayer*>(pTargetPlayer);
    pTargetGamePlayer->setPenaltyFlag(PENALTY_TYPE_KICKED);
    pTargetGamePlayer->setItemRatioBonusPoint(1);

    if (pGamePlayer != NULL) {
        Creature* pCreature = pGamePlayer->getCreature();
        filelog("change.txt", "[Kick]%s --> %s", pCreature->getName().c_str(), Name.c_str());
    }

    __LEAVE_CRITICAL_SECTION(pcFinder)

    __END_DEBUG_EX __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void opmute(GamePlayer* pGamePlayer, string msg, int i) {
    __BEGIN_TRY __BEGIN_DEBUG_EX

        uint time = 0;
    size_t j = msg.find_first_of(' ', i + 1);
    size_t k = msg.find_first_of(' ', j + 1);
    string Name;
    if (k < msg.size()) {
        Name = msg.substr(j + 1, k - j - 1).c_str();
        time = (uint)atoi(msg.substr(k + 1, msg.size() - k - 1).c_str());
    } else {
        Name = msg.substr(j + 1, msg.size() - j - 1).c_str();
    }

    Creature* pTargetCreature = NULL;
    // NoSuch removed.
    PCFinder& pcFinder = de::gameContext().playerCreatures();

    __ENTER_CRITICAL_SECTION(pcFinder)

    pTargetCreature = pcFinder.getCreature_LOCKED(Name);
    if (pTargetCreature == NULL) {
        return;
    }

    Player* pTargetPlayer = pTargetCreature->getPlayer();
    GamePlayer* pTargetGamePlayer = dynamic_cast<GamePlayer*>(pTargetPlayer);

    if (time == 0) {
        pTargetGamePlayer->setPenaltyFlag(PENALTY_TYPE_MUTE);
    } else {
        Effect* pEffect = NULL;
        if (pTargetCreature->isFlag(Effect::EFFECT_CLASS_MUTE)) {
            pEffect = pTargetCreature->findEffect(Effect::EFFECT_CLASS_MUTE);

            pEffect->setDeadline(time * 600);
            pEffect->save(pTargetCreature->getName());
        } else {
            pEffect = new EffectMute(pTargetCreature);
            pEffect->setDeadline(time * 600);
            pEffect->create(pTargetCreature->getName());
            pTargetCreature->addEffect(pEffect);
            pTargetCreature->setFlag(pEffect->getEffectClass());
        }

        GCAddEffect gcAddEffect;
        gcAddEffect.setObjectID(pTargetCreature->getObjectID());
        gcAddEffect.setEffectID(pEffect->getSendEffectClass());
        gcAddEffect.setDuration(time * 600);

        pTargetGamePlayer->sendPacket(&gcAddEffect);
    }

    __LEAVE_CRITICAL_SECTION(pcFinder)

    __END_DEBUG_EX __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void opdenychat(GamePlayer* pGamePlayer, string msg, int i) {
    __BEGIN_TRY __BEGIN_DEBUG_EX

        uint level = 0;
    uint time = 0;
    size_t j = msg.find_first_of(' ', i + 1);
    size_t k = msg.find_first_of(' ', j + 1);
    string Name;
    if (k < msg.size()) {
        level = (uint)atoi(msg.substr(j + 1, k - j - 1).c_str());
        Name = msg.substr(k + 1, msg.size() - k - 1).c_str();
    } else {
        return;
    }

    Creature* pTargetCreature = NULL;

    if (level == 1) {
        // level1 = 1 hour
        time = 1 * 60;
    } else if (level == 2) {
        // level2 = 6 hours
        time = 6 * 60;
    } else if (level == 3) {
        // level3 = 12 hours
        time = 12 * 60;
    } else if (level == 4) {
        // level4 = 24 hours
        time = 24 * 60;
    } else if (level == 5) {
        // level5 = 168 hours
        time = 168 * 60;
    }

    // NoSuch removed.
    PCFinder& pcFinder = de::gameContext().playerCreatures();

    __ENTER_CRITICAL_SECTION(pcFinder)

    pTargetCreature = pcFinder.getCreature_LOCKED(Name);
    if (pTargetCreature == NULL) {
        return;
    }

    Player* pTargetPlayer = pTargetCreature->getPlayer();
    GamePlayer* pTargetGamePlayer = dynamic_cast<GamePlayer*>(pTargetPlayer);

    if (time == 0) {
        pTargetGamePlayer->setPenaltyFlag(PENALTY_TYPE_MUTE);
    } else {
        Effect* pEffect = NULL;
        if (pTargetCreature->isFlag(Effect::EFFECT_CLASS_MUTE)) {
            pEffect = pTargetCreature->findEffect(Effect::EFFECT_CLASS_MUTE);

            pEffect->setDeadline(time * 600);
            pEffect->save(pTargetCreature->getName());
        } else {
            pEffect = new EffectMute(pTargetCreature);
            pEffect->setDeadline(time * 600);
            pEffect->create(pTargetCreature->getName());
            pTargetCreature->addEffect(pEffect);
            pTargetCreature->setFlag(pEffect->getEffectClass());
        }

        GCAddEffect gcAddEffect;
        gcAddEffect.setObjectID(pTargetCreature->getObjectID());
        gcAddEffect.setEffectID(pEffect->getSendEffectClass());
        gcAddEffect.setDuration(time * 600);

        pTargetGamePlayer->sendPacket(&gcAddEffect);
    }

    __LEAVE_CRITICAL_SECTION(pcFinder)

    __END_DEBUG_EX __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void opfreezing(GamePlayer* pGamePlayer, string msg, int i) {
    __BEGIN_TRY __BEGIN_DEBUG_EX

        size_t j = msg.find_first_of(' ', i + 1);
    string Name = msg.substr(j + 1, msg.size() - j - 1).c_str();

    Creature* pTargetCreature = NULL;

    // NoSuch removed.
    PCFinder& pcFinder = de::gameContext().playerCreatures();

    __ENTER_CRITICAL_SECTION(pcFinder)

    pTargetCreature = pcFinder.getCreature_LOCKED(Name);
    if (pTargetCreature == NULL) {
        return;
    }

    Player* pTargetPlayer = pTargetCreature->getPlayer();
    GamePlayer* pTargetGamePlayer = dynamic_cast<GamePlayer*>(pTargetPlayer);
    pTargetGamePlayer->setPenaltyFlag(PENALTY_TYPE_FREEZING);

    __LEAVE_CRITICAL_SECTION(pcFinder)

    __END_DEBUG_EX __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void opdeny(GamePlayer* pGamePlayer, string msg, int i) {
    __BEGIN_TRY __BEGIN_DEBUG_EX

        size_t j = msg.find_first_of(' ', i + 1);
    string Name = msg.substr(j + 1, msg.size() - j - 1).c_str();

    // Both statements ran with no BEGIN_DB (see opsave for what that means
    // for the failure path; the same swallow is reproduced here).
    string PlayerID;

    try {
        if (defaultCharacterRepository().loadSlayerPlayerID(PLAYERID_SPELLING_OPDENY, Name, PlayerID)) {
            defaultSessionRepository().denyAccount(PlayerID);
        }
    } catch (const DatabaseError&) {
        return;
    }
    __END_DEBUG_EX __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void opinfo(GamePlayer* pGamePlayer, string msg, int i) {
    __BEGIN_TRY __BEGIN_DEBUG_EX

        if (pGamePlayer == NULL) return;

    size_t j = msg.find_first_of(' ', i + 1);
    string Name = msg.substr(j + 1, msg.size() - j - 1).c_str();

    Creature* pTargetCreature = NULL;
    // NoSuch removed.
    PCFinder& pcFinder = de::gameContext().playerCreatures();

    __ENTER_CRITICAL_SECTION(pcFinder)

    pTargetCreature = pcFinder.getCreature_LOCKED(Name);
    if (pTargetCreature == NULL) {
        return;
    }

    StringStream msg;

    msg << "Name : " << Name << " Host : " << pTargetCreature->getPlayer()->getSocket()->getHost();

    if (pTargetCreature->isSlayer()) {
        Slayer* pSlayer = dynamic_cast<Slayer*>(pTargetCreature);

        msg << " STR : " << (int)pSlayer->getSTR() << " / " << pSlayer->getSTR(ATTR_MAX)
            << " DEX : " << (int)pSlayer->getDEX() << " / " << pSlayer->getDEX(ATTR_MAX)
            << " INT : " << (int)pSlayer->getINT() << " / " << pSlayer->getINT(ATTR_MAX)
            << " DAM : " << (int)pSlayer->getDamage() << " / " << pSlayer->getDamage(ATTR_MAX) << " / "
            << pSlayer->getDamage(ATTR_BASIC) << " Defense  : " << (int)pSlayer->getDefense()
            << " TOHIT : " << (int)pSlayer->getToHit() << " HP : " << (int)pSlayer->getHP() << " / "
            << pSlayer->getHP(ATTR_MAX) << " MP : " << (int)pSlayer->getMP() << " / " << pSlayer->getMP(ATTR_MAX)
            << " Fame : " << (int)pSlayer->getFame() << " Gold : " << (int)pSlayer->getGold()
            << " StashGold : " << (int)pSlayer->getStashGold();
    } else if (pTargetCreature->isVampire()) {
        Vampire* pVampire = dynamic_cast<Vampire*>(pTargetCreature);

        msg << " STR : " << pVampire->getSTR() << " / " << pVampire->getSTR(ATTR_MAX) << " DEX : " << pVampire->getDEX()
            << " / " << pVampire->getDEX(ATTR_MAX) << " INT : " << pVampire->getINT() << " / "
            << pVampire->getINT(ATTR_MAX) << " Defense  : " << (int)pVampire->getDefense()
            << " TOHIT : " << (int)pVampire->getToHit() << " HP : " << (int)pVampire->getHP() << " / "
            << pVampire->getHP(ATTR_MAX) << " Fame : " << (int)pVampire->getFame()
            << " Gold : " << (int)pVampire->getGold() << " StashGold : " << (int)pVampire->getStashGold();
    } else if (pTargetCreature->isOusters()) {
        Ousters* pOusters = dynamic_cast<Ousters*>(pTargetCreature);

        msg << " STR : " << pOusters->getSTR() << " / " << pOusters->getSTR(ATTR_MAX) << " DEX : " << pOusters->getDEX()
            << " / " << pOusters->getDEX(ATTR_MAX) << " INT : " << pOusters->getINT() << " / "
            << pOusters->getINT(ATTR_MAX) << " Defense  : " << (int)pOusters->getDefense()
            << " TOHIT : " << (int)pOusters->getToHit() << " HP : " << (int)pOusters->getHP() << " / "
            << pOusters->getHP(ATTR_MAX) << " Fame : " << (int)pOusters->getFame()
            << " Gold : " << (int)pOusters->getGold() << " StashGold : " << (int)pOusters->getStashGold();
    }

    GCSystemMessage gcSystemMessage;

    gcSystemMessage.setMessage(msg.toString());

    pGamePlayer->sendPacket(&gcSystemMessage);

    __LEAVE_CRITICAL_SECTION(pcFinder)

    __END_DEBUG_EX __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void opfind(GamePlayer* pGamePlayer, string msg, int i) {
    __BEGIN_TRY __BEGIN_DEBUG_EX

        if (pGamePlayer == NULL) return;
    size_t j = msg.find_first_of(' ', i + 1);
    string Name = msg.substr(j + 1, msg.size() - j - 1).c_str();

    // Ran with no BEGIN_DB; see opsave for the failure path reproduced here.
    int serverID = 0;
    bool found = false;

    try {
        found = defaultSessionRepository().loadUserServerID(Name, serverID);
    } catch (const DatabaseError&) {
        return;
    }

    static WorldID_t WorldID = de::kernelContext().config().getPropertyInt("WorldID");

    if (found) {
        ServerID_t ServerID = serverID;
        GameServerGroupInfoManager& groups = de::gameContext().gameServerGroups();
        string ServerName = groups.getGameServerGroupInfo(ServerID, WorldID)->getGroupName();

        char msg[100];
        snprintf(msg, sizeof(msg), de::gameContext().strings().c_str(STRID_PLAYER_IN_GAMESERVER), Name.c_str(),
                 ServerName.c_str());

        GCSystemMessage gcSystemMessage;
        gcSystemMessage.setMessage(msg);
        pGamePlayer->sendPacket(&gcSystemMessage);
    } else {
        char msg[100];
        snprintf(msg, sizeof(msg), de::gameContext().strings().c_str(STRID_PLAYER_NOT_IN_GAMESERVER), Name.c_str());

        GCSystemMessage gcSystemMessage;
        gcSystemMessage.setMessage(msg);
        pGamePlayer->sendPacket(&gcSystemMessage);
    }

    __END_DEBUG_EX __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
// Prints the credits
// GCSystemMessage tops out at 256 characters, so keep it short.
//////////////////////////////////////////////////////////////////////////////
void opcredit(GamePlayer* pGamePlayer, string msg, int i) {
    __BEGIN_TRY

    if (pGamePlayer == NULL)
        return;

    static unordered_map<string, string> Credits;

    // Kept hard-coded for now...
    if (Credits.empty()) {
        // No name is known for this entry.
        Credits["unnamed"] = "client, server (2005~)";
    }

    size_t j = msg.find_first_of(' ', i + 1);
    string Name = msg.substr(j + 1, msg.size() - j - 1).c_str();

    unordered_map<string, string>::const_iterator itr = Credits.find(Name);

    if (itr != Credits.end()) {
        StringStream msg;
        msg << "[" << itr->first << "] " << itr->second;

        GCSystemMessage gcSystemMessage;
        gcSystemMessage.setMessage(msg.toString());
        pGamePlayer->sendPacket(&gcSystemMessage);
    }

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void opuser(GamePlayer* pGamePlayer, string msg, int i) {
    __BEGIN_TRY __BEGIN_DEBUG_EX

        if (pGamePlayer == NULL) return;

    // A SQL failure is swallowed here; see opsave.
    int GroupCount = 0;

    try {
        GroupCount = defaultSessionRepository().countPlayersOnline();
    } catch (const DatabaseError&) {
        return;
    }

    char msg[100];
    snprintf(msg, sizeof(msg), de::gameContext().strings().c_str(STRID_CURRENT_NUMBER_OF_PLAYER), GroupCount);

    GCSystemMessage gcSystemMessage;
    gcSystemMessage.setMessage(msg);
    pGamePlayer->sendPacket(&gcSystemMessage);

    __END_DEBUG_EX __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void optrace(GamePlayer* pGamePlayer, string msg, int i) {
    __BEGIN_TRY __BEGIN_DEBUG_EX

        if (pGamePlayer == NULL) return;

    Creature* pCreature = pGamePlayer->getCreature();

    size_t j = msg.find_first_of(' ', i + 1);
    size_t k = msg.find_first_of(' ', j + 1);

    bool isNPCTrace = false;

    if (msg.size() > k && !strncasecmp(msg.substr(j + 1, k - j - 1).c_str(), "NPC", 3))
        isNPCTrace = true;

    string Name;

    if (isNPCTrace) {
        Name = msg.substr(k + 1, msg.size() - k - 1);
    } else {
        Name = msg.substr(j + 1, msg.size() - j - 1);
    }

    Creature* pTargetCreature = NULL;

    // NoSuch removed.
    PCFinder& pcFinder = de::gameContext().playerCreatures();

    __ENTER_CRITICAL_SECTION(pcFinder)

    if (isNPCTrace) {
        NPC* pNPC = pcFinder.getNPC_LOCKED(Name);
        pTargetCreature = dynamic_cast<Creature*>(pNPC);
    } else {
        pTargetCreature = pcFinder.getCreature_LOCKED(Name);
    }

    if (pTargetCreature == NULL) {
        return;
    }

    ZoneID_t ZoneNum = pTargetCreature->getZoneID();
    Coord_t ZoneX = pTargetCreature->getX();
    Coord_t ZoneY = pTargetCreature->getY();

    Assert((int)ZoneX < 256);
    Assert((int)ZoneY < 256);
    Assert(pCreature->isPC());

    transportCreature(pCreature, ZoneNum, ZoneX, ZoneY, false);

    __LEAVE_CRITICAL_SECTION(pcFinder)

    __END_DEBUG_EX __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void oppay(GamePlayer* pGamePlayer, string msg, int i) {
    __BEGIN_TRY __BEGIN_DEBUG_EX

        if (pGamePlayer == NULL) return;

    char str[80];

    if (pGamePlayer->isPayPlaying()) {
        Timeval currentTime;
        getCurrentTime(currentTime);
        Timeval payTime = pGamePlayer->getPayPlayTime(currentTime);

        if (pGamePlayer->getPayPlayType() == PAY_PLAY_TYPE_PERSON) {
            strcpy(str, "[Metrotech][Personal] ");
        } else {
            strcpy(str, "[Metrotech][PC room] ");
        }

        if (pGamePlayer->getPayType() == PAY_TYPE_FREE) {
            strcat(str, "Free account.");
        } else if (pGamePlayer->getPayType() == PAY_TYPE_PERIOD) {
            const size_t used = strlen(str);
            snprintf(str + used, sizeof(str) - used, "Available until %s.",
                     pGamePlayer->getPayPlayAvailableDateTime().toString().c_str());
        } else {
            const size_t used = strlen(str);
            snprintf(str + used, sizeof(str) - used, "Remaining time : %d / %d min", (int)(payTime.tv_sec / 60),
                     (int)pGamePlayer->getPayPlayAvailableHours());
        }
    } else {
        strcpy(str, "[Metrotech] Free play.");
    }

    GCSystemMessage gcSystemMessage;
    gcSystemMessage.setMessage(str);
    pGamePlayer->sendPacket(&gcSystemMessage);

    __END_DEBUG_EX __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void opfun(GamePlayer* pGamePlayer, string msg, int i) {
    __BEGIN_TRY __BEGIN_DEBUG_EX

        if (pGamePlayer == NULL) return;

    size_t j = msg.find_first_of(' ', i + 1);
    size_t k = msg.find_first_of(' ', j + 1);
    size_t l = msg.find_first_of(' ', k + 1);

    string FunType = trim(msg.substr(j + 1, k - j - 1));
    int value1 = atoi(msg.substr(k + 1, l - k - 1).c_str());
    int value2 = atoi(msg.substr(l + 1, msg.size() - l - 1).c_str());


    if (FunType == "mine") {
        int ItemType = value1;
        int ItemNum = value2;

        ItemType = max(0, ItemType);
        ItemType = min((int)de::gameContext().itemInfos().getItemCount(Item::ITEM_CLASS_MINE) - 1, ItemType);

        ItemNum = max(1, ItemNum);
        ItemNum = min(36, ItemNum);

        Creature* pCreature = pGamePlayer->getCreature();
        Assert(pCreature != NULL);


        Zone* pZone = pCreature->getZone();
        Assert(pZone != NULL);

        ItemInfo* pItemInfo = de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_MINE, ItemType);
        if (pItemInfo == NULL)
            return;

        Damage_t MinDamage = pItemInfo->getMinDamage();
        Damage_t MaxDamage = pItemInfo->getMaxDamage();

        for (int i = 0; i < ItemNum; i++) {
            Mine* pInstallMine = new Mine();
            Assert(pInstallMine != NULL);

            ObjectRegistry& OR = pZone->getObjectRegistry();
            OR.registerObject(pInstallMine);

            int SkillLevel = 100;
            Damage_t RealDamage = MinDamage + (max(0, ((int)MaxDamage * (int)SkillLevel / 100) - MinDamage));

            pInstallMine->setItemType(ItemType);
            pInstallMine->setDir(pCreature->getDir());
            pInstallMine->setDamage(RealDamage);
            pInstallMine->setInstallerName(pCreature->getName());
            pInstallMine->setInstallerPartyID(pCreature->getPartyID());
            pInstallMine->setFlag(Effect::EFFECT_CLASS_INSTALL);

            TPOINT pt = pZone->addItem(pInstallMine, pCreature->getX(), pCreature->getY(), true, 6000);

            addInstalledMine(pZone, pInstallMine, pt.x, pt.y);
        }

        GCSystemMessage gcSystemMessage;
        gcSystemMessage.setMessage("Mines placed.");
        pGamePlayer->sendPacket(&gcSystemMessage);
    } else {
        GCSystemMessage gcSystemMessage;
        gcSystemMessage.setMessage("Unknown fun type; only mine is supported.");
        pGamePlayer->sendPacket(&gcSystemMessage);
    }

    __END_DEBUG_EX __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void opgrant(GamePlayer* pGamePlayer, string msg, int i) {
    __BEGIN_TRY __BEGIN_DEBUG_EX

        if (pGamePlayer == NULL) return;

    Creature* pCreature = pGamePlayer->getCreature();

    size_t j = msg.find_first_of(' ', i + 1);
    size_t k = msg.find_first_of(' ', j + 1);

    BYTE Competence = atoi(msg.substr(j + 1, k - j - 1).c_str());

    Zone* pZone = pCreature->getZone();

    string Name = msg.substr(k + 1, msg.size() - k - 1).c_str();

    Creature* pTargetCreature = NULL;

    pTargetCreature = pZone->getCreature(Name);

    // NoSuch removed.
    if (pTargetCreature != NULL) {
        if (pTargetCreature->isSlayer()) {
            Slayer* pSlayer = dynamic_cast<Slayer*>(pTargetCreature);
            pSlayer->setCompetence(Competence);
        } else if (pTargetCreature->isVampire()) {
            Vampire* pVampire = dynamic_cast<Vampire*>(pTargetCreature);
            pVampire->setCompetence(Competence);
        } else if (pTargetCreature->isOusters()) {
            Ousters* pOusters = dynamic_cast<Ousters*>(pTargetCreature);
            pOusters->setCompetence(Competence);
        }
    }


    __END_DEBUG_EX __END_CATCH
}

void opsoulchain(GamePlayer* pPlayer, string msg, int i) {
    __BEGIN_TRY __BEGIN_DEBUG_EX

        size_t j = msg.find_first_of(' ', i + 1);

    string Name = msg.substr(j + 1, msg.size() - j - 1).c_str();

    CGSkillToNamed packet;
    packet.setSkillType(SKILL_SOUL_CHAIN);
    packet.setTargetName(Name);

    // The GM command feeds a synthetic packet straight to the handler.
    CGSkillToNamedHandler::execute(&packet, pPlayer);

    __END_DEBUG_EX __END_CATCH
}
} // namespace de::gm
