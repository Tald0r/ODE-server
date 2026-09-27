//////////////////////////////////////////////////////////////////////
//
// SocketEncryptInputStream.h
//
// by Reiot
//
//////////////////////////////////////////////////////////////////////

#ifndef __SOCKET_ENCRYPT_INPUT_STREAM_H__
#define __SOCKET_ENCRYPT_INPUT_STREAM_H__

// include files
#include "Encrypter.h"
#include "Exception.h"
#include "SocketInputStream.h"
#include "Types.h"

const unsigned int DefaultSocketEncryptInputBufferSize = 81920;

//////////////////////////////////////////////////////////////////////
//
// class SocketEncryptInputStream
//
//////////////////////////////////////////////////////////////////////

class SocketEncryptInputStream : public SocketInputStream {
    //////////////////////////////////////////////////
    // constructor/destructor
    //////////////////////////////////////////////////
public:
    // constructor
    SocketEncryptInputStream(Socket* sock, uint BufferSize = DefaultSocketEncryptInputBufferSize);
    ~SocketEncryptInputStream();

    //////////////////////////////////////////////////
    // methods
    //////////////////////////////////////////////////
public:
    // read data to stream (output buffer)
    // *CAUTION*
    // When writing a string to the buffer the size can be prefixed automatically.
    // It is an open question whether the string size should be a BYTE or a WORD.
    // Since the policy is that a packet should be as small as possible, the string
    // size uses a BYTE or a WORD by hand as needed.
    uint readEncrypt(bool& buf) {
        uint n = read(buf);
        buf = m_Encrypter.convert(buf);
        return n;
    }
    uint readEncrypt(char& buf) {
        uint n = read(buf);
        buf = m_Encrypter.convert(buf);
        return n;
    }
    uint readEncrypt(uchar& buf) {
        uint n = read(buf);
        buf = m_Encrypter.convert(buf);
        return n;
    }
    uint readEncrypt(short& buf) {
        uint n = read(buf);
        buf = m_Encrypter.convert(buf);
        return n;
    }
    uint readEncrypt(ushort& buf) {
        uint n = read(buf);
        buf = m_Encrypter.convert(buf);
        return n;
    }
    uint readEncrypt(int& buf) {
        uint n = read(buf);
        buf = m_Encrypter.convert(buf);
        return n;
    }
    uint readEncrypt(uint& buf) {
        uint n = read(buf);
        buf = m_Encrypter.convert(buf);
        return n;
    }
    // No signed 64-bit overload: the protocol has no signed 64-bit
    // field, and `long` is 8 bytes here but 4 on the Win32 client, so
    // de::WireScalar rejects it (WireTypes.h). This overload was dead
    // -- nothing ever called it -- but being defined in-class it still
    // instantiated SocketInputStream::read<long>. The 64-bit overload
    // below stays, spelled std::uint64_t, the width the Exchange listing
    // id uses: that is `ulong` on LP64 Linux but `unsigned long long` on
    // macOS, where de::WireScalar rejects `unsigned long`. The convert()
    // goes through ulong, which is 64 bits on both.
    uint readEncrypt(std::uint64_t& buf) {
        uint n = read(buf);
        buf = m_Encrypter.convert(static_cast<ulong>(buf));
        return n;
    }

    void setEncryptCode(uchar code) {
        m_Encrypter.setCode(code);
    }
    uchar getEncryptCode() const {
        return m_Encrypter.getCode();
    }

    //////////////////////////////////////////////////
    // attributes
    //////////////////////////////////////////////////
private:
    Encrypter m_Encrypter;
};

#endif
