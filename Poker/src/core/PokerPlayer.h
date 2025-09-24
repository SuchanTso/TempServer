#pragma once
#include "Server/logic/Player.h"
#include "logic/Card.h"
namespace Tso {
    class PokerRoom;
    class Pot;
    enum class RoleState{
        Player,
        SmallBlinder,
        GreateBlinder,
        Dealer
    };

    enum class CardState{
        Play,
        Bet,
        Raise,
        AllIn,
        Fold
    };


class PokerPlayer : public Player {
    public:
//    PokerPlayer():Player(){};
    PokerPlayer(PokerPlayer& pokerPlayer) = delete;
    PokerPlayer(uint64_t& playerID , uint32_t& netID ,const std::string& name);
//    PokerPlayer(Ref<Player> player);
    RoleState GetPlayerState(){return m_State;}
    void SetPlayerState(const RoleState& state);
    void AddMoney(const uint64_t& count);
    void HandCard(const std::vector<ParsedCard>& handcards);
    CardState& GetCardState(){return m_CardState;}
    void SetCardState(const CardState& cardState){m_CardState = cardState;}
    uint64_t& GetBetNumber(){return m_BetNumber;}
    uint64_t GetTotalNumber(){return m_TotalBetNumber;}
    uint64_t& GetCurrentMoney(){return m_Money;}
    void SetBetNumber(const uint64_t& betNumber){m_BetNumber = betNumber;}
    void Calculate(const std::vector<ParsedCard>& commonCards);
    void SetRoom(PokerRoom* room){m_Room = room;}
    void SetSeatIndex(const uint8_t& seat){m_Seat = seat;};
    uint8_t& GetSeatIndex(){return m_Seat;}
    Ref<Pot> GetPot();
    void Bet(const uint64_t& num);
    CardIdentification& GetMaxCardID(){return m_MaxCards;}
    void Reset();
    std::vector<ParsedCard> GetHandCards()&{return m_HandCards;}
    void Check();
    void Call(const uint64_t& num);
    void Fold();
    void AllIn();
    void Raise(const uint64_t& num);
    void ResetBetTurn();
    
        
    private:
    RoleState m_State = RoleState::Player;
    uint64_t m_Money = 0;
    uint64_t m_TotalMoney = 0;
    std::vector<ParsedCard> m_HandCards;
    CardState m_CardState = CardState::Play;
    uint64_t m_BetNumber = 0;//number each round
    uint64_t m_TotalBetNumber = 0;//total money each game. for pot calculation;
    CardIdentification m_MaxCards;
    PokerRoom* m_Room = nullptr;
    uint8_t m_Seat = 0;
    PokerPlayer& operator=(const PokerPlayer& other) {
            if (this != &other) { // 避免自赋值
                // 1. 调用基类的赋值操作符 (假设 Player 有赋值操作符)
                Player::operator=(other);

                // 2. 复制简单成员
                m_State = other.m_State;
                m_Money = other.m_Money;
                m_TotalMoney = other.m_TotalMoney;
                m_CardState = other.m_CardState;
                m_BetNumber = other.m_BetNumber;
                m_TotalBetNumber = other.m_TotalBetNumber;
                m_MaxCards = other.m_MaxCards;

                // 3.  复制 m_HandCards (假设 ParsedCard 具有正确的赋值操作符)
                m_HandCards = other.m_HandCards; // 使用 std::vector 的赋值操作符

                // 4.  处理 m_Room (浅拷贝)
                m_Room = other.m_Room; // 浅拷贝 (不创建新的PokerRoom)
            }
            return *this;
        }

	};

}
