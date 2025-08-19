//
//  PokerRoom.cpp
//  Poker
//
//  Created by 左斯诚 on 2025/7/20.
//

#include "SPch.h"
#include "PokerRoom.h"
#include "PokerPlayer.h"
#include "Server/Math/Random.h"
#include "logic/Card.h"


namespace Tso {

int PokerRoom::GetValidPlayerNum(){
    return m_PokerPlayerList.size();
}

int PokerRoom::GetOntablePlayerNum(){
    int res = 0;
    for(auto player : m_PokerPlayerList){
        if(player->GetCardState() != CardState::Fold){
            res++;
        }
    }
    return res;
}

PokerRoom::PokerRoom(const RoomID& roomId , const uint8_t& maxPlayer):Room(roomId) , m_State(PokerRoomState::Init){
    m_GameConfig.m_MaxPlayerNumber = maxPlayer;
}


void PokerRoom::OnAddPlayer(const PlayerID& playerID,  Ref<Player> player){
    //TODO: read player info from file
//    Ref<PokerPlayer> pokerPlayer = std::dynamic_pointer_cast<PokerPlayer>(player);
//    pokerPlayer->SetRoom(this);
//    m_Observers.push(pokerPlayer);
    // 将通用的 Player 指针转换为具体的 PokerPlayer 指针
    Ref<PokerPlayer> pokerPlayer = std::dynamic_pointer_cast<PokerPlayer>(player);
    if (pokerPlayer) {
        m_PokerPlayerList.push_back(pokerPlayer);
        SERVER_INFO("PokerPlayer %u added to PokerRoom %u.", playerID, m_ID);
        // 你之前的 Observer 逻辑可以放在这里
         pokerPlayer->SetRoom(this);
         m_Observers.push(pokerPlayer);
    } else {
        // 尝试从一个非 PokerPlayer 的基类 Player 创建，这通常不应该发生
        // 如果你的设计允许，可以这里 new 一个 PokerPlayer
        SERVER_ERROR("Failed to cast Player to PokerPlayer. This shouldn't happen if factory is correct.");
    }
}

// [IMPLEMENTED] 实现基类接口
void PokerRoom::OnRemovePlayer(const PlayerID& playerID) {
    m_PokerPlayerList.erase(std::remove_if(m_PokerPlayerList.begin(), m_PokerPlayerList.end(),
        [playerID](const Ref<PokerPlayer>& p){ return p->GetPlayerID() == playerID; }), m_PokerPlayerList.end());
    SERVER_INFO("PokerPlayer %u removed from PokerRoom %u.", playerID, m_ID);
}



bool PokerRoom::SetDealer(){
    if (m_PokerPlayerList.empty()) return false;

    int dealerIdx = -1;
    for(int i = 0 ; i < m_PokerPlayerList.size() ; i++){
        if(m_PokerPlayerList[i]->GetPlayerState() == RoleState::Dealer){
            dealerIdx = i;
            m_PokerPlayerList[i]->SetPlayerState(RoleState::Player); // [FIXED] 旧庄家变回普通玩家
            break; // [FIXED] 找到后就跳出
        }
    }
    
    if(dealerIdx == -1){
        dealerIdx = Random::RandomInt(0, m_PokerPlayerList.size() - 1);
    }
    
    int playerNum = GetValidPlayerNum();
    m_LogicConfig.m_DealerIdx = (dealerIdx + 1) % playerNum;
    m_LogicConfig.m_SmallBlinderIdx = (dealerIdx + 2) % playerNum;
    m_LogicConfig.m_GreatBlinderIdx = (dealerIdx + 3) % playerNum;
    
    m_PokerPlayerList[m_LogicConfig.m_DealerIdx]->SetPlayerState(RoleState::Dealer);
    m_PokerPlayerList[m_LogicConfig.m_SmallBlinderIdx]->SetPlayerState(RoleState::SmallBlinder);
    m_PokerPlayerList[m_LogicConfig.m_GreatBlinderIdx]->SetPlayerState(RoleState::GreateBlinder);

    return true;
}

    void PokerRoom::InitConfig(){
        //TODO: make this setting from client
        m_GameConfig.m_SmallBet = 100;
        m_GameConfig.m_GreatBet = 200;
        m_GameConfig.m_CardConfig = "pokerParse.yaml";
        m_Card = CreateRef<Card>(52 , m_GameConfig.m_CardConfig);
    }

void PokerRoom::ResetPlayerBetState(bool bet){
    for(auto& player : m_PokerPlayerList){
        auto cardState = player->GetCardState();
        if(cardState == CardState::Bet){
            player->SetCardState(CardState::Play);
        }
        if(!bet){
            player->Reset();
        }
    }
}

void PokerRoom::CollectCalculatePlayers(std::vector<Ref<PokerPlayer>>& normalPlayers , std::vector<Ref<PokerPlayer>>& allinPlayers){
    for(auto& player : m_PokerPlayerList){
        auto nowCardState = player->GetCardState();
        switch (nowCardState) {
            case CardState::AllIn:{
                allinPlayers.push_back(player);
                break;
            }
            case CardState::Play:
            case CardState::Bet:
            case CardState::Raise: { // [FIXED] Raise 状态的玩家也应该被计算
                normalPlayers.push_back(player);
                break;
            }
            case CardState::Fold: {
                // Folded players are ignored
                break;
            }
            default:{
                SERVER_ASSERT(false , "impossible card state when collecting");
                break;
            }
        }
    }
}

void PokerRoom::AddObserverPlayer(){
    while(!m_Observers.empty() && m_PokerPlayerList.size() < m_GameConfig.m_MaxPlayerNumber){
        auto& player = m_Observers.front();
        m_PokerPlayerList.emplace_back(player);
        // [MODIFIED] 同时也要更新基类的列表以保持一致性
        m_PlayerList.emplace_back(player);
        m_PlayerMap[player->GetPlayerID()] = player;
        m_Observers.pop();
    }
}

bool PokerRoom::AddMoney(){
    bool res = true;
    for(auto& player : m_PokerPlayerList){
        if(player->GetCurrentMoney() < m_GameConfig.m_GreatBet){
            //TODO: notify client to add money
            res = false;
        }
    }
    return res;
}



