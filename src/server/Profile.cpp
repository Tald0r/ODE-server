//////////////////////////////////////////////////////////////////////////////
// Filename    : Profile.cpp
// Written by  : excel96
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "Profile.h"

#include <stdio.h>

#include <fstream>
#include <iomanip>

#include "Assert.h"
#include "GMServerInfo.h"
#include "StringStream.h"
#include "Thread.h"
#include "VSDateTime.h"

//////////////////////////////////////////////////////////////////////////////
// global varibles
//////////////////////////////////////////////////////////////////////////////
ProfileSampleManager g_ProfileSampleManager;

//////////////////////////////////////////////////////////////////////////////
// class ProfileSample member methods
//////////////////////////////////////////////////////////////////////////////

ProfileSample::ProfileSample() {
    m_bUsed = false;
    m_Name = "";
    m_OpenCount = 0;
    m_CallCount = 0;
    m_ParentCount = 0;
    m_StartTime.tv_sec = 0;
    m_StartTime.tv_usec = 0;
    m_EndTime.tv_sec = 0;
    m_EndTime.tv_usec = 0;
    m_ChildTime.tv_sec = 0;
    m_ChildTime.tv_usec = 0;
    m_AccuTime.tv_sec = 0;
    m_AccuTime.tv_usec = 0;
}

ProfileSample::~ProfileSample() {}

string ProfileSample::getAverageTime(void) const {
    char avg_buf[256] = {
        0,
    };

    double accu_time = (m_AccuTime.tv_sec + (double)m_AccuTime.tv_usec / 1000000) / m_CallCount;

    snprintf(avg_buf, sizeof(avg_buf), "%5.9lfs", accu_time);

    return string(avg_buf);
}

string ProfileSample::getAccumulatedTime(void) const {
    char accu_buf[256] = {
        0,
    };
    snprintf(accu_buf, sizeof(accu_buf), "%ld.%06lds", m_AccuTime.tv_sec, static_cast<long>(m_AccuTime.tv_usec));

    return string(accu_buf);
}

string ProfileSample::getChildrenTime(void) const {
    char child_buf[256] = {
        0,
    };
    snprintf(child_buf, sizeof(child_buf), "%ld.%06lds", m_ChildTime.tv_sec, static_cast<long>(m_ChildTime.tv_usec));

    return string(child_buf);
}

string ProfileSample::toString(void) const {
    StringStream msg;
    msg << m_Name
        //<< ",OpenCount:" << m_OpenCount
        << ",AverageTime:" << getAverageTime() << ",AccuTime:" << getAccumulatedTime() << ",CallCount:"
        << m_CallCount
        //<< ",ParentCount:" << m_ParentCount
        << ",ChildTime:" << getChildrenTime();
    return msg.toString();
}

//////////////////////////////////////////////////////////////////////////////
// class ProfileHistroy member methods
//////////////////////////////////////////////////////////////////////////////

ProfileHistory::ProfileHistory() {
    m_bUsed = false;
    m_Name = "";
    m_Average = 0.0;
    m_Min = 0.0;
    m_Max = 0.0;
}

ProfileHistory::~ProfileHistory() {}

//////////////////////////////////////////////////////////////////////////////
// class ProfileSampleSet member methods
//////////////////////////////////////////////////////////////////////////////

ProfileSampleSet::ProfileSampleSet() {}

ProfileSampleSet::~ProfileSampleSet() {}

void ProfileSampleSet::initProfile(void) {
    for (int i = 0; i < MAX_PROFILE_SAMPLES; i++) {
        m_ProfileSamples[i].setUsed(false);
    }

    m_NameMap.clear();
}

void ProfileSampleSet::beginProfile(const string& name) {
    unordered_map<string, int>::iterator itr = m_NameMap.find(name);

    // If a sample with the same name already exists...
    if (itr != m_NameMap.end()) {
        int i = itr->second;

        m_ProfileSamples[i].setOpenCount(m_ProfileSamples[i].getOpenCount() + 1);
        m_ProfileSamples[i].setCallCount(m_ProfileSamples[i].getCallCount() + 1);
        m_ProfileSamples[i].setStartTime();

        // Recursive calls are not supported.
        Assert(m_ProfileSamples[i].getOpenCount() == 1);
    }
    // If it is being used for the first time...
    else {
        // an empty slot has to be found.
        for (int i = 0; i < MAX_PROFILE_SAMPLES; i++) {
            // If a sample that is not in use is found, use it.
            if (!m_ProfileSamples[i].isUsed()) {
                m_ProfileSamples[i].setUsed(true);
                m_ProfileSamples[i].setName(name);
                m_ProfileSamples[i].setOpenCount(1);
                m_ProfileSamples[i].setCallCount(1);
                m_ProfileSamples[i].setStartTime();
                m_ProfileSamples[i].initChildTime();
                m_ProfileSamples[i].initAccuTime();

                // Put the index into the hash map so that it can be searched quickly.
                m_NameMap[name] = i;

                return;
            }
        }

        // Getting here means the largest number of samples was exceeded.
        // So it is an error.
        Assert(false);
    }
}

