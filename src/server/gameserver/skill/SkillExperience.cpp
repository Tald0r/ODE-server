//////////////////////////////////////////////////////////////////////////////
// FileName 	: SkillExperience.cpp
// Description	: Experience, alignment and fame: what a kill earns a creature and how the gain is spent.
//////////////////////////////////////////////////////////////////////////////

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
#include "GameContext.h"
#include "GamePlayer.h"
#include "GameServerInfoManager.h"
#include "HitRoll.h"
#include "ItemFactoryManager.h"
#include "ItemUtil.h"
#include "MasterLairInfoManager.h"
#include "Monster.h"
#include "OustersEXPInfo.h"
#include "PKZoneInfoManager.h"
#include "PacketUtil.h"
#include "Party.h"
#include "Player.h"
#include "PrecedenceTable.h"
#include "Properties.h"
#include "SkillDomainInfoManager.h"
#include "SkillInfo.h"
#include "SkillPropertyManager.h"
#include "SkillUtil.h"
#include "SkillUtilInternal.h"
#include "SummonGroundElemental.h"
#include "Thread.h"
#include "VampEXPInfo.h"
#include "VariableManager.h"
#include "Zone.h"
#include "ZoneUtil.h"
#include "domain/Formulas.h"
#include "mission/EventQuestLootingManager.h"
#include "mission/MonsterKillQuestStatus.h"
#include "mission/QuestManager.h"

//////////////////////////////////////////////////////////////////////////////
// data structure & helper functions
//////////////////////////////////////////////////////////////////////////////
typedef struct DomainStruct {
    int DomainType;
    int DomainLevel;
} SDomain;

class isBig {
public:
    isBig(){};

    bool operator()(const DomainStruct& t, const DomainStruct& b) {
        if (t.DomainLevel > b.DomainLevel)
            return true;
        else
            return false;
    }
};

//////////////////////////////////////////////////////////////////////////////
// Premium zone experience bonus.
//////////////////////////////////////////////////////////////////////////////


RankExp_t computeRankExp(int myLevel, int otherLevel) // by sigi. 2002.12.31
{
    // The formula lives in de-core; this adapter only supplies
    // the two server-configured percentages.
    return decore::rankExp(myLevel, otherLevel, de::gameContext().variables().getVariable(RANK_EXP_GAIN_PERCENT),
                           de::gameContext().variables().getPremiumExpBonusPercent());
}

//////////////////////////////////////////////////////////////////////////////
// Effect of killing a creature.
//
// Gives the killer rank experience for the kill.
// by sigi. 2002.8.31
//////////////////////////////////////////////////////////////////////////////
void affectKillCount(Creature* pAttacker, Creature* pDeadCreature) {
    // [Cases that need no handling]
    // no attacker,
    // no dead creature,
    // the attacker is not a PC,
    // or the target is not actually dead.
    if (pAttacker == NULL || pDeadCreature == NULL || !pAttacker->isPC() || pDeadCreature->isAlive()) {
        return;
    }

    VariableManager& variables = de::gameContext().variables();

    if (pDeadCreature->isMonster()) {
        Monster* pMonster = dynamic_cast<Monster*>(pDeadCreature);

        if (pMonster->getLastKiller() == 0)
            pMonster->setLastKiller(pAttacker->getObjectID());
    }

    ItemFactoryManager& itemFactories = de::gameContext().itemFactories();

    int myLevel = 0;
    int otherLevel = 0;

    if (pAttacker->isSlayer()) {
        // A Slayer killed a Slayer.
        if (pDeadCreature->isSlayer())
            return;

        Slayer* pSlayer = dynamic_cast<Slayer*>(pAttacker);
        myLevel = pSlayer->getHighestSkillDomainLevel();

        // A Slayer with no weapon in hand is ignored.
        if (!pSlayer->isRealWearingEx(Slayer::WEAR_RIGHTHAND))
            return;

        // A Slayer killed a Vampire.
        if (pDeadCreature->isVampire()) {
            Vampire* pVampire = dynamic_cast<Vampire*>(pDeadCreature);
            otherLevel = pVampire->getLevel();
        }
        // A Slayer killed an Ousters.
        else if (pDeadCreature->isOusters()) {
            Ousters* pOusters = dynamic_cast<Ousters*>(pDeadCreature);
            otherLevel = pOusters->getLevel();
        }
        // A Slayer killed a monster.
        else if (pDeadCreature->isMonster()) {
            Monster* pMonster = dynamic_cast<Monster*>(pDeadCreature);

            // A master is handled by MasterLairManager.
            if (pMonster->isMaster()) {
                // The one who lands the last kill gains experience once more.
                pSlayer->increaseRankExp(MASTER_KILL_RANK_EXP);
                return;
            }

            otherLevel = pMonster->getLevel();
        } else
            return;
    } else if (pAttacker->isVampire()) {
        // A Vampire killed a Vampire.
        if (pDeadCreature->isVampire())
            return;

        Vampire* pVampire = dynamic_cast<Vampire*>(pAttacker);
        myLevel = pVampire->getLevel();

        // A Vampire killed a Slayer.
        if (pDeadCreature->isSlayer()) {
            Slayer* pSlayer = dynamic_cast<Slayer*>(pDeadCreature);
            otherLevel = pSlayer->getHighestSkillDomainLevel();
        }
        // A Vampire killed an Ousters.
        else if (pDeadCreature->isOusters()) {
            Ousters* pOusters = dynamic_cast<Ousters*>(pDeadCreature);
            otherLevel = pOusters->getLevel();
        }
        // A Vampire killed a monster.
        else if (pDeadCreature->isMonster()) {
            Monster* pMonster = dynamic_cast<Monster*>(pDeadCreature);

            // A master is handled by MasterLairManager.
            if (pMonster->isMaster()) {
                // The one who lands the last kill gains experience once more.
                pVampire->increaseRankExp(MASTER_KILL_RANK_EXP);
                return;
            }

            otherLevel = pMonster->getLevel();
        } else
            return;
    } else if (pAttacker->isOusters()) {
        // An Ousters killed an Ousters.
        if (pDeadCreature->isOusters())
            return;

        Ousters* pOusters = dynamic_cast<Ousters*>(pAttacker);
        myLevel = pOusters->getLevel();

        // An Ousters killed a Slayer.
        if (pDeadCreature->isSlayer()) {
            Slayer* pSlayer = dynamic_cast<Slayer*>(pDeadCreature);
            otherLevel = pSlayer->getHighestSkillDomainLevel();
        }
        // An Ousters killed a Vampire.
        if (pDeadCreature->isVampire()) {
            Vampire* pVampire = dynamic_cast<Vampire*>(pDeadCreature);
            otherLevel = pVampire->getLevel();
        }
        // A Vampire killed a monster.
        else if (pDeadCreature->isMonster()) {
            Monster* pMonster = dynamic_cast<Monster*>(pDeadCreature);

            // A master is handled by MasterLairManager.
            if (pMonster->isMaster()) {
                // The one who lands the last kill gains experience once more.
                pOusters->increaseRankExp(MASTER_KILL_RANK_EXP);
                return;
            }

            otherLevel = pMonster->getLevel();
        } else
            return;
    } else
        return;

    PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pAttacker);

    if (pDeadCreature->isMonster()) {
        Monster* pMonster = dynamic_cast<Monster*>(pDeadCreature);
        PrecedenceTable* pTable = pMonster->getPrecedenceTable();


        if (pMonster->getMonsterType() == 722) {
            if (pPC->getPartyID() == 0) {
                if (!pPC->isFlag(Effect::EFFECT_CLASS_CAN_ENTER_GDR_LAIR)) {
                    EffectCanEnterGDRLair* pEffect = new EffectCanEnterGDRLair(pPC);
                    pEffect->setDeadline(216000);

                    pPC->setFlag(pEffect->getEffectClass());
                    pPC->addEffect(pEffect);

                    pEffect->create(pPC->getName());

                    GCAddEffect gcAddEffect;
                    gcAddEffect.setObjectID(pPC->getObjectID());
                    gcAddEffect.setEffectID(pEffect->getSendEffectClass());
                    gcAddEffect.setDuration(21600);

                    pPC->getZone()->broadcastPacket(pPC->getX(), pPC->getY(), &gcAddEffect);
                }
            } else {
                LocalPartyManager* pLPM = pPC->getLocalPartyManager();
                Assert(pLPM != NULL);
                pLPM->shareGDRLairEnter(pPC->getPartyID(), pPC);
            }

            Item* pItem = itemFactories.createItem(Item::ITEM_CLASS_EVENT_ITEM, 29, list<OptionType_t>());
            pMonster->setQuestItem(pItem);
        }

        if (pTable != NULL && pTable->canGainRankExp(pPC)) {
            QuestManager* pQM = pPC->getQuestManager();

            if (pQM != NULL && pQM->killedMonster(pMonster)) {
                pPC->sendCurrentQuestInfo();
            }

            if (de::gameContext().eventQuestLoot().killed(pPC, pMonster))
                pTable->setQuestHostName(pPC->getName());

            if (pMonster->getQuestItem() == NULL && variables.getVariable(EVENT_NEW_YEAR_2005) != 0) {
                Item* pItem = getNewYear2005Item(getNewYear2005ItemKind(pPC, pMonster));
                pMonster->setQuestItem(pItem);
                if (pItem != NULL)
                    logEventItemCount(pItem);
            }

            if (pMonster->getQuestItem() == NULL && variables.isEventMoonCard()) {
                Item* pItem = getCardItem(getCardKind(pPC, pMonster));
                pMonster->setQuestItem(pItem);
            }

            if (pMonster->getQuestItem() == NULL && variables.isEventLuckyBag()) {
                Item* pItem = getLuckyBagItem(getLuckyBagKind(pPC, pMonster));
                pMonster->setQuestItem(pItem);
            }

            if (pMonster->getQuestItem() == NULL && variables.getVariable(NICKNAME_PEN_EVENT) != 0 &&
                canGiveEventItem(pPC, pMonster)) {
                int value = rand() % 100000;
                if (value < variables.getVariable(NICKNAME_PEN_RATIO)) {
                    Item* pItem = itemFactories.createItem(Item::ITEM_CLASS_EVENT_GIFT_BOX, 22, list<OptionType_t>());
                    pMonster->setQuestItem(pItem);
                }
            }

            if (pMonster->getQuestItem() == NULL && variables.getVariable(CLOVER_EVENT) != 0 &&
                canGiveEventItem(pPC, pMonster)) {
                int value = rand() % 100000;
                if (value < variables.getVariable(CLOVER_RATIO)) {
                    Item* pItem = itemFactories.createItem(Item::ITEM_CLASS_MOON_CARD, 3, list<OptionType_t>());
                    pMonster->setQuestItem(pItem);
                }
            }

            if (pMonster->getQuestItem() == NULL && variables.getVariable(PINE_CAKE_EVENT) != 0 &&
                canGiveEventItem(pPC, pMonster)) {
                int value = rand() % 100000;
                if (value < variables.getVariable(PINE_CAKE_RATIO)) {
                    int value = rand() % 10;
                    int add = 0;
                    if (value < 6)
                        add = 0;
                    else if (value < 9)
                        add = 1;
                    else
                        add = 2;
                    Item* pItem = itemFactories.createItem(Item::ITEM_CLASS_EVENT_ETC, 15 + add, list<OptionType_t>());
                    pMonster->setQuestItem(pItem);
                }
            }

            if (variables.getVariable(NETMARBLE_CARD_EVENT) != 0 && pMonster->getQuestItem() == NULL &&
                variables.getVariable(NETMARBLE_CARD_RATIO) > (rand() % 100000) && canGiveEventItem(pPC, pMonster)) {
                Item* pItem = itemFactories.createItem(Item::ITEM_CLASS_MOON_CARD, 2, list<OptionType_t>());
                pMonster->setQuestItem(pItem);
            }

            if (variables.isHeadCount()) {
                GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(pAttacker->getPlayer());
                if (pGamePlayer != NULL) {
                    EventHeadCount* pEvent =
                        dynamic_cast<EventHeadCount*>(pGamePlayer->getEvent(Event::EVENT_CLASS_HEAD_COUNT));
                    if (pEvent != NULL)
                        pEvent->cutHead();
                }
            }

            if (canGiveEventItem(pPC, pMonster) && rand() % 100000 < variables.getVariable(GOLD_MEDAL_RATIO)) {
                giveGoldMedal(pPC);
            }

            if (pPC->getLevel() - 20 <= pMonster->getLevel() && pPC->getLevel() >= 30 &&
                !GDRLairManager::Instance().isGDRLairZone(pPC->getZoneID()) && !pMonster->isChief() &&
                !pMonster->isMaster()) {
                addOlympicStat(pPC, 1);
            }

            if (pMonster->isChief()) {
                if (canGiveEventItem(pPC, pMonster) && !GDRLairManager::Instance().isGDRLairZone(pPC->getZoneID()))
                    addOlympicStat(pPC, 3);
            }

            if (pMonster->isMaster()) {
                if (canGiveEventItem(pPC, pMonster))
                    addOlympicStat(pPC, 3);
            }

            if (pMonster->getQuestItem() == NULL && canGiveEventItem(pPC, pMonster) &&
                rand() % 100000 < variables.getVariable(OLYMPIC_ITEM_RATIO)) {
                Item* pItem = itemFactories.createItem(Item::ITEM_CLASS_MOON_CARD, 4, list<OptionType_t>());
                pMonster->setQuestItem(pItem);
            }

            if (pMonster->getQuestItem() == NULL && canGiveEventItem(pPC, pMonster) &&
                rand() % 100000 < variables.getVariable(LUCK_CHARM_RATIO)) {
                Item* pItem = itemFactories.createItem(Item::ITEM_CLASS_EVENT_ITEM, 30, list<OptionType_t>());
                pMonster->setQuestItem(pItem);
            }

            if (pMonster->getQuestItem() == NULL && pMonster->getMonsterType() >= 769 &&
                canGiveEventItem(pPC, pMonster) && rand() % 100000 < variables.getVariable(HOURGLASS_RATIO_S)) {
                Item* pItem = itemFactories.createItem(Item::ITEM_CLASS_EFFECT_ITEM, 6, list<OptionType_t>());
                pMonster->setQuestItem(pItem);
            }

            if (pMonster->getQuestItem() == NULL && pMonster->getMonsterType() >= 769 &&
                canGiveEventItem(pPC, pMonster) && rand() % 100000 < variables.getVariable(HOURGLASS_RATIO_M)) {
                Item* pItem = itemFactories.createItem(Item::ITEM_CLASS_EFFECT_ITEM, 5, list<OptionType_t>());
                pMonster->setQuestItem(pItem);
            }

            if (pMonster->getQuestItem() == NULL && pMonster->getMonsterType() >= 769 &&
                canGiveEventItem(pPC, pMonster) && rand() % 100000 < variables.getVariable(HOURGLASS_RATIO_L)) {
                Item* pItem = itemFactories.createItem(Item::ITEM_CLASS_EFFECT_ITEM, 4, list<OptionType_t>());
                pMonster->setQuestItem(pItem);
            }

            if (pMonster->getZone()->isDynamicZone() &&
                pMonster->getZone()->getDynamicZone()->getTemplateZoneID() == 4002 &&
                pMonster->getQuestItem() == NULL && (rand() % 20) == 0) {
                Item* pItem = itemFactories.createItem(Item::ITEM_CLASS_EVENT_ITEM, 31, list<OptionType_t>());
                pMonster->setQuestItem(pItem);
            }

            if (variables.getVariable(PET_FOOD_EVENT) != 0 && pMonster->getQuestItem() == NULL &&
                variables.getVariable(PET_FOOD_RATIO) > rand() % 100000 && canGiveEventItem(pPC, pMonster)) {
                bool isHigher = variables.getVariable(HIGHER_PET_FOOD_RATIO) > rand() % 100;

                int itemClassSeed = rand() % 100;
                int RacePetFoodRatio = variables.getVariable(RACE_PET_FOOD_RATIO);
                int RevivalSetRatio = variables.getVariable(REVIVAL_SET_RATIO);

                Item::ItemClass iClass = Item::ITEM_CLASS_PET_FOOD;
                ItemType_t itemType = 1;

                if (itemClassSeed < RevivalSetRatio) {
                    iClass = Item::ITEM_CLASS_PET_ENCHANT_ITEM;
                    itemType = 13;
                } else if (itemClassSeed - RevivalSetRatio < RacePetFoodRatio) {
                    switch (pPC->getCreatureClass()) {
                    case Creature::CREATURE_CLASS_SLAYER:
                        itemType = 6;
                        break;

                    case Creature::CREATURE_CLASS_VAMPIRE:
                        itemType = 10;
                        break;

                    case Creature::CREATURE_CLASS_OUSTERS:
                        itemType = 14;
                        break;

                    default:
                        break;
                    }
                }

                if (iClass == Item::ITEM_CLASS_PET_FOOD && isHigher)
                    ++itemType;

                Item* pItem = itemFactories.createItem(iClass, itemType, list<OptionType_t>());
                pMonster->setQuestItem(pItem);
            }
        }
    }

    int PartyID = pPC->getPartyID();
    if (PartyID != 0) {
        // When the player is in a party, the experience is shared with the
        // nearby party members through the local party manager.
        LocalPartyManager* pLPM = pPC->getLocalPartyManager();
        Assert(pLPM != NULL);
        pLPM->shareRankExp(PartyID, pAttacker, otherLevel);
        if (pPC->isAdvanced())
            pLPM->shareAdvancementExp(PartyID, pAttacker, computeCreatureExp(pDeadCreature, 1));
    } else {
        PlayerCreature* pAttackPC = dynamic_cast<PlayerCreature*>(pAttacker);
        if (pAttackPC->isAdvanced()) {
            pAttackPC->increaseAdvancementClassExp(computeCreatureExp(pDeadCreature, 1), true);
        }

        // Without a party the player gains it alone.
        RankExp_t rankExp = computeRankExp(myLevel, otherLevel);

        if (pDeadCreature->isMonster()) {
            Monster* pMonster = dynamic_cast<Monster*>(pDeadCreature);
            PrecedenceTable* pTable = pMonster->getPrecedenceTable();

            if (pTable != NULL && pTable->canGainRankExp(pPC)) {
                pPC->increaseRankExp(rankExp);
            }
        } else {
            pPC->increaseRankExp(rankExp);
        }
    }
}

