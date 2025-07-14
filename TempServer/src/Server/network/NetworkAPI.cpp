#include "SPch.h"
#include "NetworkAPI.h"
#include <string>

namespace Tso {
#ifdef SERVER_PLATFORM_WINDOWS
#include <winsock2.h>
#include<ws2tcpip.h>
#define ECONNRESET WSAECONNRESET

    struct InetAddress {

        InetAddress(const std::string& ip, const uint16_t& port) {
            std::memset(&addr_, 0, sizeof(addr_));
            addr_.sin_family = AF_INET;
            addr_.sin_port = htons(port);
            inet_pton(AF_INET, ip.c_str(), &addr_.sin_addr);
        }
        InetAddress() {
            std::memset(&addr_, 0, sizeof(addr_));
        }
        InetAddress(sockaddr_in& addr):addr_(addr) {
            
        }

        std::string GetIP() const {
            char buffer[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &addr_.sin_addr, buffer, INET_ADDRSTRLEN);
            return std::string(buffer);
        }
        uint16_t GetPort() const { return ntohs(addr_.sin_port); }
        sockaddr_in GetSockAddr() const { return addr_; }
        socklen_t GetSockLen() const { return sizeof(addr_); }

        //bool operator==(const InetAddress& other);

    private:
        sockaddr_in addr_;
    };

    TCPChannel::TCPChannel() : socketFd_(-1), m_Connect(false) {
        WORD myVersionRequest = MAKEWORD(2, 2);
        WSADATA wsaData;
        if (WSAStartup(myVersionRequest, &wsaData) != 0)
        {
            SERVER_ERROR("Unable to open socket\n");
            SERVER_ASSERT(false , "")
        }
    }

    TCPChannel::TCPChannel(const int& clientSocket, const InetAddress& addr):socketFd_(clientSocket)
    {
        remoteAddr_ = CreateRef<InetAddress>(addr);
        WORD myVersionRequest = MAKEWORD(2, 2);
        WSADATA wsaData;
        if (WSAStartup(myVersionRequest, &wsaData) != 0)
        {
            SERVER_ERROR("Unable to open socket\n");
            SERVER_ASSERT(false, "")
        }
        m_Connect = true;
    }

    TCPChannel::~TCPChannel()
    {
        Close();
    }

    bool TCPChannel::Connect(const InetAddress& serverAddress)
    {
        socketFd_ = socket(AF_INET, SOCK_STREAM, 0);
        if (socketFd_ == -1) return false;

        // 设置非阻塞模式（可选）
        u_long mode = 1;
        ioctlsocket(socketFd_, FIONBIO, &mode);

        if (connect(socketFd_, reinterpret_cast<sockaddr*>(&serverAddress.GetSockAddr()), serverAddress.GetSockLen()) != 0) {
            // 如果连接失败且非阻塞，等待连接完成
            fd_set writeSet;
            FD_ZERO(&writeSet);
            FD_SET(socketFd_, &writeSet);

            timeval timeout = { 5, 0 }; // 5秒超时
            int result = select(socketFd_ + 1, nullptr, &writeSet, nullptr, &timeout);
            if (result <= 0) {
                close(socketFd_);
                socketFd_ = -1;
                return false;
            }
        }

        m_Connect = true;
        remoteAddr_ = std::make_shared<InetAddress>(serverAddress);
        localAddr_ = std::make_shared<InetAddress>(GetLocalAddress());
        return true;
    }

    bool TCPChannel::Connect(const std::string& ip, const uint16_t& port)
    {
        InetAddress serverAddres(ip, port);
        return Connect(serverAddres);
    }

    bool TCPChannel::Listen(const uint16_t& port)
    {
        socketFd_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (socketFd_ == INVALID_SOCKET)
        {
            SERVER_ERROR("Socket creation failed with error: {}", WSAGetLastError());
            WSACleanup();
            return false;
        }

        SOCKADDR_IN addr;
        addr.sin_family = AF_INET;
        addr.sin_addr.S_un.S_addr = htonl(INADDR_ANY);
        addr.sin_port = htons(port);

        if (bind(socketFd_, (SOCKADDR*)&addr, sizeof(addr)) == SOCKET_ERROR)
        {
            SERVER_ERROR("Bind failed with error: {}", WSAGetLastError());
            closesocket(socketFd_);
            WSACleanup();
            return false;
        }

        if (listen(socketFd_, SOMAXCONN) == SOCKET_ERROR)
        {
            SERVER_ERROR("Listen failed with error: {}", WSAGetLastError());
            closesocket(socketFd_);
            WSACleanup();
            return false;
        }
        return true;
    }

