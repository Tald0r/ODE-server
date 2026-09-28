//////////////////////////////////////////////////////////////////////////////
// Filename    : SkillOutputFormulas.h
// Description :
// de-core: the per-skill computeOutput formulas. Each function
// carries the name of the skill class whose computeOutput it is; the
// gameserver keeps those member functions as one-line adapters that
// delegate here (skill/SkillFormula.cpp).
//
// The structs mirror the gameserver's SkillInput/SkillOutput
// (skill/SkillHandler.h) field-for-field so the bodies move without edits:
// same field names, same enum values. Three impurities were externalized:
//  - DomainGrade replaces SkillInfoManager::getGradeByDomainLevel(
//    input.DomainLevel); only the three grade-using adapters fetch it
//    (ContinualLight, Purify, DetectInvisibility), preserving the
//    manager's out-of-range throw exactly where it could throw before.
//  - Gun (GunClass) replaces the Item::ItemClass comparisons; the adapter
//    maps the four gun classes and folds everything else to Other.
//  - HeadShot's Assert(false) on an unknown gun class fires in the
//    adapter, before delegation (observable state is identical: every
//    caller passes a freshly zeroed SkillOutput).
//
// The math is game balance: oddities (dead case-fallthroughs, negative
// damages, commented-out history) are preserved on purpose. The legacy
// comments arrived as double-encoded mojibake (EUC-KR -- and GBK for the
// 2007 Chinese-era additions -- read as Latin-1, re-saved as UTF-8); they
// were machine-recovered through that chain and translated to English.
//////////////////////////////////////////////////////////////////////////////

#ifndef DECORE_SKILL_OUTPUT_FORMULAS_H
#define DECORE_SKILL_OUTPUT_FORMULAS_H

namespace decore {
namespace skillformula {

// The formulas' view of the equipped gun for the four classes they branch
// on; Other covers every other ItemClass (the original comparisons simply
// failed for those).
enum class GunClass { SG, AR, SMG, SR, Other };

// Mirrors the game's SkillGrade (src/Core/types/CreatureTypes.h) values so
// the grade switches transplant verbatim and the adapter passes the game
// enum's integer value through unchanged.
enum SkillGrade {
    SKILL_GRADE_APPRENTICE = 0,
    SKILL_GRADE_ADEPT,
    SKILL_GRADE_EXPERT,
    SKILL_GRADE_MASTER,
    SKILL_GRADE_GRAND_MASTER
};

// Field-for-field mirror of the gameserver's SkillInput, minus the game
// types. DomainGrade is -1 unless the adapter fetched it (only the three
// grade-using formulas read it).
struct SkillInput {
    enum TargetType { TARGET_SELF = 0, TARGET_OTHER, TARGET_MAX };
    enum TargetRace { TARGET_PC = 0, TARGET_MONSTER };

    int SkillLevel;
    int DomainLevel;
    int DomainGrade;
    int STR;
    int DEX;
    int INTE;
    int TargetType;
    int Range;
    GunClass Gun;
    int PartySize;
};

// Field-for-field mirror of the gameserver's SkillOutput, zeroed like it.
struct SkillOutput {
    SkillOutput() : Damage(0), Duration(0), Tick(0), ToHit(0), Range(0), Delay(0) {}

