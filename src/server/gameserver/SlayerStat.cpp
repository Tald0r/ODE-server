//////////////////////////////////////////////////////////////////////////////
// FileName 	: SlayerStat.cpp
// Description	: Slayer stat computation: the castle skills, the all-stat recalculation and the item, option and blood bible contributions to it.
//////////////////////////////////////////////////////////////////////////////

#include "AbilityBalance.h"
#include "BloodBibleBonus.h"
#include "BloodBibleBonusManager.h"
#include "BloodBibleSignInfo.h"
#include "CastleInfoManager.h"
#include "CombatInfoManager.h"
#include "CoreZap.h"
#include "DefaultOptionSetInfo.h"
#include "Effect.h"
#include "GCAddEffect.h"
#include "GCBloodBibleSignInfo.h"
#include "GCChangeShape.h"
#include "GCModifyInformation.h"
#include "GCRemoveEffect.h"
#include "GameContext.h"
#include "GamePlayer.h"
#include "HolyLandRaceBonus.h"
#include "ItemInfo.h"
#include "ItemInfoManager.h"
#include "ItemUtil.h"
#include "LevelWarZoneInfoManager.h"
#include "Monster.h"
#include "OptionInfo.h"
#include "Ousters.h"
#include "PetInfo.h"
#include "Player.h"
#include "RankBonus.h"
#include "SkillInfo.h"
#include "Slayer.h"
#include "SweeperBonus.h"
#include "SweeperBonusManager.h"
#include "Vampire.h"
#include "VariableManager.h"
#include "WarSystem.h"
#include "Zone.h"
#include "domain/Formulas.h"
#include "item/OustersStone.h"
#include "item/OustersWristlet.h"
#include "skill/CastleSkillSlot.h"
#include "skill/EffectBerserker.h"
#include "skill/EffectBless.h"
#include "skill/EffectBlunting.h"
#include "skill/EffectChargingPower.h"
#include "skill/EffectConcealment.h"
#include "skill/EffectCrossGuard.h"
#include "skill/EffectDancingSword.h"
#include "skill/EffectDeath.h"
#include "skill/EffectDoom.h"
#include "skill/EffectEvade.h"
#include "skill/EffectExpansion.h"
#include "skill/EffectExtreme.h"
#include "skill/EffectGhostBlade.h"
#include "skill/EffectGnomesWhisper.h"
#include "skill/EffectGroundBless.h"
#include "skill/EffectHandsOfFire.h"
#include "skill/EffectHolyArmor.h"
#include "skill/EffectIntimateGrail.h"
#include "skill/EffectMephisto.h"
#include "skill/EffectMindControl.h"
#include "skill/EffectObservingEye.h"
#include "skill/EffectParalyze.h"
#include "skill/EffectPotentialExplosion.h"
#include "skill/EffectProtectionFromAcid.h"
#include "skill/EffectProtectionFromBlood.h"
#include "skill/EffectProtectionFromCurse.h"
#include "skill/EffectProtectionFromPoison.h"
#include "skill/EffectReactiveArmor.h"
#include "skill/EffectRediance.h"
#include "skill/EffectRingOfFlare.h"
#include "skill/EffectSeduction.h"
#include "skill/EffectSharpChakram.h"
#include "skill/EffectStriking.h"
#include "skill/EffectSummonCasket.h"
#include "skill/EffectTransformToBat.h"
#include "skill/EffectTransformToWerwolf.h"
#include "skill/EffectTransformToWolf.h"
#include "skill/EffectWaterBarrier.h"
#include "skill/EffectWhitsuntide.h"
#include "skill/OustersCastleSkillSlot.h"
#include "skill/VampireCastleSkillSlot.h"

void Slayer::initCastleSkill() {
    removeAllCastleSkill();

    if (!getZone()->isHolyLand())
        return;

    list<CastleInfo*> pCastleInfoList = de::gameContext().castleInfos().getGuildCastleInfos(getGuildID());
    if (pCastleInfoList.empty())
        return;

    list<CastleInfo*>::iterator itr = pCastleInfoList.begin();

    for (; itr != pCastleInfoList.end(); itr++) {
        SkillType_t CastleSkillType =
            de::gameContext().castleInfos().getCastleSkillType((*itr)->getZoneID(), getGuildID());
        if (CastleSkillType == SKILL_MAX)
            continue;

        SkillInfo* pSkillInfo = de::gameContext().skillInfos().getSkillInfo(CastleSkillType);
        Assert(pSkillInfo != NULL);

        Turn_t Delay = pSkillInfo->getMaxDelay();

        CastleSkillSlot* pCastleSkillSlot = new CastleSkillSlot();

        pCastleSkillSlot->setName(m_Name);
        pCastleSkillSlot->setSkillType(CastleSkillType);
        pCastleSkillSlot->setInterval(Delay);
        pCastleSkillSlot->setExpLevel(0);
        pCastleSkillSlot->setExp(1);
        pCastleSkillSlot->setRunTime();

        addSkill(pCastleSkillSlot);
    }
}

