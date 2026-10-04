//////////////////////////////////////////////////////////////////////////////
// Filename    : Utility.cpp
// Description :
// File that defines the macros and small functions used in many places.
//////////////////////////////////////////////////////////////////////////////

#include "Utility.h"

#include <stdio.h>
#include <stdlib.h>

#include <cerrno>
#include <cstdarg>
#include <cstring>
#include <new>

#include "Assert.h"
#include "BoundedDiagnosticFile.h"
#include "VSDateTime.h"

//////////////////////////////////////////////////////////////////////////////
// Rectangle class
//////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////
// Turn a number into a string
//////////////////////////////////////////////////////////////////////////////
string itos(int value) {
    char buf[100] = {
        0,
    };
    snprintf(buf, sizeof(buf), "%d", value);
    return string(buf);
}

//////////////////////////////////////////////////////////////////////////////
// Work out a direction
// Work out the direction from origin to dest in tile units. Taken from the client.
//////////////////////////////////////////////////////////////////////////////
#define BASIS_DIRECTION_HIGH 2.0f
#define BASIS_DIRECTION_LOW 0.5f
Dir_t computeDirection(int originX, int originY, int destX, int destY) {
    int stepX = destX - originX;
    int stepY = destY - originY;
    float k = (stepX == 0) ? 0 : (float)(stepY) / stepX;

    if (stepY == 0) {
        if (stepX == 0)
            return DOWN;
        else if (stepX > 0)
            return RIGHT;
        else
            return LEFT;
    } else if (stepY < 0) {
        if (stepX == 0) {
            return UP;
        } else if (stepX > 0) {
            if (k < -BASIS_DIRECTION_HIGH)
                return UP;
            else if (k < -BASIS_DIRECTION_LOW)
                return RIGHTUP;
            else
                return RIGHT;
        } else {
            if (k > BASIS_DIRECTION_HIGH)
                return UP;
            else if (k > BASIS_DIRECTION_LOW)
                return LEFTUP;
            else
                return LEFT;
        }
    } else {
        if (stepX == 0) {
            return DOWN;
        } else if (stepX > 0) {
            if (k > BASIS_DIRECTION_HIGH)
                return DOWN;
            else if (k > BASIS_DIRECTION_LOW)
                return RIGHTDOWN;
            else
                return RIGHT;
        } else {
            if (k < -BASIS_DIRECTION_HIGH)
                return DOWN;
            else if (k < -BASIS_DIRECTION_LOW)
                return LEFTDOWN;
            else
                return LEFT;
        }
    }

    return DIR_NONE;
}


//////////////////////////////////////////////////////////////////////////////
// Take one line out of a long string
// pos ends up as the index of the \n.
//////////////////////////////////////////////////////////////////////////////
string getline(const string& str, size_t& pos) {
    if (pos > str.size())
        return "";

    size_t oldpos = pos;
    pos = str.find_first_of('\n', oldpos);

    if (pos == string::npos)
        pos = str.size() + 1;
    else
        pos = pos + 1;

    return str.substr(oldpos, (pos - 1) - oldpos);
}

//////////////////////////////////////////////////////////////////////////////
// Strip the whitespace from both ends of a string
//////////////////////////////////////////////////////////////////////////////
string trim(const string& str) {
    if (str.size() == 0)
        return "";

    static const char* WhiteSpaces = " \t\n\r";
    size_t begin = str.find_first_not_of(WhiteSpaces);
    size_t end = str.find_last_not_of(WhiteSpaces);

    if (begin == string::npos)
        begin = 0;
    if (end == string::npos)
        end = str.size();

    return str.substr(begin, end - begin + 1);
}

