//
//  Pot.cpp
//  Poker
//
//  Created by 左斯诚 on 2025/7/24.
//
#include "Spch.h"
#include "Pot.h"
#include "core/PokerPlayer.h"



namespace Tso {


namespace Utils{

std::vector<Ref<PokerPlayer>> CalWinner(const std::vector<Ref<PokerPlayer>>& players) {
    if (players.empty()) {
        return {}; // 如果没有玩家，则返回空向量
    }

    std::vector<Ref<PokerPlayer>> winners;
    winners.reserve(players.size());
    CardIdentification winnerID = players[0]->GetMaxCardID(); // 初始化为第一个玩家的手牌
    winners.push_back(players[0]);

    for (size_t i = 1; i < players.size(); ++i) {
        CardIdentification playerID = players[i]->GetMaxCardID();
        int res = Card::CompareTwoSets(winnerID, playerID);
        if (res == 0) {
            // 平局
            winners.push_back(players[i]);
        } else if (res == 2) {
            // 当前玩家获胜
            winners.clear();
            winners.push_back(players[i]);
            winnerID = playerID;  // 更新获胜者的手牌
        }
        // else res == 1，当前赢家获胜，什么都不做
    }
    return winners;
}

}


//void Pot::Dispatch(const std::vector<Ref<PokerPlayer>>& players ,const std::vector<Ref<PokerPlayer>>& allInPlayers){
////        if(players.size() == 1 && allInPlayers.empty()){
////            // for most common situation that one normal person left. take ur money son.
////            players[0]->AddMoney(m_PotValue);
////            return;
////        }
//    // all in winner
//    if(players.empty() && allInPlayers.empty()){
//        
//    }
//    auto allinWinners = Utils::CalWinner(allInPlayers);
//    auto normalWinners = Utils::CalWinner(players);
//    if(allinWinners.empty()){
//        uint64_t rewards = m_PotValue / normalWinners.size();
//        for(auto& winner : normalWinners){
//            winner->AddMoney(rewards);
//        }
//    }
//    else if(normalWinners.empty()){
//        for(auto& allInWinner : allinWinners){
//            uint64_t rewards = allInWinner->GetTotalNumber() * 2 < m_PotValue / allinWinners.size() ?
//                                allInWinner->GetTotalNumber() * 2 : m_PotValue / allinWinners.size();
//            SERVER_ASSERT(rewards < m_PotValue , "pot value is not enough");
//            allInWinner->AddMoney(rewards);
//            m_PotValue -= rewards;
//        }
//    }
//    else{
//        auto normalID = normalWinners[0]->GetMaxCardID();
//        auto allInID = allinWinners[0]->GetMaxCardID();
//        int res = Card::CompareTwoSets(normalID, allInID);
//    }
//    
//    }

    void Pot::Dispatch(std::vector<Ref<PokerPlayer>>& players, std::vector<Ref<PokerPlayer>>& allInPlayers , const std::vector<Ref<PokerPlayer>>& fullPlayers) {
        // 1. 准备工作：合并所有玩家，并且按All-in金额排序
        std::vector<Ref<PokerPlayer>> allPlayers = players;
        allPlayers.insert(allPlayers.end(), allInPlayers.begin(), allInPlayers.end()); // 合并
        if(allPlayers.empty()){
            // all players fold
            for(auto& player : fullPlayers){
                player->AddMoney(player->GetTotalNumber());
            }
            m_PotValue = 0;
            return;
        }
        // Sort players by All-in amount (for creating side pots correctly)
        std::sort(allInPlayers.begin(), allInPlayers.end(), [](const Ref<PokerPlayer>& a, const Ref<PokerPlayer>& b) {
            return a->GetTotalNumber() < b->GetTotalNumber();
        });

        // 2. 计算边池和主池
        uint64_t currentPotValue = m_PotValue; // 记录当前的奖池总额，用于在计算边池时减少
        std::vector<std::pair<std::vector<Ref<PokerPlayer>>, uint64_t>> sidePots; // (参与玩家, 边池金额)

        for (size_t i = 0; i < allInPlayers.size(); ++i) {
            Ref<PokerPlayer> allInPlayer = allInPlayers[i];
            if (allInPlayer->GetTotalNumber() == 0) continue; // Skip players with no betting
            
            // 计算该All-in玩家的边池
            uint64_t sidePotAmount = 0;
            std::vector<Ref<PokerPlayer>> potParticipants;
            potParticipants.push_back(allInPlayer); // 必须包含All-in玩家

            for (size_t j = 0; j < allPlayers.size(); ++j) {
                if(std::find(allInPlayers.begin(), allInPlayers.end(), allPlayers[j]) != allInPlayers.end()) continue;
                Ref<PokerPlayer> otherPlayer = allPlayers[j];

                // 只有下注额度比当前All-in玩家大或者相等的玩家才能参与该边池
                if (otherPlayer->GetTotalNumber() > allInPlayer->GetTotalNumber()) {
                    sidePotAmount += std::min(otherPlayer->GetTotalNumber() , allInPlayer->GetTotalNumber()) ; // 只计算超过All-in的钱
                    potParticipants.push_back(otherPlayer);
                }
            }
            sidePotAmount = std::min(sidePotAmount, currentPotValue);
            if(sidePotAmount > 0) {
                sidePots.push_back({ potParticipants, sidePotAmount });
                currentPotValue -= sidePotAmount;
            }
        }

        // 3. 计算主池 (剩余的钱)
        std::vector<Ref<PokerPlayer>> mainPotParticipants = allPlayers;
        uint64_t mainPotValue = currentPotValue;

        // 4. 分配奖金
        // 主池
        if (mainPotValue > 0) {
            std::vector<Ref<PokerPlayer>> mainPotWinners = Utils::CalWinner(mainPotParticipants);
            uint64_t rewards = mainPotValue / mainPotWinners.size();
            for (auto& winner : mainPotWinners) {
                winner->AddMoney(rewards);
            }
        }
        // 边池
        for (auto& sidePot : sidePots) {
            std::vector<Ref<PokerPlayer>> sidePotWinners = Utils::CalWinner(sidePot.first);
            uint64_t rewards = sidePot.second / sidePotWinners.size();
            for (auto& winner : sidePotWinners) {
                winner->AddMoney(rewards);
            }
        }
        //ResetBettingAmount();
        m_PotValue = 0;
    }

    void Pot::AddValue(const uint64_t &num){
        m_PotValue += num;
    }
}
