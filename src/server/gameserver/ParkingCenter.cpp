//////////////////////////////////////////////////////////////////////////////
// Filename    : ParkingCenter.cpp
// Written By  : Reiot
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "ParkingCenter.h"

#include "GCDeleteObject.h"

//////////////////////////////////////////////////////////////////////////////
// class MotorcycleBox member methods
//////////////////////////////////////////////////////////////////////////////
MotorcycleBox::MotorcycleBox(Motorcycle* pMotorcycle, Zone* pZone, ZoneCoord_t X, ZoneCoord_t Y)

{
    m_pMotorcycle = pMotorcycle;
    m_pZone = pZone;
    m_X = X;
    m_Y = Y;
    m_bTransport = false;
}

MotorcycleBox::~MotorcycleBox()

{
    try {
        if (m_pMotorcycle != NULL) {
            // Deleted through m_pZone because of an m_pZone synchronization problem.
            // by sigi. 2002.5.3
            m_pZone->deleteMotorcycle(m_X, m_Y, m_pMotorcycle);
        }

        m_pZone = NULL;
    } catch (Throwable&) {
        // A Throwable from the try block is dropped here and goes no further.
    }
}

//////////////////////////////////////////////////////////////////////////////
// class ParkingCenter member methods
//////////////////////////////////////////////////////////////////////////////

ParkingCenter::ParkingCenter()

{
    m_Mutex.setName("ParkingCenter");
    m_MutexRemove.setName("ParkingCenterRemove");
}

ParkingCenter::~ParkingCenter()

{
    // No lock: nothing else may use the parking center while it is destroyed.
    unordered_map<ItemID_t, MotorcycleBox*>::iterator itr = m_Motorcycles.begin();

    for (; itr != m_Motorcycles.end(); itr++) {
        MotorcycleBox* pBox = itr->second;
        SAFE_DELETE(pBox);
    }

    m_Motorcycles.clear();
}

void ParkingCenter::addMotorcycleBox(MotorcycleBox* pMotorcycleBox)

{
    __ENTER_CRITICAL_SECTION(m_Mutex)

    Assert(pMotorcycleBox != NULL);

    unordered_map<ItemID_t, MotorcycleBox*>::iterator itr = m_Motorcycles.find(pMotorcycleBox->getItemID());

    if (itr != m_Motorcycles.end()) {
        throw DuplicatedException();
    }

    m_Motorcycles[pMotorcycleBox->getItemID()] = pMotorcycleBox;

    __LEAVE_CRITICAL_SECTION(m_Mutex)
}

// Removes the motorcycle matching the key's TargetID from the unordered_map.
// This deletes the whole motorcycle, so remove the motorcycle from the zone first
// and call this function last.
void ParkingCenter::deleteMotorcycleBox(ItemID_t keyTargetID) {
    MotorcycleBox* pMotorcycleBox = NULL;

    __ENTER_CRITICAL_SECTION(m_Mutex)

    unordered_map<ItemID_t, MotorcycleBox*>::iterator itr = m_Motorcycles.find(keyTargetID);

    if (itr == m_Motorcycles.end()) {
        // cerr << "ParkingCenter::deleteMotorcycleBox() : NoSuchElementException" << endl;
        // throw NoSuchElementException();

        return;
    }

    pMotorcycleBox = itr->second;

    m_Motorcycles.erase(itr);

    // Not deleted right away, to avoid a deadlock.
    // SAFE_DELETE(pMotorcycleBox);

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    // heartbeat() deletes it later.
    if (pMotorcycleBox != NULL) {
        __ENTER_CRITICAL_SECTION(m_MutexRemove)

        m_RemoveMotorcycles.push_back(pMotorcycleBox);

        __LEAVE_CRITICAL_SECTION(m_MutexRemove)
    }
}

// Checks whether a MotorcycleBox with the given KeyID exists.
bool ParkingCenter::hasMotorcycleBox(ItemID_t keyTargetID) {
    bool hasMotor = true;

    __ENTER_CRITICAL_SECTION(m_Mutex)

    unordered_map<ItemID_t, MotorcycleBox*>::iterator itr = m_Motorcycles.find(keyTargetID);

    if (itr == m_Motorcycles.end()) {
        hasMotor = false;
    }

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    return hasMotor;
}

// Finds and returns the motorcycle by the key's TargetID.
MotorcycleBox* ParkingCenter::getMotorcycleBox(ItemID_t keyTargetID) const {
    MotorcycleBox* pTempBox = NULL;

    try {
        __ENTER_CRITICAL_SECTION(m_Mutex)

        unordered_map<ItemID_t, MotorcycleBox*>::const_iterator itr = m_Motorcycles.find(keyTargetID);

        if (itr == m_Motorcycles.end()) {
            // cerr << "ParkingCenter::getMotorcycleBox() : NoSuchElementException" << endl;
            // throw NoSuchElementException();

            return NULL;
        }

        pTempBox = itr->second;

        __LEAVE_CRITICAL_SECTION(m_Mutex)

        return pTempBox;
    } catch (Throwable& t) {
        // cerr << "Not found yet, or the key does not match this motorcycle." << endl;
        return NULL;
    }
}

// This runs on the ClientManager thread.
void ParkingCenter::heartbeat()

{
    __ENTER_CRITICAL_SECTION(m_MutexRemove)

    list<MotorcycleBox*>::iterator itr = m_RemoveMotorcycles.begin();

    for (; itr != m_RemoveMotorcycles.end(); itr++) {
        MotorcycleBox* pMotorcycleBox = *itr;

        SAFE_DELETE(pMotorcycleBox);
    }

    m_RemoveMotorcycles.clear();

    __LEAVE_CRITICAL_SECTION(m_MutexRemove)
}
