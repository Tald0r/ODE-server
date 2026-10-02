//--------------------------------------------------------------------------------
//
// Filename    : ThreadManager.cc
// Written By  : Reiot
//
//--------------------------------------------------------------------------------

// include files
#include "ThreadManager.h"

#include "Assert.h"
#include "GameContext.h"
#include "Properties.h"
#include "ThreadPool.h"
#include "ZoneGroupManager.h"
#include "ZoneGroupThread.h"
#include "repository/ZoneInfoRepository.h"


//--------------------------------------------------------------------------------
// constructor
//--------------------------------------------------------------------------------
ThreadManager::ThreadManager() = default;


//--------------------------------------------------------------------------------
//
// destructor
//
// Must be run when Stop() has not been called.
//
//--------------------------------------------------------------------------------
ThreadManager::~ThreadManager() = default;


//--------------------------------------------------------------------------------
//
// Initialize the thread manager.
//
// Create and register threads in the sub thread pools.
//
// *CAUTION*
//
// The zone group manager must be initialized before the thread manager.
//
//--------------------------------------------------------------------------------
void ThreadManager::init()

{
    __BEGIN_TRY

    // Register one thread per zone group.
    vector<int> zoneGroupIDs = defaultZoneInfoRepository().loadZoneGroupIDs(false);

    for (size_t i = 0; i < zoneGroupIDs.size(); i++) {
        ZoneGroupID_t zoneGroupID = zoneGroupIDs[i];
        m_ZoneGroupThreadPool.addThread(
            std::make_unique<ZoneGroupThread>(de::gameContext().zoneGroups().getZoneGroup(zoneGroupID)));
    }

    __END_CATCH
}


//--------------------------------------------------------------------------------
//
// activate sub thread pools
//
// Start the sub thread pools.
//
//--------------------------------------------------------------------------------
void ThreadManager::start()

{
    __BEGIN_TRY

    // Start the Zone Thread Pool.
    m_ZoneGroupThreadPool.start();

    __END_CATCH
}


//--------------------------------------------------------------------------------
//
// deactivate sub thread pools
//
// Stop the sub thread pools.
//
//--------------------------------------------------------------------------------
void ThreadManager::stop()

{
    __BEGIN_TRY

    m_ZoneGroupThreadPool.stop();

    __END_CATCH
}
