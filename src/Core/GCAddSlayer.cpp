//////////////////////////////////////////////////////////////////////////////
// Filename    : GCAddSlayer.cpp
// Written By  : Reiot
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "GCAddSlayer.h"

//////////////////////////////////////////////////////////////////////////////
// class GCAddSlayer member methods
//////////////////////////////////////////////////////////////////////////////

GCAddSlayer::GCAddSlayer() {
    m_pEffectInfo = NULL;
    m_pPetInfo = NULL;
    m_pNicknameInfo = NULL;
}

GCAddSlayer::GCAddSlayer(const PCSlayerInfo3& info) : m_SlayerInfo(info) {
    m_pEffectInfo = NULL;
    m_pPetInfo = NULL;
    m_pNicknameInfo = NULL;
}

GCAddSlayer::~GCAddSlayer() noexcept

{
    // The effect record is built for the packet, by read() here and by a
    // fresh EffectManager snapshot on the fill side, so the packet owns it.
    // A filler's pet and nickname records belong to the creature
    // (PacketUtil.cpp); read() allocates its own, and those go with the packet.
    SAFE_DELETE(m_pEffectInfo);
    releaseRecords();
}

// A filler's pet and nickname records belong to the creature
// (PacketUtil.cpp); read()'s are the packet's.
void GCAddSlayer::releaseRecords() {
    if (m_bOwnsRecords) {
        SAFE_DELETE(m_pPetInfo);
        SAFE_DELETE(m_pNicknameInfo);
    }
    m_pPetInfo = NULL;
    m_pNicknameInfo = NULL;
    m_bOwnsRecords = false;
}

void GCAddSlayer::read(SocketInputStream& iStream)

{
    __BEGIN_TRY

    // A reader's records are its own: a filler's are dropped, unowned, and
    // an earlier read's are freed, before the flag is set.
    releaseRecords();
    m_bOwnsRecords = true;

    m_SlayerInfo.read(iStream);
    m_pEffectInfo = new EffectInfo();
    m_pEffectInfo->read(iStream);

    m_pPetInfo = new PetInfo();
    m_pPetInfo->read(iStream);

    if (m_pPetInfo->getPetType() == PET_NONE)
        SAFE_DELETE(m_pPetInfo);

    m_pNicknameInfo = new NicknameInfo;
    m_pNicknameInfo->read(iStream);

    m_StoreOutlook.read(iStream);

    __END_CATCH
}

void GCAddSlayer::write(SocketOutputStream& oStream) const

{
    __BEGIN_TRY

    // A packet carrying no effect record puts an empty list on the wire and
    // one carrying no pet puts an empty pet record there.
    EffectInfo noEffects;
    PetInfo NullPetInfo;

    m_SlayerInfo.write(oStream);

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

    __END_CATCH
}

PacketSize_t GCAddSlayer::getPacketSize() const

{
    __BEGIN_TRY

    EffectInfo noEffects;
    const EffectInfo& effects = (m_pEffectInfo != NULL) ? *m_pEffectInfo : noEffects;

    PacketSize_t ret =
        m_SlayerInfo.getSize() + effects.getSize() + ((m_pPetInfo != NULL) ? m_pPetInfo->getSize() : szPetType);

    if (m_pNicknameInfo == NULL) {
        NicknameInfo noNick;
        noNick.setNicknameType(NicknameInfo::NICK_NONE);
        ret += noNick.getSize();
    } else {
        ret += m_pNicknameInfo->getSize();
    }

    ret += m_StoreOutlook.getSize();

    return ret;

    __END_CATCH
}

string GCAddSlayer::toString() const

{
    __BEGIN_TRY

    StringStream msg;
    msg << "GCAddSlayer(" << "SlayerInfo:" << m_SlayerInfo.toString()
        << "EffectInfo:" << ((m_pEffectInfo != NULL) ? m_pEffectInfo->toString() : "NULL")
        << "PetInfo:" << ((m_pPetInfo) ? m_pPetInfo->toString() : "NULL") << ")";
    return msg.toString();

    __END_CATCH
}
