//////////////////////////////////////////////////////////////////////////////
// Filename    : CGRelicToObjectHandler.cc
// Written By  : elca@ewestsoft.com
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "CGRelicToObject.h"
#include "GameContext.h"
#include "ItemInfoManager.h"

#ifdef __GAME_SERVER__
#include <stdio.h>

#include <map>

#include "CastleShrineInfoManager.h"
#include "CombatInfoManager.h"
#include "Corpse.h"
#include "EffectHasSlayerRelic.h"
#include "EffectHasVampireRelic.h"
#include "EffectIncreaseAttr.h"
#include "EffectKillTimer.h"
#include "EffectPrecedence.h"
#include "EffectRelicTable.h"
#include "EffectSlayerRelic.h"
#include "EffectVampireRelic.h"
#include "GCAddEffect.h"
#include "GCCannotAdd.h"
#include "GCDeleteInventoryItem.h"
#include "GCDeleteObject.h"
#include "GCSay.h"
#include "GCSystemMessage.h"
#include "GamePlayer.h"
#include "LevelWarManager.h"
#include "Monster.h"
#include "MonsterCorpse.h"
#include "Relic.h"
#include "RelicUtil.h"
#include "ShrineInfoManager.h"
#include "SiegeManager.h"
#include "SkillUtil.h"
#include "Slayer.h"
#include "StringPool.h"
#include "Sweeper.h"
#include "Vampire.h"
#include "VariableManager.h"
#include "Zone.h"
#include "ZoneGroupManager.h"
#include "ZoneUtil.h"
#include "ctf/FlagManager.h"
#endif // __GAME_SERVER__

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void CGRelicToObjectHandler::execute(CGRelicToObject* pPacket, Player* pPlayer)

