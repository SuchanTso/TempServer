//
//  SystemModule.cpp
//  TempServer
//
//  Created by 左斯诚 on 2025/8/7.
//

#include "Spch.h"
#include "SystemModule.h"
#include "Server/data/ByteStream.h"
#include "Server/network/NetworkEngine.h"

namespace Tso{
    SystemModule::SystemModule() {
        m_CommandHandlers[Modules::System::CommandID::C2S_LoginReq] = [this](uint32_t cid, const ByteStream& s){ this->OnLoginReq(cid, s); };
    }

    uint8_t SystemModule::GetModuleId() const {
        return Tso::Modules::System::MODULE_ID;
    }

    std::string SystemModule::GetUserName(const uint32_t& clientId){
        std::string res = "Anonymous";
        if(m_LoggedInUsers.find(clientId) != m_LoggedInUsers.end()){
            res = m_LoggedInUsers[clientId];
        }
        return res;
    }


    void SystemModule::HandlePacket(uint32_t clientId, uint8_t commandId, ByteStream& stream) {
        auto it = m_CommandHandlers.find(commandId);
        if (it != m_CommandHandlers.end()) {
            it->second(clientId, stream);
        } else {
            SERVER_WARN("SystemModule: Received unknown command ID {} from client {}.", commandId, clientId);
        }
    }

    void SystemModule::OnLoginReq(uint32_t clientId,const ByteStream& stream) {
        std::string username = stream.readString();
        std::string password = stream.readString();
        
        SERVER_INFO("Client {} attempts to log in as '{}'", clientId, username.c_str());
        
        ByteStream response;
        // 简单的认证逻辑
        if (password == "123456") {
            m_LoggedInUsers[clientId] = username;
            response.write<uint8_t>(0); // 0 = success
            response.writeString("Login successful!");
            SERVER_INFO("User '%s' logged in successfully.", username.c_str());
        } else {
            response.write<uint8_t>(1); // 1 = failed
            response.writeString("Invalid username or password.");
            SERVER_WARN("User '%s' login failed.", username.c_str());
        }
        
        uint16_t responseId = Tso::Modules::System::MakeProtocolID(Tso::Modules::System::S2C_LoginRsp);
        NetWorkEngine::GetEngine()->SendToClient(clientId, responseId, response , true);
    }
}
