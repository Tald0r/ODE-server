//////////////////////////////////////////////////////////////////////////////
// Filename    : Formulas.cpp
// Description :
// Pure balance formulas, the math behind AbilityBalance.cpp and
// skill/SkillUtil.cpp. See Formulas.h.
//
// The originals accumulate into the narrow wire typedefs (HP_t/Damage_t are
// WORD, Speed_t/Range_t/Steal_t are BYTE), so extreme inputs wrap. That
// wrap-around is observed behavior -- the transplants keep the same widths
// on purpose, and tests/formula_test.cpp pins representative wrap cases.
//////////////////////////////////////////////////////////////////////////////

#include "domain/Formulas.h"

#include <algorithm>
#include <cmath>

using std::max;
using std::min;
// <cmath> is only guaranteed to provide std::pow/std::sqrt; the global
// names are an implementation courtesy this freestanding library must not
// lean on.
using std::pow;
using std::sqrt;

namespace decore {

namespace {
// Width-mirrors of the Core typedefs (Core's WORD/BYTE/DWORD). de-core is
// freestanding, so it carries its own copies.
typedef unsigned short Word;
typedef unsigned char Byte;
typedef unsigned int DWord;

// Caps from AbilityBalance.h.
const int kSlayerMaxHP = 10000;
const int kSlayerMaxMP = 10000;
const int kSlayerMaxToHit = 10000;
const int kSlayerMaxDefense = 10000;
const int kSlayerMaxProtection = 10000;
const int kSlayerMaxDamage = 10000;

const int kVampireMaxHP = 10000;
const int kVampireMaxToHit = 10000;
const int kVampireMaxDefense = 10000;
const int kVampireMaxProtection = 10000;
const int kVampireMaxDamage = 10000;

const int kOustersMaxHP = 10000;
const int kOustersMaxMP = 10000;
const int kOustersMaxToHit = 10000;
const int kOustersMaxDefense = 10000;
const int kOustersMaxProtection = 10000;
const int kOustersMaxDamage = 10000;

const int kMonsterMaxHP = 60000;
const int kMonsterMaxToHit = 1000;
const int kMonsterMaxDefense = 1000;
const int kMonsterMaxProtection = 1000;
const int kMonsterMaxDamage = 1000;

const Byte kSlayerMaxAttackSpeed = 35;
const Byte kVampireMaxAttackSpeed = 30;
const Byte kOustersMaxAttackSpeed = 35;
} // namespace

int percentValue(int value, int percent) {
    return (int)((long long)(value) * (long long)(percent) / 100);
}

//////////////////////////////////////////////////////////////////////////////
// Max HP
//////////////////////////////////////////////////////////////////////////////

int slayerMaxHP(const StatAttr& a, int hpRatioPercent, bool hardcoreTriple) {
    Word maxHP = 0;
    double CSTR = a.str;

    maxHP = (int)(CSTR * 3.00);

    // The weapon family swaps the whole base formula, not just the bonus:
    // priest/soldier weapons drop the base to STR*2.
    switch (a.weapon) {
    case WeaponFamily::Sword:
    case WeaponFamily::Blade:
        maxHP += (a.weaponDomainLevel * 5);
        break;
    case WeaponFamily::Cross:
    case WeaponFamily::Mace:
        maxHP = (int)(CSTR * 2.00);
        maxHP += (a.weaponDomainLevel * 4);
        break;
    case WeaponFamily::Arms:
        maxHP = (int)(CSTR * 2.00);
        maxHP += (a.weaponDomainLevel * 2);
        break;
    default:
        break;
    }

    maxHP = percentValue(maxHP, hpRatioPercent);
    maxHP = min((int)maxHP, kSlayerMaxHP);

    if (hardcoreTriple)
        maxHP *= 3;

    return maxHP;
}

int vampireMaxHP(const StatAttr& a, int hpRatioPercent, bool hardcoreTriple) {
    Word maxHP = 0;
    double CSTR = a.str;
    double CDEX = a.dex;
    double CINT = a.inte;
    double CLEVEL = a.level;

    maxHP = (int)((CSTR * 4.00 + CINT + CDEX + CLEVEL));
    maxHP = percentValue(maxHP, hpRatioPercent);
    maxHP = min((int)maxHP, kVampireMaxHP);

    if (hardcoreTriple)
        maxHP *= 3;

    return maxHP;
}

int oustersMaxHP(const StatAttr& a, int hpRatioPercent, bool hardcoreTriple) {
    Word maxHP = 0;
    double CSTR = a.str;
    double CDEX = a.dex;
    double CINT = a.inte;
    double CLEVEL = a.level;

    maxHP = (int)(CSTR * 3.00 + CINT / 2.00 + CDEX + CLEVEL);
    maxHP = percentValue(maxHP, hpRatioPercent);
    maxHP = min((int)maxHP, kOustersMaxHP);

    if (hardcoreTriple)
        maxHP *= 3;

    return maxHP;
}

int monsterMaxHP(const StatAttr& a, int enhancePercent, int hpRatioPercent, bool hardcoreTriple) {
    Word maxHP = 0;
    double CSTR = a.str;
    double CLEVEL = a.level;

    maxHP = (int)(CSTR * (2.00 + CLEVEL / 100.0));
    maxHP += percentValue(maxHP, enhancePercent);
    maxHP = percentValue(maxHP, hpRatioPercent);
    maxHP = min((int)maxHP, kMonsterMaxHP);

    if (hardcoreTriple)
        maxHP *= 3;

    return maxHP;
}

//////////////////////////////////////////////////////////////////////////////
// Max MP
//////////////////////////////////////////////////////////////////////////////

int slayerMaxMP(const StatAttr& a) {
    Word maxMP = 0;
    double CINTE = a.inte;

    maxMP = (int)(CINTE * 2.0);
    maxMP = min((int)maxMP, kSlayerMaxMP);

    return maxMP;
}

int oustersMaxMP(const StatAttr& a) {
    Word maxMP = 0;
    double CINTE = a.inte;
    double CLEVEL = a.level;

    maxMP = (int)((CINTE + CLEVEL) * 0.7);
    maxMP = min((int)maxMP, kOustersMaxMP);

    return maxMP;
}

//////////////////////////////////////////////////////////////////////////////
// To-hit
//////////////////////////////////////////////////////////////////////////////

int slayerToHit(const StatAttr& a) {
    Word toHit = 0;
    double CDEX = a.dex;

    toHit = (int)(CDEX / 2);

    // Any recognized weapon family pays a domain-level bonus.
    switch (a.weapon) {
    case WeaponFamily::Sword:
    case WeaponFamily::Blade:
    case WeaponFamily::Cross:
    case WeaponFamily::Mace:
    case WeaponFamily::Arms:
        toHit += (int)(a.weaponDomainLevel * 1.5);
        break;
    default:
        break;
    }

    toHit = min((int)toHit, kSlayerMaxToHit);
    return toHit;
}

int vampireToHit(const StatAttr& a) {
    Word toHit = 0;
    double CDEX = a.dex;
    double CLEVEL = a.level;

    toHit = (int)(CDEX + CLEVEL / 2.5);
    toHit = min((int)toHit, kVampireMaxToHit);
    return toHit;
}

int oustersToHit(const StatAttr& a) {
    Word toHit = 0;
    double CDEX = a.dex;
    double CLEVEL = a.level;

    toHit = (int)(CDEX / 2.0 + CLEVEL);
    toHit = min((int)toHit, kOustersMaxToHit);
    return toHit;
}

int monsterToHit(const StatAttr& a, int enhancePercent) {
    Word toHit = 0;
    double CDEX = a.dex;
    double CLEVEL = a.level;

    toHit = (int)((CDEX / 2.0) * (1.0 + CLEVEL / 100.0));
    toHit += percentValue(toHit, enhancePercent);
    toHit = min((int)toHit, kMonsterMaxToHit);
    return toHit;
}

//////////////////////////////////////////////////////////////////////////////
// Defense
//////////////////////////////////////////////////////////////////////////////

int slayerDefense(const StatAttr& a) {
    Word Defense = 0;
    double CDEX = a.dex;

    Defense = (int)(CDEX / 2.0);
    Defense = min((int)Defense, kSlayerMaxDefense);
    return Defense;
}

int vampireDefense(const StatAttr& a) {
    Word Defense = 0;
    double CDEX = a.dex;
    double CLEVEL = a.level;

    Defense = (int)(CDEX / 2.0 + CLEVEL / 5.0);
    Defense = min((int)Defense, kVampireMaxDefense);
    return Defense;
}

int oustersDefense(const StatAttr& a) {
    Word Defense = 0;
    double CDEX = a.dex;
    double CLEVEL = a.level;

    Defense = (int)(CDEX / 2.0 + CLEVEL / 5.0);
    Defense = min((int)Defense, kOustersMaxDefense);
    return Defense;
}

int monsterDefense(const StatAttr& a, int enhancePercent) {
    Word Defense = 0;
    double CDEX = a.dex;
    double CLEVEL = a.level;

    Defense = (int)((CDEX / 2.0) * (1.0 + CLEVEL / 100.0));
    Defense += percentValue(Defense, enhancePercent);
    Defense = min((int)Defense, kMonsterMaxDefense);
    return Defense;
}

//////////////////////////////////////////////////////////////////////////////
// Protection
//////////////////////////////////////////////////////////////////////////////

int slayerProtection(const StatAttr& a) {
    Word Protection = 0;
    double CSTR = a.str;

    Protection = (int)(CSTR);
    Protection = min((int)Protection, kSlayerMaxProtection);
    return Protection;
}

int vampireProtection(const StatAttr& a) {
    Word Protection = 0;
    double CSTR = a.str;
    double CLEVEL = a.level;

    Protection = (int)(CSTR + CLEVEL / 5.0);
    Protection = min((int)Protection, kVampireMaxProtection);
    return Protection;
}

int oustersProtection(const StatAttr& a) {
    Word Protection = 0;
    double CSTR = a.str;
    double CLEVEL = a.level;

    Protection = (int)(CSTR + CLEVEL / 10.0);
    Protection = min((int)Protection, kOustersMaxProtection);
    return Protection;
}

int monsterProtection(const StatAttr& a, int enhancePercent) {
    Word Protection = 0;
    double CSTR = a.str;
    double CLEVEL = a.level;

    Protection = (int)(CSTR / (5.0 - CLEVEL / 100.0));
    Protection += percentValue(Protection, enhancePercent);
    Protection = min((int)Protection, kMonsterMaxProtection);
    return Protection;
}

//////////////////////////////////////////////////////////////////////////////
// Melee damage range
//////////////////////////////////////////////////////////////////////////////

int slayerMinDamage(const StatAttr& a, int combatDamageBonus) {
    Word minDamage = 0;
    double CSTR = a.str;

    minDamage = (int)(CSTR / 15.0);

    // Soldier (gun) weapons get no strength bonus at all.
    if (a.weapon == WeaponFamily::Arms)
        minDamage = 1;

    minDamage += combatDamageBonus;
    minDamage = min((int)minDamage, kSlayerMaxDamage);
    return minDamage;
}

int vampireMinDamage(const StatAttr& a, int combatDamageBonus) {
    Word minDamage = 0;
    double CSTR = a.str;
    double CLEVEL = a.level;

    minDamage = (int)(CSTR / 6.0 + CLEVEL / 5.0);
    minDamage += combatDamageBonus;
    minDamage = min((int)minDamage, kVampireMaxDamage);
    return minDamage;
}

int oustersMinDamage(const StatAttr& a) {
    Word minDamage = 0;
    double CSTR = a.str;
    double CLEVEL = a.level;

    minDamage = (int)(CSTR / 10.0 + CLEVEL / 10.0);
    minDamage = min((int)minDamage, kOustersMaxDamage);
    return minDamage;
}

int monsterMinDamage(const StatAttr& a, int enhancePercent, int damageRatioPercent) {
    Word minDamage = 0;
    double CSTR = a.str;
    double CLEVEL = a.level;

    minDamage = (int)(CSTR / (6.0 - CLEVEL / 100.0));
    minDamage += percentValue(minDamage, enhancePercent);
    minDamage = percentValue(minDamage, damageRatioPercent);
    minDamage = min((int)minDamage, kMonsterMaxDamage);
    return minDamage;
}

int slayerMaxDamage(const StatAttr& a, int combatDamageBonus) {
    Word maxDamage = 0;
    double CSTR = a.str;

    maxDamage = (int)(CSTR / 10.0);

    // Soldier (gun) weapons get no strength bonus at all.
    if (a.weapon == WeaponFamily::Arms)
        maxDamage = 2;

    maxDamage += combatDamageBonus;
    maxDamage = min((int)maxDamage, kSlayerMaxDamage);
    return maxDamage;
}

int vampireMaxDamage(const StatAttr& a, int combatDamageBonus) {
    Word maxDamage = 0;
    double CSTR = a.str;
    double CLEVEL = a.level;

    maxDamage = (int)(CSTR / 4.0 + CLEVEL / 5.0);
    maxDamage += combatDamageBonus;
    maxDamage = min((int)maxDamage, kVampireMaxDamage);
    return maxDamage;
}

int oustersMaxDamage(const StatAttr& a) {
    Word maxDamage = 0;
    double CSTR = a.str;
    double CLEVEL = a.level;

    maxDamage = (int)(CSTR / 6.0 + CLEVEL / 6.0);
    maxDamage = min((int)maxDamage, kOustersMaxDamage);
    return maxDamage;
}

int monsterMaxDamage(const StatAttr& a, int enhancePercent, int damageRatioPercent) {
    Word maxDamage = 0;
    double CSTR = a.str;
    double CLEVEL = a.level;

    maxDamage = (int)(CSTR / (4.0 - CLEVEL / 100.0));
    maxDamage += percentValue(maxDamage, enhancePercent);
    maxDamage = percentValue(maxDamage, damageRatioPercent);
    maxDamage = min((int)maxDamage, kMonsterMaxDamage);
    return maxDamage;
}

//////////////////////////////////////////////////////////////////////////////
// Attack speed
//////////////////////////////////////////////////////////////////////////////

int slayerAttackSpeed(const StatAttr& a) {
    Byte AttackSpeed = 0;
    double CSTR = a.str;
    double CDEX = a.dex;

    // Strength drives attack speed by default...
    AttackSpeed = (int)(CSTR / 10.0);

    switch (a.weapon) {
    case WeaponFamily::Sword:
    case WeaponFamily::Blade:
        AttackSpeed += (int)(a.weaponDomainLevel / 5.0);
        break;
    case WeaponFamily::Arms:
        // ...but a gun's speed comes from dexterity instead.
        AttackSpeed = (int)(CDEX / 10.0);
        AttackSpeed += (int)(a.weaponDomainLevel / 5.0);
        break;
    default:
        break;
    }

    AttackSpeed = min(kSlayerMaxAttackSpeed, AttackSpeed);
    return AttackSpeed;
}

int vampireAttackSpeed(const StatAttr& a) {
    Byte AttackSpeed = 0;
    double CDEX = a.dex;

    AttackSpeed = (int)(CDEX / 10.0 + 10.0);
    AttackSpeed = min(kVampireMaxAttackSpeed, AttackSpeed);
    return AttackSpeed;
}

int oustersAttackSpeed(const StatAttr& a) {
    Byte AttackSpeed = 0;
    double CDEX = a.dex;
    double CLEVEL = a.level;

    AttackSpeed = (int)(CDEX / 10.0 + CLEVEL / 10.0);
    AttackSpeed = min(kOustersMaxAttackSpeed, AttackSpeed);
    return AttackSpeed;
}

//////////////////////////////////////////////////////////////////////////////
// Critical ratio (uncapped, and negative DEX deltas truncate toward zero)
//////////////////////////////////////////////////////////////////////////////

int slayerCriticalRatio(const StatAttr& a) {
    int CriticalRatio = 0;

    // Only sword, blade and gun domains grant criticals; priest weapons
    // (Cross/Mace) grant none.
    switch (a.weapon) {
    case WeaponFamily::Sword:
    case WeaponFamily::Blade:
    case WeaponFamily::Arms:
        CriticalRatio = (int)(a.weaponDomainLevel / 5.0);
        break;
    default:
        break;
    }

    return CriticalRatio;
}

int vampireCriticalRatio(const StatAttr& a) {
    double CDEX = a.dex;
    return (int)((CDEX - 20.0) / 30.0);
}

int oustersCriticalRatio(const StatAttr& a) {
    double CDEX = a.dex;
    return (int)((CDEX - 20.0) / 30.0);
}

int monsterCriticalRatio(const StatAttr& a, int enhancePercent) {
    double CDEX = a.dex;
    int CriticalRatio = (int)((CDEX - 20.0) / 30.0);
    CriticalRatio += percentValue(CriticalRatio, enhancePercent);
    return CriticalRatio;
}

//////////////////////////////////////////////////////////////////////////////
// Steal ratio
//
// The ratio truncates below zero from amount 48 on the 65% base and from 65
// on the 90% base. It is converted to int before it is narrowed to a byte, so
// a negative ratio wraps (-1 is 255) on every target, as x86-64's own
// conversion does. Converting a negative double straight to a byte would be
// undefined behaviour, which arm64 resolves differently (to 0, or to the
// negative int left in the register). amount is a BYTE on the server, so the
// int conversion is always in range. Where amount * 1.4 is a whole number
// (65, 70, ... on the 90% base; 50, 55, ... on the 65% base) the result also
// depends on the multiply and the subtraction not being fused, which
// de-core's -ffp-contract=off guarantees (DECORE_FP_FLAGS in
// src/domain/CMakeLists.txt; the client's copy builds with the same flag).
//////////////////////////////////////////////////////////////////////////////

int slayerStealRatio(const StatAttr& a, int amount) {
    // Stealing nothing succeeds never.
    if (amount == 0)
        return 0;

    Byte result = 0;

    // Bare hands count as the melee (90%) base; an unrecognized weapon
    // falls to the 65% base -- the one place None and Other diverge.
    switch (a.weapon) {
    case WeaponFamily::None:
    case WeaponFamily::Sword:
    case WeaponFamily::Blade:
        result = (Byte)(int)(90.0 - (float)amount * 1.4);
        break;
    default:
        result = (Byte)(int)(65.0 - (float)amount * 1.4);
        break;
    }

    return result;
}

int vampireStealRatio(int amount) {
    if (amount == 0)
        return 0;
    return (Byte)(int)(90.0 - (float)amount * 1.4);
}

int oustersStealRatio(int amount) {
    if (amount == 0)
        return 0;
    return (Byte)(int)(90.0 - (float)amount * 1.4);
}

//////////////////////////////////////////////////////////////////////////////
// Combat / progression
//////////////////////////////////////////////////////////////////////////////

double finalDamage(int realDamage, int protection, bool critical) {
    // A critical hit ignores protection and lands the damage as-is.
    if (critical)
        return realDamage;

    if (protection > 640)
        protection = 640;

    Word FinalDamage;
    FinalDamage = realDamage - (realDamage * (protection / 8)) / 100;

    return max(1, (int)FinalDamage);
}

int tileDistance(int ox, int oy, int tx, int ty) {
    double OriginX = ox;
    double OriginY = oy;
    double TargetX = tx;
    double TargetY = ty;

    double XOffset = pow(OriginX - TargetX, 2.0);
    double YOffset = pow(OriginY - TargetY, 2.0);
    // The wire-era formula intentionally returns a byte-wide distance. Make
    // the modulo explicit so Zig's checked Debug mode preserves that behavior
    // instead of trapping when the distance exceeds 255.
    Byte range = static_cast<Byte>(std::fmod(sqrt(XOffset + YOffset), 256.0));

    return range;
}

int rankExp(int myLevel, int otherLevel, int gainPercent, int premiumPercent) {
    DWord rankExp = 0;

    if (myLevel != 0 && otherLevel != 0) {
        int checkValue = otherLevel * 100 / myLevel;

        if (checkValue > 120) {
            rankExp = (otherLevel - myLevel) * (otherLevel - myLevel) / 10 + otherLevel * 100 / myLevel;
        } else {
            rankExp = otherLevel * 100 / myLevel - 10;
        }

        rankExp = max(0, (int)rankExp);
        rankExp = percentValue(rankExp, gainPercent);
    }

    rankExp = percentValue(rankExp, premiumPercent);

    return rankExp;
}

int vampireSkillConsumeMP(int originalMP, int magicLevel, int intStat) {
    int OriginalMP = originalMP;
    int MagicLevel = magicLevel;
    int INTE = max(0, intStat - 20);
    int DecreaseAmount = 0;

    if (INTE <= MagicLevel) {
    } else if (MagicLevel < INTE && INTE <= (MagicLevel * 1.5)) {
        DecreaseAmount = percentValue(OriginalMP, 10);
    } else if ((MagicLevel * 1.5) < INTE && INTE <= (MagicLevel * 2.0)) {
        DecreaseAmount = percentValue(OriginalMP, 25);
    } else if ((MagicLevel * 2.0) < INTE && INTE <= (MagicLevel * 3.0)) {
        DecreaseAmount = percentValue(OriginalMP, 50);
    } else if ((MagicLevel * 3.0) < INTE && INTE <= (MagicLevel * 4.0)) {
        DecreaseAmount = percentValue(OriginalMP, 60);
    } else if ((MagicLevel * 4.0) < INTE && INTE <= (MagicLevel * 5.0)) {
        DecreaseAmount = percentValue(OriginalMP, 75);
    } else if ((MagicLevel * 5.0) < INTE && INTE <= (MagicLevel * 6.0)) {
        DecreaseAmount = percentValue(OriginalMP, 85);
    } else if ((MagicLevel * 6.0) < INTE && INTE <= (MagicLevel * 7.0)) {
        DecreaseAmount = percentValue(OriginalMP, 90);
    } else if ((MagicLevel * 7.0) < INTE) {
        DecreaseAmount = percentValue(OriginalMP, 95);
    }

    return (OriginalMP - DecreaseAmount);
}

//////////////////////////////////////////////////////////////////////////////
// Hit-roll success ratios, as skill/HitRoll.cpp applies them.
//////////////////////////////////////////////////////////////////////////////

int meleeHitRatio(int toHit, int defense, int toHitBonus, bool involvesMonster) {
    int Result = 0;

    if (toHit >= defense) {
        // ToHit above Defense: the chance to land is quite high. The
        // bonus/2 sits outside the (int) cast here and inside it below,
        // preserved as shipped -- but note the cast is a NO-OP on this
        // path (every operand is already int; the division truncates
        // regardless), so the placement changes nothing here.
        if (involvesMonster) {
            Result = min(95, (int)(((toHit - defense) / 3) + 50) + toHitBonus / 2);
        } else {
            Result = min(90, (int)(((toHit - defense) / 3) + 50) + toHitBonus / 2);
        }
    } else {
        // ToHit below Defense: the chance to land drops sharply.
        if (involvesMonster) {
            Result = max(5, (int)(50 - ((defense - toHit) / 3) + toHitBonus / 2));
        } else {
            Result = max(10, (int)(50 - ((defense - toHit) / 3) + toHitBonus / 2));
        }
    }

    return Result;
}

int bloodDrainHitRatio(int toHit, int defense) {
    if (toHit >= defense) {
        return min(90, (toHit - defense) / 2 + 70);
    }
    return max(10, 70 - (defense - toHit) / 2);
}

bool bloodDrainHPGate(int curHP, int maxHP, int multiplier) {
    return curHP * multiplier <= maxHP;
}

int slayerMagicRatio(int skillLevel, int intStat, int expLevel, bool selfSkill) {
    int SuccessRatio = (int)(60 - skillLevel / 3 + (int)((intStat + expLevel) / 2.5));

    // Slayer self skills succeed at least half the time.
    if (selfSkill)
        SuccessRatio = max(50, SuccessRatio);

    return SuccessRatio;
}

int vampireMagicRatio(int skillLevel, int intStat, int level, int bonusPoint) {
    int Success = (int)(45 - skillLevel / 2 + (intStat + level) / 4);

    if (bonusPoint != 0) {
        Success = percentValue(Success, 100 + bonusPoint);
    }

    return Success;
}

int oustersMagicRatio(int intStat, int level, int expLevel, bool selfSkill, int bonusPoint) {
    int Success = (int)(45 + (intStat + level) / 4 + expLevel / 3);

    if (selfSkill) {
        Success = max(Success, 60);
    }

    if (bonusPoint != 0) {
        Success = percentValue(Success, 100 + bonusPoint);
    }

    return Success;
}

int monsterMagicRatio(int skillLevel, int intStat, int level) {
    return (int)(45 - skillLevel / 2 + (intStat + level) / 4);
}

int curseRatio(int magicLevel, int resist) {
    int prob_penalty = (int)(magicLevel * 2 / 1.5 - resist);
    int curse_prob = 75 + prob_penalty;
    curse_prob = max(5, curse_prob);
    return curse_prob;
}

int vampireCurseRatio(int magicLevel, int resist) {
    int prob_penalty = (int)((int)(magicLevel / 1.5) - resist);
    int curse_prob = 75 + prob_penalty;
    curse_prob = max(5, curse_prob);
    return curse_prob;
}

int dispelRatio(int base, int skillLevel, int difficulty, int magicLevel, int minRatio) {
    int ratio = base + skillLevel - difficulty - magicLevel;
    return max(minRatio, ratio);
}

int flareRatio(int skillLevel, int targetLevel) {
    return 75 + skillLevel - targetLevel;
}

int rebukeRatio(int intStat, int skillExpLevel) {
    return 20 + (intStat / 10) + (skillExpLevel / 2);
}

int totalAttrDefenseRatio(int totalAttr) {
    return 50 + (totalAttr / 15);
}

int poisonMeshRatio(int level) {
    return 30 + (level / 5);
}

int willOfLifeRatio(int level) {
    return 50 + level / 5;
}

int backStabRatio(int intStat, int dexStat) {
    return min(50, (intStat / 5) + (dexStat / 5));
}

int hallucinationRatio(int attackerAttrSum, int targetAttrSum, int minRatio, int maxRatio) {
    int Ratio = attackerAttrSum - targetAttrSum;
    Ratio = max(minRatio, Ratio);
    Ratio = min(maxRatio, Ratio);
    return Ratio;
}

//////////////////////////////////////////////////////////////////////////////
// initAllStat bonus formulas (adapters: InitAllStat.cpp). Math transplanted
// verbatim; see Formulas.h for the per-function notes.
//////////////////////////////////////////////////////////////////////////////

int concealmentDefenseBonus(int dex, int effectLevel) {
    return (int)((dex / 20) * (1.0f + ((float)effectLevel / 25.0f)));
}

int concealmentProtectionBonus(int str, int effectLevel) {
    return (int)((str / 10) * (1.0f + ((float)effectLevel / 25.0f)));
}

int willOfIronHPBonus(int maxHP) {
    return (int)(maxHP * 0.15);
}

LivenessBonus livenessBonus(int grade, int domainLevel) {
    LivenessBonus b = {0, 0};
    switch (grade) {
    case 0: // apprentice
        b.hpPercent = 0;
        b.defenseBonus = 0;
        break;
    case 1: // adept
        b.hpPercent = 10;
        b.defenseBonus = 10;
        break;
    case 2: // expert
        b.hpPercent = 20;
        b.defenseBonus = 35;
        break;
    case 3: // master
        b.hpPercent = 30;
        b.defenseBonus = 60;
        break;
    case 4: // grand master
        b.hpPercent = 40;
        b.defenseBonus = 100;
        break;
    default:
        break;
    }

    if (domainLevel >= 125)
        b.hpPercent = 50;

    return b;
}

LivenessBonus livenessBonusChina(int grade) {
    LivenessBonus b = {0, 0};
    switch (grade) {
    case 0: // apprentice
        b.hpPercent = 0;
        b.defenseBonus = 0;
        break;
    case 1: // adept
        b.hpPercent = 10;
        b.defenseBonus = 10;
        break;
    case 2: // expert
        b.hpPercent = 25;
        b.defenseBonus = 35;
        break;
    case 3: // master
        b.hpPercent = 40;
        b.defenseBonus = 60;
        break;
    case 4: // grand master
        b.hpPercent = 100;
        b.defenseBonus = 100;
        break;
    default:
        break;
    }
    // The original china branch carried this commented-out line of
    // history where the normal table's live override sits:
    //		if ( level >= 125 ) HPBonusPercent = 50;
    return b;
}

int snipingDamageBonus(int curDamage, int str, int expLevel) {
    int damageBonusPercent = str / 20 * expLevel / 20;
    return curDamage * damageBonusPercent / 100;
}

int snipingToHitBonus(int curToHit, int dex, int expLevel) {
    int toHitBonusPercent = dex / 10 * expLevel / 20;
    return curToHit * toHitBonusPercent / 100;
}

int swordMasteryDamageBonus(int domainLevel) {
    return 3 + domainLevel / 15;
}

int concentrationToHitBonus(int domainLevel) {
    return 3 + (domainLevel / 10);
}

int evasionDefenseBonus(int domainLevel) {
    return 3 + (domainLevel - 20) / 5;
}

int shieldMasteryProtectionBonus(int domainLevel) {
    return 5 + (domainLevel - 20) / 5;
}

int wolfDamageBonus(int dex, int str) {
    return dex / 8 + str / 30;
}

int werwolfDamageBonus(int dex, int str) {
    return dex / 6 + str / 40;
}

int extremeDamageBonus(int str) {
    return min(15, 4 + ((str - 20) / 30));
}

int extremeToHitBonus(int str, int dex) {
    return min(20, 4 + ((str + dex) / 40));
}

int intimateGrailRatio(int skillLevel) {
    return 10 + (skillLevel / 10);
}

int intimateGrailHPRatio(int skillLevel) {
    return 15 + (int)(skillLevel / 6.6);
}

int gunDomainDamageBonus(int gunDomainLevel) {
    return gunDomainLevel / 10;
}

int nailMasteryDamageBonus(int level) {
    return 3 + ((level - 56) / 8);
}

int vampireDexHPRegenBonus(int dexBasic) {
    if (dexBasic > 450)
        return 7;
    else if (dexBasic > 390)
        return 6;
    else if (dexBasic > 330)
        return 5;
    else if (dexBasic > 260)
        return 4;
    else if (dexBasic > 190)
        return 3;
    else if (dexBasic > 120)
        return 2;
    else if (dexBasic > 50)
        return 1;
    return 0;
}

int fireOfSoulStonePoint(int str, int dex) {
    return (int)((str / 12.0) + (dex / 3.0));
}

int iceOfSoulStonePoint(int dex) {
    return min(5, 1 + dex / 20) * 10;
}

int sandOfSoulStonePoint(int str, int dex) {
    return (int)((str / 15.0) + (dex / 5.0));
}

int blockHeadPoint(int dex) {
    return min(4, 1 + dex / 30) * 10;
}

int blessFirePoint(int str, int dex) {
    return (int)((str / 10.0) + (dex / 2.0));
}

int sandCrossPoint(int str, int dex) {
    return (int)((str / 10.0) + (dex / 10.0));
}

int slayerBloodBibleSignOpenNum(unsigned int fame, int openNumLimit, bool healOrEnchantDomain) {
    if (healOrEnchantDomain) {
        if (fame < 100000)
            return min(openNumLimit, 1);
        else if (fame < 500000)
            return min(openNumLimit, 2);
        else if (fame < 2000000)
            return min(openNumLimit, 3);
        else if (fame < 4000000)
            return min(openNumLimit, 4);
        else if (fame < 60000000)
            return min(openNumLimit, 5);
        return min(openNumLimit, 6);
    }
    if (fame < 200000)
        return min(openNumLimit, 1);
    else if (fame < 1000000)
        return min(openNumLimit, 2);
    else if (fame < 5000000)
        return min(openNumLimit, 3);
    else if (fame < 10000000)
        return min(openNumLimit, 4);
    else if (fame < 100000000)
        return min(openNumLimit, 5);
    return min(openNumLimit, 6);
}

int vampireBloodBibleSignOpenNum(unsigned int fame, int openNumLimit) {
    if (fame < 100000)
        return min(openNumLimit, 1);
    else if (fame < 1000000)
        return min(openNumLimit, 2);
    else if (fame < 5000000)
        return min(openNumLimit, 3);
    else if (fame < 10000000)
        return min(openNumLimit, 4);
    else if (fame < 200000000)
        return min(openNumLimit, 5);
    return min(openNumLimit, 6);
}

int oustersBloodBibleSignOpenNum(unsigned int fame, int openNumLimit) {
    if (fame < 30000)
        return min(openNumLimit, 1);
    else if (fame < 500000)
        return min(openNumLimit, 2);
    else if (fame < 3000000)
        return min(openNumLimit, 3);
    else if (fame < 7000000)
        return min(openNumLimit, 4);
    else if (fame < 50000000)
        return min(openNumLimit, 5);
    return min(openNumLimit, 6);
}

int summonSylphProtectionBonus(int level) {
    return max(5, level / 10);
}

int summonSylphResistBonus(int level) {
    return max(5, level / 15);
}

int hideSightToHitBonus(int expLevel) {
    if (expLevel <= 15) {
        // m_ToHit += (int)((DEX / 20.0) * ( 1.0 + (level / 15.0) ));  (old)
        return 15 + (expLevel * 8 / 9);
    }
    // m_ToHit += (int)((DEX / 20.0) * ( 1.5 + (level / 30.0) ));  (old)
    int ToHitBonus = (35 + (expLevel * 4 / 9));
    if (expLevel == 30)
        ToHitBonus = (int)(ToHitBonus * 1.1);
    return ToHitBonus;
}

} // namespace decore