{
    __BEGIN_TRY __BEGIN_DEBUG_EX

#ifdef __GAME_SERVER__


        Assert(pPacket != NULL);
    Assert(pPlayer != NULL);

    // When a relic is put into the matching relic table..
    GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(pPlayer);
    Creature* pCreature = pGamePlayer->getCreature();

    // When the relic table already holds both
    // When the relic table holds a Slayer relic and pItem is a Slayer relic
    PlayerCreature* pPlayerCreature = dynamic_cast<PlayerCreature*>(pCreature);
    if (pPlayerCreature == NULL) {
        throw DisconnectException("CGRelicToObject : invalid state");
        return;
    }

    // Is the item currently held == Relic ?
    InventorySlot* pExtraInventorySlot = pPlayerCreature->getExtraInventorySlot();
    Item* pItem = pExtraInventorySlot->getItem();

    if (pItem != NULL && pItem->getItemClass() == Item::ITEM_CLASS_EVENT_ITEM && pItem->getItemType() == 31) {
        static map<string, string> scripts;
        if (scripts.empty()) {
            scripts["Johnson"] = "Thank you. I will not forget this kindness";
            scripts["Billy"] = "Please hurry and rescue the others too. I beg you.";
            scripts["Lee"] = "Ugh.... Barely made it.";
            scripts["Ezkatan"] = "I will go on ahead.";
            scripts["Lewis"] = "Sob. That was so hard. Thank you so much.";
            scripts["Genji"] = "I am saved now. See you in the village";
            scripts["Kesui"] = "Hurry, the other comrades too....";
            scripts["Pai"] = "Ugh.... There is no strength left in my body.";
            scripts["Ninairu"] = "I will go on ahead.";
            scripts["Ryo"] = "Phew... I am saved.";
            scripts["Fate"] = "Thank you. I will go ahead and wait for you.";
            scripts["Sol"] = "Please hurry and rescue the others too. I beg you.";
            scripts["Irui"] = "Ugh.... That was awful. I barely made it.";
            scripts["Jyonki"] = "I will go on ahead.";
            scripts["Gnu"] = "I knew you would come.";
        }

        Zone* pZone = pPlayerCreature->getZone();
        Monster* pMonster = dynamic_cast<Monster*>(pZone->getCreature(pPacket->getObjectID()));
        if (pMonster == NULL) {
            GCCannotAdd _GCCannotAdd;
            _GCCannotAdd.setObjectID(pPacket->getObjectID());
            pPlayer->sendPacket(&_GCCannotAdd);

            return;
        }

        if ((pPlayerCreature->isSlayer() && pMonster->getMonsterType() != 793) ||
            (pPlayerCreature->isVampire() && pMonster->getMonsterType() != 794) ||
            (pPlayerCreature->isOusters() && pMonster->getMonsterType() != 795) ||
            scripts.find(pMonster->getName()) == scripts.end()) {
            GCCannotAdd _GCCannotAdd;
            _GCCannotAdd.setObjectID(pPacket->getObjectID());
            pPlayer->sendPacket(&_GCCannotAdd);


            return;
        }

        GCSay gcSay;
        gcSay.setObjectID(pMonster->getObjectID());
        gcSay.setColor(255);
        gcSay.setMessage(scripts[pMonster->getName()]);

        pZone->broadcastPacket(pMonster->getX(), pMonster->getY(), &gcSay);
        EffectKillTimer* pEffect = new EffectKillTimer(pMonster, true);
        pMonster->setFlag(pEffect->getEffectClass());
        pEffect->setDeadline(50);
        pMonster->addEffect(pEffect);

        pPlayerCreature->deleteItemFromExtraInventorySlot();

        GCDeleteInventoryItem gcDI;
        gcDI.setObjectID(pPacket->getItemObjectID());
        pGamePlayer->sendPacket(&gcDI);

        pItem->destroy();
        SAFE_DELETE(pItem);

        return;
    }

    if (pItem == NULL || (!isRelicItem(pItem) && !pItem->isFlagItem() && !pItem->isSweeper())) {
        GCCannotAdd _GCCannotAdd;
        _GCCannotAdd.setObjectID(pPacket->getObjectID());
        pPlayer->sendPacket(&_GCCannotAdd);

        return;
    }

    if (pItem->getItemClass() == Item::ITEM_CLASS_RELIC) {
        executeRelic(pPacket, pPlayer);
    } else if (pItem->getItemClass() == Item::ITEM_CLASS_BLOOD_BIBLE) {
        executeBloodBible(pPacket, pPlayer);
    } else if (pItem->getItemClass() == Item::ITEM_CLASS_CASTLE_SYMBOL) {
        executeCastleSymbol(pPacket, pPlayer);
    } else if (pItem->isFlagItem()) {
        executeFlag(pPacket, pPlayer);
    } else if (pItem->getItemClass() == Item::ITEM_CLASS_SWEEPER) {
        executeSweeper(pPacket, pPlayer);
    } else {
        throw DisconnectException("RelicToObject sent while holding something that is not a relic");
    }

#endif

    __END_DEBUG_EX __END_CATCH
}

void CGRelicToObjectHandler::executeRelic(CGRelicToObject* pPacket, Player* pPlayer)