//////////////////////////////////////////////////////////////////////////////
// Get the current time
//////////////////////////////////////////////////////////////////////////////
void getCurrentTimeEx(int& year, int& month, int& day, int& hour, int& minute, int& sec) {
    time_t cur_time = time(NULL);
    tm cur_tm;
    localtime_r(&cur_time, &cur_tm);

    year = cur_tm.tm_year + 1900;
    month = cur_tm.tm_mon + 1;
    day = cur_tm.tm_mday;
    hour = cur_tm.tm_hour;
    minute = cur_tm.tm_min;
    sec = cur_tm.tm_sec;
}

string getCurrentTimeStringEx(void) {
    int year, month, day, hour, minute, second;

    getCurrentTimeEx(year, month, day, hour, minute, second);

    string rValue;
    rValue += itos(year) + ". ";
    rValue += itos(month) + ". ";
    rValue += itos(day) + ". ";
    rValue += itos(hour) + ". ";
    rValue += itos(minute) + ". ";
    rValue += itos(second) + ". ";
    return rValue;
}

//////////////////////////////////////////////////////////////////////////////
// Split a word into its high and low bytes
//////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////
// Work out the percentage of a given number - rounded to one decimal place
//////////////////////////////////////////////////////////////////////////////
int getPercentValueEx(int value, int percent) {
    static int tick = 0;

    int thousand = getPercentValue(value, percent * 10);
    int ret = (thousand + tick) / 10;

    tick++;
    if (tick > 9)
        tick = 0;

    return ret;
}

//////////////////////////////////////////////////////////////////////////////
// Write a log to a file
//////////////////////////////////////////////////////////////////////////////
namespace {
void writeFileLog(const char* szFilename, const char* fmt, va_list valist, bool bounded) noexcept {
    if (!fmt) {
        de::reportDiagnosticFileFailure("format", EINVAL);
        return;
    }
    char buffer[30000];
    const int count = vsnprintf(buffer, sizeof(buffer), fmt, valist);
    if (count < 0) {
        de::reportDiagnosticFileFailure("format", EINVAL);
        return;
    }
    if (static_cast<std::size_t>(count) >= sizeof(buffer)) {
        constexpr char marker[] = " [truncated]";
        std::memcpy(buffer + sizeof(buffer) - sizeof(marker), marker, sizeof(marker));
    }
    try {
        const std::string record = VSDateTime::currentDateTime().toString() + " : " + buffer + "\n";
        if (bounded)
            de::appendBoundedDiagnosticFile(szFilename, record);
        else
            de::appendUnrotatedLogFile(szFilename, record);
    } catch (const std::bad_alloc&) {
        de::reportDiagnosticFileFailure("format allocation", ENOMEM);
    } catch (...) {
        de::reportDiagnosticFileFailure("format timestamp", EINVAL);
    }
}

} // namespace

void filelog(const char* szFilename, const char* fmt, ...) noexcept {
    va_list valist;
    va_start(valist, fmt);
    writeFileLog(szFilename, fmt, valist, false);
    va_end(valist);
}

void diagnosticFilelog(const char* szFilename, const char* fmt, ...) noexcept {
    va_list valist;
    va_start(valist, fmt);
    writeFileLog(szFilename, fmt, valist, true);
    va_end(valist);
}


//////////////////////////////////////////////////////////////////////////////
// Turn two WORDs into a DWORD. HIWORD, LOWORD
//////////////////////////////////////////////////////////////////////////////
DWORD makeDWORD(WORD hiWord, WORD loWord) {
    DWORD dwResult = ((DWORD)hiWord << 16) | loWord;

    return dwResult;
}

//////////////////////////////////////////////////////////////////////////////
// Take the HIWORD and LOWORD of a DWORD
//////////////////////////////////////////////////////////////////////////////
WORD getHIWORD(DWORD dwValue) {
    WORD wResult = (WORD)(dwValue >> 16);

    return wResult;
}

WORD getLOWORD(DWORD dwValue) {
    DWORD dwMask = 0x00FF;

    WORD wResult = (WORD)(dwMask & dwValue);

    return wResult;
}
