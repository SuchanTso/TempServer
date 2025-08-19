//
//  Card.cpp
//  Poker
//
//  Created by SuchanTso on 2025/7/18.
//
#include "SPch.h"
#include "Card.h"
#include "Server/Math/Random.h"
#include "yaml-cpp/yaml.h"
#include "LogHelper.h"

namespace Tso{

namespace Utils{
struct cardDistribution{
    uint8_t pointDistribution = 0;
    uint8_t typeDistribution = 0;
};
cardDistribution CardDistribution(const std::vector<ParsedCard>& cardSet){
    std::unordered_set<uint8_t> set;
    std::unordered_set<uint8_t> typeSet;
    for(auto card : cardSet){
        set.emplace(card.point);
        typeSet.emplace((uint8_t)card.type);
    }
    return {(uint8_t)set.size() , (uint8_t)typeSet.size()};
}

bool IsStraight(const std::vector<ParsedCard>& parsedCards , CardIdentification& cardID){
    bool res = true;
    uint8_t min = 255;
    uint8_t minSecond = 255;
    std::set<uint8_t> record;
    for(auto parsedCard : parsedCards){
        if(parsedCard.point < min){
            minSecond = min;
            min = parsedCard.point;
        }
        record.emplace(parsedCard.point);
    }
    if(minSecond == uint8_t(255)){
        for(auto parsedCard : parsedCards){
            if(parsedCard.point < minSecond && parsedCard.point != min){
                minSecond = parsedCard.point;
            }
        }
    }
    bool b1 = min == uint8_t(1);
    bool b2 = record.find(uint8_t(2)) == record.end();
    bool b3 = minSecond == uint8_t(10);
    if(min == uint8_t(1) && record.find(uint8_t(2)) == record.end() && minSecond == uint8_t(10)){
        min = minSecond;
        record.emplace((uint8_t)14);
        record.erase(uint8_t(1));
    }
    for(uint8_t i = 0 ; i < 5 && res ; i++){
        if(record.find(min + i) == record.end()) res = false;
    }
    if(res){
        for(uint8_t i = 0 ; i < 5 ; i++){
            cardID.sortedCards[4 - i] = min + i;
        }
    }
    else{
        int i = 0;
        for(auto it = record.begin() ; it != record.end() ; it++){
            cardID.sortedCards[4 - i] = *it;
            i ++;
        }
    }
    return res;
}

void SortPair(const std::vector<ParsedCard>& parsedCards , CardIdentification& cardID){
    std::set<uint8_t> record;
    uint8_t pair = 255;
    for(auto card : parsedCards){
        auto point = card.point == uint8_t(1) ? uint8_t(14) : card.point;
        if(record.find(point) != record.end()){
            pair = point;
            record.erase(point);
        }
        else{
            record.emplace(point);
        }
    }
    cardID.sortedCards[0] = cardID.sortedCards[1] = pair;
    SERVER_ASSERT(record.size() == 3 , "Unreasonable for pair remain {} elements" , record.size());
    int i = 0 ;
    for(auto it = record.begin() ; it != record.end() ; it++){
        cardID.sortedCards[4 - i] = *it;
        i++;
    }
}

void SortSitesTwoPair(const std::vector<ParsedCard>& parsedCards , CardIdentification& cardID){
    std::set<uint8_t> records;
    uint8_t pair = 255;
    uint8_t pair_2 = 255;
    for(auto card : parsedCards){
        auto point = card.point == uint8_t(1) ? uint8_t(14) : card.point;
        if(records.find(point) != records.end()){
            if(pair != uint8_t(255)){
                if(point == pair){
                    cardID.level = CardLevel::Sites;
                    records.erase(pair);
                }
                else{
                    cardID.level = CardLevel::Two_Pairs;
                    pair_2 = point;
                    records.erase(pair);
                    records.erase(pair_2);
                }
            }
            else{
                pair = point;
            }
        }
        else{
            records.emplace(point);
        }
    }
    if(cardID.level == CardLevel::Sites){
        SERVER_ASSERT(records.size() == 2 , "Unreasonable for sites remain {} elements" , records.size());
        cardID.sortedCards[0] = cardID.sortedCards[1] = cardID.sortedCards[2] = pair;
        int i = 0;
        for(auto it = records.begin() ; it != records.end() ; it++ , i++){
            cardID.sortedCards[4 - i] = *it;
        }
    }
    else{
        SERVER_ASSERT(cardID.level == CardLevel::Two_Pairs , "Unreasonable result");
        SERVER_ASSERT(records.size() == 1 , "Unreasonable for sites remain {} elements" , records.size());
        if(pair < pair_2){
            uint8_t t = pair;
            pair = pair_2;
            pair_2 = t;
        }
        cardID.sortedCards[0] = cardID.sortedCards[1] = pair;
        cardID.sortedCards[2] = cardID.sortedCards[3] = pair_2;
        cardID.sortedCards[4] = *(records.begin());
    }
}

void SortBombFullHouse(const std::vector<ParsedCard>& parsedCards , CardIdentification& cardID){
    std::unordered_map<uint8_t, uint8_t> records;
    uint8_t max = 0;
    for(auto card : parsedCards){
        auto point = card.point == uint8_t(1) ? uint8_t(14) : card.point;
        records[point]++;
        if(records[point] > max){
            max = records[point];
        }
    }
    for(auto it = records.begin() ; it != records.end() ; it++){
        if(it->second <= 2){
            for(int i = 0 ; i < it->second ; i++){
                cardID.sortedCards[4 - i] = it->first;
            }
        }
        else{
            for(int i = 0 ; i < it->second ; i++){
                cardID.sortedCards[i] = it->first;
            }
        }
    }
    if(max == uint8_t(4)){
        cardID.level = CardLevel::Bomb;
    }
    else{
        SERVER_ASSERT(max == uint8_t(3) , "Unreasonable out of bomb or fullhouse");
        cardID.level = CardLevel::Full_House;
    }
    
}

};

Card::Card(const uint8_t& cardNum , const std::filesystem::path& configPath):m_CardNum(cardNum)
{
    if(std::filesystem::exists(configPath)){
        m_ConfigPath = configPath;
    }
    else{
        SERVER_ERROR("Failed to load config file [{}]: No such a file" , configPath.string());
        // generate default config
    }
    SERVER_ASSERT(GenParseConfig(),"Unable to load parse config");
}

bool Card::GenParseConfig(){
    bool res = true;
    if(m_ConfigPath.empty()){
        m_ParseConfig = {
            //default for code running
            {{0  , 12} , CardType::Club},
            {{13 , 25} , CardType::Heart},
            {{26 , 39} , CardType::Spade},
            {{40 , 52} , CardType::Diamond},
            {{53 , 54} , CardType::Joker},
        };
    }
    else{
        
        YAML::Node data;
        try
        {
            data = YAML::LoadFile(m_ConfigPath);
        }
        catch (YAML::ParserException e)
        {
            SERVER_ERROR("Failed to load config file '{0}'\n     {1}", m_ConfigPath.string(), e.what());
            return false;
        }
        if (!data["Pokers"])
            return false;
        auto pokers = data["Pokers"];
        if(pokers){
            for(auto poker : pokers){
                std::pair<std::pair<uint8_t , uint8_t> , CardType> typeInfo;
                if(poker["Type"]){
                    typeInfo.second = (CardType)poker["Type"].as<int>();
                }
                if(poker["Point_min"]){
                    typeInfo.first.first = (uint8_t)poker["Point_min"].as<int>();
                }
                if(poker["Point_max"]){
                    typeInfo.first.second = (uint8_t)poker["Point_max"].as<int>();
                }
                m_ParseConfig.push_back(typeInfo);
                
            }
        }
    }
    return res;
}