{
    __BEGIN_TRY

    StringPool& strings = de::gameContext().strings();

#ifdef __GAME_SERVER__

    // When a relic is put into the matching relic table..
    GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(pPlayer);
    Creature* pCreature = pGamePlayer->getCreature();

    // When the relic table already holds both
    // When the relic table holds a Slayer relic and pItem is a Slayer relic
    PlayerCreature* pPlayerCreature = dynamic_cast<PlayerCreature*>(pCreature);

    // Is the item currently held == Relic ?
    InventorySlot* pExtraInventorySlot = pPlayerCreature->getExtraInventorySlot();
    Item* pItem = pExtraInventorySlot->getItem();

    Zone* pZone = pCreature->getZone();
    Assert(pZone != NULL);


    bool Success = false;

    Item* pTableItem = pZone->getItem(pPacket->getObjectID());

    // No such item, or
    // not a corpse, or
    // not a Monster corpse: then it is not a relic table.
    if (pTableItem == NULL || pTableItem->getItemClass() != Item::ITEM_CLASS_CORPSE ||
        pTableItem->getItemType() != MONSTER_CORPSE) {
        GCCannotAdd _GCCannotAdd;
        _GCCannotAdd.setObjectID(pPacket->getObjectID());
        pPlayer->sendPacket(&_GCCannotAdd);

        return;
    }

    // Relic table
    MonsterCorpse* pCorpse = dynamic_cast<MonsterCorpse*>(pTableItem);
    Assert(pCorpse != NULL);

    // It has to be within 2 tiles.
    if (!verifyDistance(pCreature, pCorpse->getX(), pCorpse->getY(), 2)) {
        GCCannotAdd _GCCannotAdd;
        _GCCannotAdd.setObjectID(pPacket->getObjectID());
        pPlayer->sendPacket(&_GCCannotAdd);

        return;
    }

    // Check whether a Relic is held.
    bool bPlayerHasSlayerRelic = pCreature->isFlag(Effect::EFFECT_CLASS_HAS_SLAYER_RELIC);
    bool bPlayerHasVampireRelic = pCreature->isFlag(Effect::EFFECT_CLASS_HAS_VAMPIRE_RELIC);
    bool bTableHasSlayerRelic = pCorpse->isFlag(Effect::EFFECT_CLASS_SLAYER_RELIC);
    bool bTableHasVampireRelic = pCorpse->isFlag(Effect::EFFECT_CLASS_VAMPIRE_RELIC);
    bool bSlayerRelicTable = pCorpse->isFlag(Effect::EFFECT_CLASS_SLAYER_RELIC_TABLE);
    bool bVampireRelicTable = pCorpse->isFlag(Effect::EFFECT_CLASS_VAMPIRE_RELIC_TABLE);

    // Both relics are already there, or
    // the Player has neither relic, or
    // there is no item?
    // or it is not a relic
    if ((bTableHasSlayerRelic && bTableHasVampireRelic) || (!bPlayerHasSlayerRelic && !bPlayerHasVampireRelic) ||
        pItem == NULL || pItem->getItemClass() != Item::ITEM_CLASS_RELIC) {
        GCCannotAdd _GCCannotAdd;
        _GCCannotAdd.setObjectID(pPacket->getObjectID());
        pPlayer->sendPacket(&_GCCannotAdd);

        return;
    }

    ItemType_t relicIndex = pItem->getItemType();

    // Get the RelicInfo.
    const RelicInfo* pRelicInfo =
        dynamic_cast<RelicInfo*>(de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_RELIC, relicIndex));

    if (pRelicInfo == NULL) {
        filelog("relic.log", "no such relic index(%d)", relicIndex);

        GCCannotAdd _GCCannotAdd;
        _GCCannotAdd.setObjectID(pPacket->getObjectID());
        pPlayer->sendPacket(&_GCCannotAdd);

        return;
    }

    bool bSlayer = pCreature->isSlayer();
    bool bVampire = pCreature->isVampire();

    // A wrong itemObjectID, or
    // no item held, or
    // someone else's table, or
    // a relic of the same race already there: then it cannot be put in.
    if (pItem->getObjectID() != pPacket->getItemObjectID() || (bSlayer && bVampireRelicTable) ||
        (bVampire && bSlayerRelicTable) || (bTableHasSlayerRelic && pRelicInfo->relicType == RELIC_TYPE_SLAYER) ||
        (bTableHasVampireRelic && pRelicInfo->relicType == RELIC_TYPE_VAMPIRE)) {
        GCCannotAdd _GCCannotAdd;
        _GCCannotAdd.setObjectID(pPacket->getObjectID());
        pPlayer->sendPacket(&_GCCannotAdd);

        return;
    }

    // A Slayer must not be on a motorcycle or in sniping mode, and
    // a Vampire must not be transformed or invisible.
    if (bSlayer) {
        Slayer* pSlayer = dynamic_cast<Slayer*>(pCreature);

        // Riding a motorcycle makes it impossible.
        if (!pSlayer->hasRideMotorcycle() && !pSlayer->isFlag(Effect::EFFECT_CLASS_SNIPING_MODE)) {
            // Attach the Effect.
            Success = true;
        }
    } else if (bVampire) {
        if (!pCreature->isFlag(Effect::EFFECT_CLASS_TRANSFORM_TO_BAT) &&
            !pCreature->isFlag(Effect::EFFECT_CLASS_TRANSFORM_TO_WOLF) &&
            !pCreature->isFlag(Effect::EFFECT_CLASS_HIDE) && !pCreature->isFlag(Effect::EFFECT_CLASS_INVISIBILITY) &&
            !pCreature->isFlag(Effect::EFFECT_CLASS_FADE_OUT)) {
            Success = true;
        }
    }

    // When the relic can be put into the relic table
    if (Success) {
        // Erase the item from the Mouse and
        pPlayerCreature->deleteItemFromExtraInventorySlot();

        // add the relic to the relic table.
        pCorpse->addTreasure(pItem);


        char msg[100];
        snprintf(msg, sizeof(msg), strings.c_str(STRID_PUT_RELIC_TO_RELIC_TABLE), pPlayerCreature->getName().c_str(),
                 pRelicInfo->getName().c_str());

        GCSystemMessage gcSystemMessage;
        gcSystemMessage.setMessage(msg);
        de::gameContext().zoneGroups().broadcast(&gcSystemMessage);

        Effect::EffectClass effectClass;
        Effect::EffectClass effectClassTable;

        // Remove the Effect from the Creature and
        if (pRelicInfo->relicType == RELIC_TYPE_SLAYER) {
            effectClass = Effect::EFFECT_CLASS_HAS_SLAYER_RELIC;
            effectClassTable = Effect::EFFECT_CLASS_SLAYER_RELIC;
        } else {
            effectClass = Effect::EFFECT_CLASS_HAS_VAMPIRE_RELIC;
            effectClassTable = Effect::EFFECT_CLASS_VAMPIRE_RELIC;
        }

        Effect* pEffect = pCreature->findEffect(effectClass);
        Assert(pEffect != NULL);

        // turn the Creature's flag off and
        // send GCRemoveEffect.
        pEffect->unaffect();
        pCreature->deleteEffect(effectClass);

        // Attach the Effect saying the relic table holds a Relic.
        if (pRelicInfo->relicType == RELIC_TYPE_SLAYER) {
            EffectSlayerRelic* pEffect = new EffectSlayerRelic(pCorpse);

            pCorpse->getEffectManager().addEffect(pEffect);
            pCorpse->setFlag(Effect::EFFECT_CLASS_SLAYER_RELIC);
            pEffect->affect(pCorpse);
        } else {
            EffectVampireRelic* pEffect = new EffectVampireRelic(pCorpse);

            pCorpse->getEffectManager().addEffect(pEffect);
            pCorpse->setFlag(Effect::EFFECT_CLASS_VAMPIRE_RELIC);
            pEffect->affect(pCorpse);
        }

        // Tell the client that an Effect was attached to the relic table.
        GCAddEffect gcAddEffect;
        gcAddEffect.setObjectID(pCorpse->getObjectID());
        gcAddEffect.setEffectID(effectClassTable);
        gcAddEffect.setDuration(65000);
        pZone->broadcastPacket(pCorpse->getX(), pCorpse->getY(), &gcAddEffect);

        // Report that the relic was placed.
        GCDeleteObject gcDeleteObject;
        gcDeleteObject.setObjectID(pItem->getObjectID());
        pPlayer->sendPacket(&gcDeleteObject);

        // RelicTable
        EffectRelicTable* pTableEffect = NULL;
        if (bSlayer) {
            // Set the relic's owner.
            de::gameContext().combatInfo().setRelicOwner(relicIndex, CombatInfoManager::RELIC_OWNER_SLAYER);

            // Find the effect.
            Effect* pEffect = pCorpse->getEffectManager().findEffect(Effect::EFFECT_CLASS_SLAYER_RELIC_TABLE);
            Assert(pEffect != NULL);

            pTableEffect = dynamic_cast<EffectSlayerRelicTable*>(pEffect);
            Assert(pTableEffect != NULL);
        } else {
            // Set the relic's owner.
            de::gameContext().combatInfo().setRelicOwner(relicIndex, CombatInfoManager::RELIC_OWNER_VAMPIRE);

            // Find the effect.
            Effect* pEffect = pCorpse->getEffectManager().findEffect(Effect::EFFECT_CLASS_VAMPIRE_RELIC_TABLE);
            Assert(pEffect != NULL);

            pTableEffect = dynamic_cast<EffectVampireRelicTable*>(pEffect);
            Assert(pTableEffect != NULL);
        }

        // Once one relic goes in,
        // no relic can be taken out for a while (10 seconds).
        Timeval lockTime;
        getCurrentTime(lockTime);
        lockTime.tv_sec += 10;
        pTableEffect->setLockTime(lockTime);


        // When both relics end up held
        if ((bTableHasSlayerRelic && pRelicInfo->relicType == RELIC_TYPE_VAMPIRE) ||
            (bTableHasVampireRelic && pRelicInfo->relicType == RELIC_TYPE_SLAYER))

        {
            // Set the relic table's safe time
            Timeval safeTime;
            getCurrentTime(safeTime);
            safeTime.tv_sec += de::gameContext().variables().getCombatBonusTime() * 60;

            // Send the victory message.
            GCSystemMessage gcSystemMessage;

            pTableEffect->setSafeTime(safeTime);

            if (bSlayer) {
                gcSystemMessage.setMessage(strings.getString(STRID_COMBAT_SLAYER_WIN));
                de::gameContext().combatInfo().setRelicOwner(relicIndex, CombatInfoManager::RELIC_OWNER_SLAYER);
            } else {
                gcSystemMessage.setMessage(strings.getString(STRID_COMBAT_VAMPIRE_WIN));
                de::gameContext().combatInfo().setRelicOwner(relicIndex, CombatInfoManager::RELIC_OWNER_VAMPIRE);
            }

            // The war has ended.
            de::gameContext().combatInfo().setCombat(false);

            // Send the message to every user.
            de::gameContext().zoneGroups().broadcast(&gcSystemMessage);

            de::gameContext().combatInfo().computeModify();
        }
    } else {
        GCCannotAdd _GCCannotAdd;
        _GCCannotAdd.setObjectID(pPacket->getObjectID());
        pPlayer->sendPacket(&_GCCannotAdd);
    }

