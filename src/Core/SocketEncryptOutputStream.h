//////////////////////////////////////////////////////////////////////
//
// SocketEncryptOutputStream.h
//
// by Reiot
//
//////////////////////////////////////////////////////////////////////

#ifndef __SOCKET_ENCRYPT_OUTPUT_STREAM_H__
#define __SOCKET_ENCRYPT_OUTPUT_STREAM_H__

// include files
#include "Encrypter.h"
#include "Exception.h"
#include "SocketOutputStream.h"
#include "Types.h"

const unsigned int DefaultSocketEncryptOutputBufferSize = 81920;

//////////////////////////////////////////////////////////////////////
//
// class SocketEncryptOutputStream
//
//////////////////////////////////////////////////////////////////////

class SocketEncryptOutputStream : public SocketOutputStream {
    //////////////////////////////////////////////////
    // constructor/destructor
    //////////////////////////////////////////////////
public:
    // constructor
    SocketEncryptOutputStream(Socket* sock, uint BufferSize = DefaultSocketEncryptOutputBufferSize);
    ~SocketEncryptOutputStream();

    //////////////////////////////////////////////////
    // methods
    //////////////////////////////////////////////////
public:
    // write data to stream (output buffer)
    // *CAUTION*
    // When writing a string to the buffer the size can be prefixed automatically.
    // It is an open question whether the string size should be a BYTE or a WORD.
    // Since the policy is that a packet should be as small as possible, the string
    // size uses a BYTE or a WORD by hand as needed.
    uint writeEncrypt(bool buf) {
        buf = m_Encrypter.convert(buf);
        return write(buf);
    }
    uint writeEncrypt(char buf) {
        buf = m_Encrypter.convert(buf);
        return write(buf);
    }
    uint writeEncrypt(uchar buf) {
        buf = m_Encrypter.convert(buf);
        return write(buf);
    }
    uint writeEncrypt(short buf) {
        buf = m_Encrypter.convert(buf);
        return write(buf);
    }
    uint writeEncrypt(ushort buf) {
        buf = m_Encrypter.convert(buf);
        return write(buf);
    }
    uint writeEncrypt(int buf) {
        buf = m_Encrypter.convert(buf);
        return write(buf);
    }
    uint writeEncrypt(uint buf) {
        buf = m_Encrypter.convert(buf);
        return write(buf);
    }
    // No signed 64-bit overload: see the note in
    // SocketEncryptInputStream.h. It was dead code that nonetheless
    // instantiated SocketOutputStream::write<long>, which
    // de::WireScalar rejects. The 64-bit overload is spelled
    // std::uint64_t for the reason given there.
    uint writeEncrypt(std::uint64_t buf) {
        buf = m_Encrypter.convert(static_cast<ulong>(buf));
        return write(buf);
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
