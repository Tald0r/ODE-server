//////////////////////////////////////////////////////////////////////////////
// Filename    : CGResurrectHandler.cpp
// Written By  : excel96
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "CGResurrect.h"

#ifdef __GAME_SERVER__
#include "Effect.h"
#include "EffectComa.h"
#include "EffectManager.h"
#include "GamePlayer.h"
#include "Ousters.h"
#include "Slayer.h"
#include "Vampire.h"
#endif

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void CGResurrectHandler::execute(CGResurrect* pPacket, Player* pPlayer)

{
    __BEGIN_DEBUG_EX

#ifdef __GAME_SERVER__

        Assert(pPacket != NULL);
    Assert(pPlayer != NULL);

    GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(pPlayer);
    Assert(pGamePlayer != NULL);

    Creature* pCreature = pGamePlayer->getCreature();
    Assert(pCreature != NULL);

    if (pCreature->findEffect(Effect::EFFECT_CLASS_ETERNITY))
        return;

    // It is an error when the creature carries no COMA effect.
    if (pCreature->isFlag(Effect::EFFECT_CLASS_COMA)) {
        // Reach the COMA effect.
        EffectManager* pEffectManager = pCreature->getEffectManager();
        Assert(pEffectManager != NULL);

        EffectComa* pEffectComa = (EffectComa*)(pEffectManager->findEffect(Effect::EFFECT_CLASS_COMA));
        Assert(pEffectComa != NULL);

        // Without 5 seconds since death, no revival is possible.
        if (pEffectComa->canResurrect()) {
            // Set the deadline to 0. The heartbeat then unaffects it and
            // revives the player automatically.
            pEffectComa->setDeadline(0);
        } else {
        }
    } else {
        // Where exactly is unclear, but after dying somewhere the coma
        // effect seems to fly off, or the HP rises while dead.
        // So code is put in that kills by force when the packet arrives
        // with no effect attached.
        if (pCreature->isSlayer()) {
            Slayer* pSlayer = dynamic_cast<Slayer*>(pCreature);
            pSlayer->setHP(0, ATTR_CURRENT);
            pSlayer->deleteEffect(Effect::EFFECT_CLASS_COMA);

            EffectComa* pEffectComa = new EffectComa(pSlayer);
            pEffectComa->setStartTime();
            pEffectComa->setDeadline(0);
            pSlayer->addEffect(pEffectComa);
            pSlayer->setFlag(Effect::EFFECT_CLASS_COMA);
        } else if (pCreature->isVampire()) {
            Vampire* pVampire = dynamic_cast<Vampire*>(pCreature);
            pVampire->setHP(0, ATTR_CURRENT);
            pVampire->deleteEffect(Effect::EFFECT_CLASS_COMA);

            EffectComa* pEffectComa = new EffectComa(pVampire);
            pEffectComa->setStartTime();
            pEffectComa->setDeadline(0);
            pVampire->addEffect(pEffectComa);
            pVampire->setFlag(Effect::EFFECT_CLASS_COMA);
        } else if (pCreature->isOusters()) {
            Ousters* pOusters = dynamic_cast<Ousters*>(pCreature);
            pOusters->setHP(0, ATTR_CURRENT);
            pOusters->deleteEffect(Effect::EFFECT_CLASS_COMA);

            EffectComa* pEffectComa = new EffectComa(pOusters);
            pEffectComa->setStartTime();
            pEffectComa->setDeadline(0);
            pOusters->addEffect(pEffectComa);
            pOusters->setFlag(Effect::EFFECT_CLASS_COMA);
        } else {
            Assert(false);
        }
    }

#endif

    __END_DEBUG_EX
}
