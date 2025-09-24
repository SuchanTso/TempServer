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
#include <string> // 用于 std::to_string

namespace Tso{

    TexasHoldemModule::TexasHoldemModule() {
        m_CommandHandlers[Modules::TexasHoldem::C2S_PlayerAction] = [this](uint32_t cid, const ByteStream& s){ this->OnPlayerActionReq(cid, s); };
//        m_CommandHandlers[Modules::TexasHoldem::C2S_ClientReady] = [this](uint32_t cid, const ByteStream& s){ this->OnPlayerActionReq(cid, s); };

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
        SERVER_INFO("TexasHoldemModule has taken over game in room {}.", roomId);
        
        // 关键: 通知房间游戏已开始，并广播状态
        LobbyModule* lobby = static_cast<LobbyModule*>(static_cast<Poker*>(Poker::GetApp())->GetModule(Tso::Modules::Lobby::MODULE_ID));
        if(lobby){
            room->SetProgress(true);
            lobby->BroadcastRoomState(room->GetRoomID());
        }
        room->StartSynchronizer(Modules::Lobby::C2S_ClientReady, [](RoomID roomID)->void{//TODO: 临时变量被销毁
//            room->StartNewGame();
            LobbyModule* lobby = static_cast<LobbyModule*>(static_cast<Poker*>(Poker::GetApp())->GetModule(Tso::Modules::Lobby::MODULE_ID));
            auto room = std::dynamic_pointer_cast<PokerRoom>(lobby->GetRoom(roomID));
            room->StartNewGame();
        });
        

    }

    void TexasHoldemModule::BroadcastToRoom(Ref<PokerRoom> room, uint16_t protocolId, ByteStream& ntf) {
        const auto& playerList = room->GetPokerPlayerList(); // 使用 PokerPlayer 列表
        for (const auto& player : playerList) {
            Tso::NetWorkEngine::GetEngine()->SendToClient(player->GetNetID(), protocolId, ntf,true);
        }
    }

    void TexasHoldemModule::OnUpdate(TimeStep ts) {
        // 1. 轮询 LobbyModule 寻找可以开始的新游戏
        LobbyModule* lobby = static_cast<LobbyModule*>(static_cast<Poker*>(Poker::GetApp())->GetModule(Tso::Modules::Lobby::MODULE_ID));
        if (lobby) {
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
            if (room->GetState() == PokerRoomState::Start && !room->IsGameInProgress()) {
                it = m_ActiveGames.erase(it);
                SERVER_INFO("TexasHoldemModule: Releasing control of room {}.", roomId);
                if(lobby) {
                    SERVER_INFO("TexasHoldemModule: round ends. notifying");
                    lobby->BroadcastRoomState(roomId); } // 通知大厅，游戏结束，可以准备下一局了
            } else {
                ++it;
            }
        }
    }

    void TexasHoldemModule::OnPlayerActionReq(uint32_t clientId, const ByteStream& stream) {
        Ref<PokerRoom> room = nullptr;
        Ref<Player> player = nullptr;
        for (const auto& [rid, r] : m_ActiveGames) {
            player = r->GetPlayerByNetID(clientId);
            if (player) {//TODO: wrong logic
                room = r;
                break;
            }
        }
        if (!room) {
            SERVER_WARN("Received player action from client {} who is not in an active game.", clientId);
            return;
        }
        
        auto pokerPlayer = std::dynamic_pointer_cast<PokerPlayer>(player);
        if (!pokerPlayer) return;
        
        int betIndex = room->GetBetIndex();
        const auto& playerList = room->GetPokerPlayerList();
        if (betIndex < 0 || betIndex >= playerList.size() || playerList[betIndex]->GetNetID() != clientId) {
             SERVER_WARN("Received player action from client {}, but it's not their turn.", clientId);
            return;
        }
        
        auto actionType = (Modules::TexasHoldem::PlayerActionType)stream.read<uint8_t>();
        uint64_t amount = 0;
        if (actionType == Modules::TexasHoldem::PlayerActionType::Raise) {
            SERVER_INFO("Received Raise action");
            std::string amountStr = stream.readString();
            amount = strtoull(amountStr.c_str(), nullptr, 10);
        }
        
        // 调用 PokerPlayer 的方法来改变其内部状态
        switch(actionType) {
            case Modules::TexasHoldem::PlayerActionType::Fold:  pokerPlayer->Fold(); break;
            case Modules::TexasHoldem::PlayerActionType::Check: pokerPlayer->Check(); break;
            case Modules::TexasHoldem::PlayerActionType::Call:  pokerPlayer->Call(room->GetCurrentBet()); break;
            case Modules::TexasHoldem::PlayerActionType::Raise: pokerPlayer->Raise(amount); break;
            case Modules::TexasHoldem::PlayerActionType::AllIn: pokerPlayer->AllIn(); break;
        }
        
        
        // 广播这个动作给房间里的所有人
        ByteStream ntf;
        ntf.writeString(player->GetPlayerIDstr());
        ntf.write<uint8_t>((uint8_t)actionType);
        if (actionType == Modules::TexasHoldem::PlayerActionType::Raise) {
            ntf.writeString(std::to_string(amount));
        }
        ntf.writeString(std::to_string(pokerPlayer->GetCurrentMoney()));
        ntf.writeString(std::to_string(room->GetPot()->GetPotMoney()));
        
        BroadcastToRoom(room, MakeProtocolID(Modules::TexasHoldem::S2C_PlayerActionNtf), ntf);
        
        room->OnPlayerAction();

    }
}
