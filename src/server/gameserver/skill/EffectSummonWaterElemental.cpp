//////////////////////////////////////////////////////////////////////////////
// Filename    : EffectSummonWaterElemental.cpp
// Written by  : elca
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "EffectSummonWaterElemental.h"

#include "GCAddEffect.h"
#include "GCAddEffectToTile.h"
#include "GCRemoveEffect.h"
#include "GCSkillToObjectOK2.h"
#include "GCSkillToObjectOK4.h"
#include "GCStatusCurrentHP.h"
#include "GameContext.h"
#include "GamePlayer.h"
#include "HitRoll.h"
#include "Monster.h"
#include "Ousters.h"
#include "PCFinder.h"
#include "SkillInfo.h"
#include "SkillUtil.h"
#include "Slayer.h"
#include "Vampire.h"
#include "ZoneInfoManager.h"
#include "ZoneUtil.h"

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
EffectSummonWaterElemental::EffectSummonWaterElemental(Creature* pCreature)

{
    Assert(pCreature != NULL);
    Assert(pCreature->isOusters());

    setTarget(pCreature);
}

void EffectSummonWaterElemental::affect()

{
    Creature* pCastCreature = dynamic_cast<Creature*>(m_pTarget);

    if (pCastCreature == NULL || !pCastCreature->isOusters() || pCastCreature->isDead())
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

    SkillInfo* pSkillInfo = de::gameContext().skillInfos().getSkillInfo(SKILL_SUMMON_WATER_ELEMENTAL);
    OustersSkillSlot* pSkillSlot = pOusters->getSkill(SKILL_SUMMON_WATER_ELEMENTAL);

    bool bHitRoll = HitRoll::isSuccessMagic(pOusters, pSkillInfo, pSkillSlot);

    if (bHitRoll) {
        int HealPoint = getDamage();

        bool healed = false;

        int PartyID = pOusters->getPartyID();
        if (PartyID != 0) {
            // When the Ousters is in a party, the local party manager
            // shares the healing with nearby party members.
            LocalPartyManager* pLPM = pOusters->getLocalPartyManager();
            Assert(pLPM != NULL);
            healed = pLPM->shareWaterElementalHeal(PartyID, pOusters, HealPoint);
        }

        if (pOusters->getHP(ATTR_MAX) != pOusters->getHP()) {
            GCModifyInformation gcMI;
            HP_t final = min((int)pOusters->getHP(ATTR_MAX), pOusters->getHP() + HealPoint);
            if (final > pOusters->getHP(ATTR_MAX) - pOusters->getSilverDamage()) {
                pOusters->setSilverDamage(pOusters->getHP(ATTR_MAX) - final);
                gcMI.addShortData(MODIFY_SILVER_DAMAGE, pOusters->getSilverDamage());
            }

            if (pOusters->getHP() != final) {
                pOusters->setHP(final);
                gcMI.addShortData(MODIFY_CURRENT_HP, final);
            }

            GCStatusCurrentHP gcHP;
            gcHP.setObjectID(pOusters->getObjectID());
            gcHP.setCurrentHP(final);

            pZone->broadcastPacket(pOusters->getX(), pOusters->getY(), &gcHP);

            pOusters->getPlayer()->sendPacket(&gcMI);

            GCAddEffect gcAddEffect;
            gcAddEffect.setObjectID(pOusters->getObjectID());
            gcAddEffect.setEffectID(Effect::EFFECT_CLASS_WATER_ELEMENTAL_HEALED);
            gcAddEffect.setDuration(0);
            pZone->broadcastPacket(pOusters->getX(), pOusters->getY(), &gcAddEffect);

            healed = true;
        }

        if (healed) {
            GCAddEffect gcAddEffect;
            gcAddEffect.setObjectID(pOusters->getObjectID());
            gcAddEffect.setEffectID(Effect::EFFECT_CLASS_WATER_ELEMENTAL_HEAL);
            pZone->broadcastPacket(pOusters->getX(), pOusters->getY(), &gcAddEffect);
        }
    }

    setNextTime(20);
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectSummonWaterElemental::unaffect(Creature* pCreature)

{
    Assert(pCreature != NULL);
    Assert(pCreature->isOusters());

    // Turns off the flag.
    pCreature->removeFlag(Effect::EFFECT_CLASS_WATER_ELEMENTAL);

    Zone* pZone = pCreature->getZone();
    Assert(pZone != NULL);

    Ousters* pTargetOusters = dynamic_cast<Ousters*>(pCreature);
    Assert(pTargetOusters != NULL);

    // Tells clients to remove the effect.
    GCRemoveEffect gcRemoveEffect;
    gcRemoveEffect.setObjectID(pCreature->getObjectID());
    gcRemoveEffect.addEffectList(Effect::EFFECT_CLASS_WATER_ELEMENTAL);
    pZone->broadcastPacket(pCreature->getX(), pCreature->getY(), &gcRemoveEffect);
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectSummonWaterElemental::unaffect()

{
    Creature* pCreature = dynamic_cast<Creature*>(m_pTarget);
    unaffect(pCreature);
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
string EffectSummonWaterElemental::toString() const {
    StringStream msg;
    msg << "EffectSummonWaterElemental(" << "ObjectID:" << getObjectID() << ")";
    return msg.toString();
}
