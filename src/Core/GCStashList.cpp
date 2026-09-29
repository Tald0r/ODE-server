//////////////////////////////////////////////////////////////////////////////
// Filename    : GCStashList.cpp
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "GCStashList.h"

#include "Assert1.h"

//////////////////////////////////////////////////////////////////////////////
// constructor
//////////////////////////////////////////////////////////////////////////////

GCStashList::GCStashList() {}

//////////////////////////////////////////////////////////////////////////////
// destructor
//////////////////////////////////////////////////////////////////////////////
GCStashList::~GCStashList()

{
    __BEGIN_TRY

    for (int r = 0; r < STASH_RACK_MAX; r++) {
        for (int i = 0; i < STASH_INDEX_MAX; i++) {
            list<SubItemInfo*>::iterator itr = m_pSubItems[r][i].begin();
            for (; itr != m_pSubItems[r][i].end(); itr++) {
                SubItemInfo* pItemInfo = *itr;
                SAFE_DELETE(pItemInfo);
            }
        }
    }

    __END_CATCH_NO_RETHROW
}

//////////////////////////////////////////////////////////////////////////////
// Read data from the input stream (buffer) and initialise the packet.
//////////////////////////////////////////////////////////////////////////////
void GCStashList::read(SocketInputStream& iStream)

