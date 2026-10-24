//----------------------------------------------------------------------
//
// Filename    : EffectContinualGroundAttack.cpp
// Written by  : elca
//
//----------------------------------------------------------------------

// include files
#include "EffectContinualGroundAttack.h"

#include "Assert.h"
#include "EffectGroundAttack.h"
#include "EffectMeteorStrike.h"
#include "GCAddEffectToTile.h"
#include "MonsterManager.h"
#include "PCManager.h"
#include "SkillUtil.h"
#include "Zone.h"
#include "ZoneUtil.h"

//----------------------------------------------------------------------
// constructor
//----------------------------------------------------------------------
EffectContinualGroundAttack::EffectContinualGroundAttack(Zone* pZone, EffectClass attackEffect, Turn_t delay)

{
    Assert(pZone != NULL);

    m_pZone = pZone;

    m_AttackEffect = attackEffect;

    m_Delay = delay;

    setNextTime(10); // Starts after 1 second
    setDeadline(delay);

    m_MinNumber = 1;
    m_MaxNumber = 1;

    // Server-only effect.
    m_bBroadcastingEffect = false;
}


//----------------------------------------------------------------------
// destructor
//----------------------------------------------------------------------
EffectContinualGroundAttack::~EffectContinualGroundAttack()

{}

void EffectContinualGroundAttack::affect()

{
    Assert(m_pZone != NULL);

    int creatureNum = m_pZone->getPCManager()->getSize() + m_pZone->getMonsterManager()->getSize();

    // To cut the load a little:
    // attach the effect only when the zone holds a character to attack.
    if (creatureNum > 0) {
        // Show the effect at a position in the zone and deal damage.
        int range = min(100, (m_MaxNumber - m_MinNumber)); // Between 1 and 100 at once
        int number = (range > 0 ? m_MinNumber + rand() % range : m_MinNumber);

        // cout << "EffectContinualGroundAttack: " << (int)m_pZone->getZoneID() << ", num= " << number << endl;

        VSRect rect(0, 0, m_pZone->getWidth() - 1, m_pZone->getHeight() - 1);

        // m_MinNumber ~ m_MaxNumber attacks
        for (int i = 0; i < number; i++) {
            const BPOINT& pt = m_pZone->getRandomEmptyTilePosition();

            if (!rect.ptInRect(pt.x, pt.y))
                continue;

            // Check the center tile.
            Tile& tile = m_pZone->getTile(pt.x, pt.y);

            int X = pt.x;
            int Y = pt.y;

            if (!tile.canAddEffect())
                continue;

            int DamagePercent = 100;

            // Create the effect object.
            Effect* pAttackEffect = NULL;

            switch (m_AttackEffect) {
            case EFFECT_CLASS_GROUND_ATTACK: {
                // If the same effect already exists, set the stronger damage.
                Effect* pOldEffect = tile.getEffect(Effect::EFFECT_CLASS_GROUND_ATTACK);
                if (pOldEffect != NULL) {
                    EffectGroundAttack* pGAEffect = dynamic_cast<EffectGroundAttack*>(pOldEffect);
                    pGAEffect->setDamagePercent(max(DamagePercent, pGAEffect->getDamagePercent()));
                    continue;
                }

                EffectGroundAttack* pEffect = new EffectGroundAttack(m_pZone, X, Y);
                pEffect->setDeadline(22); // 2 seconds plus a little
                pEffect->setDamagePercent(DamagePercent);

                pAttackEffect = pEffect;
            } break;

            case EFFECT_CLASS_METEOR_STRIKE: {
                // If the same effect exists, delete it.
                Effect* pOldEffect = tile.getEffect(Effect::EFFECT_CLASS_METEOR_STRIKE);
                if (pOldEffect != NULL) {
                    ObjectID_t effectID = pOldEffect->getObjectID();
                    m_pZone->deleteEffect(effectID);
                }

                EffectMeteorStrike* pEffect = new EffectMeteorStrike(m_pZone, X, Y);
                pEffect->setDeadline(10); // 1 second

                // 400 ~ 600 100%
                // 200 ~ 300 50%
                // 100 ~ 150 25%
                pEffect->setDamage(400 + rand() % 200);

                pAttackEffect = pEffect;
            } break;

            default:
                break;
            }


            if (pAttackEffect != NULL) {
                // An effect attached to a tile must be given an OID.
                ObjectRegistry& objectregister = m_pZone->getObjectRegistry();
                objectregister.registerObject(pAttackEffect);

                // Add the effect to the zone and to the tile.
                m_pZone->addEffect(pAttackEffect);
                tile.addEffect(pAttackEffect);

                // Show the effect only for the center one.
                GCAddEffectToTile gcAddEffectToTile;
                gcAddEffectToTile.setEffectID(pAttackEffect->getEffectClass());
                gcAddEffectToTile.setObjectID(pAttackEffect->getObjectID());
                gcAddEffectToTile.setXY(X, Y);
                gcAddEffectToTile.setDuration(20); // 2 seconds

                m_pZone->broadcastPacket(X, Y, &gcAddEffectToTile);
            }
        }
    }

    setNextTime(m_Delay);
}

//--------------------------------------------------------------------
// unaffect()
//--------------------------------------------------------------------
void EffectContinualGroundAttack::unaffect()

{}

//----------------------------------------------------------------------
// get debug string
//----------------------------------------------------------------------
string EffectContinualGroundAttack::toString() const

{
    StringStream msg;

    if (m_pZone) {
        msg << "EffectContinualGroundAttack(" << "ZoneID:" << (int)m_pZone->getZoneID();
    }

    msg << ",Delay:" << (int)m_Delay << ",Deadline:" << (int)m_Deadline.tv_sec << "." << (int)m_Deadline.tv_usec << ")";

    return msg.toString();
}
