//////////////////////////////////////////////////////////////////////////////
// Filename		: Guild.cpp
// Written by	: bezz
// Description	:
//////////////////////////////////////////////////////////////////////////////

#include "Guild.h"

#include <algorithm>

#include "StringStream.h"
#include "repository/SharedGuildRepository.h"

#ifdef __SHARED_SERVER__
#include "GuildInfo2.h"
#include "GuildMemberInfo2.h"
#endif

#include <stdio.h>

#include "GCGuildMemberList.h"
#include "GuildInfo.h"
#include "GuildMemberInfo.h"
#include "Properties.h"

//////////////////////////////////////////////////////////////////////////////
// class GuildMember member methods
//////////////////////////////////////////////////////////////////////////////

GuildMember::GuildMember() noexcept {
    m_bLogOn = false;
}

void GuildMember::create() noexcept(false) {
    __BEGIN_TRY

    SharedGuildRepository& repo = defaultSharedGuildRepository();

    // A name that already has a row (a member of another guild, or one
    // who left) is re-pointed at this guild; otherwise a row is inserted.
    if (repo.memberExists(m_Name)) {
        if (m_Rank == GUILDMEMBER_RANK_WAIT) {
            repo.rejoinWaitingMember(m_GuildID, m_Rank, getRequestDateTime(), m_Name);
        } else {
            repo.rejoinMember(m_GuildID, m_Rank, m_Name);
        }
    } else {
        if (m_Rank == GUILDMEMBER_RANK_WAIT) {
            repo.insertWaitingMember(m_GuildID, m_Name, m_Rank, getRequestDateTime());
        } else {
            repo.insertMember(m_GuildID, m_Name, m_Rank);
        }
    }

    __END_CATCH
}


bool GuildMember::load() noexcept(false) {
    __BEGIN_TRY

    SharedGuildMemberRow row;

    if (!defaultSharedGuildRepository().loadMember(m_Name, row))
        return false;

    m_GuildID = row.guildID;
    m_Name = row.name;
    m_Rank = row.rank;
    m_bLogOn = row.logOn;

    return true;

    __END_CATCH
}


void GuildMember::save() noexcept(false) {
    __BEGIN_TRY

    defaultSharedGuildRepository().saveMember(m_GuildID, m_Rank, m_Name);

    __END_CATCH
}


void GuildMember::destroy() noexcept(false) {
    __BEGIN_TRY

    defaultSharedGuildRepository().deleteMember(m_Name);

    __END_CATCH
}

void GuildMember::expire() noexcept(false) {
    __BEGIN_TRY

    // Today's date as the ExpireDate text.
    time_t daytime = time(0);
    tm Timec;
    localtime_r(&daytime, &Timec);
    char ExpireDate[8];
    snprintf(ExpireDate, sizeof(ExpireDate), "%03d%02d%02d", Timec.tm_year, Timec.tm_mon, Timec.tm_mday);

    defaultSharedGuildRepository().setMemberRankAndExpireDate(GUILDMEMBER_RANK_DENY, ExpireDate, m_Name);

    __END_CATCH
}

void GuildMember::leave() noexcept(false) {
    __BEGIN_TRY

    // Today's date as the ExpireDate text.
    time_t daytime = time(0);
    tm Timec;
    localtime_r(&daytime, &Timec);
    char ExpireDate[8];
    snprintf(ExpireDate, sizeof(ExpireDate), "%03d%02d%02d", Timec.tm_year, Timec.tm_mon, Timec.tm_mday);

    defaultSharedGuildRepository().setMemberRankAndExpireDate(GUILDMEMBER_RANK_LEAVE, ExpireDate, m_Name);

    __END_CATCH
}


void GuildMember::saveIntro(const string& intro) noexcept(false) {
    __BEGIN_TRY

    string modifyIntro = Guild::correctString(intro);

    defaultSharedGuildRepository().saveMemberIntro(modifyIntro, m_Name);

    __END_CATCH
}


string GuildMember::getIntro() const noexcept(false) {
    __BEGIN_TRY

    string intro = "";

    defaultSharedGuildRepository().loadMemberIntro(m_Name, intro);

    return intro;

    __END_CATCH
}


