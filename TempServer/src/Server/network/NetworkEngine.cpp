#include "SPch.h"
#include "NetworkEngine.h"
#include "NetworkAPI.h"
#include "data/ByteStream.h"
#include <thread>
#include <mutex>
#include <unordered_set>

namespace Tso {
    static NetWorkEngine* s_NetworkEngine = nullptr;

    // 客户端连接信息结构
    

    NetWorkEngine::NetWorkEngine()
        : m_NextClientId(1)
    {
        m_ListenerChannel = CreateRef<TCPChannel>();
    }

    void NetWorkEngine::Init()
    {
        SERVER_ASSERT(s_NetworkEngine == nullptr, "Init NetworkEngine more than once!");
        s_NetworkEngine = new NetWorkEngine();
    }

    void NetWorkEngine::Shutdown()
    {
        if (s_NetworkEngine) {
            s_NetworkEngine->StopServer();
            delete s_NetworkEngine;
            s_NetworkEngine = nullptr;
        }
    }

    bool NetWorkEngine::StartServer(const uint16_t& port)
    {
        SERVER_ASSERT(s_NetworkEngine != nullptr, "NetworkEngine not initialized");

        // 创建监听socket
        if (!s_NetworkEngine->m_ListenerChannel->Listen(port)) {
            SERVER_ERROR("Failed to start server on port {}", port);
            return false;
        }

        SERVER_INFO("Server started on port {}", port);

        // 启动监听线程
        s_NetworkEngine->m_Listening = true;
        s_NetworkEngine->m_ListenerThread = std::thread(&NetWorkEngine::AcceptConnections, s_NetworkEngine);

        return true;
    }

    void NetWorkEngine::StopServer()
    {
        s_NetworkEngine->m_Listening = false;

        // 关闭监听socket
        if (m_ListenerChannel) {
            m_ListenerChannel->Close();
        }

        // 等待监听线程结束
        if (m_ListenerThread.joinable()) {
            m_ListenerThread.join();
        }

        // 关闭所有客户端连接
        std::lock_guard<std::mutex> lock(m_ClientMutex);
        for (auto& [id, client] : m_Clients) {
            client->active = false;
            if (client->channel) {
                client->channel->Close();
            }
            if (client->recvThread.joinable()) {
                client->recvThread.join();
            }
        }
        m_Clients.clear();
    }

