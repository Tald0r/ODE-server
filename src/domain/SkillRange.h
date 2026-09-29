//////////////////////////////////////////////////////////////////////////////
// Filename    : SkillRange.h
// Description :
// de-core: the range of a slayer skill at a proficiency level.
//
// A skill's range grows from the skill table's minimum range at
// proficiency (exp) level 0 to its maximum range at level 100, in
// proportion to the level, and the result is truncated toward zero. The
// server's four sliding and walking skills (BlazeWalk, BlitzSliding,
// FlashSliding, ShadowWalk) check their target's distance against it
// through computeSkillRange (skill/SkillUtil.cpp), which reads the three
// inputs and calls this. Most other skill handlers check the table's
// maximum range instead (SkillInfo::getRange).
//
// The arithmetic is the server's, oddities included: the three inputs
// are narrowed to 8 bits first, as the server holds them (a range in a
// Range_t, the level in a SkillLevel_t), so each wraps at 256; the level
// is scaled by level * 0.01 in double; the minimum is added before the
// truncation, so with a maximum below the minimum a partial step rounds
// down, away from the minimum (minimum 6 and maximum 2 give 4 at level
// 30, where truncating the step alone would give 5); and the result is
// narrowed to 8 bits again, so a range below 0 or above 255 wraps.
// Changing any of these is a balance decision, recorded in docs/FIXES.md.
//
// The client vendors this file and computes a slayer skill's range with
// it, for how close its character walks before using a skill.
//////////////////////////////////////////////////////////////////////////////

#ifndef DECORE_SKILL_RANGE_H
#define DECORE_SKILL_RANGE_H

namespace decore {

// The range, 0 to 255, of a skill whose table ranges are minRange and
// maxRange at proficiency level expLevel. Each input is read modulo 256.
int skillRange(int minRange, int maxRange, int expLevel);

} // namespace decore

#endif
