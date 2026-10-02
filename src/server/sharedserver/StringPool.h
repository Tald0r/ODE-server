////////////////////////////////////////////////////////////////////
// Filename : StringPool.h
// Desc     : pool of strings used internally
////////////////////////////////////////////////////////////////////

#ifndef __SHARED_SERVER_STRING_POOL_H__
#define __SHARED_SERVER_STRING_POOL_H__

#include <string>

#include <unordered_map>

#include "Exception.h"

enum StringID {
    STRID_TEAM_REGISTRATION_ACCEPT,   // 0
    STRID_TEAM_REGISTRATION_ACCEPT_2, // 1
    STRID_CLAN_REGISTRATION_ACCEPT,   // 2
    STRID_CLAN_REGISTRATION_ACCEPT_2, // 3
    STRID_TEAM_JOIN_ACCEPT,           // 4
    STRID_CLAN_JOIN_ACCEPT,           // 5
    STRID_TEAM_BROKEN,                // 6
    STRID_CLAN_BROKEN,                // 7
    STRID_TEAM_CANCEL,                // 8
    STRID_CLAN_CANCEL,                // 9

    STRID_MAX
};

class SharedConfigRepository;

// Load with quiescent readers. Failed loads retain the previous strings and
// borrowed c_str pointers; successful replacement/destruction invalidates them.
class StringPool {
public:
    void load();
    void load(SharedConfigRepository& repository);

    string getString(uint strID) const;
    const char* c_str(uint strID) const;

private:
    std::unordered_map<uint, string> m_Strings;
};

#endif // __SHARED_SERVER_STRING_POOL_H__