    Ref<TCPChannel> TCPChannel::Accept()
    {
        if (socketFd_ == INVALID_SOCKET) {
            SERVER_ERROR("Accept called on invalid listening socket");
            return nullptr;
        }

        sockaddr_in clientAddr;
        memset(&clientAddr, 0, sizeof(clientAddr));  // 确保清零
        socklen_t addrLen = sizeof(clientAddr);     // 使用正确的socklen_t类型

        SOCKET clientSocket = accept(socketFd_,
            reinterpret_cast<sockaddr*>(&clientAddr),
            &addrLen);

        // 处理可能的错误
        if (clientSocket == INVALID_SOCKET) {
            const int error = WSAGetLastError();

            // 分类处理不同错误
            if (error == WSAEWOULDBLOCK) {
                // 非阻塞模式下的正常情况
                SERVER_TRACE("No pending connections");
            }
            else if (error == WSAECONNRESET) {
                // 客户端在连接建立后立即断开
                SERVER_WARN("Connection reset during accept");
            }
            else if (error == WSAEMFILE) {
                // 系统资源耗尽
                SERVER_ERROR("Too many open files/sockets");
            }
            else {
                // 其他严重错误
                SERVER_ERROR("Accept failed: error={}", error);
            }

            return nullptr;
        }

        // 检查实际获得的地址长度
        if (addrLen == 0) {
            SERVER_WARN("Accepted connection but received no address information");
            closesocket(clientSocket);
            return nullptr;
        }

        try {
            InetAddress clientAddress(clientAddr);
            SERVER_INFO("Accepted connection from {}:{}",
                clientAddress.GetIP(),
                clientAddress.GetPort());

            Ref<TCPChannel> newChannel = CreateRef<TCPChannel>(clientSocket, clientAddress);
            return newChannel;
        }
        catch (const std::exception& e) {
            SERVER_ERROR("Exception creating TCPChannel: {}", e.what());
            closesocket(clientSocket);
            return nullptr;
        }
    }

    InetAddress TCPChannel::GetLocalAddress()const {
        InetAddress addr;
        socklen_t len = sizeof(addr.GetSockAddr());
        getsockname(socketFd_, reinterpret_cast<sockaddr*>(&addr.GetSockAddr()), &len);
        return addr;
    }

    int TCPChannel::ReceiveNonBlocking(void* buffer, size_t length)
    {
        // 设置非阻塞模式
        //fcntl(socketFd_, F_SETFL, O_NONBLOCK); // Linux/macOS
        u_long mode = 1;
        ioctlsocket(socketFd_, FIONBIO, &mode); // Windows

        int received = recv(socketFd_, static_cast<char*>(buffer), length, 0);
        SERVER_TRACE("get recv = {} in API", received);
        if (received == -1 && errno == EAGAIN) {
            return 0; // 无数据可用
        }
        return received;
    }

    bool TCPChannel::Send(void* data, const size_t& length, const InetAddress& IPAddress) {
        return Send(data, length, *remoteAddr_);
    }

    bool TCPChannel::Send(const void* data, const size_t& length)
    {
        if (!m_Connect || socketFd_ == -1) return false;


        const char* buffer = static_cast<const char*>(data);
        SERVER_INFO("send {} byte", length);
        size_t totalSent = 0;
        while (totalSent < length) {
            size_t sent = send(socketFd_, buffer + totalSent, length - totalSent, 0);
            if (sent <= 0) break;
            totalSent += sent;
        }
        return totalSent == length;
    }

    size_t TCPChannel::Receive(void* buffer, const size_t& size) {
        if (!m_Connect || socketFd_ == -1) return 0;
        size_t received = recv(socketFd_, static_cast<char*>(buffer), size, 0);
        return received > 0 ? static_cast<size_t>(received) : 0;
    }

    void TCPChannel::Close() {
        if (socketFd_ != -1) {
            //shutdown(socketFd_, SD_BOTH);
            closesocket(socketFd_);
            //关闭服务
            WSACleanup();
            socketFd_ = -1;
        }
        m_Connect = false;
    }


#else
    TSO_ASSERT(false, "NetworkAPI for other platform is not implemented yet")
#endif
}