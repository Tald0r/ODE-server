//////////////////////////////////////////////////////////////////////////////
// Filename    : EffectMasterLairPass.cpp
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "EffectMasterLairPass.h"

#include "Creature.h"
#include "GCRemoveEffect.h"
#include "GCSystemMessage.h"
#include "Player.h"
#include "ZoneInfoManager.h"

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
EffectMasterLairPass::EffectMasterLairPass(Creature* pCreature, ZoneID_t zoneID)

{
    m_ZoneID = zoneID;

    setTarget(pCreature);

    // Server-only effect.
    m_bBroadcastingEffect = false;
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectMasterLairPass::affect()

{
    Creature* pCreature = dynamic_cast<Creature*>(m_pTarget);
    affect(pCreature);
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectMasterLairPass::affect(Creature* pCreature)

{}

void EffectMasterLairPass::unaffect()

{
    Creature* pCreature = dynamic_cast<Creature*>(m_pTarget);
    unaffect(pCreature);
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectMasterLairPass::unaffect(Creature* pCreature)

{
    __BEGIN_DEBUG

    // cout << "EffectMasterLairPass" << "unaffect BEGIN" << endl;

    Assert(pCreature != NULL);

    // To restore the attributes properly, clear the flag and
    // call initAllStat.
    pCreature->removeFlag(Effect::EFFECT_CLASS_MASTER_LAIR_PASS);

    Zone* pZone = pCreature->getZone();
    Assert(pZone != NULL);

    // cout << "EffectMasterLairPass" << "unaffect END" << endl;

    __END_DEBUG
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
string EffectMasterLairPass::toString() const {
    StringStream msg;
    msg << "EffectMasterLairPass(" << "ObjectID:" << getObjectID() << ",ZoneID:" << m_ZoneID << ")";
    return msg.toString();
}