void Slayer::initAllStat(int numPartyMember) {
    BASIC_ATTR attr{};
    Creature::CreatureClass CClass = getCreatureClass();

    m_Resist[MAGIC_DOMAIN_NO_DOMAIN] = 0;
    m_Resist[MAGIC_DOMAIN_POISON] = 0;
    m_Resist[MAGIC_DOMAIN_ACID] = 0;
    m_Resist[MAGIC_DOMAIN_CURSE] = 0;
    m_Resist[MAGIC_DOMAIN_BLOOD] = 0;

    // Reset the BloodBible bonus values.
    m_ConsumeMPRatio = 0;
    m_GamblePriceRatio = 0;
    m_PotionPriceRatio = 0;
    m_MagicBonusDamage = 0;
    m_PhysicBonusDamage = 0;
    m_MagicDamageReduce = 0;
    m_PhysicDamageReduce = 0;

    //////////////////////////////////////////////////////////////////////////////
    // First reset the basic attributes, then check the effects that
    // modify them.
    //////////////////////////////////////////////////////////////////////////////
    m_STR[ATTR_CURRENT] = m_STR[ATTR_MAX] = m_STR[ATTR_BASIC] = m_pAttrs[ATTR_KIND_STR]->getLevel();
    m_DEX[ATTR_CURRENT] = m_DEX[ATTR_MAX] = m_DEX[ATTR_BASIC] = m_pAttrs[ATTR_KIND_DEX]->getLevel();
    m_INT[ATTR_CURRENT] = m_INT[ATTR_MAX] = m_INT[ATTR_BASIC] = m_pAttrs[ATTR_KIND_INT]->getLevel();

    m_STR[ATTR_CURRENT] += m_AdvancedSTR;
    m_DEX[ATTR_CURRENT] += m_AdvancedDEX;
    m_INT[ATTR_CURRENT] += m_AdvancedINT;

    if (isFlag(Effect::EFFECT_CLASS_BLESS)) {
        EffectBless* pBless = dynamic_cast<EffectBless*>(findEffect(Effect::EFFECT_CLASS_BLESS));
        if (pBless != NULL) {
            // Raise STR and DEX.
            m_STR[ATTR_CURRENT] += getPercentValue(m_STR[ATTR_CURRENT], pBless->getSTRBonus());
            m_DEX[ATTR_CURRENT] += getPercentValue(m_DEX[ATTR_CURRENT], pBless->getDEXBonus());
        }
    }
    if (isFlag(Effect::EFFECT_CLASS_POTENTIAL_EXPLOSION)) {
        EffectPotentialExplosion* pPotentialExplosion =
            dynamic_cast<EffectPotentialExplosion*>(findEffect(Effect::EFFECT_CLASS_POTENTIAL_EXPLOSION));
        if (pPotentialExplosion != NULL) {
            // Raise STR and DEX.
            m_STR[ATTR_CURRENT] += pPotentialExplosion->getDiffSTR();
            m_DEX[ATTR_CURRENT] += pPotentialExplosion->getDiffDEX();
        }
    }
    if (isFlag(Effect::EFFECT_CLASS_REDIANCE)) {
        EffectRediance* pRediance = dynamic_cast<EffectRediance*>(findEffect(Effect::EFFECT_CLASS_REDIANCE));
        if (pRediance != NULL) {
            Item* pWeapon = m_pWearItem[Slayer::WEAR_RIGHTHAND];
            if (pWeapon == NULL || pWeapon->getItemClass() != Item::ITEM_CLASS_SWORD ||
                !isRealWearing(WEAR_RIGHTHAND)) {
                pRediance->setDeadline(0);
            } else {
                m_DEX[ATTR_CURRENT] += pRediance->getDexBonus();
            }
        }
    }

    //////////////////////////////////////////////////////////////////////////////
    // Initialize the parameters used to compute the attributes.
    //////////////////////////////////////////////////////////////////////////////
    attr.nSTR = m_STR[ATTR_CURRENT];
    attr.nDEX = m_DEX[ATTR_CURRENT];
    attr.nINT = m_INT[ATTR_CURRENT];
    attr.pWeapon = m_pWearItem[WEAR_RIGHTHAND];

    m_HPStealAmount = 0;
    m_MPStealAmount = 0;
    m_HPStealRatio = 0;
    m_MPStealRatio = 0;
    m_HPRegen = 0;
    m_MPRegen = 0;
    m_Luck = m_BaseLuck;

    for (int i = 0; i < SKILL_DOMAIN_VAMPIRE; i++) {
        attr.pDomainLevel[i] = m_SkillDomainLevels[i];
    }

    //////////////////////////////////////////////////////////////////////////////
    // Compute the derived attributes.
    //////////////////////////////////////////////////////////////////////////////
    m_HP[ATTR_MAX] = computeHP(CClass, &attr);
    m_HP[ATTR_BASIC] = 0;
    m_MP[ATTR_MAX] = computeMP(CClass, &attr);
    m_MP[ATTR_BASIC] = 0;
    m_ToHit[ATTR_CURRENT] = computeToHit(CClass, &attr);
    m_ToHit[ATTR_MAX] = 0;
    m_Defense[ATTR_CURRENT] = computeDefense(CClass, &attr);
    m_Defense[ATTR_MAX] = 0;
    m_Protection[ATTR_CURRENT] = computeProtection(CClass, &attr);
    m_Protection[ATTR_MAX] = 0;
    m_Damage[ATTR_CURRENT] = computeMinDamage(CClass, &attr);
    m_Damage[ATTR_MAX] = computeMaxDamage(CClass, &attr);
    m_Damage[ATTR_BASIC] = 0;
    m_AttackSpeed[ATTR_CURRENT] = computeAttackSpeed(CClass, &attr);
    m_AttackSpeed[ATTR_MAX] = 0;
    m_CriticalRatio[ATTR_CURRENT] = computeCriticalRatio(CClass, &attr);
    m_CriticalRatio[ATTR_MAX] = 0;

    //////////////////////////////////////////////////////////////////////////////
    // Keep the damage that comes from attributes, for the BERSERKER skill bonus.
    //////////////////////////////////////////////////////////////////////////////
    Damage_t AttrMinDamage = m_Damage[ATTR_CURRENT];
    Damage_t AttrMaxDamage = m_Damage[ATTR_MAX];

    int DefBonus = 0;
    int ProBonus = 0;

    if (isFlag(Effect::EFFECT_CLASS_CONCEALMENT)) {
        EffectConcealment* pEffect = dynamic_cast<EffectConcealment*>(findEffect(Effect::EFFECT_CLASS_CONCEALMENT));
        if (pEffect != NULL) {
            if (attr.pWeapon == NULL || !isArmsWeapon(attr.pWeapon)) {
                pEffect->setDeadline(0);
            } else if (isRealWearing(WEAR_RIGHTHAND)) {
                DefBonus = decore::concealmentDefenseBonus(getDEX(), pEffect->getLevel());
                ProBonus = decore::concealmentProtectionBonus(getSTR(), pEffect->getLevel());
            }
        }
    }

    int RaceWarHPBonus = 0;

    if (m_pZone->isHolyLand() || m_pZone->isLevelWarZone()) {
        RaceWarHPBonus = getPercentValue(m_HP[ATTR_MAX], de::gameContext().variables().getRaceWarHPBonus());
    }

    int DragonEyeHPBonus = 0;
    if (isFlag(Effect::EFFECT_CLASS_DRAGON_EYE)) {
        // The HP bonus is doubled.
        DragonEyeHPBonus = m_HP[ATTR_MAX];
    }

    // Passive Skill : Will of Iron (pure HP * 1.15)
    SkillSlot* pFabulousSoul = getSkill(SKILL_FABULOUS_SOUL);
    SkillSlot* pWillOfIron = getSkill(SKILL_WILL_OF_IRON);

    int HPBonus_WillOfIron = 0;

    if ((pFabulousSoul != NULL && pFabulousSoul->canUse()) || (pWillOfIron != NULL && pWillOfIron->canUse())) {
        HPBonus_WillOfIron = decore::willOfIronHPBonus(m_HP[ATTR_MAX]);
    }

    SkillSlot* pLiveness = getSkill(SKILL_LIVENESS);
    int HPBonusPercent = 0, LivenessHPBonus = 0, LivenessDefenseBonus = 0;

    if (pLiveness != NULL && pLiveness->canUse()) {
        int level = m_SkillDomainLevels[SKILL_DOMAIN_GUN];
        int grade = (int)de::gameContext().skillInfos().getGradeByDomainLevel(level);

        decore::LivenessBonus bonus = decore::livenessBonus(grade, level);
        HPBonusPercent = bonus.hpPercent;
        LivenessDefenseBonus = bonus.defenseBonus;

        LivenessHPBonus = getPercentValue(m_HP[ATTR_MAX], HPBonusPercent);
    }

    //// // War bonus
    // The bonus can now apply to either side regardless of who wins the war.
    int HPBonus = 0;
    {
        int bonusRatio = de::gameContext().variables().getCombatSlayerHPBonusRatio();

        if (bonusRatio > 0) {
            HPBonus = getPercentValue(m_HP[ATTR_MAX], bonusRatio);
        }
    }

    //////////////////////////////////////////////////////////////////////////
    // Reset the gear check flags so everything starts out as not worn.
    //////////////////////////////////////////////////////////////////////////////
    bool pOldRealWearingCheck[WEAR_MAX]; // by sigi. 2002.10.31
    for (int i = 0; i < WEAR_MAX; i++) {
        pOldRealWearingCheck[i] = m_pRealWearingCheck[i];
        m_pRealWearingCheck[i] = false;
    }

    //////////////////////////////////////////////////////////////////////////////
    // The race that owns a castle gets a bonus option.
    //////////////////////////////////////////////////////////////////////////////
    // Each Blood Bible grants its own bonus option.

    //////////////////////////////////////////////////////////////////////////////
    // Each Blood Bible grants its own bonus option.
    //////////////////////////////////////////////////////////////////////////////

    if (de::gameContext().sweeperBonuses().isAble(getZoneID()) &&
        de::gameContext().levelWarZones().isCreatureBonusZone(this, getZoneID())) {
        const SweeperBonusHashMap& sweeperBonuses = de::gameContext().sweeperBonuses().getSweeperBonuses();

        SweeperBonusHashMapConstItor itr = sweeperBonuses.begin();
        SweeperBonusHashMapConstItor endItr = sweeperBonuses.end();

        for (; itr != endItr; itr++) {
            if (itr->second->getRace() == RACE_SLAYER &&
                itr->second->getLevel() == de::gameContext().levelWarZones().getCreatureLevelGrade(this)) {
                OptionTypeList optionTypes = itr->second->getOptionTypeList();
                OptionTypeListConstItor optionItr;

                for (optionItr = optionTypes.begin(); optionItr != optionTypes.end(); optionItr++) {
                    computeOptionStat(*optionItr);
                }
            }
        }
    }

    //////////////////////////////////////////////////////////////////////////////
    // Compute the options the character has by default.
    //////////////////////////////////////////////////////////////////////////////
    forward_list<DefaultOptionSetType_t>::iterator itr = m_DefaultOptionSet.begin();
    for (; itr != m_DefaultOptionSet.end(); itr++) {
        DefaultOptionSetInfo* pDefaultOptionSetInfo = de::gameContext().optionSets().getDefaultOptionSetInfo((*itr));
        if (pDefaultOptionSetInfo != NULL) {
            const list<OptionType_t>& optionList = pDefaultOptionSetInfo->getOptionTypeList();
            list<OptionType_t>::const_iterator citr;
            for (citr = optionList.begin(); citr != optionList.end(); citr++) {
                computeOptionStat(*citr);
            }
        }
    }

    //////////////////////////////////////////////////////////////////////////////
    // Compute the bonus given by the pet.
    //////////////////////////////////////////////////////////////////////////////
    if (m_pPetInfo != NULL) {
        if (m_pPetInfo->getPetAttr() != 0xff)
            computeOptionClassStat((OptionClass)m_pPetInfo->getPetAttr(), (int)m_pPetInfo->getPetAttrLevel());
        if (m_pPetInfo->getPetOption() != 0)
            computeOptionStat(m_pPetInfo->getPetOption());
    }

    if (hasRankBonus(RankBonus::RANK_BONUS_PERCEPTION)) {
        RankBonus* pRankBonus = getRankBonus(RankBonus::RANK_BONUS_PERCEPTION);
        Assert(pRankBonus != NULL);

        computeOptionClassStat(OPTION_ALL_ATTR, pRankBonus->getPoint());
    }

    if (hasRankBonus(RankBonus::RANK_BONUS_STONE_OF_SAGE)) {
        RankBonus* pRankBonus = getRankBonus(RankBonus::RANK_BONUS_STONE_OF_SAGE);
        Assert(pRankBonus != NULL);

        computeOptionClassStat(OPTION_INT, pRankBonus->getPoint());
    }

    if (hasRankBonus(RankBonus::RANK_BONUS_FOOT_OF_RANGER)) {
        RankBonus* pRankBonus = getRankBonus(RankBonus::RANK_BONUS_FOOT_OF_RANGER);
        Assert(pRankBonus != NULL);

        computeOptionClassStat(OPTION_DEX, pRankBonus->getPoint());
    }

    if (hasRankBonus(RankBonus::RANK_BONUS_WARRIORS_FIST)) {
        RankBonus* pRankBonus = getRankBonus(RankBonus::RANK_BONUS_WARRIORS_FIST);
        Assert(pRankBonus != NULL);

        computeOptionClassStat(OPTION_STR, pRankBonus->getPoint());
    }

    //////////////////////////////////////////////////////////////////////////////
    // Check the equipped items.
    // The loop runs twice so that items which only become wearable thanks to
    // attributes raised by other items are checked as well.
    //////////////////////////////////////////////////////////////////////////////
    for (int j = 0; j < WEAR_MAX; j++) {
        int wearCount = 0;

        for (int i = 0; i < WEAR_MAX; i++) {
            Item* pItem = m_pWearItem[i];

            // If there is an item in this slot and
            // it has not been checked yet...
            if (pItem != NULL && m_pRealWearingCheck[i] == false) {
                // If it really can be worn, apply its attribute bonuses.
                if (isRealWearing(pItem)) {
                    computeItemStat(pItem);

                    // For a two-handed weapon, set the check flag for both the left and the
                    // right hand so it is not counted twice.
                    if (isTwohandWeapon(pItem)) {
                        m_pRealWearingCheck[WEAR_LEFTHAND] = true;
                        m_pRealWearingCheck[WEAR_RIGHTHAND] = true;
                    } else
                        m_pRealWearingCheck[i] = true;

                    wearCount++;
                }
            }
        }

        if (wearCount == 0)
            break;
    }

    bool zaps[4] = {false, false, false, false};
    for (int i = WEAR_ZAP1; i <= WEAR_ZAP4; ++i) {
        Item* pItem = m_pWearItem[i];
        if (pItem != NULL && m_pRealWearingCheck[i] == true && pItem->getItemClass() == Item::ITEM_CLASS_CORE_ZAP &&
            pItem->getItemType() < 4) {
            zaps[pItem->getItemType()] = true;
        }
    }
    if (zaps[0] && zaps[1] && zaps[2] && zaps[3]) {
        computeOptionStat(182); // all resistances 9
        computeOptionStat(185); // all attributes 3
    }

    applyBloodBibleSign();

    // by sigi. 2002.11.6
    bool bSendPacket = false;

    if (m_pPlayer != NULL) {
        bSendPacket = (dynamic_cast<GamePlayer*>(m_pPlayer)->getPlayerStatus() == GPS_NORMAL);
    }

    // Everything was worn above; items whose requirements the attributes do not
    // meet lose their wearing information, and an item that could not be worn
    // before but can be now gets a wear packet sent for it.
    // by sigi. 2002.10.30
    for (int i = 0; i < WEAR_MAX; i++) {
        if (m_pRealWearingCheck[i]) {
            // by sigi. 2002.10.31
            if (pOldRealWearingCheck[i] == false) {
                Item* pItem = m_pWearItem[i];
                if (pItem != NULL) {
                    Item::ItemClass IClass = pItem->getItemClass();
                    ItemType_t IType = pItem->getItemType();

                    Color_t color = getItemShapeColor(pItem);
                    addShape(IClass, IType, color);

                    if (bSendPacket) {
                        GCChangeShape _GCChangeShape;
                        _GCChangeShape.setObjectID(getObjectID());
                        _GCChangeShape.setItemClass(IClass);
                        _GCChangeShape.setItemType(IType);
                        _GCChangeShape.setOptionType(pItem->getFirstOptionType());
                        _GCChangeShape.setAttackSpeed(m_AttackSpeed[ATTR_CURRENT]);
                        m_pZone->broadcastPacket(m_X, m_Y, &_GCChangeShape, this);
                    }
                }
            }
        } else {
            Item* pItem = m_pWearItem[i];
            if (pItem != NULL) {
                removeShape(pItem->getItemClass(), bSendPacket);
            }
        }
    }

    ///////////////////////////////////////////////////////////////////////////////
    // Compute the rank bonus.
    ///////////////////////////////////////////////////////////////////////////////
    // Computed before the steal ratio.
    ///////////////////////////////////////////////////////////////////////////////
    if (hasRankBonus(RankBonus::RANK_BONUS_WIGHT_HAND)) {
        RankBonus* pRankBonus = getRankBonus(RankBonus::RANK_BONUS_WIGHT_HAND);
        Assert(pRankBonus != NULL);

        int StealBonus = pRankBonus->getPoint();

        m_HPStealAmount += StealBonus;
    }
    if (hasRankBonus(RankBonus::RANK_BONUS_SEIREN_HAND)) {
        RankBonus* pRankBonus = getRankBonus(RankBonus::RANK_BONUS_SEIREN_HAND);
        Assert(pRankBonus != NULL);

        int StealBonus = pRankBonus->getPoint();

        m_MPStealAmount += StealBonus;
    }

    //////////////////////////////////////////////////////////////////////////////
    // Compute the HP and MP steal ratios.
    //////////////////////////////////////////////////////////////////////////////
    m_HPStealRatio = computeStealRatio(CClass, m_HPStealAmount, &attr);
    m_MPStealRatio = computeStealRatio(CClass, m_MPStealAmount, &attr);

    Item* pWeapon = m_pWearItem[Slayer::WEAR_RIGHTHAND];
    Item* pShield = m_pWearItem[Slayer::WEAR_LEFTHAND];

    //////////////////////////////////////////////////////////////////////////////
    // Check the effects that directly modify the derived attributes.
    //////////////////////////////////////////////////////////////////////////////
    if (isFlag(Effect::EFFECT_CLASS_STRIKING)) {
        EffectStriking* pStriking = dynamic_cast<EffectStriking*>(findEffect(Effect::EFFECT_CLASS_STRIKING));
        if (pStriking != NULL) {
            Damage_t DamageBonus = pStriking->getDamageBonus();

            if (pWeapon != NULL && pStriking->isTargetItem(pWeapon)) {
                m_Damage[ATTR_CURRENT] = min(SLAYER_MAX_DAMAGE, m_Damage[ATTR_CURRENT] + DamageBonus);
                m_Damage[ATTR_MAX] = min(SLAYER_MAX_DAMAGE, m_Damage[ATTR_MAX] + DamageBonus);

                // A matching ItemOID means striking was just applied, or a different weapon
                // was held and the weapon carrying striking is being held again, so the
                // client must be told to attach the effect.
                GCAddEffect gcAddEffect;
                gcAddEffect.setObjectID(m_ObjectID);
                gcAddEffect.setEffectID(Effect::EFFECT_CLASS_STRIKING);
                gcAddEffect.setDuration(pStriking->getRemainDuration());
                m_pZone->broadcastPacket(m_X, m_Y, &gcAddEffect);
            } else {
                // A mismatching ItemOID means another weapon was taken while striking was
                // active, so the slayer still carries striking and the effect has to be
                // removed.
                GCRemoveEffect gcRemoveEffect;
                gcRemoveEffect.setObjectID(m_ObjectID);
                gcRemoveEffect.addEffectList(Effect::EFFECT_CLASS_STRIKING);
                m_pZone->broadcastPacket(getX(), getY(), &gcRemoveEffect);
            }
        }
    }

    if (isFlag(Effect::EFFECT_CLASS_BLOOD_DRAIN)) {
        int DefensePenalty = getPercentValue(m_Defense[ATTR_CURRENT], 20);
        int ToHitPenalty = getPercentValue(m_ToHit[ATTR_CURRENT], 20);
        int ProtectionPenalty = getPercentValue(m_Protection[ATTR_CURRENT], 20);
        int DamagePenalty1 = getPercentValue(m_Damage[ATTR_CURRENT], 20);
        int DamagePenalty2 = getPercentValue(m_Damage[ATTR_MAX], 20);

        m_Defense[ATTR_CURRENT] = max(0, m_Defense[ATTR_CURRENT] - DefensePenalty);
        m_ToHit[ATTR_CURRENT] = max(0, m_ToHit[ATTR_CURRENT] - ToHitPenalty);
        m_Protection[ATTR_CURRENT] = max(0, m_Protection[ATTR_CURRENT] - ProtectionPenalty);
        m_Damage[ATTR_CURRENT] = max(0, m_Damage[ATTR_CURRENT] - DamagePenalty1);
        m_Damage[ATTR_MAX] = max(0, m_Damage[ATTR_MAX] - DamagePenalty2);
    }
    if (isFlag(Effect::EFFECT_CLASS_DOOM)) {
        EffectDoom* pDoom = dynamic_cast<EffectDoom*>(findEffect(Effect::EFFECT_CLASS_DOOM));
        if (pDoom != NULL) {
            int DefensePenalty = getPercentValue(m_Defense[ATTR_CURRENT], pDoom->getDefensePenalty());
            int ProtectionPenalty = getPercentValue(m_Protection[ATTR_CURRENT], pDoom->getProtectionPenalty());

            m_Defense[ATTR_CURRENT] = max(0, m_Defense[ATTR_CURRENT] - DefensePenalty);
            m_Protection[ATTR_CURRENT] = max(0, m_Protection[ATTR_CURRENT] - ProtectionPenalty);
        }
    }
    if (isFlag(Effect::EFFECT_CLASS_SEDUCTION)) {
        EffectSeduction* pSeduction = dynamic_cast<EffectSeduction*>(findEffect(Effect::EFFECT_CLASS_SEDUCTION));
        if (pSeduction != NULL) {
            int ToHitPenalty = getPercentValue(m_ToHit[ATTR_CURRENT], pSeduction->getToHitPenalty());
            int DamagePenalty1 = getPercentValue(m_Damage[ATTR_CURRENT], pSeduction->getDamagePenalty());
            int DamagePenalty2 = getPercentValue(m_Damage[ATTR_MAX], pSeduction->getDamagePenalty());

            m_ToHit[ATTR_CURRENT] = max(0, m_ToHit[ATTR_CURRENT] - ToHitPenalty);
            m_Damage[ATTR_CURRENT] = max(0, m_Damage[ATTR_CURRENT] - DamagePenalty1);
            m_Damage[ATTR_MAX] = max(0, m_Damage[ATTR_MAX] - DamagePenalty2);
        }
    }
    if (isFlag(Effect::EFFECT_CLASS_CHARGING_POWER)) {
        EffectChargingPower* pChargingPower =
            dynamic_cast<EffectChargingPower*>(findEffect(Effect::EFFECT_CLASS_CHARGING_POWER));
        if (pChargingPower != NULL) {
            if (!isRealWearing(Slayer::WEAR_RIGHTHAND) || pWeapon == NULL ||
                pWeapon->getItemClass() != Item::ITEM_CLASS_BLADE) {
                pChargingPower->setDeadline(0);
            } else {
                int DamageBonus = pChargingPower->getDamageBonus();

                m_Damage[ATTR_CURRENT] = min(SLAYER_MAX_DAMAGE, m_Damage[ATTR_CURRENT] + DamageBonus);
                m_Damage[ATTR_MAX] = min(SLAYER_MAX_DAMAGE, m_Damage[ATTR_MAX] + DamageBonus);
            }
        }
    }
    if (isFlag(Effect::EFFECT_CLASS_DANCING_SWORD)) {
        EffectDancingSword* pDancingSword =
            dynamic_cast<EffectDancingSword*>(findEffect(Effect::EFFECT_CLASS_DANCING_SWORD));
        if (pDancingSword != NULL) {
            m_ToHit[ATTR_CURRENT] = min(SLAYER_MAX_TOHIT, m_ToHit[ATTR_CURRENT] + pDancingSword->getToHitBonus());
        }
    }
    if (isFlag(Effect::EFFECT_CLASS_GHOST_BLADE)) {
        EffectGhostBlade* pGhostBlade = dynamic_cast<EffectGhostBlade*>(findEffect(Effect::EFFECT_CLASS_GHOST_BLADE));
        if (pGhostBlade != NULL) {
            m_ToHit[ATTR_CURRENT] = min(SLAYER_MAX_TOHIT, m_ToHit[ATTR_CURRENT] + pGhostBlade->getToHitBonus());
        }
    }
    if (isFlag(Effect::EFFECT_CLASS_MIND_CONTROL)) {
        EffectMindControl* pMindControl =
            dynamic_cast<EffectMindControl*>(findEffect(Effect::EFFECT_CLASS_MIND_CONTROL));
        if (pMindControl != NULL) {
            m_ToHit[ATTR_CURRENT] = min(SLAYER_MAX_TOHIT, m_ToHit[ATTR_CURRENT] + pMindControl->getToHitBonus());
            m_Defense[ATTR_CURRENT] =
                min(SLAYER_MAX_DEFENSE, m_Defense[ATTR_CURRENT] + pMindControl->getDefenseBonus());
        }
    }
    if (isFlag(Effect::EFFECT_CLASS_PROTECTION_FROM_POISON)) {
        EffectProtectionFromPoison* pProtectionFromPoison =
            dynamic_cast<EffectProtectionFromPoison*>(findEffect(Effect::EFFECT_CLASS_PROTECTION_FROM_POISON));
        if (pProtectionFromPoison != NULL) {
            m_Resist[MAGIC_DOMAIN_POISON] += pProtectionFromPoison->getResist();
        }
    }
    if (isFlag(Effect::EFFECT_CLASS_PROTECTION_FROM_CURSE)) {
        EffectProtectionFromCurse* pProtectionFromCurse =
            dynamic_cast<EffectProtectionFromCurse*>(findEffect(Effect::EFFECT_CLASS_PROTECTION_FROM_CURSE));
        if (pProtectionFromCurse != NULL) {
            m_Resist[MAGIC_DOMAIN_CURSE] += pProtectionFromCurse->getResist();
        }
    }
    if (isFlag(Effect::EFFECT_CLASS_PROTECTION_FROM_ACID)) {
        EffectProtectionFromAcid* pProtectionFromAcid =
            dynamic_cast<EffectProtectionFromAcid*>(findEffect(Effect::EFFECT_CLASS_PROTECTION_FROM_ACID));
        if (pProtectionFromAcid != NULL) {
            m_Resist[MAGIC_DOMAIN_ACID] += pProtectionFromAcid->getResist();
        }
    }
    if (isFlag(Effect::EFFECT_CLASS_PROTECTION_FROM_BLOOD)) {
        EffectProtectionFromBlood* pProtectionFromBlood =
            dynamic_cast<EffectProtectionFromBlood*>(findEffect(Effect::EFFECT_CLASS_PROTECTION_FROM_BLOOD));
        if (pProtectionFromBlood != NULL) {
            m_Resist[MAGIC_DOMAIN_BLOOD] += pProtectionFromBlood->getResist();
        }
    }
    if (isFlag(Effect::EFFECT_CLASS_EXPANSION)) {
        EffectExpansion* pExpansion = dynamic_cast<EffectExpansion*>(findEffect(Effect::EFFECT_CLASS_EXPANSION));
        if (pExpansion != NULL) {
            int Bonus = pExpansion->getHPBonus();
            // Inflate the HP.
            m_HP[ATTR_MAX] = m_HP[ATTR_MAX] + Bonus;
        }
    }
    if (isFlag(Effect::EFFECT_CLASS_BERSERKER)) {
        EffectBerserker* pBerserker = dynamic_cast<EffectBerserker*>(findEffect(Effect::EFFECT_CLASS_BERSERKER));
        if (pBerserker != NULL) {
            Damage_t BladeMinDamage = 0;
            Damage_t BladeMaxDamage = 0;

            if (attr.pWeapon != NULL && attr.pWeapon->getItemClass() == Item::ITEM_CLASS_BLADE) {
                BladeMinDamage = attr.pWeapon->getMinDamage();
                BladeMaxDamage = attr.pWeapon->getMaxDamage();
            }

            // The damage and to-hit bonuses and the defense and protection penalties are
            // percentages. Damage is a ratio of attribute damage plus blade damage;
            // extra damage from other effects is left out of this computation.
            int ToHitBonus = getPercentValue(m_ToHit[ATTR_CURRENT], pBerserker->getToHitBonus());
            int MinDamageBonus = getPercentValue(AttrMinDamage + BladeMinDamage, pBerserker->getDamageBonus());
            int MaxDamageBonus = getPercentValue(AttrMaxDamage + BladeMaxDamage, pBerserker->getDamageBonus());
            int DefensePenalty = getPercentValue(m_Defense[ATTR_CURRENT], pBerserker->getDefensePenalty());
            int ProtectionPenalty = getPercentValue(m_Defense[ATTR_CURRENT], pBerserker->getProtectionPenalty());

            m_ToHit[ATTR_CURRENT] = min(SLAYER_MAX_TOHIT, m_ToHit[ATTR_CURRENT] + ToHitBonus);
            m_Damage[ATTR_CURRENT] = min(SLAYER_MAX_DAMAGE, m_Damage[ATTR_CURRENT] + MinDamageBonus);
            m_Damage[ATTR_MAX] = min(SLAYER_MAX_DAMAGE, m_Damage[ATTR_MAX] + MaxDamageBonus);
            m_Defense[ATTR_CURRENT] = max(0, m_Defense[ATTR_CURRENT] - DefensePenalty);
            m_Protection[ATTR_CURRENT] = max(0, m_Protection[ATTR_CURRENT] - ProtectionPenalty);
        }
    }
    if (isFlag(Effect::EFFECT_CLASS_DEATH)) {
        EffectDeath* pDeath = dynamic_cast<EffectDeath*>(findEffect(Effect::EFFECT_CLASS_DEATH));
        if (pDeath != NULL) {
            for (int i = 0; i < MAGIC_DOMAIN_MAX; i++) {
                m_Resist[i] -= pDeath->getResistPenalty();
            }
        }
    }
    if (isFlag(Effect::EFFECT_CLASS_BLUNTING)) {
        EffectBlunting* pBlunting = dynamic_cast<EffectBlunting*>(findEffect(Effect::EFFECT_CLASS_BLUNTING));
        if (pBlunting != NULL) {
            int DefensePenalty = pBlunting->getDefensePenalty();
            m_Defense[ATTR_CURRENT] = max(0, m_Defense[ATTR_CURRENT] - DefensePenalty);
        }
    }
    if (isFlag(Effect::EFFECT_CLASS_HOLY_ARMOR)) {
        EffectHolyArmor* pHolyArmor = dynamic_cast<EffectHolyArmor*>(findEffect(Effect::EFFECT_CLASS_HOLY_ARMOR));

        if (pHolyArmor != NULL) {
            m_Defense[ATTR_CURRENT] = m_Defense[ATTR_CURRENT] + pHolyArmor->getDefBonus();
        }
    }

    if (isFlag(Effect::EFFECT_CLASS_WHITSUNTIDE)) {
        EffectWhitsuntide* pWhitsuntide =
            dynamic_cast<EffectWhitsuntide*>(findEffect(Effect::EFFECT_CLASS_WHITSUNTIDE));

        if (pWhitsuntide != NULL) {
            m_Resist[MAGIC_DOMAIN_POISON] += pWhitsuntide->getBonus();
            m_Resist[MAGIC_DOMAIN_ACID] += pWhitsuntide->getBonus();
            m_Resist[MAGIC_DOMAIN_CURSE] += pWhitsuntide->getBonus();
            m_Resist[MAGIC_DOMAIN_BLOOD] += pWhitsuntide->getBonus();
        }
    }

    if (isFlag(Effect::EFFECT_CLASS_INTIMATE_GRAIL)) {
        EffectIntimateGrail* pIntimateGrail =
            dynamic_cast<EffectIntimateGrail*>(findEffect(Effect::EFFECT_CLASS_INTIMATE_GRAIL));

        if (pIntimateGrail != NULL) {
            // A slayer is blessed.
            int hpratio = decore::intimateGrailHPRatio(pIntimateGrail->getSkillLevel());
            m_HP[ATTR_MAX] += getPercentValue(m_HP[ATTR_MAX], hpratio);
            m_MP[ATTR_MAX] += getPercentValue(m_MP[ATTR_MAX], hpratio);

            int defratio = decore::intimateGrailRatio(pIntimateGrail->getSkillLevel());
            m_Defense[ATTR_CURRENT] += getPercentValue(m_Defense[ATTR_CURRENT], defratio);
            m_Protection[ATTR_CURRENT] += getPercentValue(m_Protection[ATTR_CURRENT], defratio);
        }
    }

    // Compute the attributes raised by passive skills.
    if (pWeapon != NULL) {
        Item::ItemClass IClass = pWeapon->getItemClass();
        int DamageBonus = 0;
        int ToHitBonus = 0;
        int CriticalRatioBonus = 0;

        // For a gun, check ObservingEye.
        if (pWeapon->isGun() && isFlag(Effect::EFFECT_CLASS_OBSERVING_EYE)) {
            EffectObservingEye* pObservingEye =
                dynamic_cast<EffectObservingEye*>(findEffect(Effect::EFFECT_CLASS_OBSERVING_EYE));
            if (pObservingEye != NULL) {
                int DamageBonus = pObservingEye->getDamageBonus();

                m_Damage[ATTR_CURRENT] = min(SLAYER_MAX_DAMAGE, m_Damage[ATTR_CURRENT] + DamageBonus);
                m_Damage[ATTR_MAX] = min(SLAYER_MAX_DAMAGE, m_Damage[ATTR_MAX] + DamageBonus);

                CriticalRatioBonus += pObservingEye->getCriticalHitBonus();


                //  This is handled by the client.
            }
        }

        // Add the Liveness bonus.
        if (pLiveness != NULL && pWeapon->isGun()) {
            m_HP[ATTR_MAX] = m_HP[ATTR_MAX] + LivenessHPBonus;
            m_Defense[ATTR_CURRENT] = min(SLAYER_MAX_DEFENSE, m_Defense[ATTR_CURRENT] + LivenessDefenseBonus);
        }

        // Passive Skill : Will of Iron, applied for SWORD or BLADE.
        if ((pFabulousSoul != NULL && pWeapon->getItemClass() == Item::ITEM_CLASS_SWORD) ||
            (pWillOfIron != NULL && pWeapon->getItemClass() == Item::ITEM_CLASS_BLADE)) {
            m_HP[ATTR_MAX] += HPBonus_WillOfIron;
        }

        if (pWeapon->isGun()) {
            DamageBonus += decore::gunDomainDamageBonus(getSkillDomainLevel(SKILL_DOMAIN_GUN));
        }

        SkillSlot* pArmsMastery1 = getSkill(SKILL_ARMS_MASTERY_1);
        SkillSlot* pArmsMastery2 = getSkill(SKILL_ARMS_MASTERY_2);

        if (IClass == Item::ITEM_CLASS_AR) {
            SkillSlot* pARMastery = getSkill(SKILL_AR_MASTERY);
            if (pARMastery != NULL && pARMastery->canUse()) {
                DamageBonus += 3;
                ToHitBonus += 5;
            }

            if (pArmsMastery2 != NULL && pArmsMastery2->canUse()) {
                ToHitBonus += 6;
                DamageBonus += 5;
                CriticalRatioBonus += 6;
            } else if (pArmsMastery1 != NULL && pArmsMastery1->canUse()) {
                ToHitBonus += 4;
                DamageBonus += 3;
                CriticalRatioBonus += 4;
            }

        } else if (IClass == Item::ITEM_CLASS_SMG) {
            SkillSlot* pSMGMastery = getSkill(SKILL_SMG_MASTERY);
            if (pSMGMastery != NULL && pSMGMastery->canUse()) {
                DamageBonus += 3;
                ToHitBonus += 5;
            }

            if (pArmsMastery2 != NULL && pArmsMastery2->canUse()) {
                ToHitBonus += 5;
                DamageBonus += 5;
                CriticalRatioBonus += 7;
            } else if (pArmsMastery1 != NULL && pArmsMastery1->canUse()) {
                ToHitBonus += 3;
                DamageBonus += 3;
                CriticalRatioBonus += 5;
            }

        } else if (IClass == Item::ITEM_CLASS_SG) {
            SkillSlot* pSGMastery = getSkill(SKILL_SG_MASTERY);
            if (pSGMastery != NULL && pSGMastery->canUse()) {
                DamageBonus += 3;
                ToHitBonus += 5;
            }

            if (pArmsMastery2 != NULL && pArmsMastery2->canUse()) {
                ToHitBonus += 5;
                DamageBonus += 6;
                CriticalRatioBonus += 10;
            } else if (pArmsMastery1 != NULL && pArmsMastery1->canUse()) {
                ToHitBonus += 3;
                DamageBonus += 5;
                CriticalRatioBonus += 9;
            }
        } else if (IClass == Item::ITEM_CLASS_SR) {
            SkillSlot* pSRMastery = getSkill(SKILL_SR_MASTERY);
            if (pSRMastery != NULL && pSRMastery->canUse()) {
                DamageBonus += 3;
                ToHitBonus += 5;
            }

            if (pArmsMastery2 != NULL && pArmsMastery2->canUse()) {
                ToHitBonus += 11;
                DamageBonus += 7;
                CriticalRatioBonus += 5;
            } else if (pArmsMastery1 != NULL && pArmsMastery1->canUse()) {
                ToHitBonus += 9;
                DamageBonus += 5;
                CriticalRatioBonus += 3;
            }

            if (isFlag(Effect::EFFECT_CLASS_SNIPING_MODE)) {
                // by sigi. 2002.12.3
                SkillSlot* pSniping = getSkill(SKILL_SNIPING);

                if (pSniping != NULL) {
                    int level = pSniping->getExpLevel();

                    DamageBonus += decore::snipingDamageBonus(m_Damage[ATTR_CURRENT], m_STR[ATTR_CURRENT], level);
                    ToHitBonus += decore::snipingToHitBonus(m_ToHit[ATTR_CURRENT], m_DEX[ATTR_CURRENT], level);
                }
            }
        } else if (IClass == Item::ITEM_CLASS_SWORD) // by sigi. 2002.6.7
        {
            // SWORD_MASTERY
            SkillSlot* pMastery = getSkill(SKILL_SWORD_MASTERY);
            if (pMastery != NULL && pMastery->canUse()) {
                int level = m_SkillDomainLevels[SKILL_DOMAIN_SWORD];


                DamageBonus += decore::swordMasteryDamageBonus(level);
            }
        } else if (IClass == Item::ITEM_CLASS_BLADE) // by sigi. 2002.6.7
        {
            // CONCENTRATION
            SkillSlot* pSkill = getSkill(SKILL_CONCENTRATION);
            if (pSkill != NULL && pSkill->canUse()) {
                int level = m_SkillDomainLevels[SKILL_DOMAIN_BLADE];

                ToHitBonus += decore::concentrationToHitBonus(level);
            }

            // EVASION
            pSkill = getSkill(SKILL_EVASION);
            if (pSkill != NULL && pSkill->canUse()) {
                int level = m_SkillDomainLevels[SKILL_DOMAIN_BLADE];

                Defense_t DefenseBonus = decore::evasionDefenseBonus(level);

                // Only Evasion changes defense, so it is computed here.
                m_Defense[ATTR_CURRENT] = min(SLAYER_MAX_DEFENSE, m_Defense[ATTR_CURRENT] + DefenseBonus);
                m_Defense[ATTR_MAX] = min(SLAYER_MAX_DEFENSE, m_Defense[ATTR_MAX] + DefenseBonus);
            }
        }


        if (pWeapon->isGun()) {
            // Add the Concealment bonus.
            m_Defense[ATTR_CURRENT] += DefBonus;
            m_Protection[ATTR_CURRENT] += ProBonus;
            m_Defense[ATTR_MAX] += DefBonus;
            m_Protection[ATTR_MAX] += ProBonus;
        }

        m_ToHit[ATTR_CURRENT] = min(SLAYER_MAX_TOHIT, m_ToHit[ATTR_CURRENT] + ToHitBonus);
        m_ToHit[ATTR_MAX] = min(SLAYER_MAX_TOHIT, m_ToHit[ATTR_MAX] + ToHitBonus);
        m_Damage[ATTR_CURRENT] = min(SLAYER_MAX_DAMAGE, m_Damage[ATTR_CURRENT] + DamageBonus);
        m_Damage[ATTR_MAX] = min(SLAYER_MAX_DAMAGE, m_Damage[ATTR_MAX] + DamageBonus);
        m_CriticalRatio[ATTR_CURRENT] = m_CriticalRatio[ATTR_CURRENT] + CriticalRatioBonus;
        m_CriticalRatio[ATTR_MAX] = m_CriticalRatio[ATTR_MAX] + CriticalRatioBonus;
    }

    // Shield check.
    if (pShield != NULL && pShield->getItemClass() == Item::ITEM_CLASS_SHIELD) {
        int ProtectionBonus = 0;
        SkillSlot* pMastery = getSkill(SKILL_SHIELD_MASTERY);
        if (pMastery != NULL && pMastery->canUse()) {
            int level = m_SkillDomainLevels[SKILL_DOMAIN_SWORD];
            ProtectionBonus += decore::shieldMasteryProtectionBonus(level);

            m_Protection[ATTR_CURRENT] = min(SLAYER_MAX_PROTECTION, m_Protection[ATTR_CURRENT] + ProtectionBonus);
            m_Protection[ATTR_MAX] = min(SLAYER_MAX_PROTECTION, m_Protection[ATTR_MAX] + ProtectionBonus);
        }
    }

    ///////////////////////////////////////////////////////////////////////////////
    // Compute the rank bonus.
    ///////////////////////////////////////////////////////////////////////////////
    if (hasRankBonus(RankBonus::RANK_BONUS_DEADLY_SPEAR)) {
        RankBonus* pRankBonus = getRankBonus(RankBonus::RANK_BONUS_DEADLY_SPEAR);
        Assert(pRankBonus != NULL);

        int CriticalRatioBonus = pRankBonus->getPoint();

        m_CriticalRatio[ATTR_CURRENT] = m_CriticalRatio[ATTR_CURRENT] + CriticalRatioBonus;
        m_CriticalRatio[ATTR_MAX] = m_CriticalRatio[ATTR_MAX] + CriticalRatioBonus;
    }
    if (hasRankBonus(RankBonus::RANK_BONUS_BEHEMOTH_ARMOR)) {
        RankBonus* pRankBonus = getRankBonus(RankBonus::RANK_BONUS_BEHEMOTH_ARMOR);
        Assert(pRankBonus != NULL);

        Defense_t DefenseBonus = pRankBonus->getPoint();

        m_Defense[ATTR_CURRENT] = min(SLAYER_MAX_DEFENSE, m_Defense[ATTR_CURRENT] + DefenseBonus);
        m_Defense[ATTR_MAX] = min(SLAYER_MAX_DEFENSE, m_Defense[ATTR_MAX] + DefenseBonus);
    }
    if (hasRankBonus(RankBonus::RANK_BONUS_DRAGON_EYE)) {
        RankBonus* pRankBonus = getRankBonus(RankBonus::RANK_BONUS_DRAGON_EYE);
        Assert(pRankBonus != NULL);

        int ToHitBonus = pRankBonus->getPoint();

        m_ToHit[ATTR_CURRENT] = min(SLAYER_MAX_TOHIT, m_ToHit[ATTR_CURRENT] + ToHitBonus);
        m_ToHit[ATTR_MAX] = min(SLAYER_MAX_TOHIT, m_ToHit[ATTR_MAX] + ToHitBonus);
    }
    if (hasRankBonus(RankBonus::RANK_BONUS_IMMORTAL_HEART)) {
        RankBonus* pRankBonus = getRankBonus(RankBonus::RANK_BONUS_IMMORTAL_HEART);
        Assert(pRankBonus != NULL);

        int HPBonus = pRankBonus->getPoint();

        m_HP[ATTR_MAX] = m_HP[ATTR_MAX] + HPBonus;
    }
    if (hasRankBonus(RankBonus::RANK_BONUS_RELIANCE_BRAIN)) {
        RankBonus* pRankBonus = getRankBonus(RankBonus::RANK_BONUS_RELIANCE_BRAIN);
        Assert(pRankBonus != NULL);

        int MPBonus = pRankBonus->getPoint();

        m_MP[ATTR_MAX] = min(SLAYER_MAX_MP, m_MP[ATTR_MAX] + MPBonus);
    }
    if (hasRankBonus(RankBonus::RANK_BONUS_SLAYING_KNIFE)) {
        RankBonus* pRankBonus = getRankBonus(RankBonus::RANK_BONUS_SLAYING_KNIFE);
        Assert(pRankBonus != NULL);

        int DamageBonus = pRankBonus->getPoint();

        m_Damage[ATTR_CURRENT] = min(SLAYER_MAX_DAMAGE, m_Damage[ATTR_CURRENT] + DamageBonus);
        m_Damage[ATTR_MAX] = min(SLAYER_MAX_DAMAGE, m_Damage[ATTR_MAX] + DamageBonus);
    }
    if (hasRankBonus(RankBonus::RANK_BONUS_HAWK_WING)) {
        RankBonus* pRankBonus = getRankBonus(RankBonus::RANK_BONUS_HAWK_WING);
        Assert(pRankBonus != NULL);

        int AttackSpeedBonus = pRankBonus->getPoint();

        m_AttackSpeed[ATTR_CURRENT] += AttackSpeedBonus;
        m_AttackSpeed[ATTR_MAX] += AttackSpeedBonus;
    }
    if (hasRankBonus(RankBonus::RANK_BONUS_SAPPHIRE_BLESS)) {
        RankBonus* pRankBonus = getRankBonus(RankBonus::RANK_BONUS_SAPPHIRE_BLESS);
        Assert(pRankBonus != NULL);

        int ResistBonus = pRankBonus->getPoint();

        m_Resist[MAGIC_DOMAIN_ACID] += ResistBonus;
    }
    if (hasRankBonus(RankBonus::RANK_BONUS_RUBY_BLESS)) {
        RankBonus* pRankBonus = getRankBonus(RankBonus::RANK_BONUS_RUBY_BLESS);
        Assert(pRankBonus != NULL);

        int ResistBonus = pRankBonus->getPoint();

        m_Resist[MAGIC_DOMAIN_BLOOD] += ResistBonus;
    }
    if (hasRankBonus(RankBonus::RANK_BONUS_DIAMOND_BLESS)) {
        RankBonus* pRankBonus = getRankBonus(RankBonus::RANK_BONUS_DIAMOND_BLESS);
        Assert(pRankBonus != NULL);

        int ResistBonus = pRankBonus->getPoint();

        m_Resist[MAGIC_DOMAIN_CURSE] += ResistBonus;
    }
    if (hasRankBonus(RankBonus::RANK_BONUS_EMERALD_BLESS)) {
        RankBonus* pRankBonus = getRankBonus(RankBonus::RANK_BONUS_EMERALD_BLESS);
        Assert(pRankBonus != NULL);

        int ResistBonus = pRankBonus->getPoint();

        m_Resist[MAGIC_DOMAIN_POISON] += ResistBonus;
    }
    if (hasRankBonus(RankBonus::RANK_BONUS_FORTUNE_HAND)) {
        RankBonus* pRankBonus = getRankBonus(RankBonus::RANK_BONUS_FORTUNE_HAND);
        Assert(pRankBonus != NULL);

        int LuckBonus = pRankBonus->getPoint();

        m_Luck += LuckBonus;
    }

    if (hasRankBonus(RankBonus::RANK_BONUS_EVOLUTION_IMMORTAL_HEART)) {
        RankBonus* pRankBonus = getRankBonus(RankBonus::RANK_BONUS_EVOLUTION_IMMORTAL_HEART);
        Assert(pRankBonus != NULL);

        HPBonus += getPercentValue(m_HP[ATTR_MAX], pRankBonus->getPoint());
    }

    if (hasRankBonus(RankBonus::RANK_BONUS_BEHEMOTH_ARMOR_2)) {
        RankBonus* pRankBonus = getRankBonus(RankBonus::RANK_BONUS_BEHEMOTH_ARMOR_2);
        Assert(pRankBonus != NULL);

        Defense_t DefenseBonus = getPercentValue(m_Defense[ATTR_CURRENT], pRankBonus->getPoint());

        m_Defense[ATTR_CURRENT] = min(SLAYER_MAX_DEFENSE, m_Defense[ATTR_CURRENT] + DefenseBonus);
        m_Defense[ATTR_MAX] = min(SLAYER_MAX_DEFENSE, m_Defense[ATTR_MAX] + DefenseBonus);
    }

    if (hasRankBonus(RankBonus::RANK_BONUS_DRAGON_EYE_2)) {
        RankBonus* pRankBonus = getRankBonus(RankBonus::RANK_BONUS_DRAGON_EYE_2);
        Assert(pRankBonus != NULL);

        int ToHitBonus = getPercentValue(m_ToHit[ATTR_CURRENT], pRankBonus->getPoint());

        m_ToHit[ATTR_CURRENT] = min(SLAYER_MAX_TOHIT, m_ToHit[ATTR_CURRENT] + ToHitBonus);
        m_ToHit[ATTR_MAX] = min(SLAYER_MAX_TOHIT, m_ToHit[ATTR_MAX] + ToHitBonus);
    }
    if (hasRankBonus(RankBonus::RANK_BONUS_EVOLUTION_RELIANCE_BRAIN)) {
        RankBonus* pRankBonus = getRankBonus(RankBonus::RANK_BONUS_EVOLUTION_RELIANCE_BRAIN);
        Assert(pRankBonus != NULL);

        int MPBonus = getPercentValue(m_MP[ATTR_MAX], pRankBonus->getPoint());

        m_MP[ATTR_MAX] = min(SLAYER_MAX_MP, m_MP[ATTR_MAX] + MPBonus);
    }
    if (hasRankBonus(RankBonus::RANK_BONUS_HIT_CONTROL)) {
        RankBonus* pRankBonus = getRankBonus(RankBonus::RANK_BONUS_HIT_CONTROL);
        Assert(pRankBonus != NULL);

        int CriticalRatioBonus = getPercentValue(m_CriticalRatio[ATTR_CURRENT], pRankBonus->getPoint());

        m_CriticalRatio[ATTR_CURRENT] = m_CriticalRatio[ATTR_CURRENT] + CriticalRatioBonus;
        m_CriticalRatio[ATTR_MAX] = m_CriticalRatio[ATTR_MAX] + CriticalRatioBonus;
    }


    // Apply the war bonus.
    if (HPBonus > 0) {
        m_HP[ATTR_MAX] = min(SLAYER_MAX_HP, m_HP[ATTR_MAX] + HPBonus);
    }

    if (RaceWarHPBonus > 0) {
        m_HP[ATTR_MAX] = min(SLAYER_MAX_HP, m_HP[ATTR_MAX] + RaceWarHPBonus);
    }

    if (DragonEyeHPBonus > 0) {
        m_HP[ATTR_MAX] = min(SLAYER_MAX_HP, m_HP[ATTR_MAX] + DragonEyeHPBonus);
    }

    // Initialize the holy land skills.
    initCastleSkill();

    // When current HP exceeds MAX HP.

    // Attributes can change with the size of the party.
}

