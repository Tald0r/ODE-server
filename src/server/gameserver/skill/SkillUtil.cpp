//////////////////////////////////////////////////////////////////////////////
// Filename    : SkillUtil.cpp
// Written by  : excel96
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "SkillUtil.h"

#include <math.h>
#include <stdio.h>

#include "AlignmentManager.h"
#include "CreatureUtil.h"
#include "DynamicZone.h"
#include "EffectAirShield.h"
#include "EffectAlignmentRecovery.h"
#include "EffectArmageddon.h"
#include "EffectAuraShield.h"
#include "EffectBlindness.h"
#include "EffectBlockHead.h"
#include "EffectCanEnterGDRLair.h"
#include "EffectDivineSpirits.h"
#include "EffectEnemyErase.h"
#include "EffectExpansion.h"
#include "EffectExplosionWater.h"
#include "EffectFrozenArmor.h"
#include "EffectGrandMasterOusters.h"
#include "EffectGrandMasterSlayer.h"
#include "EffectGrandMasterVampire.h"
#include "EffectHandsOfFire.h"
#include "EffectHymn.h"
#include "EffectIceFieldToCreature.h"
#include "EffectIceOfSoulStone.h"
#include "EffectInstallTurret.h"
#include "EffectMephisto.h"
#include "EffectPrecedence.h"
#include "EffectReactiveArmor.h"
#include "EffectRediance.h"
#include "EffectRequital.h"
#include "EffectRevealer.h"
#include "EffectShareHP.h"
#include "EffectSharpShield.h"
#include "EffectSleep.h"
#include "EffectStoneSkin.h"
#include "EffectStriking.h"
#include "EffectSwordOfThor.h"
#include "EffectWaterBarrier.h"
#include "EventHeadCount.h"
#include "EventItemUtil.h"
#include "GCAddEffect.h"
#include "GCAddEffectToTile.h"
#include "GCAddInjuriousCreature.h"
#include "GCKickMessage.h"
#include "GCLearnSkillReady.h"
#include "GCOtherModifyInfo.h"
#include "GCRemoveEffect.h"
#include "GCRemoveFromGear.h"
#include "GCSkillFailed1.h"
#include "GCSkillFailed2.h"
#include "GCSkillToObjectOK4.h"
#include "GCSkillToObjectOK6.h"
#include "GCStatusCurrentHP.h"
#include "GCSystemMessage.h"
#include "GDRLairManager.h"
#include "GQuestManager.h"
#include "GamePlayer.h"
#include "GameServerInfoManager.h"
#include "HitRoll.h"
#include "ItemFactoryManager.h"
#include "ItemUtil.h"
#include "KernelContext.h"
#include "MasterLairInfoManager.h"
#include "Monster.h"
#include "OustersEXPInfo.h"
#include "PKZoneInfoManager.h"
#include "PacketUtil.h"
#include "Party.h"
#include "Player.h"
#include "PrecedenceTable.h"
#include "Properties.h"
#include "ServerContext.h"
#include "SkillDomainInfoManager.h"
#include "SkillInfo.h"
#include "SkillPropertyManager.h"
#include "SkillUtilInternal.h"
#include "SummonGroundElemental.h"
#include "Thread.h"
#include "VampEXPInfo.h"
#include "VariableManager.h"
#include "Zone.h"
#include "ZoneUtil.h"
#include "domain/Formulas.h"
#include "domain/SkillRange.h"
#include "mission/EventQuestLootingManager.h"
#include "mission/MonsterKillQuestStatus.h"
#include "mission/QuestManager.h"


//////////////////////////////////////////////////////////////////////////////
// Computes the mana cost of a Vampire spell, which varies with INT.
//////////////////////////////////////////////////////////////////////////////
MP_t decreaseConsumeMP(Vampire* pVampire, SkillInfo* pSkillInfo) {
    Assert(pVampire != NULL);
    Assert(pSkillInfo != NULL);

    // The INT-discount bracket table lives in de-core.
    return decore::vampireSkillConsumeMP(pSkillInfo->getConsumeMP(), pSkillInfo->getLevel(), pVampire->getINT());
}


