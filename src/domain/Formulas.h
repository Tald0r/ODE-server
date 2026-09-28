//////////////////////////////////////////////////////////////////////////////
// Filename    : Formulas.h
// Description :
// de-core: pure balance formulas.
//
// Every function here is a pure function of its arguments: no globals, no
// game-object types, no server-type macros, no includes outside the C++
// standard library. The gameserver keeps its original entry points
// (AbilityBalance.cpp, SkillUtil.cpp) as thin adapters that gather the
// inputs from Creature/Item/VariableManager/Properties and delegate here.
//
// The math is transplanted verbatim from those files -- these formulas ARE
// the game balance. Oddities (narrow-integer wrap-around, asymmetric
// weapon-family branches, magic constants) are preserved on purpose and
// pinned by tests/formula_test.cpp; fixing one is a balance change, not a
// refactor.
//
// Known DIVERGENT copies that this file does NOT own (pre-existing drift,
// left untouched): Vampire::load() and Ousters::load() compute a maxHP with
// different STR coefficients than vampireMaxHP/oustersMaxHP below, and
// src/Core/Utility.h declares a Chebyshev getDistance(int,int,int,int)
// alongside SkillUtil's Euclidean one (which delegates to tileDistance
// here). Near-twin inside de-core itself: skillformula's WildWolf
// computeOutput independently encodes the same DEX/8 + STR/30 core as
// wolfDamageBonus below -- both pinned, but the coefficient pair exists
// twice. Folding any of these together is a balance decision, not a
// refactor.
//////////////////////////////////////////////////////////////////////////////

#ifndef DECORE_FORMULAS_H
#define DECORE_FORMULAS_H