bool canGiveSkillExp(Slayer* pSlayer, SkillDomainType_t SkillDomainType, SkillType_t UseSkillType) {
    if (pSlayer == NULL)
        return false;
    if (!pSlayer->isRealWearing(Slayer::WEAR_RIGHTHAND))
        return false;

    Item* pWeapon = pSlayer->getWearItem(Slayer::WEAR_RIGHTHAND);
    if (pWeapon == NULL)
        return false;

    switch (SkillDomainType) {
    case SKILL_DOMAIN_BLADE:
        if (pWeapon->getItemClass() != Item::ITEM_CLASS_BLADE)
            return false;
        break;
    case SKILL_DOMAIN_SWORD:
        if (pWeapon->getItemClass() != Item::ITEM_CLASS_SWORD)
            return false;
        break;
    case SKILL_DOMAIN_GUN:
        if (!isArmsWeapon(pWeapon))
            return false;
        break;
    case SKILL_DOMAIN_HEAL:
        if (pWeapon->getItemClass() != Item::ITEM_CLASS_CROSS)
            return false;
        break;
    case SKILL_DOMAIN_ENCHANT:
        if (pWeapon->getItemClass() != Item::ITEM_CLASS_MACE)
            return false;
        break;
    default:
        return false;
    }

    if (UseSkillType < SKILL_DOUBLE_IMPACT)
        return true;
    SkillInfo* pUseSkillInfo = de::gameContext().skillInfos().getSkillInfo(UseSkillType);
    if (SkillDomainType != pUseSkillInfo->getDomainType())
        return false;

    return true;
}

void giveSkillExp(Slayer* pSlayer, SkillType_t SkillType, ModifyInfo& AttackerMI) {
    if (pSlayer == NULL)
        return;

    SkillSlot* pSkillSlot = pSlayer->getSkill(SkillType);
    SkillInfo* pSkillInfo = de::gameContext().skillInfos().getSkillInfo(SkillType);
    if (pSkillSlot != NULL && pSkillInfo != NULL) {
        increaseSkillExp(pSlayer, pSkillInfo->getDomainType(), pSkillSlot, pSkillInfo, AttackerMI);
    }
}

