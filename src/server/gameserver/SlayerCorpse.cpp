//////////////////////////////////////////////////////////////////////////////
// Filename    : SlayerCorpse.cpp
// Written by  : Reiot
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "SlayerCorpse.h"

#include "Creature.h"
#include "Slayer.h"

SlayerCorpse::SlayerCorpse(Slayer* pSlayer)

{
    Assert(pSlayer != NULL);

    m_SlayerInfo = pSlayer->getSlayerInfo3();
    setObjectID(m_SlayerInfo.getObjectID());
}

SlayerCorpse::~SlayerCorpse()

    {__BEGIN_TRY __END_CATCH_NO_RETHROW}

string SlayerCorpse::toString() const

{
    StringStream msg;
    msg << "SlayerCorpse(" << m_SlayerInfo.toString() << ")";
    return msg.toString();
}
