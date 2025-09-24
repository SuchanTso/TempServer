//
//  systemModule.h
//  TempServer
//
//  Created by 左斯诚 on 2025/8/7.
//

#ifndef SystemModule_h
#define SystemModule_h
#pragma once
#include "Module.h"

namespace Tso::Modules::System {

    constexpr uint8_t MODULE_ID = 1;

    enum CommandID : uint8_t {
        C2S_HeartBeat = 1,
        S2C_HeartBeat = 2,
        C2S_LoginReq = 3,
        S2C_LoginRsp = 4,
    };

    inline uint16_t MakeProtocolID(CommandID cmd) {
        return (MODULE_ID << 8) | cmd;
    }

}

namespace Tso {


    class SystemModule : public IModule {
    public:
        SystemModule();
        ~SystemModule() override = default;
        
        uint8_t GetModuleId() const override;
        virtual void HandlePacket(uint32_t clientId, uint8_t commandId, ByteStream& stream) override;
        
        std::string GetUserName(const uint32_t& clientId);
        
    private:
        using CommandHandler = std::function<void(uint32_t, const ByteStream&)>;
        std::unordered_map<uint8_t, CommandHandler> m_CommandHandlers;
        
        // 业务逻辑处理函数
        void OnLoginReq(uint32_t clientId, const ByteStream& stream);
        
        // 存储已登录的用户
        std::unordered_map<uint32_t, std::string> m_LoggedInUsers; // clientId -> username
    };
}

#endif /* systemModule_h */