namespace decore {

// How the balance formulas see the equipped right-hand weapon. None (bare
// hands) and Other (a weapon class no formula branches on) are distinct
// because slayerStealRatio treats them differently.
enum class WeaponFamily { None, Sword, Blade, Cross, Mace, Arms, Other };

// Numeric inputs of the stat-derivation formulas. weaponDomainLevel is the
// skill-domain level matching `weapon` (sword domain for Sword, gun domain
// for Arms, ...); 0 for None/Other.
struct StatAttr {
    int str;
    int dex;
    int inte;
    int level;
    WeaponFamily weapon;
    int weaponDomainLevel;
};

// getPercentValue from src/Core/Utility.h, duplicated so de-core stays
// freestanding. The (long long) widening is part of the contract.
int percentValue(int value, int percent);

//////////////////////////////////////////////////////////////////////////////
// Stat derivation (adapters: AbilityBalance.cpp). hardcoreTriple is the
// "Hardcore" server config flag: max HP is tripled after the cap.
//////////////////////////////////////////////////////////////////////////////
int slayerMaxHP(const StatAttr& a, int hpRatioPercent, bool hardcoreTriple);
int vampireMaxHP(const StatAttr& a, int hpRatioPercent, bool hardcoreTriple);
int oustersMaxHP(const StatAttr& a, int hpRatioPercent, bool hardcoreTriple);
int monsterMaxHP(const StatAttr& a, int enhancePercent, int hpRatioPercent, bool hardcoreTriple);

int slayerMaxMP(const StatAttr& a);
int oustersMaxMP(const StatAttr& a);

int slayerToHit(const StatAttr& a);
int vampireToHit(const StatAttr& a);
int oustersToHit(const StatAttr& a);
int monsterToHit(const StatAttr& a, int enhancePercent);

int slayerDefense(const StatAttr& a);
int vampireDefense(const StatAttr& a);
int oustersDefense(const StatAttr& a);
int monsterDefense(const StatAttr& a, int enhancePercent);

int slayerProtection(const StatAttr& a);
int vampireProtection(const StatAttr& a);
int oustersProtection(const StatAttr& a);
int monsterProtection(const StatAttr& a, int enhancePercent);

int slayerMinDamage(const StatAttr& a, int combatDamageBonus);
int vampireMinDamage(const StatAttr& a, int combatDamageBonus);
int oustersMinDamage(const StatAttr& a);
int monsterMinDamage(const StatAttr& a, int enhancePercent, int damageRatioPercent);

int slayerMaxDamage(const StatAttr& a, int combatDamageBonus);
int vampireMaxDamage(const StatAttr& a, int combatDamageBonus);
int oustersMaxDamage(const StatAttr& a);
int monsterMaxDamage(const StatAttr& a, int enhancePercent, int damageRatioPercent);

int slayerAttackSpeed(const StatAttr& a);
int vampireAttackSpeed(const StatAttr& a);
int oustersAttackSpeed(const StatAttr& a);

int slayerCriticalRatio(const StatAttr& a);
int vampireCriticalRatio(const StatAttr& a);
int oustersCriticalRatio(const StatAttr& a);
int monsterCriticalRatio(const StatAttr& a, int enhancePercent);

// amount is the steal amount (BYTE-ranged); returns the success percentage,
// 0 for amount 0. From amount 48 (65% base) or 65 (90% base) on, the
// percentage truncates below zero and wraps to a byte (-1 is 255), the
// same result as x86-64's own conversion.
int slayerStealRatio(const StatAttr& a, int amount);
int vampireStealRatio(int amount);
int oustersStealRatio(int amount);

//////////////////////////////////////////////////////////////////////////////
// Combat / progression (adapters: skill/SkillUtil.cpp).
//////////////////////////////////////////////////////////////////////////////

// Final damage after protection. Critical hits ignore protection entirely;
// protection is clamped to 640 and each 8 points shave 1% off the damage,
// floored at 1.
double finalDamage(int realDamage, int protection, bool critical);

// Euclidean tile distance, truncated into the BYTE-wide Range_t exactly as
// the original does (distances beyond 255 wrap -- preserved behavior).
int tileDistance(int ox, int oy, int tx, int ty);

// Rank experience for a kill: killing 20%+ above your level pays a
// quadratic bonus; below that a linear ratio minus 10, floored at 0.
// gainPercent scales the base; premiumPercent applies even when a level is
// 0 (where the base is 0 anyway).
int rankExp(int myLevel, int otherLevel, int gainPercent, int premiumPercent);

// Vampire magic MP cost after the INT discount brackets. intStat is the raw
// INT stat; the 20-point deadband and the bracket table live here.
int vampireSkillConsumeMP(int originalMP, int magicLevel, int intStat);

//////////////////////////////////////////////////////////////////////////////
// Hit-roll success ratios (adapters: skill/HitRoll.cpp). Each function
// returns the success percentage that the caller compares against its dice
// roll -- the roll itself (Random/rand) stays out of de-core, as do the
// gate checks on live game state (no-damage flags, master monsters,
// precedence effects). Stat sums arrive already accumulated at their
// original (possibly narrow) widths in the adapter. One deliberate width
// note: skill levels arrive as int though SkillInfo::getLevel() returns
// uint, so the magic-ratio expressions now evaluate signed where the
// originals were unsigned -- provably identical results (every operand fits
// in int; the original's final (int) cast made the values equal by
// two's-complement).
//////////////////////////////////////////////////////////////////////////////

// Melee hit chance. involvesMonster is true when either combatant is a
// monster: the cap rises to 95 and the floor drops to 5 for those fights.
int meleeHitRatio(int toHit, int defense, int toHitBonus, bool involvesMonster);

// Blood-drain landing chance, over the drain-specific defense the adapter
// computes (defense + level/5 for non-slayer targets, timeband-scaled).
int bloodDrainHitRatio(int toHit, int defense);

// Blood drain requires the target at or below 1/multiplier of max HP
// (multiplier 3 normally, 2 for masters).
bool bloodDrainHPGate(int curHP, int maxHP, int multiplier);

// Magic success per race. Self skills floor at 50 (slayer) / 60 (ousters);
// a nonzero bonusPoint scales the result to (100+bonusPoint)%.
int slayerMagicRatio(int skillLevel, int intStat, int expLevel, bool selfSkill);
int vampireMagicRatio(int skillLevel, int intStat, int level, int bonusPoint);
int oustersMagicRatio(int intStat, int level, int expLevel, bool selfSkill, int bonusPoint);
int monsterMagicRatio(int skillLevel, int intStat, int level);

// Curse landing chance vs. resist; the vampire-cast variant divides the
// magic level by 1.5 (truncated) instead of scaling it by 2/1.5.
int curseRatio(int magicLevel, int resist);
int vampireCurseRatio(int magicLevel, int resist);

// CurePoison and RemoveCurse share this exact formula.
int dispelRatio(int base, int skillLevel, int difficulty, int magicLevel, int minRatio);

// Flare has NO floor: a target enough levels above the skill drives the
// ratio negative and the roll can never succeed -- preserved behavior.
int flareRatio(int skillLevel, int targetLevel);

int rebukeRatio(int intStat, int skillExpLevel);

// MagicElusion and IllusionOfAvenge share 50 + totalAttr/15.
int totalAttrDefenseRatio(int totalAttr);

int poisonMeshRatio(int level);
int willOfLifeRatio(int level);
int backStabRatio(int intStat, int dexStat);

// Hallucination: attacker attr sum vs. target attr sum, clamped into the
// per-race [minRatio, maxRatio] band the adapter selects.
int hallucinationRatio(int attackerAttrSum, int targetAttrSum, int minRatio, int maxRatio);

//////////////////////////////////////////////////////////////////////////////
// initAllStat bonus formulas (adapters: InitAllStat.cpp). The adapters
// keep every gate on live state (equipped item class, canUse() on the
// skill slot, effect flags, isRealWearing) and every member write
// including the per-race stat caps; only the bonus arithmetic lives
// here. grade parameters are the SkillGrade value (0 apprentice ..
// 4 grand master) as an int, same convention as skillformula's
// DomainGrade.
//////////////////////////////////////////////////////////////////////////////

// Concealment (guns only): int-divided stat scaled by a float level
// factor; the divide-then-scale truncation is the shipped math.
int concealmentDefenseBonus(int dex, int effectLevel);
int concealmentProtectionBonus(int str, int effectLevel);

// Will of Iron / Fabulous Soul passive: 15% of max HP via a double
// multiply, truncated.
int willOfIronHPBonus(int maxHP);

struct LivenessBonus {
    int hpPercent;
    int defenseBonus;
};
// Liveness passive (gun domain). hpPercent jumps to 50 from domain
// level 125 regardless of grade.
LivenessBonus livenessBonus(int grade, int domainLevel);
// A second table with different steps and no level override. Unused.
LivenessBonus livenessBonusChina(int grade);

// Sniping mode (SR): percent = STR/20 * expLevel / 20 of current damage,
// DEX/10 * expLevel / 20 of current to-hit -- evaluated left to right:
// the stat division truncates first, the final /20 only after the
// multiply. Shipped operator order, preserved.
int snipingDamageBonus(int curDamage, int str, int expLevel);
int snipingToHitBonus(int curToHit, int dex, int expLevel);

// Weapon-domain passives (slayer).
int swordMasteryDamageBonus(int domainLevel);
int concentrationToHitBonus(int domainLevel);
int evasionDefenseBonus(int domainLevel);
int shieldMasteryProtectionBonus(int domainLevel);

// Vampire transform damage bonuses (same value for min and max damage).
int wolfDamageBonus(int dex, int str);
int werwolfDamageBonus(int dex, int str);

// Vampire Extreme effect (caps included -- they are the formula).
int extremeDamageBonus(int str);
int extremeToHitBonus(int str, int dex);

// Intimate Grail ratios. All three races share the 10 + level/10 ratio,
// but the SIGN of application stays at the call sites: the slayer applies
// it (and the 6.6-divisor HP ratio) as a blessing (+=), vampire and
// ousters as a penalty (-=).
int intimateGrailRatio(int skillLevel);
int intimateGrailHPRatio(int skillLevel);

// Slayer gun-domain flat damage bonus while holding a gun.
int gunDomainDamageBonus(int gunDomainLevel);

// Vampire Nail Mastery: same const + (level - const)/divisor family as
// the slayer passives, negative below level 32 (truncated toward zero
// like evasion).
int nailMasteryDamageBonus(int level);

// Vampire basic-DEX HP-regen ladder: seven thresholds, 0 at DEX <= 50.
int vampireDexHPRegenBonus(int dexBasic);

// Ousters soul-stone passive points (adapter gates on
// satisfySkillRequire and stores into m_PassiveSkillMap; the original
// (uint) store of the double is value-identical to the int return for
// the reachable non-negative range).
int fireOfSoulStonePoint(int str, int dex);
int iceOfSoulStonePoint(int dex);
int sandOfSoulStonePoint(int str, int dex);
int blockHeadPoint(int dex);
int blessFirePoint(int str, int dex);
int sandCrossPoint(int str, int dex);

// Blood Bible sign-open ladders, one per race -- the fame thresholds have
// drifted between races and slayer has a second ladder for HEAL/ENCHANT
// domains, which is exactly why they are pinned here. fame is Fame_t
// (DWORD) -- unsigned comparison semantics preserved. The adapters keep
// the pay-status openNumLimit and the canApplyBloodBibleSign() gate
// (0 when closed).
int slayerBloodBibleSignOpenNum(unsigned int fame, int openNumLimit, bool healOrEnchantDomain);
int vampireBloodBibleSignOpenNum(unsigned int fame, int openNumLimit);
int oustersBloodBibleSignOpenNum(unsigned int fame, int openNumLimit);

// Ousters Summon Sylph flat bonuses, floored at 5.
int summonSylphProtectionBonus(int level);
int summonSylphResistBonus(int level);

// Ousters Hide Sight passive (chakram): two level bands, with a 10%
// bump exactly at exp level 30, truncated.
int hideSightToHitBonus(int expLevel);

} // namespace decore

#endif
