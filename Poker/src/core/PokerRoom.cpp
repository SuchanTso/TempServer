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
#include "Server/network/NetworkEngine.h"
#include "TexasHoldemModule.h"
#define PLAYER_ACTION_TIMEOUT 30
namespace Tso {

int PokerRoom::GetValidPlayerNum(){
    return GetPokerPlayerList().size();
}

int PokerRoom::GetOntablePlayerNum(){
    int res = 0;
    for(auto player : m_PlayerList){
        if(player->GetCardState() != CardState::Fold){
            res++;
        }
    }
    return res;
}

PokerRoom::PokerRoom(const RoomID& roomId , const uint8_t& maxPlayer):Room(roomId) , m_State(PokerRoomState::Init){
    m_MaxPlayerNum = maxPlayer;
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
        SERVER_INFO("PokerPlayer {} added to PokerRoom {}.", playerID, m_ID);
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
    SERVER_INFO("PokerPlayer {} removed from PokerRoom {}.", playerID, m_ID);
}

void PokerRoom::SetState(PokerRoomState state){
    if(m_State != state){
        m_EventQueue.push(state);
        SERVER_INFO("Room[{}] changed state to {}" , GetRoomID() , (int)m_State);
        m_State = state;
        m_LogicConfig.m_BetNum = 0;
    }
}




bool PokerRoom::SetDealer(){
    if (m_PlayerList.size() < 2) return false;
    int playerNum = m_PlayerList.size();

    int oldDealerIdx = -1;
    for(int i = 0; i < playerNum; ++i){
        m_PlayerList[i]->SetPlayerState(RoleState::Player); // 先重置所有人角色
        if(m_PlayerList[i]->GetPlayerState() == RoleState::Dealer){
            oldDealerIdx = i;
        }
    }

    m_LogicConfig.m_DealerIdx = (oldDealerIdx + 1) % playerNum;
    
    // 特殊处理两人桌 (Heads-up)
    if (playerNum == 2) {
        m_LogicConfig.m_SmallBlinderIdx = m_LogicConfig.m_DealerIdx;
        m_LogicConfig.m_GreatBlinderIdx = (m_LogicConfig.m_DealerIdx + 1) % playerNum;
    } else {
        m_LogicConfig.m_SmallBlinderIdx = (m_LogicConfig.m_DealerIdx + 1) % playerNum;
        m_LogicConfig.m_GreatBlinderIdx = (m_LogicConfig.m_DealerIdx + 2) % playerNum;
    }

    m_PlayerList[m_LogicConfig.m_DealerIdx]->SetPlayerState(RoleState::Dealer);
    m_PlayerList[m_LogicConfig.m_SmallBlinderIdx]->SetPlayerState(RoleState::SmallBlinder);
    m_PlayerList[m_LogicConfig.m_GreatBlinderIdx]->SetPlayerState(RoleState::GreateBlinder);
    
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
    for(auto& player : m_PlayerList){
        auto cardState = player->GetCardState();
        if(cardState == CardState::Bet){
            player->SetCardState(CardState::Play);
        }
        if(!bet){
            player->Reset();
        }
    }
}

void PokerRoom::ResetPlayerBetState(){
    for(auto& player : m_PlayerList){
        if(player->GetCardState() != CardState::Fold && player->GetCardState() != CardState::AllIn){
            player->SetCardState(CardState::Play);
        }
        player->ResetBetTurn();
    }
}

void PokerRoom::NotifyBlinderBet(){
    SERVER_INFO("notifing game start");
    ByteStream ntf;
    ntf.write<uint8_t>(GetRoomID());
    const auto& playerNum = GetValidPlayerNum();
    ntf.write<uint8_t>(playerNum);
    for (const auto& p : m_PlayerList) {
        ntf.writeString(p->GetPlayerIDstr());
        ntf.writeString(p->GetName());
        ntf.writeString(std::to_string(p->GetCurrentMoney()));
        ntf.write<uint8_t>((uint8_t)p->GetPlayerState());
        ntf.write<uint8_t>(p->GetSeatIndex());
    }
    BroadcastToRoom(MakeProtocolID(Modules::TexasHoldem::S2C_GameStartInfoNtf), ntf);
}

void PokerRoom::NotifyHandCard(){
    SERVER_INFO("notifing private cards");
    for (const auto& p : GetPokerPlayerList()) {
        ByteStream privateNtf;
        const auto& handCards = p->GetHandCards();
        if (handCards.size() >= 2) {
            privateNtf.write<uint8_t>(uint8_t(handCards[0].type));
            privateNtf.write<uint8_t>(handCards[0].point);
            privateNtf.write<uint8_t>(uint8_t(handCards[1].type));
            privateNtf.write<uint8_t>(handCards[1].point);
            NetWorkEngine::GetEngine()->SendToClient(p->GetNetID(), MakeProtocolID(Modules::TexasHoldem::S2C_DealPrivateCardsNtf), privateNtf, true);
        }
    }
}

void PokerRoom::NotifyCommonCards(){
    ByteStream ntf;
    const auto& commonCards = GetCommonCards();
    SERVER_INFO("Notifying common cards");
    ntf.write<uint8_t>(commonCards.size());
    for(const auto& card : commonCards) {
        ntf.write<uint8_t>(uint8_t(card.type));
        ntf.write<uint8_t>(card.point);
    }
    BroadcastToRoom(MakeProtocolID(Modules::TexasHoldem::S2C_DealPublicCardsNtf), ntf);
}

void PokerRoom::NotifyBet(){
    int betIndex = GetBetIndex();
    const auto& playerList = GetPokerPlayerList();
    
    if (betIndex >= 0 && betIndex < playerList.size()) {
        auto nextPlayer = playerList[betIndex];
        m_LogicConfig.m_NotifyBetIndex = betIndex;
        ByteStream turnNtf;
        turnNtf.writeString(nextPlayer->GetPlayerIDstr());
        turnNtf.writeString(std::to_string(GetCurrentBet() * 2));
        turnNtf.writeString(std::to_string(GetCurrentBet()));
        SERVER_INFO("It's player at {}'s turn, notifying" , betIndex);
        BroadcastToRoom(MakeProtocolID(Modules::TexasHoldem::S2C_TurnToActNtf), turnNtf);
    }
}

void PokerRoom::NotifyGameResult(const std::vector<WinnerInfo>& winnerInfo) {
     ByteStream ntf;
    ntf.write<uint8_t>(winnerInfo.size());
    bool showCards = GetCommonCards().size() == 5 && GetOntablePlayerNum() > 1;
    ntf.write<uint8_t>(showCards ? 1 : 0);
    for(auto& winners : winnerInfo){
        ntf.write<uint8_t>(winners.potWinners.size());
        ntf.writeString(std::to_string(winners.rewards));
        for(auto& potPlayer : winners.potWinners){
            ntf.writeString(std::to_string(potPlayer.player->GetPlayerID()));
        }
    }
    if(showCards){
        ntf.write<uint8_t>(GetOntablePlayerNum());
        for(auto& player : m_PlayerList){
            if(player->GetCardState() != CardState::Fold){
                ntf.writeString(player->GetPlayerIDstr());
                auto handCards = player->GetHandCards();
                ntf.write<uint8_t>(uint8_t(handCards[0].type));
                ntf.write<uint8_t>(handCards[0].point);
                ntf.write<uint8_t>(uint8_t(handCards[1].type));
                ntf.write<uint8_t>(handCards[1].point);
            }
        }
    }
    ntf.write<uint8_t>(m_PlayerList.size());
    for(auto& player : m_PlayerList){
        //notify chips
        ntf.writeString(player->GetPlayerIDstr());
        ntf.writeString(std::to_string(player->GetCurrentMoney()));
        
    }
     BroadcastToRoom(MakeProtocolID(Modules::TexasHoldem::S2C_GameResultNtf), ntf);
}

void PokerRoom::ProcessGameState() {
    switch (m_State) {
        case PokerRoomState::BlinderBet:{
            m_PlayerList[m_LogicConfig.m_SmallBlinderIdx]->Bet(m_GameConfig.m_SmallBet);
            m_PlayerList[m_LogicConfig.m_GreatBlinderIdx]->Bet(m_GameConfig.m_GreatBet);
            m_LogicConfig.m_BetNum = m_GameConfig.m_GreatBet;
            
            NotifyBlinderBet();
            SetState(PokerRoomState::HandCard);
            ProcessGameState(); // 立即进入下一状态
            break;
        }
        case PokerRoomState::HandCard:{
            for(auto& player : m_PlayerList){
                player->HandCard({m_Card->GenerateOneParsedCard(), m_Card->GenerateOneParsedCard()});
            }
            NotifyHandCard();
            SetState(PokerRoomState::HandBet);
            ProcessGameState(); // 立即进入下一状态
            break;
        }
        case PokerRoomState::HandBet:{
            m_LogicConfig.m_BetStartIndex = (m_LogicConfig.m_GreatBlinderIdx + 1) % m_PlayerList.size();
            m_LogicConfig.m_BetIndex = m_LogicConfig.m_BetStartIndex;
            CheckTurn(); // 状态机暂停，等待玩家操作
            break;
        }
        case PokerRoomState::ThreeCards:{
            m_LogicConfig.m_CommonCards.push_back(m_Card->GenerateOneParsedCard());
            m_LogicConfig.m_CommonCards.push_back(m_Card->GenerateOneParsedCard());
            m_LogicConfig.m_CommonCards.push_back(m_Card->GenerateOneParsedCard());
            NotifyCommonCards();
            SetState(PokerRoomState::ThreeBet);
            ProcessGameState();
            break;
        }
         case PokerRoomState::ThreeBet:
         case PokerRoomState::RollBet:
         case PokerRoomState::RiverBet:{
            m_LogicConfig.m_BetStartIndex = (m_LogicConfig.m_DealerIdx + 1) % m_PlayerList.size();
            m_LogicConfig.m_BetIndex = m_LogicConfig.m_BetStartIndex;
            ResetPlayerBetState();
            CheckTurn();
            break;
         }
        case PokerRoomState::RollCard: {
            m_LogicConfig.m_CommonCards.push_back(m_Card->GenerateOneParsedCard());
            NotifyCommonCards();
            SetState(PokerRoomState::RollBet);
            ProcessGameState();
            break;
        }
        case PokerRoomState::RiverCard:{
            m_LogicConfig.m_CommonCards.push_back(m_Card->GenerateOneParsedCard());
            NotifyCommonCards();
            SetState(PokerRoomState::RiverBet);
            ProcessGameState();
            break;
        }
        case PokerRoomState::PotCalculation:{
            std::vector<Ref<PokerPlayer>> normalPlayers, allinPlayers;
            CollectCalculatePlayers(normalPlayers, allinPlayers);
            SERVER_ASSERT(m_Pot , "Didn't initialize a pot for the game");
            auto winners = m_Pot->Dispatch(normalPlayers, allinPlayers, m_PlayerList);
            NotifyGameResult(winners);
            SetState(PokerRoomState::CheckPlayer);
            ProcessGameState();
            break;
        }
        case PokerRoomState::CheckPlayer:{
            AddObserverPlayer();
            ResetForNewRound();
            SetState(PokerRoomState::Start);
            m_IsGameInProgress = false;
            // 游戏结束，流程停止，等待下一轮被 TakeOverGame -> StartNewGame 触发
            break;
        }
        default: break;
    }
}



void PokerRoom::BroadcastToRoom(uint16_t protocolId, ByteStream& ntf) {
    const auto& playerList = GetPokerPlayerList(); // 使用 PokerPlayer 列表
    for (const auto& player : playerList) {
        Tso::NetWorkEngine::GetEngine()->SendToClient(player->GetNetID(), protocolId, ntf,true);
    }
}

void PokerRoom::CollectCalculatePlayers(std::vector<Ref<PokerPlayer>>& normalPlayers , std::vector<Ref<PokerPlayer>>& allinPlayers){
    for(auto& player : m_PlayerList){
        auto nowCardState = player->GetCardState();
        switch (nowCardState) {
            case CardState::AllIn:{
                player->Calculate(GetCommonCards());
                allinPlayers.push_back(player);
                break;
            }
            case CardState::Play:
            case CardState::Bet:
            case CardState::Raise: { // [FIXED] Raise 状态的玩家也应该被计算
                player->Calculate(GetCommonCards());
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
    while(!m_Observers.empty() && m_PlayerList.size() < m_GameConfig.m_MaxPlayerNumber){
        auto& player = m_Observers.front();
        m_PlayerList.emplace_back(player);
        player->SetSeatIndex(m_PlayerList.size());
        m_PlayerMap[player->GetPlayerID()] = player;
        m_Observers.pop();
    }
}

bool PokerRoom::AddMoney(){
    bool res = true;
    for(auto& player : m_PlayerList){
        if(player->GetCurrentMoney() < m_GameConfig.m_GreatBet){
            //TODO: notify client to add money
            res = false;
        }
    }
    return res;
}

void PokerRoom::StartNewGame() {
    InitConfig();
    SetState(PokerRoomState::CheckPlayer);
    ProcessGameState();
    if (m_State != PokerRoomState::Start) {
        SERVER_WARN("Attempted to start a new game from a non-start state.");
        return;
    }

    m_IsGameInProgress = true;
    m_Card->Shuffle();
    m_Pot = CreateRef<Pot>();
    SERVER_INFO("Start a New Game");
    if (SetDealer()) {
        SetState(PokerRoomState::BlinderBet);
        ProcessGameState(); // 开始处理游戏流程
    } else {
        // 人数不够等错误
        m_IsGameInProgress = false;
        SetState(PokerRoomState::Start);
    }
}

void PokerRoom::ResetForNewRound(){
    for(auto& player : m_PlayerList){
        player->Reset();
        player->SetReady(false);
    }
//    m_LogicConfig = {}; // 重置逻辑配置
    m_LogicConfig.m_CommonCards.clear();
    m_IsGameInProgress = false;
    m_Pot = nullptr;
}

void PokerRoom::OnUpdate(float ts){
    
    // OnUpdate 现在不再驱动核心游戏流程。
    // 它的主要职责是处理超时等与时间相关的逻辑。

    // 示例：处理玩家行动超时
    if (m_State >= PokerRoomState::HandBet && m_State <= PokerRoomState::RiverBet) {
        if (m_IsWaitingForPlayer) {
            m_ActionTimer += ts;
            if (m_ActionTimer >= PLAYER_ACTION_TIMEOUT) { // 假设 PLAYER_ACTION_TIMEOUT = 30.0f
                SERVER_INFO("Player at index {} timed out.", m_LogicConfig.m_BetIndex);
                // 强制当前玩家弃牌
                auto& currentPlayer = m_PlayerList[m_LogicConfig.m_BetIndex];
                currentPlayer->Fold();

                // 广播弃牌动作
                ByteStream ntf;
                ntf.writeString(std::to_string(currentPlayer->GetPlayerID()));
                ntf.write<uint8_t>((uint8_t)Modules::TexasHoldem::PlayerActionType::Fold);
                BroadcastToRoom(MakeProtocolID(Modules::TexasHoldem::S2C_PlayerActionNtf), ntf);
                
                // 触发下一轮
                OnPlayerAction();
            }
        }
    }
    
}



    void PokerRoom::CheckTurn() {
        m_IsWaitingForPlayer = false; // 先重置等待状态

        if (GetOntablePlayerNum() <= 1) {
            SetState(PokerRoomState::PotCalculation);
            ProcessGameState();
            return;
        }
        
        bool roundOver = true;
        int activePlayerCount = 0;
        for (const auto& p : m_PlayerList) {
            if (p->GetCardState() != CardState::Fold && p->GetCardState() != CardState::AllIn) {
                activePlayerCount++;
                if (p->GetBetNumber() != m_LogicConfig.m_BetNum || p->GetCardState() == CardState::Play) {
                    roundOver = false;
                }
            }
        }
        if (activePlayerCount <= 1) roundOver = true;

        if (roundOver) {
            SetState((PokerRoomState)((int)m_State + 1));
            ProcessGameState();
            return;
        }
        
        int checkedCount = 0;
        while(checkedCount < m_PlayerList.size()) {
            auto& currentPlayer = m_PlayerList[m_LogicConfig.m_BetIndex];
            if (currentPlayer->GetCardState() == CardState::Fold || currentPlayer->GetCardState() == CardState::AllIn) {
                m_LogicConfig.m_BetIndex = (m_LogicConfig.m_BetIndex + 1) % m_PlayerList.size();
                checkedCount++;
                continue;
            }
            
            // 找到了需要行动的玩家
            NotifyBet();
            m_IsWaitingForPlayer = true; // 开始等待玩家操作
            m_ActionTimer = 0.0f; // 重置计时器
            return; // **暂停状态机**
        }
    }


    // [新增] 玩家行动后被TexasHoldemModule调用
    void PokerRoom::OnPlayerAction() {
        m_IsWaitingForPlayer = false; // 收到操作，停止等待
        m_ActionTimer = 0.0f;

        auto& lastActor = m_PlayerList[m_LogicConfig.m_BetIndex];
        if (lastActor->GetCardState() == CardState::Raise) {
            m_LogicConfig.m_BetNum = lastActor->GetBetNumber();
            for(auto& p : m_PlayerList) {
                if (p != lastActor && p->GetCardState() != CardState::Fold && p->GetCardState() != CardState::AllIn) {
                    p->SetCardState(CardState::Play);
                }
            }
            lastActor->SetCardState(CardState::Bet);
        }
        
        m_LogicConfig.m_BetIndex = (m_LogicConfig.m_BetIndex + 1) % m_PlayerList.size();
        
        CheckTurn(); // **驱动状态机进入下一步**
    }

    void PokerRoom::Bet(){
        int nonFoldPlayerNum = GetOntablePlayerNum();
        if(nonFoldPlayerNum <= 1){
//            m_State = PokerRoomState::PotCalculation;
            SetState(PokerRoomState::PotCalculation);
            return;
        }

        // 确保 BetIndex 有效
        if (m_LogicConfig.m_BetIndex < 0 || m_LogicConfig.m_BetIndex >= m_PlayerList.size()) {
//            m_State = (PokerRoomState)(int(m_State) + 1); // 如果无效，直接进入下一状态
            SERVER_ERROR("Invalid bet index");
            
            return;
        }

        auto player = m_PlayerList[m_LogicConfig.m_BetIndex];
        CardState nowPlayerCardState = player->GetCardState();

        if(nowPlayerCardState == CardState::Play){
            // 等待客户端响应，TexasHoldemModule 会处理
        }
        else if(nowPlayerCardState == CardState::Fold || nowPlayerCardState == CardState::AllIn){
            m_LogicConfig.m_BetIndex = (m_LogicConfig.m_BetIndex + 1) % m_PlayerList.size();
        }
        else if(nowPlayerCardState == CardState::Bet){
            if(player->GetBetNumber() == m_LogicConfig.m_BetNum){
                m_LogicConfig.m_BetIndex = (m_LogicConfig.m_BetIndex + 1) % m_PlayerList.size();
            }
            else{
                SERVER_ASSERT(false , "not allowing bet less than current number");
            }
        }
        else if(nowPlayerCardState == CardState::Raise){
            m_LogicConfig.m_BetNum = player->GetBetNumber();
            for(auto& p: m_PlayerList){
                // 只有自己是 Raise，其他人变回 Play 等待行动
                if(p->GetPlayerID() != player->GetPlayerID() && p->GetCardState() != CardState::Fold && p->GetCardState() != CardState::AllIn){
                    p->SetCardState(CardState::Play);
                }
            }
            player->SetCardState(CardState::Bet); // Raise 动作完成后，状态变为已下注 (Bet)
            m_LogicConfig.m_BetStartIndex = m_LogicConfig.m_BetIndex;
            m_LogicConfig.m_BetIndex = (m_LogicConfig.m_BetIndex + 1) % m_PlayerList.size();
        }
        
        NotifyBet();
        
        // 检查一轮下注是否结束
        bool roundEnds = true;
        for(auto& player : m_PlayerList){
            if(player->GetCardState() == CardState::Play){
                roundEnds = false;
                break;
            }
        }
        if(roundEnds){
//            m_State = (PokerRoomState)(int(m_State) + 1);
            m_LogicConfig.m_NotifyBetIndex = -1;
            SetState((PokerRoomState)(int(m_State) + 1));
            
        }
    }
}