//////////////////////////////////////////////////////////////////////////////
// Does the caster have enough mana to use the skill?
//////////////////////////////////////////////////////////////////////////////
bool hasEnoughMana(Creature* pCaster, int RequiredMP) {
    if (pCaster->isSlayer()) {
        Slayer* pSlayer = dynamic_cast<Slayer*>(pCaster);

        int decreaseRatio = pSlayer->getConsumeMPRatio();
        if (decreaseRatio != 0) {
            RequiredMP += getPercentValue(RequiredMP, decreaseRatio);
        }

        // With Sacrifice active, HP can stand in when mana runs short.
        if (pSlayer->isFlag(Effect::EFFECT_CLASS_SACRIFICE)) {
            int margin = RequiredMP - pSlayer->getMP(ATTR_CURRENT);

            // When the requirement is above the current MP, the shortfall comes
            // out of HP, with each HP point covering two points of it.
            if (margin > 0) {
                margin = (int)pSlayer->getHP(ATTR_CURRENT) * 2 - (int)margin;
                if (margin > 0)
                    return true;
            } else {
                return true;
            }
        } else {
            if (pSlayer->getMP(ATTR_CURRENT) >= (MP_t)RequiredMP)
                return true;
        }
    } else if (pCaster->isVampire()) {
        Vampire* pVampire = dynamic_cast<Vampire*>(pCaster);

        // A Vampire's HP is also its MP, so spending mana
        // must not kill it. After a skill is used the
        // HP has to be at least 1, so > is used instead of >=.

        int decreaseRatio = pVampire->getConsumeMPRatio();
        if (decreaseRatio != 0) {
            RequiredMP += getPercentValue(RequiredMP, decreaseRatio);
        }

        if (pVampire->getHP(ATTR_CURRENT) > (HP_t)RequiredMP)
            return true;
    } else if (pCaster->isOusters()) {
        Ousters* pOusters = dynamic_cast<Ousters*>(pCaster);

        int decreaseRatio = pOusters->getConsumeMPRatio();
        if (decreaseRatio != 0) {
            RequiredMP += getPercentValue(RequiredMP, decreaseRatio);
        }

        if (pOusters->getMP(ATTR_CURRENT) >= (MP_t)RequiredMP)
            return true;
    } else if (pCaster->isMonster()) {
        // Monsters have unlimited mana.
        // A spell count for monsters may be added later.
        return true;
    } else {
        cerr << "hasEnoughMana() : Invalid Creature Class" << endl;
        Assert(false);
    }

    return false;
}


