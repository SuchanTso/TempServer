#include "Spch.h"
#include "Poker.h"
#include "Server/core/EntryPoint.h"
#include "logic/Card.h"
#include "yaml-cpp/yaml.h"
#include "core/PokerRoom.h"
#include "Server/network/NetworkEngine.h"
#include "Server/core/SystemModule.h"
#include "Server/core/LobbyModule.h"
#include "Server/core/Module.h"
#include "core/TexasHoldemModule.h"
#include "core/PokerPlayer.h"


using CardSet = std::vector<uint8_t>;
namespace YAML
{
    template<>
    struct convert<CardSet>
    {
        static Node encode(const CardSet& rhs)
        {
            Node node;
            for(int i = 0 ; i < rhs.size() ; i++){
                node.push_back(rhs[i]);
                node.push_back(rhs[i]);
            }
            node.SetStyle(EmitterStyle::Flow);
            return node;
        }
        
        static bool decode(const Node& node, CardSet& rhs)
        {
            if (!node.IsSequence() || node.size() != 5)
                return false;
            
            for(int i = 0 ; i < 5 ; i++){
                rhs.push_back(node[i].as<uint8_t>());
            }
            return true;
        }
    };
}


namespace UnitTest {

struct setMatchInfo{
    std::vector<uint8_t> set1;
    std::vector<uint8_t> set2;
    bool result;
};

std::vector<setMatchInfo> ReadTwoSetsMatch(const std::string& path){
    YAML::Node data;
    try
    {
        data = YAML::LoadFile(path);
    }
    catch (YAML::ParserException e)
    {
        SERVER_ERROR("Failed to load config file '{0}'\n     {1}", path, e.what());
    }
    std::vector<setMatchInfo> res;
    if (!data["Pokers"]){
        
    }
    auto pokers = data["Pokers"];
    if(pokers){
        for(auto poker : pokers){
            setMatchInfo setMatch;
            if(poker["Set1"]){
                setMatch.set1 = poker["Set1"].as<CardSet>();
            }
            if(poker["Set2"]){
                setMatch.set2 = poker["Set2"].as<CardSet>();
            }
            if(poker["res"]){
                setMatch.result = poker["res"].as<int>() == 1 ? true : false;
            }
            res.push_back(setMatch);
        }
    }
    return res;
}
}

namespace Tso {

void Poker::RegistryProtocol(){
    
    Tso::NetWorkEngine::GetEngine()->SetOnClientDisconnected(
    [this](uint32_t cid) {
        this->OnClientDisconnected(cid);
    });
    // 将自己注册为网络事件的处理器
    Tso::NetWorkEngine::SetAppPacketHandler(
        [this](uint32_t cid, uint16_t pid, ByteStream& s) {
            this->OnPacketReceived(cid, pid, s);
        }
    );
    
    // 1. 创建所有模块实例
    auto systemModule = std::make_unique<SystemModule>();
    auto lobbyModule = std::make_unique<LobbyModule>();
    auto texasModule = std::make_unique<TexasHoldemModule>();
    
    // [NEW] 设置工厂
    // 获取 LobbyModule 的指针来调用它的 public 方法
    LobbyModule* lobbyPtr = lobbyModule.get();

    // 创建一个 PokerRoom 的工厂函数
    auto pokerRoomFactory = [](uint32_t roomId, uint8_t maxPlayers) -> Ref<Room> {
        return CreateRef<PokerRoom>(roomId, maxPlayers);
    };
    
    auto pokerPlayerFactory = [](uint64_t playerID , uint32_t netID, std::string name) -> Ref<Player> {
        return CreateRef<PokerPlayer>(playerID , netID , name);
    };
    
    // 将工厂注册到 LobbyModule
    lobbyPtr->RegisterRoomFactory(Tso::Modules::Lobby::GameType::TexasHoldem, pokerRoomFactory);
    lobbyPtr->RegisterPlayerFactory(Tso::Modules::Lobby::GameType::TexasHoldem, pokerPlayerFactory);
    lobbyPtr->RegistrerPlayerNameGetFunction([this](const uint32_t& clientId)->std::string{
        return this->GetUserName(clientId);
    });
    // 2. 将模块添加到管理器
    m_Modules[systemModule->GetModuleId()] = std::move(systemModule);
    m_Modules[lobbyModule->GetModuleId()] = std::move(lobbyModule);
    m_Modules[texasModule->GetModuleId()] = std::move(texasModule);
}


void Poker::OnPacketReceived(uint32_t clientId, uint16_t protocolId, ByteStream& stream) {
    uint8_t moduleId = (protocolId >> 8) & 0xFF;
    uint8_t commandId = protocolId & 0xFF;

    auto it = m_Modules.find(moduleId);
    if (it != m_Modules.end()) {
        it->second->HandlePacket(clientId, commandId, stream);
    } else {
        SERVER_WARN("Received message for unknown module ID [{}] from client [{}].", moduleId, clientId);
    }
}

std::string Poker::GetUserName(const uint32_t& clientId){
    return static_cast<SystemModule*>(m_Modules[Modules::System::MODULE_ID].get())->GetUserName(clientId);
}


IModule* Poker::GetModule(const uint8_t& moduleID){
    if(m_Modules.find(moduleID) == m_Modules.end()){
        SERVER_ERROR("Not found module: [{}]" , moduleID);
        return nullptr;
    }
    return m_Modules[moduleID].get();
}

void Poker::OnClientDisconnected(uint32_t clientId){
    SERVER_INFO("Client {} has disconnected. Notifying modules.", clientId);
    for (auto const& [id, module] : m_Modules) {
        module->OnPlayerDisconnected(clientId);
    }
}




bool s_Flag = true;
	void Poker::AppOnUpdate(float ts)
	{
//        uint8_t num = 52;
//        std::filesystem::path path = "pokerLevelTest.yaml";
//        std::filesystem::path config = "pokerParse.yaml";
//
//        if(s_Flag){
//            Card card(num , config.string());
//            auto matches = UnitTest::ReadTwoSetsMatch(path);
//            for(auto match : matches){
//                std::vector<ParsedCard>parseC1(5);
//                std::vector<ParsedCard>parseC2(5);
//                for(int i = 0 ; i < 5 ; i++){
//                    parseC1[i] = card.ParseCard(match.set1[i]);
//                    parseC2[i] = card.ParseCard(match.set2[i]);
//                }
//                
//                SERVER_ASSERT(card.CampareTwoSets(parseC1, parseC2) == match.result, "");
//            }
//            s_Flag = false;
//        }
//        if(s_Flag){
//            Ref<PokerRoom> newRoom = CreateRef<PokerRoom>(8 , m_RoomList.size());
//            m_RoomList.emplace_back(newRoom);
//            s_Flag = false;
//        }
//        for(auto room : m_RoomList){
//            room->OnUpdate(ts);
//        }
        for (auto const& [id, modules] : m_Modules) {
            modules->OnUpdate(ts); // 这会调用 LobbyModule::OnUpdate 和 TexasHoldemModule::OnUpdate
        }
	}
}
