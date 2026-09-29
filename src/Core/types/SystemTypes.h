//////////////////////////////////////////////////////////////////////////////
// Filename    : SystemTypes.h
// Written By  : Reiot
// Description :
//////////////////////////////////////////////////////////////////////////////

#ifndef __SYSTEM_TYPES_H__
#define __SYSTEM_TYPES_H__

/* When the action/version is patched, the version information goes into the
 * BUILD_NUMBER and BUILD_INFO below. BUILD_NUMBER is the build number, BUILD_INFO is  +Add (something added) -Delete (something removed) *Fix
 * (a fix or a change), recorded together with the version
 */
#define BUILD_NUMBER 40518
#define BUILD_INFO "<Version Information>\n+Add ----- \n-Delete -----\nFix -----\n"


#if defined(__LINUX__) || defined(__APPLE__)
#include <sys/types.h>
#endif

#include <cstddef>
#include <fstream>
#include <iostream>
#include <string>

using namespace std;

//////////////////////////////////////////////////////////////////////////////
// The entry a table of names gives a value, or the value as a number when
// the table has no entry for it. Debug strings name enumerators through
// such tables, and a field decoded off the wire holds whatever byte the
// sender put there, so it is never used as an index unchecked.
//////////////////////////////////////////////////////////////////////////////
template <typename Value, std::size_t N> string nameOrNumber(const string (&names)[N], Value value) {
    const long long index = static_cast<long long>(value);
    if (index < 0 || static_cast<unsigned long long>(index) >= N)
        return std::to_string(index);
    return names[index];
}

//////////////////////////////////////////////////////////////////////////////
// built-in type redefinition
//////////////////////////////////////////////////////////////////////////////
typedef unsigned char uchar;
typedef unsigned short ushort;
typedef unsigned int uint;
typedef unsigned long ulong;

#if defined(__LINUX__) || defined(__APPLE__) || defined(__WIN_CONSOLE__)
typedef unsigned char BYTE;
typedef unsigned short WORD;
typedef unsigned int DWORD;
typedef unsigned long long ulonglong;
#else
typedef unsigned __int64 ulonglong;
#endif

#if defined(__LINUX__) || defined(__APPLE__)
const char separatorChar = '/';
const string separator = "/";
#endif


//////////////////////////////////////////////////////////////////////////////
// built-in type size
//////////////////////////////////////////////////////////////////////////////
const unsigned int szbool = sizeof(bool);
const unsigned int szchar = sizeof(char);
const unsigned int szshort = sizeof(short);
const unsigned int szint = sizeof(int);
const unsigned int szlong = sizeof(long);
const unsigned int szuchar = sizeof(unsigned char);
const unsigned int szushort = sizeof(unsigned short);
const unsigned int szuint = sizeof(unsigned int);
const unsigned int szulong = sizeof(unsigned long);
const unsigned int szBYTE = sizeof(BYTE);
const unsigned int szWORD = sizeof(WORD);
const unsigned int szDWORD = sizeof(DWORD);


//////////////////////////////////////////////////////////////////////////////
// ServerGroupInfo
//////////////////////////////////////////////////////////////////////////////
typedef BYTE ServerGroupID_t;
const uint szServerGroupID = sizeof(ServerGroupID_t);


//////////////////////////////////////////////////////////////////////////////
// SubServerInfo
//////////////////////////////////////////////////////////////////////////////
typedef WORD ServerID_t;
const uint szServerID = sizeof(ServerID_t);

typedef WORD UserNum_t;
const uint szUserNum = sizeof(UserNum_t);

//////////////////////////////////////////////////////////////////////////////
// SubServerInfo
//////////////////////////////////////////////////////////////////////////////
enum ServerStatus { SERVER_FREE = 0, SERVER_NORMAL, SERVER_BUSY, SERVER_VERY_BUSY, SERVER_FULL, SERVER_DOWN };

enum WorldStatus { WORLD_OPEN, WORLD_CLOSE };

typedef unsigned int IP_t;
const uint szIP = sizeof(IP_t);

typedef BYTE WorldID_t;
const uint szWorldID = sizeof(WorldID_t);

#endif
