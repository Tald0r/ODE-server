//----------------------------------------------------------------------
//
// Filename    : LCPCList.cpp
// Written By  : Reiot
// Description :
//
//----------------------------------------------------------------------

// include files
#include "LCPCList.h"

#include <memory>

#include "PCOustersInfo.h"
#include "PCSlayerInfo.h"
#include "PCVampireInfo.h"

//----------------------------------------------------------------------
// constructor
//----------------------------------------------------------------------
LCPCList::LCPCList() {
    for (uint i = 0; i < SLOT_MAX; i++)
        m_pPCInfos[i] = NULL;
}


//----------------------------------------------------------------------
// destructor
//----------------------------------------------------------------------
LCPCList::~LCPCList()

{
    // The PC Type variables created on the heap have to be deleted.
    for (uint i = 0; i < SLOT_MAX; i++) {
        SAFE_DELETE(m_pPCInfos[i]);
    }
}


//----------------------------------------------------------------------
// Read data from the input stream (buffer) and initialise the packet.
//----------------------------------------------------------------------
void LCPCList::read(SocketInputStream& iStream)

{
    __BEGIN_TRY

    //--------------------------------------------------
    // Take the PC type information.
    //
    // *OPTMIZATION*
    //
    // Later this information should go into one byte and be handled with bit operations.
    //
    //--------------------------------------------------
    char pcTypes[SLOT_MAX];

    for (uint i = 0; i < SLOT_MAX; i++)
        iStream.read(pcTypes[i]);

    //--------------------------------------------------
    // Read the PC information body.
    //--------------------------------------------------
    for (uint j = 0; j < SLOT_MAX; j++) {
        switch (pcTypes[j]) {
        case 'S': {
            std::unique_ptr<PCSlayerInfo> pPCSlayerInfo(new PCSlayerInfo);
            pPCSlayerInfo->read(iStream);
            // get() then release(): the right side of `a[i] = p.release()` is
            // sequenced first, so p->getSlot() would read a null pointer.
            m_pPCInfos[pPCSlayerInfo->getSlot()] = pPCSlayerInfo.get();
            (void)pPCSlayerInfo.release();
        } break;

        case 'V': {
            std::unique_ptr<PCVampireInfo> pPCVampireInfo(new PCVampireInfo);
            pPCVampireInfo->read(iStream);
            m_pPCInfos[pPCVampireInfo->getSlot()] = pPCVampireInfo.get();
            (void)pPCVampireInfo.release();
        } break;

        case 'O': {
            std::unique_ptr<PCOustersInfo> pPCOustersInfo(new PCOustersInfo);
            pPCOustersInfo->read(iStream);
            m_pPCInfos[pPCOustersInfo->getSlot()] = pPCOustersInfo.get();
            (void)pPCOustersInfo.release();
        } break;

        case '0':
            break;

        default:
            throw InvalidProtocolException("invalid pc type");
        }
    }

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////
// Send the packet's binary image to the output stream (buffer).
//////////////////////////////////////////////////////////////////////
void LCPCList::write(SocketOutputStream& oStream) const

{
    __BEGIN_TRY

    //--------------------------------------------------
    // First write the PC type.
    //
    // Later this information should go into one byte and be handled with bit operations.
    //
    // ex>
    // 	S0V : Slayer-EMPTY-VAMPIRE
    // 	00S : EMPTY-EMPTY-SLAYER
    //
    //--------------------------------------------------
    for (uint i = 0; i < SLOT_MAX; i++) {
        if (m_pPCInfos[i]) { // m_pPCInfos[i] != NULL

            if (m_pPCInfos[i]->getPCType() == PC_SLAYER) {
                oStream.write('S');
            } else if (m_pPCInfos[i]->getPCType() == PC_VAMPIRE) { // case of PC_VAMPIRE
                oStream.write('V');
            } else {
                oStream.write('O');
            }

        } else { // m_pPCInfos[i] == NULL
            oStream.write('0');
        }
    }

    //--------------------------------------------------
    // Then write the PCType object body.
    //--------------------------------------------------
    for (uint j = 0; j < SLOT_MAX; j++) {
        if (m_pPCInfos[j] != NULL) {
            m_pPCInfos[j]->write(oStream);
        }
    }

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
PacketSize_t LCPCList::getPacketSize() const

{
    // write() emits one type char per slot before the info bodies; the old
    // code did not count them, under-reporting the size header by SLOT_MAX.
    PacketSize_t packetSize = SLOT_MAX;
    for (uint i = 0; i < SLOT_MAX; i++) {
        if (m_pPCInfos[i]) { // m_pPCInfos[i] != NULL
            packetSize += m_pPCInfos[i]->getSize();
        }
    }

    return packetSize;
}


//////////////////////////////////////////////////////////////////////
//
// get packet's debug string
//
//////////////////////////////////////////////////////////////////////
string LCPCList::toString() const

{
    __BEGIN_TRY

    StringStream msg;

    msg << "LCPCList(\n";

    for (uint i = 0; i < SLOT_MAX; i++)
        if (m_pPCInfos[i] != NULL)
            msg << m_pPCInfos[i]->toString() << "\n";
        else
            msg << "EMPTY SLOT\n";

    msg << ")";

    return msg.toString();

    __END_CATCH
}
