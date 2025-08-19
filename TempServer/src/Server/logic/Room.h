#pragma once
#include "readerwriterqueue.h"
#include "Server/data/ByteStream.h"
namespace Tso {
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
        bool IsGameInProgress(){return m_IsGameInProgress;}
        const std::vector<Ref<Player>>& GetPlayerList() const { return m_PlayerList; }
        Ref<Player> GetPlayer(PlayerID playerID);
        bool AreAllPlayersReady()const;
    protected:
        RoomID m_ID = 0;//spare 0 for invalid room ID;
        bool m_IsGameInProgress = false;
        std::unordered_map<PlayerID, Ref<Player>> m_PlayerMap;


	private:
		moodycamel::ReaderWriterQueue<ByteStream> m_CmdQueue; // unlocked queue
        std::vector<Ref<Player>> m_PlayerList; // 为了保持顺序
	};
}
