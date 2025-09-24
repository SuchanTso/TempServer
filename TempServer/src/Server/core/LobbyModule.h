//
//  LobbyModule.h
//  TempServer
//
//  Created by 左斯诚 on 2025/8/7.
//

#ifndef LobbyModule_h
#define LobbyModule_h
#pragma once
#include "Server/core/Module.h"


namespace Tso::Modules::Lobby {

    enum class GameType : uint8_t {
            TexasHoldem = 1,
            // Landlord = 2, // for future game
        };

constexpr uint8_t MODULE_ID = 2;

enum CommandID : uint8_t {
    // C2S
    C2S_CreateRoomReq = 1,
    C2S_JoinRoomReq = 2,
    C2S_ReadyReq = 3,
    C2S_GetRoomListReq = 4,
    C2S_ClientReady = 5,

    // S2C
    S2C_CreateRoomRsp = 1,
    S2C_JoinRoomRsp = 2,
    S2C_RoomStateNtf = 3,     // 广播房间状态（玩家列表、准备状态等）
    S2C_GameStartNtf = 4,     // 通知客户端游戏开始，应切换到游戏场景
    S2C_RoomListRsp = 5,
};

inline uint16_t MakeProtocolID(CommandID cmd) {
    return (MODULE_ID << 8) | cmd;
}

// 房间内玩家信息
struct PlayerInfo {
    uint32_t clientId;
    std::string username;
    bool isReady;
};

// 房间信息
struct RoomInfo {
    uint32_t roomId;
    std::string roomName;
    std::vector<PlayerInfo> players;
    bool isGameInProgress = false;
};

}


namespace Tso{
    class Room;
    class Player;
    class LobbyModule : public IModule {
    public:
        // [NEW] 使用工厂模式来解耦房间创建
        using RoomFactory = std::function<Ref<Room>(uint32_t, uint8_t)>;
        using PlayerFactory = std::function<Ref<Player>(uint64_t , uint32_t, std::string)>;
        void RegisterRoomFactory(Tso::Modules::Lobby::GameType gameType, RoomFactory factory);
        void RegisterPlayerFactory(Tso::Modules::Lobby::GameType gameType, PlayerFactory factory);
        void RegistrerPlayerNameGetFunction(std::function<std::string(const uint32_t&)> fun);


        LobbyModule();
        uint8_t GetModuleId() const override;
        virtual void HandlePacket(uint32_t clientId, uint8_t commandId, ByteStream& stream) override;
        void OnPlayerDisconnected(uint32_t clientId) override;

        // [NEW] 提供给其他模块查询房间的接口
        Ref<Room> GetRoom(uint32_t roomId);
        std::unordered_map<uint32_t, Ref<Room>>GetRooms(){return m_Rooms;}
        void BroadcastRoomState(uint32_t roomId);

    private:
        using CommandHandler = std::function<void(uint32_t, const ByteStream&)>;
        std::unordered_map<uint8_t, CommandHandler> m_CommandHandlers;

        // 业务逻辑
        void OnCreateRoomReq(uint32_t clientId, const ByteStream& stream);
        void OnJoinRoomReq(uint32_t clientId, const ByteStream& stream);
        void OnReadyReq(uint32_t clientId, const ByteStream& stream);
        

        // [MODIFIED] 数据结构只依赖于抽象基类
        std::unordered_map<uint32_t, Ref<Room>> m_Rooms;
        std::unordered_map<uint32_t, uint32_t> m_PlayerToRoom;
        uint32_t m_NextRoomId = 1;

        // [NEW] 存储已注册的工厂
        std::unordered_map<Tso::Modules::Lobby::GameType, RoomFactory> m_RoomFactories;
        std::unordered_map<Tso::Modules::Lobby::GameType, PlayerFactory> m_PlayerFactories;
        
        std::function<std::string(const uint32_t&)> GetUserName;
    };
}
#endif /* LobbyModule_h */
