//////////////////////////////////////////////////////////////////////////////
// Filename    : SkillRange.cpp
// Description :
// A slayer skill's range at a proficiency level. See SkillRange.h. The
// body is the server's computeSkillRange (skill/SkillUtil.cpp), moved
// verbatim with its 8-bit types spelled out.
//////////////////////////////////////////////////////////////////////////////

#include "domain/SkillRange.h"

namespace decore {

namespace {
// Range_t and SkillLevel_t are BYTE in the game
// (src/Core/types/ItemTypes.h, CreatureTypes.h): the table's ranges, the
// slot's 16-bit exp level and the result all wrap to 8 bits there.
typedef unsigned char Byte;
} // namespace

int skillRange(int minRange, int maxRange, int expLevel) {
    Byte skillMinPoint = (Byte)minRange;
    Byte skillMaxPoint = (Byte)maxRange;
    Byte skillLevel = (Byte)expLevel;

    Byte range = (Byte)(int)(skillMinPoint + (skillMaxPoint - skillMinPoint) * (double)(skillLevel * 0.01));

    return range;
}

} // namespace decore