int Slayer::getBloodBibleSignOpenNum() const {
    GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(m_pPlayer);

    int openNumLimit = 6;
    if (!pGamePlayer->isPayPlaying() && !pGamePlayer->isPremiumPlay()) {
        openNumLimit = 2;
    }

    Fame_t fame = getFame();

    if (!de::gameContext().warSystem().canApplyBloodBibleSign())
        return 0;

    SkillDomainType_t domain = getHighestSkillDomain();
    bool healOrEnchant = (domain == SKILL_DOMAIN_HEAL || domain == SKILL_DOMAIN_ENCHANT);
    return decore::slayerBloodBibleSignOpenNum(fame, openNumLimit, healOrEnchant);
}

//////////////////////////////////////////////////////////////////////////////
// For STR, DEX, INT:
// CURRENT = base value + item value + magic value
// MAX     = base value + item value
// BASIC   = base value
//
// For HP, MP:
// CURRENT = current value
// MAX     = current maximum
// BASIC   = change contributed by items
//
// For Defense, Protection, ToHit:
// CURRENT = current value
// MAX     = change contributed by items
//
// For Damage:
// CURRENT = minimum damage
// MAX     = maximum damage
// BASIC   = change contributed by items
//////////////////////////////////////////////////////////////////////////////
void Slayer::computeStatOffset(void) {
    Creature::CreatureClass CClass = getCreatureClass();
    BASIC_ATTR cur_attr{};

    cur_attr.nSTR = m_STR[ATTR_CURRENT];
    cur_attr.nDEX = m_DEX[ATTR_CURRENT];
    cur_attr.nINT = m_INT[ATTR_CURRENT];
    cur_attr.pWeapon = m_pWearItem[WEAR_RIGHTHAND];

    for (int i = 0; i < SKILL_DOMAIN_VAMPIRE; i++) {
        cur_attr.pDomainLevel[i] = m_SkillDomainLevels[i];
    }

    // Recompute from the updated STR, DEX and INT, then add the
    // item and magic values.
    m_HP[ATTR_MAX] = computeHP(CClass, &cur_attr);
    m_HP[ATTR_MAX] += m_HP[ATTR_BASIC];

    m_MP[ATTR_MAX] = computeMP(CClass, &cur_attr);
    m_MP[ATTR_MAX] += m_MP[ATTR_BASIC];

    m_ToHit[ATTR_CURRENT] = computeToHit(CClass, &cur_attr);
    m_ToHit[ATTR_CURRENT] += m_ToHit[ATTR_MAX];

    m_Defense[ATTR_CURRENT] = computeDefense(CClass, &cur_attr);
    m_Defense[ATTR_CURRENT] += m_Defense[ATTR_MAX];

    m_Protection[ATTR_CURRENT] = computeProtection(CClass, &cur_attr);
    m_Protection[ATTR_CURRENT] += m_Protection[ATTR_MAX];

    m_Damage[ATTR_CURRENT] = computeMinDamage(CClass, &cur_attr);
    m_Damage[ATTR_MAX] = computeMaxDamage(CClass, &cur_attr);

    m_Damage[ATTR_CURRENT] += m_Damage[ATTR_BASIC];
    m_Damage[ATTR_MAX] += m_Damage[ATTR_BASIC];

    m_AttackSpeed[ATTR_CURRENT] = computeAttackSpeed(CClass, &cur_attr);
    m_AttackSpeed[ATTR_CURRENT] += m_AttackSpeed[ATTR_MAX];

    m_CriticalRatio[ATTR_CURRENT] = computeCriticalRatio(CClass, &cur_attr);
    m_CriticalRatio[ATTR_CURRENT] += m_CriticalRatio[ATTR_MAX];
}

