///////////////////////////////////////////////////////////////////
// CorpseItemPosition class implementation
///////////////////////////////////////////////////////////////////

#include "CorpseItemPosition.h"

#include <list>

#include "Assert.h"
#include "Corpse.h"
#include "GCAddEffectToTile.h"
#include "GameContext.h"
#include "Item.h"
#include "ItemFactoryManager.h"
#include "ItemUtil.h"
#include "MonsterCorpse.h"
#include "RelicUtil.h"
#include "Tile.h"
#include "Utility.h"
#include "Zone.h"
#include "ZoneGroup.h"
#include "ZoneUtil.h"
#include "ctf/FlagManager.h"

Item* CorpseItemPosition::popItem(bool bLock)

{
    m_pZone = NULL;

    if (bLock)
        return getItem_UNLOCKED(true);
    return getItem_LOCKED(true);
}

Item* CorpseItemPosition::getItem_LOCKED(bool isDel)

{
    Zone* pZone = getZoneByZoneID(m_ZoneID);
    m_pZone = pZone;

    if (pZone == NULL) {
        filelog("ItemError.log", "CorpseItemPosition:getItem() : Invalid ZoneID.");
        return NULL;
    }

    return getItem_CORE(pZone, isDel);
}

Item* CorpseItemPosition::getItem_UNLOCKED(bool isDel)

{
    Zone* pZone = getZoneByZoneID(m_ZoneID);
    m_pZone = pZone;

    if (pZone == NULL) {
        filelog("ItemError.log", "CorpseItemPosition:getItem() : Invalid ZoneID.");
        return NULL;
    }

    Item* pItem;

    __ENTER_CRITICAL_SECTION((*pZone))

    pItem = getItem_CORE(pZone, isDel);

    __LEAVE_CRITICAL_SECTION((*pZone))

    return pItem;
}

Item* CorpseItemPosition::getItem_CORE(Zone* pZone, bool isDel)

{
    Corpse* pCorpse = dynamic_cast<Corpse*>(pZone->getItem(m_CorpseObjectID));
    Item* pRet = NULL;

    if (pCorpse == NULL || pCorpse->getItemClass() != Item::ITEM_CLASS_CORPSE) {
        filelog("ItemError.log", "CorpseItemPosition::getItem() : No matching corpse.");

        return NULL;
    }

    Item* pFound = pCorpse->getTreasure(m_ObjectID);
    if (pFound != NULL && !isExpectedItem(pFound->getItemClass(), pFound->getItemID())) {
        filelog("ItemError.log", "CorpseItemPosition::getItem() : another item is in the corpse under that object id");

        return NULL;
    }

    if (isDel) {
        pRet = pCorpse->popTreasure(m_ObjectID);

        if (pRet == NULL) {
            filelog("ItemError.log", "CorpseItemPosition::getItem() : The item is not in the corpse.");

            return NULL;
        }

        deleteFlagEffect(pCorpse, pRet);
        deleteRelicEffect(pCorpse, pRet);
        sendRelicWarpEffect(pCorpse);

        if (pCorpse->getItemType() == MONSTER_CORPSE) {
            MonsterCorpse* pMonsterCorpse = dynamic_cast<MonsterCorpse*>(pCorpse);
            if (pMonsterCorpse != NULL) {
                FlagManager& flags = de::gameContext().flags();
                if (flags.isFlagPole(pMonsterCorpse) &&
                    flags.getFlagPoleRace(pMonsterCorpse) == flags.getWinnerRace()) {
                    Item* pGemStone = de::gameContext().itemFactories().createItem(Item::ITEM_CLASS_QUEST_ITEM, 4,
                                                                                   list<OptionType_t>());
                    pZone->registerObject(pGemStone);
                    TPOINT tp = pZone->addItem(pGemStone, pMonsterCorpse->getX(), pMonsterCorpse->getY());

                    pGemStone->create("", STORAGE_ZONE, pZone->getZoneID(), tp.x, tp.y);
                    pGemStone->setFlag(
                        (Effect::EffectClass)(Effect::EFFECT_CLASS_SLAYER_ONLY + (int)flags.getWinnerRace()));

                    EffectID_t effectClass = Effect::EFFECT_CLASS_FIRE_CRACKER_VOLLEY_1 + (rand() & 0x3);

                    GCAddEffectToTile gcAddEffectToTile;
                    gcAddEffectToTile.setObjectID(pMonsterCorpse->getObjectID());
                    gcAddEffectToTile.setEffectID(effectClass);
                    gcAddEffectToTile.setXY(pMonsterCorpse->getX(), pMonsterCorpse->getY());
                    gcAddEffectToTile.setDuration(10); // no real meaning, just 1 second

                    pZone->broadcastPacket(pMonsterCorpse->getX(), pMonsterCorpse->getY(), &gcAddEffectToTile);
                }
            }
        }
    } else
        pRet = pCorpse->getTreasure(m_ObjectID);

    if (pRet == NULL) {
        filelog("ItemError.log", "CorpseItemPosition::getItem() : The item is not in the corpse.");

        return NULL;
    }

    return pRet;
}

string CorpseItemPosition::toString() const

{
    return "CorpseItemPosition";
}