//////////////////////////////////////////////////////////////////////////////
// Reduces mana by the given number of points.
// For a Slayer carrying an effect such as Sacrifice,
// HP may be spent as well when mana runs short.
//////////////////////////////////////////////////////////////////////////////
int decreaseMana(Creature* pCaster, int MP, ModifyInfo& info) {
    Assert(pCaster != NULL);

    int RemainHP = 0;
    int RemainMP = 0;

    if (pCaster->isFlag(Effect::EFFECT_CLASS_NO_DAMAGE))
        return 0;

    if (pCaster->isSlayer()) {
        Slayer* pSlayer = dynamic_cast<Slayer*>(pCaster);

        // Magic Brain reduces MP consumption by its rank bonus percentage.
        if (pSlayer->hasRankBonus(RankBonus::RANK_BONUS_MAGIC_BRAIN)) {
            RankBonus* pRankBonus = pSlayer->getRankBonus(RankBonus::RANK_BONUS_MAGIC_BRAIN);
            Assert(pRankBonus != NULL);

            MP -= getPercentValue(MP, pRankBonus->getPoint());
        }

        // Applies the Blood Bible bonus.
        int decreaseRatio = pSlayer->getConsumeMPRatio();
        if (decreaseRatio != 0) {
            // The decreaseRatio value is itself negative.
            MP += getPercentValue(MP, decreaseRatio);
        }

        // With Sacrifice active, MP is drained first and HP covers the rest.
        if (pSlayer->isFlag(Effect::EFFECT_CLASS_SACRIFICE)) {
            int margin = (int)MP - (int)pSlayer->getMP(ATTR_CURRENT);

            // A margin above zero means the current MP is below the requirement.
            if (margin > 0) {
                // Drains the MP to zero...
                pSlayer->setMP(0, ATTR_CURRENT);
                // and drains HP as well.
                RemainHP = max(0, (int)(pSlayer->getHP(ATTR_CURRENT) - margin / 2));
                pSlayer->setHP(RemainHP, ATTR_CURRENT);

                info.addShortData(MODIFY_CURRENT_MP, pSlayer->getMP(ATTR_CURRENT));
                info.addShortData(MODIFY_CURRENT_HP, pSlayer->getHP(ATTR_CURRENT));
                return CONSUME_BOTH;
            }

            // The MP covers the cost, so it all comes out of MP.
            RemainMP = max(0, ((int)pSlayer->getMP(ATTR_CURRENT) - (int)MP));
            pSlayer->setMP(RemainMP, ATTR_CURRENT);

            info.addShortData(MODIFY_CURRENT_MP, pSlayer->getMP(ATTR_CURRENT));
            return CONSUME_MP;
        } else // Without Sacrifice, the cost simply comes out of MP.
        {
            RemainMP = max(0, ((int)pSlayer->getMP(ATTR_CURRENT) - (int)MP));
            pSlayer->setMP(RemainMP, ATTR_CURRENT);

            info.addShortData(MODIFY_CURRENT_MP, pSlayer->getMP(ATTR_CURRENT));
            return CONSUME_MP;
        }
    } else if (pCaster->isVampire()) {
        Vampire* pVampire = dynamic_cast<Vampire*>(pCaster);

        // Wisdom of Blood reduces HP consumption by its rank bonus percentage.
        if (pVampire->hasRankBonus(RankBonus::RANK_BONUS_WISDOM_OF_BLOOD)) {
            RankBonus* pRankBonus = pVampire->getRankBonus(RankBonus::RANK_BONUS_WISDOM_OF_BLOOD);
            Assert(pRankBonus != NULL);

            MP -= getPercentValue(MP, pRankBonus->getPoint());
        }

        // Applies the Blood Bible bonus.
        int decreaseRatio = pVampire->getConsumeMPRatio();
        if (decreaseRatio != 0) {
            // The decreaseRatio value is itself negative.
            MP += getPercentValue(MP, decreaseRatio);
        }

        HP_t currentHP = pVampire->getHP(ATTR_CURRENT);
        RemainHP = max(0, ((int)currentHP - (int)MP));
        pVampire->setHP(RemainHP, ATTR_CURRENT);

        // The Mephisto effect is cleared once HP drops low.
        if (pVampire->isFlag(Effect::EFFECT_CLASS_MEPHISTO)) {
            HP_t maxHP = pVampire->getHP(ATTR_MAX);

            // A third, not 30%.
            if (currentHP * 3 < maxHP) {
                Effect* pEffect = pVampire->findEffect(Effect::EFFECT_CLASS_MEPHISTO);
                if (pEffect != NULL) {
                    pEffect->setDeadline(0);
                } else {
                    pVampire->removeFlag(Effect::EFFECT_CLASS_MEPHISTO);
                }
            }
        }

        info.addShortData(MODIFY_CURRENT_HP, pVampire->getHP(ATTR_CURRENT));
        return CONSUME_HP;
    } else if (pCaster->isOusters()) {
        Ousters* pOusters = dynamic_cast<Ousters*>(pCaster);

        // Applies the Blood Bible bonus.
        int decreaseRatio = pOusters->getConsumeMPRatio();
        if (decreaseRatio != 0) {
            // The decreaseRatio value is itself negative.
            MP += getPercentValue(MP, decreaseRatio);
        }

        RemainMP = max(0, ((int)pOusters->getMP(ATTR_CURRENT) - (int)MP));
        pOusters->setMP(RemainMP, ATTR_CURRENT);

        info.addShortData(MODIFY_CURRENT_MP, pOusters->getMP(ATTR_CURRENT));
        return CONSUME_MP;
    } else if (pCaster->isMonster()) {
        // Monsters have unlimited mana.
        // A spell count for monsters may be added later.
        cerr << "decreaseMana() : Monster don't have Mana" << endl;
        Assert(false);
    } else {
        cerr << "hasEnoughMana() : Invalid Creature Class" << endl;
        Assert(false);
    }

    return CONSUME_MP;
}

