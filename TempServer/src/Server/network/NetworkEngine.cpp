#include "SPch.h"
#include "NetworkEngine.h"
#include "NetworkAPI.h"
#include "data/ByteStream.h"
#include <thread>
#include <mutex>
#include <unordered_set>
#include "Server/core/SystemModule.h"

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

        // ���������߳�s
        s_NetworkEngine->m_Listening = true;
        s_NetworkEngine->m_ListenerThread = std::thread(&NetWorkEngine::AcceptConnections, s_NetworkEngine);
        auto rawRecv = [&](ByteStream& byte) ->void {
            s_NetworkEngine->Broadcast(0, byte, true);
            };
        auto heartBeatRecv = [&](ByteStream& byte) ->void {};
        s_NetworkEngine->RegistryRecvFunction(0 , rawRecv);
        s_NetworkEngine->RegistryRecvFunction(255 , heartBeatRecv);
        
        auto rawSend = [](const ByteStream& byte) ->ByteStream {
            return byte;
            };
        s_NetworkEngine->RegistryProtocol(0, rawSend);

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

    NetWorkEngine* NetWorkEngine::GetEngine(){
        return s_NetworkEngine;
    }

    void NetWorkEngine::RegistryRecvFunction(const uint8_t& funcID, const std::function<void(ByteStream&)>& recvFunc){
        if(m_RecvFuncs.find(funcID) != m_RecvFuncs.end()){
            SERVER_WARN("Already registed {} in recv function, dropped it" , funcID);
            return;
        }
        else{
            m_RecvFuncs[funcID] = recvFunc;
        }
    }

    void NetWorkEngine::RegistryProtocol(const uint8_t& protocolID, const std::function<ByteStream(const ByteStream&)>& wrapFunc){
        if(m_WrapFuncs.find(protocolID) != m_WrapFuncs.end()){
            SERVER_WARN("Already registed {} in recv function, dropped it" , protocolID);
            return;
        }
        else{
            m_WrapFuncs[protocolID] = wrapFunc;
        }
    }

    void  NetWorkEngine::SetAppPacketHandler(const AppPacketHandler& handler){
        SERVER_ASSERT(s_NetworkEngine != nullptr , "Init networkEngine first please");
        s_NetworkEngine->SetAppHandle(handler);
    }

    void NetWorkEngine::SetAppHandle(const AppPacketHandler& handler){
        m_AppPacketHandler = handler;
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
//            if (std::chrono::duration_cast<std::chrono::seconds>(now - client->lastActiveTime) > std::chrono::seconds(30)) {
//                SERVER_INFO("Client {} heartbeat timeout", client->clientId);
//                client->active = false;
//                m_RemoveClients.push(client->clientId);
//                break;
//            }

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
//                SERVER_INFO("Client {} disconnected normally", client->clientId);
//                client->active = false;
//                m_RemoveClients.push(client->clientId);
//                break;
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

    void NetWorkEngine::HandleSystemProtocol(Packet& packet) {
        // 0x0001
        uint8_t commandId = packet.protocolId & 0xFF;
        SERVER_INFO("handling system protocol , id :{} , moduleID :{}" , packet.protocolId , commandId);
        if (commandId == Modules::System::C2S_HeartBeat) { // C2S_Heartbeat
             SERVER_TRACE("Received heartbeat from client {}", packet.clientId);
            ByteStream emptyPayload;
            SendToClient(packet.clientId, Modules::System::S2C_HeartBeat, emptyPayload , true); // S2C_Heartbeat
        }
        else if(commandId == Modules::System::C2S_LoginReq){
            SERVER_TRACE("Received login requst from client {}", packet.clientId);
            s_NetworkEngine->m_AppPacketHandler(packet.clientId, packet.protocolId, packet.payload);
        }
    }

//    void NetWorkEngine::ProcessClientBuffer(std::shared_ptr<ClientConnection> client, std::vector<uint8_t>& buffer)
//    {
//        while (buffer.size() >= ByteStream::HEADER_SIZE) {
//            // ����ByteStream��������������ֻ��鳤��
//            ByteStream byte(buffer);
//            uint8_t protocol = byte.read<uint8_t>(); // ��ȡprotocol
//            size_t dataLength = byte.read<size_t>();   // ��ȡdataLength
//            /*uint8_t protocol = *reinterpret_cast<uint8_t*>(buffer.data());
//            size_t dataLength = *reinterpret_cast<size_t*>(buffer.data() + sizeof(uint8_t));*/
//            const size_t packetSize = dataLength;
//
//            if (buffer.size() < packetSize) {
//                // ���ݲ��㣬�ȴ���������
//                SERVER_TRACE("get {}/{} data , waiting next pack", buffer.size(), packetSize);
//                break;
//            }
//
//            // ����ByteStream������
//            
//
//            // �����ݰ��������
//            SERVER_TRACE("Received packet: protocol={}, length={}", protocol, dataLength);
//            m_PacketQueue.enqueue({protocol, byte });
//
//            // �Ƴ��Ѵ�������
//            buffer.erase(buffer.begin(), buffer.begin() + packetSize);
//        }
//    }

    // ProcessClientBuffer:[totalLength(4bytes)][protocolID(2bytes)][Payload]
        void NetWorkEngine::ProcessClientBuffer(std::shared_ptr<ClientConnection> client, std::vector<uint8_t>& buffer) {
            
            while (buffer.size() >= sizeof(uint32_t)) {

                uint32_t totalPacketSize;
                memcpy(&totalPacketSize, buffer.data(), sizeof(uint32_t));

                
                if (buffer.size() < totalPacketSize) {
                    
                    SERVER_WARN("incomplete data {} / {}", buffer.size(), totalPacketSize);
                    break;
                }


                const size_t headerSize = sizeof(uint32_t) + sizeof(uint16_t);
                if (totalPacketSize < headerSize) {
                    
                    SERVER_ERROR("Invalid packet size: {} is smaller than header size {}", totalPacketSize, headerSize);
                    
                    buffer.clear();
                    break;
                }

                const uint8_t* payloadData = buffer.data() + headerSize;
                size_t payloadSize = totalPacketSize - headerSize;
                ByteStream payload(payloadData, payloadSize);

                
                uint16_t protocolId;
                memcpy(&protocolId, buffer.data() + sizeof(uint32_t), sizeof(uint16_t));
                
                Packet packet{client->clientId, protocolId, payload};

                
                SERVER_INFO("Received protocol {} from client {}", protocolId, client->clientId);
                m_PacketQueue.enqueue(packet);

                
                buffer.erase(buffer.begin(), buffer.begin() + totalPacketSize);
            }
        }

    void NetWorkEngine::RemoveClient(uint32_t clientId)
    {
        if (m_OnClientDisconnectedHandler) {
            m_OnClientDisconnectedHandler(clientId);
        }
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

void NetWorkEngine::SetOnClientDisconnected(const std::function<void (uint32_t)> &handler){
    m_OnClientDisconnectedHandler = handler;
}

//    void NetWorkEngine::OnUpdate(TimeStep ts)
//    {
//        // ÿ֡��ദ��50������ֹ����
//        constexpr int MAX_PACKETS_PER_FRAME = 50;
//        int processed = 0;
//
//        // ������ն���
//        std::pair<uint8_t, ByteStream> packet;
//        while (processed++ < MAX_PACKETS_PER_FRAME &&
//            s_NetworkEngine->m_PacketQueue.try_dequeue(packet)) {
//            // deal meesage protocol
//            //TODO: NetworkEngine does systematical issues instead of broadcasting every time;
//            // �����ص�
//            //ParseCallback(packet.byte);
//
//            // ����ע��Ľ��պ���
//            if (s_NetworkEngine->m_RecvFuncs.find(packet.first) != s_NetworkEngine->m_RecvFuncs.end()) {
//                s_NetworkEngine->m_RecvFuncs[packet.first](packet.second);
//            }
//            else {
//                SERVER_ERROR("Received unknown protocol message [{}], dropped it",packet.first);
//            }
//
//            // �㲥�����пͻ���
////            s_NetworkEngine->Broadcast(packet.first , packet.second , true);
//        }
//
//        while (!s_NetworkEngine->m_RemoveClients.empty()) {
//            uint32_t clientID = s_NetworkEngine->m_RemoveClients.front();
//            s_NetworkEngine->RemoveClient(clientID);
//            s_NetworkEngine->m_RemoveClients.pop();
//        }
//
//        
//    }

    void NetWorkEngine::OnUpdate(TimeStep ts) {
            constexpr int MAX_PACKETS_PER_FRAME = 50;
            int processed = 0;
            
            Packet packet;
            while (processed++ < MAX_PACKETS_PER_FRAME && s_NetworkEngine->m_PacketQueue.try_dequeue(packet)) {

                uint8_t moduleId = (packet.protocolId >> 8) & 0xFF;
                SERVER_INFO("Dealing message {}" , moduleId);
                if (moduleId == Modules::System::MODULE_ID) {
                    // systematical protocol
                    s_NetworkEngine->HandleSystemProtocol(packet);
                } else {
                    // transfer to application
                    if (s_NetworkEngine->m_AppPacketHandler) {
                        SERVER_INFO("App request, trasferring message");

                        s_NetworkEngine->m_AppPacketHandler(packet.clientId, packet.protocolId, packet.payload);
                    }
                }
            }
            // remove client
            while (!s_NetworkEngine->m_RemoveClients.empty()) {
                uint32_t clientID = s_NetworkEngine->m_RemoveClients.front();
                s_NetworkEngine->RemoveClient(clientID);
                s_NetworkEngine->m_RemoveClients.pop();
            }
        }

    bool NetWorkEngine::SendToClient(uint32_t clientId, const uint16_t& protocolID , ByteStream& byte, bool Reliable)
    {
        SERVER_ASSERT(s_NetworkEngine != nullptr, "NetworkEngine not initialized");

        ByteStream wrappedByte = byte;
        std::lock_guard<std::mutex> lock(m_ClientMutex);
        auto it = m_Clients.find(clientId);
        if (it == m_Clients.end() || !it->second->active) {
            SERVER_ERROR("Client {} not found or disconnected", clientId);
            return false;
        }
        
        size_t payloadSize = byte.getRawBufferLength();
        uint32_t totalSize = ByteStream::HEADER_SIZE + payloadSize;
        
        std::vector<uint8_t> sendBuffer;
        sendBuffer.resize(totalSize);

        // total length
        *reinterpret_cast<uint32_t*>(sendBuffer.data()) = totalSize;
        // prtocolID
        *reinterpret_cast<uint16_t*>(sendBuffer.data() + sizeof(uint32_t)) = protocolID;
        // cpoy Payload
        memcpy(sendBuffer.data() + sizeof(uint32_t) + sizeof(uint16_t), byte.getRawBuffer(), payloadSize);
        SERVER_INFO("Sent protocol:[{}] , totalSize:[{}]" , protocolID , totalSize);
        return it->second->channel->Send(sendBuffer.data(), totalSize);

//        size_t dataLength = wrappedByte.getRawBufferLength();
//        return it->second->channel->Send(wrappedByte.getRawBuffer(), dataLength);
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
