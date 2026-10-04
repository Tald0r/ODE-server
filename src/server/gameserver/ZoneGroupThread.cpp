//////////////////////////////////////////////////////////////////////////////
// Filename    : ZoneGroupThread.cc
// Written by  : reiot@ewestsoft.com
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "ZoneGroupThread.h"

#include "DB.h"
#include "GMServerInfo.h"
#include "KeepAlive.h"
#include "KernelContext.h"
#include "Profile.h"
#include "Properties.h"
#include "ServerContext.h"
#include "Timeval.h"
#include "VSDateTime.h"
#include "ZonePlayerManager.h"


#ifndef __FULL_PROFILE__
#undef beginProfileEx
#define beginProfileEx(name) ((void)0)
#undef endProfileEx
#define endProfileEx(name) ((void)0)
#endif

//////////////////////////////////////////////////////////////////////////////
// constructor
//////////////////////////////////////////////////////////////////////////////
ZoneGroupThread::ZoneGroupThread(ZoneGroup* pZoneGroup)

    : m_pZoneGroup(pZoneGroup){__BEGIN_TRY __END_CATCH}

      //////////////////////////////////////////////////////////////////////////////
      // destructor
      //////////////////////////////////////////////////////////////////////////////
      ZoneGroupThread::~ZoneGroupThread() noexcept

{
    __BEGIN_TRY

    stop();
    join();

    __END_CATCH_NO_RETHROW
}

//////////////////////////////////////////////////////////////////////////////
// Thread methods are the outermost frame, so they need no __BEGIN_TRY and
// __END_CATCH: every exception has to be caught and handled here.
//////////////////////////////////////////////////////////////////////////////
void ZoneGroupThread::run()

{
    __BEGIN_DEBUG

    Properties& config = de::kernelContext().config();

    // From here on, this thread owns the zone group's state: arm the
    // debug ownership check (see CLAUDE.md, "Thread ownership").
    m_pZoneGroup->armOwnershipAssert();

    DatabaseManager& database = de::serverContext().database();

    string host = config.getProperty("DB_HOST");
    string db = config.getProperty("DB_DB");
    string user = config.getProperty("DB_USER");
    string password = config.getProperty("DB_PASSWORD");
    uint port = 0;
    if (config.hasKey("DB_PORT"))
        port = config.getPropertyInt("DB_PORT");

    if (stopRequested())
        return;
    Connection* pConnection = new Connection(host, db, user, password, port);
    database.addConnection((int)(long)Thread::self(), pConnection);
    cout << "******************************************************" << endl;
    cout << " THREAD CONNECT DB " << endl;
    cout << "******************************************************" << endl;

    string dist_host = config.getProperty("UI_DB_HOST");
    string dist_db = "DARKEDEN";
    string dist_user = config.getProperty("UI_DB_USER");
    string dist_password = config.getProperty("UI_DB_PASSWORD");
    uint dist_port = 0;
    if (config.hasKey("UI_DB_PORT"))
        dist_port = config.getPropertyInt("UI_DB_PORT");

    if (stopRequested())
        return;
    Connection* pDistConnection = new Connection(dist_host, dist_db, dist_user, dist_password, dist_port);
    database.addDistConnection(((int)(long)Thread::self()), pDistConnection);
    cout << "******************************************************" << endl;
    cout << " THREAD CONNECT UIIRIBUTION DB " << endl;
    cout << " TID Number = " << (int)(long)Thread::self() << endl;
    cout << "******************************************************" << endl;

    Timeval NextTime;
    getCurrentTime(NextTime);
    Timeval currentTime;

    NextTime.tv_sec += 2;

    Timeval dummyQueryTime;
    getCurrentTime(dummyQueryTime);

    try {
        while (!stopRequested()) {
            try {
                beginProfileExNoTry("ZGT_MAIN");

                if (!pauseFor(std::chrono::milliseconds(1))) {
                    endProfileExNoCatch("ZGT_MAIN");
                    break;
                }

                __ENTER_CRITICAL_SECTION((*m_pZoneGroup))

                // Group-level commands other threads posted run first, under
                // the group mutex we now hold -- see ZoneGroup::post(). (Work
                // aimed at one player rides the player's own box, drained in
                // ZonePlayerManager::processCommands below.)
                beginProfileEx("ZG_MAILBOX");
                m_pZoneGroup->drainMailbox();
                endProfileEx("ZG_MAILBOX");

                beginProfileEx("ZG_PP");
                m_pZoneGroup->processPlayers(); // process all players in ZonePlayerManager;
                endProfileEx("ZG_PP");

                beginProfileEx("ZG_HEARTBEAT");
                m_pZoneGroup->heartbeat(); // process all npc, monster, ... in Zones
                endProfileEx("ZG_HEARTBEAT");

                __LEAVE_CRITICAL_SECTION((*m_pZoneGroup))

                getCurrentTime(currentTime);

                endProfileExNoCatch("ZGT_MAIN");
            } catch (Throwable&) {
                endProfileExNoCatch("ZGT_MAIN");
                throw;
            }

            if (dummyQueryTime < currentTime) {
                database.executeDummyQuery(pConnection);
                database.executeDummyQuery(pDistConnection);

                dummyQueryTime = de::nextKeepAliveDeadline(dummyQueryTime, rand());
            }


            if (NextTime < currentTime) {
                GMServerInfo gmServerInfo;
                m_pZoneGroup->makeZoneUserInfo(gmServerInfo);

                (g_ProfileSampleManager.getProfileSampleSet())
                    ->outputProfileToFile("Profile", false, false, &gmServerInfo);


                NextTime.tv_sec = currentTime.tv_sec + 10;
                NextTime.tv_usec = currentTime.tv_usec;

                // Reset the profile data every turn,
                // to measure times per period rather than accumulated totals.
                initProfileEx();
            }
        }

    } catch (Throwable& t) {
        diagnosticFilelog("zoneGroupThreadError.log", "%s", t.toString().c_str());
        throw;
    }

    __END_DEBUG
}

//////////////////////////////////////////////////////////////////////////////
// get debug string
//////////////////////////////////////////////////////////////////////////////
string ZoneGroupThread::toString() const

{
    StringStream msg;
    msg << "ZoneGroupThread(" << m_pZoneGroup->toString() << ")";
    return msg.toString();
}