//////////////////////////////////////////////////////////////////////////////
// Computes the range of a Slayer skill: the skill table's min and max
// range and the slot's proficiency level, through de-core's skillRange
// (src/domain/SkillRange.h), the rule the client computes too.
//////////////////////////////////////////////////////////////////////////////
Range_t computeSkillRange(SkillSlot* pSkillSlot, SkillInfo* pSkillInfo) {
    Assert(pSkillSlot != NULL);
    Assert(pSkillInfo != NULL);

    return decore::skillRange((int)pSkillInfo->getMinRange(), (int)pSkillInfo->getMaxRange(),
                              pSkillSlot->getExpLevel());
}


//////////////////////////////////////////////////////////////////////////////
// Verifies the run time of a Slayer skill.
//////////////////////////////////////////////////////////////////////////////
bool verifyRunTime(SkillSlot* pSkillSlot) {
    Assert(pSkillSlot != NULL);

    Timeval CurrentTime;
    Timeval LastTime = pSkillSlot->getRunTime();

    getCurrentTime(CurrentTime);

    if (CurrentTime > LastTime)
        return true;

    return false;
}


//////////////////////////////////////////////////////////////////////////////
// Verifies the run time of a Vampire skill.
//////////////////////////////////////////////////////////////////////////////
bool verifyRunTime(VampireSkillSlot* pSkillSlot) {
    Assert(pSkillSlot != NULL);

    Timeval CurrentTime;
    Timeval LastTime = pSkillSlot->getRunTime();

    getCurrentTime(CurrentTime);

    if (CurrentTime > LastTime)
        return true;

    return false;
}


//////////////////////////////////////////////////////////////////////////////
// Verifies the run time of an Ousters skill.
//////////////////////////////////////////////////////////////////////////////
bool verifyRunTime(OustersSkillSlot* pSkillSlot) {
    Assert(pSkillSlot != NULL);

    Timeval CurrentTime;
    Timeval LastTime = pSkillSlot->getRunTime();

    getCurrentTime(CurrentTime);

    if (CurrentTime > LastTime)
        return true;

    return false;
}


