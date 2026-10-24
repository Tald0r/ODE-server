////////////////////////////////////////////////////////////////////////////////
// Filename    : PropertyBuffer.cpp
// Written By  :
// Description :
////////////////////////////////////////////////////////////////////////////////

#include "PropertyBuffer.h"

#include "Utility.h" // trim(), getline()


////////////////////////////////////////////////////////////////////////////////
// static constant variables
////////////////////////////////////////////////////////////////////////////////
const char PropertyBuffer::Comment = '#';
const char PropertyBuffer::Separator = ':';
const char PropertyBuffer::EOL = '\n';
const char* PropertyBuffer::WhiteSpaces = " \t\n";
const char* PropertyBuffer::SpaceTab = " \t";


////////////////////////////////////////////////////////////////////////////////
// constructor
////////////////////////////////////////////////////////////////////////////////
PropertyBuffer::PropertyBuffer(const string& buffer) {
    m_Index = 0;
    m_Buffer = buffer;
}


////////////////////////////////////////////////////////////////////////////////
// destructor
////////////////////////////////////////////////////////////////////////////////
PropertyBuffer::~PropertyBuffer() {}

string PropertyBuffer::getProperty(string key) {
    size_t bufferLen = m_Buffer.size();

    while (m_Index < bufferLen) {
        string line = trim(getline(m_Buffer, m_Index));

        // Neither an empty line nor a comment line.
        if (line.size() != 0 && line[0] != Comment) {
            size_t i = line.find(Separator);

            if (i == string::npos) {
                throw Error("missing separator");
            }

            string paramName = trim(line.substr(0, i - 1));

            if (paramName == key) {
                return trim(line.substr(i + 1));
            } else {
                throw NoSuchElementException(key);
            }
        }
    }

    cout << "PropertyBuffer::getProperty() : thers is no [" << key << "] element." << endl;
    throw NoSuchElementException(key);
}

int PropertyBuffer::getPropertyInt(string key) {
    return atoi(getProperty(key).c_str());
}

bool PropertyBuffer::getProperty(string key, string& value) {
    size_t bufferLen = m_Buffer.size();

    while (m_Index < bufferLen) {
        string line = trim(getline(m_Buffer, m_Index));

        // Neither an empty line nor a comment line.
        if (line.size() != 0 && line[0] != Comment) {
            size_t i = line.find(Separator);

            if (i == string::npos) {
                return false;
            }

            string paramName = trim(line.substr(0, i - 1));

            if (paramName == key) {
                value = trim(line.substr(i + 1));
                return true;
            } else {
                return false;
            }
        }
    }

    return false;
}

bool PropertyBuffer::getPropertyInt(string key, int& value) {
    string rstring;

    if (getProperty(key, rstring)) {
        value = atoi(rstring.c_str());
        return true;
    }

    return false;
}

////////////////////////////////////////////////////////////////////////////////
// get debug string
////////////////////////////////////////////////////////////////////////////////
string PropertyBuffer::toString() const {
    StringStream msg;

    msg << "PropertyBuffer(" << "Index:" << m_Index << ",Buffer:" << m_Buffer << ")";

    return msg.toString();
}
