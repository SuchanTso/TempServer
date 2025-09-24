#include "Spch.h"
#include "LobbyModule.h"
#include "Server/network/NetworkEngine.h"
#include "Server/data/ByteStream.h"
#include "Server/logic/Room.h"
#include "Server/logic/Player.h"
#include "SystemModule.h"
namespace Tso{

    // [NEW] 注册工厂
    void LobbyModule::RegisterRoomFactory(Modules::Lobby::GameType gameType, RoomFactory factory) {
        if (m_RoomFactories.find(gameType) != m_RoomFactories.end()) {
            SERVER_WARN("GameType {} factory already registered. Overwriting.", (uint8_t)gameType);
        }
        m_RoomFactories[gameType] = factory;
        SERVER_INFO("Registered factory for game type {}.", (uint8_t)gameType);
    }

    void LobbyModule::RegisterPlayerFactory(Tso::Modules::Lobby::GameType gameType, PlayerFactory factory){
        if (m_PlayerFactories.find(gameType) != m_PlayerFactories.end()) {
            SERVER_WARN("GameType {} factory already registered. Overwriting.", (uint8_t)gameType);
        }
        m_PlayerFactories[gameType] = factory;
        SERVER_INFO("Registered factory for game type {}.", (uint8_t)gameType);
    }

    void LobbyModule::RegistrerPlayerNameGetFunction(std::function<std::string(const uint32_t&)> fun){
        GetUserName = fun;
    }


    // [NEW] 获取房间接口
    Ref<Room> LobbyModule::GetRoom(uint32_t roomId) {
        auto it = m_Rooms.find(roomId);
        if (it != m_Rooms.end()) {
            return it->second;
        }
        return nullptr;
    }

    LobbyModule::LobbyModule() {
        m_CommandHandlers[Modules::Lobby::C2S_CreateRoomReq] = [this](uint32_t cid, const ByteStream& s){ this->OnCreateRoomReq(cid, s); };
        m_CommandHandlers[Modules::Lobby::C2S_JoinRoomReq] =   [this](uint32_t cid, const ByteStream& s){ this->OnJoinRoomReq(cid, s); };
        m_CommandHandlers[Modules::Lobby::C2S_ReadyReq] =      [this](uint32_t cid, const ByteStream& s){ this->OnReadyReq(cid, s); };
    }

    uint8_t LobbyModule::GetModuleId() const { return Modules::Lobby::MODULE_ID; }

    void LobbyModule::HandlePacket(uint32_t clientId, uint8_t commandId, ByteStream& stream) {
        auto it = m_CommandHandlers.find(commandId);
        if (it != m_CommandHandlers.end()) it->second(clientId, stream);
        auto rooms = GetRooms();
        for(auto& room : rooms){
            room.second->OnMessageNotify(clientId, commandId, stream);
        }
    }

    // [MODIFIED] 使用工厂创建房间，不再关心具体类型
    void LobbyModule::OnCreateRoomReq(uint32_t clientId, const ByteStream& stream) {
        auto gameType = (Modules::Lobby::GameType)stream.read<uint8_t>();
        std::string roomName = stream.readString();
        uint8_t maxPlayer = stream.read<uint8_t>();
        
        auto factoryIt = m_RoomFactories.find(gameType);
        if (factoryIt == m_RoomFactories.end()) {
            SERVER_ERROR("No factory registered for game type {}.", (uint8_t)gameType);
            // ... 发送错误响应给客户端 ...
            return;
        }
        
        uint32_t newRoomId = m_NextRoomId++;
        Ref<Room> newRoom = factoryIt->second(newRoomId, maxPlayer); // 使用工厂创建
        if (!newRoom) {
            SERVER_ERROR("Factory for game type {} failed to create a room.", (uint8_t)gameType);
            return;
        }
        
        // newRoom->SetRoomName(roomName); // Room 基类可以有 SetName 方法
        newRoom->SetGameType((uint8_t)gameType);
        // Player 的创建也应该是通用的
        PlayerID playerID = strtoull(stream.readString().c_str() , NULL , 10);
        std::string playerName = GetUserName ? GetUserName(clientId) : "player" + std::to_string(clientId);
        Ref<Player> player = m_PlayerFactories[gameType](playerID , clientId , playerName);
        newRoom->AddPlayer(clientId, player); // 调用 Room 基类的 AddPlayer
        
        m_Rooms[newRoomId] = newRoom;
        m_PlayerToRoom[clientId] = newRoomId;
        
        ByteStream response;
        response.write<uint8_t>(0); // success
        response.write<uint32_t>(newRoomId);
        NetWorkEngine::GetEngine()->SendToClient(clientId, MakeProtocolID(Modules::Lobby::S2C_CreateRoomRsp), response,true);
        
        BroadcastRoomState(newRoomId);
    }

