//////////////////////////////////////////////////////////////////////
//
// Filename    : ZoneGroupThread.h
// Written by  : reiot@ewestsoft.com
// Description :
//
//////////////////////////////////////////////////////////////////////

#ifndef __ZONE_THREAD_H__
#define __ZONE_THREAD_H__

#include <chrono>
#include <mutex>

#include <condition_variable>
#include <stop_token>

// include files
#include "Exception.h"
#include "ManagedThread.h"
#include "Thread.h"
#include "Types.h"
#include "ZoneGroup.h"

//////////////////////////////////////////////////////////////////////
//
// class ZoneGroupThread;
//
// Thread that takes charge of a single ZoneGroup: the PCs bound to the zone
// group, the NPCs and MOBs in its zones, and the zones' various processing.
// Processing inside a zone group is therefore sequential.
//
//////////////////////////////////////////////////////////////////////

class ZoneGroupThread : public ManagedThread {
public:
    // constructor
    ZoneGroupThread(ZoneGroup* pZoneGroup);

    // destructor
    ~ZoneGroupThread() noexcept;

    // main method
    void run() override;

    // get debug string
    string toString() const override;

    // get thread's name
    string getName() const override {
        return "ZoneGroupThread";
    }

    ZoneGroup* getZoneGroup() {
        return m_pZoneGroup;
    }

private:
    ZoneGroup* m_pZoneGroup;
};

#endif