//////////////////////////////////////////////////////////////////////////////
// Changes alignment.
// Computes the alignment change caused by using a skill or by PK.
//////////////////////////////////////////////////////////////////////////////
void computeAlignmentChange(Creature* pTargetCreature, Damage_t Damage, Creature* pAttacker, ModifyInfo* pMI,
                            ModifyInfo* pAttackerMI) {
    Assert(pTargetCreature != NULL);

    // Alignment does not change in a PK zone.
    if (de::gameContext().pkZoneInfos().isPKZone(pAttacker->getZoneID()))
        return;

    Zone* pZone = pTargetCreature->getZone();
    Assert(pZone != NULL);

    bool bSameRace = false;

    // With an attacker present, check whether it is the same race.
    if (pAttacker != NULL) {
        bSameRace = isSameRace(pTargetCreature, pAttacker);

        bool bPKOlympic = false;

        switch (pAttacker->getZoneID()) {
        case 1122:
        case 1131:
        case 1132:
        case 1133:
        case 1134:
            bPKOlympic = true;
            break;
        default:
            break;
        }

        // Killing a different race scores an olympic point.
        if (!bSameRace && bPKOlympic && pTargetCreature->isPC() && pTargetCreature->isDead() &&
            !GDRLairManager::Instance().isGDRLairZone(pTargetCreature->getZoneID())) {
            PlayerCreature* pAttackPC = dynamic_cast<PlayerCreature*>(pAttacker);
            if (pAttackPC->getLevel() - 10 <= pTargetCreature->getLevel() &&
                !pTargetCreature->isFlag(Effect::EFFECT_CLASS_PK_COUNTED)) {
                addSimpleCreatureEffect(pTargetCreature, Effect::EFFECT_CLASS_PK_COUNTED, 6000, false);
                addOlympicStat(pAttackPC, 2);
            }
        }

        if (pZone->isPKZone())
            return;
    }

    // Alignment can change when both are the same race.
    if (bSameRace) {
        PlayerCreature* pAttackPC = dynamic_cast<PlayerCreature*>(pAttacker);
        PlayerCreature* pTargetPC = dynamic_cast<PlayerCreature*>(pTargetCreature);

        string AttackName = pAttackPC->getName();
        string TargetName = pTargetPC->getName();

        Alignment_t AttackAlignment = pAttackPC->getAlignment();
        Alignment_t TargetAlignment = pTargetPC->getAlignment();

        Alignment_t ModifyAlignment = 0;

        // Remember whether the change is a decrease or an increase.
        bool bdecrease = false;
        if (pTargetPC->isDead()) {
            ModifyAlignment =
                de::gameContext().alignments().getMultiplier(AttackAlignment, TargetAlignment); // Damage* 2

            if (ModifyAlignment < 0) {
                ModifyAlignment = ModifyAlignment * 10;
                bdecrease = true;
            } else if (ModifyAlignment > 0) {
                // (victim level) / (killer level) * (base alignment gain), capped at the base gain

                if (pAttackPC->getLevel() - 10 <= pTargetPC->getLevel() &&
                    !pTargetPC->isFlag(Effect::EFFECT_CLASS_PUNISH_COUNTED)) {
                    addOlympicStat(pAttackPC, 7);
                    addSimpleCreatureEffect(pTargetPC, Effect::EFFECT_CLASS_PUNISH_COUNTED, 6000, false);
                }

                if (pAttackPC->isSlayer()) {
                    Slayer* pAttackSlayer = dynamic_cast<Slayer*>(pAttackPC);
                    Slayer* pTargetSlayer = dynamic_cast<Slayer*>(pTargetPC);

                    SkillLevel_t AttackLevel = pAttackSlayer->getHighestSkillDomainLevel();
                    SkillLevel_t TargetLevel = pTargetSlayer->getHighestSkillDomainLevel();

                    if (AttackLevel > TargetLevel && AttackLevel != 0) {
                        ModifyAlignment = (Alignment_t)(ModifyAlignment * ((float)TargetLevel / (float)AttackLevel));
                    }
                } else if (pAttackPC->isVampire()) {
                    Vampire* pAttackVampire = dynamic_cast<Vampire*>(pAttackPC);
                    Vampire* pTargetVampire = dynamic_cast<Vampire*>(pTargetPC);

                    Level_t AttackLevel = pAttackVampire->getLevel();
                    Level_t TargetLevel = pTargetVampire->getLevel();

                    if (AttackLevel > TargetLevel && AttackLevel != 0) {
                        ModifyAlignment = (Alignment_t)(ModifyAlignment * ((float)TargetLevel / (float)AttackLevel));
                    }
                } else if (pAttackPC->isOusters()) {
                    Ousters* pAttackOusters = dynamic_cast<Ousters*>(pAttackPC);
                    Ousters* pTargetOusters = dynamic_cast<Ousters*>(pTargetPC);

                    Level_t AttackLevel = pAttackOusters->getLevel();
                    Level_t TargetLevel = pTargetOusters->getLevel();

                    if (AttackLevel > TargetLevel && AttackLevel != 0) {
                        ModifyAlignment = (Alignment_t)(ModifyAlignment * ((float)TargetLevel / (float)AttackLevel));
                    }
                }
            }

            pTargetPC->setPK(true);
        }

        Alignment_t ResultAlignment = AttackAlignment + ModifyAlignment;

        ResultAlignment = max(-10000, ResultAlignment);
        ResultAlignment = min(10000, ResultAlignment);

        EffectManager* pAttackEffectManager = pAttackPC->getEffectManager();
        EffectManager* pTargetEffectManager = pTargetPC->getEffectManager();

        // Regardless of alignment, hitting someone who has no self-defense claim always grants them one.
        if (!pAttackPC->hasEnemy(TargetName) &&
            de::gameContext().alignments().getAlignmentType(TargetAlignment) >= NEUTRAL) {
            GCAddInjuriousCreature gcAddInjuriousCreature;
            gcAddInjuriousCreature.setName(AttackName);
            pTargetPC->getPlayer()->sendPacket(&gcAddInjuriousCreature);

            // Add the attacker to the victim's aggressor list and attach an
            // effect that clears it after five minutes.
            pTargetPC->addEnemy(AttackName);

            EffectEnemyErase* pEffectEnemyErase = new EffectEnemyErase(pTargetPC);
            pEffectEnemyErase->setDeadline(3000);
            pEffectEnemyErase->setEnemyName(AttackName);
            pEffectEnemyErase->create(TargetName);
            pTargetEffectManager->addEffect(pEffectEnemyErase);
        }

        // Erase the effect once the target is a self-defense target of mine and has been killed.
        if (pAttackPC->hasEnemy(TargetName) && pTargetPC->isDead()) {
            EffectEnemyErase* pAttackerEffect =
                (EffectEnemyErase*)pAttackEffectManager->findEffect(Effect::EFFECT_CLASS_ENEMY_ERASE, TargetName);

            if (pAttackerEffect != NULL) {
                // Being on the aggressor list means the effect that clears it always exists, so it
                // cannot be NULL.
                Assert(pAttackerEffect != NULL);
                Assert(pAttackerEffect->getEffectClass() == Effect::EFFECT_CLASS_ENEMY_ERASE);
                // Erase it.
                pAttackerEffect->setDeadline(0);
            }
        }

        // When the defender's name is on the aggressor list and the attacker's alignment is Good
        // or Neutral, the kill counts as self-defense and the alignment is not lowered.
        if (!(bdecrease && pAttackPC->hasEnemy(TargetName) &&
              de::gameContext().alignments().getAlignmentType(AttackAlignment) >= NEUTRAL)) {
            // Whether it goes up or down, the value has to be set first.
            // Set it here.
            if (pAttackerMI && ModifyAlignment != 0) {
                if (pAttackPC->isSlayer()) {
                    Slayer* pSlayer = dynamic_cast<Slayer*>(pAttackPC);

                    pAttackerMI->addShortData(MODIFY_ALIGNMENT, ResultAlignment);
                    pSlayer->setAlignment(ResultAlignment);

                    WORD AlignmentSaveCount = pSlayer->getAlignmentSaveCount();
                    if (AlignmentSaveCount > ALIGNMENT_SAVE_PERIOD) {
                        char pField[80];
                        snprintf(pField, sizeof(pField), "ALIGNMENT=%d", ResultAlignment);
                        pSlayer->tinysave(pField);

                        AlignmentSaveCount = 0;
                    } else
                        AlignmentSaveCount++;

                    pSlayer->setAlignmentSaveCount(AlignmentSaveCount);
                } else if (pAttackPC->isVampire()) {
                    Vampire* pVampire = dynamic_cast<Vampire*>(pAttackPC);

                    pAttackerMI->addShortData(MODIFY_ALIGNMENT, ResultAlignment);
                    pVampire->setAlignment(ResultAlignment);

                    WORD AlignmentSaveCount = pVampire->getAlignmentSaveCount();
                    if (AlignmentSaveCount > ALIGNMENT_SAVE_PERIOD) {
                        char pField[80];
                        snprintf(pField, sizeof(pField), "ALIGNMENT=%d", ResultAlignment);
                        pVampire->tinysave(pField);

                        AlignmentSaveCount = 0;
                    } else
                        AlignmentSaveCount++;

                    pVampire->setAlignmentSaveCount(AlignmentSaveCount);
                } else if (pAttackPC->isOusters()) {
                    Ousters* pOusters = dynamic_cast<Ousters*>(pAttackPC);

                    pAttackerMI->addShortData(MODIFY_ALIGNMENT, ResultAlignment);
                    pOusters->setAlignment(ResultAlignment);

                    WORD AlignmentSaveCount = pOusters->getAlignmentSaveCount();
                    if (AlignmentSaveCount > ALIGNMENT_SAVE_PERIOD) {
                        char pField[80];
                        snprintf(pField, sizeof(pField), "ALIGNMENT=%d", ResultAlignment);
                        pOusters->tinysave(pField);

                        AlignmentSaveCount = 0;
                    } else
                        AlignmentSaveCount++;

                    pOusters->setAlignmentSaveCount(AlignmentSaveCount);
                }
            }

            // When alignment drops it is cut tenfold, then recovered gradually by an effect.
            if (bdecrease) {
                // My name on the defender's aggressor list makes the attacker the bad one.
                // Being on the aggressor list means the effect is still attached.
                // At most one alignment recovery effect exists at a time; they do not stack.
                EffectAlignmentRecovery* pAttackerEffect =
                    (EffectAlignmentRecovery*)pAttackEffectManager->findEffect(Effect::EFFECT_CLASS_ALIGNMENT_RECOVERY);
                // Fetch the effect and set its values again.
                // The effect is certainly there, since my name is on the aggressor list.
                // Synchronization can still break, so the deadline is set a little longer.

                if (pAttackerEffect != NULL) {
                    // How much to recover in total.
                    Alignment_t Amount = abs(ModifyAlignment / 10 * 9);

                    // How much per tick: 10.
                    Alignment_t Quantity = 10;

                    // How long between ticks: 30 seconds.
                    int DelayProvider = 300;

                    // How many times to recover.
                    double temp = (double)((double)Amount / (double)Quantity);
                    int Period = (uint)floor(temp);

                    // How long the whole recovery takes.
                    Turn_t Deadline = Period * DelayProvider;

                    pAttackerEffect->setQuantity(Quantity);
                    pAttackerEffect->setPeriod(Period);
                    pAttackerEffect->setDeadline(Deadline);
                    pAttackerEffect->setDelay(DelayProvider);
                } else {
                    // With no effect this is the first aggression, so a new one is attached for five minutes.
                    // Do not forget that the defender needs the erasing effect attached too.
                    // The erasing effect belongs to the other side's effect manager.
                    // Attach an effect to the attacker that recovers 10 alignment every 30 seconds.

                    // How much to recover in total.
                    Alignment_t Amount = abs(ModifyAlignment / 10 * 9);

                    // How much per tick: 10.
                    Alignment_t Quantity = 10;

                    // How long between ticks: 30 seconds.
                    int DelayProvider = 300;

                    // How many times to recover.
                    double temp = (double)((double)Amount / (double)Quantity);
                    int Period = (uint)floor(temp);

                    // How long the whole recovery takes.
                    Turn_t Deadline = Period * DelayProvider;

                    // Attach the recovery effect first.
                    EffectAlignmentRecovery* pEffectAlignmentRecovery = new EffectAlignmentRecovery();

                    pEffectAlignmentRecovery->setTarget(pAttackPC);
                    pEffectAlignmentRecovery->setDeadline(Deadline);
                    pEffectAlignmentRecovery->setDelay(DelayProvider);
                    pEffectAlignmentRecovery->setNextTime(DelayProvider);
                    pEffectAlignmentRecovery->setQuantity(Quantity);
                    pEffectAlignmentRecovery->setPeriod(Period);

                    pAttackEffectManager->addEffect(pEffectAlignmentRecovery);
                }

                // The deadline of the effect on the defender has to be set again.
                EffectEnemyErase* pDefenderEffect =
                    (EffectEnemyErase*)pTargetEffectManager->findEffect(Effect::EFFECT_CLASS_ENEMY_ERASE, AttackName);

                if (pDefenderEffect != NULL) {
                    // Being on the aggressor list means the effect that clears it always exists, so it
                    // cannot be NULL.
                    Assert(pDefenderEffect != NULL);
                    Assert(pDefenderEffect->getEffectClass() == Effect::EFFECT_CLASS_ENEMY_ERASE);
                    // Set to five minutes.
                    pDefenderEffect->setDeadline(36000);
                    pDefenderEffect->save(TargetName);
                }
            }
        }
    }
}

