//////////////////////////////////////////////////////////////////////////////
// Filename    : OustersCorpse.cpp
// Written by  : Reiot
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "OustersCorpse.h"

#include "Creature.h"
#include "Ousters.h"

OustersCorpse::OustersCorpse(Ousters* pOusters)

{
    Assert(pOusters != NULL);

    m_OustersInfo = pOusters->getOustersInfo3();
    setObjectID(m_OustersInfo.getObjectID());
}

OustersCorpse::~OustersCorpse()

    {__BEGIN_TRY __END_CATCH_NO_RETHROW} string OustersCorpse::toString() const {
    StringStream msg;
    msg << "OustersCorpse(" << m_OustersInfo.toString() << ")";
    return msg.toString();
}
