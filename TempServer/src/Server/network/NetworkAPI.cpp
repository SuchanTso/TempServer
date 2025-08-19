#include "SPch.h"
#include "NetworkAPI.h"
#include <string>
#include <cstring> // For std::memset
#ifdef SERVER_PLATFORM_WINDOWS
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
#elif defined(SERVER_PLATFORM_MACOSX)
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <fcntl.h>
    #include <cerrno>
#endif


namespace Tso {

// Common InetAddress implementation for both platforms
// (Moved outside of #ifdef blocks to avoid duplication)
struct InetAddress {
public:
    InetAddress(const std::string& ip, const uint16_t& port) {
        std::memset(&addr_, 0, sizeof(addr_));
        addr_.sin_family = AF_INET;
        addr_.sin_port = htons(port);
        inet_pton(AF_INET, ip.c_str(), &addr_.sin_addr);
    }

    InetAddress() {
        std::memset(&addr_, 0, sizeof(addr_));
    }

    // Constructor from a native sockaddr_in structure
    InetAddress(const sockaddr_in& addr) : addr_(addr) {}

    std::string GetIP() const {
        char buffer[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &addr_.sin_addr, buffer, INET_ADDRSTRLEN);
        return std::string(buffer);
    }

    uint16_t GetPort() const { return ntohs(addr_.sin_port); }

    // For const access (e.g., connect)
    const sockaddr_in& GetSockAddr() const { return addr_; }
    
    // For non-const access (e.g., getsockname, accept)
    sockaddr_in* GetSockAddrPtr() { return &addr_; }

    socklen_t GetSockLen() const { return sizeof(addr_); }

private:
    sockaddr_in addr_;
};


#ifdef SERVER_PLATFORM_WINDOWS