#endif // __GAME_SERVER__

    __END_CATCH
}

void CGRelicToObjectHandler::executeBloodBible(CGRelicToObject* pPacket, Player* pPlayer)

{
    __BEGIN_TRY

#ifdef __GAME_SERVER__

    GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(pPlayer);
    Creature* pCreature = pGamePlayer->getCreature();
    Zone* pZone = pCreature->getZone();
    Assert(pZone != NULL);


    // When the relic table already holds both
    // When the relic table holds a Slayer relic and pItem is a Slayer relic
    PlayerCreature* pPlayerCreature = dynamic_cast<PlayerCreature*>(pCreature);

    // Is the item currently held == Relic ?
    InventorySlot* pExtraInventorySlot = pPlayerCreature->getExtraInventorySlot();
    Item* pItem = pExtraInventorySlot->getItem();

    Item* pTableItem = pZone->getItem(pPacket->getObjectID());

    // No such item, or
    // not a corpse, or
    // Not a Monster corpse, or
    // neither a ShrineGuard nor a ShrineHoly: then it is not a shrine.
    if (pTableItem == NULL || pTableItem->getItemClass() != Item::ITEM_CLASS_CORPSE ||
        pTableItem->getItemType() != MONSTER_CORPSE ||
        (!pTableItem->isFlag(Effect::EFFECT_CLASS_SHRINE_GUARD) &&
         !pTableItem->isFlag(Effect::EFFECT_CLASS_SHRINE_HOLY))) {
        GCCannotAdd _GCCannotAdd;
        _GCCannotAdd.setObjectID(pPacket->getObjectID());
        pPlayer->sendPacket(&_GCCannotAdd);

        return;
    }

    // Shrine
    MonsterCorpse* pCorpse = dynamic_cast<MonsterCorpse*>(pTableItem);
    Assert(pCorpse != NULL);

    // Not within 2 tiles, or
    // not set up as a shrine
    if (!verifyDistance(pCreature, pCorpse->getX(), pCorpse->getY(), 2) || !pCorpse->isShrine()) {
        GCCannotAdd _GCCannotAdd;
        _GCCannotAdd.setObjectID(pPacket->getObjectID());
        pPlayer->sendPacket(&_GCCannotAdd);

        return;
    }

    if (de::gameContext().shrines().putBloodBible(pPlayerCreature, pItem, pCorpse)) {
        // Handled inside putBloodBible.
    }

#endif

    __END_CATCH
}

