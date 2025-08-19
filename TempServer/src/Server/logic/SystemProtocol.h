//
//  Protocol.h
//  TempServer
//
//  Created by 左斯诚 on 2025/8/7.
//

#ifndef Protocol_h
#define Protocol_h
#pragma once

namespace Tso::TexasHoldem {

enum class ProtocolID : uint8_t {
    // C2S: Client to Server
    // S2C: Server to Client
    // REQ: Request
    // RSP: Response
    // NTF: Notify (Server主动推送)

    // --- 系统级 (NetworkEngine处理) 0-19 ---
    C2S_Heartbeat = 1,
    S2C_Heartbeat = 2,

    // --- 应用级 (GameServer处理) 20+ ---

    // 登录与玩家
    C2S_LoginReq = 20,
    S2C_LoginRsp = 21,

    // 房间管理
    C2S_CreateRoomReq = 30,
    S2C_CreateRoomRsp = 31,
    C2S_JoinRoomReq = 32,
    S2C_JoinRoomRsp = 33,
    S2C_PlayerEnterNtf = 34,    // 通知房间内其他人有新玩家/旁观者进入
    S2C_PlayerLeaveNtf = 35,    // 通知玩家离开
    C2S_ReadyReq = 36,
    S2C_RoomStateNtf = 37,      // 完整房间状态同步 (给新加入者)

    // 游戏核心流程
    S2C_GameStartNtf = 50,      // 游戏开始，包含座位信息、盲注信息
    S2C_DealCardsNtf = 51,      // 发牌通知 (手牌+公共牌)
    S2C_TurnToActNtf = 52,      // 通知轮到谁行动
    C2S_PlayerActionReq = 53,   // 玩家提交操作 (Check, Call, Raise, Fold)
    S2C_PlayerActionNtf = 54,   // 广播玩家的操作
    S2C_GameResultNtf = 55,     // 游戏结算通知
};


} // namespace


#endif /* Protocol_h */