    // Helper RAII class for WSA initialization
    class WSAInitializer {
    public:
        WSAInitializer() {
            WSADATA wsaData;
            if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
                SERVER_ERROR("WSAStartup failed.");
                // In a real application, you might want to throw or exit here
            }
        }
        ~WSAInitializer() {
            WSACleanup();
        }
    };
    // This static instance ensures WSA is initialized once when the program starts
    // and cleaned up once when it exits.
    static WSAInitializer wsaInitializer;


    TCPChannel::TCPChannel() : socketFd_(INVALID_SOCKET), m_Connect(false) {}

    TCPChannel::TCPChannel(const int& clientSocket, const InetAddress& addr)
        : socketFd_(clientSocket), m_Connect(true) {
        remoteAddr_ = CreateRef<InetAddress>(addr);
    }

    TCPChannel::~TCPChannel() {
        Close();
    }

    bool TCPChannel::Listen(const uint16_t& port) {
        socketFd_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (socketFd_ == INVALID_SOCKET) {
            SERVER_ERROR("Socket creation failed with error: {}", WSAGetLastError());
            return false;
        }

        // Allow socket descriptor to be reusable
        char opt = 1;
        setsockopt(socketFd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

        SOCKADDR_IN addr;
        addr.sin_family = AF_INET;
        addr.sin_addr.S_un.S_addr = htonl(INADDR_ANY);
        addr.sin_port = htons(port);

        if (bind(socketFd_, (SOCKADDR*)&addr, sizeof(addr)) == SOCKET_ERROR) {
            SERVER_ERROR("Bind failed with error: {}", WSAGetLastError());
            closesocket(socketFd_);
            socketFd_ = INVALID_SOCKET;
            return false;
        }

        if (listen(socketFd_, SOMAXCONN) == SOCKET_ERROR) {
            SERVER_ERROR("Listen failed with error: {}", WSAGetLastError());
            closesocket(socketFd_);
            socketFd_ = INVALID_SOCKET;
            return false;
        }

        // Set to non-blocking mode for accept
        u_long mode = 1;
        ioctlsocket(socketFd_, FIONBIO, &mode);
        
        m_Connect = true; // Representing that the listener is active
        localAddr_ = CreateRef<InetAddress>(GetLocalAddress());
        SERVER_INFO("Server listening on port {}", port);
        return true;
    }

    Ref<TCPChannel> TCPChannel::Accept() {
        if (socketFd_ == INVALID_SOCKET || !m_Connect) {
            SERVER_ERROR("Accept called on invalid or non-listening socket");
            return nullptr;
        }

        sockaddr_in clientAddr;
        socklen_t addrLen = sizeof(clientAddr);

        SOCKET clientSocket = accept(socketFd_, reinterpret_cast<sockaddr*>(&clientAddr), &addrLen);

        if (clientSocket == INVALID_SOCKET) {
            int error = WSAGetLastError();
            if (error != WSAEWOULDBLOCK) {
                SERVER_ERROR("Accept failed with error: {}", error);
            }
            // WSAEWOULDBLOCK is expected in non-blocking mode, so we just return nullptr
            return nullptr;
        }

        InetAddress clientAddress(clientAddr);
        SERVER_INFO("Accepted connection from {}:{}", clientAddress.GetIP(), clientAddress.GetPort());

        Ref<TCPChannel> newChannel = CreateRef<TCPChannel>(clientSocket, clientAddress);
        return newChannel;
    }

    void TCPChannel::Close() {
        if (socketFd_ != INVALID_SOCKET) {
            shutdown(socketFd_, SD_BOTH);
            closesocket(socketFd_);
            socketFd_ = INVALID_SOCKET;
        }
        m_Connect = false;
    }
    
    bool TCPChannel::Send(const void* data, const size_t& length)
    {
        if (!m_Connect || socketFd_ == INVALID_SOCKET) return false;

        const char* buffer = static_cast<const char*>(data);
        size_t totalSent = 0;
        while (totalSent < length) {
            int sent = send(socketFd_, buffer + totalSent, length - totalSent, 0);
            if (sent == SOCKET_ERROR) {
                SERVER_ERROR("Send failed with error: {}", WSAGetLastError());
                break;
            }
            totalSent += sent;
        }
        return totalSent == length;
    }

    size_t TCPChannel::Receive(void* buffer, const size_t& size) {
        if (!m_Connect || socketFd_ == INVALID_SOCKET) return 0;
        int received = recv(socketFd_, static_cast<char*>(buffer), size, 0);
        if (received < 0) {
            int error = WSAGetLastError();
            if (error != WSAEWOULDBLOCK) {
                 SERVER_ERROR("Recv failed with error: {}", error);
            }
            return 0;
        }
        return static_cast<size_t>(received);
    }
    
    InetAddress TCPChannel::GetLocalAddress()const {
        InetAddress addr;
        socklen_t len = sizeof(sockaddr_in);
        getsockname(socketFd_, reinterpret_cast<sockaddr*>(addr.GetSockAddrPtr()), &len);
        return addr;
    }

    // Other functions (assuming they are less critical for server or similar to client)
    // ...


#elif defined(SERVER_PLATFORM_MACOSX) // Or any other POSIX system like Linux



    TCPChannel::TCPChannel() : socketFd_(-1), m_Connect(false) {}

    TCPChannel::TCPChannel(const int& clientSocket, const InetAddress& addr)
        : socketFd_(clientSocket), m_Connect(true) {
        remoteAddr_ = CreateRef<InetAddress>(addr);
        // No WSAStartup needed on macOS
    }

    TCPChannel::~TCPChannel() {
        Close();
    }

    bool TCPChannel::Listen(const uint16_t& port) {
        socketFd_ = socket(AF_INET, SOCK_STREAM, 0);
        if (socketFd_ == -1) {
            SERVER_ERROR("Socket creation failed: {}", strerror(errno));
            return false;
        }

        // **Crucial for servers**: Allow reuse of the address to avoid
        // "Address already in use" errors on quick restarts.
        int opt = 1;
        if (setsockopt(socketFd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
            SERVER_ERROR("setsockopt(SO_REUSEADDR) failed: {}", strerror(errno));
            close(socketFd_);
            socketFd_ = -1;
            return false;
        }

        sockaddr_in addr;
        std::memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
        addr.sin_port = htons(port);

        if (bind(socketFd_, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
            SERVER_ERROR("Bind failed: {}", strerror(errno));
            close(socketFd_);
            socketFd_ = -1;
            return false;
        }

        if (listen(socketFd_, SOMAXCONN) == -1) {
            SERVER_ERROR("Listen failed: {}", strerror(errno));
            close(socketFd_);
            socketFd_ = -1;
            return false;
        }

        // Set to non-blocking mode for accept
        int flags = fcntl(socketFd_, F_GETFL, 0);
        fcntl(socketFd_, F_SETFL, flags | O_NONBLOCK);
        
        m_Connect = true; // Representing that the listener is active
        localAddr_ = CreateRef<InetAddress>(GetLocalAddress());
        SERVER_INFO("Server listening on port {}", port);
        return true;
    }

    Ref<TCPChannel> TCPChannel::Accept() {
        if (socketFd_ == -1 || !m_Connect) {
            SERVER_ERROR("Accept called on invalid or non-listening socket");
            return nullptr;
        }

        sockaddr_in clientAddr;
        socklen_t addrLen = sizeof(clientAddr);

        int clientSocket = accept(socketFd_, reinterpret_cast<sockaddr*>(&clientAddr), &addrLen);

        if (clientSocket == -1) {
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                SERVER_ERROR("Accept failed with error: {}", strerror(errno));
            }
            // EAGAIN/EWOULDBLOCK are expected in non-blocking mode, just return nullptr
            return nullptr;
        }

        InetAddress clientAddress(clientAddr);
        SERVER_INFO("Accepted connection from {}:{}", clientAddress.GetIP(), clientAddress.GetPort());

        Ref<TCPChannel> newChannel = CreateRef<TCPChannel>(clientSocket, clientAddress);
        return newChannel;
    }

    void TCPChannel::Close() {
        if (socketFd_ != -1) {
            shutdown(socketFd_, SHUT_RDWR);
            close(socketFd_);
            socketFd_ = -1;
        }
        m_Connect = false;
    }

    bool TCPChannel::Send(const void* data, const size_t& length) {
        if (!m_Connect || socketFd_ == -1) return false;

        const char* buffer = static_cast<const char*>(data);
        size_t totalSent = 0;
        while (totalSent < length) {
            ssize_t sent = send(socketFd_, buffer + totalSent, length - totalSent, 0);
            if (sent <= 0) {
                 if (sent < 0) {
                    SERVER_ERROR("Send failed: {}", strerror(errno));
                }
                break;
            }
            totalSent += sent;
        }
        return totalSent == length;
    }

bool TCPChannel::Send(void* data, const size_t& length, const InetAddress& IPAddress){
    return Send(data, length);
}

    size_t TCPChannel::Receive(void* buffer, const size_t& size) {
        if (!m_Connect || socketFd_ == -1) return 0;
        ssize_t received = recv(socketFd_, static_cast<char*>(buffer), size, 0);
        if (received < 0) {
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                SERVER_ERROR("Recv failed: {}", strerror(errno));
            }
            return 0; // No data received or an error occurred
        }
        return static_cast<size_t>(received);
    }
    
    InetAddress TCPChannel::GetLocalAddress()const {
        InetAddress addr;
        socklen_t len = sizeof(sockaddr_in);
        getsockname(socketFd_, reinterpret_cast<sockaddr*>(addr.GetSockAddrPtr()), &len);
        return addr;
    }
    
    // The following functions are not typically used by a listener, but are here for completeness
    bool TCPChannel::Connect(const std::string& ip, const uint16_t& port){
        // Implementation would be same as client-side macOS version
        SERVER_ERROR("Connect() not implemented for this server-side channel");
        return false;
    }

    int TCPChannel::ReceiveNonBlocking(void* buffer, size_t length){
        // The socket should already be in non-blocking mode from Listen() or Accept()
        // so this is equivalent to Receive
        return Receive(buffer, length);
    }

#else
    TSO_ASSERT(false, "NetworkAPI for other platform is not implemented yet")
#endif
}