void CGRelicToObjectHandler::executeCastleSymbol(CGRelicToObject* pPacket, Player* pPlayer)

{
    __BEGIN_TRY

#ifdef __GAME_SERVER__

    GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(pPlayer);
    Creature* pCreature = pGamePlayer->getCreature();
    Zone* pZone = pCreature->getZone();
    Assert(pZone != NULL);

    // When the relic table already holds both
    // When the relic table holds a Slayer relic and pItem is a Slayer relic
    PlayerCreature* pPlayerCreature = dynamic_cast<PlayerCreature*>(pCreature);

    // Is the item currently held == Relic ?
    InventorySlot* pExtraInventorySlot = pPlayerCreature->getExtraInventorySlot();
    Item* pItem = pExtraInventorySlot->getItem();


    Item* pTableItem = pZone->getItem(pPacket->getObjectID());


    // No such item, or
    // not a corpse, or
    // Not a Monster corpse, or
    // neither a ShrineGuard nor a ShrineHoly: then it is not a shrine.
    if (pTableItem == NULL || pTableItem->getItemClass() != Item::ITEM_CLASS_CORPSE ||
        pTableItem->getItemType() != MONSTER_CORPSE ||
        (!pTableItem->isFlag(Effect::EFFECT_CLASS_CASTLE_SHRINE_GUARD) &&
         !pTableItem->isFlag(Effect::EFFECT_CLASS_CASTLE_SHRINE_HOLY))) {
        GCCannotAdd _GCCannotAdd;
        _GCCannotAdd.setObjectID(pPacket->getObjectID());
        pPlayer->sendPacket(&_GCCannotAdd);

        return;
    }

    // Shrine
    MonsterCorpse* pCorpse = dynamic_cast<MonsterCorpse*>(pTableItem);
    Assert(pCorpse != NULL);

    // Not within 2 tiles, or
    // not set up as a shrine
    if (!verifyDistance(pCreature, pCorpse->getX(), pCorpse->getY(), 2) || !pCorpse->isShrine()) {
        GCCannotAdd _GCCannotAdd;
        _GCCannotAdd.setObjectID(pPacket->getObjectID());
        pPlayer->sendPacket(&_GCCannotAdd);

        return;
    }

    cout << "siegeManager Call" << endl;
    SiegeManager::Instance().putItem(pPlayerCreature, pCorpse, pItem);
    return;

#endif

    __END_CATCH
}

