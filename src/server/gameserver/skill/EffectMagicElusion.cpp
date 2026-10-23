#include "EffectMagicElusion.h"

#include "Zone.h"

EffectMagicElusion::EffectMagicElusion(Zone* pZone, ZoneCoord_t ZoneX, ZoneCoord_t ZoneY)
    : Effect(pZone, ZoneX, ZoneY, NULL, 0) {}

void EffectMagicElusion::unaffect() {
    Tile& tile = m_pZone->getTile(m_X, m_Y);
    tile.deleteEffect(m_ObjectID);
}

string EffectMagicElusion::toString() const {
    return "EffectMagicElusion()";
}
