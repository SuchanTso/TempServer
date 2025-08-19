//
//  Player.cpp
//  Poker
//
//  Created by 左斯诚 on 2025/7/20.
//
#include "Spch.h"
#include "PokerPlayer.h"
#include "PokerRoom.h"

namespace Tso {




    void PokerPlayer::SetPlayerState(const RoleState &state){
        m_State = state;
    }

//    bool PokerPlayer::RequestMoney(const uint64_t &count){
//        if(m_Money < count){
//            //TODO: Ask player to add money through net
//            return false;
//        }
//        m_Money -= count;
//        return true;
//    }
    
    void PokerPlayer::AddMoney(const uint64_t &count){
        m_Money += count;
    }

    void PokerPlayer::HandCard(const std::vector<ParsedCard> &handcards){
        m_HandCards = handcards;
    }

    void PokerPlayer::Check(){
        m_CardState = CardState::Bet;
    }
    void PokerPlayer::Call(const uint64_t& num){
        m_BetNumber = num;
        m_TotalBetNumber += num;
        m_Money -= num;
        m_CardState = CardState::Bet;
    }
    void PokerPlayer::Fold(){
        m_CardState = CardState::Fold;
    }
    void PokerPlayer::AllIn(){
        m_CardState = CardState::AllIn;
        m_BetNumber = m_Money;
        m_Money = 0;
        m_TotalBetNumber += m_BetNumber;
    }
    void PokerPlayer::Raise(const uint64_t& num){
        m_CardState = CardState::Raise;
        m_BetNumber = num;
        m_Money -= num;
        m_TotalBetNumber += num;
    }

void PokerPlayer::Reset(){
    m_BetNumber = 0;
    m_TotalBetNumber = 0;
    m_CardState = CardState::Play;
}


    void PokerPlayer::Calculate(const std::vector<ParsedCard>& commonCards){
        SERVER_ASSERT(m_HandCards.size() == 2 && commonCards.size() == 5 , "Unable to calculate with incomplete handcards and comoon cards");
        std::vector<ParsedCard> totalCards;
        totalCards.insert(totalCards.end() , m_HandCards.begin() , m_HandCards.end());
        totalCards.insert(totalCards.end(), commonCards.begin() , commonCards.end());
        CardIdentification maxCards;
        std::vector<ParsedCard> temCards;
        temCards.reserve(5);
        for(int i = 0 ; i < totalCards.size() ; i++){
            for(int j = i + 1 ; j < totalCards.size() ; j++){
                for(int k = 0 ; k < totalCards.size() ; k++){
                    if(k == i || k == j){
                        continue;
                    }
                    temCards.emplace_back(totalCards[k]);
                    if(i == 0 && j == 1){
                        maxCards = Card::CalCardID(temCards);
                        continue;
                    }
                    else{
                        CardIdentification temID = Card::CalCardID(temCards);
                        if(Card::CompareTwoSets(maxCards, temID) == 2){
                            maxCards = temID;
                        }
                    }
                }
            }
        }
        m_MaxCards = maxCards;
    }

void PokerPlayer::Bet(const uint64_t& num){
    auto pot = GetPot();
    SERVER_ASSERT(pot != nullptr , "invalid pot when player has to bet");
    if(num < m_Money){
        m_BetNumber = num;
        m_TotalBetNumber += num;
        m_Money -= num;
        pot->AddValue(num);
        SetCardState(CardState::Bet);
    }
    else {
        m_BetNumber = m_Money;
        m_TotalBetNumber += m_Money;
        pot->AddValue(m_Money);
        m_Money = 0;
        SetCardState(CardState::AllIn);
    }
}

Ref<Pot> PokerPlayer::GetPot(){
    if(m_Room != nullptr){
        return m_Room->GetPot();
    }
    
    return nullptr;
}



}