string GuildMember::toString() const noexcept {
    StringStream msg;
    msg << "GuildID = " << (int)m_GuildID << " Name = " << m_Name << " Rank = " << (int)m_Rank << "\n";
    return msg.toString();
}


GuildMember& GuildMember::operator=(GuildMember& Member) {
    m_GuildID = Member.m_GuildID;
    m_Name = Member.m_Name;
    m_Rank = Member.m_Rank;

    return *this;
}

string GuildMember::getRequestDateTime() const noexcept(false) {
    __BEGIN_TRY

    char buf[20];

    snprintf(buf, sizeof(buf), "%4d-%02d-%02d %02d:%02d:%02d", m_RequestDateTime.date().year(),
             m_RequestDateTime.date().month(), m_RequestDateTime.date().day(), m_RequestDateTime.time().hour(),
             m_RequestDateTime.time().minute(), m_RequestDateTime.time().second());

    cout << buf << endl;

    return string(buf);

    __END_CATCH
}


void GuildMember::setRank(GuildMemberRank_t rank) noexcept(false) {
    __BEGIN_TRY

    m_Rank = rank;

    __END_CATCH
}


void GuildMember::setRequestDateTime(const string& rtime) noexcept(false) {
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

bool GuildMember::isRequestDateTimeOut(const VSDateTime& currentDateTime) const noexcept {
    VSDateTime limitDateTime = m_RequestDateTime.addDays(7);
    return currentDateTime > limitDateTime;
}


//////////////////////////////////////////////////////////////////////////////
// global variable initialization
//////////////////////////////////////////////////////////////////////////////

GuildID_t Guild::m_MaxGuildID = 0;
ZoneID_t Guild::m_MaxSlayerZoneID = 10000;
ZoneID_t Guild::m_MaxVampireZoneID = 20000;
ZoneID_t Guild::m_MaxOustersZoneID = 30000;

//////////////////////////////////////////////////////////////////////////////
// class Guild member methods
//////////////////////////////////////////////////////////////////////////////

Guild::Guild() noexcept(false) {
    m_ID = 0;
    m_Name = "";
    m_Type = 0;
    m_State = 0;
    m_ServerGroupID = 0;
    m_ZoneID = 0;
    m_Master = "";
    m_Date = "";
    m_Intro = "";

    m_ActiveMemberCount = 0;
    m_WaitMemberCount = 0;

    __BEGIN_TRY

    m_Mutex.setName("Guild");

    __END_CATCH
}


Guild::~Guild() noexcept {
    try {
        __ENTER_CRITICAL_SECTION(m_Mutex)

        HashMapGuildMemberItor itr = m_Members.begin();
        for (; itr != m_Members.end(); itr++) {
            SAFE_DELETE(itr->second);
        }

        m_Members.clear();

#ifdef __GAME_SERVER__
        m_CurrentMembers.clear();
#endif

        __LEAVE_CRITICAL_SECTION(m_Mutex)
    } catch (...) {
        // destructor must not throw
    }
}


void Guild::create() noexcept(false) {
    __BEGIN_TRY

    __ENTER_CRITICAL_SECTION(m_Mutex)

    SharedGuildRecord record;
    record.id = m_ID;
    record.name = m_Name;
    record.type = m_Type;
    record.race = m_Race;
    record.state = m_State;
    record.serverGroupID = m_ServerGroupID;
    record.zoneID = m_ZoneID;
    record.master = m_Master;
    record.date = m_Date;
    record.intro = correctString(m_Intro);

    defaultSharedGuildRepository().insertGuild(record);

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    __END_CATCH
}


bool Guild::load() noexcept(false) {
    __BEGIN_TRY

    SharedGuildRow row;

    __ENTER_CRITICAL_SECTION(m_Mutex)

    if (!defaultSharedGuildRepository().loadGuild(m_ID, row))
        return false;

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


void Guild::save() noexcept(false) {
    __BEGIN_TRY

    __ENTER_CRITICAL_SECTION(m_Mutex)

    SharedGuildRecord record;
    record.id = m_ID;
    record.name = m_Name;
    record.type = m_Type;
    record.race = m_Race;
    record.state = m_State;
    record.serverGroupID = m_ServerGroupID;
    record.zoneID = m_ZoneID;
    record.master = m_Master;
    record.date = m_Date;

    defaultSharedGuildRepository().saveGuild(record);

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    __END_CATCH
}


void Guild::destroy() noexcept(false) {
    __BEGIN_TRY

    __ENTER_CRITICAL_SECTION(m_Mutex)

    defaultSharedGuildRepository().deleteGuild(m_ID);

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    __END_CATCH
}


#ifdef __SHARED_SERVER__
void Guild::saveIntro(const string& intro) noexcept(false) {
    __BEGIN_TRY

    m_Intro = intro;

    string modifyIntro = Guild::correctString(intro);

    defaultSharedGuildRepository().saveGuildIntro(modifyIntro, m_ID);

    __END_CATCH
}

void Guild::tinysave(const char* field) const noexcept(false) {
    __BEGIN_TRY

    defaultSharedGuildRepository().updateGuildFields(field, m_ID);

    __END_CATCH
}

void Guild::saveCount() const noexcept(false){__BEGIN_TRY


                                                  __END_CATCH}
#endif


GuildMember* Guild::getMember(const string& name) const noexcept(false) {
    __BEGIN_TRY

    HashMapGuildMemberConstItor itr;
    GuildMember* pGuildMember = NULL;

    __ENTER_CRITICAL_SECTION(m_Mutex)

    itr = m_Members.find(name);

    if (itr == m_Members.end()) {
        return NULL;
    }

    pGuildMember = itr->second;

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    return pGuildMember;

    __END_CATCH
}


GuildMember* Guild::getMember_NOLOCKED(const string& name) const noexcept(false) {
    __BEGIN_TRY

    HashMapGuildMemberConstItor itr;
    GuildMember* pGuildMember = NULL;

    itr = m_Members.find(name);

    if (itr == m_Members.end()) {
        return NULL;
    }

    pGuildMember = itr->second;

    return pGuildMember;

    __END_CATCH
}

void Guild::addMember(GuildMember* pMember) noexcept(false) {
    __BEGIN_TRY

    __ENTER_CRITICAL_SECTION(m_Mutex)

    Assert(pMember);

    HashMapGuildMemberConstItor itr;

    itr = m_Members.find(pMember->getName());

    if (itr != m_Members.end()) {
        throw DuplicatedException();
    }

    m_Members[pMember->getName()] = pMember;

    GuildMemberRank_t rank = pMember->getRank();

    if (rank == GuildMember::GUILDMEMBER_RANK_NORMAL || rank == GuildMember::GUILDMEMBER_RANK_MASTER ||
        rank == GuildMember::GUILDMEMBER_RANK_SUBMASTER) {
        // Increase ActiveMemberCount when a normal member or a (sub)master is added.
        m_ActiveMemberCount++;
    } else if (rank == GuildMember::GUILDMEMBER_RANK_WAIT) {
        // Increase WaitMemberCount when a waiting member is added.
        m_WaitMemberCount++;
    }

#ifdef __SHARED_SERVER__
    saveCount();
#endif

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    __END_CATCH
}


void Guild::deleteMember(const string& name) noexcept(false) {
    __BEGIN_TRY

    __ENTER_CRITICAL_SECTION(m_Mutex)

    HashMapGuildMemberItor itr;

    itr = m_Members.find(name);

    if (itr == m_Members.end()) {
        cerr << "Guild::deleteMember() : NoSuchElementException" << endl;

        return;
    }

    GuildMemberRank_t rank = itr->second->getRank();

    if (rank == GuildMember::GUILDMEMBER_RANK_NORMAL || rank == GuildMember::GUILDMEMBER_RANK_MASTER ||
        rank == GuildMember::GUILDMEMBER_RANK_SUBMASTER) {
        // Decrease the active member counter
        m_ActiveMemberCount--;
    } else if (rank == GuildMember::GUILDMEMBER_RANK_WAIT) {
        m_WaitMemberCount--;
    }

    SAFE_DELETE(itr->second);

    m_Members.erase(itr);

#ifdef __SHARED_SERVER__
    saveCount();
#endif

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    __END_CATCH
}


void Guild::modifyMember(GuildMember& Member) noexcept(false) {
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


void Guild::modifyMemberRank(const string& name, GuildMemberRank_t rank) noexcept(false) {
    __BEGIN_TRY

    GuildMember* pMember = getMember(name);
    if (pMember == NULL)
        return;

    GuildMemberRank_t oldRank = pMember->getRank();

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

    __END_CATCH
}


#ifdef __GAME_SERVER__
void Guild::addCurrentMember(const string& name) noexcept(false) {
    __BEGIN_TRY

    __ENTER_CRITICAL_SECTION(m_Mutex) // a different mutex would probably do.

    if (m_CurrentMembers.end() != find(m_CurrentMembers.begin(), m_CurrentMembers.end(), name)) {
        return;
    }

    m_CurrentMembers.push_back(name);

    // Set log-on on the Guild Member object.
    GuildMember* pGuildMember = getMember_NOLOCKED(name);
    if (pGuildMember == NULL) {
        return;
    }

    pGuildMember->setLogOn(true);

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    __END_CATCH
}

void Guild::deleteCurrentMember(const string& name) noexcept(false) {
    __BEGIN_TRY

    __ENTER_CRITICAL_SECTION(m_Mutex)

    list<string>::iterator itr = find(m_CurrentMembers.begin(), m_CurrentMembers.end(), name);

    if (m_CurrentMembers.end() == itr) {
        return;
    }

    m_CurrentMembers.erase(itr);

    // Set log-off on the Guild Member object.
    GuildMember* pGuildMember = getMember_NOLOCKED(name);
    if (pGuildMember == NULL) {
        return;
    }

    pGuildMember->setLogOn(false);

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    __END_CATCH
}

list<string> Guild::getCurrentMembers() noexcept(false) {
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
void Guild::makeInfo(GuildInfo2* pGuildInfo) noexcept(false) {
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

void Guild::makeInfo(GuildInfo* pGuildInfo) noexcept(false) {
    __BEGIN_TRY

    __ENTER_CRITICAL_SECTION(m_Mutex)

    pGuildInfo->setGuildID(m_ID);
    pGuildInfo->setGuildName(m_Name);
    pGuildInfo->setGuildMaster(m_Master);
    pGuildInfo->setGuildMemberCount(m_ActiveMemberCount);

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    __END_CATCH
}

void Guild::makeMemberInfo(GCGuildMemberList& gcGuildMemberList) noexcept(false) {
    __BEGIN_TRY

    __ENTER_CRITICAL_SECTION(m_Mutex)

    HashMapGuildMember& Members = getMembers();
    HashMapGuildMemberConstItor itr = Members.begin();

    for (; itr != Members.end(); itr++) {
        GuildMember* pGuildMember = itr->second;

        GuildMemberInfo* pGuildMemberInfo = new GuildMemberInfo();
        pGuildMemberInfo->setName(pGuildMember->getName());
        pGuildMemberInfo->setRank(pGuildMember->getRank());
        pGuildMemberInfo->setLogOn(pGuildMember->getLogOn());

        gcGuildMemberList.addGuildMemberInfo(pGuildMemberInfo);
    }

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    __END_CATCH
}


void Guild::expireTimeOutWaitMember(VSDateTime currentDateTime, list<string>& mList) noexcept(false) {
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


string Guild::toString() const noexcept {
    StringStream msg;
    msg << " GuildID = " << m_ID << " GuildName = " << m_Name << " GuildType = " << (int)m_Type
        << " GuildState = " << (int)m_State << " ServerGroupID = " << (int)m_ServerGroupID
        << " GuildZoneID = " << (int)m_ZoneID << " Master = " << m_Master << " Date = " << m_Date << " \n";

    return msg.toString();
}

string Guild::correctString(const string& str) noexcept {
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
}