void Slayer::computeItemStat(Item* pItem) {
    if (isSlayerWeapon(pItem->getItemClass())) {
        // For a weapon, add the weapon's speed parameter.
        ItemInfo* pItemInfo = de::gameContext().itemInfos().getItemInfo(pItem->getItemClass(), pItem->getItemType());
        m_AttackSpeed[ATTR_CURRENT] += pItemInfo->getSpeed();
        m_AttackSpeed[ATTR_MAX] += pItemInfo->getSpeed();
    }

    if (pItem->getItemClass() == Item::ITEM_CLASS_CORE_ZAP) {
        CoreZapInfo* pItemInfo = dynamic_cast<CoreZapInfo*>(
            de::gameContext().itemInfos().getItemInfo(pItem->getItemClass(), pItem->getItemType()));
        if (pItemInfo != NULL) {
            computeOptionClassStat(pItemInfo->getOptionClass(), pItem->getGrade());
        }
    }

    m_Protection[ATTR_CURRENT] += pItem->getProtectionBonus();
    m_Protection[ATTR_MAX] += pItem->getProtectionBonus();

    m_Defense[ATTR_CURRENT] += pItem->getDefenseBonus();
    m_Defense[ATTR_MAX] += pItem->getDefenseBonus();

    m_ToHit[ATTR_CURRENT] += pItem->getToHitBonus();
    m_ToHit[ATTR_MAX] += pItem->getToHitBonus();

    m_MP[ATTR_MAX] += pItem->getMPBonus();
    m_MP[ATTR_BASIC] += pItem->getMPBonus();

    m_Luck += pItem->getLuck();

    // Additional options.
    const list<OptionType_t>& optionType = pItem->getOptionTypeList();
    list<OptionType_t>::const_iterator itr;
    for (itr = optionType.begin(); itr != optionType.end(); itr++) {
        computeOptionStat(*itr);
    }

    // Apply the item's own defaultOption.
    const list<OptionType_t>& defaultOptions = pItem->getDefaultOptions();
    list<OptionType_t>::const_iterator iOptions;

    for (iOptions = defaultOptions.begin(); iOptions != defaultOptions.end(); iOptions++) {
        computeOptionStat(*iOptions);
    }
}

