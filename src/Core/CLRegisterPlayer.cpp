//////////////////////////////////////////////////////////////////////////////
// Filename    : CLRegisterPlayer.cpp
// Written By  : Reiot
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "CLRegisterPlayer.h"

#include "WireString.h"

void CLRegisterPlayer::read(SocketInputStream& iStream)

{
    __BEGIN_TRY

    // Player's basic information (ID - Password)
    de::wire::readString(iStream, m_ID, {minIDLength, maxIDLength}, "ID");

    de::wire::readString(iStream, m_Password, {minPasswordLength, maxPasswordLength}, "Password");

    // Player's personal information (Name - Sex - SSN)
    de::wire::readString(iStream, m_Name, {1, maxNameLength}, "Name");

    BYTE sex;
    iStream.read(sex);

    // Sex has two enumerators and Sex2String two entries. The byte is checked
    // before it becomes a Sex, for the same reason CLCreatePC checks its slot.
    if (sex > (BYTE)MALE)
        throw InvalidProtocolException("sex out of range");

    m_Sex = (Sex)sex;

    de::wire::readString(iStream, m_SSN, {1, maxSSNLength}, "SSN");

    // Player's contact details (Telephone - Cellular - ZipCode - Address - Nation)
    de::wire::readString(iStream, m_Telephone, {1, maxTelephoneLength}, "Telephone");

    de::wire::readString(iStream, m_Cellular, {1, maxCellularLength}, "Cellular");

    de::wire::readString(iStream, m_ZipCode, {1, maxZipCodeLength}, "ZipCode");

    de::wire::readString(iStream, m_Address, {1, maxAddressLength}, "Address");

    BYTE nation;
    iStream.read(nation);
    m_Nation = (Nation)nation;

    // Player's electronic details (Email - Homepage)
    de::wire::readString(iStream, m_Email, {1, maxEmailLength}, "Email");

    de::wire::readString(iStream, m_Homepage, {1, maxHomepageLength}, "Homepage");

    // Other (Profile - Public)
    de::wire::readString(iStream, m_Profile, {1, maxProfileLength}, "Profile");

    iStream.read(m_bPublic);

    __END_CATCH
}

void CLRegisterPlayer::write(SocketOutputStream& oStream) const

{
    __BEGIN_TRY

    // Player's basic information (ID - Password)
    de::wire::writeString(oStream, m_ID, {minIDLength, maxIDLength}, "ID");

    de::wire::writeString(oStream, m_Password, {minPasswordLength, maxPasswordLength}, "Password");

    // Player's personal information (Name - Sex - SSN)
    de::wire::writeString(oStream, m_Name, {1, maxNameLength}, "Name");

    oStream.write((BYTE)m_Sex);

    de::wire::writeString(oStream, m_SSN, {1, maxSSNLength}, "SSN");

    // Player's contact details (Telephone - Cellular - ZipCode - Address - Nation)
    de::wire::writeString(oStream, m_Telephone, {1, maxTelephoneLength}, "Telephone");

    de::wire::writeString(oStream, m_Cellular, {1, maxCellularLength}, "Cellular");

    de::wire::writeString(oStream, m_ZipCode, {1, maxZipCodeLength}, "ZipCode");

    de::wire::writeString(oStream, m_Address, {1, maxAddressLength}, "Address");

    oStream.write((BYTE)m_Nation);

    // Player's electronic details (Email - Homepage)
    de::wire::writeString(oStream, m_Email, {1, maxEmailLength}, "Email");

    de::wire::writeString(oStream, m_Homepage, {1, maxHomepageLength}, "Homepage");

    // Other (Profile - Public)
    de::wire::writeString(oStream, m_Profile, {1, maxProfileLength}, "Profile");

    oStream.write(m_bPublic);

    __END_CATCH
}

string CLRegisterPlayer::toString() const

{
    __BEGIN_TRY

    StringStream msg;
    msg << "CLRegisterPlayer(" << "ID:" << m_ID << ",Password:" << m_Password << ",Name:" << m_Name
        << ",Sex:" << nameOrNumber(Sex2String, m_Sex) << ",SSN:" << m_SSN << ",Telephone:" << m_Telephone
        << ",Cellular:" << m_Cellular << ",ZipCode:" << m_ZipCode << ",Address:" << m_Address
        << ",Nation:" << nameOrNumber(Nation2String, m_Nation) << ",e-mail:" << m_Email << ",Homepage:" << m_Homepage
        << ",Profile:" << m_Profile << ",Public:" << ((m_bPublic == true) ? "PUBLIC" : "PRIVATE") << ")";
    return msg.toString();

    __END_CATCH
}
