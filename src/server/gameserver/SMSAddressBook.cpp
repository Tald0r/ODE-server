#include "SMSAddressBook.h"

#include "GCAddressListVerify.h"
#include "PlayerCreature.h"
#include "repository/SMSAddressRepository.h"

AddressUnit* SMSAddressElement::getAddressUnit() const {
    AddressUnit* pRet = new AddressUnit;
    pRet->ElementID = m_ElementID;
    pRet->CharacterName = m_CharacterName;
    pRet->CustomName = m_CustomName;
    pRet->Number = m_Number;

    return pRet;
}

SMSAddressBook::~SMSAddressBook() {
    unordered_map<DWORD, SMSAddressElement*>::iterator itr = m_Addresses.begin();
    unordered_map<DWORD, SMSAddressElement*>::iterator endItr = m_Addresses.end();

    for (; itr != endItr; ++itr) {
        SAFE_DELETE(itr->second);
    }
}

void SMSAddressBook::load() {
    m_Addresses.clear();
    Assert(m_pOwner != NULL);

    m_NextEID = 1;

    vector<SMSAddressRow> rows = defaultSMSAddressRepository().load(m_pOwner->getName());

    for (size_t r = 0; r < rows.size(); r++) {
        SMSAddressElement* pElement =
            new SMSAddressElement(rows[r].eID, rows[r].characterName, rows[r].customName, rows[r].number);

        //			Assert( addAddressElement( pElement ) );
        Assert(m_Addresses[pElement->getID()] == NULL);
        m_Addresses[pElement->getID()] = pElement;
        if (m_NextEID <= pElement->getID())
            m_NextEID = pElement->getID() + 1;
    }
}

GCSMSAddressList* SMSAddressBook::getGCSMSAddressList() const {
    GCSMSAddressList* pRet = new GCSMSAddressList;

    unordered_map<DWORD, SMSAddressElement*>::const_iterator itr = m_Addresses.begin();
    unordered_map<DWORD, SMSAddressElement*>::const_iterator endItr = m_Addresses.end();

    for (; itr != endItr; ++itr) {
        if (itr->second != NULL)
            pRet->getAddresses().push_back(itr->second->getAddressUnit());
    }

    return pRet;
}

int SMSAddressBook::addAddressElement(SMSAddressElement* pElement) {
    // The listing packet carries MAX_ADDRESS_NUM entries, so the book
    // may not pass it.
    if (m_Addresses.size() >= MAX_ADDRESS_NUM)
        return GCAddressListVerify::ADD_FAIL_MAX_NUM_EXCEEDED;
    if (m_Addresses[pElement->getID()] != NULL)
        return GCAddressListVerify::ADD_FAIL_INVALID_DATA;

    m_Addresses[pElement->getID()] = pElement;

    defaultSMSAddressRepository().insert(m_pOwner->getName(), pElement->m_ElementID, pElement->m_CharacterName,
                                         pElement->m_CustomName, pElement->m_Number);

    return 0;
}

int SMSAddressBook::removeAddressElement(DWORD eID) {
    unordered_map<DWORD, SMSAddressElement*>::iterator itr = m_Addresses.find(eID);

    if (itr == m_Addresses.end())
        return GCAddressListVerify::DELETE_FAIL_NO_SUCH_EID;

    m_Addresses.erase(itr);

    defaultSMSAddressRepository().remove(m_pOwner->getName(), eID);

    return 0;
}
