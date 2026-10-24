//////////////////////////////////////////////////////////////////////////////
// Filename    : EffectInstallTurret.cpp
// Written by  : elca
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "EffectInstallTurret.h"

#include "GCModifyInformation.h"
#include "GCOtherModifyInfo.h"
#include "GCRemoveEffect.h"
#include "GCStatusCurrentHP.h"
#include "Monster.h"
#include "PacketUtil.h"
#include "Player.h"
#include "Slayer.h"
#include "Vampire.h"

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
EffectInstallTurret::EffectInstallTurret(Creature* pCreature)

{
    Assert(pCreature != NULL);
    Assert(pCreature->isSlayer());

    setTarget(pCreature);
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectInstallTurret::affect(Creature* pCreature)

{}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectInstallTurret::unaffect()

{
    Creature* pCreature = dynamic_cast<Creature*>(m_pTarget);
    unaffect(pCreature);
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectInstallTurret::unaffect(Creature* pCreature)

{
    Assert(pCreature != NULL);
    Assert(pCreature->isSlayer()); // Only a Slayer can be affected.

    pCreature->removeFlag(Effect::EFFECT_CLASS_INSTALL_TURRET);

    Slayer* pSlayer = dynamic_cast<Slayer*>(pCreature);

    SLAYER_RECORD prev;
    pSlayer->getSlayerRecord(prev);
    pSlayer->initAllStat();
    pSlayer->sendRealWearingInfo();
    pSlayer->sendModifyInfo(prev);

    Zone* pZone = pCreature->getZone();
    Assert(pZone != NULL);

    GCRemoveEffect gcRemoveEffect;
    gcRemoveEffect.setObjectID(pCreature->getObjectID());
    gcRemoveEffect.addEffectList(Effect::EFFECT_CLASS_INSTALL_TURRET);
    pZone->broadcastPacket(pCreature->getX(), pCreature->getY(), &gcRemoveEffect);

    GCOtherModifyInfo gcOtherModifyInfo;
    makeGCOtherModifyInfo(&gcOtherModifyInfo, pSlayer, &prev);
    pZone->broadcastPacket(pSlayer->getX(), pSlayer->getY(), &gcOtherModifyInfo, pSlayer);
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
string EffectInstallTurret::toString() const {
    StringStream msg;
    msg << "EffectInstallTurret(" << "ObjectID:" << getObjectID() << ")";
    return msg.toString();
}