    uint8_t Card::GenerateOneCard(bool repeatable){
        uint8_t cardPoint = 255;
        if(repeatable){
            cardPoint = (uint8_t)Random::RandomInt(0, m_CardNum - 1);
            m_GeneratedCards.emplace(cardPoint);
        }
        else{
            if(m_GeneratedCards.size() == m_CardNum){
                SERVER_ERROR("Out of {} cards, shuffle please" , m_CardNum);
            }
            else{
                cardPoint = (uint8_t)Random::RandomInt(0, m_CardNum - 1);
                while(m_GeneratedCards.find(cardPoint) != m_GeneratedCards.end()){
                    cardPoint = (uint8_t)Random::RandomInt(0, m_CardNum - 1);
                }
                m_GeneratedCards.emplace(cardPoint);
            }
        }
        return cardPoint;//255 for invalid
    }

    ParsedCard Card::GenerateOneParsedCard(bool repeatable){
        auto cardIdx = GenerateOneCard(repeatable);
        return ParseCard(cardIdx);
    }



    void Card::Shuffle(){
        m_GeneratedCards.clear();
    }

    bool Card::SameType(const std::vector<ParsedCard>& parsedCards){
        SERVER_ASSERT(parsedCards.size() > 0 , "Invalid input cards.size");
        bool res = true;
        CardType type = CardType::Cover;
        for(int i = 0 ; i < parsedCards.size() && res ; i++){
            if(i == 0) type = parsedCards[i].type;
            res = false;
        }
        return res;
    }

ParsedCard Card::ParseCard(const uint8_t& card){
    ParsedCard parsedCard;
    for(auto it = m_ParseConfig.begin() ; it != m_ParseConfig.end() ; it++){
        if(card >= it->first.first && card <= it->first.second){
            parsedCard.type = it->second;
            parsedCard.point = card - it->first.first + 1; // 1~13
        }
    }
    SERVER_ASSERT(parsedCard.type != CardType::Cover , "illegal card :[{}]" , card);
    return parsedCard;
}

CardIdentification Card::CalCardID(const std::vector<ParsedCard>& parsedCards){
    SERVER_ASSERT(parsedCards.size() == 5, "Unable to calculate non-five cards");
    CardIdentification res;
    Utils::cardDistribution distribution = Utils::CardDistribution(parsedCards);
    res.sortedCards.resize(5);
    switch (distribution.pointDistribution) {
        case 5:{
            bool straight = Utils::IsStraight(parsedCards, res);
            bool sameColor = distribution.typeDistribution == 1;
            if(straight){
                if(sameColor){
                    if(res.sortedCards[0] == ((uint8_t)14)){
                        res.level = CardLevel::Royal_Flush;
                    }
                    else{
                        res.level = CardLevel::Straight_Flush;
                    }
                }
            }
            else{
                if(sameColor) res.level = CardLevel::Flush;
                else res.level = CardLevel::High;
            }
            break;
            }
        case 4:{
            //
            res.level = CardLevel::Pair;
            Utils::SortPair(parsedCards, res);
            break;
        }
        case 3:{
            Utils::SortSitesTwoPair(parsedCards, res);
            break;
        }
        case 2:
        {
            Utils::SortBombFullHouse(parsedCards, res);
            break;
        }
        default:
            SERVER_ASSERT(false , "Impossible set distributions : {}" , distribution.pointDistribution);
            break;
    }
    
    
    return res;
}


int Card::CompareTwoSets(const std::vector<ParsedCard>& firstCards , const std::vector<ParsedCard>& secondCards){
    //return:
    //0 : eqaul sets
    //1 : first set is greater
    //2 : second set is greater
    CardIdentification firstID = CalCardID(firstCards);
    CardIdentification secondID = CalCardID(secondCards);
    SERVER_INFO("[{}] vs [{}]" , CardLevelLog[int(firstID.level)] , CardLevelLog[int(secondID.level)]);
    return CompareTwoSets(firstID, secondID);
}

int Card::CompareTwoSets(const CardIdentification& firstCards , const CardIdentification& secondCards){
    if(firstCards.level == secondCards.level){
        for(int i = 0 ; i < 5 ; i++){
            if(firstCards.sortedCards[i] != secondCards.sortedCards[i])
                return firstCards.sortedCards[i] > secondCards.sortedCards[i] ? 1 : 2;
        }
        return 0;
    }
    else{
        return firstCards.level > secondCards.level ? 1 : 2;
    }
    SERVER_ASSERT(false, "u can't be here");
    return 0;
}






}