void CGRelicToObjectHandler::executeFlag(CGRelicToObject* pPacket, Player* pPlayer)

{
    __BEGIN_TRY

#ifdef __GAME_SERVER__

    if (!de::gameContext().flags().hasFlagWar())
        return;

    GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(pPlayer);
    Creature* pCreature = pGamePlayer->getCreature();
    Zone* pZone = pCreature->getZone();
    Assert(pZone != NULL);

    // When the relic table already holds both
    // When the relic table holds a Slayer relic and pItem is a Slayer relic
    PlayerCreature* pPlayerCreature = dynamic_cast<PlayerCreature*>(pCreature);

    // Is the item currently held == Flag ?
    InventorySlot* pExtraInventorySlot = pPlayerCreature->getExtraInventorySlot();
    Item* pItem = pExtraInventorySlot->getItem();

    Item* pTableItem = pZone->getItem(pPacket->getObjectID());

    // No such item, or
    // not a corpse, or
    // Not a Monster corpse, or
    // Not a flagpole
    if (pTableItem == NULL || pTableItem->getItemClass() != Item::ITEM_CLASS_CORPSE ||
        pTableItem->getItemType() != MONSTER_CORPSE ||
        !de::gameContext().flags().isFlagPole(dynamic_cast<MonsterCorpse*>(pTableItem))) {
        GCCannotAdd _GCCannotAdd;
        _GCCannotAdd.setObjectID(pPacket->getObjectID());
        pPlayer->sendPacket(&_GCCannotAdd);

        return;
    }

    // Shrine
    MonsterCorpse* pCorpse = dynamic_cast<MonsterCorpse*>(pTableItem);
    Assert(pCorpse != NULL);

    // Not within 2 tiles, or
    // not set up as a shrine
    if (!verifyDistance(pCreature, pCorpse->getX(), pCorpse->getY(), 2) || !pCorpse->isShrine()) {
        GCCannotAdd _GCCannotAdd;
        _GCCannotAdd.setObjectID(pPacket->getObjectID());
        pPlayer->sendPacket(&_GCCannotAdd);

        return;
    }

    if (de::gameContext().flags().putFlag(pPlayerCreature, pItem, pCorpse)) {
        // Handled inside putCastleSymbol.
    } else {
        GCCannotAdd _GCCannotAdd;
        _GCCannotAdd.setObjectID(pPacket->getObjectID());
        pPlayer->sendPacket(&_GCCannotAdd);

        return;
    }

#endif

    __END_CATCH
}