//////////////////////////////////////////////////////////////////////////////
// Recovers a little alignment when a Slayer or Vampire kills a monster.
//////////////////////////////////////////////////////////////////////////////
void increaseAlignment(Creature* pCreature, Creature* pEnemy, ModifyInfo& mi) {
    Assert(pCreature != NULL);
    Assert(pEnemy != NULL);

    // Alignment is not raised in a PK zone.
    if (de::gameContext().pkZoneInfos().isPKZone(pCreature->getZoneID()))
        return;

    // Alignment is not raised inside a dynamic zone.
    if (pCreature->getZone() != NULL && pCreature->getZone()->isDynamicZone())
        return;

    // Alignment does not change while the monster is still alive.
    if (!pEnemy->isDead())
        return;

    // Alignment is not raised when the enemy is an NPC or of the same race.
    if (pEnemy->isNPC())
        return;
    if (pCreature->isSlayer() && pEnemy->isSlayer())
        return;
    if (pCreature->isVampire() && pEnemy->isVampire())
        return;
    if (pCreature->isOusters() && pEnemy->isOusters())
        return;

    Alignment_t OldAlignValue = 0;
    Alignment_t NewAlignValue = 0;

    if (pCreature->isSlayer()) {
        Slayer* pSlayer = dynamic_cast<Slayer*>(pCreature);

        // Read the current alignment value.
        OldAlignValue = pSlayer->getAlignment();

        // Alignment above 0 does not change from killing a monster.
        if (OldAlignValue > 0)
            return;

        // Compute how much alignment to gain.
        if (pEnemy->isMonster()) {
            Monster* pMonster = dynamic_cast<Monster*>(pEnemy);
            Assert(pMonster != NULL);
            NewAlignValue = max(0, (int)(pMonster->getLevel() / 10));
        } else if (pEnemy->isVampire()) {
            Vampire* pVampire = dynamic_cast<Vampire*>(pEnemy);
            Assert(pVampire != NULL);
            NewAlignValue = max(0, (int)(pVampire->getLevel() / 5));
        } else if (pEnemy->isOusters()) {
            Ousters* pOusters = dynamic_cast<Ousters*>(pEnemy);
            Assert(pOusters != NULL);
            NewAlignValue = max(0, (int)(pOusters->getLevel() / 5));
        }

        NewAlignValue = OldAlignValue + NewAlignValue;

        if (OldAlignValue != NewAlignValue) {
            // Report the alignment change in the packet.
            mi.addShortData(MODIFY_ALIGNMENT, NewAlignValue);

            WORD AlignmentSaveCount = pSlayer->getAlignmentSaveCount();
            if (AlignmentSaveCount > ALIGNMENT_SAVE_PERIOD) {
                pSlayer->saveAlignment(NewAlignValue);
                AlignmentSaveCount = 0;
            } else {
                pSlayer->setAlignment(NewAlignValue);
                AlignmentSaveCount++;
            }

            pSlayer->setAlignmentSaveCount(AlignmentSaveCount);
        }
    } else if (pCreature->isVampire()) {
        Vampire* pVampire = dynamic_cast<Vampire*>(pCreature);

        // Read the current alignment value.
        OldAlignValue = pVampire->getAlignment();

        // Alignment above 0 does not change from killing a monster.
        if (OldAlignValue > 0)
            return;

        // Compute how much alignment to gain.
        NewAlignValue = 0;
        if (pEnemy->isMonster()) {
            Monster* pMonster = dynamic_cast<Monster*>(pEnemy);
            Assert(pMonster != NULL);
            NewAlignValue = max(0, (int)(pMonster->getLevel() / 10));
        } else if (pEnemy->isSlayer()) {
            Slayer* pSlayer = dynamic_cast<Slayer*>(pEnemy);
            Assert(pSlayer != NULL);
            NewAlignValue = max(0, (int)(pSlayer->getSTR(ATTR_BASIC) + pSlayer->getDEX(ATTR_BASIC) +
                                         pSlayer->getINT(ATTR_BASIC) + pSlayer->getSkillDomainLevelSum()) /
                                       5);
        } else if (pEnemy->isOusters()) {
            Ousters* pOusters = dynamic_cast<Ousters*>(pEnemy);
            Assert(pOusters != NULL);
            NewAlignValue = max(0, (int)(pOusters->getLevel() / 5));
        }

        NewAlignValue = OldAlignValue + NewAlignValue;

        if (OldAlignValue != NewAlignValue) {
            // Report the alignment change in the packet.
            mi.addShortData(MODIFY_ALIGNMENT, NewAlignValue);

            WORD AlignmentSaveCount = pVampire->getAlignmentSaveCount();
            if (AlignmentSaveCount > ALIGNMENT_SAVE_PERIOD) {
                pVampire->saveAlignment(NewAlignValue);
                AlignmentSaveCount = 0;
            } else {
                pVampire->setAlignment(NewAlignValue);
                AlignmentSaveCount++;
            }
        }
    } else if (pCreature->isOusters()) {
        Ousters* pOusters = dynamic_cast<Ousters*>(pCreature);

        // Read the current alignment value.
        OldAlignValue = pOusters->getAlignment();

        // Alignment above 0 does not change from killing a monster.
        if (OldAlignValue > 0)
            return;

        // Compute how much alignment to gain.
        NewAlignValue = 0;
        if (pEnemy->isMonster()) {
            Monster* pMonster = dynamic_cast<Monster*>(pEnemy);
            Assert(pMonster != NULL);
            NewAlignValue = max(0, (int)(pMonster->getLevel() / 10));
        } else if (pEnemy->isSlayer()) {
            Slayer* pSlayer = dynamic_cast<Slayer*>(pEnemy);
            Assert(pSlayer != NULL);
            NewAlignValue = max(0, (int)(pSlayer->getSTR(ATTR_BASIC) + pSlayer->getDEX(ATTR_BASIC) +
                                         pSlayer->getINT(ATTR_BASIC) + pSlayer->getSkillDomainLevelSum()) /
                                       5);
        } else if (pEnemy->isVampire()) {
            Vampire* pVampire = dynamic_cast<Vampire*>(pEnemy);
            Assert(pVampire != NULL);
            NewAlignValue = max(0, (int)(pVampire->getLevel() / 5));
        }

        NewAlignValue = OldAlignValue + NewAlignValue;

        if (OldAlignValue != NewAlignValue) {
            // Report the alignment change in the packet.
            mi.addShortData(MODIFY_ALIGNMENT, NewAlignValue);

            WORD AlignmentSaveCount = pOusters->getAlignmentSaveCount();
            if (AlignmentSaveCount > ALIGNMENT_SAVE_PERIOD) {
                pOusters->saveAlignment(NewAlignValue);
                AlignmentSaveCount = 0;
            } else {
                pOusters->setAlignment(NewAlignValue);
                AlignmentSaveCount++;
            }
        }
    }

    // A change of alignment tier has to be announced to everyone else.
    Alignment beforeAlignment = de::gameContext().alignments().getAlignmentType(OldAlignValue);
    Alignment afterAlignment = de::gameContext().alignments().getAlignmentType(NewAlignValue);

    if (beforeAlignment != afterAlignment) {
        GCOtherModifyInfo gcOtherModifyInfo;
        gcOtherModifyInfo.setObjectID(pCreature->getObjectID());
        gcOtherModifyInfo.addShortData(MODIFY_ALIGNMENT, NewAlignValue);

        Zone* pZone = pCreature->getZone();
        Assert(pZone != NULL);
        pZone->broadcastPacket(pCreature->getX(), pCreature->getY(), &gcOtherModifyInfo, pCreature);
    }
}

//////////////////////////////////////////////////////////////////////////////
// Party-aware Slayer experience computation.
//////////////////////////////////////////////////////////////////////////////
void shareAttrExp(Slayer* pSlayer, Damage_t Damage, BYTE STRMultiplier, BYTE DEXMultiplier, BYTE INTMultiplier,
                  ModifyInfo& _ModifyInfo) {
    Assert(pSlayer != NULL);

    // No experience is given inside a PK zone.
    if (de::gameContext().pkZoneInfos().isPKZone(pSlayer->getZoneID()))
        return;

    // No experience is given inside a dynamic zone.
    if (pSlayer->getZone() != NULL && pSlayer->getZone()->isDynamicZone())
        return;

    VariableManager& variables = de::gameContext().variables();

    // Premium play grants more experience.
    GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(pSlayer->getPlayer());
    Assert(pGamePlayer != NULL);

    if (pGamePlayer->isPremiumPlay() ||
        pGamePlayer->isFamilyFreePass()) // pZone->isPayPlay() || pZone->isPremiumZone() )
    {
        Damage = getPercentValue(Damage, variables.getPremiumExpBonusPercent());
        STRMultiplier = getPercentValue(STRMultiplier, variables.getPremiumExpBonusPercent());
        DEXMultiplier = getPercentValue(DEXMultiplier, variables.getPremiumExpBonusPercent());
        INTMultiplier = getPercentValue(INTMultiplier, variables.getPremiumExpBonusPercent());
    }

    if (pGamePlayer->isPCRoomPlay()) {
        Damage = getPercentValue(Damage, variables.getPCRoomExpBonusPercent());
        STRMultiplier = getPercentValue(STRMultiplier, variables.getPCRoomExpBonusPercent());
        DEXMultiplier = getPercentValue(DEXMultiplier, variables.getPCRoomExpBonusPercent());
        INTMultiplier = getPercentValue(INTMultiplier, variables.getPCRoomExpBonusPercent());
    }

    int PartyID = pSlayer->getPartyID();
    if (PartyID != 0) {
        // When the player is in a party, the experience is shared with the
        // nearby party members through the local party manager.
        LocalPartyManager* pLPM = pSlayer->getLocalPartyManager();
        Assert(pLPM != NULL);
        pLPM->shareAttrExp(PartyID, pSlayer, Damage, STRMultiplier, DEXMultiplier, INTMultiplier, _ModifyInfo);
    } else {
        // Without a party the player gains it alone.
        divideAttrExp(pSlayer, Damage, STRMultiplier, DEXMultiplier, INTMultiplier, _ModifyInfo);
    }
}

//////////////////////////////////////////////////////////////////////////////
// Party-aware Vampire experience computation.
//////////////////////////////////////////////////////////////////////////////
void shareVampExp(Vampire* pVampire, Exp_t Point, ModifyInfo& _ModifyInfo) {
    Assert(pVampire != NULL);
    if (Point <= 0)
        return;

    // No experience is gained inside a PK zone.
    if (de::gameContext().pkZoneInfos().isPKZone(pVampire->getZoneID()))
        return;

    // No experience is gained inside a dynamic zone.
    if (pVampire->getZone() != NULL && pVampire->getZone()->isDynamicZone())
        return;

    // Premium play grants more experience.
    GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(pVampire->getPlayer());
    Assert(pGamePlayer != NULL);

    if (pGamePlayer->isPremiumPlay() ||
        pGamePlayer->isFamilyFreePass()) // pZone->isPayPlay() || pZone->isPremiumZone() )
    {
        Point = getPercentValue(Point, de::gameContext().variables().getPremiumExpBonusPercent());
    }

    if (pGamePlayer->isPCRoomPlay()) // pZone->isPayPlay() || pZone->isPremiumZone() )
    {
        Point = getPercentValue(Point, de::gameContext().variables().getPCRoomExpBonusPercent());
    }

    int PartyID = pVampire->getPartyID();
    if (PartyID != 0) {
        // When the player is in a party, the experience is shared with the
        // nearby party members through the local party manager.
        LocalPartyManager* pLPM = pVampire->getLocalPartyManager();
        Assert(pLPM != NULL);
        pLPM->shareVampireExp(PartyID, pVampire, Point, _ModifyInfo);
    } else {
        // Without a party the player gains it alone.
        increaseVampExp(pVampire, Point, _ModifyInfo);
    }
}

