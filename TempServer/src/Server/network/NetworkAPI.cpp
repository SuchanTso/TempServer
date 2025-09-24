#include "SPch.h"
#include "NetworkAPI.h"
#include <string>
#include <cstring> // For std::memset

// --- Platform-specific includes ---
#ifdef SERVER_PLATFORM_WINDOWS
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
#elif defined(SERVER_PLATFORM_MACOSX) || defined(SERVER_PLATFORM_LINUX) // [修改]
    // POSIX-compliant platforms (macOS, Linux, etc.) share these headers
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>     // For close()
    #include <fcntl.h>      // For fcntl() and O_NONBLOCK
    #include <cerrno>       // For errno
#else
    // It's good practice to have a fallback for unsupported platforms
    #error "Unsupported platform for NetworkAPI"
#endif


namespace Tso {

// --- Common InetAddress implementation ---
// This part is platform-independent and remains unchanged.
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

    InetAddress(const sockaddr_in& addr) : addr_(addr) {}

    std::string GetIP() const {
        char buffer[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &addr_.sin_addr, buffer, INET_ADDRSTRLEN);
        return std::string(buffer);
    }

    uint16_t GetPort() const { return ntohs(addr_.sin_port); }

    const sockaddr_in& GetSockAddr() const { return addr_; }
    
    sockaddr_in* GetSockAddrPtr() { return &addr_; }

    socklen_t GetSockLen() const { return sizeof(addr_); }

private:
    sockaddr_in addr_;
};


// --- Windows Implementation ---
#ifdef SERVER_PLATFORM_WINDOWS

    class WSAInitializer {
    public:
        WSAInitializer() {
            WSADATA wsaData;
            if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
                SERVER_ERROR("WSAStartup failed.");
            }
        }
        ~WSAInitializer() {
            WSACleanup();
        }
    };
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

        u_long mode = 1;
        ioctlsocket(socketFd_, FIONBIO, &mode);
        
        m_Connect = true;
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
    
    bool TCPChannel::Send(const void* data, const size_t& length) {
        if (!m_Connect || socketFd_ == INVALID_SOCKET) return false;
        const char* buffer = static_cast<const char*>(data);
        size_t totalSent = 0;
        while (totalSent < length) {
            int sent = send(socketFd_, buffer + totalSent, static_cast<int>(length - totalSent), 0);
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
        int received = recv(socketFd_, static_cast<char*>(buffer), static_cast<int>(size), 0);
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

    // Unused methods for server...
    bool TCPChannel::Send(void* data, const size_t& length, const InetAddress& IPAddress) { return Send(data, length); }
    bool TCPChannel::Connect(const std::string& ip, const uint16_t& port) { return false; }
    int TCPChannel::ReceiveNonBlocking(void* buffer, size_t length) { return static_cast<int>(Receive(buffer, length)); }


// --- POSIX Implementation (macOS & Linux) ---
#elif defined(SERVER_PLATFORM_MACOSX) || defined(SERVER_PLATFORM_LINUX) // [修改]

    TCPChannel::TCPChannel() : socketFd_(-1), m_Connect(false) {}

    TCPChannel::TCPChannel(const int& clientSocket, const InetAddress& addr)
        : socketFd_(clientSocket), m_Connect(true) {
        remoteAddr_ = CreateRef<InetAddress>(addr);
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
        if (flags == -1) {
            SERVER_ERROR("fcntl(F_GETFL) failed: {}", strerror(errno));
            close(socketFd_);
            socketFd_ = -1;
            return false;
        }
        if (fcntl(socketFd_, F_SETFL, flags | O_NONBLOCK) == -1) {
            SERVER_ERROR("fcntl(F_SETFL) failed: {}", strerror(errno));
            close(socketFd_);
            socketFd_ = -1;
            return false;
        }
        
        m_Connect = true;
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
                SERVER_ERROR("Accept failed: {}", strerror(errno));
            }
            return nullptr;
        }

        // It's good practice to also set the new client socket to non-blocking
        int flags = fcntl(clientSocket, F_GETFL, 0);
        fcntl(clientSocket, F_SETFL, flags | O_NONBLOCK);

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
            // MSG_NOSIGNAL is a Linux-specific flag that prevents the program
            // from crashing with a SIGPIPE signal if the other end closes the connection.
            // It's a no-op on macOS but good practice for Linux compatibility.
            #ifdef SERVER_PLATFORM_LINUX
                ssize_t sent = send(socketFd_, buffer + totalSent, length - totalSent, MSG_NOSIGNAL);
            #else
                ssize_t sent = send(socketFd_, buffer + totalSent, length - totalSent, 0);
            #endif
            
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

    size_t TCPChannel::Receive(void* buffer, const size_t& size) {
        if (!m_Connect || socketFd_ == -1) return 0;
        ssize_t received = recv(socketFd_, static_cast<char*>(buffer), size, 0);
        if (received < 0) {
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                SERVER_ERROR("Recv failed: {}", strerror(errno));
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
    
    // Unused methods for server...
    bool TCPChannel::Send(void* data, const size_t& length, const InetAddress& IPAddress) { return Send(data, length); }
    bool TCPChannel::Connect(const std::string& ip, const uint16_t& port) { return false; }
    int TCPChannel::ReceiveNonBlocking(void* buffer, size_t length) { return static_cast<int>(Receive(buffer, length)); }

#endif // End of platform-specific implementations
}
