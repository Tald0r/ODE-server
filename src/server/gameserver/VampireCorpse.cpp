//////////////////////////////////////////////////////////////////////////////
// Filename    : VampireCorpse.cpp
// Written by  : Reiot
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "VampireCorpse.h"

#include "Creature.h"
#include "Vampire.h"

VampireCorpse::VampireCorpse(Vampire* pVampire)

{
    Assert(pVampire != NULL);

    m_VampireInfo = pVampire->getVampireInfo3();
    setObjectID(m_VampireInfo.getObjectID());
}

VampireCorpse::~VampireCorpse()

    {__BEGIN_TRY __END_CATCH_NO_RETHROW} string VampireCorpse::toString() const {
    StringStream msg;
    msg << "VampireCorpse(" << m_VampireInfo.toString() << ")";
    return msg.toString();
}
