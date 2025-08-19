#include "Server.h"
namespace Tso {
class PokerRoom;
class ByteStream;
class IModule;
	class Poker : public Application {
	public:
        Poker() = default;
		virtual void AppOnUpdate(float ts)override;
        void OnPacketReceived(uint32_t clientId, uint16_t protocolId, ByteStream& stream);
        virtual void RegistryProtocol()override;
        void CreateRoom();
        IModule* GetModule(const uint8_t& moduleID);
        void OnClientDisconnected(uint32_t clientId);

    private:
        std::vector<Ref<PokerRoom>> m_RoomList;
        std::unordered_map<uint8_t, std::unique_ptr<IModule>> m_Modules;
	};

    inline Application* CreateApplication() {
		return new Poker();
	}


}
