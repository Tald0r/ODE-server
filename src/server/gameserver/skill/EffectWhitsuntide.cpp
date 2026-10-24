//////////////////////////////////////////////////////////////////////////////
// Filename    : EffectWhitsuntide.cpp
// Written by  : elca
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "EffectWhitsuntide.h"

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
EffectWhitsuntide::EffectWhitsuntide(Creature* pCreature)

{
    Assert(pCreature != NULL);
    Assert(pCreature->isSlayer());

    setTarget(pCreature);
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectWhitsuntide::unaffect()

{
    Creature* pCreature = dynamic_cast<Creature*>(m_pTarget);
    unaffect(pCreature);
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectWhitsuntide::unaffect(Creature* pCreature)

{
    Assert(pCreature != NULL);
    Assert(pCreature->isSlayer()); // Only a Slayer can be affected.

    pCreature->removeFlag(Effect::EFFECT_CLASS_WHITSUNTIDE);

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
    gcRemoveEffect.addEffectList(Effect::EFFECT_CLASS_WHITSUNTIDE);
    pZone->broadcastPacket(pCreature->getX(), pCreature->getY(), &gcRemoveEffect);
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
string EffectWhitsuntide::toString() const {
    StringStream msg;
    msg << "EffectWhitsuntide(" << "ObjectID:" << getObjectID() << ")";
    return msg.toString();
}