void Slayer::computeOptionStat(Item* pItem) {
    // Fetch the option types.

    // Additional options.
    const list<OptionType_t>& optionType = pItem->getOptionTypeList();
    list<OptionType_t>::const_iterator itr;
    for (itr = optionType.begin(); itr != optionType.end(); itr++) {
        computeOptionStat(*itr);
    }

    // Apply the item's own defaultOption.
    const list<OptionType_t>& defaultOptions = pItem->getDefaultOptions();
    list<OptionType_t>::const_iterator iOptions;

    for (iOptions = defaultOptions.begin(); iOptions != defaultOptions.end(); iOptions++) {
        computeOptionStat(*iOptions);
    }
}

void Slayer::computeOptionClassStat(OptionClass OClass, int PlusPoint) {
    switch (OClass) {
    case OPTION_STR:
        m_STR[ATTR_CURRENT] += PlusPoint;
        m_STR[ATTR_MAX] += PlusPoint;
        computeStatOffset();
        break;
    case OPTION_DEX:
        m_DEX[ATTR_CURRENT] += PlusPoint;
        m_DEX[ATTR_MAX] += PlusPoint;
        computeStatOffset();
        break;
    case OPTION_INT:
        m_INT[ATTR_CURRENT] += PlusPoint;
        m_INT[ATTR_MAX] += PlusPoint;
        computeStatOffset();
        break;
    case OPTION_HP:
        m_HP[ATTR_MAX] += PlusPoint;
        m_HP[ATTR_BASIC] += PlusPoint;
        break;
    case OPTION_MP:
        m_MP[ATTR_MAX] += PlusPoint;
        m_MP[ATTR_BASIC] += PlusPoint;
        break;
    case OPTION_HP_STEAL:
        m_HPStealAmount += PlusPoint;
        break;
    case OPTION_MP_STEAL:
        m_MPStealAmount += PlusPoint;
        break;
    case OPTION_HP_REGEN:
        m_HPRegen += PlusPoint;
        break;
    case OPTION_MP_REGEN:
        m_MPRegen += PlusPoint;
        break;
    case OPTION_TOHIT:
        m_ToHit[ATTR_CURRENT] += PlusPoint;
        m_ToHit[ATTR_MAX] += PlusPoint;
        break;
    case OPTION_DEFENSE:
        m_Defense[ATTR_CURRENT] += PlusPoint;
        m_Defense[ATTR_MAX] += PlusPoint;
        break;
    case OPTION_DAMAGE:
        m_Damage[ATTR_CURRENT] += PlusPoint;
        m_Damage[ATTR_MAX] += PlusPoint;
        m_Damage[ATTR_BASIC] += PlusPoint;
        break;
    case OPTION_PROTECTION:
        m_Protection[ATTR_CURRENT] += PlusPoint;
        m_Protection[ATTR_MAX] += PlusPoint;
        break;
    case OPTION_ATTACK_SPEED:
        m_AttackSpeed[ATTR_CURRENT] += PlusPoint;
        m_AttackSpeed[ATTR_MAX] += PlusPoint;
        break;
    case OPTION_POISON:
        m_Resist[MAGIC_DOMAIN_POISON] += PlusPoint;
        break;
    case OPTION_ACID:
        m_Resist[MAGIC_DOMAIN_ACID] += PlusPoint;
        break;
    case OPTION_CURSE:
        m_Resist[MAGIC_DOMAIN_CURSE] += PlusPoint;
        break;
    case OPTION_BLOOD:
        m_Resist[MAGIC_DOMAIN_BLOOD] += PlusPoint;
        break;
    case OPTION_VISION:
        break;
    case OPTION_CRITICAL_HIT:
        m_CriticalRatio[ATTR_CURRENT] += PlusPoint;
        m_CriticalRatio[ATTR_MAX] += PlusPoint;
        break;

    case OPTION_ALL_ATTR:
        m_STR[ATTR_CURRENT] += PlusPoint;
        m_STR[ATTR_MAX] += PlusPoint;

        m_DEX[ATTR_CURRENT] += PlusPoint;
        m_DEX[ATTR_MAX] += PlusPoint;

        m_INT[ATTR_CURRENT] += PlusPoint;
        m_INT[ATTR_MAX] += PlusPoint;

        computeStatOffset();
        break;

    case OPTION_ALL_RES:
        m_Resist[MAGIC_DOMAIN_POISON] += PlusPoint;
        m_Resist[MAGIC_DOMAIN_ACID] += PlusPoint;
        m_Resist[MAGIC_DOMAIN_CURSE] += PlusPoint;
        m_Resist[MAGIC_DOMAIN_BLOOD] += PlusPoint;
        break;

    case OPTION_LUCK:
        m_Luck += PlusPoint;
        break;

    case OPTION_STR_TO_DEX: {
        int trans = getPercentValue(m_STR[ATTR_BASIC], PlusPoint);
        m_STR[ATTR_CURRENT] -= trans;
        m_STR[ATTR_MAX] -= trans;
        m_DEX[ATTR_CURRENT] += trans;
        m_DEX[ATTR_MAX] += trans;

        computeStatOffset();
        break;
    }

    case OPTION_STR_TO_INT: {
        int trans = getPercentValue(m_STR[ATTR_BASIC], PlusPoint);
        m_STR[ATTR_CURRENT] -= trans;
        m_STR[ATTR_MAX] -= trans;
        m_INT[ATTR_CURRENT] += trans;
        m_INT[ATTR_MAX] += trans;

        computeStatOffset();
        break;
    }

    case OPTION_DEX_TO_STR: {
        int trans = getPercentValue(m_DEX[ATTR_BASIC], PlusPoint);
        m_DEX[ATTR_CURRENT] -= trans;
        m_DEX[ATTR_MAX] -= trans;
        m_STR[ATTR_CURRENT] += trans;
        m_STR[ATTR_MAX] += trans;

        computeStatOffset();
        break;
    }

    case OPTION_DEX_TO_INT: {
        int trans = getPercentValue(m_DEX[ATTR_BASIC], PlusPoint);
        m_DEX[ATTR_CURRENT] -= trans;
        m_DEX[ATTR_MAX] -= trans;
        m_INT[ATTR_CURRENT] += trans;
        m_INT[ATTR_MAX] += trans;

        computeStatOffset();
        break;
    }

    case OPTION_INT_TO_STR: {
        int trans = getPercentValue(m_INT[ATTR_BASIC], PlusPoint);
        m_INT[ATTR_CURRENT] -= trans;
        m_INT[ATTR_MAX] -= trans;
        m_STR[ATTR_CURRENT] += trans;
        m_STR[ATTR_MAX] += trans;

        computeStatOffset();
        break;
    }

    case OPTION_INT_TO_DEX: {
        int trans = getPercentValue(m_INT[ATTR_BASIC], PlusPoint);
        m_INT[ATTR_CURRENT] -= trans;
        m_INT[ATTR_MAX] -= trans;
        m_DEX[ATTR_CURRENT] += trans;
        m_DEX[ATTR_MAX] += trans;

        computeStatOffset();
        break;
    }
    case OPTION_CONSUME_MP: {
        m_ConsumeMPRatio += PlusPoint;
        break;
    }
    case OPTION_MAGIC_DAMAGE: {
        m_MagicBonusDamage += PlusPoint;
        break;
    }
    case OPTION_PHYSIC_DAMAGE: {
        m_PhysicBonusDamage += PlusPoint;
        break;
    }
    case OPTION_GAMBLE_PRICE: {
        m_GamblePriceRatio += PlusPoint;
        break;
    }
    case OPTION_POTION_PRICE: {
        m_PotionPriceRatio += PlusPoint;
        break;
    }
    case OPTION_PHYSIC_PRO: {
        m_PhysicDamageReduce += PlusPoint;
        break;
    }
    case OPTION_MAGIC_PRO: {
        m_MagicDamageReduce += PlusPoint;
        break;
    }

    default:
        break;
    }
}

