#pragma once

namespace Tso {
	class Player {
	public:
        Player();
        Player(Player& player) = default;
//        Player(PlayerID playerID, const std::string& name): m_ID(playerID),m_Name(name){}
		Player(PlayerID& playerID , uint32_t& clientID , const std::string& name): m_ID(playerID) , m_ClientID(clientID),m_Name(name){}
        virtual ~Player() = default;
        PlayerID& GetPlayerID(){return m_ID;}
        std::string GetPlayerIDstr();
        uint32_t& GetNetID(){return m_ClientID;}
        bool IsReady(){return m_Ready;}
        std::string GetName(){return m_Name;}
        void SetReady(bool ready){m_Ready = ready;}
    protected:
        std::string m_Name = "Player";
        bool m_Ready = false;
		PlayerID m_ID = 0;//spare 0 for invalid playerID
        // I may design PlayerID unique for every account for now.
		uint32_t m_ClientID = -1;// clientID from networkEngine.
	};
}