//////////////////////////////////////////////////////////////////////////////
// Party-aware Ousters experience computation.
//////////////////////////////////////////////////////////////////////////////
void shareOustersExp(Ousters* pOusters, Exp_t Point, ModifyInfo& _ModifyInfo) {
    Assert(pOusters != NULL);
    if (Point <= 0)
        return;

    // No experience is gained inside a PK zone.
    if (de::gameContext().pkZoneInfos().isPKZone(pOusters->getZoneID()))
        return;

    // No experience is gained inside a dynamic zone.
    if (pOusters->getZone() != NULL && pOusters->getZone()->isDynamicZone())
        return;

    // Premium play grants more experience.
    GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(pOusters->getPlayer());
    Assert(pGamePlayer != NULL);

    if (pGamePlayer->isPremiumPlay() || pGamePlayer->isFamilyFreePass()) {
        Point = getPercentValue(Point, de::gameContext().variables().getPremiumExpBonusPercent());
    }

    if (pGamePlayer->isPCRoomPlay()) {
        Point = getPercentValue(Point, de::gameContext().variables().getPCRoomExpBonusPercent());
    }

    int PartyID = pOusters->getPartyID();
    if (PartyID != 0) {
        // When the player is in a party, the experience is shared with the
        // nearby party members through the local party manager.
        LocalPartyManager* pLPM = pOusters->getLocalPartyManager();
        Assert(pLPM != NULL);
        pLPM->shareOustersExp(PartyID, pOusters, Point, _ModifyInfo);
    } else {
        // Without a party the player gains it alone.
        increaseOustersExp(pOusters, Point, _ModifyInfo);
    }
}


//////////////////////////////////////////////////////////////////////////////
// Computes Slayer stat (STR, DEX, INT) experience.
//////////////////////////////////////////////////////////////////////////////
void divideAttrExp(Slayer* pSlayer, Damage_t Damage, BYTE STRMultiplier, BYTE DEXMultiplier, BYTE INTMultiplier,
                   ModifyInfo& _ModifyInfo, int numPartyMember) {
    Assert(pSlayer != NULL);

    // STR has the largest multiplier.
    if (STRMultiplier > DEXMultiplier && STRMultiplier > INTMultiplier) {
        pSlayer->divideAttrExp(ATTR_KIND_STR, Damage, _ModifyInfo);
        // DEX has the largest multiplier.
    } else if (DEXMultiplier > STRMultiplier && DEXMultiplier > INTMultiplier) {
        pSlayer->divideAttrExp(ATTR_KIND_DEX, Damage, _ModifyInfo);
        // INT has the largest multiplier.
    } else if (INTMultiplier > STRMultiplier && INTMultiplier > DEXMultiplier) {
        pSlayer->divideAttrExp(ATTR_KIND_INT, Damage, _ModifyInfo);
    }

    return;
}

//////////////////////////////////////////////////////////////////////////////
// Computes Slayer skill experience.
//////////////////////////////////////////////////////////////////////////////
void increaseSkillExp(Slayer* pSlayer, SkillDomainType_t DomainType, SkillSlot* pSkillSlot, SkillInfo* pSkillInfo,
                      ModifyInfo& _ModifyInfo) {
    Assert(pSlayer != NULL);
    Assert(pSkillSlot != NULL);
    Assert(pSkillInfo != NULL);

    // No experience is given inside a PK zone.
    if (de::gameContext().pkZoneInfos().isPKZone(pSlayer->getZoneID()))
        return;

    // No experience is given inside a dynamic zone.
    if (pSlayer->getZone() != NULL && pSlayer->getZone()->isDynamicZone())
        return;

    VariableManager& variables = de::gameContext().variables();

    // Experience is not raised when the new level cannot pass the current domain level.
    Level_t CurrentLevel = pSkillSlot->getExpLevel();

    // Read the Slayer's current domain level.
    Level_t DomainLevel = pSlayer->getSkillDomainLevel(DomainType);

    // Read the domain's grade.
    SkillGrade Grade = de::gameContext().skillInfos().getGradeByDomainLevel(DomainLevel);

    // Read the limit level of the grade one above the current one.
    Level_t LimitLevel = de::gameContext().skillInfos().getLimitLevelByDomainGrade(SkillGrade(Grade + 1));

    if (CurrentLevel < LimitLevel) {
        // Compute the experience.
        Exp_t MaxExp = pSkillInfo->getSubSkill();
        Exp_t OldExp = pSkillSlot->getExp();
        Exp_t NewExp;

        Exp_t plusExp = 1;

        if (variables.getEventActivate() == 1) {
            plusExp = plusExp * variables.getExpRatio() / 100;
            plusExp = getPercentValue(plusExp, variables.getPremiumExpBonusPercent());
            if (pSlayer->isFlag(Effect::EFFECT_CLASS_BONUS_EXP))
                plusExp *= 2;
        }

        // Double experience.
        if (isAffectExp2X())
            plusExp *= 2;

        NewExp = min(MaxExp, OldExp + plusExp);

        pSkillSlot->setExp(NewExp);

        SkillLevel_t NewLevel = (NewExp * 100 / MaxExp) + 1;
        NewLevel = min((int)NewLevel, 100);

        ulong longData = (((ulong)pSkillSlot->getSkillType()) << 16) | (ulong)(NewExp / 10);
        _ModifyInfo.addLongData(MODIFY_SKILL_EXP, longData);

        // Quick fire will need a database change later.
        if (CurrentLevel != NewLevel) {
            pSkillSlot->setExpLevel(NewLevel);
            pSkillSlot->save();

            longData = (((ulong)pSkillSlot->getSkillType()) << 16) | (ulong)NewLevel;
            _ModifyInfo.addLongData(MODIFY_SKILL_LEVEL, longData);

            pSlayer->getGQuestManager()->skillLevelUp(pSkillSlot);
        } else {
            WORD SkillExpSaveCount = pSlayer->getSkillExpSaveCount();
            if (SkillExpSaveCount > SKILL_EXP_SAVE_PERIOD) {
                pSkillSlot->save();
                SkillExpSaveCount = 0;
            } else
                SkillExpSaveCount++;
            pSlayer->setSkillExpSaveCount(SkillExpSaveCount);
        }
    }
}

