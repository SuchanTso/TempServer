#pragma once
namespace Tso {

    enum class CardType{
        Cover = 0,
        Club = 1,
        Diamond = 2,
        Spade = 3,
        Heart = 4,
        Joker = 5
    };
    enum class CardLevel{
        Royal_Flush     = 10,//皇家同花顺
        Straight_Flush  = 9,//同花顺
        Bomb            = 8,//四条
        Full_House      = 7,//葫芦
        Flush           = 6,//同花
        Straight        = 5,//顺子
        Sites           = 4,//三条
        Two_Pairs       = 3,//两对
        Pair            = 2,//对
        High            = 1,//高牌
        None            = 0
    };

    struct ParsedCard{
        CardType type = CardType::Cover;
        uint8_t point = 0;
    };

struct CardIdentification{
    CardLevel level = CardLevel::None;
    std::vector<uint8_t>sortedCards;
};

	class Card {
    public:
        Card() = default;
        Card(const uint8_t& cardNum , const std::filesystem::path& configPath);
        ~Card() = default;
        uint8_t GenerateOneCard(bool repeatable = false);
        ParsedCard GenerateOneParsedCard(bool repeatable = false);
        void Shuffle();
        bool SameType(const std::vector<ParsedCard>& parsedCards);
        ParsedCard ParseCard(const uint8_t& card);
        static int CompareTwoSets(const std::vector<ParsedCard>& firstCards , const std::vector<ParsedCard>& secondCards);
        static int CompareTwoSets(const CardIdentification& firstCards , const CardIdentification& secondCards);
        //return true if first one is greater than second one;
        bool GenParseConfig();
        static CardIdentification CalCardID(const std::vector<ParsedCard>& parsedCards);
        
    private:
        uint8_t m_CardNum = 52;
        std::filesystem::path m_ConfigPath = "";
        std::unordered_set<uint8_t> m_GeneratedCards;
        std::vector<std::pair<std::pair<uint8_t , uint8_t> , CardType>> m_ParseConfig;
	};
}
