//----------------------------------------------------------------------
//
// Filename    : EffectContinualBloodyWall.cpp
// Written by  : elca
//
//----------------------------------------------------------------------

// include files
#include "EffectContinualBloodyWall.h"

#include "Assert.h"
#include "DB.h"
#include "EffectBloodyWall.h"
#include "GCAddEffectToTile.h"
#include "SkillUtil.h"
#include "Zone.h"
#include "ZoneUtil.h"
#include "repository/ZoneInfoRepository.h"

//----------------------------------------------------------------------
// constructor
//----------------------------------------------------------------------
EffectContinualBloodyWall::EffectContinualBloodyWall(Zone* pZone)

{
    Assert(pZone != NULL);

    m_pZone = pZone;
    m_Tick = 30;
    m_Duration = 10;
    m_Damage = 1000;

    setNextTime(10); // Starts after 1 second

    // This is a server-only effect.
    m_bBroadcastingEffect = false;
}


//----------------------------------------------------------------------
// destructor
//----------------------------------------------------------------------
EffectContinualBloodyWall::~EffectContinualBloodyWall()

{}

void EffectContinualBloodyWall::affect()

{
    Assert(m_pZone != NULL);

    VSRect rect(0, 0, m_pZone->getWidth() - 1, m_pZone->getHeight() - 1);

    for (int X = m_Left; X <= m_Right; X++)
        for (int Y = m_Top; Y <= m_Bottom; Y++) {
            if (rect.ptInRect(X, Y)) {
                Tile& tile = m_pZone->getTile(X, Y);
                if (tile.canAddEffect()) {
                    EffectBloodyWall* pEffect = new EffectBloodyWall(m_pZone, X, Y);
                    pEffect->setDamage(getDamage());
                    pEffect->setDeadline(getDuration());
                    pEffect->setTick(getDuration());
                    pEffect->setNextTime(0);
                    pEffect->setForce(true);

                    m_pZone->registerObject(pEffect);
                    m_pZone->addEffect(pEffect);
                    tile.addEffect(pEffect);

                    GCAddEffectToTile gcAddEffectToTile;
                    gcAddEffectToTile.setEffectID(pEffect->getEffectClass());
                    gcAddEffectToTile.setXY(X, Y);
                    gcAddEffectToTile.setDuration(getDuration());

                    m_pZone->broadcastPacket(X, Y, &gcAddEffectToTile);
                }
            }
        }

    setNextTime(m_Tick);
}

//--------------------------------------------------------------------
// unaffect()
//--------------------------------------------------------------------
void EffectContinualBloodyWall::unaffect()

{}

//----------------------------------------------------------------------
// get debug string
//----------------------------------------------------------------------
string EffectContinualBloodyWall::toString() const {
    StringStream msg;

    if (m_pZone) {
        msg << "EffectContinualBloodyWall(" << "ZoneID:" << (int)m_pZone->getZoneID();
    }

    msg << ",Delay:" << (int)m_Tick << ",Deadline:" << (int)m_Deadline.tv_sec << "." << (int)m_Deadline.tv_usec << ")";

    return msg.toString();
}

void EffectContinualBloodyWallLoader::load(Zone* pZone)

{
    vector<ZoneEffectRow> rows = defaultZoneInfoRepository().loadZoneEffectRects(
        pZone->getZoneID(), (int)Effect::EFFECT_CLASS_CONTINUAL_BLOODY_WALL);

    for (size_t r = 0; r < rows.size(); r++) {
        ZoneCoord_t left = rows[r].left;
        ZoneCoord_t top = rows[r].top;
        ZoneCoord_t right = rows[r].right;
        ZoneCoord_t bottom = rows[r].bottom;
        int value1 = rows[r].value1;
        int value2 = rows[r].value2;
        int value3 = rows[r].value3;

        EffectContinualBloodyWall* pEffect = new EffectContinualBloodyWall(pZone);
        pEffect->setRect(left, right, top, bottom);
        pEffect->setTick(value1);
        pEffect->setDuration(value2);
        pEffect->setDamage(value3);

        // Register the effect and add it to the zone.
        pZone->registerObject(pEffect);
        pZone->addEffect(pEffect);
    }
}