void Slayer::computeOptionStat(OptionType_t optionType) {
    OptionInfo* pOptionInfo = de::gameContext().optionInfos().getOptionInfo(optionType);
    computeOptionClassStat(pOptionInfo->getClass(), pOptionInfo->getPlusPoint());
}

void Slayer::addModifyInfo(const SLAYER_RECORD& prev, ModifyInfo& pkt) const

{
    if (prev.pSTR[ATTR_CURRENT] != m_STR[ATTR_CURRENT])
        pkt.addShortData(MODIFY_CURRENT_STR, m_STR[ATTR_CURRENT]);
    if (prev.pSTR[ATTR_MAX] != m_STR[ATTR_MAX])
        pkt.addShortData(MODIFY_MAX_STR, m_STR[ATTR_MAX]);
    if (prev.pSTR[ATTR_BASIC] != m_STR[ATTR_BASIC])
        pkt.addShortData(MODIFY_BASIC_STR, m_STR[ATTR_BASIC]);

    if (prev.pDEX[ATTR_CURRENT] != m_DEX[ATTR_CURRENT])
        pkt.addShortData(MODIFY_CURRENT_DEX, m_DEX[ATTR_CURRENT]);
    if (prev.pDEX[ATTR_MAX] != m_DEX[ATTR_MAX])
        pkt.addShortData(MODIFY_MAX_DEX, m_DEX[ATTR_MAX]);
    if (prev.pDEX[ATTR_BASIC] != m_DEX[ATTR_BASIC])
        pkt.addShortData(MODIFY_BASIC_DEX, m_DEX[ATTR_BASIC]);

    if (prev.pINT[ATTR_CURRENT] != m_INT[ATTR_CURRENT])
        pkt.addShortData(MODIFY_CURRENT_INT, m_INT[ATTR_CURRENT]);
    if (prev.pINT[ATTR_MAX] != m_INT[ATTR_MAX])
        pkt.addShortData(MODIFY_MAX_INT, m_INT[ATTR_MAX]);
    if (prev.pINT[ATTR_BASIC] != m_INT[ATTR_BASIC])
        pkt.addShortData(MODIFY_BASIC_INT, m_INT[ATTR_BASIC]);

    if (prev.pHP[ATTR_CURRENT] != m_HP[ATTR_CURRENT])
        pkt.addShortData(MODIFY_CURRENT_HP, m_HP[ATTR_CURRENT]);
    if (prev.pHP[ATTR_MAX] != m_HP[ATTR_MAX])
        pkt.addShortData(MODIFY_MAX_HP, m_HP[ATTR_MAX]);

    if (prev.pMP[ATTR_CURRENT] != m_MP[ATTR_CURRENT])
        pkt.addShortData(MODIFY_CURRENT_MP, m_MP[ATTR_CURRENT]);
    if (prev.pMP[ATTR_MAX] != m_MP[ATTR_MAX])
        pkt.addShortData(MODIFY_MAX_MP, m_MP[ATTR_MAX]);

    if (prev.pDamage[ATTR_CURRENT] != m_Damage[ATTR_CURRENT])
        pkt.addShortData(MODIFY_MIN_DAMAGE, m_Damage[ATTR_CURRENT]);
    if (prev.pDamage[ATTR_MAX] != m_Damage[ATTR_MAX])
        pkt.addShortData(MODIFY_MAX_DAMAGE, m_Damage[ATTR_MAX]);

    if (prev.Defense != m_Defense[ATTR_CURRENT])
        pkt.addShortData(MODIFY_DEFENSE, m_Defense[ATTR_CURRENT]);
    if (prev.Protection != m_Protection[ATTR_CURRENT])
        pkt.addShortData(MODIFY_PROTECTION, m_Protection[ATTR_CURRENT]);
    if (prev.ToHit != m_ToHit[ATTR_CURRENT])
        pkt.addShortData(MODIFY_TOHIT, m_ToHit[ATTR_CURRENT]);
    if (prev.AttackSpeed != m_AttackSpeed[ATTR_CURRENT])
        pkt.addShortData(MODIFY_ATTACK_SPEED, m_AttackSpeed[ATTR_CURRENT]);

    // by sigi. 2002.9.10
    if (prev.Rank != getRank()) {
        pkt.addShortData(MODIFY_RANK, getRank());
        pkt.addLongData(MODIFY_RANK_EXP, getRankGoalExp());
    }
}

void Slayer::sendModifyInfo(const SLAYER_RECORD& prev) const

{
    GCModifyInformation gcModifyInformation;
    addModifyInfo(prev, gcModifyInformation);
    m_pPlayer->sendPacket(&gcModifyInformation);

    BloodBibleSignInfo* pInfo = getBloodBibleSign();
    GCBloodBibleSignInfo gcInfo;
    gcInfo.setSignInfo(pInfo);
    m_pPlayer->sendPacket(&gcInfo);
}

void Slayer::initAllStatAndSend() {
    SLAYER_RECORD prev;
    getSlayerRecord(prev);
    initAllStat();
    sendModifyInfo(prev);
}
