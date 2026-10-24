//////////////////////////////////////////////////////////////////////////////
// FileName 	: VampireStat.cpp
// Description	: Vampire stat computation: the castle skills, the all-stat recalculation and the item, option and blood bible contributions to it.
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

void Vampire::initCastleSkill() {
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

        VampireCastleSkillSlot* pCastleSkillSlot = new VampireCastleSkillSlot();

        pCastleSkillSlot->setName(m_Name);
        pCastleSkillSlot->setSkillType(CastleSkillType);
        pCastleSkillSlot->setInterval(Delay);
        pCastleSkillSlot->setRunTime();

        addSkill(pCastleSkillSlot);
    }
}

void Vampire::initAllStat(int numPartyMember)

{
    BASIC_ATTR attr;
    Creature::CreatureClass CClass = getCreatureClass();

    m_Resist[MAGIC_DOMAIN_NO_DOMAIN] = 0;
    m_Resist[MAGIC_DOMAIN_POISON] = 50;
    m_Resist[MAGIC_DOMAIN_ACID] = 0;
    m_Resist[MAGIC_DOMAIN_CURSE] = 0;
    m_Resist[MAGIC_DOMAIN_BLOOD] = 0;

    m_Mastery[MAGIC_DOMAIN_NO_DOMAIN] = 0;
    m_Mastery[MAGIC_DOMAIN_POISON] = 0;
    m_Mastery[MAGIC_DOMAIN_ACID] = 0;
    m_Mastery[MAGIC_DOMAIN_CURSE] = 0;
    m_Mastery[MAGIC_DOMAIN_BLOOD] = 0;

    // Reset the BloodBible related bonus values
    m_ConsumeMPRatio = 0;
    m_GamblePriceRatio = 0;
    m_PotionPriceRatio = 0;
    m_MagicBonusDamage = 0;
    m_PhysicBonusDamage = 0;
    m_MagicDamageReduce = 0;
    m_PhysicDamageReduce = 0;

    //////////////////////////////////////////////////////////////////////////////
    // First reset the base attributes, then
    // check the effects that modify them.
    //////////////////////////////////////////////////////////////////////////////
    m_STR[ATTR_CURRENT] = m_STR[ATTR_MAX] = m_STR[ATTR_BASIC];
    m_DEX[ATTR_CURRENT] = m_DEX[ATTR_MAX] = m_DEX[ATTR_BASIC];
    m_INT[ATTR_CURRENT] = m_INT[ATTR_MAX] = m_INT[ATTR_BASIC];

    //////////////////////////////////////////////////////////////////////////////
    // Initialize the parameters used for the attribute computation.
    //////////////////////////////////////////////////////////////////////////////
    attr.nSTR = m_STR[ATTR_CURRENT];
    attr.nDEX = m_DEX[ATTR_CURRENT];
    attr.nINT = m_INT[ATTR_CURRENT];
    attr.pWeapon = NULL;
    attr.nLevel = m_Level;

    m_HPStealRatio = 0;
    m_HPStealAmount = 0;
    m_HPRegen = 0;
    m_Luck = m_BaseLuck;
    m_HPRegenBonus = 0;

    ////////////////////////////////////////////////////////////
    // Recompute the derived attributes.
    ////////////////////////////////////////////////////////////
    m_HP[ATTR_MAX] = computeHP(CClass, &attr);
    m_HP[ATTR_BASIC] = 0;
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

    int RaceWarHPBonus = 0;

    if (m_pZone->isHolyLand() || m_pZone->isLevelWarZone()) {
        RaceWarHPBonus = getPercentValue(m_HP[ATTR_MAX], de::gameContext().variables().getRaceWarHPBonus());
    }

    int DragonEyeHPBonus = 0;
    if (isFlag(Effect::EFFECT_CLASS_DRAGON_EYE)) {
        // The HP bonus is doubled
        DragonEyeHPBonus = m_HP[ATTR_MAX];
    }

    // War bonus.
    // For now the bonus can apply to either side, win or lose.
    int HPBonus = 0;
    {
        int bonusRatio = de::gameContext().variables().getCombatVampireHPBonusRatio();

        if (bonusRatio > 0) {
            HPBonus = getPercentValue(m_HP[ATTR_MAX], bonusRatio);
        }
    }

    //////////////////////////////////////////////////////////////////////////////
    // Reset the gear check flags first, so every gear slot starts out as not worn.
    //////////////////////////////////////////////////////////////////////////////
    bool pOldRealWearingCheck[VAMPIRE_WEAR_MAX]; // by sigi. 2002.10.31
    for (int i = 0; i < VAMPIRE_WEAR_MAX; i++) {
        pOldRealWearingCheck[i] = m_pRealWearingCheck[i];
        m_pRealWearingCheck[i] = false;
    }

    //////////////////////////////////////////////////////////////////////////////
    // The race that owns the castle receives a bonus option.
    //////////////////////////////////////////////////////////////////////////////
    // Each Blood Bible grants its own bonus option.
    //////////////////////////////////////////////////////////////////////////////
    // Each Blood Bible bonus option is applied.
    //////////////////////////////////////////////////////////////////////////////

    if (de::gameContext().sweeperBonuses().isAble(getZoneID()) &&
        de::gameContext().levelWarZones().isCreatureBonusZone(this, getZoneID())) {
        const SweeperBonusHashMap& sweeperBonuses = de::gameContext().sweeperBonuses().getSweeperBonuses();

        SweeperBonusHashMapConstItor itr = sweeperBonuses.begin();
        SweeperBonusHashMapConstItor endItr = sweeperBonuses.end();

        for (; itr != endItr; itr++) {
            if (itr->second->getRace() == RACE_VAMPIRE &&
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
    // Compute the bonus granted by the pet.
    //////////////////////////////////////////////////////////////////////////////
    if (m_pPetInfo != NULL) {
        if (m_pPetInfo->getPetAttr() != 0xff)
            computeOptionClassStat((OptionClass)m_pPetInfo->getPetAttr(), (int)m_pPetInfo->getPetAttrLevel());
        if (m_pPetInfo->getPetOption() != 0)
            computeOptionStat(m_pPetInfo->getPetOption());
    }

    //////////////////////////////////////////////////////////////////////////////
    // The loop runs twice so that items which only become wearable
    // through attributes raised by other items are checked too.
    //////////////////////////////////////////////////////////////////////////////
    for (int j = 0; j < VAMPIRE_WEAR_MAX; j++) {
        int wearCount = 0;
        for (int i = 0; i < VAMPIRE_WEAR_MAX; i++) {
            Item* pItem = m_pWearItem[i];
            // If there is an item in this slot and
            // it has not been checked yet...
            if (pItem != NULL && m_pRealWearingCheck[i] == false) {
                // Raise the attributes if the item really can be worn.
                if (isRealWearing(pItem)) {
                    computeItemStat(pItem);

                    // For a two-handed weapon set the check flag for both
                    // the left and the right hand, so it is not counted twice.
                    if (isTwohandWeapon(pItem)) {
                        m_pRealWearingCheck[WEAR_LEFTHAND] = true;
                        m_pRealWearingCheck[WEAR_RIGHTHAND] = true;
                    } else
                        m_pRealWearingCheck[i] = true;

                    wearCount++;
                }
            }
        }

        if (wearCount == 0) // by sigi. 2002.10.30
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

    // Everything above was treated as worn, but
    // items whose outfit does not apply at these attributes have their outfit info cleared.
    // by sigi. 2002.10.30
    int i = WEAR_BODY;
    {
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
                        GCChangeShape pkt;
                        pkt.setObjectID(getObjectID());
                        pkt.setItemClass(IClass);
                        pkt.setItemType(IType);
                        pkt.setOptionType(pItem->getFirstOptionType());
                        pkt.setAttackSpeed(m_AttackSpeed[ATTR_CURRENT]);

                        m_pZone->broadcastPacket(m_X, m_Y, &pkt, this);
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

    //////////////////////////////////////////////////////////////////////////////
    // Compute the HP and MP steal chances.
    //////////////////////////////////////////////////////////////////////////////
    m_HPStealRatio = computeStealRatio(CClass, m_HPStealAmount, &attr);

    //////////////////////////////////////////////////////////////////////////////
    // Check the effects that directly modify the derived attributes.
    //////////////////////////////////////////////////////////////////////////////
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
    if (isFlag(Effect::EFFECT_CLASS_TRANSFORM_TO_WOLF)) {
        EffectTransformToWolf* pTransformToWolf =
            dynamic_cast<EffectTransformToWolf*>(findEffect(Effect::EFFECT_CLASS_TRANSFORM_TO_WOLF));
        if (pTransformToWolf != NULL) {
            int PenaltyRatio = (isFlag(Effect::EFFECT_CLASS_HOWL)) ? 10 : 30;
            int ToHitBonus = getPercentValue(m_ToHit[ATTR_CURRENT], 20);
            int MinDamageBonus = decore::wolfDamageBonus(
                m_DEX[ATTR_CURRENT], m_STR[ATTR_CURRENT]); // getPercentValue(m_Damage[ATTR_CURRENT], 20);
            int MaxDamageBonus = decore::wolfDamageBonus(
                m_DEX[ATTR_CURRENT], m_STR[ATTR_CURRENT]); // getPercentValue(m_Damage[ATTR_MAX], 20);
            int DefensePenalty = getPercentValue(m_Defense[ATTR_CURRENT], PenaltyRatio);       // 50);
            int ProtectionPenalty = getPercentValue(m_Protection[ATTR_CURRENT], PenaltyRatio); // 50);

            m_ToHit[ATTR_CURRENT] = min(VAMPIRE_MAX_TOHIT, m_ToHit[ATTR_CURRENT] + ToHitBonus);
            m_Damage[ATTR_CURRENT] = min(VAMPIRE_MAX_DAMAGE, m_Damage[ATTR_CURRENT] + MinDamageBonus);
            m_Damage[ATTR_MAX] = min(VAMPIRE_MAX_DAMAGE, m_Damage[ATTR_MAX] + MaxDamageBonus);
            m_Defense[ATTR_CURRENT] = max(0, m_Defense[ATTR_CURRENT] - DefensePenalty);
            m_Protection[ATTR_CURRENT] = max(0, m_Protection[ATTR_CURRENT] - ProtectionPenalty);
        }
    } else if (isFlag(Effect::EFFECT_CLASS_HOWL)) {
        Effect* pEffect = findEffect(Effect::EFFECT_CLASS_HOWL);
        if (pEffect != NULL)
            pEffect->setDeadline(0);
    }

    if (isFlag(Effect::EFFECT_CLASS_TRANSFORM_TO_WERWOLF)) {
        EffectTransformToWerwolf* pTransformToWerwolf =
            dynamic_cast<EffectTransformToWerwolf*>(findEffect(Effect::EFFECT_CLASS_TRANSFORM_TO_WERWOLF));
        if (pTransformToWerwolf != NULL) {
            int ToHitBonus = getPercentValue(m_ToHit[ATTR_CURRENT], 20);
            int MinDamageBonus = decore::werwolfDamageBonus(
                m_DEX[ATTR_CURRENT], m_STR[ATTR_CURRENT]); // getPercentValue(m_Damage[ATTR_CURRENT], 20);
            int MaxDamageBonus = decore::werwolfDamageBonus(
                m_DEX[ATTR_CURRENT], m_STR[ATTR_CURRENT]); // getPercentValue(m_Damage[ATTR_MAX], 20);
            int ResistBonus = 9;

            m_ToHit[ATTR_CURRENT] = min(VAMPIRE_MAX_TOHIT, m_ToHit[ATTR_CURRENT] + ToHitBonus);
            m_Damage[ATTR_CURRENT] = min(VAMPIRE_MAX_DAMAGE, m_Damage[ATTR_CURRENT] + MinDamageBonus);
            m_Damage[ATTR_MAX] = min(VAMPIRE_MAX_DAMAGE, m_Damage[ATTR_MAX] + MaxDamageBonus);

            m_Resist[MAGIC_DOMAIN_POISON] += ResistBonus;
            m_Resist[MAGIC_DOMAIN_ACID] += ResistBonus;
            m_Resist[MAGIC_DOMAIN_CURSE] += ResistBonus;
            m_Resist[MAGIC_DOMAIN_BLOOD] += ResistBonus;
        }
    }
    if (isFlag(Effect::EFFECT_CLASS_TRANSFORM_TO_BAT)) {
        EffectTransformToBat* pTransformToBat =
            dynamic_cast<EffectTransformToBat*>(findEffect(Effect::EFFECT_CLASS_TRANSFORM_TO_BAT));
        if (pTransformToBat != NULL) {
            int DefensePenalty = getPercentValue(m_Defense[ATTR_CURRENT], 25);
            int ProtectionPenalty = getPercentValue(m_Protection[ATTR_CURRENT], 25);

            m_Defense[ATTR_CURRENT] = max(0, m_Defense[ATTR_CURRENT] - DefensePenalty);
            m_Protection[ATTR_CURRENT] = max(0, m_Protection[ATTR_CURRENT] - ProtectionPenalty);
        }
    }
    if (isFlag(Effect::EFFECT_CLASS_EXTREME)) {
        EffectExtreme* pExtreme = dynamic_cast<EffectExtreme*>(findEffect(Effect::EFFECT_CLASS_EXTREME));
        if (pExtreme != NULL) {
            int DamageBonus = decore::extremeDamageBonus(m_STR[ATTR_CURRENT]);
            int ToHitBonus = decore::extremeToHitBonus(m_STR[ATTR_CURRENT], m_DEX[ATTR_CURRENT]);

            m_Damage[ATTR_CURRENT] = max(0, m_Damage[ATTR_CURRENT] + DamageBonus);
            m_Damage[ATTR_MAX] = max(0, m_Damage[ATTR_MAX] + DamageBonus);
            m_ToHit[ATTR_CURRENT] = min(VAMPIRE_MAX_TOHIT, m_ToHit[ATTR_CURRENT] + ToHitBonus);
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

    if (isFlag(Effect::EFFECT_CLASS_MEPHISTO)) {
        EffectMephisto* pMephisto = dynamic_cast<EffectMephisto*>(findEffect(Effect::EFFECT_CLASS_MEPHISTO));
        if (pMephisto != NULL) {
            int bonusPercent = 100 + pMephisto->getBonus();

            m_ToHit[ATTR_CURRENT] = min(VAMPIRE_MAX_TOHIT, m_ToHit[ATTR_CURRENT] * bonusPercent / 100);
            m_Defense[ATTR_CURRENT] = min(VAMPIRE_MAX_DEFENSE, m_Defense[ATTR_CURRENT] * bonusPercent / 100);
            m_Protection[ATTR_CURRENT] = min(VAMPIRE_MAX_PROTECTION, m_Protection[ATTR_CURRENT] * bonusPercent / 100);
        }
    }

    // by sigi. 2002.6.19
    if (isFlag(Effect::EFFECT_CLASS_CASKET)) {
        EffectSummonCasket* pCasket = dynamic_cast<EffectSummonCasket*>(findEffect(Effect::EFFECT_CLASS_CASKET));
        if (pCasket != NULL) {
            // This could differ depending on pCasket->getType().
            // by sigi. 2002.12.3. 20 --> 30
            int DefenseBonus = getPercentValue(m_Defense[ATTR_CURRENT], 30);
            int ProtectionBonus = getPercentValue(m_Protection[ATTR_CURRENT], 30);

            m_Defense[ATTR_CURRENT] = max(0, m_Defense[ATTR_CURRENT] + DefenseBonus);
            m_Protection[ATTR_CURRENT] = max(0, m_Protection[ATTR_CURRENT] + ProtectionBonus);
        }
    }
    if (isFlag(Effect::EFFECT_CLASS_BLUNTING)) {
        EffectBlunting* pBlunting = dynamic_cast<EffectBlunting*>(findEffect(Effect::EFFECT_CLASS_BLUNTING));
        if (pBlunting != NULL) {
            int DefensePenalty = pBlunting->getDefensePenalty();
            m_Defense[ATTR_CURRENT] = max(0, m_Defense[ATTR_CURRENT] - DefensePenalty);
        }
    }

    if (isFlag(Effect::EFFECT_CLASS_INTIMATE_GRAIL)) {
        EffectIntimateGrail* pIntimateGrail =
            dynamic_cast<EffectIntimateGrail*>(findEffect(Effect::EFFECT_CLASS_INTIMATE_GRAIL));
        if (pIntimateGrail != NULL) {
            int ratio = decore::intimateGrailRatio(pIntimateGrail->getSkillLevel());
            m_Defense[ATTR_CURRENT] -= getPercentValue(m_Defense[ATTR_CURRENT], ratio);
            m_HP[ATTR_CURRENT] -= getPercentValue(m_HP[ATTR_CURRENT], ratio);
            m_HP[ATTR_MAX] -= getPercentValue(m_HP[ATTR_MAX], ratio);
        }
    }


    ///////////////////////////////////////////////////////////////////////////////
    // Compute the rank bonus.
    ///////////////////////////////////////////////////////////////////////////////
    if (hasRankBonus(RankBonus::RANK_BONUS_IMMORTAL_BLOOD)) {
        RankBonus* pRankBonus = getRankBonus(RankBonus::RANK_BONUS_IMMORTAL_BLOOD);
        Assert(pRankBonus != NULL);

        int HPBonus = pRankBonus->getPoint();

        m_HP[ATTR_MAX] = min(VAMPIRE_MAX_HP, m_HP[ATTR_MAX] + HPBonus);
    }
    if (hasRankBonus(RankBonus::RANK_BONUS_BEHEMOTH_SKIN)) {
        RankBonus* pRankBonus = getRankBonus(RankBonus::RANK_BONUS_BEHEMOTH_SKIN);
        Assert(pRankBonus != NULL);

        int DefenseBonus = pRankBonus->getPoint();

        m_Defense[ATTR_CURRENT] = min(VAMPIRE_MAX_DEFENSE, m_Defense[ATTR_CURRENT] + DefenseBonus);
    }
    if (hasRankBonus(RankBonus::RANK_BONUS_SAFE_ROBE)) {
        RankBonus* pRankBonus = getRankBonus(RankBonus::RANK_BONUS_SAFE_ROBE);
        Assert(pRankBonus != NULL);

        int ProtectionBonus = pRankBonus->getPoint();

        m_Protection[ATTR_CURRENT] = min(VAMPIRE_MAX_PROTECTION, m_Protection[ATTR_CURRENT] + ProtectionBonus);
    }
    if (hasRankBonus(RankBonus::RANK_BONUS_CROW_WING)) {
        RankBonus* pRankBonus = getRankBonus(RankBonus::RANK_BONUS_CROW_WING);
        Assert(pRankBonus != NULL);

        int AttackSpeedBonus = pRankBonus->getPoint();

        m_AttackSpeed[ATTR_CURRENT] += AttackSpeedBonus;
        m_AttackSpeed[ATTR_MAX] += AttackSpeedBonus;
    }
    if (hasRankBonus(RankBonus::RANK_BONUS_URANUS_BLESS)) {
        RankBonus* pRankBonus = getRankBonus(RankBonus::RANK_BONUS_URANUS_BLESS);
        Assert(pRankBonus != NULL);

        int HPRegenBonus = pRankBonus->getPoint();

        m_HPRegenBonus += HPRegenBonus;
    }
    if (hasRankBonus(RankBonus::RANK_BONUS_ACID_INQUIRY)) {
        RankBonus* pRankBonus = getRankBonus(RankBonus::RANK_BONUS_ACID_INQUIRY);
        Assert(pRankBonus != NULL);

        m_Resist[MAGIC_DOMAIN_ACID] += getPercentValue(m_Resist[MAGIC_DOMAIN_ACID], pRankBonus->getPoint());
        ;
    }
    if (hasRankBonus(RankBonus::RANK_BONUS_BLOODY_INQUIRY)) {
        RankBonus* pRankBonus = getRankBonus(RankBonus::RANK_BONUS_BLOODY_INQUIRY);
        Assert(pRankBonus != NULL);

        m_Resist[MAGIC_DOMAIN_BLOOD] += getPercentValue(m_Resist[MAGIC_DOMAIN_BLOOD], pRankBonus->getPoint());
        ;
    }
    if (hasRankBonus(RankBonus::RANK_BONUS_CURSE_INQUIRY)) {
        RankBonus* pRankBonus = getRankBonus(RankBonus::RANK_BONUS_CURSE_INQUIRY);
        Assert(pRankBonus != NULL);

        m_Resist[MAGIC_DOMAIN_CURSE] += getPercentValue(m_Resist[MAGIC_DOMAIN_CURSE], pRankBonus->getPoint());
        ;
    }
    if (hasRankBonus(RankBonus::RANK_BONUS_POISON_INQUIRY)) {
        RankBonus* pRankBonus = getRankBonus(RankBonus::RANK_BONUS_POISON_INQUIRY);
        Assert(pRankBonus != NULL);

        m_Resist[MAGIC_DOMAIN_POISON] += getPercentValue(m_Resist[MAGIC_DOMAIN_POISON], pRankBonus->getPoint());
        ;
    }
    if (hasRankBonus(RankBonus::RANK_BONUS_INQUIRY_MASTERY)) {
        RankBonus* pRankBonus = getRankBonus(RankBonus::RANK_BONUS_INQUIRY_MASTERY);
        Assert(pRankBonus != NULL);

        for (int i = 0; i < MAGIC_DOMAIN_MAX; i++) {
            m_Resist[i] += getPercentValue(m_Resist[i], pRankBonus->getPoint());
            ;
        }
    }

    if (hasRankBonus(RankBonus::RANK_BONUS_ACID_MASTERY)) {
        RankBonus* pRankBonus = getRankBonus(RankBonus::RANK_BONUS_ACID_MASTERY);
        Assert(pRankBonus != NULL);

        m_Mastery[MAGIC_DOMAIN_ACID] += pRankBonus->getPoint();
    }
    if (hasRankBonus(RankBonus::RANK_BONUS_BLOODY_MASTERY)) {
        RankBonus* pRankBonus = getRankBonus(RankBonus::RANK_BONUS_BLOODY_MASTERY);
        Assert(pRankBonus != NULL);

        m_Mastery[MAGIC_DOMAIN_BLOOD] += pRankBonus->getPoint();
    }
    if (hasRankBonus(RankBonus::RANK_BONUS_CURSE_MASTERY)) {
        RankBonus* pRankBonus = getRankBonus(RankBonus::RANK_BONUS_CURSE_MASTERY);
        Assert(pRankBonus != NULL);

        m_Mastery[MAGIC_DOMAIN_CURSE] += pRankBonus->getPoint();
    }
    if (hasRankBonus(RankBonus::RANK_BONUS_POISON_MASTERY)) {
        RankBonus* pRankBonus = getRankBonus(RankBonus::RANK_BONUS_POISON_MASTERY);
        Assert(pRankBonus != NULL);

        m_Mastery[MAGIC_DOMAIN_POISON] += pRankBonus->getPoint();
    }
    if (hasRankBonus(RankBonus::RANK_BONUS_SKILL_MASTERY)) {
        RankBonus* pRankBonus = getRankBonus(RankBonus::RANK_BONUS_SKILL_MASTERY);
        Assert(pRankBonus != NULL);

        for (int i = 0; i < MAGIC_DOMAIN_MAX; i++) {
            m_Mastery[i] += getPercentValue(m_Mastery[i], pRankBonus->getPoint());
            ;
        }
    }

    // HPRegenBonus points from DEX
    m_HPRegenBonus += decore::vampireDexHPRegenBonus(m_DEX[ATTR_BASIC]);

    // Attributes may change with the size of the party.

    // Apply the war bonus
    if (HPBonus > 0) {
        m_HP[ATTR_MAX] = min(VAMPIRE_MAX_HP, m_HP[ATTR_MAX] + HPBonus);
    }

    if (RaceWarHPBonus > 0) {
        m_HP[ATTR_MAX] = min(VAMPIRE_MAX_HP, m_HP[ATTR_MAX] + RaceWarHPBonus);
    }

    if (DragonEyeHPBonus > 0) {
        m_HP[ATTR_MAX] = min(VAMPIRE_MAX_HP, m_HP[ATTR_MAX] + DragonEyeHPBonus);
    }

    // If the current HP exceeds the maximum HP,
    // set the current value to the maximum
    if (m_HP[ATTR_CURRENT] > m_HP[ATTR_MAX]) {
        m_HP[ATTR_CURRENT] = m_HP[ATTR_MAX];
    }

    //////////////////////////////////////////////////////////////////////////////
    // Compute the passive skills.
    //////////////////////////////////////////////////////////////////////////////
    VampireSkillSlot* pNailMastery = getSkill(SKILL_NAIL_MASTERY);
    if (pNailMastery != NULL) {
        int DamageBonus = decore::nailMasteryDamageBonus(getLevel());

        m_Damage[ATTR_CURRENT] = max(0, m_Damage[ATTR_CURRENT] + DamageBonus);
        m_Damage[ATTR_MAX] = max(0, m_Damage[ATTR_MAX] + DamageBonus);
    }


    initCastleSkill();

    if (isFlag(Effect::EFFECT_CLASS_TRANSFORM_TO_BAT)) {
        m_Resist[MAGIC_DOMAIN_NO_DOMAIN] = 0;
        m_Resist[MAGIC_DOMAIN_POISON] = 50;
        m_Resist[MAGIC_DOMAIN_ACID] = 0;
        m_Resist[MAGIC_DOMAIN_CURSE] = 0;
        m_Resist[MAGIC_DOMAIN_BLOOD] = 0;
    }
}

int Vampire::getBloodBibleSignOpenNum() const {
    GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(m_pPlayer);

    int openNumLimit = 6;
    if (!pGamePlayer->isPayPlaying() && !pGamePlayer->isPremiumPlay()) {
        openNumLimit = 2;
    }

    Fame_t fame = getFame();

    if (!de::gameContext().warSystem().canApplyBloodBibleSign())
        return 0;

    return decore::vampireBloodBibleSignOpenNum(fame, openNumLimit);
}

//////////////////////////////////////////////////////////////////////////////
// For STR, DEX and INT:
// CURRENT = base value + item value + magic value
// MAX     = base value + item value
// BASIC   = base value
//
// For HP and MP:
// CURRENT = current value
// MAX     = current maximum
// BASIC   = change contributed by items
//
// For Defense, Protection and ToHit:
// CURRENT = current value
// MAX     = change contributed by items
//
// For Damage:
// CURRENT = Min damage
// MAX     = Max damage
// BASIC   = change contributed by items
//////////////////////////////////////////////////////////////////////////////
void Vampire::computeStatOffset()

{
    Creature::CreatureClass CClass = getCreatureClass();
    BASIC_ATTR cur_attr;

    cur_attr.nSTR = m_STR[ATTR_CURRENT];
    cur_attr.nDEX = m_DEX[ATTR_CURRENT];
    cur_attr.nINT = m_INT[ATTR_CURRENT];
    cur_attr.nLevel = m_Level;

    // Recompute with the renewed STR, DEX and INT, then
    // add the item and magic values.
    m_HP[ATTR_MAX] = computeHP(CClass, &cur_attr);
    m_HP[ATTR_MAX] += m_HP[ATTR_BASIC];

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

void Vampire::computeItemStat(Item* pItem)

{
    if (isVampireWeapon(pItem->getItemClass())) {
        // For a weapon, add the speed parameter the weapon carries.
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

    m_Luck += pItem->getLuck();

    // Additional options
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

void Vampire::computeOptionStat(Item* pItem)

{
    // Fetch the option type.

    // Additional options
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

void Vampire::computeOptionClassStat(OptionClass OClass, int PlusPoint) {
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
        m_HP[ATTR_MAX] += PlusPoint;
        m_HP[ATTR_BASIC] += PlusPoint;
        break;
    // For a vampire an item with an MP steal option is treated as HP steal too.
    // 2003. 1. 17. Sequoia
    case OPTION_HP_STEAL:
    case OPTION_MP_STEAL:
        m_HPStealAmount += PlusPoint;
        break;
    case OPTION_HP_REGEN:
    case OPTION_MP_REGEN:
        m_HPRegen += PlusPoint;
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
    case OPTION_ATTACK_SPEED:
        m_AttackSpeed[ATTR_CURRENT] += PlusPoint;
        m_AttackSpeed[ATTR_MAX] += PlusPoint;
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

    case OPTION_CONSUME_MP:
        m_ConsumeMPRatio += PlusPoint;
        break;

    case OPTION_MAGIC_DAMAGE:
        m_MagicBonusDamage += PlusPoint;
        break;

    case OPTION_PHYSIC_DAMAGE:
        m_PhysicBonusDamage += PlusPoint;
        break;

    case OPTION_GAMBLE_PRICE:
        m_GamblePriceRatio += PlusPoint;
        break;

    case OPTION_POTION_PRICE:
        m_PotionPriceRatio += PlusPoint;
        break;
    case OPTION_PHYSIC_PRO:
        m_PhysicDamageReduce += PlusPoint;
        break;
    case OPTION_MAGIC_PRO:
        m_MagicDamageReduce += PlusPoint;
        break;

    default:
        break;
    }
}

void Vampire::computeOptionStat(OptionType_t OptionType)

{
    OptionInfo* pOptionInfo = de::gameContext().optionInfos().getOptionInfo(OptionType);
    computeOptionClassStat(pOptionInfo->getClass(), pOptionInfo->getPlusPoint());
}

void Vampire::addModifyInfo(const VAMPIRE_RECORD& prev, ModifyInfo& pkt) const

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

void Vampire::sendModifyInfo(const VAMPIRE_RECORD& prev) const

{
    GCModifyInformation gcModifyInformation;
    addModifyInfo(prev, gcModifyInformation);
    m_pPlayer->sendPacket(&gcModifyInformation);

    BloodBibleSignInfo* pInfo = getBloodBibleSign();
    GCBloodBibleSignInfo gcInfo;
    gcInfo.setSignInfo(pInfo);
    m_pPlayer->sendPacket(&gcInfo);
}

void Vampire::initAllStatAndSend() {
    VAMPIRE_RECORD prev;
    getVampireRecord(prev);
    initAllStat();
    sendModifyInfo(prev);
}