    void NetWorkEngine::AcceptConnections()
    {
        while (m_Listening) {
            // 等待新客户端连接
            Ref<TCPChannel> newChannel = m_ListenerChannel->Accept();

            if (newChannel) {
                uint32_t clientId = m_NextClientId++;

                // 创建客户端连接
                auto client = std::make_shared<ClientConnection>();
                client->channel = newChannel;
                client->clientId = clientId;

                // 添加到客户端列表
                {
                    std::lock_guard<std::mutex> lock(m_ClientMutex);
                    m_Clients[clientId] = client;
                }

                //SERVER_INFO("Client {} connected from {}", clientId, client->channel->GetRemoteAddress());

                // 启动接收线程
                client->recvThread = std::thread(&NetWorkEngine::HandleClient, this, client);
            }
            else {
                // 短暂休眠避免CPU占用过高
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }
    }

    void NetWorkEngine::HandleClient(std::shared_ptr<ClientConnection> client)
    {
        constexpr size_t CHUNK_SIZE = 4096;
        std::vector<uint8_t> tempBuffer;
        client->lastActiveTime = std::chrono::steady_clock::now();

        while (client->active) {
            // 心跳超时检测
            auto now = std::chrono::steady_clock::now();
            if (std::chrono::duration_cast<std::chrono::seconds>(now - client->lastActiveTime) > std::chrono::seconds(30)) {
                SERVER_INFO("Client {} heartbeat timeout", client->clientId);
                client->active = false;
                m_RemoveClients.push(client->clientId);
                break;
            }

            char chunk[CHUNK_SIZE];
            int received = client->channel->ReceiveNonBlocking(chunk, CHUNK_SIZE);
            int current_errno = errno;
            //SERVER_INFO("get recv = {} , err = {}", received, current_errno);
            if (received > 0) {
                client->lastActiveTime = now;
                tempBuffer.insert(tempBuffer.end(), chunk, chunk + received);
                ProcessClientBuffer(client, tempBuffer);
            }
            else if (received == 0) {  // 客户端正常关闭连接[1,8](@ref)
                SERVER_INFO("Client {} disconnected normally", client->clientId);
                client->active = false;
                m_RemoveClients.push(client->clientId);
                break;
            }
            else if (received == -1) {
                

                if (current_errno == EWOULDBLOCK || current_errno == EAGAIN) {
                    // 新增：尝试发送空数据检测连接状态[9](@ref)

                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    continue;
                }
                else if (current_errno == ECONNRESET) {  // 客户端异常断开[8](@ref)
                    SERVER_INFO("Client {} connection reset", client->clientId);
                    client->active = false;
                    m_RemoveClients.push(client->clientId);
                    break;
                }
                // 新增：处理其他关键错误码[1,9](@ref)
                else if (current_errno == ENOTCONN || current_errno == EPIPE) {
                    SERVER_INFO("Client {} not connected (errno: {})", client->clientId, strerror(current_errno));
                    client->active = false;
                    m_RemoveClients.push(client->clientId);
                    break;
                }
                else if (current_errno == 0) {
                    continue;
                }
                else {
                    SERVER_ERROR("Critical error for client {}: {}", client->clientId, strerror(current_errno));
                    m_RemoveClients.push(client->clientId);
                    client->active = false;
                    break;
                }
            }
        }

        // 确保资源立即释放
        if (client->channel) {
            client->channel->Close();
        }
    }

    void NetWorkEngine::ProcessClientBuffer(std::shared_ptr<ClientConnection> client, std::vector<uint8_t>& buffer)
    {
        while (buffer.size() >= ByteStream::HEADER_SIZE) {
            // 创建ByteStream但不立即解析，只检查长度
            ByteStream byte(buffer);
            uint8_t protocol = byte.read<uint8_t>(); // 读取protocol
            size_t dataLength = byte.read<size_t>();   // 读取dataLength
            /*uint8_t protocol = *reinterpret_cast<uint8_t*>(buffer.data());
            size_t dataLength = *reinterpret_cast<size_t*>(buffer.data() + sizeof(uint8_t));*/
            const size_t packetSize = dataLength;

            if (buffer.size() < packetSize) {
                // 数据不足，等待更多数据
                SERVER_TRACE("get {}/{} data , waiting next pack", buffer.size(), packetSize);
                break;
            }

            // 创建ByteStream并解析
            

            // 将数据包加入队列
            SERVER_TRACE("Received packet: protocol={}, length={}", protocol, dataLength);
            m_PacketQueue.enqueue({protocol, byte });

            // 移除已处理数据
            buffer.erase(buffer.begin(), buffer.begin() + packetSize);
        }
    }

    void NetWorkEngine::RemoveClient(uint32_t clientId)
    {
        std::lock_guard<std::mutex> lock(m_ClientMutex);
        auto it = m_Clients.find(clientId);
        if (it != m_Clients.end()) {
            // 确保线程已结束
            if (it->second->recvThread.joinable()) {
                it->second->recvThread.join();
            }
            m_Clients.erase(it);
            SERVER_INFO("Client {} removed", clientId);
        }
    }

    void NetWorkEngine::OnUpdate(TimeStep ts)
    {
        // 每帧最多处理50个包防止卡顿
        constexpr int MAX_PACKETS_PER_FRAME = 50;
        int processed = 0;

        // 处理接收队列
        std::pair<uint8_t, ByteStream> packet;
        while (processed++ < MAX_PACKETS_PER_FRAME &&
            s_NetworkEngine->m_PacketQueue.try_dequeue(packet)) {
            // deal meesage protocol
            //TODO: NetworkEngine does systematical issues instead of broadcasting every time;
            // 解析回调
            //ParseCallback(packet.byte);

            // 调用注册的接收函数
            if (s_NetworkEngine->m_RecvFuncs.find(packet.first) != s_NetworkEngine->m_RecvFuncs.end()) {
                s_NetworkEngine->m_RecvFuncs[packet.first](packet.second);
            }
            else {
                SERVER_ERROR("Received unknown protocol message [{}], dropped it",packet.first);
            }

            // 广播给所有客户端
            s_NetworkEngine->Broadcast(packet.first , packet.second , true);
        }

        while (!s_NetworkEngine->m_RemoveClients.empty()) {
            uint32_t clientID = s_NetworkEngine->m_RemoveClients.front();
            s_NetworkEngine->RemoveClient(clientID);
            s_NetworkEngine->m_RemoveClients.pop();
        }

        
    }

    bool NetWorkEngine::SendToClient(uint32_t clientId, const uint8_t& protocolID, const ByteStream& byte, bool Reliable)
    {
        SERVER_ASSERT(s_NetworkEngine != nullptr, "NetworkEngine not initialized");

        if (m_WrapFuncs.find(protocolID) == m_WrapFuncs.end()) {
            SERVER_ERROR("Protocol [{}] not registered", protocolID);
            return false;
        }

        ByteStream wrappedByte = m_WrapFuncs[protocolID](byte);

        std::lock_guard<std::mutex> lock(m_ClientMutex);
        auto it = m_Clients.find(clientId);
        if (it == m_Clients.end() || !it->second->active) {
            SERVER_ERROR("Client {} not found or disconnected", clientId);
            return false;
        }

        size_t dataLength = wrappedByte.getRawBufferLength();
        return it->second->channel->Send(wrappedByte.getRawBuffer(), dataLength);
    }

    bool NetWorkEngine::Broadcast(const uint8_t& protocolID, const ByteStream& byte, bool Reliable=true)
    {
        SERVER_ASSERT(s_NetworkEngine != nullptr, "NetworkEngine not initialized");

        if (m_WrapFuncs.find(protocolID) == m_WrapFuncs.end()) {
            SERVER_ERROR("Protocol [{}] not registered", protocolID);
            //return false;
        }

        ByteStream wrappedByte = byte;//m_WrapFuncs[protocolID](byte);
        size_t dataLength = wrappedByte.getRawBufferLength();
        if (dataLength > 100) {
            int a = 0;
        }
        //const uint8_t* data = wrappedByte.getRawBuffer();

        std::lock_guard<std::mutex> lock(m_ClientMutex);
        bool allSuccess = true;

        for (auto& [id, client] : m_Clients) {
            if (client->active) {
                if (!client->channel->Send(wrappedByte.getRawBuffer(), dataLength)) {
                    SERVER_ERROR("Failed to send to client {}", id);
                    allSuccess = false;
                }
            }
        }

        return allSuccess;
    }

    // ... 其他函数保持不变 ...

    NetWorkEngine::~NetWorkEngine()
    {
        StopServer();
    }
}