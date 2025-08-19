//
//  Pot.hpp
//  Poker
//
//  Created by 左斯诚 on 2025/7/24.
//

#ifndef Pot_hpp
#define Pot_hpp

namespace Tso{

class PokerPlayer;

    class Pot{
    public:
        Pot() = default;
        ~Pot() = default;
        void Dispatch( std::vector<Ref<PokerPlayer>>& players ,  std::vector<Ref<PokerPlayer>>& allInPlayers ,const std::vector<Ref<PokerPlayer>>& fullPlayers);
        void AddValue(const uint64_t& num);
    private:
        uint64_t m_PotValue;
    };


}
#endif /* Pot_hpp */
