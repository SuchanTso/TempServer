#pragma once
#include "readerwriterqueue.h"
#include "Server/data/ByteStream.h"
namespace Tso {
struct SyncTask{
    uint8_t rspId = 0;
    std::unordered_map<uint32_t, bool> syncMap;
    std::function<void(RoomID)> OnSyncFun;
};
	class Player;
	class Room {
	public:
        Room(RoomID roomID):m_ID(roomID){}
        virtual ~Room() = default;
        RoomID GetRoomID() const { return m_ID; }
		virtual void OnUpdate(float ts) = 0;
        void AddPlayer(const PlayerID& playerID, Ref<Player>& player);// net player management;
        virtual void OnAddPlayer(const PlayerID& playerID, Ref<Player>player) = 0;
        void RemovePlayer(const PlayerID& playerID);
        virtual void OnRemovePlayer(const PlayerID& playerID) = 0;
		void LogicalDispatcher(const ByteStream& byte);
        Ref<Player> GetPlayerByID(const PlayerID& playerID);
        Ref<Player> GetPlayerByNetID(const uint32_t& netID);
        bool IsGameInProgress(){return m_IsGameInProgress;}
        void SetProgress(bool state){m_IsGameInProgress = state;}
        const std::vector<Ref<Player>>& GetPlayerList() const { return m_PlayerList; }
        Ref<Player> GetPlayer(PlayerID playerID);
        bool AreAllPlayersReady()const;
        bool IsFull(){return m_PlayerList.size() >= m_MaxPlayerNum - 1;}
        uint8_t GetGameType(){return m_GameType;}
        void SetGameType(const uint8_t& gameType){m_GameType = gameType;}
        void OnMessageNotify(uint32_t clientId, uint8_t commandId, ByteStream& stream);
        void StartSynchronizer(const uint8_t& commadID , std::function<void(RoomID)>recallFun);
        void ClearClientReadyState();// 用于记录客户端的回应
        bool GetClientReadyState();

    protected:
        RoomID m_ID = 0;//spare 0 for invalid room ID;
        bool m_IsGameInProgress = false;
        std::unordered_map<PlayerID, Ref<Player>> m_PlayerMap;
        std::unordered_map<uint32_t, PlayerID> m_ClientMap;
        uint8_t m_MaxPlayerNum = 2;


	private:
		moodycamel::ReaderWriterQueue<ByteStream> m_CmdQueue; // unlocked queue
        std::vector<Ref<Player>> m_PlayerList; // 为了保持顺序
        std::vector<bool> m_PlayerReady;
        uint8_t m_GameType = 0;
        bool m_Sync = false;
        SyncTask m_SyncTask;
	};
}
