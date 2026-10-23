//////////////////////////////////////////////////////////////////////////////
// Filename    : EffectRingOfFlare.cpp
// Written by  : elca
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "EffectRingOfFlare.h"

#include "GCAddEffectToTile.h"
#include "GCRemoveEffect.h"
#include "GCSkillToObjectOK2.h"
#include "GCSkillToObjectOK4.h"
#include "GCStatusCurrentHP.h"
#include "GamePlayer.h"
#include "Monster.h"
#include "Ousters.h"
#include "PCFinder.h"
#include "SkillUtil.h"
#include "Slayer.h"
#include "Vampire.h"
#include "ZoneInfoManager.h"
#include "ZoneUtil.h"

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
EffectRingOfFlare::EffectRingOfFlare(Creature* pCreature)

{
    Assert(pCreature != NULL);
    Assert(pCreature->isOusters());

    setTarget(pCreature);
}

void EffectRingOfFlare::affect()

{
    Creature* pCastCreature = dynamic_cast<Creature*>(m_pTarget);

    if (pCastCreature == NULL || !pCastCreature->isOusters())
        return;

    Ousters* pOusters = dynamic_cast<Ousters*>(pCastCreature);
    Assert(pOusters != NULL);

    Item* pWeapon = pOusters->getWearItem(Ousters::WEAR_RIGHTHAND);
    if (pWeapon == NULL || pWeapon->getItemClass() != Item::ITEM_CLASS_OUSTERS_WRISTLET ||
        !pOusters->isRealWearingEx(Ousters::WEAR_RIGHTHAND)) {
        // Removing the wristlet ends the effect.
        setDeadline(0);
        return;
    }

    Player* pPlayer = dynamic_cast<Player*>(pCastCreature->getPlayer());
    Assert(pPlayer != NULL);

    Zone* pZone = pCastCreature->getZone();
    Assert(pZone != NULL);

    VSRect rect(0, 0, pZone->getWidth() - 1, pZone->getHeight() - 1);

    ZoneCoord_t Cx = pCastCreature->getX();
    ZoneCoord_t Cy = pCastCreature->getY();

    for (int x = -1; x <= 1; x++) {
        for (int y = -1; y <= 1; y++) {
            if (x == 0 && y == 0)
                continue;

            int X = Cx + x;
            int Y = Cy + y;

            if (!rect.ptInRect(X, Y))
                continue;

            // Fetches the objects on the tile.
            Tile& tile = pZone->getTile(X, Y);

            if (tile.hasCreature(Creature::MOVE_MODE_WALKING)) {
                Creature* pCreature = tile.getCreature(Creature::MOVE_MODE_WALKING);
                Assert(pCreature != NULL);

                GCModifyInformation gcAttackerMI;

                // The target itself, invulnerable creatures, Slayers and NPCs are not hit.
                // Safe zone check
                // 2003.1.10 by bezz, Sequoia
                if (pCreature == m_pTarget || !canAttack(pCastCreature, pCreature) ||
                    pCreature->isFlag(Effect::EFFECT_CLASS_COMA) || pCreature->isOusters() ||
                    !checkZoneLevelToHitTarget(pCreature)) {
                    continue;
                }

                if (pCreature->isPC()) {
                    GCModifyInformation gcMI;
                    ::setDamage(pCreature, m_Damage, pCastCreature, SKILL_RING_OF_FLARE, &gcMI, &gcAttackerMI);

                    pCreature->getPlayer()->sendPacket(&gcMI);

                    // Shows the hit animation.
                    GCSkillToObjectOK2 gcSkillToObjectOK2;
                    gcSkillToObjectOK2.setObjectID(1); // Unused
                    gcSkillToObjectOK2.setSkillType(SKILL_ATTACK_MELEE);
                    gcSkillToObjectOK2.setDuration(0);
                    pCreature->getPlayer()->sendPacket(&gcSkillToObjectOK2);

                } else if (pCreature->isMonster()) {
                    Monster* pMonster = dynamic_cast<Monster*>(pCreature);

                    ::setDamage(pMonster, m_Damage, pCastCreature, SKILL_RING_OF_FLARE, NULL, &gcAttackerMI);

                    pMonster->addEnemy(pCastCreature);
                }

                GCSkillToObjectOK4 gcSkillToObjectOK4;
                gcSkillToObjectOK4.setSkillType(SKILL_ATTACK_MELEE);
                gcSkillToObjectOK4.setTargetObjectID(pCreature->getObjectID());
                gcSkillToObjectOK4.setDuration(0);

                pZone->broadcastPacket(X, Y, &gcSkillToObjectOK4, pCreature);

                // Grant experience if the target died.
                if (pCastCreature != NULL) {
                    if (pCreature->isDead() && pCastCreature->isOusters()) {
                        Ousters* pCastOusters = dynamic_cast<Ousters*>(pCastCreature);
                        Assert(pCastOusters != NULL);

                        int exp = computeCreatureExp(pCreature, 100, pCastOusters);
                        shareOustersExp(pCastOusters, exp, gcAttackerMI);
                    }
                }

                if (gcAttackerMI.getShortCount() != 0 || gcAttackerMI.getLongCount() != 0)
                    pCastCreature->getPlayer()->sendPacket(&gcAttackerMI);
            }
        }
    }

    setNextTime(10);
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectRingOfFlare::unaffect(Creature* pCreature)

{
    Assert(pCreature != NULL);
    Assert(pCreature->isOusters());

    // Turns off the flag.
    pCreature->removeFlag(Effect::EFFECT_CLASS_RING_OF_FLARE);

    Zone* pZone = pCreature->getZone();
    Assert(pZone != NULL);

    Ousters* pTargetOusters = dynamic_cast<Ousters*>(pCreature);
    Assert(pTargetOusters != NULL);

    // Tells clients to remove the effect.
    GCRemoveEffect gcRemoveEffect;
    gcRemoveEffect.setObjectID(pCreature->getObjectID());
    gcRemoveEffect.addEffectList(getSendEffectClass());
    pZone->broadcastPacket(pCreature->getX(), pCreature->getY(), &gcRemoveEffect);
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectRingOfFlare::unaffect()

{
    Creature* pCreature = dynamic_cast<Creature*>(m_pTarget);
    unaffect(pCreature);
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
string EffectRingOfFlare::toString() const {
    StringStream msg;
    msg << "EffectRingOfFlare(" << "ObjectID:" << getObjectID() << ")";
    return msg.toString();
}
