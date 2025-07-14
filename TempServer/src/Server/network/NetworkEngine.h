#pragma once
#include<string>
#include "readerwriterqueue.h"
#include "data/ByteStream.h"
namespace Tso {
	class TCPChannel;
	//class UDPChannel;
	struct ClientConnection {
		Ref<TCPChannel> channel;
		std::thread recvThread;
		std::atomic<bool> active{ true };
		uint32_t clientId;
		std::chrono::steady_clock::time_point lastActiveTime;
	};
	class NetWorkEngine {
	public:
		NetWorkEngine();
		~NetWorkEngine();
		void static Init();
		void static Shutdown();
		bool static Connect(const std::string& ip, const uint16_t& port);
		bool static DisConnect();
		//bool StartServer(const uint16_t port);
		void static RegistryProtocol(const uint8_t& protocolID, const std::function<ByteStream(const ByteStream&)>& wrapFunc);
		void static RegistryProtocol(const std::unordered_map<uint8_t, std::function<ByteStream(const ByteStream&)>>& wrapFuncs);
		void static RegistryRecvFunction(const uint8_t& funcID, const std::function<void(ByteStream&)>& recvFunc);
		bool static HandlerNetwork(const uint8_t& protocolID, const ByteStream& byte, bool Reliable = true);
		void static OnUpdate(TimeStep ts);
		void static RegistryScene(Ref<Scene>scene);
		void StartReceiveThread();
		void ProcessBuffer(std::vector<uint8_t>& buffer);
		void ProcessClientBuffer(std::shared_ptr<ClientConnection> client, std::vector<uint8_t>& buffer);

		bool static StartServer(const uint16_t& port);
		void StopServer();
		void AcceptConnections();
		void RemoveClient(uint32_t clientId);
		void HandleClient(std::shared_ptr<ClientConnection> client);
		bool SendToClient(uint32_t clientId, const uint8_t& protocolID, const ByteStream& byte, bool Reliable);
		bool Broadcast(const uint8_t& protocolID, const ByteStream& byte, bool Reliable);
		//void static DefaultWrap(const uint8_t& protocolID , ByteStream& byte);
	private:
		std::unordered_map<uint8_t, std::function<ByteStream(const ByteStream&)>> m_WrapFuncs;
		std::unordered_map<uint8_t, std::function<void(ByteStream&)>> m_RecvFuncs;
		Ref<TCPChannel> m_ListenerChannel = nullptr;
		//Ref<UDPChannel> m_UDPChannel = nullptr;
		std::atomic<bool> m_Receiving{ false };
		std::thread m_RecvThread;
		std::thread m_ListenerThread;
		Ref<Scene> m_Scene;
		bool m_Listening = false;
		uint32_t m_NextClientId = 0;
		std::unordered_map<uint32_t, Ref<ClientConnection>> m_Clients;
		std::mutex m_ClientMutex;
		std::queue<uint32_t> m_RemoveClients;
		moodycamel::ReaderWriterQueue<std::pair<uint8_t, ByteStream>> m_PacketQueue; // нчкЬ╤сап
	};
}
