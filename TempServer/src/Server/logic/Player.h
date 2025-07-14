#pragma once

namespace Tso {
	class Player {
	public:
		Player() = delete;
		Player(PlayerID& playerID , uint32_t& clientID): m_ID(playerID) , m_ClientID(clientID){}
	private:
		PlayerID m_ID = 0;//spare 0 for invalid playerID
		uint32_t m_ClientID = -1;// clientID from networkEngine.
	};
}
