//////////////////////////////////////////////////////////////////////////////
// Filename    : CGThrowBombHandler.cc
// Written By  : elca@ewestsoft.com
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "CGThrowBomb.h"
#include "GameContext.h"

#ifdef __GAME_SERVER__
#include <list>

#include "GCSkillFailed1.h"
#include "GCSkillFailed2.h"
#include "GCStatusCurrentHP.h"
#include "GCThrowBombOK1.h"
#include "GCThrowBombOK2.h"
#include "GCThrowBombOK3.h"
#include "GamePlayer.h"
#include "ItemUtil.h"
#include "SkillUtil.h"
#include "Slayer.h"
#include "ZoneUtil.h"
#include "item/Bomb.h"
#include "skill/Sniping.h"
#endif // __GAME_SERVER__

#ifdef __GAME_SERVER__

// type
// direction
// mask
int BombMask[5][9][9] = {
    // type 0 bomb
    {{0, 0, 0, 0, 100, 0, 0, 0, 0},
     {0, 0, 0, 0, 100, 0, 0, 0, 0},
     {0, 0, 0, 0, 100, 0, 0, 0, 0},
     {0, 0, 0, 0, 100, 0, 0, 0, 0},
     {0, 0, 0, 0, 100, 0, 0, 0, 0},
     {0, 0, 0, 0, 100, 0, 0, 0, 0},
     {0, 0, 0, 0, 100, 0, 0, 0, 0},
     {0, 0, 0, 0, 100, 0, 0, 0, 0},
     {0, 0, 0, 0, 0, 0, 0, 0, 0}},
    // type 1 bomb
    {{0, 0, 0, 0, 100, 0, 0, 0, 0},
     {0, 0, 0, 0, 100, 0, 0, 0, 0},
     {0, 0, 0, 0, 100, 0, 0, 0, 0},
     {0, 0, 0, 0, 100, 0, 0, 0, 0},
     {0, 0, 0, 0, 100, 0, 0, 0, 0},
     {0, 0, 0, 0, 100, 0, 0, 0, 0},
     {0, 0, 0, 0, 100, 0, 0, 0, 0},
     {0, 0, 0, 0, 100, 0, 0, 0, 0},
     {0, 0, 0, 0, 0, 0, 0, 0, 0}},
    // type 2 bomb
    {{0, 0, 0, 0, 100, 0, 0, 0, 0},
     {0, 0, 0, 0, 100, 0, 0, 0, 0},
     {0, 0, 0, 0, 100, 0, 0, 0, 0},
     {0, 0, 0, 0, 100, 0, 0, 0, 0},
     {0, 0, 0, 0, 100, 0, 0, 0, 0},
     {0, 0, 0, 0, 100, 0, 0, 0, 0},
     {0, 0, 0, 0, 100, 0, 0, 0, 0},
     {0, 0, 0, 0, 100, 0, 0, 0, 0},
     {0, 0, 0, 0, 0, 0, 0, 0, 0}},
    // type 3 bomb
    {{0, 50, 0, 0, 100, 0, 0, 50, 0},
     {50, 0, 0, 0, 100, 0, 0, 0, 50},
     {0, 0, 0, 50, 100, 50, 0, 0, 0},
     {0, 0, 50, 0, 100, 0, 50, 0, 0},
     {0, 50, 0, 0, 100, 0, 0, 50, 0},
     {50, 0, 0, 0, 100, 0, 0, 0, 50},
     {0, 0, 0, 50, 100, 50, 0, 0, 0},
     {0, 0, 50, 0, 100, 0, 50, 0, 0},
     {0, 0, 0, 0, 0, 0, 0, 0, 0}},
    // type 4 bomb
    {{0, 50, 0, 50, 100, 50, 0, 50, 0},
     {50, 0, 50, 0, 100, 0, 50, 0, 50},
     {0, 50, 0, 50, 100, 50, 0, 50, 0},
     {50, 0, 50, 0, 100, 0, 50, 0, 50},
     {0, 50, 0, 50, 100, 50, 0, 50, 0},
     {50, 0, 50, 0, 100, 0, 50, 0, 50},
     {0, 50, 0, 50, 100, 50, 0, 50, 0},
     {50, 0, 50, 0, 100, 0, 50, 0, 50},
     {0, 0, 0, 0, 0, 0, 0, 0, 0}}};

#endif

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void CGThrowBombHandler::execute(CGThrowBomb* pPacket, Player* pPlayer)

