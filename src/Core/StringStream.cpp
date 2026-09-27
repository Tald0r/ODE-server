//////////////////////////////////////////////////////////////////////
//
// Filename    : StringStream.cc
// Written By  : reiot@ewestsoft.com
// Description :
//
//////////////////////////////////////////////////////////////////////

// include files
#include "StringStream.h"

#include <stdio.h>

//////////////////////////////////////////////////////////////////////
// constructor
//////////////////////////////////////////////////////////////////////
StringStream::StringStream() : m_Size(0), m_bInserted(false), m_Buffer("") {}


//////////////////////////////////////////////////////////////////////
// destructor
//////////////////////////////////////////////////////////////////////
StringStream::~StringStream() {}


//////////////////////////////////////////////////////////////////////
// add string to stream
//////////////////////////////////////////////////////////////////////
StringStream& StringStream::operator<<(bool T) {
    string buf(T == true ? "true" : "false");

    m_Strings.push_back(buf);

    m_Size += buf.size();
    m_bInserted = true;

    return *this;
}

StringStream& StringStream::operator<<(char T) {
    string buf(1, '\0');
    buf[0] = T;

    m_Strings.push_back(buf);

    m_Size += buf.size();
    m_bInserted = true;

    return *this;
}

StringStream& StringStream::operator<<(uchar T) {
    string buf(1, 0);
    buf[0] = T;

    m_Strings.push_back(buf);

    m_Size += buf.size();
    m_bInserted = true;

    return *this;
}

StringStream& StringStream::operator<<(short T) {
    char buf[7];
    snprintf(buf, sizeof(buf), "%d", T);

    string str(buf);

    m_Strings.push_back(str);

    m_Size += str.size();
    m_bInserted = true;

    return *this;
}

StringStream& StringStream::operator<<(ushort T) {
    char buf[7];
    snprintf(buf, sizeof(buf), "%u", T);

    string str(buf);

    m_Strings.push_back(str);

    m_Size += str.size();
    m_bInserted = true;

    return *this;
}

StringStream& StringStream::operator<<(int T) {
    char buf[12];
    snprintf(buf, sizeof(buf), "%d", T);

    string str(buf);

    m_Strings.push_back(str);

    m_Size += str.size();
    m_bInserted = true;

    return *this;
}

StringStream& StringStream::operator<<(uint T) {
    char buf[12];
    snprintf(buf, sizeof(buf), "%u", T);

    string str(buf);

    m_Strings.push_back(str);

    m_Size += str.size();
    m_bInserted = true;

    return *this;
}

StringStream& StringStream::operator<<(long T) {
    // 64-bit on the LP64 build target: LONG_MIN prints as 20 digits plus a
    // sign, so the 12-byte buffer this was copy-pasted from the int overload
    // with overflowed the stack for any value outside the 32-bit range.
    char buf[24];
    snprintf(buf, sizeof(buf), "%ld", T);

    string str(buf);

    m_Strings.push_back(str);

    m_Size += str.size();
    m_bInserted = true;

    return *this;
}

StringStream& StringStream::operator<<(ulong T) {
    // See operator<<(long): 64-bit here, ULONG_MAX is 20 digits.
    char buf[24];
    snprintf(buf, sizeof(buf), "%lu", T);

    string str(buf);

    m_Strings.push_back(str);

    m_Size += str.size();
    m_bInserted = true;

    return *this;
}

StringStream& StringStream::operator<<(float T) {
    // Same defect as operator<<(long) had: "%f" prints the number in full,
    // never in exponent form, plus six decimals. A float argument is promoted
    // to double here, so FLT_MAX needs 39 integer digits + '.' + 6 + sign.
    char buf[64];
    snprintf(buf, sizeof(buf), "%f", T);

    string str(buf);

    m_Strings.push_back(str);

    m_Size += str.size();
    m_bInserted = true;

    return *this;
}

StringStream& StringStream::operator<<(double T) {
    // See operator<<(float): DBL_MAX is 309 integer digits under "%f".
    char buf[352];
    snprintf(buf, sizeof(buf), "%f", T);

    string str(buf);

    m_Strings.push_back(str);

    m_Size += str.size();
    m_bInserted = true;

    return *this;
}

StringStream& StringStream::operator<<(const char* buf) {
    string str(buf);

    m_Strings.push_back(str);

    m_Size += str.size();
    m_bInserted = true;

    return *this;
}

StringStream& StringStream::operator<<(const string& str) {
    m_Strings.push_back(str);

    m_Size += str.size();
    m_bInserted = true;

    return *this;
}


//////////////////////////////////////////////////////////////////////
// make string
//////////////////////////////////////////////////////////////////////
const string& StringStream::toString() const {
    // Once the string has been built,
    // On later calls it is used as it is unless something new is added.
    if (m_bInserted) {
        m_bInserted = false;

        // For speed, grab all the memory up front so pointless copies are avoided.
        m_Buffer.reserve(m_Size);

        for (list<string>::const_iterator itr = m_Strings.begin(); itr != m_Strings.end(); itr++) {
            // Append to the buffer one by one.
            m_Buffer.append(*itr);
        }
    }

    return m_Buffer;
}
