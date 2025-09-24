//
//  Pot.hpp
//  Poker
//
//  Created by 左斯诚 on 2025/7/24.
//

#ifndef Pot_hpp
#define Pot_hpp
#include "Card.h"

namespace Tso{

class PokerPlayer;

struct CardInfo{
    Ref<PokerPlayer> player;
    CardIdentification cardID;
};
struct WinnerInfo{
    std::vector<CardInfo> potWinners;
    uint64_t rewards;
};

    class Pot{
    public:
        Pot() = default;
        ~Pot() = default;
        std::vector<WinnerInfo> Dispatch( std::vector<Ref<PokerPlayer>>& players ,  std::vector<Ref<PokerPlayer>>& allInPlayers ,const std::vector<Ref<PokerPlayer>>& fullPlayers);
        void AddValue(const uint64_t& num);
        uint64_t& GetPotMoney(){return m_PotValue;}
    private:
        uint64_t m_PotValue;
    };


}
#endif /* Pot_hpp */
