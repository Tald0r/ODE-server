//////////////////////////////////////////////////////////////////////
//
// ServerSocket.cpp
//
// by Reiot
//
//////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////
// include files
//////////////////////////////////////////////////
#include "ServerSocket.h"

#include <memory>

//////////////////////////////////////////////////////////////////////
// constructor
//////////////////////////////////////////////////////////////////////
ServerSocket::ServerSocket(uint port, uint backlog) : m_Impl(NULL) {
    __BEGIN_TRY

    // create socket implementation object
    auto impl = std::make_unique<SocketImpl>(port);

    // create socket
    impl->create();

    // reuse address before Bind()
    // Tell the system to reuse the address before binding.
    impl->setReuseAddr();

    // bind address to socket
    // The port is already stored in impl, so Bind() can be called without a parameter.
    impl->bind();


    // set listening queue size
    impl->listen(backlog);
    // A throwing constructor never runs ~ServerSocket. Keep ownership local
    // until create, bind and listen have all succeeded.
    m_Impl = impl.release();

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////
// destructor
//////////////////////////////////////////////////////////////////////
ServerSocket::~ServerSocket() noexcept {
    if (m_Impl != NULL) {
        m_Impl->close();
        delete m_Impl;
        m_Impl = NULL;
    }
}


//////////////////////////////////////////////////////////////////////
// close socket
//////////////////////////////////////////////////////////////////////
void ServerSocket::close() {
    __BEGIN_TRY

    m_Impl->close();

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////
// accept new connection
//////////////////////////////////////////////////////////////////////
Socket* ServerSocket::accept() {
    __BEGIN_TRY

    Socket* Client = NULL;

    try {
        SocketImpl* impl = m_Impl->accept();

        if (impl == NULL)
            throw UnknownError("impl == NULL");

        Client = new Socket(impl);
    } catch (NonBlockingIOException&) {
        // ignore
    } catch (ConnectException&) {
        // ignore
    }

    return Client;

    __END_CATCH
}
