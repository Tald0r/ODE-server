//////////////////////////////////////////////////////////////////////////////
// Filename    : SkillOutputFormulas.cpp
// Description : see SkillOutputFormulas.h. Bodies are verbatim from
// skill/SkillFormula.cpp except the three substitutions the header
// documents; the legacy comments were later recovered from their
// double-encoded mojibake and translated to English (see the header).
//////////////////////////////////////////////////////////////////////////////

#include "domain/SkillOutputFormulas.h"

#include <algorithm>

#include "domain/Formulas.h"

using std::max;
using std::min;

namespace decore {
namespace skillformula {

namespace {

// Same name the bodies call in the gameserver (src/Core/Utility.h there);
// delegates to the de-core duplicate so this file stays freestanding.
inline int getPercentValue(int value, int percent) {
    return percentValue(value, percent);
}

// Attr_t is WORD in the game (src/Core/types/CreatureTypes.h). Two bodies
// (MagicElusion, IllusionOfAvenge) funnel the stat sum through it, so the
// 16-bit truncation is part of the shipped math -- duplicated here to keep
// this file freestanding, pinned by formula_test.cpp.
typedef unsigned short Attr_t;

const int PartyEffectBoost[7] = {
    100, // 0
    100, // 1
    120, // 2
    135, // 3
    140, // 4
    145, // 5
    150  // 6
};

const int PartyDurationBoost[7] = {
    100, // 0
    100, // 1
    130, // 2
    155, // 3
    175, // 4
    190, // 5
    200  // 6
};

// The index both tables are read at: the party size, clamped to 0..6 so
// that no size can read outside them.
int partyTableIndex(int partySize) {
    return min(max(partySize, 0), 6);
}

} // namespace

int partyEffectBoost(int partySize) {
    return PartyEffectBoost[partyTableIndex(partySize)];
}

int partyDurationBoost(int partySize) {
    return PartyDurationBoost[partyTableIndex(partySize)];
}

//////////////////////////////////////////////////////////////////////////////
// Sword family
//////////////////////////////////////////////////////////////////////////////

void DoubleImpact(const SkillInput& input, SkillOutput& output) {
    output.Damage = 1 + (input.STR / 20) + input.SkillLevel / 20;
    output.Delay = 8; // 1 sec
                      // output.Damage = 2 + input.SkillLevel/33;
    // output.Delay  = 10; // 1 sec
}

void TripleSlasher(const SkillInput& input, SkillOutput& output) {
    //	output.Damage = 2 + (input.STR/20) + (input.SkillLevel/15);
    output.Damage = 3 + (input.STR / 20) + (input.SkillLevel / 15);
    output.Delay = 8; // 0.7 sec
                      // output.Damage = 3 + input.SkillLevel/33;
    // output.Delay  = 7; // 0.7 sec
}

void RainbowSlasher(const SkillInput& input, SkillOutput& output) {
    output.Damage = 4 + input.STR / 15 + input.SkillLevel / 10;
    output.Delay = 8; // 0.8 sec
                      // output.Damage = 7 + input.SkillLevel/33;
    // output.Delay  = 8; // 0.8 sec
}

void ThunderSpark(const SkillInput& input, SkillOutput& output) {
    output.Damage = 3 + input.STR / 20 + input.SkillLevel / 10;
    output.Delay = 7; // 0.7 sec
                      // output.Damage = 5 + input.SkillLevel/33;
    // output.Delay  = 7; // 0.7 sec
}

void DancingSword(const SkillInput& input, SkillOutput& output) {
    output.Damage = 1 + (input.DEX / 10) + (input.SkillLevel / 10);
    //	output.Duration = (10 + input.STR/10 + input.SkillLevel/2) * 10;
    output.Duration = (30 + input.STR / 10 + input.SkillLevel / 2) * 10;
    // output.Delay    = (5 - input.SkillLevel/33) * 10; // 5->2 sec
    output.Delay = output.Duration; // Delay and Duration are the same. by bezz 2003.3.5

    // output.Damage   = 5 + input.SkillLevel/10;
    // output.Duration = (30 + input.SkillLevel/4) * 10;
    // output.Delay    = (5 - input.SkillLevel/33) * 10; // 5->2 sec
}

void CrossCounter(const SkillInput& input, SkillOutput& output) {
    output.Duration = (10 + input.SkillLevel / 2) * 10;
    // output.Delay    = max(5 - input.SkillLevel/20, 2) * 10; // 5->2 sec
    output.Delay = output.Duration; // Delay and Duration are the same. by bezz 2003.3.5

    // output.Duration = (30 + input.SkillLevel/4) * 10;
    // output.Delay    = max(5 - input.SkillLevel/20, 2) * 10; // 5->2 sec
}

void FlashSliding(const SkillInput& input, SkillOutput& output) {
    output.Delay = max(3 - input.SkillLevel / 50, 1) * 10; // 3->1 sec
    output.Duration = 3 - input.SkillLevel / 50;
    // output.Delay = (3 - input.SkillLevel/50) * 10; // 3->1 sec
}

void LightningHand(const SkillInput& input, SkillOutput& output) {
    output.Damage = input.STR / 10 + input.SkillLevel / 10;
    // Because of client synchronization issues the delay must be absent or slightly shorter.
    output.Delay = 4; // 0.2 sec
                      // output.Damage = 7 + input.SkillLevel/20;
    // output.Delay  = 2; // 0.2 sec
}

void SwordWave(const SkillInput& input, SkillOutput& output) {
    //	output.Damage = 5 + input.SkillLevel/33;
    output.Damage = 15 + input.SkillLevel / 20;
    output.Delay = 4; // 1 sec
                      // output.Damage = 5 + input.SkillLevel/33;
    // output.Delay  = 10; // 1 sec
}

void SnakeCombo(const SkillInput& input, SkillOutput& output) {
    // by sigi. 2002.12.3
    output.Damage = 5 + input.STR / 12 + input.SkillLevel / 8;
    output.Delay = 10; // 1 sec
                       // output.Damage = 7 + input.SkillLevel/10;
    // output.Delay  = 10; // 1 sec
}

void WindDivider(const SkillInput& input, SkillOutput& output) {
    output.Damage = input.STR / 14 + input.SkillLevel / 20;
    output.Delay = 13; // 1 sec
}

void ThunderBolt(const SkillInput& input, SkillOutput& output) {
    output.Damage = input.STR / 10 + input.SkillLevel / 10; // by bezz. 2002.12.10
    output.Delay = (3 - input.SkillLevel / 50) * 10;        // by sigi. 2002.12.3
}

void Expansion(const SkillInput& input, SkillOutput& output) {
    output.Damage = 10 + input.SkillLevel / 2; // the caster's + value
    output.ToHit = 5 + input.SkillLevel / 3;   // the party's + value
                                               //	output.Duration = (30 + input.STR/10 + input.SkillLevel*2/3) * 10;
    output.Duration = (45 + input.STR / 10 + input.SkillLevel) * 10;
    // output.Delay    = max(5 - input.SkillLevel/33,2) * 10;
    output.Delay = output.Duration; // Delay and Duration are the same. by bezz 2003.3.5

    // output.Damage   = 30 + input.SkillLevel/2; // the caster's + value
    // output.ToHit    = 15 + input.SkillLevel/4; // the party's + value
    // output.Duration = (30 + input.SkillLevel/3) * 10;
    // output.Delay    = (5 - input.SkillLevel/33) * 10;
}

void MiracleShield(const SkillInput& input, SkillOutput& output) {
    output.Damage = (5 + input.SkillLevel / 5);         // 5->25
    output.Duration = (30 + input.SkillLevel / 5) * 10; // 30~50 sec
    output.Delay = (5 - input.SkillLevel / 33) * 10;    // 5->2 sec
}

void ThunderFlash(const SkillInput& input, SkillOutput& output) {
    output.Damage = 5 + input.SkillLevel / 10;
    output.Delay = 20; // 2 sec
}

void ThunderStorm(const SkillInput& input, SkillOutput& output) {
    output.Damage = input.STR / 10 + input.SkillLevel / 10; // by bezz. 2002.12.10
    output.Delay = (3 - input.SkillLevel / 50) * 10;        // 1 sec
                                                            // output.Damage = 10 + input.SkillLevel/5;
    // output.Delay  = (5 - input.SkillLevel/33)*10; // 1 sec
}

void MentalSword(const SkillInput& input, SkillOutput& output) {
    output.Damage = 30 + (15 * input.SkillLevel / 50);
    output.Range = 2 + input.SkillLevel / 33;
    output.Delay = (8 - input.SkillLevel / 20) * 10; // 5~3 sec
                                                     // output.Damage = 30 + ( 15 * input.SkillLev/50 );
    // output.Range = 2 + input.SkillLevel/20;
    // output.Delay  = (5 - input.SkillLevel / 50)*10; // 5~3 sec
}

//////////////////////////////////////////////////////////////////////////////
// Blade family
//////////////////////////////////////////////////////////////////////////////

void SingleBlow(const SkillInput& input, SkillOutput& output) {
    output.Damage = 1 + input.STR / 20 + input.SkillLevel / 20; // by sigi. 2002.12.3
    output.Delay = 7;                                           // 1 sec
}

void SpiralSlay(const SkillInput& input, SkillOutput& output) {
    //	output.Damage = 2 + input.STR/20 + input.SkillLevel/15;	// by sigi. 2002.12.3
    output.Damage = 4 + input.STR / 20 + input.SkillLevel / 15;
    output.Delay = 7; // 0.7 sec
}

void TripleBreak(const SkillInput& input, SkillOutput& output) {
    output.Damage = 3 + input.STR / 20 + input.SkillLevel / 10; // by sigi. 2002.12.3
    output.Delay = 7;                                           // 0.7 sec
}

void WildSmash(const SkillInput& input, SkillOutput& output) {
    output.Damage = 4 + input.STR / 15 + input.SkillLevel / 10; // by sigi. 2002.12.3
    output.Delay = 8;                                           // 0.8 sec
}

void GhostBlade(const SkillInput& input, SkillOutput& output) {
    output.Damage = 5 + input.SkillLevel / 10;
    // output.Duration = (30 + input.SkillLevel/4) * 10;
    // output.Duration = (10 + input.STR/10 + input.SkillLevel/2) * 10;
    output.Duration = (30 + input.STR / 10 + input.SkillLevel / 2) * 10;
    // output.Delay    = (5 - input.SkillLevel/33) * 10; // 5->2 sec
    output.Delay = output.Duration; // Delay and Duration are the same. by bezz 2003.3.5
}

void PotentialExplosion(const SkillInput& input, SkillOutput& output) {
    output.Damage = 1 + input.SkillLevel / 15;
    //	output.Duration = (10 + input.SkillLevel/2) * 10;
    output.Duration = (30 + input.SkillLevel / 2) * 10;
    // output.Delay    = (7 - input.SkillLevel/20) * 10; // 7->2 sec
    output.Delay = output.Duration; // Delay and Duration are the same. by bezz 2003.3.5
}

void ShadowWalk(const SkillInput& input, SkillOutput& output) {
    output.Range = max(2 + input.SkillLevel / 25, 2);
    output.Delay = (3 - input.SkillLevel / 50) * 10; // 3->1 sec
}

void ChargingPower(const SkillInput& input, SkillOutput& output) {
    output.Damage = 5 + (input.SkillLevel / 20) + (input.DomainLevel / 30);
    output.Duration = (60 + input.SkillLevel * 3 / 2) * 10;
    // output.Delay    = (5 - input.SkillLevel/33) * 10; // 5->2 sec
    output.Delay = output.Duration; // Delay and Duration are the same. by bezz 2003.3.5
}

void HurricaneCombo(const SkillInput& input, SkillOutput& output) {
    output.Damage = 5 + input.STR / 12 + input.SkillLevel / 8;
    output.Delay = 10; // 1 sec
}

void TornadoSever(const SkillInput& input, SkillOutput& output) {
    //	output.Damage = 8 + input.STR/15 + input.SkillLevel/10;
    //	output.Damage = 3 + input.STR/20 + input.SkillLevel/10;
    output.Damage = 10 + input.STR / 20 + input.SkillLevel / 8;
    //	output.Delay  = 20 - (input.SkillLevel/100); // 2 sec
    output.Delay = 4;
}

void Earthquake(const SkillInput& input, SkillOutput& output) {
    output.Damage = 5 + input.STR / 20 + input.SkillLevel / 10; // by sigi. 2002.12.3
    output.Range = 7;                                           // by bezz. 2002.12.10
    output.Delay = 13;                                          // 1.3 sec by bezz. 2002.12.10
}

void Berserker(const SkillInput& input, SkillOutput& output) {
    // the penalty value lives in Berserker.cpp.
    // output.Damage    = (input.STR/20) * (1 + input.SkillLevel/20); //  damage bonus
    // output.ToHit    = (input.DEX/10) * (1 + input.SkillLevel/10); //  to-hit bonus

    // 2002.12.06 Jang Hong-chang
    output.Damage = (input.STR / 20) * (1 + input.SkillLevel / 25); //  damage bonus
    //	output.ToHit    = (input.DEX/10) * (1 + input.SkillLevel/12); //  to-hit bonus
    output.ToHit = 10 + (1 + input.SkillLevel / 12); //  to-hit bonus

    //	output.Duration = (30 + input.SkillLevel/5) * 10;
    output.Duration = (int)((45 + input.SkillLevel / 1.5) * 10);
    // output.Delay    = max(5 - input.SkillLevel/33,2) * 10;
    output.Delay = output.Duration; // Delay and Duration are the same. by bezz 2003.3.5
}

void MoonlightSever(const SkillInput& input, SkillOutput& output) {
    //	output.Damage   = 8 + input.STR/10 + input.SkillLevel/10;
    output.Damage = 15 + input.STR / 10 + input.SkillLevel / 8;
    //	output.Delay    = 8; // 0.8 sec
    output.Delay = 4; // 0.4 sec
}

void ShadowDancing(const SkillInput& input, SkillOutput& output) {
    //	output.Damage = 30 + ( 15 * input.SkillLevel/50 );
    output.Damage = 30 + (15 * input.SkillLevel / 25);
    output.ToHit = 10 + input.SkillLevel / 20; // critical-chance increase
    // output.Delay    = (5 - input.SkillLevel / 50)*10; // 2 sec divided in three = 0.66666 = 0.7 sec
    output.Delay = (8 - input.SkillLevel / 20) * 10; // by sigi. 2002.12.3
}

void Typhoon(const SkillInput& input, SkillOutput& output) {
    output.Damage = input.STR / 10 + input.SkillLevel / 5;
    // output.Duration = 10; // 1 sec (the time the target spins around) - there is no effect -_-;
    // output.Delay    = 15; // 1.5 sec
    output.Duration =
        (1 + input.SkillLevel / 100) * 10; // 1 sec (the time the target spins around) - there is no effect -_-;
    output.Delay = (3 - input.SkillLevel / 50) * 10; // by bezz. 2002.12.10
}

//////////////////////////////////////////////////////////////////////////////
// Gun family
//////////////////////////////////////////////////////////////////////////////

void QuickFire(const SkillInput& input, SkillOutput& output) {
    // output.ToHit  = -20 + (input.DEX/10) + input.SkillLevel/5;
    // output.Damage = -50 + input.STR/10 + input.SkillLevel/3;
    output.ToHit = -30 + input.DEX / 20 + input.SkillLevel / 5;
    output.Damage = -30 + input.STR / 20 + input.SkillLevel / 5;
    output.Delay = 2;
}

void DoubleShot(const SkillInput& input, SkillOutput& output) {
    output.ToHit = -20 + input.SkillLevel / 5;
    output.Damage = -50 + input.SkillLevel / 3;
    output.Delay = 2;
}

void TripleShot(const SkillInput& input, SkillOutput& output) {
    output.ToHit = -20 + input.SkillLevel / 5;
    output.Damage = -50 + input.SkillLevel / 3;
    output.Delay = 2;
}

void MultiShot(const SkillInput& input, SkillOutput& output) {
    if (input.Gun == GunClass::SG) {
        //		output.Damage = 3 + input.SkillLevel/10;
        output.Damage = 8 + input.SkillLevel / 10;
    }

    else if (input.Gun == GunClass::AR || input.Gun == GunClass::SMG) {
        //		output.Damage = 1 + input.SkillLevel/30;
        output.Damage = 5 + input.SkillLevel / 15;
    }

    else if (input.Gun == GunClass::SR) {
        //		output.Damage = input.SkillLevel/50;
        output.Damage = 3 + input.SkillLevel / 20;
    }

    output.ToHit = -20 + input.SkillLevel / 5;
    output.Delay = 8; // 0.8 sec
}

void HeadShot(const SkillInput& input, SkillOutput& output) {
    output.Damage = 0;

    // computed in SkillUtil.cpp's computeArmsWeaponDamageBonus() already.. so this is duplicate damage. - -; by sigi. 2002.12.3
    // Each case falls through to the next, so ranges 1 to 3 all end with
    // case 1's damage. That is the shipped balance, kept on purpose.
    if (input.Gun == GunClass::SG) {
        switch (input.Range) {
        case 3:
            output.Damage = 5;
            [[fallthrough]];
        case 2:
            output.Damage = 8;
            [[fallthrough]];
        case 1:
            output.Damage = 10;
            [[fallthrough]];
        default:
            break;
        }
    } else if (input.Gun == GunClass::AR || input.Gun == GunClass::SMG) {
        switch (input.Range) {
        case 3:
            output.Damage = 5;
            [[fallthrough]];
        case 2:
            output.Damage = 6;
            [[fallthrough]];
        case 1:
            output.Damage = 8;
            [[fallthrough]];
        default:
            break;
        }
    } else if (input.Gun == GunClass::SR) {
        switch (input.Range) {
        case 3:
            output.Damage = 3;
            [[fallthrough]];
        case 2:
            output.Damage = 6;
            [[fallthrough]];
        case 1:
            output.Damage = 8;
            [[fallthrough]];
        default:
            break;
        }
    } else {
        // unknown gun class: the adapter Asserts before delegating
    }

    output.Delay = 8; // 0.8 sec
}

void Piercing(const SkillInput& input, SkillOutput& output) {
    (void)input;
    output.ToHit = 0;
    output.Damage = 2;
    output.Delay = 8; // 0.8 sec
}

void Sniping(const SkillInput& input, SkillOutput& output) {
    output.Duration = (30 + input.SkillLevel / 5) * 10;
    output.Delay = (10 - input.SkillLevel / 20) * 10; // 10->5 sec
}

void MindControl(const SkillInput& input, SkillOutput& output) {
    output.Damage = 15 + input.SkillLevel / 10;
    output.Duration = (30 + input.SkillLevel / 5) * 10;
    output.Delay = 30; // 3 sec
}

void Revealer(const SkillInput& input, SkillOutput& output) {
    output.Duration = (30 + input.SkillLevel / 5) * 10; // 30~50 sec
    output.Delay = output.Duration;
    //	output.Delay    = (5 - input.SkillLevel/33) * 10; // 5->2 sec

    // Compute the party bonus.
    // output.Damage   = getPercentValue(output.Damage, PartyEffectBoost[input.PartySize]);
    output.Duration = getPercentValue(output.Duration, partyDurationBoost(input.PartySize));
}

void CreateBomb(const SkillInput& input, SkillOutput& output) {
    output.Damage = 15 + input.SkillLevel / 10;
    output.Duration = (30 + input.SkillLevel / 5) * 10;
    output.Delay = 30 - input.SkillLevel / 10; // 3 sec ~ 2 sec
}

void CreateMine(const SkillInput& input, SkillOutput& output) {
    output.Damage = 15 + input.SkillLevel / 10;
    output.Duration = (30 + input.SkillLevel / 5) * 10;
    output.Delay = 30 - input.SkillLevel / 10; // 3 sec ~ 2 sec
}

void InstallMine(const SkillInput& input, SkillOutput& output) {
    output.Damage = 15 + input.SkillLevel / 10;
    output.Duration = (30 + input.SkillLevel / 5) * 10;
    //	output.Delay    = 30 - input.SkillLevel / 10; // 3 sec ~ 2 sec
    output.Delay = (int)(90 - input.SkillLevel / 2.5); // 9 sec ~ 5 sec
}

void DisarmMine(const SkillInput& input, SkillOutput& output) {
    output.Damage = 15 + input.SkillLevel / 10;
    output.Duration = (30 + input.SkillLevel / 5) * 10;
    output.Delay = 30; // 3 sec
}

void ObservingEye(const SkillInput& input, SkillOutput& output) {
    //	output.Duration = (10 + input.SkillLevel/5)*10;
    output.Duration = (30 + input.SkillLevel / 3) * 10;
    // the Damage value carries the vision, and
    // computes some other value as a rough stand-in. -_-;
    output.Damage = 1 + input.SkillLevel / 50;
    // output.Delay  = (5 - input.DEX/50) * 10; // 20 sec
    output.Delay = output.Duration; // Delay and Duration are the same. by bezz 2003.3.5
}

//////////////////////////////////////////////////////////////////////////////
// Enchant family
//////////////////////////////////////////////////////////////////////////////

void CreateHolyWater(const SkillInput& input, SkillOutput& output) {
    output.Delay = (5 - input.SkillLevel / 33) * 10; // 5->2 sec
}

void Light(const SkillInput& input, SkillOutput& output) {
    output.Duration = (60 + input.SkillLevel / 20 * 60) * 10; // 60~360 sec
    output.Delay = (5 - input.SkillLevel / 33) * 10;          // 5->2 sec

    // Compute the party bonus.
    // output.Damage   = getPercentValue(output.Damage, PartyEffectBoost[input.PartySize]);
    output.Duration = getPercentValue(output.Duration, partyDurationBoost(input.PartySize));
}

void DetectHidden(const SkillInput& input, SkillOutput& output) {
    output.Duration = (10 + input.SkillLevel / 2) * 10; // 30~50 sec
    output.Delay = (5 - input.INTE / 50) * 10;          // 5->2 sec

    // Compute the party bonus.
    // output.Damage   = getPercentValue(output.Damage, PartyEffectBoost[input.PartySize]);
    output.Duration = getPercentValue(output.Duration, partyDurationBoost(input.PartySize));
}

void AuraBall(const SkillInput& input, SkillOutput& output) {
    //	output.Damage = 5 + (input.INTE/20) + input.SkillLevel/5;
    //	output.Damage = 10 + (input.INTE/10) + input.SkillLevel/5;
    output.Damage = 16 + (input.INTE / 10) + input.SkillLevel / 4;
    output.Delay = 10; // 1 sec
    //	output.Range = 2 + (input.SkillLevel/25); // an unused value.. why was it ever put here? 8/18

    // Attack-type skills have no party bonus.
}

void Bless(const SkillInput& input, SkillOutput& output) {
    if (input.TargetType == SkillInput::TARGET_SELF) {
        //		output.Damage   = input.INTE/20 + input.SkillLevel/20;
        output.Damage = 4 + input.INTE / 40 + input.SkillLevel / 20;
    } else {
        //		output.Damage   = input.INTE/20 + input.SkillLevel/20;
        output.Damage = 2 + input.INTE / 40 + input.SkillLevel / 20;
    }

    output.Duration = (30 + input.SkillLevel * 3 / 2) * 10; // 30~50 sec
    output.Delay = (7 - input.SkillLevel / 20) * 10;        // 6->3 sec

    // Compute the party bonus.
    output.Damage = getPercentValue(output.Damage, partyEffectBoost(input.PartySize));
    output.Duration = getPercentValue(output.Duration, partyDurationBoost(input.PartySize));
}

void ContinualLight(const SkillInput& input, SkillOutput& output) {
    switch (input.DomainGrade) {
    case SKILL_GRADE_APPRENTICE:
        output.Range = 1;
        break;
    case SKILL_GRADE_ADEPT:
        output.Range = 3;
        break;
    case SKILL_GRADE_EXPERT:
        output.Range = 4;
        break;
    case SKILL_GRADE_MASTER:
        output.Range = 5;
        break;
    case SKILL_GRADE_GRAND_MASTER:
        output.Range = 6;
        break;
    default:
        output.Range = 0;
        break;
    }

    output.Delay = (6 - input.SkillLevel / 25) * 10; // 5->2 sec
    output.Duration = (10 + input.SkillLevel / 2) * 10;
}

void Flare(const SkillInput& input, SkillOutput& output) {
    output.Duration = (5 + input.SkillLevel / 5) * 10; // 10~20 sec
    output.Delay = (6 - input.SkillLevel / 33) * 10;   // 6->3 sec

    // Compute the party bonus.
    // output.Damage   = getPercentValue(output.Damage, PartyEffectBoost[input.PartySize]);
    output.Duration = getPercentValue(output.Duration, partyDurationBoost(input.PartySize));
}

void Purify(const SkillInput& input, SkillOutput& output) {
    output.Damage = 10 + input.SkillLevel / 10;
    output.Delay = (5 - input.SkillLevel / 33) * 10; // 5->2 sec

    switch (input.DomainGrade) {
    case SKILL_GRADE_APPRENTICE:
        output.Range = 3;
        break;
    case SKILL_GRADE_ADEPT:
        output.Range = 3;
        break;
    case SKILL_GRADE_EXPERT:
        output.Range = 5;
        break;
    case SKILL_GRADE_MASTER:
        output.Range = 5;
        break;
    case SKILL_GRADE_GRAND_MASTER:
        output.Range = 7;
        break;
    default:
        output.Range = 0;
        break;
    }

    // Attack-type skills have no party bonus.
}

void AuraRing(const SkillInput& input, SkillOutput& output) {
    //	output.Damage = 5 + input.INTE/10 + input.SkillLevel/3; // by bezz. 2002.12.10
    output.Damage = 15 + input.INTE / 10 + input.SkillLevel / 3; // by bezz. 2002.12.10
    output.Delay = max(1, 2 - (input.SkillLevel / 50)) * 10;     // by bezz. 2002.12.10

    // Attack-type skills have no party bonus.
}

void Striking(const SkillInput& input, SkillOutput& output) {
    if (input.TargetType == SkillInput::TARGET_OTHER) {
        //		output.Damage = input.INTE/33 + input.SkillLevel/20;
        output.Damage = input.INTE / 30 + input.SkillLevel / 20;
    } else {
        //		output.Damage = input.INTE/20 + input.SkillLevel/10;
        output.Damage = input.INTE / 30 + input.SkillLevel / 10;
    }

    output.Duration = (30 + input.SkillLevel * 3 / 2) * 10;
    //	output.Duration = (30 + input.SkillLevel * 2 / 3) * 10;
    output.Delay = (6 - input.SkillLevel / 33) * 10; // 6->3 sec

    // Compute the party bonus.
    output.Duration = getPercentValue(output.Duration, partyDurationBoost(input.PartySize));
}

void DetectInvisibility(const SkillInput& input, SkillOutput& output) {
    output.Duration = (10 + input.SkillLevel / 2) * 10; // 30~50 sec
    output.Delay = (6 - input.SkillLevel / 33) * 10;    // 6->3 sec

    switch (input.DomainGrade) {
    case SKILL_GRADE_APPRENTICE:
        output.Range = 3;
        break;
    case SKILL_GRADE_ADEPT:
        output.Range = 3;
        break;
    case SKILL_GRADE_EXPERT:
        output.Range = 5;
        break;
    case SKILL_GRADE_MASTER:
        output.Range = 5;
        break;
    case SKILL_GRADE_GRAND_MASTER:
        output.Range = 7;
        break;
    default:
        output.Range = 0;
        break;
    }

    // Compute the party bonus.
    output.Duration = getPercentValue(output.Duration, partyDurationBoost(input.PartySize));
}

void AuraShield(const SkillInput& input, SkillOutput& output) {
    output.Duration = (10 + input.SkillLevel / 3) * 10; // 15~35 sec
    // output.Delay    = (5 - input.SkillLevel/33) * 10; // 5->2 sec

    // Compute the party bonus.
    // output.Damage   = getPercentValue(output.Damage, PartyEffectBoost[input.PartySize]);
    output.Duration = getPercentValue(output.Duration, partyDurationBoost(input.PartySize));
    output.Delay = output.Duration; // Delay and Duration are the same. by bezz 2003.3.5
}

void Visible(const SkillInput& input, SkillOutput& output) {
    output.Delay = (5 - input.SkillLevel / 50) * 10; // 5->3 sec
    output.Range = 3 + (input.SkillLevel / 50);

    // Compute the party bonus.
    // output.Damage   = getPercentValue(output.Damage, PartyEffectBoost[input.PartySize]);
    // output.Duration = getPercentValue(output.Duration, PartyDurationBoost[input.PartySize]);
}

//////////////////////////////////////////////////////////////////////////////
// Healing family
//////////////////////////////////////////////////////////////////////////////

void CureLightWounds(const SkillInput& input, SkillOutput& output) {
    if (input.TargetType == SkillInput::TARGET_OTHER) {
        output.Damage = 10 + input.SkillLevel / 8;
    } else {
        output.Damage = 10 + input.SkillLevel / 4;
    }

    output.Delay = 10; // 1 sec

    // Compute the party bonus.
    output.Damage = getPercentValue(output.Damage, partyEffectBoost(input.PartySize));
    // output.Duration = getPercentValue(output.Duration, PartyDurationBoost[input.PartySize]);
}

void CureAll(const SkillInput& input, SkillOutput& output) {
    if (input.TargetType == SkillInput::TARGET_OTHER) {
        output.Damage = 50 + input.SkillLevel / 4;
    } else {
        output.Damage = 35 + input.SkillLevel / 2;
    }

    output.Delay = 40 - input.SkillLevel / 5; // 4 sec~ 2 sec

    // Compute the party bonus.
    output.Damage = getPercentValue(output.Damage, partyEffectBoost(input.PartySize));
    // output.Duration = getPercentValue(output.Duration, PartyDurationBoost[input.PartySize]);
}

void CurePoison(const SkillInput& input, SkillOutput& output) {
    // Compute the party bonus.
    // output.Damage   = getPercentValue(output.Damage, PartyEffectBoost[input.PartySize]);
    // output.Duration = getPercentValue(output.Duration, PartyDurationBoost[input.PartySize]);

    output.Delay = (4 - input.SkillLevel / 33) * 10; // 4->1 sec
}

void ProtectionFromPoison(const SkillInput& input, SkillOutput& output) {
    if (input.TargetType == SkillInput::TARGET_OTHER) {
        // output.Damage = 10 + input.SkillLevel/10;
        output.Damage = min(20, 10 + input.INTE / 20);
    } else {
        // output.Damage = 20 + input.SkillLevel/10;
        output.Damage = min(30, 20 + input.INTE / 20);
    }

    // output.Duration = (30 + input.SkillLevel/2) * 10; // 30~80 sec
    //	output.Duration = (50 + input.INTE * 2 / 3) * 10; // 30~80 sec// by sigi. 2002.12.3
    output.Duration = (60 + input.INTE / 2 + input.SkillLevel / 2) * 10; //
    output.Delay = (5 - input.SkillLevel / 33) * 10;                     // 5->2 sec

    // Compute the party bonus.
    output.Damage = getPercentValue(output.Damage, partyEffectBoost(input.PartySize));
    output.Duration = getPercentValue(output.Duration, partyDurationBoost(input.PartySize));
}

void CauseLightWounds(const SkillInput& input, SkillOutput& output) {
    // output.Damage = 10 + input.SkillLevel/10;
    // output.Damage = 5 + input.INTE/10 + input.SkillLevel/10;	// by sigi. 2002.12.3
    output.Damage = 10 + input.INTE / 10 + input.SkillLevel / 10; // by sigi. 2002.12.3
    output.Delay = 10;                                            // 1 sec

    // Attack-type skills have no party bonus.
}

void CureSeriousWounds(const SkillInput& input, SkillOutput& output) {
    if (input.TargetType == SkillInput::TARGET_OTHER) {
        output.Damage = 30 + input.SkillLevel / 8;
    } else {
        output.Damage = 30 + input.SkillLevel / 4;
    }

    output.Delay = 10; // 1 sec

    // Compute the party bonus.
    output.Damage = getPercentValue(output.Damage, partyEffectBoost(input.PartySize));
    // output.Duration = getPercentValue(output.Duration, PartyDurationBoost[input.PartySize]);
}

void RemoveCurse(const SkillInput& input, SkillOutput& output) {
    output.Delay = (5 - input.SkillLevel / 33) * 10; // 5->2 sec

    // Compute the party bonus.
    // output.Damage   = getPercentValue(output.Damage, PartyEffectBoost[input.PartySize]);
    // output.Duration = getPercentValue(output.Duration, PartyDurationBoost[input.PartySize]);
}

void ProtectionFromCurse(const SkillInput& input, SkillOutput& output) {
    // by sigi. 2002.12.3
    if (input.TargetType == SkillInput::TARGET_OTHER) {
        // output.Damage = 10 + input.SkillLevel/10;
        output.Damage = min(20, 10 + input.INTE / 20);
    } else {
        // output.Damage = 20 + input.SkillLevel/10;
        output.Damage = max(30, 20 + input.INTE / 20);
    }

    // output.Duration = (25 + input.SkillLevel/2) * 10; // 25~75 sec
    // output.Duration = (40 + input.INTE*2/3) * 10; // 25~75 sec	// by sigi. 2002.12.3
    output.Duration = (60 + input.SkillLevel / 2 + input.INTE / 2) * 10; // 25~75 sec	// by sigi. 2002.12.3
    output.Delay = (5 - input.SkillLevel / 33) * 10;                     // 5->2 sec

    // Compute the party bonus.
    output.Damage = getPercentValue(output.Damage, partyEffectBoost(input.PartySize));
    output.Duration = getPercentValue(output.Duration, partyDurationBoost(input.PartySize));
}

void Resurrect(const SkillInput& input, SkillOutput& output) {
    (void)input;
    output.Delay = 20; // 2 sec
}

void CauseSeriousWounds(const SkillInput& input, SkillOutput& output) {
    // output.Damage = 30 + input.SkillLevel/4;
    //  by sigi. 2002.12.3
    // output.Damage = 5 + input.INTE/8 + input.SkillLevel/5;
    output.Damage = 15 + input.INTE / 8 + input.SkillLevel / 5;
    output.Delay = 10; // 1 sec

    // Attack-type skills have no party bonus.
}

void CureCriticalWounds(const SkillInput& input, SkillOutput& output) {
    if (input.TargetType == SkillInput::TARGET_OTHER) {
        output.Damage = 10 + input.SkillLevel / 10;
    } else {
        output.Damage = 10 + input.SkillLevel / 20;
    }

    output.Delay = (40 - input.SkillLevel / 5) * 10; // 40 sec~20 sec

    output.Duration = (20 + input.SkillLevel / 5) * 10; // 20 sec~40 sec

    // Compute the party bonus.
    output.Damage = getPercentValue(output.Damage, partyEffectBoost(input.PartySize));
    // output.Duration = getPercentValue(output.Duration, PartyDurationBoost[input.PartySize]);
}

void ProtectionFromAcid(const SkillInput& input, SkillOutput& output) {
    // by sigi. 2002.12.3
    if (input.TargetType == SkillInput::TARGET_OTHER) {
        // output.Damage = 10 + input.SkillLevel/10;
        output.Damage = min(20, 10 + input.INTE / 20);
    } else {
        // output.Damage = 20 + input.SkillLevel/10;
        output.Damage = min(30, 20 + input.INTE / 20);
    }

    // output.Duration = (20 + input.SkillLevel/2) * 10; // 20~70 sec
    // output.Duration = (40 + input.INTE*2/3) * 10; // 20~70 sec
    output.Duration = (60 + input.SkillLevel / 2 + input.INTE / 2) * 10; // 20~70 sec
    output.Delay = (5 - input.SkillLevel / 33) * 10;                     // 5->2 sec

    // Compute the party bonus.
    output.Damage = getPercentValue(output.Damage, partyEffectBoost(input.PartySize));
    output.Duration = getPercentValue(output.Duration, partyDurationBoost(input.PartySize));
}

void Sacrifice(const SkillInput& input, SkillOutput& output) {
    output.Duration = (30 + input.SkillLevel / 5) * 10; // 30~50 sec
    output.Delay = (5 - input.SkillLevel / 33) * 10;    // 5->2 sec

    // Compute the party bonus.
    // output.Damage   = getPercentValue(output.Damage, PartyEffectBoost[input.PartySize]);
    output.Duration = getPercentValue(output.Duration, partyDurationBoost(input.PartySize));
}

void CauseCriticalWounds(const SkillInput& input, SkillOutput& output) {
    // output.Damage = 3 + input.SkillLevel / 10; // 3-13
    //  by sigi. 2002.12.3
    output.Damage = 5 + input.INTE / 10 + input.SkillLevel / 5; // 3-13
    output.Delay = (10 - input.SkillLevel / 33) * 10;           // 10-7 sec
    output.Duration = (3 + input.SkillLevel / 50) * 10;         // 3-5 sec

    // Attack-type skills have no party bonus.
}

void RegenerationSkill(const SkillInput& input, SkillOutput& output) {
    (void)input;
    (void)output;
}

void EnergyDrop(const SkillInput& input, SkillOutput& output) {
    // output.Damage = 25 + input.SkillLevel/5; // 20 ~ 45
    // output.Delay = 30; // 3 sec
    // output.Damage = 10 + input.INTE/10 + input.SkillLevel/5; // 20 ~ 45 12.06

    // 2002.12.06 Jang Hong-chang
    //	output.Damage = 10 + input.INTE/10 + input.SkillLevel/3;
    output.Damage = (int)(18 + input.INTE / 10 + input.SkillLevel / 2.5);
    //	output.Delay = (8 - input.SkillLevel/20)*10;
    output.Delay = (5 - input.SkillLevel / 33) * 10;
}

void VigorDrop(const SkillInput& input, SkillOutput& output) {
    // by sigi. 2002.12.3
    // output.Damage = 25 + input.SkillLevel/5; // 20 ~ 45
    // output.Delay = 30; // 3 sec
    // output.Damage = 10 + input.INTE/10 + input.SkillLevel/5; // 20 ~ 45 2002.12.06

    // 2002.12.06 Jang Hong-chang
    //	output.Damage = 10 + input.INTE/10 + input.SkillLevel/3;
    output.Damage = 18 + input.INTE / 10 + input.SkillLevel / 3;
    //	output.Delay = (8 - input.SkillLevel/20)*10;
    output.Delay = (5 - input.SkillLevel / 33) * 10;
}

void Activation(const SkillInput& input, SkillOutput& output) {
    output.Damage = 0;

    output.Duration = (40 + input.SkillLevel / 5) * 10;
    output.Delay = (6 - input.SkillLevel / 50) * 10;

    // Compute the party bonus.
    output.Damage = 0;
    output.Duration = getPercentValue(output.Duration, partyDurationBoost(input.PartySize));
}

void HolyBlast(const SkillInput& input, SkillOutput& output) {
    output.Damage = 15 + input.SkillLevel / 8;
    output.Delay = (8 - input.SkillLevel / 33) * 10;
    output.Duration = 10;
}

void Sanctuary(const SkillInput& input, SkillOutput& output) {
    output.Damage = 0;
    output.Delay = 5 - input.SkillLevel / 50;
    output.Duration = (5 + input.SkillLevel / 20) * 10;
}

void Reflection(const SkillInput& input, SkillOutput& output) {
    output.Damage = 0;
    // output.Delay = 5 - input.SkillLevel/33;
    output.Duration = (20 + input.SkillLevel / 5) * 10;
    output.Delay = output.Duration; // Delay and Duration are the same. by bezz 2003.3.5
}

void Hymn(const SkillInput& input, SkillOutput& output) {
    output.Damage = 10 + input.SkillLevel / 10;
    output.Duration = (20 + input.SkillLevel / 2) * 10;
    output.Delay = 7 - input.SkillLevel / 20;
}

//////////////////////////////////////////////////////////////////////////////
// Vampire family
//////////////////////////////////////////////////////////////////////////////

void PoisonousHands(const SkillInput& input, SkillOutput& output) {
    output.Damage = min(20, 3 + (input.INTE - 20) / 3);
    output.Delay = 6; // 0.6 sec
}

void AcidTouch(const SkillInput& input, SkillOutput& output) {
    output.Damage = input.STR / 6 + input.INTE / 15;
    output.Delay = 6; // 0.6 sec
}

void GreenPoison(const SkillInput& input, SkillOutput& output) {
    output.Damage = min(15, 5 + (input.INTE - 20) / 10);
    output.Duration = min(20, 10 + (input.INTE - 20) / 10) * 10;
    output.Delay = max(2, 5 - (input.INTE - 20) / 50) * 10; // 5->2 sec
}

void Darkness(const SkillInput& input, SkillOutput& output) {
    output.Duration = (10 + input.INTE / 6) * 10;
    output.Delay = max(2, 10 - input.DEX / 30) * 10;
    output.Range = (3 + (input.SkillLevel - 15) / 25);
}

void YellowPoison(const SkillInput& input, SkillOutput& output) {
    output.Duration = input.INTE / 4 * 10;
    output.Delay = max(2, 10 - input.DEX / 30) * 10;
}

void TransformToBat(const SkillInput& input, SkillOutput& output) {
    (void)input;
    output.Delay = 30;
}

void SummonCasket(const SkillInput& input, SkillOutput& output) {
    (void)input;
    output.Delay = 10;
}

void OpenCasket(const SkillInput& input, SkillOutput& output) {
    (void)input;
    output.Delay = 30;
}

void AcidBolt(const SkillInput& input, SkillOutput& output) {
    output.Damage = min(40, input.STR / 20 + input.INTE / 5);
    output.Delay = max(1, 3 - (input.DEX / 50) - (input.INTE / 50)) * 10; // by bezz. 2002.12.10
}

void GreenStalker(const SkillInput& input, SkillOutput& output) {
    output.Damage = input.STR / 20 + input.INTE / 10;
    output.Tick = 40;
    output.Duration = (input.DEX / 5 + input.INTE / 5) * 10; // by sigi. 2002.12.3
    output.Delay = max(1, 5 - input.DEX / 40) * 10;
}

void BloodyTunnel(const SkillInput& input, SkillOutput& output) {
    (void)input;
    output.Delay = 30;
}

void Paralyze(const SkillInput& input, SkillOutput& output) {
    output.Damage = min(40, 25 + (input.INTE - 20) / 20);
    output.Duration = (3 + input.INTE / 15) * 10;
    output.Delay = max(3, (6 - input.DEX / 50)) * 10;
}

void BloodyMarker(const SkillInput& input, SkillOutput& output) {
    (void)input;
    output.Delay = 30;
}

void DarkBluePoison(const SkillInput& input, SkillOutput& output) {
    output.Damage = min(20, 5 + (input.INTE - 20) / 10);
    output.Duration = min(45, 21 + (input.INTE - 20) / 5) * 10;
    output.Tick = 30;
    output.Delay = max(3, 5 - (input.INTE - 20) / 10) * 10;
}

void TransformToWolf(const SkillInput& input, SkillOutput& output) {
    (void)input;
    output.Delay = 30;
}

void Doom(const SkillInput& input, SkillOutput& output) {
    output.Damage = min(30, 20 + (input.INTE - 20) / 20);
    //	output.Duration = min(80, 30 + (input.INTE-20)/3) * 10;
    output.Duration = min(30, 10 + (input.INTE - 20) / 10) * 10;
    output.Delay = max(3, 5 - (input.INTE - 20) / 10) * 10;
}

void AcidBall(const SkillInput& input, SkillOutput& output) {
    output.Damage = input.STR / 15 + input.INTE / 5;                  // by bezz. 2002.12.10
    output.Delay = max(1, 3 - input.DEX / 50 - input.INTE / 50) * 10; // by bezz. 2002.12.10
}

void Invisibility(const SkillInput& input, SkillOutput& output) {
    (void)input;
    output.Delay = 30;
}

void AcidSwamp(const SkillInput& input, SkillOutput& output) {
    output.Damage = input.STR / 15 + input.INTE / 4;
    output.Duration = max(40, ((input.DEX - 40 + input.INTE - 40) / 6) * 10);
    output.Tick = 15;
    output.Delay = max(2, 6 - input.DEX / 40) * 10;
}

void Seduction(const SkillInput& input, SkillOutput& output) {
    output.Damage = min(30, 10 + (input.INTE - 20) / 10);
    output.Duration = min(50, 20 + (input.INTE - 20) / 5) * 10;
    output.Delay = max(3, 5 - (input.INTE - 20) / 10) * 10;
}

void BloodyNail(const SkillInput& input, SkillOutput& output) {
    output.Damage = input.STR / 8 + input.DEX / 10 + input.INTE / 30;
    output.Delay = 6; // 0.6 sec
}

void BloodyKnife(const SkillInput& input, SkillOutput& output) {
    output.Damage = input.STR / 10 + input.INTE / 5 + input.DEX / 10;
    output.Delay = max(2, 6 - (input.DEX / 50) - (input.INTE / 50)); // by bezz. 2002.12.10
}

void BloodyBall(const SkillInput& input, SkillOutput& output) {
    output.Damage = input.STR / 8 + input.INTE / 4 + input.DEX / 8;
    output.Delay = 30;
}

void BloodyWave(const SkillInput& input, SkillOutput& output) {
    output.Damage = input.INTE / 6 + input.STR / 7 + input.DEX / 7;
    output.Delay = max(1, 3 - input.DEX / 50) * 10;
    // ToHit is used as bKnockback.
    output.ToHit = 50; // min(35, 10 + (int)(input.INTE/10));
}

void BloodyMasterWave(const SkillInput& input, SkillOutput& output) {
    output.Damage = min(80, 40 + (input.INTE - 20) / 4);
    output.Delay = 30;
    // ToHit is used as bKnockback.
    output.ToHit = min(55, 30 + (int)(input.INTE / 10));
}

void BloodyWarp(const SkillInput& input, SkillOutput& output) {
    output.Damage = min(70, 30 + (input.INTE - 20) / 4);
    output.Delay = 30;
    // ToHit is used as bKnockback.
    output.ToHit = min(35, 10 + (int)(input.INTE / 10));
}

void BloodyWall(const SkillInput& input, SkillOutput& output) {
    output.Damage = input.INTE / 5 + input.STR / 10;
    output.Duration = (input.DEX / 15 + input.INTE / 10) * 10;
    output.Delay = max(2, 5 - input.DEX / 50) * 10;
    output.Tick = 10;
}

void BloodySnake(const SkillInput& input, SkillOutput& output) {
    output.Damage = min(35, 10 + (input.INTE - 20) / 9);
    output.Duration = (1 + (input.INTE - 20) / 80) * 10;
    output.Delay = max(3, 6 - (input.INTE - 20) / 50) * 10;
    output.Tick = 3;
}


void BloodySpear(const SkillInput& input, SkillOutput& output) {
    output.Damage = min(180, input.STR / 6 + input.INTE / 2 + input.DEX / 12);
    output.Delay = max(2, 6 - input.DEX / 50) * 10;
}

void PoisonStrike(const SkillInput& input, SkillOutput& output) {
    output.Damage = min(65, input.STR / 10 + input.INTE * 2 / 9);
    output.Delay = max(1, 5 - input.DEX / 50) * 10;
}

void AcidStrike(const SkillInput& input, SkillOutput& output) {
    output.Damage = min(75, input.STR / 8 + input.INTE / 4);
    output.Delay = max(1, 5 - input.DEX / 50) * 10;
}

void BloodyStrike(const SkillInput& input, SkillOutput& output) {
    output.Damage = min(90, input.STR / 8 + input.INTE * 10 / 23);
    output.Delay = max(2, 6 - input.DEX / 50) * 10;
}

void PoisonStorm(const SkillInput& input, SkillOutput& output) {
    // output.Damage = input.STR/30 + input.INTE/9; 12.6

    // 2002.12.06 Jang Hong-chang
    output.Damage = input.STR / 30 + input.INTE / 4;

    // 2003.3.20 Sequoia added a delay minimum.
    output.Delay = max((8 - input.DEX / 50) * 10, 20);
    output.Range = min(6, 3 + (input.SkillLevel - 40) / 8);
}

void AcidStorm(const SkillInput& input, SkillOutput& output) {
    // output.Damage = input.STR/28 + input.INTE/8;  12.6

    // 2002.12.06 Jang Hong-chang
    output.Damage = input.STR / 25 + input.INTE * 2 / 7;

    // 2003.3.20 Sequoia added a delay minimum.
    output.Delay = max((9 - input.DEX / 50) * 10, 20);
}

void BloodyStorm(const SkillInput& input, SkillOutput& output) {
    output.Damage = input.STR / 25 + input.INTE / 6;
    output.Duration = (input.DEX / 100 + input.INTE / 100) * 10;

    // 2003.3.20 Sequoia added a delay minimum.
    output.Delay = max((9 - input.DEX / 50) * 10, 20);
}

void Extreme(const SkillInput& input, SkillOutput& output) {
    // the damage bonus is not computed here,
    // computed in Vampire::initAllStat().
    output.Damage = 0;
    //	output.Duration = (30 + (input.INTE-20)/6) * 10;  // 30 -> 80
    output.Duration = (60 + input.STR / 15 + input.DEX / 6 + input.INTE / 20) * 10; // 30 -> 80
    output.Delay = (6 - input.STR / 70) * 10;                                       // 5 -> 2
}


void Peace(const SkillInput& input, SkillOutput& output) {
    //	output.Duration = (15 + input.INTE / 10)*10;
    output.Duration = (10 + input.INTE / 40 + input.SkillLevel / 20) * 10;
    output.Delay = max(10 - input.INTE / 20, 5) * 10;
}

void Death(const SkillInput& input, SkillOutput& output) {
    // 2003.3.20 by Sequoia a maximum value was added.
    //	output.Damage   = min( input.INTE*10/25 + input.STR/6, 150 );
    output.Damage = min(input.INTE / 5 + input.STR / 12, 80);
    output.Duration = (input.INTE / 5) * 10;
    output.Delay = max(2, 5 - input.DEX / 50) * 10;
}

void Mephisto(const SkillInput& input, SkillOutput& output) {
    // This skill alone, specially.. passes its parameters on as follows.
    // input.SkillLevel = pVampire->getSTR()+pVampire->getDEX()+pVampire->getINT();
    // input.DomainLevel = pVampire->getLevel();

    // output.Damage   = 10 + (input.SkillLevel-60)/15;
    // output.Duration = (25 + (input.DomainLevel/4))*10;

    // by sigi. 2002.12.3
    output.Damage = 10 + (input.STR + input.DEX + input.INTE) / 30;
    output.Duration = max(10, (input.DomainLevel - 95) * 10) * 10;
    output.Delay = max(2, 5 - (input.INTE - 20) / 10) * 10;
}

void Transfusion(const SkillInput& input, SkillOutput& output) {
    output.Delay = max(3, 6 - (input.INTE - 20) / 50) * 10;
}

void SummonMonsters(const SkillInput& input, SkillOutput& output) {
    (void)input;
    output.Damage = 0;
    output.Delay = 20; // 2 sec
}

void GroundAttack(const SkillInput& input, SkillOutput& output) {
    output.Damage = min(100, 30 + (input.INTE - 20) / 5); // % damage
    output.Duration = 20;                                 // explodes a few seconds later
    output.Tick = 15;
    output.Delay = max(3, 6 - (input.INTE - 20) / 50) * 10;
}


void Hallucination(const SkillInput& input, SkillOutput& output) {
    // output.Damage   = min(20, 10 + (input.INTE-20)/20);
    // output.Duration = min(80, 30 + (input.INTE-20)/3) * 10;
    output.Duration = (20 + (input.STR / 65) + (input.DEX / 85) + (input.INTE / 40)) * 10;
    // output.Delay    = max(3, 5 - (input.INTE-20)/10) * 10;
    // output.Delay	= ( 10 - (input.DEX/50) ) * 10;
    output.Delay = output.Duration;
}


void SoulChain(const SkillInput& input, SkillOutput& output) {
    (void)input;
    output.Duration = 100;
    output.Delay = 1200;
}

void SharpShield(const SkillInput& input, SkillOutput& output) {
    output.Duration = (10 + (input.SkillLevel / 2)) * 10;
    output.Damage = (input.STR / 20) + (input.SkillLevel / 20);
    // output.Delay    = ( 5 - ( input.SkillLevel / 25 ) ) * 10;
    output.Delay = output.Duration; // Delay and Duration are the same. by bezz 2003.3.5
}

void WideLightning(const SkillInput& input, SkillOutput& output) {
    output.Delay = 10;
    output.Damage = (input.STR / 8) + (input.SkillLevel / 3);
    output.Duration = 6; // 0.6 sec
    output.Tick = 6;     // 0.6 sec
}

void GunShotGuidance(const SkillInput& input, SkillOutput& output) {
    output.Damage = 50 + input.SkillLevel / 2;
    output.Duration = 10;
    output.Delay = 10;
    output.Range = 8;
}

void AirShield(const SkillInput& input, SkillOutput& output) {
    //	output.Damage	= 50 + ( input.SkillLevel >> 1 ); 			// 50 + SkillLevel / 2
    output.Damage = 50 + (input.SkillLevel / 5);           // 50 + SkillLevel / 2
    output.Duration = (10 + (input.SkillLevel >> 1)) * 10; // 10 + SkillLevel / 2 (sec)
    // output.Delay	= ( 5 - ( input.SkillLevel / 25 ) ) * 10;
    output.Delay = output.Duration; // Delay and Duration are the same. by bezz 2003.3.5
}

void BulletOfLight(const SkillInput& input, SkillOutput& output) {
    output.Damage = -20 + (input.SkillLevel * 2 / 3); // / 1.5 );
    output.ToHit = -10 + (input.SkillLevel / 2);      // / 1.5 );
    output.Delay = 2;                                 // 0.2 sec
}

void HandsOfWisdom(const SkillInput& input, SkillOutput& output) {
    output.Damage = input.STR / 15 + input.INTE / 8;
    output.Delay = 6; // 0.6 sec
}

void LightBall(const SkillInput& input, SkillOutput& output) {
    //	output.Damage = 2 + (input.INTE/20) + (input.SkillLevel/10);
    output.Damage = 9 + (input.INTE / 20) + (input.SkillLevel / 10);
    output.Delay = 10; // 1 sec
    output.Range = 2 + (input.SkillLevel / 25);
}

void HolyArrow(const SkillInput& input, SkillOutput& output) {
    //	output.Damage = 2 + (input.INTE/20) + (input.SkillLevel/10);
    output.Damage = 8 + (input.INTE / 20) + (input.SkillLevel / 10);
    output.Delay = 10; // 1 sec
    output.Range = 2 + (input.SkillLevel / 25);
}

void Rebuke(const SkillInput& input, SkillOutput& output) {
    //	output.Duration = ( 2 + (input.SkillLevel/20) ) * 10;
    output.Duration = (8 + (input.SkillLevel / 20)) * 10;
    //	output.Damage	= ( input.INTE/10 ) + ( input.SkillLevel/5 );
    output.Damage = 10 + (input.INTE / 10) + (input.SkillLevel / 5);
    output.Delay = 80;
}

void SpiritGuard(const SkillInput& input, SkillOutput& output) {
    output.Duration = (10 + (input.SkillLevel / 2)) * 10;
    output.Damage = (input.INTE / 10) + (input.SkillLevel / 4);
    output.Delay = 10;
    output.Tick = 10;
}

void Regeneration(const SkillInput& input, SkillOutput& output) {
    //	output.Duration = ( 5 + (input.SkillLevel/10) ) * 10;
    output.Damage = 10 + (input.INTE / 40) + (input.SkillLevel / 10);
    output.Duration = (15 + (input.SkillLevel / 2)) * 10;
    //	output.Delay	= ( 6 + (input.SkillLevel/50) ) * 10;
    output.Delay = output.Duration;
    output.Tick = 10;
}

void PowerOfLand(const SkillInput& input, SkillOutput& output) {
    output.Delay = 10;
    output.Damage = (input.STR / 8) + (input.SkillLevel / 3);
    output.Duration = 10; // 1 sec
    output.Tick = 10;     // 1 sec
}

void TurnUndead(const SkillInput& input, SkillOutput& output) {
    //	output.Damage	= 10 + ( input.INTE / 20 ) + ( input.SkillLevel / 2 );
    output.Damage = 20 + (input.INTE / 20) + (input.SkillLevel / 3);
    output.Delay = 10;
}

void Armageddon(const SkillInput& input, SkillOutput& output) {
    output.Damage = (input.STR / 8) + (input.INTE / 6);
    output.Delay = 10;
    output.Duration = ((input.DEX / 10) + (input.INTE / 20)) * 10;
    output.Tick = 10;
}

void BloodyBreaker(const SkillInput& input, SkillOutput& output) {
    output.Damage = input.INTE / 4 + input.STR / 5 + input.DEX / 5;
    output.Delay = max(1, 3 - input.DEX / 50) * 10;
    // ToHit is the knockback chance
    output.ToHit = 50;
}

void RapidGliding(const SkillInput& input, SkillOutput& output) {
    output.Range = min(6, 2 + (input.DEX / 50));
    output.Delay = max(1, 5 - (input.DEX / 60)) * 10; // min=1
}

void MagicElusion(const SkillInput& input, SkillOutput& output) {
    Attr_t SUM = (Attr_t)(input.STR + input.DEX + input.INTE);
    output.Damage = SUM / 5;
    output.Duration = 50 + (SUM / 3);
    output.Delay = 50;
}

void PoisonMesh(const SkillInput& input, SkillOutput& output) {
    output.Damage = 5 + input.SkillLevel / 4;
    output.Duration = 50;
    output.Delay = 50;
}

void IllusionOfAvenge(const SkillInput& input, SkillOutput& output) {
    Attr_t SUM = (Attr_t)(input.STR + input.DEX + input.INTE);
    output.Damage = 15 + (SUM / 3);
    output.Delay = 50;
}

void WillOfLife(const SkillInput& input, SkillOutput& output) {
    output.Damage = 5 + input.SkillLevel / 7;
    output.Duration = 30 + input.SkillLevel;
    output.Delay = output.Duration * 2;
}

void DenialMagic(const SkillInput& input, SkillOutput& output) {
    output.Duration = (10 + (input.SkillLevel / 2)) * 10;
    output.Delay = (8 - (input.SkillLevel / 20)) * 10;
}

void Requital(const SkillInput& input, SkillOutput& output) {
    output.Duration = (10 + (input.SkillLevel / 2)) * 10;
    output.Delay = (10 + (input.SkillLevel / 2)) * 10;
    output.Damage = (input.INTE / 8) + (input.SkillLevel / 4);
}

void Concealment(const SkillInput& input, SkillOutput& output) {
    //	output.Duration = ( 5 + ( input.SkillLevel / 5 )) * 10;
    //	output.Duration = ( 15 + ( input.SkillLevel / 3 )) * 10;
    output.Duration = (30 + (input.SkillLevel / 2)) * 10;
    //	output.Delay = max( 1, 5 - ( input.DEX / 50 ) ) * 10;
    output.Delay = output.Duration;
}

void SwordRay(const SkillInput& input, SkillOutput& output) {
    //	output.Damage = 5 + input.STR/15 + input.SkillLevel/10;
    output.Damage = input.STR / 25 + input.SkillLevel / 20;
    output.Range = 2 + input.SkillLevel / 33;
    output.Delay = (5 - input.SkillLevel / 25) * 10;
    output.Tick = 5;
    output.Duration = 5;
}

void MultiAmputate(const SkillInput& input, SkillOutput& output) {
    output.Damage = input.STR / 20 + input.SkillLevel / 10;
    output.Range = 2 + input.SkillLevel / 25;
    output.Delay = (5 - input.SkillLevel / 25) * 10;
}

void HitConvert(const SkillInput& input, SkillOutput& output) {
    output.Delay = (8 - input.SkillLevel / 33) * 10;
    output.Range = 2 + input.SkillLevel / 33;
}

void WildTyphoon(const SkillInput& input, SkillOutput& output) {
    output.Damage = (input.STR / 12) + (input.SkillLevel / 5);
    output.Delay = 50 - input.SkillLevel * 10 / 25;
    output.Range = 1;
}

void UltimateBlow(const SkillInput& input, SkillOutput& output) {
    output.Delay = 100 - input.SkillLevel * 10 / 25;
    output.Range = 1 + input.SkillLevel / 50;
}

void Illendue(const SkillInput& input, SkillOutput& output) {
    output.Damage = (input.INTE / 10) * (1 + (input.SkillLevel / 33));
    output.Delay = 50 - input.SkillLevel * 10 / 33;
    output.Range = 5;
}

void Lightness(const SkillInput& input, SkillOutput& output) {
    output.Duration = (15 + ((input.SkillLevel / 25) * 10)) * 10;
    output.Delay = min(300, output.Duration);
}

void Flourish(const SkillInput& input, SkillOutput& output) {
    // 1 <= SkillLevel <= 15
    if (input.SkillLevel < 16) {
        output.Damage = 3 + (int)((input.STR / 20.0) * (1.0 + (input.SkillLevel / 22.5)));
        output.Damage = min(30, output.Damage);
    }
    // 16 <= SkillLevel <= 30
    else {
        output.Damage = 3 + (int)((input.STR / 20.0) * (4.0 / 3.0 + (input.SkillLevel / 45.0)));
        output.Damage = min(30, output.Damage);
        if (input.SkillLevel == 30)
            output.Damage = (int)(output.Damage * 1.1);
    }
}

void Evade(const SkillInput& input, SkillOutput& output) {
    if (input.SkillLevel <= 15) {
        //		output.Damage = (int)( ( input.DEX / 10.0 ) * ( 1.0 + ( (float)(input.SkillLevel) / 22.5 ) ) );
        output.Damage = 10 + (int)(input.SkillLevel * 16.0 / 9.0);
    } else {
        //		output.Damage = (int)( ( input.DEX / 10.0 ) * ( 4.0/3.0 + ( (float)(input.SkillLevel) / 45.0 ) ) );
        output.Damage = 10 + (int)(40.0 / 3.0 + input.SkillLevel * 8.0 / 9.0);
        if (input.SkillLevel == 30)
            output.Damage = (int)(output.Damage * 1.1);
    }

    output.Damage = min(50, output.Damage);
    //	output.Duration = (int)(( 30.0 + ( (float)(input.SkillLevel) * 3.0 ) ) * 10);
    output.Duration = (60 + input.DEX / 10 * (input.SkillLevel * 2 / 3)) * 10;
    output.Duration = min(1200, output.Duration);
    //	output.Delay = max ( 50 , output.Duration * 2 - ( input.DEX * 2 ) );
    output.Delay = output.Duration - (input.SkillLevel / 5);
}

void SharpRound(const SkillInput& input, SkillOutput& output) {
    if (input.SkillLevel <= 15) {
        //		output.Damage = (int)( ( input.STR / 9.0 ) * ( 1.0 + ( input.SkillLevel / 15.0 ) ) );
        output.Damage = (int)(14.0 + (input.STR / 15.0) * (1.0 + (input.SkillLevel / 15.0)));
        output.Damage = min(55, output.Damage);
    } else {
        //		output.Damage = (int)( ( input.STR / 9.0 ) * ( 1.5 + ( input.SkillLevel / 30.0 ) ) );
        output.Damage = (int)(14.0 + (input.STR / 15.0) * (1.5 + (input.SkillLevel / 30.0)));
        output.Damage = min(55, output.Damage);
        if (input.SkillLevel == 30)
            output.Damage = (int)(output.Damage * 1.1);
    }

    output.Delay = 0;
}

void BackStab(const SkillInput& input, SkillOutput& output) {
    (void)input;
    (void)output;
}

void Blunting(const SkillInput& input, SkillOutput& output) {
    if (input.SkillLevel <= 15) {
        output.Damage = min(60, (int)(((input.STR / 20.0) + (input.DEX / 10.0)) * (1.0 + (input.SkillLevel / 15.0))));
    } else {
        output.Damage = min(60, (int)(((input.STR / 20.0) + (input.DEX / 10.0)) * (1.5 + (input.SkillLevel / 30.0))));
        if (input.SkillLevel == 30)
            output.Damage = (int)(output.Damage * 1.1);
    }

    output.Duration = min(300, (int)((((input.DEX + input.INTE) / 20.0) + ((input.SkillLevel / 2.0))) * 10));
    if (input.SkillLevel == 30)
        output.Duration = (int)(output.Duration * 1.1);

    output.Delay = max(40, (int)((8 - (input.DEX / 50.0)) * 10));
    output.Range = 1 + (input.SkillLevel / 10);
}

void GammaChop(const SkillInput& input, SkillOutput& output) {
    if (input.SkillLevel <= 15) {
        output.Damage = 10 + (int)((input.STR / 10.0) * (1.0 + (input.SkillLevel / 11.25)));
        output.Damage = min(120, output.Damage);
    } else {
        output.Damage = 10 + (int)((input.STR / 10.0) * (5.0 / 3.0 + (input.SkillLevel / 22.5)));
        output.Damage = min(120, output.Damage);
        if (input.SkillLevel == 30)
            output.Damage = (int)(output.Damage * 1.1);
    }
    output.Delay = 10; // max(10,(int)(( 4 - ( input.DEX / 60.0 ) ) * 10));
    output.Range = 1 + (input.SkillLevel / 15);
}

void CrossGuard(const SkillInput& input, SkillOutput& output) {
    if (input.SkillLevel <= 15) {
        //		output.Damage = (int)( ( input.STR / 20.0 ) * ( 1.0 + ( input.SkillLevel / 11.25 ) ) );
        output.Damage = 15 + (input.SkillLevel * 14 / 9);
    } else {
        //		output.Damage = (int)( ( input.STR / 20.0 ) * ( 5.0/3.0 + ( input.SkillLevel / 22.5 ) ) );
        output.Damage = 15 + (35 / 3 + input.SkillLevel * 7 / 9);
    }

    output.Damage = min(50, output.Damage);
    output.Duration = (int)((60.0 + (input.DEX / 10.0) + (input.SkillLevel * 2.0)) * 10);
    output.Duration = min(1800, output.Duration);

    if (input.SkillLevel == 30)
        output.Duration = (int)(output.Duration * 1.1);

    output.Delay = output.Duration;
}

void KasasArrow(const SkillInput& input, SkillOutput& output) {
    if (input.SkillLevel <= 15) {
        //		output.Damage = 7 + (int)( ( input.INTE / 8.0 ) * ( 1.0 + ( (float)(input.SkillLevel) / 22.5 ) ) );
        output.Damage = 15 + (int)((input.INTE / 55.0) * (1.0 + ((float)(input.SkillLevel) / 30.0)));
    } else {
        //		output.Damage = 7 + (int)( ( input.INTE / 8.0 ) * ( 4.0/3.0 + ( (float)(input.SkillLevel) / 45.0 ) ) );
        output.Damage = 15 + (int)((input.INTE / 50.0) * (1.5 + ((float)(input.SkillLevel) / 25.0)));
        if (input.SkillLevel == 30)
            output.Damage = (int)(output.Damage * 1.2);
    }

    output.Delay = 0;
    output.Range = 3 + (input.SkillLevel / 10);
}

void HandsOfFire(const SkillInput& input, SkillOutput& output) {
    output.Damage = (int)(5.0 + input.SkillLevel * 15.0 / 30.0);
    if (input.SkillLevel <= 15) {
        //		output.Damage = (int)( ( 1 + input.INTE / 50.0 ) * ( 1.0 + ( (float)(input.SkillLevel) / 22.5 ) ) );
        //		output.Damage = (int)( ( 7 + input.INTE / 12.0 ) * ( 1.0 + ( (float)(input.SkillLevel) / 11.25 ) ) );
    } else {
        //		output.Damage = (int)( ( 1 + input.INTE / 50.0 ) * ( 4.0/3.0 + ( (float)(input.SkillLevel) / 45.0 ) ) );
        //		output.Damage = (int)( ( 7 + input.INTE / 12.0 ) * ( 5.0/3.0 + ( (float)(input.SkillLevel) / 22.5 ) ) );
    }

    output.Duration = (int)((60.0 + (input.DEX / 10.0) + (input.SkillLevel * 1.5)) * 10);
    if (input.SkillLevel == 30)
        output.Duration = (int)(output.Duration * 1.1);
    output.Duration = min(1200, output.Duration);
    output.Delay = output.Duration;
}

void Prominence(const SkillInput& input, SkillOutput& output) {
    if (input.SkillLevel <= 15) {
        //		output.Damage = (int)( ( input.INTE / 30.0 ) * ( 1.0 + ( (float)(input.SkillLevel) / 15.0 ) ) );
        //		output.Damage = 15 + (int)( ( input.INTE / 20.0 ) * ( 1.0 + ( (float)(input.SkillLevel) / 11.25 ) ) );
        output.Damage = 5 + (int)((input.INTE / 25.0) * (1.0 + ((float)(input.SkillLevel) / 10.0)));
    } else {
        //		output.Damage = (int)( ( input.INTE / 30.0 ) * ( 1.5 + ( (float)(input.SkillLevel) / 30.0 ) ) );
        //		output.Damage = 15 + (int)( ( input.INTE / 20.0 ) * ( 5.0/3.0 + ( (float)(input.SkillLevel) / 22.5 ) )
        //);
        output.Damage = 5 + (int)((input.INTE / 25.0) * (2.0 + ((float)(input.SkillLevel) / 25.0)));
    }

    output.Duration = (int)((5.0 + (input.INTE / 30.0) + (input.SkillLevel / 5.0)) * 10);
    output.Delay = output.Duration - (input.DEX / 20);
    if (input.SkillLevel == 30)
        output.Duration = (int)(output.Duration * 1.1);

    output.Range = 2 + (input.SkillLevel / 10);
    output.Tick = 10;
}

void RingOfFlare(const SkillInput& input, SkillOutput& output) {
    if (input.SkillLevel <= 15) {
        //		output.Damage = (int)( ( input.INTE / 10.0 ) * ( 1.0 + ( (float)(input.SkillLevel) / 15.0 ) ) );
        output.Damage = (int)(5 + (input.INTE / 17.0) + (1.0 + ((float)(input.SkillLevel) / 3.0)));
    } else {
        //		output.Damage = (int)( ( input.INTE / 10.0 ) * ( 1.5 + ( (float)(input.SkillLevel) / 30.0 ) ) );
        output.Damage = (int)(2 + (input.INTE / 15.0) + (1.0 + ((float)(input.SkillLevel) / 2.0)));
    }

    output.Duration = (int)((35.0 + (input.INTE / 30.0) + (input.SkillLevel / 3.0)) * 10);
    if (input.SkillLevel == 30)
        output.Duration = (int)(output.Duration * 1.1);
    output.Delay = output.Duration;
}

void BlazeBolt(const SkillInput& input, SkillOutput& output) {
    if (input.SkillLevel <= 15) {
        //		output.Damage = 10 + (int)( ( input.INTE / 13.0 ) * ( 1.0 + ( (float)(input.SkillLevel) / 11.25 ) ) );
        output.Damage = 20 + (int)((input.INTE / 20.0) + (1.0 + ((float)(input.SkillLevel) / 3.0)));
    } else {
        //		output.Damage = 10 + (int)( ( input.INTE / 13.0 ) * ( 5.0 / 3.0 + ( (float)(input.SkillLevel) / 22.5 ) )
        //);
        output.Damage = 20 + (int)((input.INTE / 20.0) + (((float)(input.SkillLevel) / 2.5)));
        if (input.SkillLevel == 30)
            output.Damage = (int)(output.Damage * 1.2);
    }

    output.Delay = 10;
    output.Range = 4 + (input.SkillLevel / 10);
}

void IceField(const SkillInput& input, SkillOutput& output) {
    if (input.SkillLevel <= 15) {
        output.Duration = (int)max(
            10, (int)min(300.0, ((input.INTE / 30.0) * (1.0 + ((float)(input.SkillLevel) / 22.5)) + 5.0) * 10));
        output.Range = max(20, min(150, output.Duration));
    } else {
        output.Duration = (int)max(
            10, (int)min(300.0, ((input.INTE / 30.0) * (4.0 / 3.0 + ((float)(input.SkillLevel) / 45.0)) + 5.0) * 10));
        output.Range = max(20, min(150, output.Duration));
    }

    output.Delay = (int)(output.Duration * 1.2 - (input.SkillLevel));
    output.Tick = 10;
}

void WaterBarrier(const SkillInput& input, SkillOutput& output) {
    if (input.SkillLevel <= 15) {
        output.Damage = (int)min(40, (int)(10.0 + (input.INTE / 20.0) * (1.0 + ((float)(input.SkillLevel) / 15.0))));
    } else {
        output.Damage = (int)min(40, (int)(10.0 + (input.INTE / 20.0) * (1.5 + ((float)(input.SkillLevel) / 30.0))));
        if (input.SkillLevel == 30)
            output.Damage = (int)(output.Damage * 1.1);
    }

    output.Duration = (int)((30.0 + (input.INTE / 10.0) * (1.0 + (float)(input.SkillLevel) / 20.0)) * 10);
    output.Duration = min(1800, output.Duration);
    if (input.SkillLevel == 30)
        output.Duration = (int)(output.Duration * 1.1);
    output.Delay = output.Duration;
}

void NymphRecovery(const SkillInput& input, SkillOutput& output) {
    if (input.SkillLevel <= 15) {
        if (input.TargetType == SkillInput::TARGET_OTHER) {
            output.Damage = 15 + (int)((input.INTE / 13.0) * (1.0 + ((float)(input.SkillLevel) / 15.0)));
        } else {
            output.Damage = 15 + (int)((input.INTE / 10.0) * (1.0 + ((float)(input.SkillLevel) / 15.0)));
        }
    } else {
        if (input.TargetType == SkillInput::TARGET_OTHER) {
            output.Damage = 15 + (int)((input.INTE / 13.0) * (1.5 + ((float)(input.SkillLevel) / 30.0)));
        } else {
            output.Damage = 15 + (int)((input.INTE / 10.0) * (1.5 + ((float)(input.SkillLevel) / 30.0)));
        }
    }
    if (input.SkillLevel == 30)
        output.Damage = (int)(output.Damage * 1.1);

    output.Delay = (int)((4 - (input.SkillLevel / 15.0)) * 10);
}

void Liberty(const SkillInput& input, SkillOutput& output) {
    output.Delay = (8 - (input.SkillLevel / 10)) * 10;
}

void Tendril(const SkillInput& input, SkillOutput& output) {
    if (input.SkillLevel <= 15) {
        //		output.Duration = (int)(( 3 + ( input.INTE / 30.0 ) * ( 1.0 + (input.SkillLevel / 22.5) ) ) * 10);
        output.Duration = (3 + input.SkillLevel / 7) * 10;
    } else {
        //		output.Duration = (int)(( 3 + ( input.INTE / 30.0 ) * ( 4.0/3.0 + (input.SkillLevel / 45.0) ) ) * 10);
        output.Duration = (5 + input.SkillLevel / 15) * 10;
    }

    output.Delay = (9 - (input.SkillLevel / 6)) * 10;
    output.Range = 1 + (input.SkillLevel / 10);
}

void StoneAuger(const SkillInput& input, SkillOutput& output) {
    if (input.SkillLevel <= 15) {
        output.Damage = 20 + (int)(((input.INTE / 30.0) * (1.0 + (input.SkillLevel / 15.0))));
        output.Damage = min(50, output.Damage);
    } else {
        output.Damage = 20 + (int)(((input.INTE / 30.0) * (1.5 + (input.SkillLevel / 30.0))));
        output.Damage = min(50, output.Damage);
        if (input.SkillLevel == 30)
            output.Damage = (int)(output.Damage * 1.1);
    }

    output.Delay = (4 - (input.SkillLevel / 10)) * 10;
}

void EarthsTeeth(const SkillInput& input, SkillOutput& output) {
    if (input.SkillLevel <= 15) {
        output.Damage = 18 + (int)((input.INTE / 13.0) * (1.0 + ((float)(input.SkillLevel) / 11.25)));
        output.Damage = min(90, output.Damage);
    } else {
        output.Damage = 18 + (int)((input.INTE / 13.0) * (5.0 / 3.0 + ((float)(input.SkillLevel) / 22.5)));
        output.Damage = min(90, output.Damage);
        if (input.SkillLevel == 30)
            output.Damage = (int)(output.Damage * 1.1);
    }

    output.Delay = (2 - (input.SkillLevel / 10)) * 10;
    output.Delay = max(output.Delay, 6);
    output.Range = 3 + (input.SkillLevel / 15);
}

void HandsOfNizie(const SkillInput& input, SkillOutput& output) {
    if (input.SkillLevel <= 15) {
        output.Damage = 10 + (int)((input.INTE / 18.0) * (1.0 + ((float)(input.SkillLevel) / 15.0)));
    } else {
        output.Damage = 10 + (int)((input.INTE / 18.0) * (1.5 + ((float)(input.SkillLevel) / 30.0)));
        if (input.SkillLevel == 30)
            output.Damage = (int)(output.Damage * 1.1);
    }

    output.Delay = 6;
}

void GnomesWhisper(const SkillInput& input, SkillOutput& output) {
    if (input.SkillLevel <= 15) {
        output.Duration = (int)((input.INTE / 5.0) * (1.0 + ((float)(input.SkillLevel) / 11.25)));
    } else {
        output.Duration = (int)((input.INTE / 5.0) * (5.0 / 3.0 + ((float)(input.SkillLevel) / 22.5)));
        if (input.SkillLevel == 30)
            output.Duration = (int)(output.Duration * 1.1);
    }

    output.Duration *= 10;
    output.Delay = output.Duration;
}

void RefusalEther(const SkillInput& input, SkillOutput& output) {
    output.Delay = (5 - (input.SkillLevel / 15)) * 10;

    if (input.SkillLevel <= 8) {
        output.Range = 1;
    } else if (input.SkillLevel <= 16) {
        output.Range = 3;
    } else if (input.SkillLevel <= 23) {
        output.Range = 4;
    } else if (input.SkillLevel <= 29) {
        output.Range = 5;
    } else {
        output.Range = 7;
    }
}

void EmissionWater(const SkillInput& input, SkillOutput& output) {
    if (input.SkillLevel <= 15) {
        output.Damage = 10 + (int)((input.INTE / 50.0) * (1.0 + ((float)(input.SkillLevel) / 20.0)));
    } else {
        output.Damage = 10 + (int)((input.INTE / 55.0) * (2.0 + ((float)(input.SkillLevel) / 15.0)));
        if (input.SkillLevel == 30)
            output.Damage = (int)(output.Damage * 1.1);
    }

    output.Delay = 10;
}

void BeatHead(const SkillInput& input, SkillOutput& output) {
    if (input.SkillLevel <= 15) {
        output.Damage = 10 + (int)((input.INTE / 8.0) * (1.0 + ((float)(input.SkillLevel) / 22.5)));
    } else {
        output.Damage = 10 + (int)((input.INTE / 8.0) * (4.0 / 3.0 + ((float)(input.SkillLevel) / 45.0)));
        if (input.SkillLevel == 30)
            output.Damage = (int)(output.Damage * 1.1);
    }

    output.Delay = 0;
}

void DivineSpirits(const SkillInput& input, SkillOutput& output) {
    if (input.SkillLevel <= 15) {
        output.Damage = 10 + (int)((float)input.SkillLevel * 4.0 / 3.0);
    } else {
        output.Damage = 20 + (int)((float)input.SkillLevel * 2.0 / 3.0);
        if (input.SkillLevel == 30)
            output.Damage = (int)(output.Damage * 1.1);
    }

    output.Duration = 30 + input.SkillLevel * 2;
    output.Duration *= 10;
    output.Delay = output.Duration;
}

void BlitzSliding(const SkillInput& input, SkillOutput& output) {
    output.Damage = 4 * (1 + input.SkillLevel / 25);
    output.Delay = (4 - (input.SkillLevel / 50)) * 10;
}

void JabbingVein(const SkillInput& input, SkillOutput& output) {
    output.Damage = -30 + (input.STR / 20 + input.SkillLevel / 3);
    output.Delay = 2;

    if (input.TargetType == SkillInput::TARGET_PC) {
        output.Range = 2 + (input.SkillLevel / 33);
        output.Duration = (1 + (input.SkillLevel / 50)) * 10;
    } else {
        output.Range = 5 + (input.SkillLevel / 20);
        output.Duration = (2 + (input.SkillLevel / 25)) * 10;
    }
}

void GreatHeal(const SkillInput& input, SkillOutput& output) {
    if (input.TargetType == SkillInput::TARGET_OTHER) {
        output.Damage = 100 + input.SkillLevel / 3;
    } else {
        output.Damage = 110 + input.SkillLevel / 3;
    }

    output.Delay = (5 - input.SkillLevel / 33) * 10;

    // Compute the party bonus.
    // output.Duration = getPercentValue(output.Duration, PartyDurationBoost[input.PartySize]);
}

void DivineGuidance(const SkillInput& input, SkillOutput& output) {
    if (input.TargetType == SkillInput::TARGET_PC) {
        output.Damage = 15 + (2 * input.SkillLevel / 25);
        output.Tick = 15;
        output.Range = (input.INTE / 5) + (input.SkillLevel / 10);
        output.Range = min(output.Range, 70);
    } else {
        output.Damage = 20 + (3 * input.SkillLevel / 20);
        output.Tick = 10;
        output.Range = (input.INTE / 3) + (input.SkillLevel / 10);
        output.Range = min(output.Range, 90);
    }

    output.Delay = (10 + input.SkillLevel / 20) * 10;
    output.Duration = (10 + input.SkillLevel / 20) * 10;
}

void BlazeWalk(const SkillInput& input, SkillOutput& output) {
    output.Delay = (5 - input.SkillLevel / 50) * 10;
    output.Damage = 2 * (1 + input.SkillLevel / 25);
}

void BloodyZenith(const SkillInput& input, SkillOutput& output) {
    output.Delay = (7 - (input.STR / 100 + input.DEX / 220 + input.INTE / 330)) * 10;
    output.Delay = max(30, output.Delay);
    //	output.Damage = 20 + input.STR/8 + input.DEX/20;
    output.Damage = min(150, 30 + input.STR / 3 + input.DEX / 10 + input.INTE / 20);
}

void Rediance(const SkillInput& input, SkillOutput& output) {
    output.Duration = (60 + input.SkillLevel * 3 / 2) * 10;
    //	output.Duration = ( 30 + input.STR/10 + input.SkillLevel*10/12 ) * 10;
    output.Delay = output.Duration;
    output.Damage = 7 + input.SkillLevel / 20 + input.DomainLevel / 10;
}

void LarSlash(const SkillInput& input, SkillOutput& output) {
    output.Delay = 4;
    output.Damage = 5 + input.STR / 10 + input.SkillLevel / 8;
}

void Trident(const SkillInput& input, SkillOutput& output) {
    output.ToHit = -30 + input.DEX / 20 + input.SkillLevel / 4;
    output.Damage = -30 + input.STR / 20 + input.SkillLevel / 4;
    output.Delay = 2;
}

void HeartCatalyst(const SkillInput& input, SkillOutput& output) {
    output.Damage = 2 + input.SkillLevel / 20;
    output.Tick = 20;
    output.Duration = (10 + input.SkillLevel / 2) * 10;
    output.Delay = output.Duration;
}

void ProtectionFromBlood(const SkillInput& input, SkillOutput& output) {
    // by sigi. 2002.12.3
    if (input.TargetType == SkillInput::TARGET_OTHER) {
        // output.Damage = 10 + input.SkillLevel/10;
        output.Damage = min(20, 10 + input.INTE / 20);
    } else {
        // output.Damage = 20 + input.SkillLevel/10;
        output.Damage = min(30, 20 + input.INTE / 20);
    }

    // output.Duration = (20 + input.SkillLevel/2) * 10; // 20~70 sec
    output.Duration = (60 + input.INTE / 2 + input.SkillLevel / 2) * 10; // 20~70 sec
    output.Delay = (5 - input.SkillLevel / 33) * 10;                     // 5->2 sec

    // Compute the party bonus.
    output.Damage = getPercentValue(output.Damage, partyEffectBoost(input.PartySize));
    output.Duration = getPercentValue(output.Duration, partyDurationBoost(input.PartySize));
}

void MoleShot(const SkillInput& input, SkillOutput& output) {
    if (input.Gun == GunClass::SG) {
        output.Damage = 3 + input.SkillLevel / 10;
    }

    else if (input.Gun == GunClass::AR || input.Gun == GunClass::SMG) {
        output.Damage = 1 + input.SkillLevel / 30;
    }

    else if (input.Gun == GunClass::SR) {
        output.Damage = input.SkillLevel / 50;
    }

    output.ToHit = -20 + input.SkillLevel / 5;
    output.Delay = 2;
}

void Eternity(const SkillInput& input, SkillOutput& output) {
    output.Damage = -50 + (input.SkillLevel / 2);
    //	output.Range = 50 + (input.SkillLevel/3);
    output.Duration = 50;
}

void InstallTrap(const SkillInput& input, SkillOutput& output) {
    output.Duration = 600;
    output.Tick = (5 + (input.SkillLevel / 25)) * 10;
    output.Delay = (60 - (int)(input.SkillLevel / 2.5)) * 10;
}

void HolyArmor(const SkillInput& input, SkillOutput& output) {
    output.Duration = (30 + (input.SkillLevel / 2)) * 10;
    output.Damage = 10 + (input.INTE / 20) + (input.SkillLevel / 10);
    output.Delay = output.Duration;
}

void MercyGround(const SkillInput& input, SkillOutput& output) {
    //	output.Duration = (10 + input.INTE/20 + input.SkillLevel/10)*10;
    output.Duration = (15 + input.INTE / 15 + input.SkillLevel / 6) * 10;
    output.Delay = (10 - input.SkillLevel / 33) * 10;
}

void CreateHolyPotion(const SkillInput& input, SkillOutput& output) {
    output.Delay = (5 - input.SkillLevel / 33) * 10;
}

void TransformToWerwolf(const SkillInput& input, SkillOutput& output) {
    (void)input;
    output.Delay = 50;
}

void GrayDarkness(const SkillInput& input, SkillOutput& output) {
    output.Duration = (10 + input.INTE / 8) * 10;
    output.Delay = max(3, 10 - input.DEX / 50) * 10;
    //	output.Range    = (  3 + (input.SkillLevel-15) / 25);
}

void StoneSkin(const SkillInput& input, SkillOutput& output) {
    output.Duration = min(60, 10 + input.STR / 10 + input.INTE / 6) * 10;
    output.Delay = min(60, 10 + input.STR / 10 + input.INTE / 6) * 10;
    output.Damage = min(30, 20 + input.INTE / 20);
}

void TalonOfCrow(const SkillInput& input, SkillOutput& output) {
    output.Delay = 6;
    output.Damage = input.STR / 6 + input.INTE / 30;
}

void Howl(const SkillInput& input, SkillOutput& output) {
    output.Duration = min(120, 30 + input.STR / 4 + input.DEX / 3 + input.INTE / 6) * 10;
    output.Range = min(100, 60 + input.DEX / 20);
}

void AcidEruption(const SkillInput& input, SkillOutput& output) {
    output.Damage = min(180, (input.STR / 6) + (input.INTE / 2) + (input.DEX / 10));
    output.Delay = max(3, 6 - (input.DEX / 50)) * 10;
}

void Teleport(const SkillInput& input, SkillOutput& output) {
    output.Range = min(6, 3 + input.SkillLevel / 10);
    output.Delay = max(1, 5 - (input.DEX / 60) - (input.INTE / 60)) * 10;
}

void FirePiercing(const SkillInput& input, SkillOutput& output) {
    if (input.SkillLevel <= 15) {
        output.Damage = 30 + (input.INTE / 20 + (3 + input.SkillLevel / 4));
    } else {
        output.Damage = 30 + (input.INTE / 20 + (2 + input.SkillLevel / 3));
    }

    if (input.SkillLevel == 30)
        output.Damage = (int)(output.Damage * 1.2);

    output.Delay = 0;
}

void SoulRebirth(const SkillInput& input, SkillOutput& output) {
    output.Delay = (8 - input.SkillLevel / 10) * 10;
    output.Range = 2 + input.SkillLevel / 10;

    if (input.SkillLevel <= 15) {
        output.Tick = 20 + input.SkillLevel * 2;
    } else {
        output.Tick = 50 + input.SkillLevel / 2;
    }
}

void IceLance(const SkillInput& input, SkillOutput& output) {
    if (input.SkillLevel <= 15) {
        output.Damage = (int)(28 + (input.INTE / 25) * (1.2 + (input.SkillLevel / 12.0)));
    } else {
        output.Damage = (int)(28 + (input.INTE / 22) * (1.0 + (input.SkillLevel / 13.0)));
    }

    output.Range = 4 + input.SkillLevel / 10;
}

void ExplosionWater(const SkillInput& input, SkillOutput& output) {
    if (input.SkillLevel <= 15) {
        output.Damage = (int)(20 + (input.INTE / 30.0) + (3.0 + (input.SkillLevel / 4.0)));
    } else {
        output.Damage = (int)(20 + (input.INTE / 23.0) + (3.0 + (input.SkillLevel / 3.0)));
    }

    output.Range = 4 + input.SkillLevel / 10;
    output.Duration = (2 + (input.SkillLevel / 10)) * 10;
    output.Delay = (15 - (input.SkillLevel / 6)) * 10;
    output.Tick = 40 - (input.SkillLevel / 3);
}

void FrozenArmor(const SkillInput& input, SkillOutput& output) {
    if (input.SkillLevel <= 15) {
        output.Damage = (int)min(20, (int)(10.0 + (input.INTE / 40.0) * (1.0 + ((float)(input.SkillLevel) / 15.0))));
        output.Tick =
            max(20, (int)min(15, (int)(5.0 + (input.INTE / 30.0) * (1.0 + ((float)(input.SkillLevel) / 22.5)))) * 10);
    } else {
        output.Damage = (int)min(20, (int)(10.0 + (input.INTE / 40.0) * (1.5 + ((float)(input.SkillLevel) / 30.0))));
        //		if ( input.SkillLevel == 30 ) output.Damage = (int)(output.Damage * 1.1);
        output.Tick = max(
            20, (int)min(15, (int)(5.0 + (input.INTE / 30.0) * (4.0 / 3.0 + ((float)(input.SkillLevel) / 45.0)))) * 10);
    }

    output.Duration = (int)((20.0 + (input.INTE / 20.0) * (1.0 + (float)(input.SkillLevel) / 20.0)) * 10);
    if (input.SkillLevel == 30)
        output.Duration = (int)(output.Duration * 1.1);
    output.Duration = min(600, output.Duration);
    output.Delay = output.Duration;
    output.Range = 1;
}

void ReactiveArmor(const SkillInput& input, SkillOutput& output) {
    if (input.SkillLevel <= 15) {
        output.Damage = (int)min(30, (int)(5.0 + (input.INTE / 30.0) * (1.1 + ((float)(input.SkillLevel) / 20.0))));
    } else {
        output.Damage = (int)min(45, (int)(5.0 + (input.INTE / 30.0) * (1.0 + ((float)(input.SkillLevel) / 15.0))));
        //		if ( input.SkillLevel == 30 ) output.Damage = (int)(output.Damage * 1.1);
    }

    output.Duration = (int)((20.0 + (input.INTE / 20.0) * (1.0 + (float)(input.SkillLevel) / 4.0)) * 10);
    //	if ( input.SkillLevel == 30 ) output.Duration = (int)(output.Duration * 1.1);
    output.Duration = min(1800, output.Duration);
    output.Delay = max(2, 5 - (input.INTE - 20) / 10) * 10;
}


void MagnumSpear(const SkillInput& input, SkillOutput& output) {
    if (input.SkillLevel <= 15) {
        output.Damage = (int)(40.0 + (input.INTE / 10.0) + (3.0 + (float)(input.SkillLevel) / 4.0));
    } else {
        output.Damage = (int)(40.0 + (input.INTE / 6.0) + (3.0 + (float)(input.SkillLevel) / 3.0));
    }

    output.Range = (int)(4.0 + (float)(input.SkillLevel) / 10);
}

void HellFire(const SkillInput& input, SkillOutput& output) {
    if (input.SkillLevel <= 15) {
        output.Damage = (int)(30.0 + (input.INTE / 8.0) + (1.0 + (float)(input.SkillLevel) / 2.0));
        //		output.Duration = (int)max(10, (int)min(100.0, ( ( input.INTE / 30.0 ) * ( 1.0 + (
        //(float)(input.SkillLevel) / 22.5 ) ) + 5.0 ) * 10));
        output.Duration = (int)((6.0 + (input.INTE / 60.0) + (input.SkillLevel / 15.0)) * 10);
        output.Range =
            max(20, min(150, (int)(((input.INTE / 30.0) * (1.0 + ((float)(input.SkillLevel) / 22.5)) + 5.0) * 10)));
    } else {
        output.Damage = (int)(30.0 + (input.INTE / 6.0) + (1.0 + (float)(input.SkillLevel)));
        //		output.Duration = (int)max(10, (int)min(100.0, ( ( input.INTE / 30.0 ) * ( 4.0/3.0 + (
        //(float)(input.SkillLevel) / 45.0 ) ) + 5.0 ) * 10));
        output.Duration = (int)((6.0 + (input.INTE / 60.0) + (input.SkillLevel / 10.0)) * 10);
        output.Range =
            max(20, min(150, (int)(((input.INTE / 30.0) * (1.0 + ((float)(input.SkillLevel) / 45.0)) + 5.0) * 10)));
    }

    // ToHit is used as Speed Damage
    output.ToHit = (int)(5.0 + (input.INTE / 50.0) * (1.0 + (input.SkillLevel / 15.0)));
    output.Delay = (int)max(40.0, (output.Duration * 1.2 - (input.SkillLevel * 10)));
    output.Tick = 5;
}

void GroundBless(const SkillInput& input, SkillOutput& output) {
    if (input.SkillLevel <= 15) {
        output.Damage = (int)(1.0 + (input.STR + input.DEX + input.INTE) / 100.0 + 0.5 + input.SkillLevel / 10.0);
    } else {
        output.Damage = (int)(1.0 + (input.STR + input.DEX + input.INTE) / 100.0 + 1.0 + input.SkillLevel / 12.0);
    }

    output.Duration = (int)((20.0 + (input.INTE / 20.0) * (1.0 + (float)(input.SkillLevel) / 4.0)) * 10);
    output.Duration = min(1800, output.Duration);
    output.Delay = max(2, 5 - (input.INTE - 20) / 10) * 10;
}

void SharpChakram(const SkillInput& input, SkillOutput& output) {
    if (input.SkillLevel <= 15) {
        output.Damage = 11 + input.SkillLevel / 6;
    } else {
        output.Damage = 11 + input.SkillLevel / 4;
        //		if ( input.SkillLevel == 30 ) output.Damage = (int)(output.Damage * 1.1);
    }

    output.Duration = 450 + input.DEX / 2 + input.SkillLevel * 15;
    output.Duration = min(900, output.Duration);
    if (input.SkillLevel == 30)
        output.Duration = (int)(output.Duration * 1.1);
    output.Delay = output.Duration;
}

void DestructionSpear(const SkillInput& input, SkillOutput& output) {
    if (input.SkillLevel <= 15) {
        output.Damage = min(60, 15 + (input.DEX / 20) + (input.SkillLevel / 3));
        output.Duration = max(20, min(150, (int)((5.0 + (input.DEX / 30.0) * (1.0 + (input.SkillLevel / 22.5))) * 10)));
    } else {
        output.Damage = min(60, 15 + (input.DEX / 20) + (input.SkillLevel / 2));
        output.Duration =
            max(20, min(150, (int)((5.0 + (input.DEX / 30.0) * (4.0 / 3.0 + (input.SkillLevel / 45.0))) * 10)));
    }

    output.Delay = 30 - input.DEX / 6 - input.SkillLevel;
    output.Delay = max(output.Delay, 6);
}

void ShiftBreak(const SkillInput& input, SkillOutput& output) {
    if (input.SkillLevel <= 15) {
        output.Damage = 5 + input.STR / 20 + input.DEX / 20 + input.SkillLevel / 3;
    } else {
        output.Damage = 5 + input.STR / 20 + input.DEX / 20 + input.SkillLevel / 2;
    }

    if (input.SkillLevel == 30)
        output.Damage = (int)(output.Damage * 1.1);

    output.Delay = max(6, (int)(30 - (input.DEX / 12) - (input.SkillLevel / 1.5)));
    output.Range = 1;
}

void FatalSnick(const SkillInput& input, SkillOutput& output) {
    if (input.SkillLevel <= 15) {
        output.Damage = min(160, 20 + (int)((input.STR / 10.0) * (1.0 + (input.SkillLevel / 11.25))));
    } else {
        output.Damage = min(160, 20 + (int)((input.STR / 10.0) * (5.0 / 3.0 + (input.SkillLevel / 22.5))));
        if (input.SkillLevel == 30)
            output.Damage = (int)(output.Damage * 1.1);
    }
    //	output.Delay = max(20, (4 - (input.DEX/60)) * 10 );
    output.Delay = 10;
    output.Range = 1 + (input.SkillLevel / 15);
}

void ChargingAttack(const SkillInput& input, SkillOutput& output) {
    if (input.SkillLevel <= 15) {
        output.Damage = 20 + (int)((input.STR / 10.0) * (1.0 + (input.SkillLevel / 11.25)));
    } else {
        output.Damage = 20 + (int)((input.STR / 10.0) * (5.0 / 3.0 + (input.SkillLevel / 22.5)));
        if (input.SkillLevel == 30)
            output.Damage = (int)(output.Damage * 1.1);
    }

    output.Delay = max(50, (10 - (input.DEX / 60) - (input.SkillLevel / 10)) * 10);
    output.Range = 3 + (input.SkillLevel / 10);
}

void DuckingWallop(const SkillInput& input, SkillOutput& output) {
    if (input.SkillLevel <= 15) {
        output.Damage = (int)(20 + (input.DEX / 10) * (1 + (input.SkillLevel / 11.25)));
    } else {
        output.Damage = (int)(20 + (input.DEX / 10) * (5.0 / 3.0 + (input.SkillLevel / 22.5)));
    }

    output.Delay = max(20, 100 - (input.DEX / 6) - input.SkillLevel);
}

void DistanceBlitz(const SkillInput& input, SkillOutput& output) {
    if (input.SkillLevel <= 15) {
        output.Damage = (int)(20 + (input.STR + input.DEX) / 30 * (1.0 + (input.SkillLevel / 15.0)));
    } else {
        output.Damage = (int)(20 + (input.STR + input.DEX) / 30 * (5.0 / 3.0 + (input.SkillLevel / 10.0)));
    }

    output.Delay = (int)((3 - (input.SkillLevel / 15.0)) * 10);
    output.Delay = max(10, output.Delay);
}

void SummonGroundElemental(const SkillInput& input, SkillOutput& output) {
    output.Duration = (10 + input.INTE / 8 + input.SkillLevel) * 10;
    output.Delay = output.Duration;
}

void SummonFireElemental(const SkillInput& input, SkillOutput& output) {
    output.Duration = (10 + input.INTE / 8 + input.SkillLevel * 2) * 10;
    output.Duration = min(output.Duration, 1200);
    output.Delay = output.Duration;

    if (input.SkillLevel <= 15) {
        output.Damage = 12 + (input.INTE / 20) + (input.SkillLevel / 4);
    } else {
        output.Damage = 12 + (input.INTE / 15) + (input.SkillLevel / 3);
        if (input.SkillLevel == 30)
            output.Damage = (int)(output.Damage * 1.2);
    }

    output.Range = 4 + input.SkillLevel / 10;
}

void SummonWaterElemental(const SkillInput& input, SkillOutput& output) {
    output.Duration = (5 + input.INTE / 8 + input.SkillLevel * 2) * 10;
    output.Duration = min(output.Duration, 1200);
    output.Delay = output.Duration;

    if (input.SkillLevel <= 15) {
        output.Damage = (int)(8 + (input.INTE / 30.0) * (1 + input.SkillLevel / 45.0));
        output.Damage = min(output.Damage, 20);
    } else {
        output.Damage = (int)(8 + (input.INTE / 40.0) * (1.5 + input.SkillLevel / 30.0));
        output.Damage = min(output.Damage, 30);
        if (input.SkillLevel == 30)
            output.Damage = (int)(output.Damage * 1.1);
    }
}

void MeteorStorm(const SkillInput& input, SkillOutput& output) {
    if (input.SkillLevel <= 15) {
        output.Damage = 50 + (int)(((input.INTE / 10.0) + (1.0 + (input.SkillLevel / 6.0))));
    } else {
        output.Damage = 50 + (int)(((input.INTE / 5.0) + (1 + (input.SkillLevel / 3.0))));
        if (input.SkillLevel == 30)
            output.Damage = (int)(output.Damage * 1.1);
    }

    output.Delay = (8 - (input.SkillLevel / 5)) * 10;
}

void WideIceField(const SkillInput& input, SkillOutput& output) {
    (void)input;
    output.Duration = 200;
    output.Range = 200;
    output.Tick = 5;
}

void Glacier1(const SkillInput& input, SkillOutput& output) {
    (void)input;
    output.Duration = 50;
}

void Glacier2(const SkillInput& input, SkillOutput& output) {
    (void)input;
    output.Duration = 100;
}

void IceAuger(const SkillInput& input, SkillOutput& output) {
    (void)input;
    output.Damage = 300;
    output.Delay = 10;
}

void IceHail(const SkillInput& input, SkillOutput& output) {
    (void)input;
    output.Duration = 30;
    output.Damage = 50;
    output.Tick = 5;
}

void WideIceHail(const SkillInput& input, SkillOutput& output) {
    (void)input;
    output.Duration = 50;
    output.Damage = 50;
    output.Tick = 5;
}

void IceWave(const SkillInput& input, SkillOutput& output) {
    (void)input;
    output.Damage = 320;
    output.Delay = 10;
}

void LandMineExplosion(const SkillInput& input, SkillOutput& output) {
    (void)input;
    output.Damage = 320;
    output.Delay = 10;
}

void ClaymoreExplosion(const SkillInput& input, SkillOutput& output) {
    (void)input;
    output.Damage = 320;
    output.Delay = 10;
}

void PleasureExplosion(const SkillInput& input, SkillOutput& output) {
    (void)input;
    (void)output;
}

void DeleoEfficio(const SkillInput& input, SkillOutput& output) {
    (void)input;
    output.Delay = 150;
}
void ReputoFactum(const SkillInput& input, SkillOutput& output) {
    (void)input;
    output.Duration = 300;
    output.Delay = 200;
}

void SwordOfThor(const SkillInput& input, SkillOutput& output) {
    output.Damage = 80 + input.STR / 2 + input.SkillLevel / 2;
    output.Delay = 150;
    output.Duration = 150 + input.SkillLevel;
}

void BurningSolCharging(const SkillInput& input, SkillOutput& output) {
    (void)input;
    output.Delay = 0;
}

void BurningSolLaunch(const SkillInput& input, SkillOutput& output) {
    output.Delay = 0;

    switch (input.Range) {
    case 0:
        output.Damage = 35 + (input.STR / 6) + (input.SkillLevel / 3);
        break;
    case 1:
        output.Damage = 70 + (input.STR / 6) + (input.SkillLevel / 3);
        break;
    case 2:
        output.Damage = 120 + (input.STR / 5) + (input.SkillLevel / 2);
        break;
    default:
        output.Damage = 200 + (input.STR / 5) + (input.SkillLevel / 2);
        break;
    }
}

void SweepVice(const SkillInput& input, SkillOutput& output) {
    output.Delay = 20;

    switch (input.Range) {
    case 0:
        output.Damage = 40 + (input.INTE / 5) + (input.SkillLevel / 3);
        break;
    case 1:
        output.Damage = 30 + (input.INTE / 5) + (input.SkillLevel / 3);
        break;
    case 2:
    default:
        output.Damage = 20 + (input.INTE / 5) + (input.SkillLevel / 3);
        break;
    }
}

void Whitsuntide(const SkillInput& input, SkillOutput& output) {
    output.Damage = 50 + (input.SkillLevel / 2);
    output.Range = 20 + (input.SkillLevel / 10);
    //	output.Delay = 80 - input.SkillLevel/3;
    output.Delay = 100 - input.SkillLevel / 5;
    output.Duration = 300;
}

void ViolentPhantom(const SkillInput& input, SkillOutput& output) {
    output.Damage = 5 + input.STR / 30 + input.DEX / 60;
    output.Delay = 0;
}

void InstallTurret(const SkillInput& input, SkillOutput& output) {
    output.Damage = 8 + (input.SkillLevel / 7);
    output.ToHit = 40 + (input.SkillLevel / 10);
    //	output.Delay = 330 + input.SkillLevel*2;
    output.Delay = 100 - input.SkillLevel / 33 * 10;
    output.Duration = 900 + input.SkillLevel * 20;
}

void TurretFire(const SkillInput& input, SkillOutput& output) {
    output.ToHit = 0;
    output.Damage = -10 + (input.SkillLevel / 5);
}

void SummonGoreGland(const SkillInput& input, SkillOutput& output) {
    output.Duration = input.INTE * 10 / 25 + 50;
    output.Delay = output.Duration;
}

void GoreGlandFire(const SkillInput& input, SkillOutput& output) {
    output.Damage = 20 + input.INTE / 10;
    output.Damage = min(80, output.Damage);
}


void PlayingWithFire(const SkillInput& input, SkillOutput& output) {
    (void)input;
    output.Damage = 400;
    output.Duration = 20; // explodes a few seconds later
    output.Delay = 0;
}

void InfinityThunderbolt(const SkillInput& input, SkillOutput& output) {
    //	output.Damage = 40 + (input.SkillLevel/2);
    output.Damage = 10 + (input.SkillLevel / 5);
    output.Duration = 0;
    output.Delay = 4;
}

void SpitStream(const SkillInput& input, SkillOutput& output) {
    output.Damage = 30 + (input.STR / 10) + (input.SkillLevel / 5);
    output.Duration = 4; // explodes a few seconds later
    output.Delay = 0;
}


void PlasmaRocketLauncher(const SkillInput& input, SkillOutput& output) {
    output.Damage = (int)(30 + (input.SkillLevel / 1.5));
    output.ToHit = 10 + (input.STR / 10) + (input.SkillLevel / 5);
    output.Duration = (input.Range - 1) * 3;
    output.Delay = 0;
}

void BombingStar(const SkillInput& input, SkillOutput& output) {
    output.Damage = 40 + (input.INTE / 5) + (input.SkillLevel / 2);
    output.Delay = 20;
}

void IntimateGrail(const SkillInput& input, SkillOutput& output) {
    if (input.TargetType == SkillInput::TARGET_SELF)
        output.Duration = 90 + (input.INTE / 5) + (input.SkillLevel / 2);
    else
        output.Duration = (int)(10 + (input.INTE / 100.0) + (input.SkillLevel / 15.0));

    output.Duration *= 10;

    output.Delay = 15 - (input.SkillLevel / 30);
    output.Delay *= 10;
    output.Range = 4 + (input.SkillLevel / 50);
}

void NooseOfWraith(const SkillInput& input, SkillOutput& output) {
    output.Damage = min(30 + (input.INTE / 3), 200);
    output.Delay = 18;
    output.Range = 7;
}

void SetAfire(const SkillInput& input, SkillOutput& output) {
    output.Delay = 0;
    // output.Damage = 10 + (input.STR/8) + (input.DEX/12);
    output.Damage = 20 + (input.STR / 6) + (input.DEX / 10);
}

void SharpHail(const SkillInput& input, SkillOutput& output) {
    output.Delay = 20;

    if (input.SkillLevel <= 15) {
        output.Damage = (int)(30 + ((input.DEX + input.STR) / 20.0) * (1 + (input.SkillLevel / 15.0)));
    } else {
        output.Damage = (int)(30 + ((input.DEX + input.STR) / 20.0) * (5.0 / 3.0 + (input.SkillLevel / 10.0)));
    }
    output.Tick = 3;
    output.Duration = 10;
}
void IceHorizon(const SkillInput& input, SkillOutput& output) {
    output.Duration = (5 + (input.INTE / 25) + (input.SkillLevel / 3)) * 10;
    output.Delay = output.Duration;
    output.Tick = 50;

    if (input.SkillLevel <= 15) {
        output.Damage = min(150, (int)(20 + (input.INTE / 3.0) + (3 + (input.SkillLevel / 4.0))));
        ;
    } else {
        output.Damage = min(200, (int)(20 + (input.INTE / 2.0) + (3 + (input.SkillLevel / 3.0))));
    }
}

void FuryOfGnome(const SkillInput& input, SkillOutput& output) {
    if (input.SkillLevel <= 15) {
        output.Duration = min(10, (int)(5 * (1 + (input.SkillLevel / 22.5))));
        output.Damage = (int)(70 + (input.INTE / 10.0) + (1 + (input.SkillLevel / 6.0)));
    } else {
        output.Duration = min(10, (int)(5 * (4.0 / 3.0 + (input.SkillLevel / 45.0))));
        output.Damage = (int)(70 + (input.INTE / 5.0) + (1 + (input.SkillLevel / 3.0)));
    }
    output.Duration *= 10;
    output.Delay = (output.Duration) - (input.SkillLevel);
}

void SummonMiga(const SkillInput& input, SkillOutput& output) {
    output.Duration = 15 + (input.SkillLevel / 2);
    output.Duration *= 10;

    output.Delay = 80 - input.SkillLevel;
}

void SummonMigaAttack(const SkillInput& input, SkillOutput& output) {
    if (input.DEX <= 15) {
        output.Damage = 40 + (int)((input.INTE / 8.0) + (1 + (((float)input.DEX) / 6.0)));
    } else {
        output.Damage = 40 + (int)((input.INTE / 6.0) + (1 + (((float)input.DEX) / 3.0)));
    }
}


void ARAttack(const SkillInput& input, SkillOutput& output) {
    (void)input;
    output.Delay = 0;
    output.Damage = 0;
}

void SMGAttack(const SkillInput& input, SkillOutput& output) {
    (void)input;
    output.Delay = 0;
    output.Damage = 0;
}

void GrenadeAttack(const SkillInput& input, SkillOutput& output) {
    (void)input;
    output.Duration = 10;
    output.Damage = 80;
}

void Halo(const SkillInput& input, SkillOutput& output) {
    output.Damage = min(200, 80 + (input.STR / 7) + (min(input.Range, 10) * 5));
    output.Range = 60 + (min(input.Range, 10) * 3);
    output.Duration = (int)((5 + (input.DEX / 100.0) * 3) * 10);
    output.Delay = output.Duration;
}

void Destinies(const SkillInput& input, SkillOutput& output) {
    output.Damage = 70 + (input.INTE / 10) + (min(input.Range, 10) * 4);
    output.Delay = 100;
}

void FierceFlame(const SkillInput& input, SkillOutput& output) {
    output.Damage = 50 + (input.INTE / 30) + min(input.Range, 10);
    output.Duration = (5 + (min(input.Range, 10) / 2)) * 10;
    output.Delay = output.Duration;
}

void ShadowOfStorm(const SkillInput& input, SkillOutput& output) {
    output.Damage = min(80, 40 + (input.INTE / 30) + min(input.Range, 10));
    output.Duration = 50 + (min(input.Range, 10) * 5);
    output.Delay = 50;
}
// Wild Wolf
void WildWolf(const SkillInput& input, SkillOutput& output) {
    output.Damage = (input.DEX / 8) + (input.STR / 30) + min(input.Range, 10) * 2;
    output.Delay = max(5, (10 - (input.DEX / 200))) * 10;
}

void Aberration(const SkillInput& input, SkillOutput& output) {
    output.Duration = (10 + input.INTE / 40 + min(input.Range, 10) * 2) * 10;
    output.Delay = output.Duration;
    output.Range = min(40, 30 + input.INTE / 40);
}

void DragonTornado(const SkillInput& input, SkillOutput& output) {
    output.Damage = 30 + (input.STR / 15) + (min(input.Range, 10) * 2);
    output.Range = min(200, 80 + (input.STR / 10) + (min(input.Range, 10) * 4));
    output.Tick = min(200, 50 + (input.STR / 10) + (min(input.Range, 10) * 4));
    output.Duration = (20 + min(input.Range, 10)) * 10;
    output.Delay = 300;
}

void BikeCrash(const SkillInput& input, SkillOutput& output) {
    output.Damage = (20 + (input.STR / 15) + (min(input.Range, 10) * 2)) * 3;
    output.Delay = (10 - min(input.Range, 10) / 5) * 10;
}

void HarpoonBomb(const SkillInput& input, SkillOutput& output) {
    output.Damage = -25 + (input.STR / 15) + (min(input.Range, 10) * 4);
    output.Delay = 0;
    output.Range = 30 + min(input.Range, 10) * 2;
    output.Tick = 100 + (min(input.Range, 10) * 5);
    output.Duration = (int)((4 - (min(input.Range, 10) / 10.0)) * 10);
}

void PassingHeal(const SkillInput& input, SkillOutput& output) {
    output.Damage = 80 + (min(input.Range, 10) * 4);
    output.Delay = (3 - (min(input.Range, 10) / 10)) * 10;
    output.Range = 5 + (min(input.Range, 10) / 5);
    output.Duration = 0;
}

void RottenApple(const SkillInput& input, SkillOutput& output) {
    output.Delay = (8 - (min(input.Range, 10) / 3)) * 10;
}

// add by coffee 2007-2-17
// Blood Brand
void BloodyScarify(const SkillInput& input, SkillOutput& output) {
    output.Damage = (input.DEX / 5) + (input.STR / 25) + min(input.Range, 10) * 2;
    output.Delay = max(5, (10 - (input.DEX / 200))) * 10;
}
// Blood Curse


// Shining Sword
void ShineSword(const SkillInput& input, SkillOutput& output) {
    output.Damage = (20 + (input.STR / 15) + (min(input.Range, 10) * 2)) * 3;
    output.Delay = (4 - min(input.Range, 10) / 5) * 10;
    output.Duration = 20; // delay before the effect shows
}

// Cannon Bombardment
void BombCrashWalk(const SkillInput& input, SkillOutput& output) {
    output.Delay = 10;
    output.Damage = (input.STR / 8) + (input.SkillLevel / 3);
    output.Duration = 7; // 1 sec
    output.Tick = 1;     // 1 sec
}
// Satellite Strike (human gunner new skill)
void SatelliteBomb(const SkillInput& input, SkillOutput& output) {
    output.Damage = 150 + input.SkillLevel / 2;
    output.Duration = 6; // 0.6 sec
    output.Delay = 35 - input.SkillLevel * 10 / 33;
    output.Range = 8;
}

// Terror Hallucination (human doctor new skill)
void IllusionInversion(const SkillInput& input, SkillOutput& output) {
    output.Damage = max(200, ((input.INTE / 8) * (1 + (input.SkillLevel / 33))));
    // output.Damage = (( input.INTE / 5 )* 0.7) * ( 1 + ( input.SkillLevel / 33 ) );
    output.Delay = 170 - input.SkillLevel * 10 / 33;
    output.Range = 5;
}
// Heaven's Descent (human blessing new skill)
void HeavenGround(const SkillInput& input, SkillOutput& output) {
    //	output.Duration = (10 + input.INTE/20 + input.SkillLevel/10)*10;
    output.Duration = (10 + input.INTE / 20 + input.SkillLevel / 6) * 9;
    // cout << "Heaven's Descent delay:" << (int)output.Duration << endl;

    output.Delay = output.Duration; //(10 - input.SkillLevel/33) * 10;
    // edit by Coffee 2007-5-8
    output.Damage = min(80, (input.INTE / 18) * (1 + (input.SkillLevel / 33)));
    output.Tick = 20;
}
// Drake Puppet (Ousters, fire magic)
void DummyDrake(const SkillInput& input, SkillOutput& output) {
    output.Damage = 250 + (input.INTE / 50) + min(input.Range, 10);
    output.Duration = (10 + (min(input.Range, 10) / 2)) * 5;
    output.Delay = output.Duration;
}
// Composite Water Heal (water magic)
void HydroConvergence(const SkillInput& input, SkillOutput& output) {
    output.Damage = 20 + (input.INTE / 50) + min(input.Range, 10);
    output.Duration = (10 + (min(input.Range, 10) / 2)) * 5;
    output.Delay = output.Duration;
}
// Clay Summon (earth magic)
void SummonClay(const SkillInput& input, SkillOutput& output) {
    // output.Damage = 20 + (input.INTE/50) + min(input.Range,10);
    output.Duration = ((input.INTE / 30) + (min(input.Range, 10) / 2)) * 13;
    // cout << "Clay Summon:" << (int)output.Duration << endl;
    output.Delay = output.Duration;
}
// Heter Chakram (magic warrior)
void HeterChakram(const SkillInput& input, SkillOutput& output) {
    output.Damage = min(200, 140 + (input.STR / 5) + (min(input.Range, 10) * 5));
    output.Range = 60 + (min(input.Range, 10) * 3);
    output.Duration = 18;
    output.Delay = 70;
}

void SkyFire(const SkillInput& input, SkillOutput& output) {
    output.Damage = 60 + (int)((input.Range - 1) / 10) * 10;
    output.Duration = 0;
    output.Delay = 4;
}

void CutStorm(const SkillInput& input, SkillOutput& output) {
    output.Damage = (20 + (input.STR / 15) + (min(input.Range, 10) * 2)) * 3;
    output.Delay = (4 - min(input.Range, 10) / 5) * 10;
    output.Duration = 20; // delay before the effect shows
}
void XRLMissile(const SkillInput& input, SkillOutput& output) {
    output.Damage = 150 + input.SkillLevel / 2;
    output.Duration = 6; // 0.6 sec
    output.Delay = 35 - input.SkillLevel * 10 / 33;
    output.Range = 8;
}
void SacredStamp(const SkillInput& input, SkillOutput& output) {
    output.Damage = max(200, ((input.INTE / 8) * (1 + (input.SkillLevel / 33))));
    // output.Damage = (( input.INTE / 5 )* 0.7) * ( 1 + ( input.SkillLevel / 33 ) );
    output.Delay = 170 - input.SkillLevel * 10 / 33;
    output.Range = 5;
}
void BrambleHalo(const SkillInput& input, SkillOutput& output) {
    output.Duration = (10 + input.INTE / 20 + input.SkillLevel / 6) * 9;
    output.Delay = output.Duration; //(10 - input.SkillLevel/33) * 10;
    output.Damage = min(80, (input.INTE / 18) * (1 + (input.SkillLevel / 33)));
    output.Tick = 20;
}
void DeadlyClaw(const SkillInput& input, SkillOutput& output) {
    output.Damage = (input.DEX / 40) + (input.STR / 20) + min(input.Range, 10);
    output.Delay = 0;
}
void PenetrateWheel(const SkillInput& input, SkillOutput& output) {
    output.Damage = (int)((30 + (input.DEX / 10.0)) * (11 + (int)((input.Range - 1) / 10)) / 10);
    output.Duration = max(20, min(150, (int)((5.0 + (input.DEX / 30.0) * 2.0) * 10.0)));
    output.Delay = 6;
}

void FireMeteor(const SkillInput& input, SkillOutput& output) {
    output.Damage = 250 + (input.INTE / 50) + min(input.Range, 10);
    output.Duration = (10 + (min(input.Range, 10) / 2)) * 5;
    output.Delay = output.Duration;
}
void BigRockfall(const SkillInput& input, SkillOutput& output) {
    output.Duration = ((input.INTE / 30) + (min(input.Range, 10) / 2)) * 13;
    output.Delay = output.Duration;
}
void RapidFreeze(const SkillInput& input, SkillOutput& output) {
    output.Damage = 20 + (input.INTE / 50) + min(input.Range, 10);
    output.Duration = (10 + (min(input.Range, 10) / 2)) * 5;
    output.Delay = output.Duration;
}

} // namespace skillformula
} // namespace decore