    // [MODIFIED] 只与 Room 基类交互
    void LobbyModule::OnJoinRoomReq(uint32_t clientId, const ByteStream& stream) {
        uint8_t roomId = stream.read<uint8_t>();
        auto it = m_Rooms.find(roomId);
        
        if (it == m_Rooms.end() || it->second->IsGameInProgress()) { // Room 基类需要 IsGameInProgress
            // ... 发送失败响应 ...
            return;
        }
        if(it->second->IsFull()){
            //TODO: max full room , unable to join
            return;
        }
        PlayerID playerID = strtoull(stream.readString().c_str() , NULL , 10);
        
        std::string playerName = GetUserName ? GetUserName(clientId) : "player" + std::to_string(clientId);
        Ref<Player> player = m_PlayerFactories[(Modules::Lobby::GameType)it->second->GetGameType()](playerID , clientId, playerName);
        //CreateRef<Player>(playerID , clientId, "Player" + std::to_string(clientId));//clientID , playerID
        it->second->AddPlayer(clientId, player);
        m_PlayerToRoom[clientId] = roomId;
        
        // ... 发送成功响应 ...
        BroadcastRoomState(roomId);
    }

    // [MODIFIED] 只与 Player 基类交互
    void LobbyModule::OnReadyReq(uint32_t clientId, const ByteStream& stream) {
        bool isReady = stream.read<bool>();
        
        if (m_PlayerToRoom.find(clientId) == m_PlayerToRoom.end()) return;
        uint32_t roomId = m_PlayerToRoom[clientId];
        
        if (m_Rooms.find(roomId) == m_Rooms.end()) return;
        auto& room = m_Rooms[roomId];
        
        auto player = room->GetPlayer(clientId);
        if (player) {
            player->SetReady(isReady); // Player 基类需要 SetReady
        }
        
        BroadcastRoomState(roomId);
        
        // [NEW] 检查并通知游戏可以开始
        // LobbyModule 不再关心游戏如何开始，但它可以派发一个事件或提供查询接口
        if (room->AreAllPlayersReady()) { // Room 基类需要 AreAllPlayersReady
            SERVER_INFO("Room {} is ready to start game.", roomId);
        }
    }

    // [MODIFIED] 只使用基类提供的信息
    void LobbyModule::BroadcastRoomState(uint32_t roomId) {
        if (m_Rooms.find(roomId) == m_Rooms.end()) return;
        const auto& room = m_Rooms[roomId];
        
        ByteStream ntf;
        ntf.write<RoomID>(room->GetRoomID());
        // ntf.write(room->GetName());
        ntf.write<uint8_t>(room->IsGameInProgress() ? 1 : 0);
        
        const auto& playerList = room->GetPlayerList();
        ntf.write<uint8_t>(playerList.size());
        for (const auto& player : playerList) {
            ntf.writeString(std::to_string(player->GetPlayerID()));
            ntf.writeString(player->GetName());
            ntf.write<uint8_t>(player->IsReady() ? 1 : 0);
        }
        
        uint16_t ntfId = MakeProtocolID(Modules::Lobby::S2C_RoomStateNtf);
        for (const auto& player : playerList) {
            NetWorkEngine::GetEngine()->SendToClient(player->GetNetID(), ntfId, ntf,true);
        }
    }

    // [MODIFIED] 只与基类交互
    void LobbyModule::OnPlayerDisconnected(uint32_t clientId) {
        if (m_PlayerToRoom.find(clientId) == m_PlayerToRoom.end()) return;
        
        uint32_t roomId = m_PlayerToRoom[clientId];
        auto roomIt = m_Rooms.find(roomId);
        if (roomIt == m_Rooms.end()) return;
        
        auto& room = roomIt->second;
        room->RemovePlayer(clientId); // 调用基类的 RemovePlayer
        m_PlayerToRoom.erase(clientId);
        
        if (room->GetPlayerList().empty()) {
            m_Rooms.erase(roomIt);
        } else {
            BroadcastRoomState(roomId);
        }
    }
}
