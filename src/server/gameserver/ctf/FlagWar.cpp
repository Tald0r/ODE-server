#include "FlagWar.h"

#include "FlagManager.h"
#include "GCNoticeEvent.h"
#include "ItemFactoryManager.h"
#include "MonsterSummonInfo.h"
#include "Zone.h"
#include "ZoneGroupManager.h"
#include "ZoneInfoManager.h"
#include "ZoneUtil.h"
#include "war/WarZoneWork.h"

namespace {

// Drops count flags on random free tiles of zone, on the zone's own thread,
// and writes each one's id into the round's ledger. Zone::addItem keeps a
// flag out of the safe zones. The zone's own mutex is taken as well, as the
// item positions take it, so the main thread's zone lockers stay excluded.
void dropFlags(Zone& zone, uint count, de::ctf::FlagLedger& ledger) {
    VSRect rect(0, 0, zone.getWidth() - 1, zone.getHeight() - 1);
    BPOINT pt;

    __ENTER_CRITICAL_SECTION(zone)

    for (uint i = 0; i < count; ++i) {
        pt.x = rand() % zone.getWidth();
        pt.y = rand() % zone.getHeight();

        while (!rect.ptInRect(pt.x, pt.y) || zone.getTile(pt.x, pt.y).hasItem() ||
               zone.getTile(pt.x, pt.y).isBlocked(Creature::MOVE_MODE_WALKING)) {
            pt.x = rand() % zone.getWidth();
            pt.y = rand() % zone.getHeight();
        }

        Item* pItem =
            de::gameContext().itemFactories().createItem(Item::ITEM_CLASS_EVENT_ITEM, 27, list<OptionType_t>());
        Assert(pItem != NULL);

        zone.registerObject(pItem);
        TPOINT ptInZone = zone.addItem(pItem, pt.x, pt.y, true, 36000);
        pItem->create("", STORAGE_ZONE, zone.getZoneID(), ptInZone.x, ptInZone.y);

        filelog("FlagWar.log", "%d : a flag was created at (%d,%d).", zone.getZoneID(), ptInZone.x, ptInZone.y);

        ledger.add(pItem->getItemID());
    }

    __LEAVE_CRITICAL_SECTION(zone)
}

// What an end's return does with a flag it took: the flag is gone for good.
void destroyFlag(Zone&, Item* pFlag) {
    pFlag->destroy();
    SAFE_DELETE(pFlag);
}

} // namespace

void FlagWar::execute() {
    switch (getState()) {
    case STATE_WAIT:
        filelog("FlagWar.log", "FlagWar Ready..");
        executeReady();
        setState(STATE_READY);
        break;
    case STATE_READY:
        filelog("FlagWar.log", "FlagWar Start..");
        executeStart();
        setState(STATE_START);
        break;
    case STATE_START:
        filelog("FlagWar.log", "FlagWar Finising..");
        executeFinish();
        setState(STATE_FINISH);
        break;
    case STATE_FINISH:
        filelog("FlagWar.log", "FlagWar End..");
        executeEnd();
        setState(STATE_WAIT);
        break;
    default:
        filelog("FlagWar.log", "Unexpected FlagWar state..");
        break;
    }
}

void FlagWar::executeReady() {
    GCNoticeEvent gcNE;
    gcNE.setCode(NOTICE_EVENT_FLAG_WAR_READY);

    VSDateTime current = VSDateTime::currentDateTime();
    gcNE.setParameter(((DWORD)((DWORD)(current.date().year() - 2000)) * 1000000) +
                      ((DWORD)((DWORD)current.date().month()) * 10000) + ((DWORD)((DWORD)current.date().day()) * 100) +
                      ((DWORD)((DWORD)current.time().hour())));

    m_Context.zoneGroups().broadcast(&gcNE);

    // Start in 5 minutes
    m_FlagManager.addSchedule(new Schedule(this, VSDateTime::currentDateTime().addSecs(300)));
}

void FlagWar::executeStart() {
    GCNoticeEvent gcNE;
    gcNE.setCode(NOTICE_EVENT_FLAG_WAR_START);

    VSDateTime current = VSDateTime::currentDateTime();
    gcNE.setParameter(((DWORD)((DWORD)(current.date().year() - 2000)) * 1000000) +
                      ((DWORD)((DWORD)current.date().month()) * 10000) + ((DWORD)((DWORD)current.date().day()) * 100) +
                      ((DWORD)((DWORD)current.time().hour())));

    m_Context.zoneGroups().broadcast(&gcNE);

    // Each drop runs on its zone's own thread and adds its flags to this
    // round's ledger, which the drop holds for as long as it needs it.
    m_pFlags = std::make_shared<de::ctf::FlagLedger>();

    std::vector<de::ctf::FlagDrop> drops = flagDrops();
    m_FlagManager.setAllowedZones(de::ctf::flagAllowMapOf(drops));

    for (const de::ctf::FlagDrop& drop : drops) {
        uint count = drop.count;
        std::shared_ptr<de::ctf::FlagLedger> pFlags = m_pFlags;

        de::war::postToZone(drop.zoneID, [count, pFlags](Zone& zone) { dropFlags(zone, count, *pFlags); });
    }

    // Let it run for 2 hours
    m_FlagManager.addSchedule(new Schedule(this, VSDateTime::currentDateTime().addSecs(getWarTime())));
    m_FlagManager.startFlagWar(getWarTime());
}

