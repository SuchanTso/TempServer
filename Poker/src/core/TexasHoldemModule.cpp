//
//  TexasHoldemModule.cpp
//  Poker
//
//  Created by 左斯诚 on 2025/8/8.
//
#include "Spch.h"
#include "TexasHoldemModule.h"
#include "Server/network//NetworkEngine.h"
#include "Server/core/LobbyModule.h"
#include "PokerPlayer.h"
#include "Poker.h"

namespace Tso{

    TexasHoldemModule::TexasHoldemModule() {
        m_CommandHandlers[Modules::TexasHoldem::C2S_PlayerAction] = [this](uint32_t cid, const ByteStream& s){ this->OnPlayerActionReq(cid, s); };
    }

    uint8_t TexasHoldemModule::GetModuleId() const { return Modules::TexasHoldem::MODULE_ID; }

    void TexasHoldemModule::HandlePacket(uint32_t clientId, uint8_t commandId, ByteStream& stream) {
        auto it = m_CommandHandlers.find(commandId);
        if (it != m_CommandHandlers.end()) it->second(clientId, stream);
    }

    void TexasHoldemModule::TakeOverGame(Ref<PokerRoom> room) {
        uint32_t roomId = room->GetRoomID();
        if (m_ActiveGames.count(roomId)) return; // 防止重复接管
        
        m_ActiveGames[roomId] = room;
        // 设置一个不存在的初始状态，强制在下一帧 OnUpdate 中进行第一次状态广播
        m_LastKnownState[roomId] = PokerRoomState::Unknown;
        SERVER_INFO("TexasHoldemModule has taken over game in room %u.", roomId);
    }

    void TexasHoldemModule::BroadcastToRoom(Ref<PokerRoom> room, uint16_t protocolId, ByteStream& ntf) {
        const auto& playerList = room->GetPokerPlayerList(); // 使用 PokerPlayer 列表
        for (const auto& player : playerList) {
            Tso::NetWorkEngine::GetEngine()->SendToClient(player->GetNetID(), protocolId, ntf,true);
        }
    }

    void TexasHoldemModule::OnUpdate(TimeStep ts) {
        // 1. 轮询 LobbyModule 寻找可以开始的新游戏
        Poker* poker = static_cast<Poker*>(Poker::GetApp());
        LobbyModule* lobby = static_cast<LobbyModule*>(static_cast<Poker*>(Poker::GetApp())->GetModule(Tso::Modules::Lobby::MODULE_ID));
        if (lobby) {
            // 假设 LobbyModule 提供了 GetRooms() 方法
            const auto& allRooms = lobby->GetRooms();
            for (const auto& [roomId, room] : allRooms) {
                // 如果房间准备好，且游戏未开始，且未被接管
                if (room->AreAllPlayersReady() && !room->IsGameInProgress() && !m_ActiveGames.count(roomId)) {
                    Ref<PokerRoom> pokerRoom = std::dynamic_pointer_cast<PokerRoom>(room);
                    if (pokerRoom) {
                        TakeOverGame(pokerRoom);
                    }
                }
            }
        }
        
        // 2. 驱动并翻译所有已接管的游戏
        for (auto it = m_ActiveGames.begin(); it != m_ActiveGames.end(); ) {
            auto& room = it->second;
            uint32_t roomId = it->first;
            
            // 驱动游戏逻辑状态机前进
            room->OnUpdate(ts);
            
            PokerRoomState currentState = room->GetState();
            
            // 游戏结束，释放控制权
            if (currentState == PokerRoomState::CheckPlayer) {
                m_LastKnownState.erase(roomId);
                it = m_ActiveGames.erase(it);
                SERVER_INFO("TexasHoldemModule: Releasing control of room %u.", roomId);
                continue;
            }
            
            PokerRoomState lastState = m_LastKnownState[roomId];
            if (currentState != lastState) {
                SERVER_INFO("Room %u state changed: %d -> %d. Broadcasting update.", roomId, (int)lastState, (int)currentState);
                
                // 根据状态变化，翻译成网络消息
                switch(currentState) {
                    case PokerRoomState::BlinderBet: {
                        ByteStream ntf;
                        ntf.write(room->GetRoomID());
                        // ... 填充座位、盲注、玩家筹码等信息 ...
                        BroadcastToRoom(room, MakeProtocolID(Modules::TexasHoldem::S2C_GameStartInfoNtf), ntf);
                        break;
                    }
                    case PokerRoomState::HandCard: {
                        ByteStream ntf;
                        const auto& playerList = room->GetPokerPlayerList();
                        ntf.write<uint8_t>(playerList.size());
                        for (const auto& p : playerList) {
                            ntf.write(p->GetPlayerID());
                            const auto& handCards = p->GetHandCards();
                            ntf.write(handCards[0]); // ParsedCard
                            ntf.write(handCards[1]);
                        }
                        BroadcastToRoom(room, MakeProtocolID(Modules::TexasHoldem::S2C_DealPrivateCardsNtf), ntf);
                        break;
                    }
                    case PokerRoomState::ThreeCards:
                    case PokerRoomState::RollCard:
                    case PokerRoomState::RiverCard: {
                        ByteStream ntf;
                        const auto& commonCards = room->GetCommonCards();
                        ntf.write<uint8_t>(commonCards.size());
                        for(const auto& card : commonCards) ntf.write(card);
                        BroadcastToRoom(room, MakeProtocolID(Modules::TexasHoldem::S2C_DealPublicCardsNtf), ntf);
                        break;
                    }
                    case PokerRoomState::PotCalculation: {
                        ByteStream ntf;
                        // ... 从 room 的 Pot 对象获取赢家和筹码变动 ...
                        BroadcastToRoom(room, MakeProtocolID(Modules::TexasHoldem::S2C_GameResultNtf), ntf);
                        break;
                    }
                    default:
                        break;
                }
                
                // 如果进入了任何下注阶段，就通知轮到谁行动
                if (currentState >= PokerRoomState::HandBet && currentState <= PokerRoomState::RiverBet) {
                    int betIndex = room->GetBetIndex();
                    const auto& playerList = room->GetPokerPlayerList();
                    if (betIndex >= 0 && betIndex < playerList.size()) {
                        auto nextPlayer = playerList[betIndex];
                        ByteStream turnNtf;
                        turnNtf.write(nextPlayer->GetPlayerID());
                        // turnNtf.write(room->GetCurrentBet()); // TODO: 当前需要跟注的金额
                        BroadcastToRoom(room, MakeProtocolID(Modules::TexasHoldem::S2C_TurnToActNtf), turnNtf);
                    }
                }
                
                m_LastKnownState[roomId] = currentState;
            }
            ++it;
        }
    }