    int Damage;
    int Duration;
    int Tick;
    int ToHit;
    int Range;
    int Delay;
};

void DoubleImpact(const SkillInput& input, SkillOutput& output);
void TripleSlasher(const SkillInput& input, SkillOutput& output);
void RainbowSlasher(const SkillInput& input, SkillOutput& output);
void ThunderSpark(const SkillInput& input, SkillOutput& output);
void DancingSword(const SkillInput& input, SkillOutput& output);
void CrossCounter(const SkillInput& input, SkillOutput& output);
void FlashSliding(const SkillInput& input, SkillOutput& output);
void LightningHand(const SkillInput& input, SkillOutput& output);
void SwordWave(const SkillInput& input, SkillOutput& output);
void SnakeCombo(const SkillInput& input, SkillOutput& output);
void WindDivider(const SkillInput& input, SkillOutput& output);
void ThunderBolt(const SkillInput& input, SkillOutput& output);
void Expansion(const SkillInput& input, SkillOutput& output);
void MiracleShield(const SkillInput& input, SkillOutput& output);
void ThunderFlash(const SkillInput& input, SkillOutput& output);
void ThunderStorm(const SkillInput& input, SkillOutput& output);
void MentalSword(const SkillInput& input, SkillOutput& output);
void SingleBlow(const SkillInput& input, SkillOutput& output);
void SpiralSlay(const SkillInput& input, SkillOutput& output);
void TripleBreak(const SkillInput& input, SkillOutput& output);
void WildSmash(const SkillInput& input, SkillOutput& output);
void GhostBlade(const SkillInput& input, SkillOutput& output);
void PotentialExplosion(const SkillInput& input, SkillOutput& output);
void ShadowWalk(const SkillInput& input, SkillOutput& output);
void ChargingPower(const SkillInput& input, SkillOutput& output);
void HurricaneCombo(const SkillInput& input, SkillOutput& output);
void TornadoSever(const SkillInput& input, SkillOutput& output);
void Earthquake(const SkillInput& input, SkillOutput& output);
void Berserker(const SkillInput& input, SkillOutput& output);
void MoonlightSever(const SkillInput& input, SkillOutput& output);
void ShadowDancing(const SkillInput& input, SkillOutput& output);
void Typhoon(const SkillInput& input, SkillOutput& output);
void QuickFire(const SkillInput& input, SkillOutput& output);
void DoubleShot(const SkillInput& input, SkillOutput& output);
void TripleShot(const SkillInput& input, SkillOutput& output);
void MultiShot(const SkillInput& input, SkillOutput& output);
void HeadShot(const SkillInput& input, SkillOutput& output);
void Piercing(const SkillInput& input, SkillOutput& output);
void Sniping(const SkillInput& input, SkillOutput& output);
void MindControl(const SkillInput& input, SkillOutput& output);
void Revealer(const SkillInput& input, SkillOutput& output);
void CreateBomb(const SkillInput& input, SkillOutput& output);
void CreateMine(const SkillInput& input, SkillOutput& output);
void InstallMine(const SkillInput& input, SkillOutput& output);
void DisarmMine(const SkillInput& input, SkillOutput& output);
void ObservingEye(const SkillInput& input, SkillOutput& output);
void CreateHolyWater(const SkillInput& input, SkillOutput& output);
void Light(const SkillInput& input, SkillOutput& output);
void DetectHidden(const SkillInput& input, SkillOutput& output);
void AuraBall(const SkillInput& input, SkillOutput& output);
void Bless(const SkillInput& input, SkillOutput& output);
void ContinualLight(const SkillInput& input, SkillOutput& output);
void Flare(const SkillInput& input, SkillOutput& output);
void Purify(const SkillInput& input, SkillOutput& output);
void AuraRing(const SkillInput& input, SkillOutput& output);
void Striking(const SkillInput& input, SkillOutput& output);
void DetectInvisibility(const SkillInput& input, SkillOutput& output);
void AuraShield(const SkillInput& input, SkillOutput& output);
void Visible(const SkillInput& input, SkillOutput& output);
void CureLightWounds(const SkillInput& input, SkillOutput& output);
void CureAll(const SkillInput& input, SkillOutput& output);
void CurePoison(const SkillInput& input, SkillOutput& output);
void ProtectionFromPoison(const SkillInput& input, SkillOutput& output);
void CauseLightWounds(const SkillInput& input, SkillOutput& output);
void CureSeriousWounds(const SkillInput& input, SkillOutput& output);
void RemoveCurse(const SkillInput& input, SkillOutput& output);
void ProtectionFromCurse(const SkillInput& input, SkillOutput& output);
void Resurrect(const SkillInput& input, SkillOutput& output);
void CauseSeriousWounds(const SkillInput& input, SkillOutput& output);
void CureCriticalWounds(const SkillInput& input, SkillOutput& output);
void ProtectionFromAcid(const SkillInput& input, SkillOutput& output);
void Sacrifice(const SkillInput& input, SkillOutput& output);
void CauseCriticalWounds(const SkillInput& input, SkillOutput& output);
void RegenerationSkill(const SkillInput& input, SkillOutput& output);
void EnergyDrop(const SkillInput& input, SkillOutput& output);
void VigorDrop(const SkillInput& input, SkillOutput& output);
void Activation(const SkillInput& input, SkillOutput& output);
void HolyBlast(const SkillInput& input, SkillOutput& output);
void Sanctuary(const SkillInput& input, SkillOutput& output);
void Reflection(const SkillInput& input, SkillOutput& output);
void Hymn(const SkillInput& input, SkillOutput& output);
void PoisonousHands(const SkillInput& input, SkillOutput& output);
void AcidTouch(const SkillInput& input, SkillOutput& output);
void GreenPoison(const SkillInput& input, SkillOutput& output);
void Darkness(const SkillInput& input, SkillOutput& output);
void YellowPoison(const SkillInput& input, SkillOutput& output);
void TransformToBat(const SkillInput& input, SkillOutput& output);
void SummonCasket(const SkillInput& input, SkillOutput& output);
void OpenCasket(const SkillInput& input, SkillOutput& output);
void AcidBolt(const SkillInput& input, SkillOutput& output);
void GreenStalker(const SkillInput& input, SkillOutput& output);
void BloodyTunnel(const SkillInput& input, SkillOutput& output);
void Paralyze(const SkillInput& input, SkillOutput& output);
void BloodyMarker(const SkillInput& input, SkillOutput& output);
void DarkBluePoison(const SkillInput& input, SkillOutput& output);
void TransformToWolf(const SkillInput& input, SkillOutput& output);
void Doom(const SkillInput& input, SkillOutput& output);
void AcidBall(const SkillInput& input, SkillOutput& output);
void Invisibility(const SkillInput& input, SkillOutput& output);
void AcidSwamp(const SkillInput& input, SkillOutput& output);
void Seduction(const SkillInput& input, SkillOutput& output);
void BloodyNail(const SkillInput& input, SkillOutput& output);
void BloodyKnife(const SkillInput& input, SkillOutput& output);
void BloodyBall(const SkillInput& input, SkillOutput& output);
void BloodyWave(const SkillInput& input, SkillOutput& output);
void BloodyMasterWave(const SkillInput& input, SkillOutput& output);
void BloodyWarp(const SkillInput& input, SkillOutput& output);
void BloodyWall(const SkillInput& input, SkillOutput& output);
void BloodySnake(const SkillInput& input, SkillOutput& output);
void BloodySpear(const SkillInput& input, SkillOutput& output);
void PoisonStrike(const SkillInput& input, SkillOutput& output);
void AcidStrike(const SkillInput& input, SkillOutput& output);
void BloodyStrike(const SkillInput& input, SkillOutput& output);
void PoisonStorm(const SkillInput& input, SkillOutput& output);
void AcidStorm(const SkillInput& input, SkillOutput& output);
void BloodyStorm(const SkillInput& input, SkillOutput& output);
void Extreme(const SkillInput& input, SkillOutput& output);
void Peace(const SkillInput& input, SkillOutput& output);
void Death(const SkillInput& input, SkillOutput& output);
void Mephisto(const SkillInput& input, SkillOutput& output);
void Transfusion(const SkillInput& input, SkillOutput& output);
void SummonMonsters(const SkillInput& input, SkillOutput& output);
void GroundAttack(const SkillInput& input, SkillOutput& output);
void Hallucination(const SkillInput& input, SkillOutput& output);
void SoulChain(const SkillInput& input, SkillOutput& output);
void SharpShield(const SkillInput& input, SkillOutput& output);
void WideLightning(const SkillInput& input, SkillOutput& output);
void GunShotGuidance(const SkillInput& input, SkillOutput& output);
void AirShield(const SkillInput& input, SkillOutput& output);
void BulletOfLight(const SkillInput& input, SkillOutput& output);
void HandsOfWisdom(const SkillInput& input, SkillOutput& output);
void LightBall(const SkillInput& input, SkillOutput& output);
void HolyArrow(const SkillInput& input, SkillOutput& output);
void Rebuke(const SkillInput& input, SkillOutput& output);
void SpiritGuard(const SkillInput& input, SkillOutput& output);
void Regeneration(const SkillInput& input, SkillOutput& output);
void PowerOfLand(const SkillInput& input, SkillOutput& output);
void TurnUndead(const SkillInput& input, SkillOutput& output);
void Armageddon(const SkillInput& input, SkillOutput& output);
void BloodyBreaker(const SkillInput& input, SkillOutput& output);
void RapidGliding(const SkillInput& input, SkillOutput& output);
void MagicElusion(const SkillInput& input, SkillOutput& output);
void PoisonMesh(const SkillInput& input, SkillOutput& output);
void IllusionOfAvenge(const SkillInput& input, SkillOutput& output);
void WillOfLife(const SkillInput& input, SkillOutput& output);
void DenialMagic(const SkillInput& input, SkillOutput& output);
void Requital(const SkillInput& input, SkillOutput& output);
void Concealment(const SkillInput& input, SkillOutput& output);
void SwordRay(const SkillInput& input, SkillOutput& output);
void MultiAmputate(const SkillInput& input, SkillOutput& output);
void HitConvert(const SkillInput& input, SkillOutput& output);
void WildTyphoon(const SkillInput& input, SkillOutput& output);
void UltimateBlow(const SkillInput& input, SkillOutput& output);
void Illendue(const SkillInput& input, SkillOutput& output);
void Lightness(const SkillInput& input, SkillOutput& output);
void Flourish(const SkillInput& input, SkillOutput& output);
void Evade(const SkillInput& input, SkillOutput& output);
void SharpRound(const SkillInput& input, SkillOutput& output);
void BackStab(const SkillInput& input, SkillOutput& output);
void Blunting(const SkillInput& input, SkillOutput& output);
void GammaChop(const SkillInput& input, SkillOutput& output);
void CrossGuard(const SkillInput& input, SkillOutput& output);
void KasasArrow(const SkillInput& input, SkillOutput& output);
void HandsOfFire(const SkillInput& input, SkillOutput& output);
void Prominence(const SkillInput& input, SkillOutput& output);
void RingOfFlare(const SkillInput& input, SkillOutput& output);
void BlazeBolt(const SkillInput& input, SkillOutput& output);
void IceField(const SkillInput& input, SkillOutput& output);
void WaterBarrier(const SkillInput& input, SkillOutput& output);
void NymphRecovery(const SkillInput& input, SkillOutput& output);
void Liberty(const SkillInput& input, SkillOutput& output);
void Tendril(const SkillInput& input, SkillOutput& output);
void StoneAuger(const SkillInput& input, SkillOutput& output);
void EarthsTeeth(const SkillInput& input, SkillOutput& output);
void HandsOfNizie(const SkillInput& input, SkillOutput& output);
void GnomesWhisper(const SkillInput& input, SkillOutput& output);
void RefusalEther(const SkillInput& input, SkillOutput& output);
void EmissionWater(const SkillInput& input, SkillOutput& output);
void BeatHead(const SkillInput& input, SkillOutput& output);
void DivineSpirits(const SkillInput& input, SkillOutput& output);
void BlitzSliding(const SkillInput& input, SkillOutput& output);
void JabbingVein(const SkillInput& input, SkillOutput& output);
void GreatHeal(const SkillInput& input, SkillOutput& output);
void DivineGuidance(const SkillInput& input, SkillOutput& output);
void BlazeWalk(const SkillInput& input, SkillOutput& output);
void BloodyZenith(const SkillInput& input, SkillOutput& output);
void Rediance(const SkillInput& input, SkillOutput& output);
void LarSlash(const SkillInput& input, SkillOutput& output);
void Trident(const SkillInput& input, SkillOutput& output);
void HeartCatalyst(const SkillInput& input, SkillOutput& output);
void ProtectionFromBlood(const SkillInput& input, SkillOutput& output);
void MoleShot(const SkillInput& input, SkillOutput& output);
void Eternity(const SkillInput& input, SkillOutput& output);
void InstallTrap(const SkillInput& input, SkillOutput& output);
void HolyArmor(const SkillInput& input, SkillOutput& output);
void MercyGround(const SkillInput& input, SkillOutput& output);
void CreateHolyPotion(const SkillInput& input, SkillOutput& output);
void TransformToWerwolf(const SkillInput& input, SkillOutput& output);
void GrayDarkness(const SkillInput& input, SkillOutput& output);
void StoneSkin(const SkillInput& input, SkillOutput& output);
void TalonOfCrow(const SkillInput& input, SkillOutput& output);
void Howl(const SkillInput& input, SkillOutput& output);
void AcidEruption(const SkillInput& input, SkillOutput& output);
void Teleport(const SkillInput& input, SkillOutput& output);
void FirePiercing(const SkillInput& input, SkillOutput& output);
void SoulRebirth(const SkillInput& input, SkillOutput& output);
void IceLance(const SkillInput& input, SkillOutput& output);
void ExplosionWater(const SkillInput& input, SkillOutput& output);
void FrozenArmor(const SkillInput& input, SkillOutput& output);
void ReactiveArmor(const SkillInput& input, SkillOutput& output);
void MagnumSpear(const SkillInput& input, SkillOutput& output);
void HellFire(const SkillInput& input, SkillOutput& output);
void GroundBless(const SkillInput& input, SkillOutput& output);
void SharpChakram(const SkillInput& input, SkillOutput& output);
void DestructionSpear(const SkillInput& input, SkillOutput& output);
void ShiftBreak(const SkillInput& input, SkillOutput& output);
void FatalSnick(const SkillInput& input, SkillOutput& output);
void ChargingAttack(const SkillInput& input, SkillOutput& output);
void DuckingWallop(const SkillInput& input, SkillOutput& output);
void DistanceBlitz(const SkillInput& input, SkillOutput& output);
void SummonGroundElemental(const SkillInput& input, SkillOutput& output);
void SummonFireElemental(const SkillInput& input, SkillOutput& output);
void SummonWaterElemental(const SkillInput& input, SkillOutput& output);
void MeteorStorm(const SkillInput& input, SkillOutput& output);
void WideIceField(const SkillInput& input, SkillOutput& output);
void Glacier1(const SkillInput& input, SkillOutput& output);
void Glacier2(const SkillInput& input, SkillOutput& output);
void IceAuger(const SkillInput& input, SkillOutput& output);
void IceHail(const SkillInput& input, SkillOutput& output);
void WideIceHail(const SkillInput& input, SkillOutput& output);
void IceWave(const SkillInput& input, SkillOutput& output);
void LandMineExplosion(const SkillInput& input, SkillOutput& output);
void ClaymoreExplosion(const SkillInput& input, SkillOutput& output);
void PleasureExplosion(const SkillInput& input, SkillOutput& output);
void DeleoEfficio(const SkillInput& input, SkillOutput& output);
void ReputoFactum(const SkillInput& input, SkillOutput& output);
void SwordOfThor(const SkillInput& input, SkillOutput& output);
void BurningSolCharging(const SkillInput& input, SkillOutput& output);
void BurningSolLaunch(const SkillInput& input, SkillOutput& output);
void SweepVice(const SkillInput& input, SkillOutput& output);
void Whitsuntide(const SkillInput& input, SkillOutput& output);
void ViolentPhantom(const SkillInput& input, SkillOutput& output);
void InstallTurret(const SkillInput& input, SkillOutput& output);
void TurretFire(const SkillInput& input, SkillOutput& output);
void SummonGoreGland(const SkillInput& input, SkillOutput& output);
void GoreGlandFire(const SkillInput& input, SkillOutput& output);
void PlayingWithFire(const SkillInput& input, SkillOutput& output);
void InfinityThunderbolt(const SkillInput& input, SkillOutput& output);
void SpitStream(const SkillInput& input, SkillOutput& output);
void PlasmaRocketLauncher(const SkillInput& input, SkillOutput& output);
void BombingStar(const SkillInput& input, SkillOutput& output);
void IntimateGrail(const SkillInput& input, SkillOutput& output);
void NooseOfWraith(const SkillInput& input, SkillOutput& output);
void SetAfire(const SkillInput& input, SkillOutput& output);
void SharpHail(const SkillInput& input, SkillOutput& output);
void IceHorizon(const SkillInput& input, SkillOutput& output);
void FuryOfGnome(const SkillInput& input, SkillOutput& output);
void SummonMiga(const SkillInput& input, SkillOutput& output);
void SummonMigaAttack(const SkillInput& input, SkillOutput& output);
void ARAttack(const SkillInput& input, SkillOutput& output);
void SMGAttack(const SkillInput& input, SkillOutput& output);
void GrenadeAttack(const SkillInput& input, SkillOutput& output);
void Halo(const SkillInput& input, SkillOutput& output);
void Destinies(const SkillInput& input, SkillOutput& output);
void FierceFlame(const SkillInput& input, SkillOutput& output);
void ShadowOfStorm(const SkillInput& input, SkillOutput& output);
void WildWolf(const SkillInput& input, SkillOutput& output);
void Aberration(const SkillInput& input, SkillOutput& output);
void DragonTornado(const SkillInput& input, SkillOutput& output);
void BikeCrash(const SkillInput& input, SkillOutput& output);
void HarpoonBomb(const SkillInput& input, SkillOutput& output);
void PassingHeal(const SkillInput& input, SkillOutput& output);
void RottenApple(const SkillInput& input, SkillOutput& output);
void BloodyScarify(const SkillInput& input, SkillOutput& output);
void ShineSword(const SkillInput& input, SkillOutput& output);
void BombCrashWalk(const SkillInput& input, SkillOutput& output);
void SatelliteBomb(const SkillInput& input, SkillOutput& output);
void IllusionInversion(const SkillInput& input, SkillOutput& output);
void HeavenGround(const SkillInput& input, SkillOutput& output);
void DummyDrake(const SkillInput& input, SkillOutput& output);
void HydroConvergence(const SkillInput& input, SkillOutput& output);
void SummonClay(const SkillInput& input, SkillOutput& output);
void HeterChakram(const SkillInput& input, SkillOutput& output);
void SkyFire(const SkillInput& input, SkillOutput& output);
void CutStorm(const SkillInput& input, SkillOutput& output);
void XRLMissile(const SkillInput& input, SkillOutput& output);
void SacredStamp(const SkillInput& input, SkillOutput& output);
void BrambleHalo(const SkillInput& input, SkillOutput& output);
void DeadlyClaw(const SkillInput& input, SkillOutput& output);
void PenetrateWheel(const SkillInput& input, SkillOutput& output);
void FireMeteor(const SkillInput& input, SkillOutput& output);
void BigRockfall(const SkillInput& input, SkillOutput& output);
void RapidFreeze(const SkillInput& input, SkillOutput& output);

} // namespace skillformula
} // namespace decore

#endif
