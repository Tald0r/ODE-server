///////////////////////////////////////////////////////////////////
// General war information and the routines run when a war starts and ends
///////////////////////////////////////////////////////////////////

#include "War.h"

#include <stdio.h>

#include "Assert.h"
#include "CastleInfoManager.h"
#include "DB.h"
#include "GCNoticeEvent.h"
#include "GCSystemMessage.h"
#include "GameContext.h"
#include "HolyLandRaceBonus.h"
#include "KernelContext.h"
#include "Mutex.h"
#include "PCManager.h"
#include "Properties.h"
#include "ShrineInfoManager.h"
#include "StringPool.h"
#include "StringStream.h"
#include "WarSystem.h"
#include "Zone.h"
#include "ZoneGroup.h"
#include "ZoneGroupManager.h"
#include "ZoneInfoManager.h"
#include "ZoneUtil.h"
#include "repository/WarInfoRepository.h"

//--------------------------------------------------------------------------------
// static members
//--------------------------------------------------------------------------------
Mutex War::m_Mutex;
WarID_t War::m_WarIDRegistry = 0;

//--------------------------------------------------------------------------------
//
// constructor / destructor
//
//--------------------------------------------------------------------------------
// A new war takes the next id under the registry's mutex: castle wars are
// registered from the castle NPCs' zone threads and the race war from the
// main thread, so two could otherwise read the same id. The mutex is a leaf.
War::War(WarState warState, WarID_t warID) : m_State(warState) {
    if (warID == 0) {
        __ENTER_CRITICAL_SECTION(m_Mutex)

        m_WarIDRegistry += de::gameContext().warSystem().getWarIDSuccessor();
        m_WarID = m_WarIDRegistry;

        __LEAVE_CRITICAL_SECTION(m_Mutex)
    } else {
        m_WarID = warID;
    }
}

War::~War() {}

//--------------------------------------------------------------------------------
//
// init WarID Registry
//
//--------------------------------------------------------------------------------
void War::initWarIDRegistry()

{
    __BEGIN_TRY

    m_WarIDRegistry = 0;

    __ENTER_CRITICAL_SECTION(m_Mutex)

    WarInfoRepository& repository = defaultWarInfoRepository();

    if (repository.countWarSchedules() != 0)
        m_WarIDRegistry = repository.loadMaxWarID();

    static WarID_t WarIDBase = de::kernelContext().config().getPropertyInt("ServerID");
    static WarID_t WarIDSuccessor = de::kernelContext().config().getPropertyInt("ServerCount");
    m_WarIDRegistry += (WarIDSuccessor - (m_WarIDRegistry % WarIDSuccessor)) + WarIDBase;

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    cout << "War::WarIDRegistry:" << m_WarIDRegistry << endl;

    __END_CATCH
}

const string& War::getState2DBString() const {
    Assert(m_State < MAX_WAR_STATE);

    static string WarState[] = {"WAIT", "START", "END", "CANCEL"};

    return WarState[m_State];
}

//--------------------------------------------------------------------------------
//
// execute
//
//--------------------------------------------------------------------------------
void War::execute()

{
    __BEGIN_TRY

    switch (m_State) {
    case WAR_STATE_WAIT:
        executeStart();
        m_State = WAR_STATE_CURRENT;
        break;

    case WAR_STATE_CURRENT:
        executeEnd();
        m_State = WAR_STATE_END;
        break;

    case WAR_STATE_END:
        Assert(false);
        break;

    default:
        Assert(false);
    }

    // After execute(), WarSchedule's heartbeat() sets the Status in the DB.

    __END_CATCH
}

//--------------------------------------------------------------------------------
//
// send Message
//
//--------------------------------------------------------------------------------
// When a war starts
//--------------------------------------------------------------------------------
void War::sendWarStartMessage() const

{
    __BEGIN_TRY

    GCSystemMessage gcSystemMessage;
    char str[80];
    snprintf(str, sizeof(str), de::gameContext().strings().c_str(STRID_WAR_START), getWarName().c_str());

    gcSystemMessage.setMessage(str);
    de::gameContext().zoneGroups().broadcast(&gcSystemMessage);

    filelog("WarLog.txt", "[WarID=%u] %s", (int)m_WarID, str);

    __END_CATCH
}

//--------------------------------------------------------------------------------
// When the war ends
//--------------------------------------------------------------------------------
void War::sendWarEndMessage() const

{
    __BEGIN_TRY

    GCSystemMessage gcSystemMessage;
    char str[80];
    snprintf(str, sizeof(str), de::gameContext().strings().c_str(STRID_WAR_END), getWarName().c_str());

    gcSystemMessage.setMessage(str);
    de::gameContext().zoneGroups().broadcast(&gcSystemMessage);

    filelog("WarLog.txt", "[WarID=%u] %s", (int)m_WarID, str);

    __END_CATCH
}
