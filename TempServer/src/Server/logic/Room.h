#pragma once
#include "readerwriterqueue.h"
#include "data/ByteStream.h"
namespace Tso {
	class Player;
	class Room {
	public:
		Room(RoomID roomID);
		virtual void OnUpdate(float ts) = 0;
		virtual void AddPlayer(const PlayerID& playerID, const Player& player);
		virtual void RemovePlayer(const PlayerID& playerID);
		void LogicalDispatcher(const ByteStream& byte);
	private:
		RoomID m_ID = 0;//spare 0 for invalid room ID;
		std::unordered_map<PlayerID, Ref<Player>> m_PlayerList;
		moodycamel::ReaderWriterQueue<ByteStream> m_CmdQueue; // unlocked queue
	};
}
