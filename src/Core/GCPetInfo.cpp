//////////////////////////////////////////////////////////////////////////////
// Filename    : GCPetInfo.cpp
// Written By  :
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "GCPetInfo.h"

#include "Assert1.h"
#include "GuildWarInfo.h"
#include "LevelWarInfo.h"
#include "RaceWarInfo.h"

//////////////////////////////////////////////////////////////////////////////
// constructor
//////////////////////////////////////////////////////////////////////////////
GCPetInfo::GCPetInfo()

{
    __BEGIN_TRY

    m_pPetInfo = NULL;
    m_ObjectID = 0;
    m_IsSummonInfo = 0;

    __END_CATCH;
}

//////////////////////////////////////////////////////////////////////////////
// destructor
//////////////////////////////////////////////////////////////////////////////
GCPetInfo::~GCPetInfo()

{
    __BEGIN_TRY

    // a filler's record is the pet item's (CreatureUtil.cpp); read()'s is the packet's
    if (m_bOwnsPetInfo)
        SAFE_DELETE(m_pPetInfo);

    __END_CATCH_NO_RETHROW
}

//////////////////////////////////////////////////////////////////////////////
// Read data from the input stream (buffer) and initialise the packet.
//////////////////////////////////////////////////////////////////////////////
void GCPetInfo::read(SocketInputStream& iStream)

{
    __BEGIN_TRY

    // A reader's record is its own: a filler's is dropped, unowned, and an
    // earlier read's is freed, before the flag is set.
    if (m_bOwnsPetInfo)
        SAFE_DELETE(m_pPetInfo);
    m_pPetInfo = NULL;
    m_bOwnsPetInfo = true;

    m_pPetInfo = new PetInfo;
    m_pPetInfo->read(iStream);
    m_IsSummonInfo = m_pPetInfo->isSummonInfo();
    iStream.read(m_ObjectID);

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////////////
// Send the packet's binary image to the output stream (buffer).
//////////////////////////////////////////////////////////////////////////////
void GCPetInfo::write(SocketOutputStream& oStream) const

{
    __BEGIN_TRY

    if (m_pPetInfo == NULL) {
        PetType_t PetType = PET_NONE;
        oStream.write(PetType);
    } else {
        m_pPetInfo->setSummonInfo(m_IsSummonInfo);
        m_pPetInfo->write(oStream);
    }

    oStream.write(m_ObjectID);

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////
// get packet's debug string
//////////////////////////////////////////////////////////////////////////////
string GCPetInfo::toString() const

{
    __BEGIN_TRY

    StringStream msg;

    msg << "GCPetInfo(" << "ObjectID:" << m_ObjectID << "PetInfo:";

    if (m_pPetInfo == NULL) {
        msg << "NULL";
    } else {
        msg << m_pPetInfo->toString();
    }

    msg << "IsSummonInfo:" << m_IsSummonInfo;
    msg << ")";

    return msg.toString();

    __END_CATCH
}
