//////////////////////////////////////////////////////////////////////
//
// SocketAPI.cpp
//
// by Reiot, the Fallen Lord of MUDMANIA(TM)
//
//////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////
// include files
//////////////////////////////////////////////////

#include "SocketAPI.h"

#if defined(__LINUX__) || defined(__APPLE__)
#include <errno.h> // for errno

#include <arpa/inet.h> // for inet_xxx()
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h> // for accept()
#endif

#include "FileAPI.h"


//////////////////////////////////////////////////
// external variable
//////////////////////////////////////////////////
#if defined(__LINUX__) || defined(__APPLE__)
extern int errno;
#endif

using namespace FileAPI;

//////////////////////////////////////////////////////////////////////
//
// SOCKET SocketAPI::socket_ex ( int domain , int type , int protocol )
//
//
// exception version of socket()
//
// Parameters
//     domain - AF_INET(internet socket), AF_UNIX(internal socket), ...
//	   type  - SOCK_STREAM(TCP), SOCK_DGRAM(UDP), ...
//     protocol - 0
//
// Return
//     socket descriptor
//
// Exceptions
//     Error
//
//////////////////////////////////////////////////////////////////////
SOCKET SocketAPI::socket_ex(int domain, int type, int protocol) {
    __BEGIN_TRY

    SOCKET s = ::socket(domain, type, protocol);

    if (s == INVALID_SOCKET) {
#if defined(__LINUX__) || defined(__APPLE__)
        switch (errno) {
        case EPROTONOSUPPORT:
            throw Error("The protocol type or the specified protocol is not supported within this domain.");
        case EMFILE:
            throw Error("The per-process descriptor table is full.");
        case ENFILE:
            throw Error("The system file table is full.");
        case EACCES:
            throw Error("Permission to create a socket of the specified type and/or protocol is denied.");
        case ENOBUFS:
            throw Error("Insufficient buffer space is available. The socket cannot be created until sufficient "
                        "resources are freed.");
        default:
            throw UnknownError(strerror(errno), errno);
        } // end of switch
#endif
    }

    return s;

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////
//
// void SocketAPI::bind_ex ( SOCKET s , const struct sockaddr * addr , uint addrlen )
//      ;
//
// exception version of bind()
//
// Parameters
//     s       - socket descriptor
//     addr    - socket address structure ( normally struct sockaddr_in )
//     addrlen - length of socket address structure
//
// Return
//     none
//
// Exceptions
//     MBindException
//     Error
//
//////////////////////////////////////////////////////////////////////
void SocketAPI::bind_ex(SOCKET s, const struct sockaddr* addr, uint addrlen) {
    __BEGIN_TRY

    if (bind(s, addr, addrlen) == SOCKET_ERROR) {
#if defined(__LINUX__) || defined(__APPLE__)
        switch (errno) {
        case EADDRINUSE:
            throw BindException("The address is already in use. Kill the server holding this address or port, "
                                "or use another port.");
        case EINVAL:
            throw BindException("The socket is already bound to an address , or the addr_len was wrong, or the socket "
                                "was not in the AF_UNIX family.");
        case EACCES:
            throw BindException("The address is protected, and the user is not the super-user. or search permission is "
                                "denied on a component of the path prefix.");
        case ENOTSOCK:
            throw Error("Argument is a descriptor for a file, not a socket. The following errors are specific to UNIX "
                        "domain (AF_UNIX) sockets:");
        case EBADF:
            throw Error("sockfd is not a valid descriptor.");
        case EROFS:
            throw Error("The socket inode would reside on a read-only file system.");
        case EFAULT:
            throw Error("my_addr points outside your accessible address space.");
        case ENAMETOOLONG:
            throw Error("my_addr is too long.");
        case ENOENT:
            throw Error("The file does not exist.");
        case ENOMEM:
            throw Error("Insufficient kernel memory was available.");
        case ENOTDIR:
            throw Error("A component of the path prefix is not a directory.");
        case ELOOP:
            throw Error("Too many symbolic links were encountered in resolving my_addr.");
        default:
            throw UnknownError(strerror(errno), errno);
        } // end of switch
#endif
    }

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////
//
// void SocketAPI::connect_ex ( SOCKET s , const struct sockaddr * addr , uint addrlen )
//      ;
//
// exception version of connect() system call
//
// Parameters
//     s       - socket descriptor
//     addr    - socket address structure
//     addrlen - length of socket address structure
//
// Return
//     none
//
// Exceptions
//     ConnectException
//     NonBlockingIOException
//     Error
//
//////////////////////////////////////////////////////////////////////
void SocketAPI::connect_ex(SOCKET s, const struct sockaddr* addr, uint addrlen) {
    __BEGIN_TRY

    if (connect(s, addr, addrlen) == SOCKET_ERROR) {
#if defined(__LINUX__) || defined(__APPLE__)
        switch (errno) {
        case EALREADY:
            throw NonBlockingIOException(
                "The socket is non-blocking and a previous connection attempt has not yet been completed.");
        case EINPROGRESS:
            throw ConnectException("The socket is non-blocking and the connection can not be completed immediately.");
        case ECONNREFUSED:
            throw ConnectException("Connection refused at server.");
        case EISCONN:
            throw ConnectException("The socket is already connected.");
        case ETIMEDOUT:
            throw ConnectException("Timeout while attempting connection.");
        case ENETUNREACH:
            throw ConnectException("Network is unreachable.");
        case EADDRINUSE:
            throw ConnectException("Address is already in use.");
        case EBADF:
            throw Error("Bad descriptor.");
        case EFAULT:
            throw Error("The socket structure address is outside your address space.");
        case ENOTSOCK:
            throw Error("The descriptor is not associated with a socket.");
        default:
            throw UnknownError(strerror(errno), errno);
        } // end of switch
#endif
    }

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////
//
// void SocketAPI::listen_ex ( SOCKET s , uint backlog )
//      ;
//
// exception version of listen()
//
// Parameters
//     s       - socket descriptor
//     backlog - waiting queue length
//
// Return
//     none
//
// Exceptions
//     Error
//
//////////////////////////////////////////////////////////////////////
void SocketAPI::listen_ex(SOCKET s, uint backlog) {
    __BEGIN_TRY

    if (listen(s, backlog) == SOCKET_ERROR) {
#if defined(__LINUX__) || defined(__APPLE__)
        switch (errno) {
        case EBADF:
            throw Error("Bad descriptor.");
        case ENOTSOCK:
            throw Error("Not a socket.");
        case EOPNOTSUPP:
            throw Error("The socket is not of a type that supports the operation listen.");
        default:
            throw UnknownError(strerror(errno), errno);
        } // end of switch
#endif
    }

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////
//
// SOCKET SocketAPI::accept_ex ( SOCKET s , struct sockaddr * addr , uint * addrlen )
//       ;
//
// exception version of accept()
//
// Parameters
//     s       - socket descriptor
//     addr    - socket address structure
//     addrlen - length of socket address structure
//
// Return
//     none
//
// Exceptions
//     NonBlockingIOException
//     Error
//
//////////////////////////////////////////////////////////////////////
SOCKET SocketAPI::accept_ex(SOCKET s, struct sockaddr* addr, uint* addrlen) {
    __BEGIN_TRY

#if defined(__LINUX__) || defined(__APPLE__)
    SOCKET client = accept(s, addr, addrlen);
#endif

    if (client == INVALID_SOCKET) {
#if defined(__LINUX__) || defined(__APPLE__)
        switch (errno) {
        case EWOULDBLOCK:
            throw NonBlockingIOException();

        case ECONNRESET:
        case ECONNABORTED:
        case EPROTO:
        case EINTR:
            // from UNIX Network Programming 2nd, 15.6
            // with nonblocking-socket, ignore above errors
            throw ConnectException(strerror(errno));

        case EBADF:
        case ENOTSOCK:
        case EOPNOTSUPP:
        case EFAULT:
            throw Error(strerror(errno));

        default:
            throw UnknownError(strerror(errno), errno);
        } // end of switch
#endif
    } else {
    }

    return client;

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////
//
// void SocketAPI::getsockopt_ex ( SOCKET s , int level , int optname , void * optval , uint * optlen )
//      ;
//
// exception version of getsockopt()
//
// Parameters
//     s       - socket descriptor
//     level   - socket option level ( SOL_SOCKET , ... )
//     optname - socket option name ( SO_REUSEADDR , SO_LINGER , ... )
//     optval  - pointer to contain option value
//     optlen  - length of optval
//
// Return
//     none
//
// Exceptions
//     Error
//
//////////////////////////////////////////////////////////////////////
void SocketAPI::getsockopt_ex(SOCKET s, int level, int optname, void* optval, uint* optlen) {
    __BEGIN_TRY

#if defined(__LINUX__) || defined(__APPLE__)
    if (getsockopt(s, level, optname, optval, optlen) == SOCKET_ERROR) {
        switch (errno) {
        case EBADF:
            throw Error("Bad descriptor.");
        case ENOTSOCK:
            throw Error("Not a socket.");
        case ENOPROTOOPT:
            throw Error("The option is unknown at the level indicated.");
        case EFAULT:
            throw Error(
                "The address pointed to by optval is not in a valid part of the process address space. For getsockopt, "
                "this error may also be returned if optlen is not in a valid part of the process address space.");
        default:
            throw UnknownError(strerror(errno), errno);
        } // end of switch
    }
#endif

    __END_CATCH
}

uint SocketAPI::getsockopt_ex2(SOCKET s, int level, int optname, void* optval, uint* optlen) {
    __BEGIN_TRY

#if defined(__LINUX__) || defined(__APPLE__)
    if (getsockopt(s, level, optname, optval, optlen) == SOCKET_ERROR) {
        switch (errno) {
        case EBADF:
            return 1;
        case ENOTSOCK:
            return 2;
        case ENOPROTOOPT:
            return 3;
        case EFAULT:
            return 4;
        default:
            return 5;
        } // end of switch
    }
    return 0;
#endif

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////
//
// void SocketAPI::setsockopt_ex ( SOCKET s , int level , int optname , const void * optval , uint optlen )
//      ;
//
// exception version of setsockopt()
//
// Parameters
//     s       - socket descriptor
//     level   - socket option level ( SOL_SOCKET , ... )
//     optname - socket option name ( SO_REUSEADDR , SO_LINGER , ... )
//     optval  - pointer to contain option value
//     optlen  - length of optval
//
// Return
//     none
//
// Exceptions
//     Error
//
//////////////////////////////////////////////////////////////////////
void SocketAPI::setsockopt_ex(SOCKET s, int level, int optname, const void* optval, uint optlen) {
    __BEGIN_TRY

#if defined(__LINUX__) || defined(__APPLE__)
    if (setsockopt(s, level, optname, optval, optlen) == SOCKET_ERROR) {
        switch (errno) {
        case EBADF:
            throw Error("Bad descriptor.");
        case ENOTSOCK:
            throw Error("Not a socket.");
        case ENOPROTOOPT:
            throw Error("The option is unknown at the level indicated.");
        case EFAULT:
            throw Error(
                "The address pointed to by optval is not in a valid part of the process address space. For getsockopt, "
                "this error may also be returned if optlen is not in a valid part of the process address space.");
        default:
            throw UnknownError(strerror(errno), errno);
        } // end of switch
    }
#endif

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////
//
// uint SocketAPI::send_ex ( SOCKET s , const void * buf , uint len , uint flags )
//
//
// exception version of send()
//
// Parameters
//     s     - socket descriptor
//     buf   - input buffer
//     len   - input data length
//     flags - send flag (MSG_OOB,MSG_DONTROUTE)
//
// Return
//     length of bytes sent
//
// Exceptions
//     NonBlockingIOException
//     ConnectException
//     Error
//
//////////////////////////////////////////////////////////////////////
uint SocketAPI::send_ex(SOCKET s, const void* buf, uint len, uint flags) {
    __BEGIN_TRY

    int nSent;

    try {
#if defined(__LINUX__) || defined(__APPLE__)
        nSent = send(s, buf, len, flags);
#endif

        if (nSent == SOCKET_ERROR) {
#if defined(__LINUX__) || defined(__APPLE__)
            switch (errno) {
            case EWOULDBLOCK:
                // throw NonBlockingIOException();
                //  by sigi. 2002.5.17
                return 0;

            case ECONNRESET:
            case EPIPE:
                throw ConnectException(strerror(errno));

            case EBADF:
            case ENOTSOCK:
            case EFAULT:
            case EMSGSIZE:
            case ENOBUFS:
                throw Error(strerror(errno));

            default:
                throw UnknownError(strerror(errno), errno);
            } // end of switch
#endif
        } else if (nSent == 0) {
            throw ConnectException("connect closed.");
        }
    } catch (Throwable& t) {
        cout << "SocketAPI::send_ex Exception Check!" << endl;
        cout << t.toString() << endl;
        throw InvalidProtocolException("SocketAPI::send_ex failed");
    }

    return nSent;

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////
// exception version of sendto()
//////////////////////////////////////////////////////////////////////
uint SocketAPI::sendto_ex(SOCKET s, const void* buf, int len, unsigned int flags, const struct sockaddr* to,
                          int tolen) {
    __BEGIN_TRY

#if defined(__LINUX__) || defined(__APPLE__)
    int nSent = sendto(s, buf, len, flags, to, tolen);
#endif

    if (nSent == SOCKET_ERROR) {
#if defined(__LINUX__) || defined(__APPLE__)
        switch (errno) {
        case EWOULDBLOCK:
            // throw NonBlockingIOException();
            //  by sigi. 2002.5.17
            return 0;

        case ECONNRESET:
        case EPIPE:
            throw ConnectException(strerror(errno));

        case EBADF:
        case ENOTSOCK:
        case EFAULT:
        case EMSGSIZE:
        case ENOBUFS:
            throw Error(strerror(errno));

        default:
            // throw UnknownError(strerror(errno),errno);
            throw ConnectException(strerror(errno));
        }
#endif
    }

    return nSent;

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////
//
// uint SocketAPI::recv_ex ( SOCKET s , void * buf , uint len , uint flags )
//
//
// exception version of recv()
//
// Parameters
//     s     - socket descriptor
//     buf   - input buffer
//     len   - input data length
//     flags - send flag (MSG_OOB,MSG_DONTROUTE)
//
// Return
//     length of bytes received
//
// Exceptions
//     NonBlockingIOException
//     ConnectException
//     Error
//
//////////////////////////////////////////////////////////////////////
uint SocketAPI::recv_ex(SOCKET s, void* buf, uint len, uint flags) {
    __BEGIN_TRY

#if defined(__LINUX__) || defined(__APPLE__)
    int nrecv = recv(s, buf, len, flags);
#endif

    if (nrecv == SOCKET_ERROR) {
#if defined(__LINUX__) || defined(__APPLE__)
        switch (errno) {
        case EWOULDBLOCK:
            // by sigi. 2002.5.17
            return 0;
            // throw NonBlockingIOException();

        case ECONNRESET:
        case EPIPE:
            throw ConnectException(strerror(errno));

        case EBADF:
        case ENOTCONN:
        case ENOTSOCK:
        case EINTR:
        case EFAULT:
            throw Error(strerror(errno));

        default:
            throw UnknownError(strerror(errno), errno);
        } // end of switch

#endif
    } else if (nrecv == 0) {
        throw PeerClosedException();
    }

    return nrecv;

    __END_CATCH
}


/////////////////////////////////////////////////////////////////////
// exception version of recvfrom()
/////////////////////////////////////////////////////////////////////
uint SocketAPI::recvfrom_ex(SOCKET s, void* buf, int len, uint flags, struct sockaddr* from, uint* fromlen) {
    __BEGIN_TRY

#if defined(__LINUX__) || defined(__APPLE__)
    int nReceived = recvfrom(s, buf, len, flags, from, fromlen);


#endif

    if (nReceived == SOCKET_ERROR) {
#if defined(__LINUX__) || defined(__APPLE__)
        switch (errno) {
        case EWOULDBLOCK:
            // throw NonBlockingIOException();
            //  by sigi. 2002.5.17
            return 0;

        case ECONNRESET:
        case EPIPE:
            throw ConnectException(strerror(errno));

        case EBADF:
        case ENOTCONN:
        case ENOTSOCK:
        case EINTR:
        case EFAULT:
            throw Error(strerror(errno));

        default:
            throw UnknownError(strerror(errno), errno);
        } // end of switch
#endif
    }

    return nReceived;

    __END_CATCH
}


/////////////////////////////////////////////////////////////////////
//
// void SocketAPI::closesocket_ex ( SOCKET s )
//
//
// exception version of closesocket()
//
// Parameters
//     s - socket descriptor
//
// Return
//     none
//
// Exceptions
//     Error
//
/////////////////////////////////////////////////////////////////////
void SocketAPI::closesocket_ex(SOCKET s) {
    __BEGIN_TRY

#if defined(__LINUX__) || defined(__APPLE__)
    // using close_ex()
    FileAPI::close_ex(s);
#endif

    __END_CATCH
}


/////////////////////////////////////////////////////////////////////
//
// void SocketAPI::ioctlsocket_ex ( SOCKET s , long cmd , ulong * argp )
//
//
// exception version of ioctlsocket()
//
/////////////////////////////////////////////////////////////////////
void SocketAPI::ioctlsocket_ex(SOCKET s, long cmd, ulong* argp) {
    __BEGIN_TRY

#if defined(__LINUX__) || defined(__APPLE__)
    throw UnsupportedError();
#endif

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////
//
// bool SocketAPI::getsocketnonblocking_ex ( SOCKET s )
//      ;
//
// check if this socket is nonblocking mode
//
// Parameters
//     s - socket descriptor
//
// Return
//     true if nonblocking, false if blocking
//
// Exceptions
//     Error
//
//////////////////////////////////////////////////////////////////////
bool SocketAPI::getsocketnonblocking_ex(SOCKET s) {
    __BEGIN_TRY

#if defined(__LINUX__) || defined(__APPLE__)
    return FileAPI::getfilenonblocking_ex(s);
#endif

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////
//
// void SocketAPI::setsocketnonblocking_ex ( SOCKET s , bool on )
//      ;
//
// make this socket blocking/nonblocking
//
// Parameters
//     s  - socket descriptor
//     on - true if nonblocking, false if blocking
//
// Return
//     none
//
// Exceptions
//     Error
//
//////////////////////////////////////////////////////////////////////
void SocketAPI::setsocketnonblocking_ex(SOCKET s, bool on) {
    __BEGIN_TRY

#if defined(__LINUX__) || defined(__APPLE__)
    FileAPI::setfilenonblocking_ex(s, on);
#endif

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////
//
// uint SocketAPI::availablesocket_ex ( SOCKET s )
//
//
// get amount of data in socket input buffer
//
// Parameters
//    s - socket descriptor
//
// Return
//    amount of data in socket input buffer
//
// Exceptions
//    Error
//
//////////////////////////////////////////////////////////////////////
uint SocketAPI::availablesocket_ex(SOCKET s) {
    __BEGIN_TRY

#if defined(__LINUX__) || defined(__APPLE__)
    return availablefile_ex(s);
#endif

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////
//
// void SocketAPI::shutdown_ex ( SOCKET s , uint how )
//
//
// shutdown all or part of connection of socket
//
// Parameters
//     s   - socket descriptor
//     how - how to close ( all , send , receive )
//
// Return
//     none
//
// Exceptions
//     Error
//
//////////////////////////////////////////////////////////////////////
void SocketAPI::shutdown_ex(SOCKET s, uint how) {
    __BEGIN_TRY

    if (shutdown(s, how) < 0) {
#if defined(__LINUX__) || defined(__APPLE__)
        switch (errno) {
        case EBADF:
            throw Error("s is not a valid descriptor.");
        case ENOTSOCK:
            throw Error("s is a file, not a socket.");
        case ENOTCONN:
            throw Error("The specified socket is not connected.");
        default:
            throw UnknownError(strerror(errno), errno);
        }
#endif
    }

    __END_CATCH
}
