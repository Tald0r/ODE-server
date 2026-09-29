//////////////////////////////////////////////////////////////////////////////
// Filename    : DarkLight.cpp
// Description :
// The dark and light levels a player is sent. See DarkLight.h. The branches
// are makeGCUpdateInfo's (PacketUtil.cpp), in its order.
//////////////////////////////////////////////////////////////////////////////

#include "domain/DarkLight.h"

#include <algorithm>

using std::max;
using std::min;

namespace decore {

namespace {
// The game's DARK_MAX and LIGHT_MAX (src/Core/types/ZoneTypes.h).
const int DARK_MAX = 13;
const int LIGHT_MAX = 15;
} // namespace

DarkLight darkLightForViewer(const DarkLightViewer& viewer, int zoneDarkLevel, int zoneLightLevel) {
    DarkLight levels = {};

    if (viewer.castleZone) {
        levels.darkLevel = zoneDarkLevel;
        levels.lightLevel = zoneLightLevel;
    } else if (viewer.pkZone) {
        levels.lightLevel = 14;
        levels.darkLevel = 0;
    } else if (viewer.race == DarkLightRace::Slayer) {
        if (viewer.lightness) {
            levels.lightLevel = 15;
            levels.darkLevel = 1;
        } else if (viewer.yellowPoison) {
            levels.darkLevel = 15;
            levels.lightLevel = 1;
        } else {
            levels.darkLevel = zoneDarkLevel;
            levels.lightLevel = zoneLightLevel;
        }
    } else if (viewer.race == DarkLightRace::Vampire) {
        levels.darkLevel = max(0, DARK_MAX - zoneDarkLevel);
        levels.lightLevel = min(13, LIGHT_MAX - zoneLightLevel);
    } else if (viewer.race == DarkLightRace::Ousters) {
        if (viewer.yellowPoison) {
            levels.darkLevel = 15;
            levels.lightLevel = 1;
        } else {
            levels.darkLevel = 13;
            levels.lightLevel = 6;
        }
    }

    return levels;
}

} // namespace decore
