#include "SPch.h"
#include "NetworkEngine.h"
#include "NetworkAPI.h"
#include "data/ByteStream.h"
#include <thread>
#include <mutex>
#include <unordered_set>

namespace Tso {
    static NetWorkEngine* s_NetworkEngine = nullptr;

    // �ͻ���������Ϣ�ṹ
    

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

        // ��������socket
        if (!s_NetworkEngine->m_ListenerChannel->Listen(port)) {
            SERVER_ERROR("Failed to start server on port {}", port);
            return false;
        }

        SERVER_INFO("Server started on port {}", port);

        // ���������߳�
        s_NetworkEngine->m_Listening = true;
        s_NetworkEngine->m_ListenerThread = std::thread(&NetWorkEngine::AcceptConnections, s_NetworkEngine);

        return true;
    }

    void NetWorkEngine::StopServer()
    {
        s_NetworkEngine->m_Listening = false;

        // �رռ���socket
        if (m_ListenerChannel) {
            m_ListenerChannel->Close();
        }

        // �ȴ������߳̽���
        if (m_ListenerThread.joinable()) {
            m_ListenerThread.join();
        }

        // �ر����пͻ�������
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
            // �ȴ��¿ͻ�������
            Ref<TCPChannel> newChannel = m_ListenerChannel->Accept();

            if (newChannel) {
                uint32_t clientId = m_NextClientId++;

                // �����ͻ�������
                auto client = std::make_shared<ClientConnection>();
                client->channel = newChannel;
                client->clientId = clientId;

                // ��ӵ��ͻ����б�
                {
                    std::lock_guard<std::mutex> lock(m_ClientMutex);
                    m_Clients[clientId] = client;
                }

                //SERVER_INFO("Client {} connected from {}", clientId, client->channel->GetRemoteAddress());

                // ���������߳�
                client->recvThread = std::thread(&NetWorkEngine::HandleClient, this, client);
            }
            else {
                // �������߱���CPUռ�ù���
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
            // ������ʱ���
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
            else if (received == 0) {  // �ͻ��������ر�����[1,8](@ref)
                SERVER_INFO("Client {} disconnected normally", client->clientId);
                client->active = false;
                m_RemoveClients.push(client->clientId);
                break;
            }
            else if (received == -1) {
                

                if (current_errno == EWOULDBLOCK || current_errno == EAGAIN) {
                    // ���������Է��Ϳ����ݼ������״̬[9](@ref)

                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    continue;
                }
                else if (current_errno == ECONNRESET) {  // �ͻ����쳣�Ͽ�[8](@ref)
                    SERVER_INFO("Client {} connection reset", client->clientId);
                    client->active = false;
                    m_RemoveClients.push(client->clientId);
                    break;
                }
                // ���������������ؼ�������[1,9](@ref)
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

        // ȷ����Դ�����ͷ�
        if (client->channel) {
            client->channel->Close();
        }
    }

    void NetWorkEngine::ProcessClientBuffer(std::shared_ptr<ClientConnection> client, std::vector<uint8_t>& buffer)
    {
        while (buffer.size() >= ByteStream::HEADER_SIZE) {
            // ����ByteStream��������������ֻ��鳤��
            ByteStream byte(buffer);
            uint8_t protocol = byte.read<uint8_t>(); // ��ȡprotocol
            size_t dataLength = byte.read<size_t>();   // ��ȡdataLength
            /*uint8_t protocol = *reinterpret_cast<uint8_t*>(buffer.data());
            size_t dataLength = *reinterpret_cast<size_t*>(buffer.data() + sizeof(uint8_t));*/
            const size_t packetSize = dataLength;

            if (buffer.size() < packetSize) {
                // ���ݲ��㣬�ȴ���������
                SERVER_TRACE("get {}/{} data , waiting next pack", buffer.size(), packetSize);
                break;
            }

            // ����ByteStream������
            

            // �����ݰ��������
            SERVER_TRACE("Received packet: protocol={}, length={}", protocol, dataLength);
            m_PacketQueue.enqueue({protocol, byte });

            // �Ƴ��Ѵ�������
            buffer.erase(buffer.begin(), buffer.begin() + packetSize);
        }
    }

    void NetWorkEngine::RemoveClient(uint32_t clientId)
    {
        std::lock_guard<std::mutex> lock(m_ClientMutex);
        auto it = m_Clients.find(clientId);
        if (it != m_Clients.end()) {
            // ȷ���߳��ѽ���
            if (it->second->recvThread.joinable()) {
                it->second->recvThread.join();
            }
            m_Clients.erase(it);
            SERVER_INFO("Client {} removed", clientId);
        }
    }

    void NetWorkEngine::OnUpdate(TimeStep ts)
    {
        // ÿ֡��ദ��50������ֹ����
        constexpr int MAX_PACKETS_PER_FRAME = 50;
        int processed = 0;

        // ������ն���
        std::pair<uint8_t, ByteStream> packet;
        while (processed++ < MAX_PACKETS_PER_FRAME &&
            s_NetworkEngine->m_PacketQueue.try_dequeue(packet)) {
            // deal meesage protocol
            //TODO: NetworkEngine does systematical issues instead of broadcasting every time;
            // �����ص�
            //ParseCallback(packet.byte);

            // ����ע��Ľ��պ���
            if (s_NetworkEngine->m_RecvFuncs.find(packet.first) != s_NetworkEngine->m_RecvFuncs.end()) {
                s_NetworkEngine->m_RecvFuncs[packet.first](packet.second);
            }
            else {
                SERVER_ERROR("Received unknown protocol message [{}], dropped it",packet.first);
            }

            // �㲥�����пͻ���
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

    // ... �����������ֲ��� ...

    NetWorkEngine::~NetWorkEngine()
    {
        StopServer();
    }
}