//////////////////////////////////////////////////////////////////////////////
// Filename		: Guild.cpp
// Written by	: bezz
// Description	:
//////////////////////////////////////////////////////////////////////////////

#include "Guild.h"

#include <algorithm>

#include "StringStream.h"
#include "repository/GuildRepository.h"

#ifdef __SHARED_SERVER__
#include "GuildInfo2.h"
#include "GuildMemberInfo2.h"
#endif

#include <stdio.h>

#include "GCGuildMemberList.h"
#include "GuildInfo.h"
#include "GuildMemberInfo.h"
#include "KernelContext.h"
#include "Properties.h"

//////////////////////////////////////////////////////////////////////////////
// class GuildMember member methods
//////////////////////////////////////////////////////////////////////////////

GuildMember::GuildMember()

{
    m_Rank = GUILDMEMBER_RANK_NORMAL; // was left indeterminate
    m_bLogOn = false;
    m_ServerID = 255;
    m_bRetired = false;
}

void GuildMember::create()

{
    __BEGIN_TRY

    GuildRepository& repository = defaultGuildRepository();

    if (repository.memberExists(m_Name)) {
        // Already in the DB (a former member of another guild): rewrite the row.
        if (m_Rank == GUILDMEMBER_RANK_WAIT) {
            repository.rejoinWaitingMember(m_GuildID, m_Rank, getRequestDateTime(), m_Name);
        } else {
            repository.rejoinMember(m_GuildID, m_Rank, m_Name);
        }
    } else {
        if (m_Rank == GUILDMEMBER_RANK_WAIT) {
            repository.insertWaitingMember(m_GuildID, m_Name, m_Rank, getRequestDateTime());
        } else {
            repository.insertMember(m_GuildID, m_Name, m_Rank);
        }
    }

    __END_CATCH
}


bool GuildMember::load()

{
    __BEGIN_TRY

    GuildMemberRow row;
    if (!defaultGuildRepository().loadMember(m_Name, row)) {
        return false;
    }

    m_GuildID = row.guildID;
    m_Name = row.name;
    m_Rank = row.rank;
    m_bLogOn = row.logOn;

    m_ServerID = de::kernelContext().config().getPropertyInt("ServerID");

    return true;

    __END_CATCH
}


void GuildMember::save()

{
    __BEGIN_TRY

    defaultGuildRepository().saveMember(m_GuildID, m_Rank, m_Name);

    __END_CATCH
}


void GuildMember::destroy()

{
    __BEGIN_TRY

    defaultGuildRepository().deleteMember(m_Name);

    __END_CATCH
}

void GuildMember::expire()

{
    __BEGIN_TRY

    // The current local date.
    time_t daytime = time(0);
    tm Timec;
    localtime_r(&daytime, &Timec);
    char ExpireDate[8];
    snprintf(ExpireDate, sizeof(ExpireDate), "%03d%02d%02d", Timec.tm_year, Timec.tm_mon, Timec.tm_mday);

    defaultGuildRepository().setMemberRankAndExpireDate(GUILDMEMBER_RANK_DENY, ExpireDate, m_Name);

    __END_CATCH
}

void GuildMember::leave()

{
    __BEGIN_TRY

    // The current local date.
    time_t daytime = time(0);
    tm Timec;
    localtime_r(&daytime, &Timec);
    char ExpireDate[8];
    snprintf(ExpireDate, sizeof(ExpireDate), "%03d%02d%02d", Timec.tm_year, Timec.tm_mon, Timec.tm_mday);

    defaultGuildRepository().setMemberRankAndExpireDate(GUILDMEMBER_RANK_LEAVE, ExpireDate, m_Name);

    __END_CATCH
}


void GuildMember::saveIntro(const string& intro)

{
    __BEGIN_TRY

    string modifyIntro = Guild::correctString(intro);

    defaultGuildRepository().saveMemberIntro(modifyIntro, m_Name);

    __END_CATCH
}


string GuildMember::getIntro() const

{
    __BEGIN_TRY

    string intro = "";

    defaultGuildRepository().loadMemberIntro(m_Name, intro);

    return intro;

    __END_CATCH
}


string GuildMember::toString() const

{
    __BEGIN_TRY

    StringStream msg;
    msg << "GuildID = " << (int)m_GuildID << " Name = " << m_Name << " Rank = " << (int)m_Rank << "\n";

    return msg.toString();

    __END_CATCH
}