void ProfileSampleSet::endProfile(const string& name) {
    unordered_map<string, int>::iterator itr = m_NameMap.find(name);

    // If no sample with that name exists,
    // it is an error...
    if (itr == m_NameMap.end()) {
        Assert(false);
    }

    int index = itr->second;
    int Parent = -1;
    int ParentCount = 0;

    Timeval endTime;
    getCurrentTime(endTime);

    m_ProfileSamples[index].setEndTime(endTime);

    m_ProfileSamples[index].setOpenCount(m_ProfileSamples[index].getOpenCount() - 1);

    // Compute the elapsed time.
    Timeval timeoffset = timediff(m_ProfileSamples[index].getStartTime(), endTime);

    // Count every possible parent and find the real parent.
    for (int i = 0; i < MAX_PROFILE_SAMPLES; i++) {
        // 1. it is a sample in use.
        // 2. it is open.
        // Such a sample can be a parent.
        if (m_ProfileSamples[i].isUsed() && m_ProfileSamples[i].getOpenCount() > 0) {
            ParentCount++;

            // The first parent...
            if (Parent < 0) {
                Parent = i;
            }
            // If it is not the first parent, the most recently opened parent is the real one.
            else if (m_ProfileSamples[i].getStartTime() >= m_ProfileSamples[Parent].getStartTime()) {
                Parent = i;
            }
        }
    }

    // Tell the current sample how many parents it has.
    m_ProfileSamples[index].setParentCount(ParentCount);

    // Add the elapsed time to the current sample's accumulated time.
    m_ProfileSamples[index].addAccuTime(timeoffset);

    if (Parent >= 0) {
        // Add the elapsed time to the parent's child time.
        m_ProfileSamples[Parent].addChildTime(timeoffset);
    }
}

void ProfileSampleSet::outputProfile(bool bOutputOnlyRootNode, bool bOutputThreadID) {
    cout << "==================================================" << endl;

    if (bOutputThreadID)
        cout << "TID:" << Thread::self() << endl;

    cout << setw(15) << " Average       ";
    cout << setw(15) << " Total         ";
    cout << setw(15) << " CallCount     ";
    cout << setw(15) << " Child         ";
    cout << setw(15) << " Name          ";
    cout << endl;

    for (int i = 0; i < MAX_PROFILE_SAMPLES; i++) {
        if (m_ProfileSamples[i].isUsed()) {
            cout << setw(15) << m_ProfileSamples[i].getAverageTime();
            cout << setw(15) << m_ProfileSamples[i].getAccumulatedTime();
            cout << setw(15) << m_ProfileSamples[i].getCallCount();
            cout << setw(15) << m_ProfileSamples[i].getChildrenTime();

            cout << " ";

            int ParentCount = m_ProfileSamples[i].getParentCount();
            for (int t = 0; t < ParentCount; t++) {
                cout << "  ";
            }

            cout << m_ProfileSamples[i].getName();
            cout << endl;
        }

        // Taking only the root node is the same as taking only the node at the very
        // front of the array. So take one and
        // return right away.
        if (bOutputOnlyRootNode) {
            return;
        }
    }

    cout << "==================================================" << endl;
}

void ProfileSampleSet::outputProfileToFile(const char* filename, bool bOutputOnlyRootNode, bool bOutputThreadID,
                                           GMServerInfo* pServerInfo) {}

void ProfileSampleSet::storeProfileInHistory(const string& name, float percent) {}

void ProfileSampleSet::getProfileFromHistory(const string& name, float& ave, float& min, float& max) {}

//////////////////////////////////////////////////////////////////////////////
// class ProfileSampleManager
//////////////////////////////////////////////////////////////////////////////

ProfileSampleManager::ProfileSampleManager() {
    init();
}

ProfileSampleManager::~ProfileSampleManager() {
    unordered_map<int, ProfileSampleSet*>::iterator itr = m_ProfileSampleMap.begin();
    for (; itr != m_ProfileSampleMap.end(); itr++) {
        SAFE_DELETE(itr->second);
    }

    m_ProfileSampleMap.clear();
}

void ProfileSampleManager::init(void) {
    m_Mutex.setName("ProfileSampleManager");
}

void ProfileSampleManager::addProfileSampleSet(int TID, ProfileSampleSet* pSet) {
    unordered_map<int, ProfileSampleSet*>::iterator itr = m_ProfileSampleMap.find(TID);
    if (itr == m_ProfileSampleMap.end()) {
        m_ProfileSampleMap[TID] = pSet;
    } else {
        SAFE_DELETE(pSet);
    }
}

ProfileSampleSet* ProfileSampleManager::getProfileSampleSet(void) {
    unordered_map<int, ProfileSampleSet*>::iterator itr = m_ProfileSampleMap.find((int)(long)Thread::self());

    // If there is none, create and register one.
    if (itr == m_ProfileSampleMap.end()) {
        ProfileSampleSet* pProfileSampleSet = new ProfileSampleSet;
        m_ProfileSampleMap[(int)(long)Thread::self()] = pProfileSampleSet;
        return pProfileSampleSet;
    }

    return itr->second;
}
