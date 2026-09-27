//--------------------------------------------------------------------------------
//
// Filename    : Resource.cpp
// Written By  : Reiot
//
//--------------------------------------------------------------------------------

// include files
#include "Resource.h"

#include "Socket.h"
#include "WireString.h"


//--------------------------------------------------------------------------------
// constructor
//--------------------------------------------------------------------------------
Resource::Resource(Version_t version, const string& str) : m_Version(version), m_Filename(""), m_FileSize(0) {
    __BEGIN_TRY

    //--------------------------------------------------------------------------------
    //
    // The structure of the input string is as follows.
    //
    // ex> "creature.spk 30021"
    //      012345678901234567
    //
    //      length     = 17
    //      firstspace = 12
    //
    //--------------------------------------------------------------------------------
    if (str != "") {
        // Work out the index of the first and the last space.
        size_t firstspace = str.find_first_of(' ');

        if (firstspace == string::npos)
            throw InvalidProtocolException("invalid resource format");

        m_Filename = str.substr(0, firstspace);
        m_FileSize = atoi(str.substr(firstspace + 1, str.size() - firstspace).c_str());

        cout << toString() << endl;
    }

    __END_CATCH
}


//--------------------------------------------------------------------------------
// copy constructor
//--------------------------------------------------------------------------------
Resource::Resource(const Resource& resource)
    : m_Version(resource.m_Version), m_Filename(resource.m_Filename), m_FileSize(resource.m_FileSize) {}


//--------------------------------------------------------------------------------
// load from resource file
//--------------------------------------------------------------------------------
void Resource::load(ifstream& ifile) {
    __BEGIN_TRY

    //--------------------------------------------------------------------------------
    // read version
    //--------------------------------------------------------------------------------
    ifile.read((char*)&m_Version, szVersion);

    //--------------------------------------------------------------------------------
    // read filename
    //--------------------------------------------------------------------------------
    BYTE szFilename;
    ifile.read((char*)&szFilename, szBYTE);

    if (szFilename == 0) {
        throw Error("szFilename == 0");
    }

    char filename[maxFilename + 1];
    ifile.read(filename, szFilename);
    filename[szFilename] = 0;

    m_Filename = filename;

    //--------------------------------------------------------------------------------
    // read filesize
    //--------------------------------------------------------------------------------
    ifile.read((char*)&m_FileSize, szFileSize);

    __END_CATCH
}


//--------------------------------------------------------------------------------
// save to resource file
//--------------------------------------------------------------------------------
void Resource::save(ofstream& ofile) const {
    __BEGIN_TRY

    //--------------------------------------------------------------------------------
    // write version
    //--------------------------------------------------------------------------------
    ofile.write((const char*)&m_Version, szVersion);

    //--------------------------------------------------------------------------------
    // write filename
    //--------------------------------------------------------------------------------
    BYTE szFilename = m_Filename.size();

    if (szFilename == 0)
        throw Error("szFilename == 0");

    if (szFilename > 20)
        throw Error("too large Filename length");

    ofile.write((const char*)&szFilename, szBYTE);

    ofile.write(m_Filename.c_str(), szFilename);

    //--------------------------------------------------------------------------------
    // write filesize
    //--------------------------------------------------------------------------------
    ofile.write((const char*)&m_FileSize, szFileSize);

    __END_CATCH
}


//--------------------------------------------------------------------------------
// read from socket input stream
//--------------------------------------------------------------------------------
void Resource::read(SocketInputStream& iStream) {
    __BEGIN_TRY

    //--------------------------------------------------------------------------------
    // read version
    //--------------------------------------------------------------------------------
    iStream.read(m_Version);

    //--------------------------------------------------------------------------------
    // read filename
    //--------------------------------------------------------------------------------
    // maxFilename is 256, one past what the length byte can describe, so
    // the byte's own range is the cap. Names come from the resource list,
    // which save() holds to 20.
    de::wire::readString(iStream, m_Filename, {1, de::wire::kMaxByteStringLength}, "Filename");

    //--------------------------------------------------------------------------------
    // read filesize
    //--------------------------------------------------------------------------------
    iStream.read(m_FileSize);

    __END_CATCH
}


//--------------------------------------------------------------------------------
// read from socket
//--------------------------------------------------------------------------------
void Resource::read(Socket* pSocket) {
    __BEGIN_TRY

    //--------------------------------------------------------------------------------
    // read version
    //--------------------------------------------------------------------------------
    pSocket->receive(&m_Version, szVersion);

    //--------------------------------------------------------------------------------
    // read filename
    //--------------------------------------------------------------------------------
    BYTE szFilename;
    pSocket->receive(&szFilename, szBYTE);

    if (szFilename == 0)
        throw InvalidProtocolException("szFilename == 0");

    char filename[maxFilename + 1];
    pSocket->receive(filename, szFilename);
    filename[szFilename] = 0;

    m_Filename = filename;

    //--------------------------------------------------------------------------------
    // read filesize
    //--------------------------------------------------------------------------------
    pSocket->receive(&m_FileSize, szFileSize);

    __END_CATCH
}


//--------------------------------------------------------------------------------
// write to socket output stream
//--------------------------------------------------------------------------------
void Resource::write(SocketOutputStream& oStream) const {
    __BEGIN_TRY

    //--------------------------------------------------------------------------------
    // write version
    //--------------------------------------------------------------------------------
    oStream.write(m_Version);

    //--------------------------------------------------------------------------------
    // write filename
    //--------------------------------------------------------------------------------
    de::wire::writeString(oStream, m_Filename, {1, de::wire::kMaxByteStringLength}, "Filename");

    //--------------------------------------------------------------------------------
    // write filesize
    //--------------------------------------------------------------------------------
    oStream.write(m_FileSize);

    __END_CATCH
}


//--------------------------------------------------------------------------------
// write to socket
//--------------------------------------------------------------------------------
void Resource::write(Socket* pSocket) const {
    __BEGIN_TRY

    //--------------------------------------------------------------------------------
    // write version
    //--------------------------------------------------------------------------------
    pSocket->send(&m_Version, szVersion);

    //--------------------------------------------------------------------------------
    // write filename
    //--------------------------------------------------------------------------------
    BYTE szFilename = m_Filename.size();

    if (szFilename == 0)
        throw InvalidProtocolException("szFilename == 0");

    pSocket->send(&szFilename, szBYTE);

    pSocket->send(m_Filename.c_str(), szFilename);

    //--------------------------------------------------------------------------------
    // write filesize
    //--------------------------------------------------------------------------------
    pSocket->send(&m_FileSize, szFileSize);

    __END_CATCH
}


//--------------------------------------------------------------------------------
// get debug string
//--------------------------------------------------------------------------------
string Resource::toString() const {
    StringStream msg;
    msg << "Resource(" << "Version:" << m_Version << ",Filename:" << m_Filename << ",FileSize:" << m_FileSize << ")";
    return msg.toString();
}