    void PokerRoom::OnUpdate(float ts){
        PokerRoomState oldState = m_State;
        switch (m_State) {
            case PokerRoomState::Unknown:{
                SERVER_ERROR("There is something wrong");
                break;
            }
            case PokerRoomState::Init:{
                InitConfig();
                m_State = PokerRoomState::Start;
                break; // [FIXED] 添加 break
            }
            case PokerRoomState::Start:{
                if(GetValidPlayerNum() > 2){ // [MODIFIED] 通常是2人开始
                    m_State = PokerRoomState::SetDealer;
                    m_Card->Shuffle();
                }
                // else: 等待更多玩家
                break; // [FIXED] 添加 break
            }
            case PokerRoomState::SetDealer: {
                if(SetDealer()){
                    m_State = PokerRoomState::BlinderBet;
                }
                break;
            }
            case PokerRoomState::BlinderBet:{
                if(m_LogicConfig.m_SmallBlinderIdx >= 0 && m_LogicConfig.m_SmallBlinderIdx < m_PokerPlayerList.size()){
                    m_PokerPlayerList[m_LogicConfig.m_SmallBlinderIdx]->Bet(m_GameConfig.m_SmallBet);
                } else {
                    SERVER_ERROR("wrong small bet index:{} , reset Dealer" , m_LogicConfig.m_SmallBlinderIdx);
                    m_State = PokerRoomState::SetDealer;
                    break;
                }
                if(m_LogicConfig.m_GreatBlinderIdx >= 0 && m_LogicConfig.m_GreatBlinderIdx < m_PokerPlayerList.size()){
                    m_PokerPlayerList[m_LogicConfig.m_GreatBlinderIdx]->Bet(m_GameConfig.m_GreatBet);
                } else {
                    SERVER_ERROR("wrong great bet index:{} , reset Dealer" , m_LogicConfig.m_GreatBlinderIdx);
                    m_PokerPlayerList[m_LogicConfig.m_SmallBlinderIdx]->AddMoney(m_GameConfig.m_SmallBet); // [FIXED] 返还小盲注
                    m_State = PokerRoomState::SetDealer;
                    break;
                }
                m_State = PokerRoomState::HandCard;
                break;
            }
            case PokerRoomState::HandCard:{
                int smallBlinderIdx = m_LogicConfig.m_SmallBlinderIdx;
                for(int i = 0 ; i < m_PokerPlayerList.size() ; i++){
                    int index = (smallBlinderIdx + i) % m_PokerPlayerList.size();
                    std::vector<ParsedCard> cards = {m_Card->GenerateOneParsedCard() , m_Card->GenerateOneParsedCard()};
                    // [MODIFIED] 将牌发给玩家
                    m_PokerPlayerList[index]->HandCard(cards);
                }
                m_State = PokerRoomState::HandBet;
                break;
            }
            // [MODIFIED] 修正 switch fall-through 逻辑
            case PokerRoomState::HandBet:{
                if(m_LogicConfig.m_BetStartIndex < 0){
                    m_LogicConfig.m_BetStartIndex = (m_LogicConfig.m_GreatBlinderIdx + 1) % m_PokerPlayerList.size();
                    m_LogicConfig.m_BetIndex = m_LogicConfig.m_BetStartIndex;
                    ResetPlayerBetState(true);
                }
                Bet(); // Bet 里面包含了状态切换的逻辑
                break;
            }
            case PokerRoomState::ThreeBet:
            case PokerRoomState::RollBet:
            case PokerRoomState::RiverBet:{
                if(m_LogicConfig.m_BetStartIndex < 0){
                    m_LogicConfig.m_BetStartIndex = (m_LogicConfig.m_SmallBlinderIdx) % m_PokerPlayerList.size();
                    m_LogicConfig.m_BetIndex = m_LogicConfig.m_BetStartIndex;
                    ResetPlayerBetState(true);
                }
                Bet();
                break;
            }
            case PokerRoomState::ThreeCards:{
                std::vector<ParsedCard> temCards;
                for(int i = 0 ; i < 3 ; i++){
                    temCards.push_back(m_Card->GenerateOneParsedCard());
                }
                m_LogicConfig.m_CommonCards.insert(m_LogicConfig.m_CommonCards.end(), temCards.begin() , temCards.end());
                m_State = PokerRoomState::ThreeBet;
                break;
            }
            case PokerRoomState::RollCard: {
                std::vector<ParsedCard> temCards = {m_Card->GenerateOneParsedCard()};
                m_LogicConfig.m_CommonCards.insert(m_LogicConfig.m_CommonCards.end(), temCards.begin() , temCards.end());
                m_State = PokerRoomState::RollBet;
                break;
            }
            case PokerRoomState::RiverCard:{
                std::vector<ParsedCard> temCards = {m_Card->GenerateOneParsedCard()};
                m_LogicConfig.m_CommonCards.insert(m_LogicConfig.m_CommonCards.end(), temCards.begin() , temCards.end());
                m_State = PokerRoomState::RiverBet;
                break;
            }
            case PokerRoomState::PotCalculation:{
                std::vector<Ref<PokerPlayer>> normalPlayers;
                std::vector<Ref<PokerPlayer>> allinPlayers;
                CollectCalculatePlayers(normalPlayers , allinPlayers);
                m_Pot->Dispatch(normalPlayers, allinPlayers, m_PokerPlayerList);
                m_State = PokerRoomState::CheckPlayer; // 假设结算完进入 CheckPlayer
                break;
            }
            case PokerRoomState::CheckPlayer:{
                if(!m_CheckState){
                    AddObserverPlayer();
                    ResetPlayerBetState(false);
                    m_CheckState = true;
                }
                if(AddMoney()){
                    m_State = PokerRoomState::Start;
                }
                break;
            }
            default:
                break;
        }
        
        PokerRoomState newState = m_State;
        if (oldState != newState) {
            // 更新 IsGameInProgress 状态，供 LobbyModule 查询
            if (oldState == PokerRoomState::Start && newState > PokerRoomState::Start) {
                m_IsGameInProgress = true;
            }
            else if (newState == PokerRoomState::Start) {
                m_IsGameInProgress = false;
            }
            
            // [MODIFIED] 重置 BetStartIndex，以便下一轮下注可以正确初始化
            if ((oldState == PokerRoomState::HandBet && newState == PokerRoomState::ThreeCards) ||
                (oldState == PokerRoomState::ThreeBet && newState == PokerRoomState::RollCard) ||
                (oldState == PokerRoomState::RollBet && newState == PokerRoomState::RiverCard))
            {
                m_LogicConfig.m_BetStartIndex = -1;
                m_LogicConfig.m_BetIndex = -1;
            }
        }
    }

