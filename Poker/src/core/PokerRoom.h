#pragma once
#include "Server/logic/Room.h"
#include "logic/Card.h"
#include "Pot.h"
namespace Tso
{
class PokerPlayer;

enum class PokerRoomState{
    Unknown     = 0,
    Init,
    Start ,
    SetDealer,
    BlinderBet,
    HandCard,
    HandBet,
    ThreeCards,
    ThreeBet,
    RollCard,
    RollBet,
    RiverCard,
    RiverBet,
    PotCalculation,
    CheckPlayer,
};
struct PokerConfig{
    uint8_t m_MaxPlayerNumber = 8;
    uint64_t m_SmallBet = 0;
    uint64_t m_GreatBet = 0;
    std::string m_CardConfig = "";
};

struct LogicConfig{
    int m_DealerIdx = -1;
    int m_SmallBlinderIdx = -1;
    int m_GreatBlinderIdx = -1;
    int m_BetIndex = -1;
    int m_BetStartIndex = -1;
    uint64_t m_BetNum = 0;
    bool m_RaiseFlag = false;
    std::vector<ParsedCard> m_CommonCards;
};
    class PokerRoom : public Room{
    public:
        PokerRoom() = delete;
        PokerRoom(const RoomID& roomId , const uint8_t& maxPlayer);
        void InitConfig();
        virtual void OnUpdate(float ts)override;
        virtual void OnAddPlayer(const PlayerID& playerID, Ref<Player> player)override;
        virtual void OnRemovePlayer(const PlayerID& playerID)override;
        int GetValidPlayerNum();
        bool SetDealer();
        int GetOntablePlayerNum();
        void Bet();
        void ResetPlayerBetState(bool bet);
        void CollectCalculatePlayers(std::vector<Ref<PokerPlayer>>& normalPlayers , std::vector<Ref<PokerPlayer>>& allinPlayers);
        Ref<Pot> GetPot(){return m_Pot;}
        void AddObserverPlayer();
        bool AddMoney();
        std::vector<Ref<PokerPlayer>> GetPokerPlayerList()&{return m_PlayerList;}
        PokerRoomState GetState()&{return m_State;}
        std::vector<ParsedCard> GetCommonCards()&{return m_LogicConfig.m_CommonCards;}
        int GetBetIndex()&{return m_LogicConfig.m_BetIndex;};
        uint64_t GetCurrentBet()&{return m_LogicConfig.m_BetNum;}
    private:
        PokerRoomState m_State = PokerRoomState::Init;
        std::vector<Ref<PokerPlayer>> m_PlayerList;
        std::queue<Ref<PokerPlayer>> m_Observers;
        PokerConfig m_GameConfig;
        LogicConfig m_LogicConfig;
        Ref<Card> m_Card = nullptr;
        Ref<Pot> m_Pot = nullptr;
        bool m_CheckState = false;
        // [MODIFIED] PokerRoom 现在持有具体的 PokerPlayer 列表
        std::vector<Ref<PokerPlayer>> m_PokerPlayerList;
    };
}
