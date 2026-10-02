//--------------------------------------------------------------------------------
//
// Filename    : Properties.cpp
// Written By  : Reiot
// Description :
//
//--------------------------------------------------------------------------------

// include files
#include "Properties.h"

#include <stdlib.h> // atoi()

#include <filesystem>

#include "PropertiesParser.h"

//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
const char Properties::Comment = '#';
const char Properties::Separator = ':';
const char* Properties::WhiteSpaces = " \t";


//--------------------------------------------------------------------------------
// constructor
//--------------------------------------------------------------------------------
Properties::Properties(){__BEGIN_TRY __END_CATCH}

Properties::Properties(const string& filename)
    : m_Filename(filename){__BEGIN_TRY __END_CATCH}


      //--------------------------------------------------------------------------------
      // destructor
      //--------------------------------------------------------------------------------
      Properties::~Properties() noexcept {
    // Delete every pair.
    m_Properties.clear();
}


//--------------------------------------------------------------------------------
// load from file
//--------------------------------------------------------------------------------
void Properties::load() {
    __BEGIN_TRY

    if (m_Filename.empty())
        throw Error("filename not specified");

    // Some file stream implementations treat reading a directory as normal
    // EOF. Reject it here so startup cannot publish an empty configuration.
    std::error_code statusError;
    if (std::filesystem::is_directory(m_Filename, statusError))
        throw IOException("error reading properties: path is a directory");

    ifstream ifile(m_Filename.c_str(), ios::in);

    if (!ifile)
        throw FileNotExistException(m_Filename.c_str());

    de::readProperties(ifile, *this);

    ifile.close();

    __END_CATCH
}


//--------------------------------------------------------------------------------
// save to file
//--------------------------------------------------------------------------------
void Properties::save() {
    __BEGIN_TRY

    if (m_Filename.empty())
        throw Error("filename not specified");

    ofstream ofile(m_Filename.c_str(), ios::out | ios::trunc);

    for (map<string, string, StringCompare>::iterator itr = m_Properties.begin(); itr != m_Properties.end(); itr++)
        ofile << itr->first << ' ' << Separator << ' ' << itr->second << endl;

    ofile.close();

    __END_CATCH
}


//--------------------------------------------------------------------------------
// get property
//--------------------------------------------------------------------------------
string Properties::getProperty(string key) const {
    __BEGIN_TRY

    string value;

    map<string, string, StringCompare>::const_iterator itr = m_Properties.find(key);

    if (itr != m_Properties.end())
        value = itr->second;
    else
        throw NoSuchElementException(key);

    return value;

    __END_CATCH
}


//--------------------------------------------------------------------------------
// get property as int
//--------------------------------------------------------------------------------
int Properties::getPropertyInt(string key) const {
    __BEGIN_TRY

    return atoi(getProperty(key).c_str());

    __END_CATCH
}


//--------------------------------------------------------------------------------
// set property
//--------------------------------------------------------------------------------
void Properties::setProperty(string key, string value) {
    __BEGIN_TRY

    // If the key exists already, the value is overwritten.
    m_Properties[key] = value;

    __END_CATCH
}


//--------------------------------------------------------------------------------
// get debug string
//--------------------------------------------------------------------------------
string Properties::toString() const {
    __BEGIN_TRY

    StringStream msg;

    for (map<string, string, StringCompare>::const_iterator itr = m_Properties.begin(); itr != m_Properties.end();
         itr++) {
        msg << itr->first << " : " << itr->second << "\n";
    }

    if (msg.isEmpty())
        msg << "empty properties";

    return msg.toString();

    __END_CATCH
}