//////////////////////////////////////////////////////////////////////////////
// Decides whether PK is allowed, following each zone's PK policy.
//////////////////////////////////////////////////////////////////////////////
bool verifyPK(Creature* pAttacker, Creature* pDefender) {
    Zone* pZone = pDefender->getZone();
    Assert(pZone != NULL);

    if (pDefender != NULL && pAttacker != NULL) {
        if (pZone->getZoneID() == 1412 || pZone->getZoneID() == 1413) {
            if (pDefender->isPC() && pAttacker->isPC())
                return false;
        }

        if (pDefender->getCreatureClass() == pAttacker->getCreatureClass() && pAttacker->isPC()) {
            // A zone level that forbids PK blocks the attack.
            if (pZone->getZoneLevel() == NO_PK_ZONE)
                return false;

            // Members of the same party cannot attack each other.
            int PartyID1 = pAttacker->getPartyID();
            int PartyID2 = pDefender->getPartyID();
            if (PartyID1 != 0 && PartyID1 == PartyID2)
                return false;

            if (pDefender->isFlag(Effect::EFFECT_CLASS_HAS_FLAG) ||
                pDefender->isFlag(Effect::EFFECT_CLASS_HAS_SWEEPER)) {
                if (pAttacker->isPC() && !dynamic_cast<PlayerCreature*>(pAttacker)->hasEnemy(pDefender->getName()))
                    return false;
            }
        }
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////////
// Is this a zone where the skill may be used?
// (For a self skill, this checks the zone level.)
//////////////////////////////////////////////////////////////////////////////
bool checkZoneLevelToUseSkill(Creature* pCaster) {
    Assert(pCaster != NULL);

    if (pCaster->isFlag(Effect::EFFECT_CLASS_PLEASURE_EXPLOSION))
        return false;

    Zone* pZone = pCaster->getZone();
    Assert(pZone != NULL);

    ZoneCoord_t cx = pCaster->getX();
    ZoneCoord_t cy = pCaster->getY();
    ZoneLevel_t ZoneLevel = pZone->getZoneLevel(cx, cy);

    // Self skills cannot be used in a safe zone.
    if ((ZoneLevel & SAFE_ZONE)) // && pZone->isHolyLand() )
        return false;

    if (pCaster->isFlag(Effect::EFFECT_CLASS_REFINIUM_TICKET))
        return false;

    return true;
}

//////////////////////////////////////////////////////////////////////////////
// Checks whether the creature standing at X, Y can be affected by a skill.
//////////////////////////////////////////////////////////////////////////////
bool checkZoneLevelToHitTarget(Creature* pTargetCreature) {
    Assert(pTargetCreature != NULL);

    Zone* pZone = pTargetCreature->getZone();
    Assert(pZone != NULL);

    ZoneCoord_t tx = pTargetCreature->getX();
    ZoneCoord_t ty = pTargetCreature->getY();
    ZoneLevel_t ZoneLevel = pZone->getZoneLevel(tx, ty);

    // A Slayer is not hit by skills in a Slayer safe zone.
    if ((ZoneLevel & SLAYER_SAFE_ZONE) && pTargetCreature->isSlayer())
        return false;
    // A Vampire is not hit by skills in a Vampire safe zone.
    else if ((ZoneLevel & VAMPIRE_SAFE_ZONE) && pTargetCreature->isVampire())
        return false;
    // Ousters are not hit by skills in an Ousters safe zone.
    else if ((ZoneLevel & OUSTERS_SAFE_ZONE) && pTargetCreature->isOusters())
        return false;
    // Nobody is hit in a complete safe zone.
    else if (ZoneLevel & COMPLETE_SAFE_ZONE)
        return false;

    return true;
}


//////////////////////////////////////////////////////////////////////////////
// Sends a packet when a skill fails.
// For an ordinary failure (a missed hit roll, not enough mana, and so on),
// the packet goes to the caster and to everyone who can see it.
//////////////////////////////////////////////////////////////////////////////
void executeSkillFailNormal(Creature* pCreature, SkillType_t SkillType, Creature* pTargetCreature, BYTE Grade) {
    Assert(pCreature != NULL);

    if (pCreature->isPC()) {
        GCSkillFailed1 gcSkillFailed1;
        gcSkillFailed1.setSkillType(SkillType);
        gcSkillFailed1.setGrade(Grade);
        (pCreature->getPlayer())->sendPacket(&gcSkillFailed1);
    }

    GCSkillFailed2 gcSkillFailed2;
    gcSkillFailed2.setSkillType(SkillType);
    gcSkillFailed2.setObjectID(pCreature->getObjectID());
    gcSkillFailed2.setGrade(Grade);

    // For an object skill the target's object id, when present, is carried in the packet.
    // For a self or tile skill NULL is normally passed instead.
    // (On a GCSkillFailed2 sent for a failed self or tile skill the client
    // does not even read TargetObjectID.)
    if (pTargetCreature != NULL) {
        gcSkillFailed2.setTargetObjectID(pTargetCreature->getObjectID());
    } else {
        gcSkillFailed2.setTargetObjectID(0);
    }

    Zone* pZone = pCreature->getZone();
    Assert(pZone != NULL);

    pZone->broadcastPacket(pCreature->getX(), pCreature->getY(), &gcSkillFailed2, pCreature);
}

//////////////////////////////////////////////////////////////////////////////
// Sends a packet when a skill fails.
// The skill result has to be sent twice:
// once for creating the larva,
// once for the soul absorption.
// So when the opening condition check fails,
// the SkillFail packet is sent twice.
//////////////////////////////////////////////////////////////////////////////
void executeAbsorbSoulSkillFail(Creature* pCreature, SkillType_t SkillType, ObjectID_t TargetObjectID, bool bBroadcast,
                                bool bSendTwice) {
    Assert(pCreature != NULL);

    // While the client is locked, the caster needs the acknowledgement packet twice.
    if (pCreature->isPC()) {
        GCSkillFailed1 gcSkillFailed1;
        gcSkillFailed1.setSkillType(SkillType);
        (pCreature->getPlayer())->sendPacket(&gcSkillFailed1);
        if (bSendTwice)
            (pCreature->getPlayer())->sendPacket(&gcSkillFailed1);
    }

    if (bBroadcast) {
        GCSkillFailed2 gcSkillFailed2;
        gcSkillFailed2.setSkillType(SkillType);
        gcSkillFailed2.setObjectID(pCreature->getObjectID());
        gcSkillFailed2.setTargetObjectID(TargetObjectID);

        Zone* pZone = pCreature->getZone();
        Assert(pZone != NULL);

        pZone->broadcastPacket(pCreature->getX(), pCreature->getY(), &gcSkillFailed2, pCreature);
    }
}

//////////////////////////////////////////////////////////////////////////////
// Sends a packet when a skill fails.
// For an ordinary failure (a missed hit roll, not enough mana, and so on),
// the packet goes to the caster and to everyone who can see it.
//////////////////////////////////////////////////////////////////////////////
void executeSkillFailNormalWithGun(Creature* pCreature, SkillType_t SkillType, Creature* pTargetCreature,
                                   BYTE RemainBullet) {
    Assert(pCreature != NULL);

    if (pCreature->isPC()) {
        GCSkillFailed1 gcSkillFailed1;
        gcSkillFailed1.setSkillType(SkillType);
        gcSkillFailed1.addShortData(MODIFY_BULLET, RemainBullet);
        (pCreature->getPlayer())->sendPacket(&gcSkillFailed1);
    }

    GCSkillFailed2 gcSkillFailed2;
    gcSkillFailed2.setSkillType(SkillType);
    gcSkillFailed2.setObjectID(pCreature->getObjectID());

    // For an object skill the target's object id, when present, is carried in the packet.
    // For a self or tile skill NULL is normally passed instead.
    // (On a GCSkillFailed2 sent for a failed self or tile skill the client
    // does not even read TargetObjectID.)
    if (pTargetCreature != NULL) {
        gcSkillFailed2.setTargetObjectID(pTargetCreature->getObjectID());
    } else {
        gcSkillFailed2.setTargetObjectID(0);
    }

    Zone* pZone = pCreature->getZone();
    Assert(pZone != NULL);

    pZone->broadcastPacket(pCreature->getX(), pCreature->getY(), &gcSkillFailed2, pCreature);
}

//////////////////////////////////////////////////////////////////////////////
// Sends a packet when a skill fails.
// For an exceptional failure (attacking an NPC, and so on),
// the packet goes only to the caster.
//////////////////////////////////////////////////////////////////////////////
void executeSkillFailException(Creature* pCreature, SkillType_t SkillType, BYTE Grade) {
    if (pCreature != NULL && pCreature->isPC()) {
        GCSkillFailed1 gcSkillFailed1;
        gcSkillFailed1.setSkillType(SkillType);
        gcSkillFailed1.setGrade(Grade);

        Player* pPlayer = pCreature->getPlayer();
        pPlayer->sendPacket(&gcSkillFailed1);
    }
}


ElementalType getElementalTypeFromString(const string& type) {
    if (type == "Fire")
        return ELEMENTAL_FIRE;
    else if (type == "Water")
        return ELEMENTAL_WATER;
    else if (type == "Earth")
        return ELEMENTAL_EARTH;
    else if (type == "Wind")
        return ELEMENTAL_WIND;

    return ELEMENTAL_MAX;
}


//////////////////////////////////////////////////////////////////////////////
// Can the attack proceed?
// Checks invulnerability and the non-PK setting.
//////////////////////////////////////////////////////////////////////////////
bool canAttack(Creature* pAttacker, Creature* pDefender) {
    Assert(pDefender != NULL);

    // Invulnerability check
    if (pDefender->isFlag(Effect::EFFECT_CLASS_NO_DAMAGE))
        return false;

    // A NULL attacker is simply allowed.
    // The attacker can be NULL when the check comes from an effect.
    if (pAttacker == NULL)
        return true;

    // Is this game server configured as non-PK?
    static const int serverID = de::kernelContext().config().getPropertyInt("ServerID");
    static const int worldID = de::kernelContext().config().getPropertyInt("WorldID");
    static bool bNonPK = de::serverContext().serverInfos().getGameServerInfo(1, serverID, worldID)->isNonPKServer();

    // non-PK check
    if (bNonPK && pAttacker->isPC() && pDefender->isPC())
        return false;

    return true;
}

//////////////////////////////////////////////////////////////////////////
// add by Coffee 2007-6-9
// Consumes the caster's race-specific skill card, failing if none is held.
//////////////////////////////////////////////////////////////////////////
bool useSkillCrad(Creature* pCreature) {
    GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(pCreature->getPlayer());
    PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pGamePlayer->getCreature());

    CoordInven_t InvenX = 0;
    CoordInven_t InvenY = 0;
    ItemType_t fitItem = 0; // Skill card item type
    if (pCreature->isSlayer()) {
        fitItem = 5;
    } else if (pCreature->isVampire()) {
        fitItem = 6;
    } else if (pCreature->isOusters()) {
        fitItem = 7;
    }
    if (fitItem != 0) {
        Item* pItem = pPC->getInventory()->findItem(Item::ITEM_CLASS_MOON_CARD, fitItem, InvenX, InvenY);
        if (pItem == NULL) {
            GCSystemMessage gcSystemMessage1;
            gcSystemMessage1.setMessage("You need a skill card to use this skill!");
            gcSystemMessage1.setType(SYSTEM_MESSAGE_OPERATOR);
            pGamePlayer->sendPacket(&gcSystemMessage1);
            return false;
        }
        ItemNum_t OldNum = pItem->getNum();
        if (OldNum == 1) {
            pPC->getInventory()->deleteItem(pItem->getObjectID());
            pItem->destroy();
            SAFE_DELETE(pItem);
        } else {
            OldNum--;
            pItem->setNum((pItem->getNum() - 1));
            pItem->save(pPC->getName(), STORAGE_INVENTORY, 0, InvenX, InvenY);
        }

    } else {
        GCSystemMessage gcSystemMessage1;
        gcSystemMessage1.setMessage("You need a skill card to use this skill!");
        gcSystemMessage1.setType(SYSTEM_MESSAGE_OPERATOR);
        pGamePlayer->sendPacket(&gcSystemMessage1);
        return false;
    }
    return true;
}
