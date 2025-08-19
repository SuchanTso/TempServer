//
//  Module.h
//  TempServer
//
//  Created by 左斯诚 on 2025/8/7.
//

#ifndef Module_h
#define Module_h

#pragma once

namespace Tso{
    class ByteStream;
    class IModule {
    public:
        virtual ~IModule() = default;
        
        // 获取模块ID
        virtual uint8_t GetModuleId() const = 0;
        
        // 模块初始化
        virtual void Init() {}
        
        // 模块销毁
        virtual void Shutdown() {}
        
        // 处理该模块的网络包
        virtual void HandlePacket(uint32_t clientId, uint8_t commandId, ByteStream& stream) = 0;
        
        // 每帧更新
        virtual void OnUpdate(TimeStep ts) {}
        
        // 玩家连接和断开的通知
        virtual void OnPlayerConnected(uint32_t clientId) {}
        virtual void OnPlayerDisconnected(uint32_t clientId) {}
    };
}
#endif /* Module_h */
