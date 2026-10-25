//////////////////////////////////////////////////////////////////////////////
// Filename    : GCAddVampire.cpp
// Written By  : Reiot
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "GCAddVampire.h"

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
GCAddVampire::~GCAddVampire()

{
    __BEGIN_TRY

    // The effect record is built for the packet, by read() here and by a
    // fresh EffectManager snapshot on the fill side, so the packet owns it.
    // A filler's pet and nickname records belong to the creature
    // (PacketUtil.cpp); read() allocates its own, and those go with the packet.
    SAFE_DELETE(m_pEffectInfo);
    if (m_bOwnsRecords) {
        SAFE_DELETE(m_pPetInfo);
        SAFE_DELETE(m_pNicknameInfo);
    }

    __END_CATCH_NO_RETHROW
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void GCAddVampire::read(SocketInputStream& iStream)

{
    __BEGIN_TRY

    // A reader's records are its own; a filler's belong to the creature, so
    // any it installed are dropped, unowned, before the flag is set.
    m_pPetInfo = NULL;
    m_pNicknameInfo = NULL;
    m_bOwnsRecords = true;

    m_VampireInfo.read(iStream);

    m_pEffectInfo = new EffectInfo();
    m_pEffectInfo->read(iStream);

    m_pPetInfo = new PetInfo();
    m_pPetInfo->read(iStream);

    if (m_pPetInfo->getPetType() == PET_NONE)
        SAFE_DELETE(m_pPetInfo);

    m_pNicknameInfo = new NicknameInfo;
    m_pNicknameInfo->read(iStream);

    m_StoreOutlook.read(iStream);

    iStream.read(m_FromFlag);

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void GCAddVampire::write(SocketOutputStream& oStream) const

{
    __BEGIN_TRY

    // A packet carrying no effect record puts an empty list on the wire and
    // one carrying no pet puts an empty pet record there.
    EffectInfo noEffects;
    PetInfo NullPetInfo;

    m_VampireInfo.write(oStream);

    const EffectInfo& effects = (m_pEffectInfo != NULL) ? *m_pEffectInfo : noEffects;
    effects.write(oStream);

    if (m_pPetInfo == NULL)
        NullPetInfo.write(oStream);
    else {
        m_pPetInfo->setSummonInfo(0);
        m_pPetInfo->write(oStream);
    }

    if (m_pNicknameInfo == NULL) {
        NicknameInfo noNick;
        noNick.setNicknameType(NicknameInfo::NICK_NONE);
        noNick.write(oStream);
    } else {
        m_pNicknameInfo->write(oStream);
    }

    m_StoreOutlook.write(oStream);
    oStream.write(m_FromFlag);

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
string GCAddVampire::toString() const

{
    __BEGIN_TRY

    StringStream msg;
    msg << "GCAddVampire(" << "VampireInfo:" << m_VampireInfo.toString()
        << ",EffectInfo:" << ((m_pEffectInfo != NULL) ? m_pEffectInfo->toString() : "NULL")
        << ",FromFlag:" << (int)m_FromFlag << ")";
    return msg.toString();

    __END_CATCH
}
