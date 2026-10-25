//////////////////////////////////////////////////////////////////////////////
// Filename    : GCPetStashList.cpp
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "GCPetStashList.h"

#include "Assert1.h"

//////////////////////////////////////////////////////////////////////////////
// constructor
//////////////////////////////////////////////////////////////////////////////

GCPetStashList::GCPetStashList()

{
    __BEGIN_TRY

    m_PetStashItemInfos.reserve(MAX_PET_STASH);
    for (int i = 0; i < MAX_PET_STASH; ++i) {
        m_PetStashItemInfos.push_back(NULL);
    }

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
// destructor
//////////////////////////////////////////////////////////////////////////////
GCPetStashList::~GCPetStashList()

{
    __BEGIN_TRY

    releaseSlots();

    __END_CATCH_NO_RETHROW
}

//////////////////////////////////////////////////////////////////////////////
// A filler's pets are the stash items' (PacketUtil.cpp); read()'s are the
// packet's. The slot records are the packet's either way.
//////////////////////////////////////////////////////////////////////////////
void GCPetStashList::releaseSlots() {
    for (int i = 0; i < MAX_PET_STASH; ++i) {
        if (m_bOwnsPetInfos && m_PetStashItemInfos[i] != NULL)
            SAFE_DELETE(m_PetStashItemInfos[i]->pPetInfo);
        SAFE_DELETE(m_PetStashItemInfos[i]);
    }
    m_bOwnsPetInfos = false;
}

//////////////////////////////////////////////////////////////////////////////
// Read data from the input stream (buffer) and initialise the packet.
//////////////////////////////////////////////////////////////////////////////
void GCPetStashList::read(SocketInputStream& iStream)

{
    __BEGIN_TRY

    // A reader's pets are its own: a filler's slots are dropped with their
    // pets unowned, and an earlier read's are freed, before the flag is set.
    releaseSlots();
    m_bOwnsPetInfos = true;

    for (int i = 0; i < MAX_PET_STASH; i++) {
        BYTE isAvailable;
        iStream.read(isAvailable);
        if (isAvailable == 0)
            continue;

        m_PetStashItemInfos[i] = new PetStashItemInfo;
        m_PetStashItemInfos[i]->pPetInfo = new PetInfo;
        m_PetStashItemInfos[i]->pPetInfo->read(iStream);

        iStream.read(m_PetStashItemInfos[i]->KeepDays);
    }


    __END_CATCH
}


//////////////////////////////////////////////////////////////////////////////
// Send the packet's binary image to the output stream (buffer).
//////////////////////////////////////////////////////////////////////////////
void GCPetStashList::write(SocketOutputStream& oStream) const

{
    __BEGIN_TRY
    __BEGIN_DEBUG

    for (int i = 0; i < MAX_PET_STASH; i++) {
        BYTE isAvailable;
        if (m_PetStashItemInfos[i] == NULL) {
            isAvailable = 0;
            oStream.write(isAvailable);
            continue;
        } else {
            isAvailable = 1;
            oStream.write(isAvailable);
        }

        m_PetStashItemInfos[i]->pPetInfo->write(oStream);
        oStream.write(m_PetStashItemInfos[i]->KeepDays);
    }

    __END_DEBUG
    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
PacketSize_t GCPetStashList::getPacketSize() const

{
    __BEGIN_TRY
    __BEGIN_DEBUG

    PacketSize_t ret = 0;

    for (int i = 0; i < MAX_PET_STASH; ++i) {
        ret += szBYTE;
        if (m_PetStashItemInfos[i] != NULL) {
            ret += m_PetStashItemInfos[i]->pPetInfo->getSize();
            ret += szDWORD;
        }
    }

    return ret;

    __END_DEBUG
    __END_CATCH
}


//////////////////////////////////////////////////////////////////////////////
// get packet's debug string
//////////////////////////////////////////////////////////////////////////////
string GCPetStashList::toString() const

{
    __BEGIN_TRY

    StringStream msg;
    msg << "GCPetStashList(";
    for (int i = 0; i < MAX_PET_STASH; ++i) {
        if (m_PetStashItemInfos[i] == NULL) {
            msg << "NULL,";
        } else {
            msg << "PetItem(" << m_PetStashItemInfos[i]->pPetInfo->toString() << "," << m_PetStashItemInfos[i]->KeepDays
                << "),";
        }
    }
    msg << ")";
    return msg.toString();

    __END_CATCH
}