void FlagWar::executeFinish() {
    GCNoticeEvent gcNE;
    gcNE.setCode(NOTICE_EVENT_FLAG_WAR_FINISH);

    uint winnerCount = 0;
    Race_t winnerRace = m_FlagManager.getWinnerRace(winnerCount);
    gcNE.setParameter(((DWORD)((DWORD)winnerRace << 16)) | (DWORD)winnerCount);

    m_Context.zoneGroups().broadcast(&gcNE);

    // The items burst in 3 minutes.
    m_FlagManager.addSchedule(new Schedule(this, VSDateTime::currentDateTime().addSecs(180)));
    m_FlagManager.endFlagWar();
}

void FlagWar::executeEnd() {
    // Chase down every flag that was created and destroy it, on the thread
    // of whoever holds it: the zone it lies in, a pole's zone, or the player
    // carrying it. The pole sweeps follow the returns (FlagWarPlan.h). The
    // counts stay as the round left them until the next start zeroes them,
    // so the returns, which pay a pole of the winning race its gem stone as
    // they take its flag, and a newbie war's end name the round's winner.
    std::vector<ItemID_t> flagIDs = m_pFlags->take();

    for (const de::ctf::FlagWarEndStep& step : de::ctf::flagWarEndSteps(flagIDs, m_FlagManager.getPoleFields())) {
        switch (step.kind) {
        case de::ctf::FlagWarEndStep::Kind::ReturnFlag:
            if (!de::war::postItemReturn(Item::ITEM_CLASS_EVENT_ITEM, step.flagID, destroyFlag))
                filelog("FlagWar.log", "Failed to track the flag item %u...", (unsigned)step.flagID);
            break;
        case de::ctf::FlagWarEndStep::Kind::SweepPoles:
            m_FlagManager.postPoleSweep(step.zoneID);
            break;
        }
    }

    m_FlagManager.deleteFlagWarStats();

    // Until next time
    m_FlagManager.addSchedule(new Schedule(this, getNextFlagWarTime()));
}

VSDateTime FlagWar::getNextFlagWarTime() {
    static const int NextFlagWarDay[8] = {0, 0, 1, 0, 1, 0, 0, 0};

    VSDateTime dt = VSDateTime::currentDateTime();

    VSDateTime nextWarDateTime;
    nextWarDateTime = dt.addDays(NextFlagWarDay[dt.date().dayOfWeek()]);

    if (nextWarDateTime.date().dayOfWeek() < 6)
        nextWarDateTime.setTime(VSTime(18, 55, 0));
    else
        nextWarDateTime.setTime(VSTime(13, 55, 0));

    if (nextWarDateTime < VSDateTime::currentDateTime()) {
        bool anotherDay = true;
        if (nextWarDateTime.date().dayOfWeek() == 6) {
            nextWarDateTime.setTime(VSTime(18, 55, 0));
            if (nextWarDateTime >= VSDateTime::currentDateTime())
                anotherDay = false;
        }

        if (anotherDay) {
            nextWarDateTime = nextWarDateTime.addDays(1);
            nextWarDateTime = nextWarDateTime.addDays(NextFlagWarDay[nextWarDateTime.date().dayOfWeek()]);

            if (nextWarDateTime.date().dayOfWeek() < 6)
                nextWarDateTime.setTime(VSTime(18, 55, 0));
            else
                nextWarDateTime.setTime(VSTime(13, 55, 0));
        }
    }

    filelog("FlagWar.log", "Capture-the-flag event starts at %s", nextWarDateTime.toString().c_str());

    return nextWarDateTime;
}

std::vector<de::ctf::FlagDrop> FlagWar::flagDrops() const {
    return {
        {21, 6}, {22, 6}, {23, 6}, {24, 7}, {11, 7},  {12, 6},  {13, 6},  {14, 6},
        {31, 6}, {32, 6}, {33, 7}, {34, 6}, {61, 11}, {62, 11}, {63, 12}, {64, 11},
    };
}

int FlagWar::getWarTime() const {
    return 7200;
}