    void TexasHoldemModule::OnPlayerActionReq(uint32_t clientId, const ByteStream& stream) {
        // 找到该玩家所在的活跃游戏房间
        Ref<PokerRoom> room = nullptr;
        for (const auto& [rid, r] : m_ActiveGames) {
            if (r->GetPlayer(clientId)) {
                room = r;
                break;
            }
        }
        if (!room) {
            SERVER_WARN("Received player action from client %u who is not in an active game.", clientId);
            return;
        }
        
        auto player = std::dynamic_pointer_cast<PokerPlayer>(room->GetPlayer(clientId));
        if (!player) return;
        
        // 检查是否轮到该玩家行动
        int betIndex = room->GetBetIndex();
        const auto& playerList = room->GetPokerPlayerList();
        if (playerList[betIndex]->GetPlayerID() != clientId) {
            SERVER_WARN("Received player action from client %u, but it's not their turn.", clientId);
            return;
        }
        
        auto actionType = stream.read<Modules::TexasHoldem::PlayerActionType>();
        int64_t amount = 0;
        if (actionType == Modules::TexasHoldem::PlayerActionType::Raise) {
            amount = stream.read<int64_t>();
        }
        
        // 调用 PokerPlayer 的方法来改变其内部状态
        switch(actionType) {
            case Modules::TexasHoldem::PlayerActionType::Fold:  player->Fold(); break;
            case Modules::TexasHoldem::PlayerActionType::Check: player->Check(); break;
            case Modules::TexasHoldem::PlayerActionType::Call:  player->Call(room->GetCurrentBet()); break;
            case Modules::TexasHoldem::PlayerActionType::Raise: player->Raise(amount); break;
            case Modules::TexasHoldem::PlayerActionType::AllIn: player->AllIn(); break;
        }
        
        // 广播这个动作给房间里的所有人
        ByteStream ntf;
        ntf.write(clientId);
        ntf.write(actionType);
        if (actionType == Modules::TexasHoldem::PlayerActionType::Raise) ntf.write(amount);
        
        BroadcastToRoom(room, MakeProtocolID(Modules::TexasHoldem::S2C_PlayerActionNtf), ntf);
    }
}
