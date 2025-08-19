#include"Spch.h"
#include"Room.h"
#include "Player.h"
#include "core/Application.h"

namespace Tso {

    void Room::AddPlayer(const PlayerID &playerID, Ref<Player>& player){
        if (m_PlayerMap.find(playerID) != m_PlayerMap.end()) {
            SERVER_WARN("Player %u already in room %u.", playerID, m_ID);
            return;
        }
        m_PlayerMap[playerID] = player;
        m_PlayerList.push_back(player);
        OnAddPlayer(playerID, player); // 调用子类的实现
    }

    void Room::RemovePlayer(const PlayerID& playerID){
        if (m_PlayerMap.find(playerID) == m_PlayerMap.end()) {
            return;
        }
        m_PlayerMap.erase(playerID);
        m_PlayerList.erase(std::remove_if(m_PlayerList.begin(), m_PlayerList.end(),
            [playerID](const Ref<Player>& p){ return p->GetPlayerID() == playerID; }), m_PlayerList.end());
        OnRemovePlayer(playerID); // 调用子类的实现
    }

    Ref<Player> Room::GetPlayer(PlayerID playerID) {
        auto it = m_PlayerMap.find(playerID);
        if (it != m_PlayerMap.end()) {
            return it->second;
        }
        return nullptr;
    }

    bool Room::AreAllPlayersReady() const {
        if (m_PlayerList.empty()) {
            return false;
        }
        for (const auto& player : m_PlayerList) {
            if (!player->IsReady()) {
                return false;
            }
        }
        return true;
    }



    Ref<Player> Room::GetPlayerByID(const PlayerID& playerID){
        
        if(m_PlayerMap.find(playerID) != m_PlayerMap.end()){
            return m_PlayerMap[playerID];
        }
        return nullptr;
    }

  
}