{
    __BEGIN_DEBUG_EX

#ifdef __GAME_SERVER__

        Assert(pPacket != NULL);
    Assert(pPlayer != NULL);

    // Just return if the game player's state is not normal.
    GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(pPlayer);
    if (pGamePlayer->getPlayerStatus() != GPS_NORMAL)
        return;

    Creature* pCreature = pGamePlayer->getCreature();
    Assert(pCreature != NULL);

    // Just return if the player is not a Slayer.
    if (!pCreature->isSlayer())
        return;

    Slayer* pSlayer = dynamic_cast<Slayer*>(pCreature);

    Zone* pZone = pCreature->getZone();
    Assert(pZone != NULL);

    int ZoneX = pPacket->getZoneX();
    int ZoneY = pPacket->getZoneY();
    int BombX = pPacket->getBombX();
    int BombY = pPacket->getBombY();

    Inventory* pInventory = pSlayer->getInventory();
    Assert(pInventory != NULL);


    // A complete safe zone forbids skill use.
    ZoneLevel_t ZoneLevel = pZone->getZoneLevel(pSlayer->getX(), pSlayer->getY());
    if ((ZoneLevel & COMPLETE_SAFE_ZONE) || (BombX >= pInventory->getWidth() || BombY >= pInventory->getHeight())) {
        GCSkillFailed1 _GCSkillFailed1;
        _GCSkillFailed1.setSkillType(SKILL_THROW_BOMB);
        pPlayer->sendPacket(&_GCSkillFailed1);
        return;
    }

    ObjectID_t slayerObjectID = pCreature->getObjectID();

    bool bAttackSlayer = (pPacket->getAttackSlayerFlag() != 0) ? true : false;

    try {
        if (pCreature->isFlag(Effect::EFFECT_CLASS_SNIPING_MODE)) {
            g_Sniping.checkRevealRatio(pCreature, 20, 10);
        }

        Item* pItem = pInventory->getItem(BombX, BombY);

        // If the item is null or not a bomb...
        if (pItem == NULL || pItem->getItemClass() != Item::ITEM_CLASS_BOMB) {
            GCSkillFailed1 _GCSkillFailed1;
            pPlayer->sendPacket(&_GCSkillFailed1);
            return;
        }


        // Get the bomb's various information.
        Bomb* pBomb = dynamic_cast<Bomb*>(pItem);
        ItemType_t BombType = pBomb->getItemType();
        Damage_t MinDamage = pBomb->getMinDamage();
        Damage_t MaxDamage = pBomb->getMaxDamage();
        SkillSlot* pSkillSlot = pSlayer->hasSkill(SKILL_THROW_BOMB);
        SkillInfo* pSkillInfo = de::gameContext().skillInfos().getSkillInfo(SKILL_THROW_BOMB);
        Level_t SkillLevel = pSkillSlot->getExpLevel();
        Damage_t RealDamage = MinDamage + (max(0, ((int)MaxDamage * (int)SkillLevel / 100) - MinDamage));

        // Compute the direction from the user's position and the throw position.
        ZoneCoord_t myX = pSlayer->getX();
        ZoneCoord_t myY = pSlayer->getY();
        Dir_t dir = calcDirection(myX, myY, ZoneX, ZoneY);
        int count = 0;
        bool bHit = false;
        bool bRaceCheck = true;
        ;

        list<Creature*> cList;
        GCThrowBombOK1 _GCThrowBombOK1;
        GCThrowBombOK2 _GCThrowBombOK2;
        GCThrowBombOK3 _GCThrowBombOK3;

        int RequiredMP = (int)pSkillInfo->getConsumeMP();
        bool bManaCheck = hasEnoughMana(pSlayer, RequiredMP);
        bool bTimeCheck = verifyRunTime(pSkillSlot);
        bool bRangeCheck = verifyDistance(pSlayer, ZoneX, ZoneY, 6);

        // Hit or not, the bomb count has to go down.
        decreaseItemNum(pBomb, pInventory, pSlayer->getName(), STORAGE_INVENTORY, 0, BombX, BombY);

        if (bManaCheck && bTimeCheck && bRangeCheck) {
        } else {
            GCSkillFailed1 _GCSkillFailed1;
            pPlayer->sendPacket(&_GCSkillFailed1);
            return;
        }

        decreaseMana(pSlayer, RequiredMP, _GCThrowBombOK1);

        // Walk each tile and do the hit check.
        for (int tileY = ZoneY - 1; tileY <= ZoneY + 1; tileY++) {
            for (int tileX = ZoneX - 1; tileX <= ZoneX + 1; tileX++, count++) {
                // continue when the zone coordinates are invalid
                if (!isValidZoneCoord(pZone, tileX, tileY))
                    continue;

                // continue when the mask is 0, that is, when no hit roll is needed
                int Mask = BombMask[BombType][dir][count];
                if (Mask == 0)
                    continue;

                Tile& tile = pZone->getTile(tileX, tileY);

                checkMine(pZone, tileX, tileY);

                list<Creature*> targetList;
                if (tile.hasCreature(Creature::MOVE_MODE_WALKING)) {
                    Creature* pCreature = tile.getCreature(Creature::MOVE_MODE_WALKING);

                    // NoSuch removed.
                    if (pCreature != NULL && !pCreature->isFlag(Effect::EFFECT_CLASS_COMA))
                        targetList.push_back(pCreature);
                }
                if (tile.hasCreature(Creature::MOVE_MODE_BURROWING)) {
                    Creature* pCreature = tile.getCreature(Creature::MOVE_MODE_BURROWING);

                    // NoSuch removed.
                    if (pCreature != NULL && !pCreature->isFlag(Effect::EFFECT_CLASS_COMA))
                        targetList.push_back(pCreature);
                }

                list<Creature*>::iterator itr = targetList.begin();
                for (; itr != targetList.end(); itr++) {
                    Creature* pTargetCreature = (*itr);
                    Assert(pTargetCreature != NULL);

                    bool bHitRoll = true;
                    if (pTargetCreature->isSlayer() && !bAttackSlayer)
                        bHitRoll = false;
                    else if (pTargetCreature->isNPC())
                        bHitRoll = false;

                    if (!canAttack(pSlayer, pTargetCreature))
                        continue;

                    if (!bHitRoll)
                        continue;
                    bool bPK = verifyPK(pSlayer, pTargetCreature);
                    bool bZoneLevelCheck = checkZoneLevelToHitTarget(pTargetCreature);

                    // When the user is the target, try not to raise experience.
                    // This is because a punching bag could be made.
                    if (tileX == ZoneX && tileY == ZoneY) {
                        if (pTargetCreature->isSlayer()) {
                            bRaceCheck = false;
                        }
                    }

                    if (HitRoll::isSuccess(pSlayer, pTargetCreature, 50) && bPK && bZoneLevelCheck) {
                        Damage_t Damage = getPercentValue(RealDamage, Mask);
                        setDamage(pTargetCreature, Damage, pSlayer, 0, NULL, &_GCThrowBombOK1);
                        computeAlignmentChange(pTargetCreature, Damage, pSlayer, NULL, &_GCThrowBombOK1);
                        increaseAlignment(pSlayer, pTargetCreature, _GCThrowBombOK1);

                        bHit = true;

                        cList.push_back(pTargetCreature);
                        ObjectID_t targetObjectID = pTargetCreature->getObjectID();
                        _GCThrowBombOK1.addCListElement(targetObjectID);
                        _GCThrowBombOK2.addCListElement(targetObjectID);
                        _GCThrowBombOK3.addCListElement(targetObjectID);
                    }
                }
            }
        } // for (int tileY = ZoneY-1; tileY <= ZoneY+1; tileY++)

        // Every tile is checked, so raise the experience a little and lower the item count.
        if (bHit && bRaceCheck) {
            shareAttrExp(pSlayer, RealDamage, 1, 8, 1, _GCThrowBombOK1);
            increaseDomainExp(pSlayer, SKILL_DOMAIN_GUN, pSkillInfo->getPoint(), _GCThrowBombOK1);

            // Skill experience is only raised when the skill can be used.
            // 2003. 1. 12 by bezz
            if (pSkillSlot->canUse())
                increaseSkillExp(pSlayer, SKILL_DOMAIN_GUN, pSkillSlot, pSkillInfo, _GCThrowBombOK1);
            // A packet should be sent here..
        }

        _GCThrowBombOK1.setXYDir(ZoneX, ZoneY, dir);
        _GCThrowBombOK1.setItemType(BombType);
        _GCThrowBombOK2.setXYDir(ZoneX, ZoneY, dir);
        _GCThrowBombOK2.setObjectID(slayerObjectID);
        _GCThrowBombOK2.setItemType(BombType);
        _GCThrowBombOK3.setXYDir(ZoneX, ZoneY, dir);
        _GCThrowBombOK3.setObjectID(slayerObjectID);
        _GCThrowBombOK3.setItemType(BombType);

        for (list<Creature*>::const_iterator itr = cList.begin(); itr != cList.end(); itr++) {
            Creature* pTargetCreature = *itr;
            _GCThrowBombOK2.clearList();

            HP_t targetHP = 0;
            if (pTargetCreature->isSlayer()) {
                targetHP = (dynamic_cast<Slayer*>(pTargetCreature))->getHP();
            } else if (pTargetCreature->isVampire()) {
                targetHP = (dynamic_cast<Vampire*>(pTargetCreature))->getHP();
            } else if (pTargetCreature->isOusters()) {
                targetHP = (dynamic_cast<Ousters*>(pTargetCreature))->getHP();
            }

            _GCThrowBombOK2.addShortData(MODIFY_CURRENT_HP, targetHP);

            // Lower the item's durability.
            decreaseDurability(NULL, pTargetCreature, pSkillInfo, NULL, &_GCThrowBombOK2);


            if (pTargetCreature->isPC()) {
                pTargetCreature->getPlayer()->sendPacket(&_GCThrowBombOK2);
            }
        }

        pPlayer->sendPacket(&_GCThrowBombOK1);

        cList.push_back(pSlayer);

        pZone->broadcastPacket(ZoneX, ZoneY, &_GCThrowBombOK3, cList);
    } catch (Throwable& t) {
        GCSkillFailed1 _GCSkillFailed1;
        GCSkillFailed2 failpkt2;

        failpkt2.setObjectID(slayerObjectID);
        failpkt2.setSkillType(SKILL_THROW_BOMB);
        pZone->broadcastPacket(ZoneX, ZoneY, &failpkt2, pSlayer);

        pPlayer->sendPacket(&_GCSkillFailed1);
    }


#endif // __GAME_SERVER__

    __END_DEBUG_EX
}