//////////////////////////////////////////////////////////////////////////////
// Computes Slayer domain experience.
//////////////////////////////////////////////////////////////////////////////
bool increaseDomainExp(Slayer* pSlayer, SkillDomainType_t Domain, Exp_t Point, ModifyInfo& _ModifyInfo,
                       Level_t EnemyLevel, int TargetNum) {
    if (pSlayer == NULL || Point == 0 || TargetNum == 0)
        return false;
    if (pSlayer->isAdvanced())
        return false;

    // No experience is given inside a PK zone.
    if (de::gameContext().pkZoneInfos().isPKZone(pSlayer->getZoneID()))
        return false;

    // No experience is given inside a dynamic zone.
    if (pSlayer->getZone() != NULL && pSlayer->getZone()->isDynamicZone())
        return false;

    VariableManager& variables = de::gameContext().variables();

    int PartyID = pSlayer->getPartyID();

    if (EnemyLevel != 0) {
        int levelDiff = (int)pSlayer->getLevel() - (int)EnemyLevel;


        if (levelDiff > 50)
            Point = getPercentValue(Point, 30);
        else if (levelDiff > 40)
            Point = getPercentValue(Point, 50);
        else if (levelDiff < -40)
            Point = getPercentValue(Point, 140);
        else if (levelDiff < -30)
            Point = getPercentValue(Point, 130);
        else if (levelDiff < -20)
            Point = getPercentValue(Point, 120);
    }

    if (TargetNum != -1)
        Point = Point * (TargetNum + 1) / 3;


    // Assuming the weapon held already matches the given domain,
    // the skill points granted vary with the weapon type.
    // by sigi. 2002.10.30
    Item* pWeapon = pSlayer->getWearItem(Slayer::WEAR_RIGHTHAND);
    if (pWeapon != NULL) {
        SkillLevel_t DomainLevel = pSlayer->getSkillDomainLevel(Domain);

        Point = computeSkillPointBonus(Domain, DomainLevel, pWeapon, Point);
    }


    // Premium play grants more experience.
    GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(pSlayer->getPlayer());
    Assert(pGamePlayer != NULL);

    if (pGamePlayer->isPremiumPlay() ||
        pGamePlayer->isFamilyFreePass()) // pZone->isPayPlay() || pZone->isPremiumZone() )
    {
        Point = getPercentValueEx(Point, variables.getPremiumExpBonusPercent());
    }

    if (pGamePlayer->isPCRoomPlay()) // pZone->isPayPlay() || pZone->isPremiumZone() )
    {
        Point = getPercentValueEx(Point, variables.getPCRoomExpBonusPercent());
    }

    if (PartyID != 0) {
        LocalPartyManager* pLPM = pSlayer->getLocalPartyManager();
        Assert(pLPM != NULL);

        int nMemberSize = pLPM->getAdjacentMemberSize(PartyID, pSlayer);
        switch (nMemberSize) {
        case 2:
            Point = getPercentValue(Point, 110);
            break;
        case 3:
            Point = getPercentValue(Point, 120);
            break;
        case 4:
            Point = getPercentValue(Point, 130);
            break;
        case 5:
            Point = getPercentValue(Point, 140);
            break;
        case 6:
            Point = getPercentValue(Point, 150);
            break;
        default:
            break;
        }
    }

    // Apply the point increase configured in VariableManager.
    if (variables.getExpRatio() > 100 && variables.getEventActivate() == 1)
        Point = getPercentValue(Point, variables.getExpRatio());

    if (pSlayer->isFlag(Effect::EFFECT_CLASS_BONUS_EXP))
        Point *= 2;

    // Double experience.
    if (isAffectExp2X())
        Point *= 2;

    Level_t CurDomainLevel = pSlayer->getSkillDomainLevel(Domain);
    Level_t NewDomainLevel = CurDomainLevel;
    SkillInfoManager& skillInfos = de::gameContext().skillInfos();
    SkillType_t LearnSkillType = skillInfos.getSkillTypeByLevel(Domain, CurDomainLevel);
    Exp_t NewGoalExp = 0;
    bool availiable = false;

    // Check whether a skill can be learned at the current level.
    if (LearnSkillType != 0) {
        // Domain experience rises only when that skill is already learned.
        if (pSlayer->hasSkill(LearnSkillType)) {
            availiable = true;
        }
    } else {
        availiable = true;
    }

    if (availiable) {
        bool isLevelUp = false;

        // The experience gained varies with the time of day.
        Point = (Exp_t)getPercentValue(Point, DomainExpTimebandFactor[getZoneTimeband(pSlayer->getZone())]);


        // Reward code.

        // Domain goal experience
        Exp_t GoalExp = pSlayer->getGoalExp(Domain);

        // New goal experience, which is all this code keeps: the domain has
        // no accumulated experience of its own.
        NewGoalExp = max(0, (int)(GoalExp - Point));

        pSlayer->setGoalExp(Domain, NewGoalExp);


        // With the goal experience at 0, check whether a level up is possible.
        if (NewGoalExp == 0 && CurDomainLevel != SLAYER_MAX_DOMAIN_LEVEL) {
            // Raise the domain level and announce any skill that becomes learnable.
            NewDomainLevel = CurDomainLevel + 1;

            // Take the goal experience from the domain info and reset the level.
            NewGoalExp =
                de::gameContext().skillDomains().getDomainInfo((SkillDomain)Domain, NewDomainLevel)->getGoalExp();

            pSlayer->setGoalExp(Domain, NewGoalExp);
            pSlayer->setSkillDomainLevel(Domain, NewDomainLevel);


            SkillType_t NewLearnSkillType = skillInfos.getSkillTypeByLevel(Domain, NewDomainLevel);

            // Check whether a skill can be learned at the new level.
            if (NewLearnSkillType != 0) {
                // When that skill is not learned yet, send the packet announcing it.
                if (pSlayer->hasSkill(NewLearnSkillType) == NULL) {
                    // The packet carries only the domain that leveled up; the
                    // client works out which skill that makes learnable.
                    GCLearnSkillReady readyPacket;
                    readyPacket.setSkillDomainType((SkillDomainType_t)Domain);
                    // send packet
                    pSlayer->getPlayer()->sendPacket(&readyPacket);
                }
            }

            isLevelUp = true;
        }


        Level_t DomainLevelSum = pSlayer->getSkillDomainLevelSum();

        // On a level up, when the sum of domain levels passes the maximum, the highest
        // domain level other than the current domain has to be lowered.
        if (isLevelUp && DomainLevelSum > SLAYER_MAX_DOMAIN_LEVEL) {
            SDomain ds[SKILL_DOMAIN_VAMPIRE];

            for (int i = 0; i < SKILL_DOMAIN_VAMPIRE; i++) {
                ds[i].DomainType = i;
                ds[i].DomainLevel = pSlayer->getSkillDomainLevel((SkillDomain)i);
            }

            // Find the highest level excluding the current domain.
            stable_sort(ds, ds + SKILL_DOMAIN_VAMPIRE, isBig());

            // After the sort, the first entry is the domain with the highest level.
            int j = 0;
            while (ds[j].DomainType == Domain) {
                j++;
                if (j > SKILL_DOMAIN_VAMPIRE) {
                    cerr << "Out of Skill Domain Range!!!" << endl;
                    Assert(false);
                }
            }

            // So ds[j] is the highest level domain that is not the current one.
            SkillDomainType_t DownDomainType = ds[j].DomainType;
            Level_t DownDomainLevel = ds[j].DomainLevel;


            // Disable the skill learnable in that domain, if there is one.
            SkillType_t eraseSkillType = skillInfos.getSkillTypeByLevel(DownDomainType, DownDomainLevel);
            SkillSlot* pESkillSlot = pSlayer->hasSkill(eraseSkillType);
            if (pESkillSlot != NULL) {
                pESkillSlot->setDisable();
            }

            // Lower the domain level.
            DownDomainLevel--;

            // Store the lowered domain's level.
            pSlayer->setSkillDomainLevel(DownDomainType, DownDomainLevel);

            // Look up the lowered domain's goal experience.
            // Look up the lowered domain's accumulated experience.
            Exp_t DownDomainGoalExp = de::gameContext()
                                          .skillDomains()
                                          .getDomainInfo((SkillDomain)DownDomainType, DownDomainLevel)
                                          ->getGoalExp();

            // Reset it to the downgraded goal experience.
            // After the downgrade, the domain experience matching that level is set.
            pSlayer->setGoalExp(DownDomainType, DownDomainGoalExp);

            StringStream DownSave;
            if (DownDomainType == SKILL_DOMAIN_BLADE) {
                DownSave << "BladeLevel = " << (int)DownDomainLevel << ",BladeGoalExp = " << (int)DownDomainGoalExp;
                _ModifyInfo.addShortData(MODIFY_BLADE_DOMAIN_LEVEL, DownDomainLevel);
                _ModifyInfo.addLongData(MODIFY_BLADE_DOMAIN_EXP, DownDomainGoalExp);
            } else if (DownDomainType == SKILL_DOMAIN_SWORD) {
                DownSave << "SwordLevel = " << (int)DownDomainLevel << ",SwordGoalExp = " << (int)DownDomainGoalExp;
                _ModifyInfo.addLongData(MODIFY_SWORD_DOMAIN_EXP, DownDomainGoalExp);
                _ModifyInfo.addShortData(MODIFY_SWORD_DOMAIN_LEVEL, DownDomainLevel);
            } else if (DownDomainType == SKILL_DOMAIN_GUN) {
                DownSave << "GunLevel = " << (int)DownDomainLevel << ",GunGoalExp = " << (int)DownDomainGoalExp;
                _ModifyInfo.addLongData(MODIFY_GUN_DOMAIN_EXP, DownDomainGoalExp);
                _ModifyInfo.addShortData(MODIFY_GUN_DOMAIN_LEVEL, DownDomainLevel);
            } else if (DownDomainType == SKILL_DOMAIN_ENCHANT) {
                DownSave << "EnchantLevel = " << (int)DownDomainLevel << ",EnchantGoalExp = " << (int)DownDomainGoalExp;
                _ModifyInfo.addShortData(MODIFY_ENCHANT_DOMAIN_LEVEL, DownDomainLevel);
                _ModifyInfo.addLongData(MODIFY_ENCHANT_DOMAIN_EXP, DownDomainGoalExp);
            } else if (DownDomainType == SKILL_DOMAIN_HEAL) {
                DownSave << "HealLevel = " << (int)DownDomainLevel << ",HealGoalExp = " << (int)DownDomainGoalExp;
                _ModifyInfo.addShortData(MODIFY_HEAL_DOMAIN_LEVEL, DownDomainLevel);
                _ModifyInfo.addLongData(MODIFY_HEAL_DOMAIN_EXP, DownDomainGoalExp);
            } else if (DownDomainType == SKILL_DOMAIN_ETC) {
                DownSave << "ETCLevel = " << (int)DownDomainLevel << ",ETCGoalExp = " << (int)DownDomainGoalExp;
                _ModifyInfo.addShortData(MODIFY_ETC_DOMAIN_LEVEL, DownDomainLevel);
            } else {
            }

            // Save the lowered domain level.
            pSlayer->tinysave(DownSave.toString());
        }

        WORD DomainExpSaveCount = pSlayer->getDomainExpSaveCount();

        if (Domain == SKILL_DOMAIN_BLADE) {
            StringStream attrsave;
            if (isLevelUp) {
                attrsave << "BladeLevel = " << (int)NewDomainLevel << ",BladeGoalExp = " << (int)NewGoalExp;
                _ModifyInfo.addShortData(MODIFY_BLADE_DOMAIN_LEVEL, NewDomainLevel);
                pSlayer->tinysave(attrsave.toString());
            } else {
                attrsave << "BladeGoalExp = " << (int)NewGoalExp;

                if (DomainExpSaveCount > DOMAIN_EXP_SAVE_PERIOD) {
                    pSlayer->tinysave(attrsave.toString());
                    DomainExpSaveCount = 0;
                } else
                    DomainExpSaveCount++;
            }

            _ModifyInfo.addLongData(MODIFY_BLADE_DOMAIN_EXP, NewGoalExp);
        } else if (Domain == SKILL_DOMAIN_SWORD) {
            StringStream attrsave;
            if (isLevelUp) {
                attrsave << "SwordLevel = " << (int)NewDomainLevel << ",SwordGoalExp = " << (int)NewGoalExp;
                _ModifyInfo.addShortData(MODIFY_SWORD_DOMAIN_LEVEL, NewDomainLevel);
                pSlayer->tinysave(attrsave.toString());
            } else {
                attrsave << "SwordGoalExp = " << (int)NewGoalExp;

                if (DomainExpSaveCount > DOMAIN_EXP_SAVE_PERIOD) {
                    pSlayer->tinysave(attrsave.toString());
                    DomainExpSaveCount = 0;
                } else
                    DomainExpSaveCount++;
            }

            _ModifyInfo.addLongData(MODIFY_SWORD_DOMAIN_EXP, NewGoalExp);
        } else if (Domain == SKILL_DOMAIN_GUN) {
            StringStream attrsave;
            if (isLevelUp) {
                attrsave << "GunLevel = " << (int)NewDomainLevel << ",GunGoalExp = " << (int)NewGoalExp;
                _ModifyInfo.addShortData(MODIFY_GUN_DOMAIN_LEVEL, NewDomainLevel);
                pSlayer->tinysave(attrsave.toString());
            } else {
                attrsave << "GunGoalExp = " << (int)NewGoalExp;

                if (DomainExpSaveCount > DOMAIN_EXP_SAVE_PERIOD) {
                    pSlayer->tinysave(attrsave.toString());
                    DomainExpSaveCount = 0;
                } else
                    DomainExpSaveCount++;
            }

            _ModifyInfo.addLongData(MODIFY_GUN_DOMAIN_EXP, NewGoalExp);
        } else if (Domain == SKILL_DOMAIN_ENCHANT) {
            StringStream attrsave;
            if (isLevelUp) {
                attrsave << "EnchantLevel = " << (int)NewDomainLevel << ",EnchantGoalExp = " << (int)NewGoalExp;
                _ModifyInfo.addShortData(MODIFY_ENCHANT_DOMAIN_LEVEL, NewDomainLevel);
                pSlayer->tinysave(attrsave.toString());
            } else {
                attrsave << "EnchantGoalExp = " << (int)NewGoalExp;

                if (DomainExpSaveCount > DOMAIN_EXP_SAVE_PERIOD) {
                    pSlayer->tinysave(attrsave.toString());
                    DomainExpSaveCount = 0;
                } else
                    DomainExpSaveCount++;
            }

            _ModifyInfo.addLongData(MODIFY_ENCHANT_DOMAIN_EXP, NewGoalExp);
        } else if (Domain == SKILL_DOMAIN_HEAL) {
            StringStream attrsave;
            if (isLevelUp) {
                attrsave << "HealLevel = " << (int)NewDomainLevel << ",HealGoalExp = " << (int)NewGoalExp;
                _ModifyInfo.addShortData(MODIFY_HEAL_DOMAIN_LEVEL, NewDomainLevel);
                pSlayer->tinysave(attrsave.toString());
            } else {
                attrsave << "HealGoalExp = " << (int)NewGoalExp;

                if (DomainExpSaveCount > DOMAIN_EXP_SAVE_PERIOD) {
                    pSlayer->tinysave(attrsave.toString());
                    DomainExpSaveCount = 0;
                } else
                    DomainExpSaveCount++;
            }

            _ModifyInfo.addLongData(MODIFY_HEAL_DOMAIN_EXP, NewGoalExp);
        } else if (Domain == SKILL_DOMAIN_ETC) {
            StringStream attrsave;
            if (isLevelUp) {
                attrsave << "ETCLevel = " << (int)NewDomainLevel << ",ETCGoalExp = " << (int)NewGoalExp;
                _ModifyInfo.addShortData(MODIFY_ETC_DOMAIN_LEVEL, NewDomainLevel);
                pSlayer->tinysave(attrsave.toString());
            } else {
                attrsave << "ETCGoalExp = " << (int)NewGoalExp;

                if (DomainExpSaveCount > DOMAIN_EXP_SAVE_PERIOD) {
                    pSlayer->tinysave(attrsave.toString());
                    DomainExpSaveCount = 0;
                } else
                    DomainExpSaveCount++;
            }

            _ModifyInfo.addLongData(MODIFY_ETC_DOMAIN_EXP, NewGoalExp);
        } else {
        }

        // A grand master gets the effect attached.
        // by sigi. 2002.11.8
        if (isLevelUp && DomainLevelSum >= GRADE_GRAND_MASTER_LIMIT_LEVEL) {
            // One domain is past the grand master level and the effect is not attached yet.
            if (pSlayer->getHighestSkillDomainLevel() >= GRADE_GRAND_MASTER_LIMIT_LEVEL &&
                !pSlayer->isFlag(Effect::EFFECT_CLASS_GRAND_MASTER_SLAYER)) {
                EffectGrandMasterSlayer* pEffect = new EffectGrandMasterSlayer(pSlayer);
                pEffect->setDeadline(999999);

                pSlayer->getEffectManager()->addEffect(pEffect);

                // affect() sets the flag and broadcasts to the surroundings.
                pEffect->affect();
            } else if (pSlayer->getHighestSkillDomainLevel() == 130 || pSlayer->getHighestSkillDomainLevel() == 150) {
                Effect* pEffect = pSlayer->findEffect(Effect::EFFECT_CLASS_GRAND_MASTER_SLAYER);
                if (pEffect != NULL)
                    pEffect->affect();
            }
        }

        pSlayer->setDomainExpSaveCount(DomainExpSaveCount);

        // Refill health on any level up.
        if (isLevelUp) {
            SLAYER_RECORD prev;
            pSlayer->getSlayerRecord(prev);
            pSlayer->initAllStat();
            healCreatureForLevelUp(pSlayer, _ModifyInfo, &prev);

            // Show the level up effect as well.
            sendEffectLevelUp(pSlayer);

            pSlayer->whenQuestLevelUpgrade();
        }
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////////
// Computes Vampire experience.
//////////////////////////////////////////////////////////////////////////////
void increaseVampExp(Vampire* pVampire, Exp_t Point, ModifyInfo& _ModifyInfo) {
    Assert(pVampire != NULL);
    if (Point <= 0)
        return;
    if (pVampire->isAdvanced())
        return;

    // No experience is gained while in bat form.
    if (pVampire->isFlag(Effect::EFFECT_CLASS_TRANSFORM_TO_BAT))
        return;

    // No experience is gained inside a dynamic zone.
    if (pVampire->getZone() != NULL && pVampire->getZone()->isDynamicZone())
        return;

    Level_t curLevel = pVampire->getLevel();

    // Increase configured in VariableManager.
    if (de::gameContext().variables().getExpRatio() > 100 && de::gameContext().variables().getEventActivate() == 1)
        Point = getPercentValue(Point, de::gameContext().variables().getExpRatio());

    if (pVampire->isFlag(Effect::EFFECT_CLASS_BONUS_EXP))
        Point *= 2;

    // Double experience.
    if (isAffectExp2X())
        Point *= 2;


    Exp_t OldGoalExp = pVampire->getGoalExp();
    Exp_t NewGoalExp = max(0, (int)(OldGoalExp - Point));

    pVampire->setGoalExp(NewGoalExp);


    // When the goal experience is not 0 or the level is already at the maximum, only the
    // experience is stored and the level does not rise.
    if (NewGoalExp > 0 || curLevel == VAMPIRE_MAX_LEVEL) {
        _ModifyInfo.addLongData(MODIFY_VAMP_GOAL_EXP, NewGoalExp);
        WORD ExpSaveCount = pVampire->getExpSaveCount();

        // Once the experience save count reaches its threshold, save and
        // reset the count.
        if (ExpSaveCount > VAMPIRE_EXP_SAVE_PERIOD) {
            StringStream attrsave;
            attrsave << "GoalExp = " << NewGoalExp;
            pVampire->tinysave(attrsave.toString());

            ExpSaveCount = 0;
        } else
            ExpSaveCount++;

        pVampire->setExpSaveCount(ExpSaveCount);
    } else {
        // Level up.
        VAMPIRE_RECORD prev;
        pVampire->getVampireRecord(prev);

        curLevel++;

        pVampire->setLevel(curLevel);
        _ModifyInfo.addShortData(MODIFY_LEVEL, curLevel);

        // add bonus point
        Bonus_t bonus = pVampire->getBonus();

        {
            // The bonus is always 3, regardless of level.
            bonus += 3;
        }

        pVampire->setBonus(bonus);
        _ModifyInfo.addShortData(MODIFY_BONUS_POINT, bonus);

        VampEXPInfo* pNextExpInfo = de::gameContext().vampireExp().getVampEXPInfo(curLevel);
        Exp_t NextGoalExp = pNextExpInfo->getGoalExp();

        pVampire->setGoalExp(NextGoalExp);
        _ModifyInfo.addLongData(MODIFY_VAMP_GOAL_EXP, NextGoalExp);

        StringStream sav;
        sav << "Level = "
            << (int)curLevel
            //			<< ",Exp = " << (int)pBeforeExpInfo->getAccumExp()
            << ",GoalExp = " << (int)NextGoalExp << ",Bonus = " << (int)bonus;
        pVampire->tinysave(sav.toString());

        // Announce any skill that the new level makes learnable.
        SkillType_t NewLearnSkillType =
            de::gameContext().skillInfos().getSkillTypeByLevel(SKILL_DOMAIN_VAMPIRE, curLevel);
        if (NewLearnSkillType != 0) {
            // When that skill is not learned yet, send the packet announcing it.
            if (pVampire->hasSkill(NewLearnSkillType) == NULL) {
                // The packet carries only the domain that leveled up; the
                // client works out which skill that makes learnable.
                GCLearnSkillReady readyPacket;
                readyPacket.setSkillDomainType(SKILL_DOMAIN_VAMPIRE);
                pVampire->getPlayer()->sendPacket(&readyPacket);
            }
        }

        healCreatureForLevelUp(pVampire, _ModifyInfo, &prev);

        // Show the level up effect as well.
        sendEffectLevelUp(pVampire);

        pVampire->whenQuestLevelUpgrade();

        // A grand master gets the effect attached.
        // The level is past the grand master level and the effect is not attached yet.
        // by sigi. 2002.11.9
        if (curLevel >= GRADE_GRAND_MASTER_LIMIT_LEVEL &&
            !pVampire->isFlag(Effect::EFFECT_CLASS_GRAND_MASTER_VAMPIRE)) {
            EffectGrandMasterVampire* pEffect = new EffectGrandMasterVampire(pVampire);
            pEffect->setDeadline(999999);

            pVampire->getEffectManager()->addEffect(pEffect);

            // affect() sets the flag and broadcasts to the surroundings.
            pEffect->affect();
        } else if (curLevel == 130 || curLevel == 150) {
            Effect* pEffect = pVampire->findEffect(Effect::EFFECT_CLASS_GRAND_MASTER_VAMPIRE);
            if (pEffect != NULL)
                pEffect->affect();
        }
    }
}

//////////////////////////////////////////////////////////////////////////////
// Computes Ousters experience.
//////////////////////////////////////////////////////////////////////////////
void increaseOustersExp(Ousters* pOusters, Exp_t Point, ModifyInfo& _ModifyInfo) {
    Assert(pOusters != NULL);
    if (Point <= 0)
        return;
    if (pOusters->isAdvanced())
        return;

    Level_t curLevel = pOusters->getLevel();

    // No experience is gained inside a dynamic zone.
    if (pOusters->getZone() != NULL && pOusters->getZone()->isDynamicZone())
        return;

    // Increase configured in VariableManager.
    if (de::gameContext().variables().getExpRatio() > 100 && de::gameContext().variables().getEventActivate() == 1)
        Point = getPercentValue(Point, de::gameContext().variables().getExpRatio());

    if (pOusters->isFlag(Effect::EFFECT_CLASS_BONUS_EXP))
        Point *= 2;

    // Double experience.
    if (isAffectExp2X())
        Point *= 2;

    // The experience gained varies with the time of day.
    Point = (Exp_t)getPercentValue(Point, DomainExpTimebandFactor[getZoneTimeband(pOusters->getZone())]);


    Exp_t OldGoalExp = pOusters->getGoalExp();
    Exp_t NewGoalExp = max(0, (int)(OldGoalExp - Point));

    // Accumulated experience should rise by as much as the goal experience fell.

    pOusters->setGoalExp(NewGoalExp);


    if (NewGoalExp > 0 || curLevel == OUSTERS_MAX_LEVEL) {
        WORD ExpSaveCount = pOusters->getExpSaveCount();
        _ModifyInfo.addLongData(MODIFY_OUSTERS_GOAL_EXP, NewGoalExp);

        // Once the experience save count reaches its threshold, save and
        // reset the count.
        if (ExpSaveCount > OUSTERS_EXP_SAVE_PERIOD) {
            StringStream attrsave;
            attrsave << "GoalExp = " << NewGoalExp;
            pOusters->tinysave(attrsave.toString());

            ExpSaveCount = 0;
        } else
            ExpSaveCount++;

        pOusters->setExpSaveCount(ExpSaveCount);
    } else {
        // Level up.
        OUSTERS_RECORD prev;
        pOusters->getOustersRecord(prev);

        curLevel++;
        pOusters->setLevel(curLevel);

        OustersEXPInfo* pNextExpInfo = de::gameContext().oustersExp().getOustersEXPInfo(curLevel);
        Exp_t NextGoalExp = pNextExpInfo->getGoalExp();

        // add bonus point
        Bonus_t bonus = pOusters->getBonus();
        SkillBonus_t skillBonus = pOusters->getSkillBonus();

        bonus += 3;
        skillBonus += pNextExpInfo->getSkillPointBonus();

        pOusters->setBonus(bonus);
        pOusters->setSkillBonus(skillBonus);

        _ModifyInfo.addShortData(MODIFY_LEVEL, curLevel);
        _ModifyInfo.addShortData(MODIFY_BONUS_POINT, bonus);
        _ModifyInfo.addShortData(MODIFY_SKILL_BONUS_POINT, skillBonus);

        pOusters->setGoalExp(NextGoalExp);
        _ModifyInfo.addLongData(MODIFY_OUSTERS_GOAL_EXP, NextGoalExp);

        StringStream sav;
        sav << "Level = "
            << (int)curLevel
            //			<< ",Exp = " << (int)pBeforeExpInfo->getAccumExp()
            << ",GoalExp = " << (int)NextGoalExp << ",Bonus = " << (int)bonus << ",SkillBonus = " << (int)skillBonus;
        pOusters->tinysave(sav.toString());

        // Announce any skill that the new level makes learnable.
        SkillType_t NewLearnSkillType =
            de::gameContext().skillInfos().getSkillTypeByLevel(SKILL_DOMAIN_OUSTERS, curLevel);
        if (NewLearnSkillType != 0) {
            // When that skill is not learned yet, send the packet announcing it.
            if (pOusters->hasSkill(NewLearnSkillType) == NULL) {
                // The packet carries only the domain that leveled up; the
                // client works out which skill that makes learnable.
                GCLearnSkillReady readyPacket;
                readyPacket.setSkillDomainType(SKILL_DOMAIN_OUSTERS);
                pOusters->getPlayer()->sendPacket(&readyPacket);
            }
        }

        healCreatureForLevelUp(pOusters, _ModifyInfo, &prev);

        // Show the level up effect as well.
        sendEffectLevelUp(pOusters);

        pOusters->whenQuestLevelUpgrade();

        // A grand master gets the effect attached.
        // The level is past the grand master level and the effect is not attached yet.
        // by sigi. 2002.11.9
        if (curLevel >= GRADE_GRAND_MASTER_LIMIT_LEVEL &&
            !pOusters->isFlag(Effect::EFFECT_CLASS_GRAND_MASTER_OUSTERS)) {
            EffectGrandMasterOusters* pEffect = new EffectGrandMasterOusters(pOusters);
            pEffect->setDeadline(999999);

            pOusters->getEffectManager()->addEffect(pEffect);

            // affect() sets the flag and broadcasts to the surroundings.
            pEffect->affect();
        } else if (curLevel == 130 || curLevel == 150) {
            Effect* pEffect = pOusters->findEffect(Effect::EFFECT_CLASS_GRAND_MASTER_OUSTERS);
            if (pEffect != NULL)
                pEffect->affect();
        }
    }
}

//////////////////////////////////////////////////////////////////////////////
// Computes Slayer and Vampire fame.
//////////////////////////////////////////////////////////////////////////////
void increaseFame(Creature* pCreature, uint amount) {
    if (pCreature == NULL)
        return;

    // Fame is not raised inside a PK zone.
    if (de::gameContext().pkZoneInfos().isPKZone(pCreature->getZoneID()))
        return;

    // Fame is not raised inside a dynamic zone.
    if (pCreature->getZone() != NULL && pCreature->getZone()->isDynamicZone())
        return;

    // With a local party, the amount gained varies with the number of party members.
    int PartyID = pCreature->getPartyID();
    if (PartyID != 0) {
        LocalPartyManager* pLPM = pCreature->getLocalPartyManager();
        Assert(pLPM != NULL);

        int nMemberSize = pLPM->getAdjacentMemberSize(PartyID, pCreature);
        switch (nMemberSize) {
        case 2:
            amount = getPercentValue(amount, 120);
            break;
        case 3:
            amount = getPercentValue(amount, 140);
            break;
        case 4:
            amount = getPercentValue(amount, 160);
            break;
        case 5:
            amount = getPercentValue(amount, 180);
            break;
        case 6:
            amount = getPercentValue(amount, 200);
            break;
        default:
            break;
        }
    }

    StringStream attrsave;

    if (pCreature->isSlayer()) {
        Slayer* pSlayer = dynamic_cast<Slayer*>(pCreature);
        Fame_t OldFame = pSlayer->getFame();
        Fame_t NewFame = OldFame + amount;

        NewFame = min(2000000000, (int)NewFame);

        if (NewFame != OldFame) {
            WORD FameSaveCount = pSlayer->getFameSaveCount();
            if (FameSaveCount > FAME_SAVE_PERIOD) {
                attrsave << "Fame = " << (int)NewFame;
                pSlayer->tinysave(attrsave.toString());

                FameSaveCount = 0;
            } else
                FameSaveCount++;

            pSlayer->setFameSaveCount(FameSaveCount);

            // The fame value is set whether or not it is saved.
            pSlayer->setFame(NewFame);
        }
    } else if (pCreature->isVampire()) {
        Vampire* pVampire = dynamic_cast<Vampire*>(pCreature);
        Fame_t OldFame = pVampire->getFame();
        Fame_t NewFame = OldFame + amount;

        NewFame = min(2000000000, (int)NewFame);

        if (NewFame != OldFame) {
            WORD FameSaveCount = pVampire->getFameSaveCount();
            if (FameSaveCount > FAME_SAVE_PERIOD) {
                attrsave << "Fame = " << (int)NewFame;
                pVampire->tinysave(attrsave.toString());

                FameSaveCount = 0;
            } else
                FameSaveCount++;

            pVampire->setFameSaveCount(FameSaveCount);

            // The fame value is set whether or not it is saved.
            pVampire->setFame(NewFame);
        }
    } else if (pCreature->isOusters()) {
        Ousters* pOusters = dynamic_cast<Ousters*>(pCreature);
        Fame_t OldFame = pOusters->getFame();
        Fame_t NewFame = OldFame + amount;

        NewFame = min(2000000000, (int)NewFame);

        if (NewFame != OldFame) {
            WORD FameSaveCount = pOusters->getFameSaveCount();
            if (FameSaveCount > FAME_SAVE_PERIOD) {
                attrsave << "Fame = " << (int)NewFame;
                pOusters->tinysave(attrsave.toString());

                FameSaveCount = 0;
            } else
                FameSaveCount++;

            pOusters->setFameSaveCount(FameSaveCount);

            // The fame value is set whether or not it is saved.
            pOusters->setFame(NewFame);
        }
    }
}

//////////////////////////////////////////////////////////////////////////////
// Fills HP and MP to full when any stat has risen.
// For Slayers.
//////////////////////////////////////////////////////////////////////////////
void healCreatureForLevelUp(Slayer* pSlayer, ModifyInfo& _ModifyInfo, SLAYER_RECORD* prev) {
    // Recompute the stats.
    pSlayer->initAllStat();

    // Stats rose, so the derived values changed and are sent out.
    pSlayer->sendRealWearingInfo();
    pSlayer->addModifyInfo(*prev, _ModifyInfo);

    if (pSlayer->isDead())
        return;

    // Fill HP and MP to full when any stat has risen.
    HP_t OldHP = pSlayer->getHP(ATTR_CURRENT);
    HP_t OldMP = pSlayer->getMP(ATTR_CURRENT);

    // Fill to full.
    pSlayer->setHP(pSlayer->getHP(ATTR_MAX), ATTR_CURRENT);
    pSlayer->setMP(pSlayer->getMP(ATTR_MAX), ATTR_CURRENT);

    HP_t NewHP = pSlayer->getHP(ATTR_CURRENT);
    HP_t NewMP = pSlayer->getMP(ATTR_CURRENT);

    // HP changed.
    if (OldHP != NewHP) {
        _ModifyInfo.addShortData(MODIFY_CURRENT_HP, NewHP);

        // Broadcast the new health to the surroundings.
        GCStatusCurrentHP gcStatusCurrentHP;
        gcStatusCurrentHP.setObjectID(pSlayer->getObjectID());
        gcStatusCurrentHP.setCurrentHP(NewHP);
        Zone* pZone = pSlayer->getZone();
        Assert(pZone != NULL);
        pZone->broadcastPacket(pSlayer->getX(), pSlayer->getY(), &gcStatusCurrentHP, pSlayer);
    }

    // MP changed.
    if (OldMP != NewMP) {
        _ModifyInfo.addShortData(MODIFY_CURRENT_MP, NewMP);
    }
}

//////////////////////////////////////////////////////////////////////////////
// Fills HP to full when stats have risen. For Vampires.
//////////////////////////////////////////////////////////////////////////////
void healCreatureForLevelUp(Vampire* pVampire, ModifyInfo& _ModifyInfo, VAMPIRE_RECORD* prev) {
    // Recompute the stats.
    pVampire->initAllStat();

    // Stats rose, so the derived values changed and are sent out.
    pVampire->sendRealWearingInfo();
    pVampire->addModifyInfo(*prev, _ModifyInfo);

    if (pVampire->isDead())
        return;

    HP_t OldHP = pVampire->getHP(ATTR_CURRENT);

    // Fill to full.
    pVampire->setHP(pVampire->getHP(ATTR_MAX), ATTR_CURRENT);

    HP_t NewHP = pVampire->getHP(ATTR_CURRENT);

    // HP changed.
    if (OldHP != NewHP) {
        _ModifyInfo.addShortData(MODIFY_CURRENT_HP, NewHP);

        // Broadcast the new health to the surroundings.
        GCStatusCurrentHP gcStatusCurrentHP;
        gcStatusCurrentHP.setObjectID(pVampire->getObjectID());
        gcStatusCurrentHP.setCurrentHP(NewHP);
        Zone* pZone = pVampire->getZone();
        Assert(pZone != NULL);
        pZone->broadcastPacket(pVampire->getX(), pVampire->getY(), &gcStatusCurrentHP, pVampire);
    }
}

//////////////////////////////////////////////////////////////////////////////
// Fills HP and MP to full when stats have risen. For Ousters.
// -- 2003.04.19 by bezz
//////////////////////////////////////////////////////////////////////////////
void healCreatureForLevelUp(Ousters* pOusters, ModifyInfo& _ModifyInfo, OUSTERS_RECORD* prev) {
    // Recompute the stats.
    pOusters->initAllStat();

    // Stats rose, so the derived values changed and are sent out.
    pOusters->sendRealWearingInfo();
    pOusters->addModifyInfo(*prev, _ModifyInfo);

    if (pOusters->isDead())
        return;

    HP_t OldHP = pOusters->getHP(ATTR_CURRENT);
    MP_t OldMP = pOusters->getMP(ATTR_CURRENT);

    // Fill to full.
    pOusters->setHP(pOusters->getHP(ATTR_MAX), ATTR_CURRENT);
    pOusters->setMP(pOusters->getMP(ATTR_MAX), ATTR_CURRENT);

    HP_t NewHP = pOusters->getHP(ATTR_CURRENT);
    MP_t NewMP = pOusters->getMP(ATTR_CURRENT);

    // HP changed.
    if (OldHP != NewHP) {
        _ModifyInfo.addShortData(MODIFY_CURRENT_HP, NewHP);

        // Broadcast the new health to the surroundings.
        GCStatusCurrentHP gcStatusCurrentHP;
        gcStatusCurrentHP.setObjectID(pOusters->getObjectID());
        gcStatusCurrentHP.setCurrentHP(NewHP);
        Zone* pZone = pOusters->getZone();
        Assert(pZone != NULL);
        pZone->broadcastPacket(pOusters->getX(), pOusters->getY(), &gcStatusCurrentHP, pOusters);
    }

    if (OldMP != NewMP)
        _ModifyInfo.addShortData(MODIFY_CURRENT_MP, NewMP);
}

Exp_t computeSkillPointBonus(SkillDomainType_t Domain, SkillLevel_t DomainLevel, Item* pWeapon, Exp_t Point) {
    Assert(pWeapon != NULL);

    ItemType_t itemType = pWeapon->getItemType();
    ItemType_t bestItemType =
        de::gameContext().skillDomains().getDomainInfo((SkillDomain)Domain, DomainLevel)->getBestItemType();

    Exp_t newPoint;

    if (pWeapon->isUnique()) {
        newPoint = getPercentValue(Point, 120);
    } else if (itemType <= bestItemType) {
        newPoint = Point * (itemType + 1) / (bestItemType + 1);
    } else {
        Exp_t Point120 = getPercentValue(Point, 120);
        newPoint = Point * itemType / (bestItemType + 1);

        newPoint = min(Point120, newPoint);
    }


    // by sigi. 2002.11.5
    newPoint = max(1, (int)newPoint);

    return newPoint;
}
