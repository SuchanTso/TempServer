//
//  TexasHoldemModule.hpp
//  Poker
//
//  Created by 左斯诚 on 2025/8/8.
//

#ifndef TexasHoldemModule_hpp
#define TexasHoldemModule_hpp


#pragma once

// [NEW] 包含 Card 的定义，以便在协议中使用
#include "logic/Card.h" // 假设这是你的 Card 定义文件路径
#include "Server/core/Module.h"
#include "PokerRoom.h"

namespace Tso::Modules::TexasHoldem {

// --- 模块ID ---
constexpr uint8_t MODULE_ID = 3;

// --- 命令ID ---
enum CommandID : uint8_t {
    // C2S: Client to Server
    C2S_PlayerAction = 1,       // 玩家提交动作

    // S2C: Server to Client
    S2C_GameStartInfoNtf = 1,   // 游戏开始，包含座位、盲注、筹码信息
    S2C_DealPrivateCardsNtf = 2,// 分发私有手牌
    S2C_DealPublicCardsNtf = 3, // 分发公共牌 (翻牌、转牌、河牌)
    S2C_TurnToActNtf = 4,       // 通知轮到谁行动
    S2C_PlayerActionNtf = 5,    // 广播玩家的行动
    S2C_GameResultNtf = 6,      // 广播游戏结果
};

// --- 辅助函数 ---
inline uint16_t MakeProtocolID(CommandID cmd) {
    return (MODULE_ID << 8) | cmd;
}

// --- 枚举定义 ---

// 玩家动作类型
enum class PlayerActionType : uint8_t {
    Fold = 0,
    Check = 1,
    Call = 2,
    Raise = 3,
    AllIn = 4,
};

// 玩家在牌桌上的角色
enum class PlayerRole : uint8_t {
    Player = 0,
    Dealer = 1,
    SmallBlind = 2,
    BigBlind = 3,
};

// --- 协议 Payload 中使用的数据结构 ---

// S2C_GameStartInfoNtf 中单个玩家的信息
struct PlayerGameInfo {
    uint32_t playerId;
    uint8_t seatIndex;
    int64_t chips;
    PlayerRole role;
};

// S2C_DealPrivateCardsNtf 中单个玩家的手牌
struct PlayerHandCards {
    uint32_t playerId;
    ParsedCard card1;
    ParsedCard card2;
};

// S2C_PlayerActionNtf 中广播的动作信息
struct ActionBroadcast {
    uint32_t playerId;
    PlayerActionType action;
    int64_t amount; // 如果是 Raise/Call/AllIn，这里的金额
    int64_t newTotalPot; // 动作发生后，总底池大小
};

// S2C_TurnToActNtf 中需要的信息
struct TurnInfo {
    uint32_t playerIdToAct;
    int64_t amountToCall;     // 需要跟注的金额
    int64_t minRaiseAmount;   // 最小可以加注到的总额
    bool canCheck;            // 是否可以过牌
    bool canRaise;            // 是否可以加注
};

// S2C_GameResultNtf 中单个玩家的结算信息
struct PlayerResultInfo {
    uint32_t playerId;
    int64_t winAmount;         // 赢得的筹码（正数）或输掉的（负数）
    std::string bestHandDesc;  // 最佳牌型描述，如 "Full House"
    ParsedCard handCard1;      // 如果摊牌，展示其手牌
    ParsedCard handCard2;
};

} // namespace

namespace Tso{

    class TexasHoldemModule : public IModule {
    public:
        TexasHoldemModule();
        uint8_t GetModuleId() const override;
        void HandlePacket(uint32_t clientId, uint8_t commandId, ByteStream& stream) override;
        void OnUpdate(TimeStep ts) override;

    private:
        using CommandHandler = std::function<void(uint32_t, const ByteStream&)>;
        std::unordered_map<uint8_t, CommandHandler> m_CommandHandlers;

        void OnPlayerActionReq(uint32_t clientId, const ByteStream& stream);

        // 内部方法
        void TakeOverGame(Ref<PokerRoom> room);
        void BroadcastToRoom(Ref<PokerRoom> room, uint16_t protocolId, ByteStream& ntf);

        // 管理正在进行的游戏
        std::unordered_map<uint32_t, Ref<PokerRoom>> m_ActiveGames;
        // 跟踪每个游戏的上一个状态，以便检测变化
        std::unordered_map<uint32_t, PokerRoomState> m_LastKnownState;
    };
}

#endif /* TexasHoldemModule_hpp */