{
    __BEGIN_TRY

    BYTE i = 0;
    BYTE nTotal = 0;
    BYTE rack = 0;
    BYTE index = 0;

    // The listing replaces the stash the packet holds.
    for (int r = 0; r < STASH_RACK_MAX; r++) {
        for (int s = 0; s < STASH_INDEX_MAX; s++) {
            m_bExist[r][s] = false;
            m_pItems[r][s] = _STASHITEM();

            list<SubItemInfo*>::iterator itr = m_pSubItems[r][s].begin();
            for (; itr != m_pSubItems[r][s].end(); itr++) {
                SubItemInfo* pItemInfo = *itr;
                SAFE_DELETE(pItemInfo);
            }
            m_pSubItems[r][s].clear();
        }
    }

    // Read the number of stashes.
    iStream.read(m_StashNum);

    // Read the total number of items.
    iStream.read(nTotal);

    // Read the information of each item.
    for (i = 0; i < nTotal; i++) {
        iStream.read(rack);
        iStream.read(index);
        if (rack >= STASH_RACK_MAX || index >= STASH_INDEX_MAX)
            throw InvalidProtocolException("stash slot out of range");
        _STASHITEM& item = m_pItems[rack][index];
        iStream.read(item.objectID);
        iStream.read(item.itemClass);
        iStream.read(item.itemType);

        BYTE optionSize;
        iStream.read(optionSize);

        if (optionSize > STASHITEM::kMaxOptionCount)
            throw InvalidProtocolException("too many item options");

        for (int j = 0; j < optionSize; j++) {
            OptionType_t optionType;
            iStream.read(optionType);
            item.optionType.push_back(optionType);
        }

        iStream.read(item.durability);
        iStream.read(item.num);
        iStream.read(item.silver);
        iStream.read(item.grade);
        iStream.read(item.enchantLevel);

        // Read the sub-item information.
        BYTE subItemCount;
        iStream.read(subItemCount);

        if (subItemCount > kMaxSubItemCount)
            throw InvalidProtocolException("too many sub items");

        for (int s = 0; s < subItemCount; s++) {
            SubItemInfo* pSubItemInfo = new SubItemInfo();
            pSubItemInfo->read(iStream);
            m_pSubItems[rack][index].push_back(pSubItemInfo);
        }

        m_bExist[rack][index] = true;
    }

    // Read the amount of money.
    iStream.read(m_StashGold);

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////////////
// Send the packet's binary image to the output stream (buffer).
//////////////////////////////////////////////////////////////////////////////
void GCStashList::write(SocketOutputStream& oStream) const

{
    __BEGIN_TRY
    __BEGIN_DEBUG

    BYTE r = 0;
    BYTE i = 0;
    BYTE nTotal = 0;

    // Count the total number of items.
    for (r = 0; r < STASH_RACK_MAX; r++)
        for (i = 0; i < STASH_INDEX_MAX; i++)
            if (m_bExist[r][i])
                nTotal++;

    // Write the number of stashes.
    oStream.write(m_StashNum);

    // Write the total number of items
    oStream.write(nTotal);

    // write each item info
    for (r = 0; r < STASH_RACK_MAX; r++) {
        for (i = 0; i < STASH_INDEX_MAX; i++) {
            if (m_bExist[r][i]) {
                const _STASHITEM& item = m_pItems[r][i];
                oStream.write(r);
                oStream.write(i);
                oStream.write(item.objectID);
                oStream.write(item.itemClass);
                oStream.write(item.itemType);

                if (item.optionType.size() > STASHITEM::kMaxOptionCount)
                    throw InvalidProtocolException("too many item options");

                BYTE optionSize = item.optionType.size();
                oStream.write(optionSize);
                list<OptionType_t>::const_iterator iOption;
                for (iOption = item.optionType.begin(); iOption != item.optionType.end(); iOption++) {
                    OptionType_t optionType = *iOption;
                    oStream.write(optionType);
                }

                oStream.write(item.durability);
                oStream.write(item.num);
                oStream.write(item.silver);
                oStream.write(item.grade);
                oStream.write(item.enchantLevel);

                // Write the sub-item information.
                if (m_pSubItems[r][i].size() > kMaxSubItemCount)
                    throw InvalidProtocolException("too many sub items");

                oStream.write((BYTE)m_pSubItems[r][i].size());

                list<SubItemInfo*>::const_iterator itr = m_pSubItems[r][i].begin();
                for (; itr != m_pSubItems[r][i].end(); itr++) {
                    // The count is the list, so an entry with no record
                    // would leave the body one record short of it.
                    if (*itr == NULL)
                        throw InvalidProtocolException("sub item record missing");
                    (*itr)->write(oStream);
                }
            }
        }
    }

    // Write the amount of money.
    oStream.write(m_StashGold);

    __END_DEBUG
    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
PacketSize_t GCStashList::getPacketSize() const

{
    __BEGIN_TRY
    __BEGIN_DEBUG

    PacketSize_t size = szBYTE; // Number of stashes

    size += szBYTE; // Total number of items

    for (int r = 0; r < STASH_RACK_MAX; r++) {
        for (int i = 0; i < STASH_INDEX_MAX; i++) {
            if (m_bExist[r][i]) {
                // rack and index
                size += szBYTE * 2;

                // Actual information
                size += m_pItems[r][i].getPacketSize();

                // Number of items in the belt
                size += szBYTE;

                // Size of the items in the belt
                size += SubItemInfo::getSize() * m_pSubItems[r][i].size();
            }
        }
    }

    size += szGold; // Money in the stash

    return size;

    __END_DEBUG
    __END_CATCH
}


//////////////////////////////////////////////////////////////////////////////
// get packet's debug string
//////////////////////////////////////////////////////////////////////////////
string GCStashList::toString() const

{
    __BEGIN_TRY

    StringStream msg;
    msg << "GCStashList(";
    for (int r = 0; r < STASH_RACK_MAX; r++) {
        for (int i = 0; i < STASH_INDEX_MAX; i++) {
            msg << "(Item:" << r << ", " << i << ":";
            if (m_bExist[r][i]) {
                msg << "ObjectID:" << (int)(m_pItems[r][i].objectID) << "ItemClass:" << (int)(m_pItems[r][i].itemClass)
                    << "ItemType:" << (int)(m_pItems[r][i].itemType)
                    << "OptionTypeSize:" << (int)(m_pItems[r][i].optionType.size())
                    << "Durability:" << (int)(m_pItems[r][i].durability) << "Num:" << (int)(m_pItems[r][i].num)
                    << "Silver:" << (int)(m_pItems[r][i].silver) << "Grade:" << (int)(m_pItems[r][i].grade)
                    << "EnchantLevel:" << (int)(m_pItems[r][i].enchantLevel);
            }
            msg << ")";
        }
    }
    msg << ")";
    return msg.toString();

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
bool GCStashList::isExist(BYTE rack, BYTE index) const

{
    __BEGIN_TRY

    Assert(rack < STASH_RACK_MAX && index < STASH_INDEX_MAX);
    return m_bExist[rack][index];

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
STASHITEM GCStashList::getStashItem(BYTE rack, BYTE index) const

{
    __BEGIN_TRY

    Assert(rack < STASH_RACK_MAX && index < STASH_INDEX_MAX);
    return m_pItems[rack][index];

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
// setStashItem(BYTE, BYTE, Item*) is defined in the gameserver
// (packetfill/GCStashListFill.cpp): it converts a live Item into wire
// fields, which the wire library must not depend on.

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
list<SubItemInfo*>& GCStashList::getSubItems(BYTE rack, BYTE index)

{
    __BEGIN_TRY

    Assert(rack < STASH_RACK_MAX && index < STASH_INDEX_MAX);
    return m_pSubItems[rack][index];

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
BYTE GCStashList::getSubItemCount(BYTE rack, BYTE index) const

{
    __BEGIN_TRY

    Assert(rack < STASH_RACK_MAX && index < STASH_INDEX_MAX);
    return (BYTE)m_pSubItems[rack][index].size();

    __END_CATCH
}