GuildMember& GuildMember::operator=(GuildMember& Member) {
    m_GuildID = Member.m_GuildID;
    m_Name = Member.m_Name;
    m_Rank.store(Member.m_Rank.load(std::memory_order_relaxed), std::memory_order_relaxed);

    return *this;
}

string GuildMember::getRequestDateTime() const

{
    __BEGIN_TRY

    char buf[20];

    snprintf(buf, sizeof(buf), "%4d-%02d-%02d %02d:%02d:%02d", m_RequestDateTime.date().year(),
             m_RequestDateTime.date().month(), m_RequestDateTime.date().day(), m_RequestDateTime.time().hour(),
             m_RequestDateTime.time().minute(), m_RequestDateTime.time().second());

    cout << buf << "\n";

    return string(buf);

    __END_CATCH
}


void GuildMember::setRank(GuildMemberRank_t rank)

{
    __BEGIN_TRY

    m_Rank = rank;

    __END_CATCH
}


void GuildMember::setRequestDateTime(const string& rtime)

{
    __BEGIN_TRY

    // 0123456789012345678
    // YYYY-MM-DD HH:MM:SS
    if (rtime.size() == 19) {
        int year = atoi(rtime.substr(0, 4).c_str());
        int month = atoi(rtime.substr(5, 2).c_str());
        int day = atoi(rtime.substr(8, 2).c_str());
        int hour = atoi(rtime.substr(11, 2).c_str());
        int min = atoi(rtime.substr(14, 2).c_str());
        int second = atoi(rtime.substr(17, 2).c_str());

        m_RequestDateTime.setDate(VSDate(year, month, day));
        m_RequestDateTime.setTime(VSTime(hour, min, second));
    } else {
        m_RequestDateTime.setDate(VSDate(2000, 1, 1));
        m_RequestDateTime.setTime(VSTime(0, 0, 0));
    }

    __END_CATCH
}

bool GuildMember::isRequestDateTimeOut(const VSDateTime& currentDateTime) const