    void PokerRoom::Bet(){
        int nonFoldPlayerNum = GetOntablePlayerNum();
        if(nonFoldPlayerNum <= 1){
            m_State = PokerRoomState::PotCalculation;
            return;
        }

        // 确保 BetIndex 有效
        if (m_LogicConfig.m_BetIndex < 0 || m_LogicConfig.m_BetIndex >= m_PokerPlayerList.size()) {
            m_State = (PokerRoomState)(int(m_State) + 1); // 如果无效，直接进入下一状态
            return;
        }

        auto player = m_PokerPlayerList[m_LogicConfig.m_BetIndex];
        CardState nowPlayerCardState = player->GetCardState();

        if(nowPlayerCardState == CardState::Play){
            // 等待客户端响应，TexasHoldemModule 会处理
        }
        else if(nowPlayerCardState == CardState::Fold || nowPlayerCardState == CardState::AllIn){
            m_LogicConfig.m_BetIndex = (m_LogicConfig.m_BetIndex + 1) % m_PokerPlayerList.size();
        }
        else if(nowPlayerCardState == CardState::Bet){
            if(player->GetBetNumber() == m_LogicConfig.m_BetNum){
                m_LogicConfig.m_BetIndex = (m_LogicConfig.m_BetIndex + 1) % m_PokerPlayerList.size();
            }
            else{
                SERVER_ASSERT(false , "not allowing bet less than current number");
            }
        }
        else if(nowPlayerCardState == CardState::Raise){
            m_LogicConfig.m_BetNum = player->GetBetNumber();
            for(auto& p: m_PokerPlayerList){
                // 只有自己是 Raise，其他人变回 Play 等待行动
                if(p->GetPlayerID() != player->GetPlayerID() && p->GetCardState() != CardState::Fold && p->GetCardState() != CardState::AllIn){
                    p->SetCardState(CardState::Play);
                }
            }
            player->SetCardState(CardState::Bet); // Raise 动作完成后，状态变为已下注 (Bet)
            m_LogicConfig.m_BetStartIndex = m_LogicConfig.m_BetIndex;
            m_LogicConfig.m_BetIndex = (m_LogicConfig.m_BetIndex + 1) % m_PokerPlayerList.size();
        }
        
        // 检查一轮下注是否结束
        if(m_LogicConfig.m_BetIndex == m_LogicConfig.m_BetStartIndex){
            m_State = (PokerRoomState)(int(m_State) + 1);
        }
    }
}
