//////////////////////////////////////////////////////////////////////////////
// Filename    : DarkLight.h
// Description :
// de-core: the dark and light levels a player is sent for its zone.
//
// Each zone has a dark level and a light level that follow the game clock
// (the DarkLightInfo table). A player does not see them as they are: a
// slayer sees them unless Lightness brightens its view or Yellow Poison
// darkens it, a vampire sees them inverted (it sees best in the dark),
// and an ousters sees a fixed dusk unless Yellow Poison darkens it. A
// castle zone shows every race the zone's own levels and a PK zone a fixed
// daylight. The server sends the result in GCUpdateInfo when a player
// enters a zone (makeGCUpdateInfo, PacketUtil.cpp), and in GCChangeDarkLight
// when the zone's levels change (WeatherManager.cpp, for a slayer and a
// vampire outside a castle and a PK zone) and when a vampire's Flare wears
// off (skill/EffectFlare.cpp).
//
// The levels are the server's, oddities included: a vampire's dark level
// is 13 minus the zone's, at least 0, and its light level 15 minus the
// zone's, at most 13, so a zone light level above 15 gives it a negative
// light level, which the wire's 8-bit field wraps. Changing any of this is
// a balance decision, recorded in docs/FIXES.md.
//
// Only the server computes it; the client draws the levels it is sent.
// This file is not in the vendored subset (DECORE_VENDORED_SOURCES), and
// its rows are in vectors/server/, which the client does not copy.
//////////////////////////////////////////////////////////////////////////////

#ifndef DECORE_DARK_LIGHT_H
#define DECORE_DARK_LIGHT_H

namespace decore {

enum class DarkLightRace { Slayer, Vampire, Ousters };

// What decides the levels a player sees, besides its zone's.
struct DarkLightViewer {
    DarkLightRace race;
    bool castleZone;   // the zone is a castle (ZONE_CASTLE)
    bool pkZone;       // the zone is a PK zone
    bool lightness;    // under Lightness; read for a slayer only
    bool yellowPoison; // under Yellow Poison; read for a slayer and an ousters
};

struct DarkLight {
    int darkLevel;
    int lightLevel;
};

// The dark and light levels `viewer` is sent in a zone whose own levels are
// zoneDarkLevel and zoneLightLevel. The results are not narrowed; the
// caller's 8-bit packet field does that.
DarkLight darkLightForViewer(const DarkLightViewer& viewer, int zoneDarkLevel, int zoneLightLevel);

} // namespace decore

#endif