void CGRelicToObjectHandler::executeSweeper(CGRelicToObject* pPacket, Player* pPlayer)

{
    __BEGIN_TRY

    StringPool& strings = de::gameContext().strings();

#ifdef __GAME_SERVER__

    GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(pPlayer);
    Creature* pCreature = pGamePlayer->getCreature();
    Zone* pZone = pCreature->getZone();
    Assert(pZone != NULL);

    LevelWarManager* pLevelWarManager = pZone->getLevelWarManager();
    Assert(pLevelWarManager != NULL);

    if (!pLevelWarManager->hasWar())
        return;

    PlayerCreature* pPlayerCreature = dynamic_cast<PlayerCreature*>(pCreature);

    InventorySlot* pExtraInventorySlot = pPlayerCreature->getExtraInventorySlot();
    Item* pItem = pExtraInventorySlot->getItem();

    Item* pTableItem = pZone->getItem(pPacket->getObjectID());

    const SweeperInfo* pSweeperInfo = dynamic_cast<SweeperInfo*>(
        de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_SWEEPER, pItem->getItemType()));

    // No such item, or
    // not a corpse, or
    // Not a Monster corpse, or
    // Not a flagpole
    if (pTableItem == NULL || pTableItem->getItemClass() != Item::ITEM_CLASS_CORPSE ||
        pTableItem->getItemType() != MONSTER_CORPSE ||
        !pLevelWarManager->isSafe(dynamic_cast<MonsterCorpse*>(pTableItem))) {
        GCCannotAdd _GCCannotAdd;
        _GCCannotAdd.setObjectID(pPacket->getObjectID());
        pPlayer->sendPacket(&_GCCannotAdd);

        return;
    }

    // Shrine
    MonsterCorpse* pCorpse = dynamic_cast<MonsterCorpse*>(pTableItem);
    Assert(pCorpse != NULL);

    // Not within 2 tiles, or
    // not set up as a shrine
    if (!verifyDistance(pCreature, pCorpse->getX(), pCorpse->getY(), 2)) {
        GCCannotAdd _GCCannotAdd;
        _GCCannotAdd.setObjectID(pPacket->getObjectID());
        pPlayer->sendPacket(&_GCCannotAdd);

        return;
    }

    if (pLevelWarManager->putSweeper(pPlayerCreature, pItem, pCorpse)) {
        // Once the Sweeper is planted, the one held is erased
        pPlayerCreature->deleteItemFromExtraInventorySlot();
        GCDeleteInventoryItem gcDeleteInventoryItem;
        gcDeleteInventoryItem.setObjectID(pPacket->getItemObjectID());
        pPlayerCreature->getPlayer()->sendPacket(&gcDeleteInventoryItem);
        Effect* pEffect = pPlayerCreature->findEffect(Effect::EFFECT_CLASS_HAS_SWEEPER);

        if (pEffect != NULL) {
            pEffect->setDeadline(0);
        }

        // Broadcast a system message to the zone when it is planted
        char race[15];
        if (pCreature->isSlayer()) {
            snprintf(race, sizeof(race), strings.c_str(STRID_SLAYER));
        } else if (pCreature->isVampire()) {
            snprintf(race, sizeof(race), strings.c_str(STRID_VAMPIRE));
        } else if (pCreature->isOusters()) {
            snprintf(race, sizeof(race), strings.c_str(STRID_OUSTERS));
        } else {
            Assert(false);
        }

        char msg[100];

        snprintf(msg, sizeof(msg), strings.c_str(STRID_PUT_SWEEPER), pCreature->getName().c_str(), race,
                 pSweeperInfo->getName().c_str());
        GCSystemMessage gcSystemMessage;
        gcSystemMessage.setMessage(msg);
        pZone->broadcastPacket(&gcSystemMessage);
    } else {
        GCCannotAdd _GCCannotAdd;
        _GCCannotAdd.setObjectID(pPacket->getObjectID());
        pPlayer->sendPacket(&_GCCannotAdd);

        return;
    }

#endif

    __END_CATCH
}