{
    __BEGIN_TRY

    VSDateTime limitDateTime = m_RequestDateTime.addDays(7);

    return currentDateTime > limitDateTime;

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////////////
// global variable initialization
//////////////////////////////////////////////////////////////////////////////

// The integral guild fields are read on zone threads while the
// SharedServerManager thread writes them. A lock-based atomic would put a
// hidden mutex on every one of those reads, so require the lock-free ones.
static_assert(std::atomic<GuildID_t>::is_always_lock_free);
static_assert(std::atomic<GuildType_t>::is_always_lock_free);
static_assert(std::atomic<GuildRace_t>::is_always_lock_free);
static_assert(std::atomic<GuildState_t>::is_always_lock_free);
static_assert(std::atomic<ServerGroupID_t>::is_always_lock_free);
static_assert(std::atomic<ZoneID_t>::is_always_lock_free);
static_assert(std::atomic<GuildMemberRank_t>::is_always_lock_free);

GuildID_t Guild::m_MaxGuildID = 0;
ZoneID_t Guild::m_MaxSlayerZoneID = 10000;
ZoneID_t Guild::m_MaxVampireZoneID = 20000;
ZoneID_t Guild::m_MaxOustersZoneID = 30000;

//////////////////////////////////////////////////////////////////////////////
// class Guild member methods
//////////////////////////////////////////////////////////////////////////////

Guild::Guild()

{
    m_ID = 0;
    m_Name = "";
    m_Type = 0;
    m_Race = 0;
    m_State = 0;
    m_ServerGroupID = 0;
    m_ZoneID = 0;
    m_bRetired = false;
    m_Master = "";
    m_Date = "";
    m_Intro = "";

    m_ActiveMemberCount = 0;
    m_WaitMemberCount = 0;

    __BEGIN_TRY

    m_Mutex.setName("Guild");

    __END_CATCH
}


Guild::~Guild()

{
    __BEGIN_TRY

    __ENTER_CRITICAL_SECTION(m_Mutex)

    HashMapGuildMemberItor itr = m_Members.begin();
    for (; itr != m_Members.end(); itr++) {
        SAFE_DELETE(itr->second);
    }

    m_Members.clear();

    for (size_t i = 0; i < m_RetiredMembers.size(); i++)
        SAFE_DELETE(m_RetiredMembers[i]);
    m_RetiredMembers.clear();

#ifdef __GAME_SERVER__
    m_CurrentMembers.clear();
#endif

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    __END_CATCH_NO_RETHROW
}


void Guild::create()

{
    __BEGIN_TRY

    __ENTER_CRITICAL_SECTION(m_Mutex)

    string correctIntro = correctString(m_Intro);

    GuildRecord record;
    record.id = m_ID;
    record.name = m_Name;
    record.type = m_Type;
    record.race = m_Race;
    record.state = m_State;
    record.serverGroupID = m_ServerGroupID;
    record.zoneID = m_ZoneID;
    record.master = m_Master;
    record.date = m_Date;
    record.intro = correctIntro;
    defaultGuildRepository().insertGuild(record);

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    __END_CATCH
}


bool Guild::load()

{
    __BEGIN_TRY

    __ENTER_CRITICAL_SECTION(m_Mutex)

    GuildRow row;
    if (!defaultGuildRepository().loadGuild(m_ID, row)) {
        return false;
    }

    m_Name = row.name;
    m_Type = row.type;
    m_Race = row.race;
    m_State = row.state;
    m_ServerGroupID = row.serverGroupID;
    m_ZoneID = row.zoneID;
    m_Master = row.master;
    m_Date = row.date;

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    return true;

    __END_CATCH
}


void Guild::save()

{
    __BEGIN_TRY

    __ENTER_CRITICAL_SECTION(m_Mutex)

    GuildRecord record;
    record.id = m_ID;
    record.name = m_Name;
    record.type = m_Type;
    record.race = m_Race;
    record.state = m_State;
    record.serverGroupID = m_ServerGroupID;
    record.zoneID = m_ZoneID;
    record.master = m_Master;
    record.date = m_Date;
    defaultGuildRepository().saveGuild(record);

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    __END_CATCH
}


void Guild::destroy()

{
    __BEGIN_TRY

    __ENTER_CRITICAL_SECTION(m_Mutex)

    defaultGuildRepository().deleteGuild(m_ID);

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    __END_CATCH
}

// Guild::saveIntro / tinysave / saveCount lived here under
// __SHARED_SERVER__, which no build of this file defines (the sharedserver
// compiles its own Guild.cpp). Gone with their SQL.


//////////////////////////////////////////////////////////////////////////////
// string identity fields: copied in and out under m_Mutex
//
// Each of these takes m_Mutex and nothing else, so the mutex stays a leaf and
// a caller may hold any other lock across the call. m_Mutex is not recursive,
// so none of them may be called from code that already holds this guild's
// mutex; such code reads and writes the members directly.
//////////////////////////////////////////////////////////////////////////////

string Guild::getName() const {
    string name;

    __ENTER_CRITICAL_SECTION(m_Mutex)

    name = m_Name;

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    return name;
}

void Guild::setName(const string& name) {
    __ENTER_CRITICAL_SECTION(m_Mutex)

    m_Name = name;

    __LEAVE_CRITICAL_SECTION(m_Mutex)
}

string Guild::getMaster() const {
    string master;

    __ENTER_CRITICAL_SECTION(m_Mutex)

    master = m_Master;

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    return master;
}

void Guild::setMaster(const string& master) {
    __ENTER_CRITICAL_SECTION(m_Mutex)

    m_Master = master;

    __LEAVE_CRITICAL_SECTION(m_Mutex)
}

string Guild::getDate() const {
    string date;

    __ENTER_CRITICAL_SECTION(m_Mutex)

    date = m_Date;

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    return date;
}

void Guild::setDate(const string& Date) {
    __ENTER_CRITICAL_SECTION(m_Mutex)

    m_Date = Date;

    __LEAVE_CRITICAL_SECTION(m_Mutex)
}

string Guild::getIntro() const {
    string intro;

    __ENTER_CRITICAL_SECTION(m_Mutex)

    intro = m_Intro;

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    return intro;
}

void Guild::setIntro(const string& intro) {
    __ENTER_CRITICAL_SECTION(m_Mutex)

    m_Intro = intro;

    __LEAVE_CRITICAL_SECTION(m_Mutex)
}


GuildMember* Guild::getMember(const string& name) const

{
    __BEGIN_TRY

    HashMapGuildMemberConstItor itr;
    GuildMember* pGuildMember = NULL;

    // A retired guild has no members: it is out of GuildManager's table and
    // only still readable because a thread may hold the pointer.
    if (isRetired())
        return NULL;

    __ENTER_CRITICAL_SECTION(m_Mutex)

    itr = m_Members.find(name);

    if (itr == m_Members.end()) {
        // cout << "Guild::getMember() : NoSuchMember" << endl;

        return NULL;
    }

    pGuildMember = itr->second;

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    return pGuildMember;

    __END_CATCH
}


GuildMember* Guild::getMember_NOLOCKED(const string& name) const

{
    __BEGIN_TRY

    HashMapGuildMemberConstItor itr;
    GuildMember* pGuildMember = NULL;

    itr = m_Members.find(name);

    if (itr == m_Members.end()) {
        // cerr << "Guild::getMember() : NoSuchMember" << endl;

        return NULL;
    }

    pGuildMember = itr->second;

    return pGuildMember;

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
// guild teardown: take every member out of the map under the mutex
//////////////////////////////////////////////////////////////////////////////
std::vector<std::pair<std::string, GuildMemberRank_t>> Guild::retireAllMembers() {
    std::vector<std::pair<std::string, GuildMemberRank_t>> members;

    __ENTER_CRITICAL_SECTION(m_Mutex)

    // Two passes: the first copies names and can fail on allocation, while
    // nothing has moved yet; the second moves the pointers and cannot throw
    // (both vectors are reserved), so a member is never owned by the map
    // and the retired list at once.
    members.reserve(m_Members.size());
    m_RetiredMembers.reserve(m_RetiredMembers.size() + m_Members.size());
    for (HashMapGuildMemberItor itr = m_Members.begin(); itr != m_Members.end(); ++itr)
        members.push_back(std::make_pair(itr->first, itr->second->getStoredRank()));
    for (HashMapGuildMemberItor itr = m_Members.begin(); itr != m_Members.end(); ++itr) {
        itr->second->retire();
        m_RetiredMembers.push_back(itr->second);
    }
    m_Members.clear();
    m_ActiveMemberCount = 0;
    m_WaitMemberCount = 0;

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    return members;
}

//////////////////////////////////////////////////////////////////////////////
// the guild has left GuildManager's table and is kept only so the threads
// holding it do not read freed memory; mark it, and its members with it
//////////////////////////////////////////////////////////////////////////////
void Guild::retire() {
    m_bRetired.store(true, std::memory_order_relaxed);

    __ENTER_CRITICAL_SECTION(m_Mutex)

    for (HashMapGuildMemberItor itr = m_Members.begin(); itr != m_Members.end(); ++itr)
        itr->second->retire();

    __LEAVE_CRITICAL_SECTION(m_Mutex)
}

//////////////////////////////////////////////////////////////////////////////
// member names, copied under the guild mutex -- for readers on other threads
//////////////////////////////////////////////////////////////////////////////
std::vector<std::string> Guild::getMemberNames() const {
    std::vector<std::string> names;

    __ENTER_CRITICAL_SECTION(m_Mutex)

    names.reserve(m_Members.size());
    for (HashMapGuildMemberConstItor itr = m_Members.begin(); itr != m_Members.end(); ++itr)
        names.push_back(itr->first);

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    return names;
}


void Guild::addMember(GuildMember* pMember)

{
    __BEGIN_TRY

    __ENTER_CRITICAL_SECTION(m_Mutex)

    Assert(pMember);

    if (m_Members.contains(pMember->getName())) {
        throw DuplicatedException();
    }

    m_Members[pMember->getName()] = pMember;

    GuildMemberRank_t rank = pMember->getStoredRank();

    if (rank == GuildMember::GUILDMEMBER_RANK_NORMAL || rank == GuildMember::GUILDMEMBER_RANK_MASTER ||
        rank == GuildMember::GUILDMEMBER_RANK_SUBMASTER) {
        // Adding a normal member or a (sub)master increases m_ActiveMemberCount.
        m_ActiveMemberCount++;
    } else if (rank == GuildMember::GUILDMEMBER_RANK_WAIT) {
        // Adding an applicant increases m_WaitMemberCount.
        m_WaitMemberCount++;
    }

#ifdef __SHARED_SERVER__
    saveCount();
#endif

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    __END_CATCH
}


void Guild::deleteMember(const string& name)

{
    __BEGIN_TRY

    __ENTER_CRITICAL_SECTION(m_Mutex)

    HashMapGuildMemberItor itr;

    itr = m_Members.find(name);

    if (itr == m_Members.end()) {
        cerr << "Guild::deleteMember() : NoSuchElementException" << endl;

        return;
    }

    GuildMemberRank_t rank = itr->second->getStoredRank();

    if (rank == GuildMember::GUILDMEMBER_RANK_NORMAL || rank == GuildMember::GUILDMEMBER_RANK_MASTER ||
        rank == GuildMember::GUILDMEMBER_RANK_SUBMASTER) {
        // Decrease the active member counter.
        m_ActiveMemberCount--;
    } else if (rank == GuildMember::GUILDMEMBER_RANK_WAIT) {
        m_WaitMemberCount--;
    }

    // Retire, don't free: a zone thread may hold this pointer (see
    // m_RetiredMembers). The flag goes up here, under the mutex the removal
    // runs under, so a holder that looks again sees the member has gone.
    itr->second->retire();
    m_RetiredMembers.push_back(itr->second);

    m_Members.erase(itr);

#ifdef __SHARED_SERVER__
    saveCount();
#endif

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    __END_CATCH
}


void Guild::modifyMember(GuildMember& Member)

{
    __BEGIN_TRY

    __ENTER_CRITICAL_SECTION(m_Mutex)

    HashMapGuildMemberConstItor itr;

    itr = m_Members.find(Member.getName());

    if (itr == m_Members.end()) {
        cerr << "Guild::modifyMember() : NoSuchElementException" << endl;

        return;
    }

    *(itr->second) = Member;

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    __END_CATCH
}


void Guild::modifyMemberRank(const string& name, GuildMemberRank_t rank)

{
    __BEGIN_TRY

    // The counters and the member's rank change together; hold the guild
    // mutex across the lookup and both updates so a reader (a zone thread
    // building a member list) never sees the rank moved but the counts not.
    __ENTER_CRITICAL_SECTION(m_Mutex)

    GuildMember* pMember = getMember_NOLOCKED(name);
    if (pMember == NULL)
        return;

    GuildMemberRank_t oldRank = pMember->getStoredRank();

    if (oldRank == rank)
        return;

    if (oldRank == GuildMember::GUILDMEMBER_RANK_WAIT) {
        m_WaitMemberCount--;
    } else if (oldRank == GuildMember::GUILDMEMBER_RANK_NORMAL || oldRank == GuildMember::GUILDMEMBER_RANK_MASTER ||
               oldRank == GuildMember::GUILDMEMBER_RANK_SUBMASTER) {
        m_ActiveMemberCount--;
    }

    if (rank == GuildMember::GUILDMEMBER_RANK_WAIT) {
        m_WaitMemberCount++;
    } else if (rank == GuildMember::GUILDMEMBER_RANK_NORMAL || rank == GuildMember::GUILDMEMBER_RANK_MASTER ||
               rank == GuildMember::GUILDMEMBER_RANK_SUBMASTER) {
        m_ActiveMemberCount++;
    }

    pMember->setRank(rank);

#ifdef __SHARED_SERVER__
    pMember->save();
    saveCount();
#endif

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    __END_CATCH
}


#ifdef __GAME_SERVER__
void Guild::addCurrentMember(const string& name) {
    __BEGIN_TRY

    __ENTER_CRITICAL_SECTION(m_Mutex) // A separate mutex would do here.

    if (std::ranges::find(m_CurrentMembers, name) != m_CurrentMembers.end()) {
        return;
    }

    m_CurrentMembers.push_back(name);

    // Set the logon flag on the GuildMember object.
    GuildMember* pGuildMember = getMember_NOLOCKED(name);
    if (pGuildMember == NULL) {
        return;
    }

    pGuildMember->setLogOn(true);

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    __END_CATCH
}

void Guild::deleteCurrentMember(const string& name) {
    __BEGIN_TRY

    __ENTER_CRITICAL_SECTION(m_Mutex)

    list<string>::iterator itr = std::ranges::find(m_CurrentMembers, name);

    if (m_CurrentMembers.end() == itr) {
        return;
    }

    m_CurrentMembers.erase(itr);

    // Set the logoff flag on the GuildMember object.
    GuildMember* pGuildMember = getMember_NOLOCKED(name);
    if (pGuildMember == NULL) {
        return;
    }

    pGuildMember->setLogOn(false);

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    __END_CATCH
}

list<string> Guild::getCurrentMembers()

{
    __BEGIN_TRY

    list<string> cmList;

    __ENTER_CRITICAL_SECTION(m_Mutex)

    cmList = m_CurrentMembers;

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    return cmList;

    __END_CATCH
}
#endif

#ifdef __SHARED_SERVER__
void Guild::makeInfo(GuildInfo2* pGuildInfo)

{
    __BEGIN_TRY

    __ENTER_CRITICAL_SECTION(m_Mutex)

    pGuildInfo->setID(m_ID);
    pGuildInfo->setName(m_Name);
    pGuildInfo->setType(m_Type);
    pGuildInfo->setRace(m_Race);
    pGuildInfo->setState(m_State);
    pGuildInfo->setServerGroupID(m_ServerGroupID);
    pGuildInfo->setZoneID(m_ZoneID);
    pGuildInfo->setMaster(m_Master);
    pGuildInfo->setDate(m_Date);
    pGuildInfo->setIntro(m_Intro);

    HashMapGuildMemberConstItor itr = m_Members.begin();
    for (; itr != m_Members.end(); itr++) {
        GuildMemberInfo2* pGuildMemberInfo = new GuildMemberInfo2();
        pGuildMemberInfo->setGuildID(itr->second->getGuildID());
        pGuildMemberInfo->setName(itr->second->getName());
        pGuildMemberInfo->setRank(itr->second->getRank());
        pGuildMemberInfo->setLogOn(itr->second->getLogOn());

        pGuildInfo->addGuildMemberInfo(pGuildMemberInfo);
    }

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    __END_CATCH
}
#endif

void Guild::makeInfo(GuildInfo* pGuildInfo)

{
    __BEGIN_TRY

    __ENTER_CRITICAL_SECTION(m_Mutex)

    pGuildInfo->setGuildID(m_ID);
    pGuildInfo->setGuildName(m_Name);
    pGuildInfo->setGuildMaster(m_Master);
    pGuildInfo->setGuildMemberCount(m_ActiveMemberCount);

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    __END_CATCH
}

void Guild::makeMemberInfo(GCGuildMemberList& gcGuildMemberList)

{
    __BEGIN_TRY

    __ENTER_CRITICAL_SECTION(m_Mutex)

    HashMapGuildMember& Members = getMembers_NOLOCKED(); // m_Mutex is held above
    HashMapGuildMemberConstItor itr = Members.begin();

    for (; itr != Members.end(); itr++) {
        GuildMember* pGuildMember = itr->second;

        GuildMemberInfo* pGuildMemberInfo = new GuildMemberInfo();
        pGuildMemberInfo->setName(pGuildMember->getName());
        pGuildMemberInfo->setRank(pGuildMember->getRank());
        pGuildMemberInfo->setLogOn(pGuildMember->getLogOn());
        pGuildMemberInfo->setServerID(pGuildMember->getServerID());

        gcGuildMemberList.addGuildMemberInfo(pGuildMemberInfo);
    }

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    __END_CATCH
}


void Guild::expireTimeOutWaitMember(VSDateTime currentDateTime, list<string>& mList)

{
    __BEGIN_TRY

#ifdef __SHARED_SERVER__

    HashMapGuildMemberItor itr = m_Members.begin();

    while (itr != m_Members.end()) {
        GuildMember* pGuildMember = itr->second;

        if (pGuildMember->getRank() == GuildMember::GUILDMEMBER_RANK_WAIT &&
            pGuildMember->isRequestDateTimeOut(currentDateTime)) {
            mList.push_back(pGuildMember->getName());

            // Decrease the wait member count.
            m_WaitMemberCount--;

            pGuildMember->expire();

            SAFE_DELETE(pGuildMember);

            m_Members.erase(itr++);
        } else {
            itr++;
        }
    }

#endif

    __END_CATCH
}


string Guild::toString() const

{
    __BEGIN_TRY

    string name;
    string master;
    string date;

    __ENTER_CRITICAL_SECTION(m_Mutex)

    name = m_Name;
    master = m_Master;
    date = m_Date;

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    // The atomics are loaded after the strings were copied, so a concurrent
    // writer can leave the line describing two moments of the guild. That is
    // enough for a diagnostic dump.
    StringStream msg;
    msg << " GuildID = " << getID() << " GuildName = " << name << " GuildType = " << (int)getType()
        << " GuildState = " << (int)getState() << " ServerGroupID = " << (int)getServerGroupID()
        << " GuildZoneID = " << (int)getZoneID() << " Master = " << master << " Date = " << date << "\n";

    return msg.toString();

    __END_CATCH
}

string Guild::correctString(const string& str)

{
    __BEGIN_TRY

    string correct = str;

    unsigned int i = 0;
    unsigned int size = str.size();

    while (i < size) {
        if (correct[i] == '\\') {
            correct.replace(i, 1, "\\\\");
            i = i + 2;
            size++;
        } else if (correct[i] == '\'') {
            correct.replace(i, 1, "\\'");
            i = i + 2;
            size++;
        } else {
            i++;
        }
    }

    return correct;

    __END_CATCH
}
