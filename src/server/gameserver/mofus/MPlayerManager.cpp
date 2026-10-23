/////////////////////////////////////////////////////////////////////////////
// Filename : MPlayerManager.cpp
// Desc		:
/////////////////////////////////////////////////////////////////////////////

// include files
#include "MPlayerManager.h"

#include <unistd.h>

#include "Assert.h"
#include "DB.h"
#include "GCRequestPowerPointResult.h"
#include "GameContext.h"
#include "KeepAlive.h"
#include "KernelContext.h"
#include "MPlayer.h"
#include "Mofus.h"
#include "PCFinder.h"
#include "PKTError.h"
#include "PlayerCreature.h"
#include "Properties.h"
#include "ServerContext.h"

// constructor
MPlayerManager::MPlayerManager()

{
    m_Mutex.setName("MPlayerManager");
    m_pCurrentJob = NULL;
}

// destructor
MPlayerManager::~MPlayerManager() noexcept

{
    stop();
    join();
}

// stop thread. unsupport
void MPlayerManager::stop()

{
    ManagedThread::stop();
}

// main method
void MPlayerManager::run() {
    Properties& config = de::kernelContext().config();

    string host = config.getProperty("DB_HOST");
    string db = config.getProperty("DB_DB");
    string user = config.getProperty("DB_USER");
    string password = config.getProperty("DB_PASSWORD");
    uint port = 0;
    if (config.hasKey("DB_PORT"))
        port = config.getPropertyInt("DB_PORT");

    Connection* pConnection = new Connection(host, db, user, password, port);
    de::serverContext().database().addConnection((int)(long)Thread::self(), pConnection);
    cout << "******************************************************" << endl;
    cout << " Mofus THREAD CONNECT DB " << endl;
    cout << "******************************************************" << endl;

    Timeval dummyQueryTime;
    getCurrentTime(dummyQueryTime);

    while (!stopRequested()) {
        usleep(100);

        // Fetch a job if none is currently running.
        if (m_pCurrentJob == NULL) {
            m_pCurrentJob = popJob();
        }

        // If a job is available, process it.
        if (m_pCurrentJob != NULL) {
            // Spin up an MPlayer to handle the job.
            MPlayer* pPlayer = new MPlayer(m_pCurrentJob);

            // Run the player.
            pPlayer->process();

            // Persist any job results.
            processResult();

            // Clean up player and job objects.
            SAFE_DELETE(pPlayer);
            SAFE_DELETE(m_pCurrentJob);
        }

        Timeval currentTime;
        getCurrentTime(currentTime);

        if (dummyQueryTime < currentTime) {
            de::serverContext().database().executeDummyQuery(pConnection);

            dummyQueryTime = de::nextKeepAliveDeadline(dummyQueryTime, rand());
        }
    }
}

void MPlayerManager::addJob(const string& userID, const string& name, const string& cellnum) {
    // Allocate and enqueue a new job.
    MJob* pJob = new MJob(userID, name, cellnum);

    __ENTER_CRITICAL_SECTION(m_Mutex)

    m_Jobs.push_back(pJob);

    __LEAVE_CRITICAL_SECTION(m_Mutex)
}

MJob* MPlayerManager::popJob() {
    MJob* pJob = NULL;

    __ENTER_CRITICAL_SECTION(m_Mutex)

    if (!m_Jobs.empty()) {
        pJob = m_Jobs.front();
        m_Jobs.pop_front();
    }

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    return pJob;
}

void MPlayerManager::processResult() {
    // Locate the target player creature.
    PCFinder& pcFinder = de::gameContext().playerCreatures();

    __ENTER_CRITICAL_SECTION(pcFinder)

    Creature* pCreature = pcFinder.getCreature_LOCKED(m_pCurrentJob->getName());

    if (pCreature != NULL) {
        PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pCreature);
        Assert(pPC != NULL);

        // Refresh the player's PowerPoint from the database value.
        pPC->setPowerPoint(loadPowerPoint(pPC->getName()));

        GCRequestPowerPointResult gcRequestPowerPointResult;

        gcRequestPowerPointResult.setSumPowerPoint(pPC->getPowerPoint());
        gcRequestPowerPointResult.setRequestPowerPoint(m_pCurrentJob->getPowerPoint());

        if (m_pCurrentJob->getErrorCode() == 0) {
            gcRequestPowerPointResult.setErrorCode(0);
        } else {
            switch (m_pCurrentJob->getErrorCode()) {
            case MERR_SERVER:
                gcRequestPowerPointResult.setErrorCode(GCRequestPowerPointResult::SERVER_ERROR);
                break;

            case MERR_CONFIRM:
            case MERR_PACKET:
                gcRequestPowerPointResult.setErrorCode(GCRequestPowerPointResult::CONNECT_ERROR);
                break;

            case MERR_PROCESS:
                gcRequestPowerPointResult.setErrorCode(GCRequestPowerPointResult::PROCESS_ERROR);
                break;

            case MERR_SEARCH:
                gcRequestPowerPointResult.setErrorCode(GCRequestPowerPointResult::NO_MEMBER);
                break;

            case MERR_NULLPOINT:
                gcRequestPowerPointResult.setErrorCode(GCRequestPowerPointResult::NO_POINT);
                break;

            case MERR_MATCHING:
                gcRequestPowerPointResult.setErrorCode(GCRequestPowerPointResult::NO_MATCHING);
                break;

            default:
                filelog(MOFUS_ERROR_FILE, "INVALID ERROR CODE:%d", m_pCurrentJob->getErrorCode());
                return;
            }
        }

        // Send the result back to the client.
        pPC->getPlayer()->sendPacket(&gcRequestPowerPointResult);
    }

    __LEAVE_CRITICAL_SECTION(pcFinder)
}
