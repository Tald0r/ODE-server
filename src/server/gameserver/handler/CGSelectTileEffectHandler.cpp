//////////////////////////////////////////////////////////////////////////////
// Filename    : CGSelectTileEffectHandler.cpp
// Written By  : excel96
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "CGSelectTileEffect.h"
#include "GameContext.h"

#ifdef __GAME_SERVER__
#include "Effect.h"
#include "EffectManager.h"
#include "GCEnterVampirePortal.h"
#include "GCSystemMessage.h"
#include "GQuestManager.h"
#include "GamePlayer.h"
#include "PacketUtil.h"
#include "Properties.h"
#include "RelicUtil.h"
#include "Slayer.h"
#include "StringPool.h"
#include "Vampire.h"
#include "ZoneInfoManager.h"
#include "ZoneUtil.h"
#include "skill/EffectVampirePortal.h"
#endif // __GAME_SERVER__

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void CGSelectTileEffectHandler::execute(CGSelectTileEffect* pPacket, Player* pPlayer)

{
    __BEGIN_TRY __BEGIN_DEBUG_EX

#ifdef __GAME_SERVER__

        Assert(pPacket != NULL);
    Assert(pPlayer != NULL);

    try {
        // Just return if the game player's state is not normal.
        GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(pPlayer);
        Assert(pGamePlayer != NULL);
        if (pGamePlayer->getPlayerStatus() != GPS_NORMAL)
            return;

        Creature* pCreature = pGamePlayer->getCreature();
        Assert(pCreature != NULL);

        // A portal cannot be entered while holding a relic.
        if (pCreature->hasRelicItem() || pCreature->isFlag(Effect::EFFECT_CLASS_HAS_FLAG) ||
            pCreature->isFlag(Effect::EFFECT_CLASS_HAS_SWEEPER)) {
            return;
        }

        Zone* pZone = pCreature->getZone();
        Assert(pZone != NULL);

        Effect* pEffect = NULL;

        // Look in the vampire portal manager first.
        EffectManager* pVampirePortalManager = pZone->getVampirePortalManager();

        pEffect = pVampirePortalManager->findEffect(pPacket->getEffectObjectID());

        if (pEffect != NULL) {
            switch (pEffect->getEffectClass()) {
            case Effect::EFFECT_CLASS_VAMPIRE_PORTAL:
                executeVampirePortal(pPacket, pPlayer, pEffect);
                break;
            default:
                Assert(false);
                break;
            }
        } else {
            cout << "CGSelectTileEffectHandler::execute() : Effect DOES NOT Exist" << endl;

            // Hmm... when no effect with that OID exists, quietly ignore it.
        }
    } catch (Throwable& t) {
        cerr << t.toString() << endl;
    }

#endif // __GAME_SERVER__

    __END_DEBUG_EX __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void CGSelectTileEffectHandler::executeVampirePortal(CGSelectTileEffect* pPacket, Player* pPlayer, Effect* pEffect)

{
    __BEGIN_TRY __BEGIN_DEBUG_EX

#ifdef __GAME_SERVER__

        Assert(pPacket != NULL);
    Assert(pPlayer != NULL);
    Assert(pEffect != NULL);
    Assert(pEffect->getEffectClass() == Effect::EFFECT_CLASS_VAMPIRE_PORTAL);

    try {
        // Just return if the game player's state is not normal.
        GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(pPlayer);
        Assert(pGamePlayer != NULL);
        if (pGamePlayer->getPlayerStatus() != GPS_NORMAL)
            return;

        Creature* pCreature = pGamePlayer->getCreature();
        Assert(pCreature != NULL);

        // Only a Vampire can use it.
        if (!pCreature->isVampire())
            return;

        Zone* pZone = pCreature->getZone();
        Assert(pZone != NULL);

        Vampire* pVampire = dynamic_cast<Vampire*>(pCreature);
        Assert(pVampire != NULL);

        EffectVampirePortal* pEffectVampirePortal = dynamic_cast<EffectVampirePortal*>(pEffect);
        ZONE_COORD zonecoord = pEffectVampirePortal->getZoneCoord();

        // Temerie cannot be reached.
        // Really a bloody tunnel should not be creatable at all, but
        // a seal with the coordinates already set exists, so it is blocked here too.
        if (zonecoord.id == 1122 || zonecoord.id == 8000) {
            return;
        }

        try {
            de::gameContext().zoneInfos().getZoneInfo(zonecoord.id);
        } catch (NoSuchElementException&) {
            return;
        }

        if (pEffectVampirePortal->getCount() > 0) {
            // Set the flag on the Vampire itself.
            // This is so that when Zone::addPC adds the vampire, the GCAddVampire
            // broadcast around it says the vampire came from a portal.
            // Zone::addPC clears it again.
            pVampire->setFlag(Effect::EFFECT_CLASS_VAMPIRE_PORTAL);

            // First tell those around that the vampire disappears through a portal.
            GCEnterVampirePortal gcEnterVampirePortal;
            gcEnterVampirePortal.setObjectID(pVampire->getObjectID());
            gcEnterVampirePortal.setX(pEffectVampirePortal->getX());
            gcEnterVampirePortal.setY(pEffectVampirePortal->getY());
            pZone->broadcastPacket(pVampire->getX(), pVampire->getY(), &gcEnterVampirePortal);

            pVampire->getGQuestManager()->illegalWarp();
            // Actually move it.
            transportCreature(pCreature, zonecoord.id, zonecoord.x, zonecoord.y, false);

            // After the move, lower the count; when the count reaches 0 the effect disappears.
            pEffectVampirePortal->setCount(pEffectVampirePortal->getCount() - 1);
        }
    } catch (Throwable& t) {
        cerr << t.toString() << endl;
    }

#endif // __GAME_SERVER__

    __END_DEBUG_EX __END_CATCH
}
