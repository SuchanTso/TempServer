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
        m_ClientMap[player->GetNetID()] = playerID;
        OnAddPlayer(playerID, player); // 调用子类的实现
    }

    void Room::RemovePlayer(const PlayerID& playerID){
        if (m_PlayerMap.find(playerID) == m_PlayerMap.end()) {
            return;
        }
        auto player = m_PlayerMap[playerID];
        m_ClientMap.erase(player->GetNetID());
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

Ref<Player> Room::GetPlayerByNetID(const uint32_t& netID){
    if(m_ClientMap.find(netID) != m_ClientMap.end()){
        PlayerID playerID = m_ClientMap[netID];
        return GetPlayer(playerID);
    }
    return nullptr;
}


void Room::ClearClientReadyState(){
    m_PlayerReady.resize(m_PlayerList.size());
    for(int i = 0 ; i < m_PlayerReady.size() ; i++){
        m_PlayerReady[i] = false;
    }
}
bool Room::GetClientReadyState(){
    for(auto ready : m_PlayerReady){
        if(!ready) return false;;
    }
    return true;
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

void Room::OnMessageNotify(uint32_t clientId, uint8_t commandId, ByteStream &stream){
    if(m_ClientMap.find(clientId) == m_ClientMap.end() || !m_Sync || (commandId != m_SyncTask.rspId)){
        return;
    }
    m_SyncTask.syncMap[clientId] = true;
    for(auto& client : m_ClientMap){
        if(m_SyncTask.syncMap.find(client.first) == m_SyncTask.syncMap.end()){
            return;
        }
    }
    m_SyncTask.OnSyncFun(m_ID);
    m_Sync = false;
}

void Room::StartSynchronizer(const uint8_t &commadID, std::function<void (RoomID)> recallFun){
    SERVER_ASSERT(m_Sync == false , "Synchroning task is occupied with other task, unable to create one more sync task");
    m_SyncTask.rspId = commadID;
    m_Sync = true;
    m_SyncTask.syncMap.clear();
    m_SyncTask.OnSyncFun = recallFun;
}


    Ref<Player> Room::GetPlayerByID(const PlayerID& playerID){
        
        if(m_PlayerMap.find(playerID) != m_PlayerMap.end()){
            return m_PlayerMap[playerID];
        }
        return nullptr;
    }

  
}
