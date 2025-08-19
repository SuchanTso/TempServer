#pragma once


namespace Tso {
	class Application {
	public:
		Application();
		~Application() = default;
		void Run();
		virtual void AppOnUpdate(float ts){}
		inline static Application* GetApp() { return s_Instance; }
        virtual void RegistryProtocol(){}
	private:
		static Application* s_Instance;
	};

	Application* CreateApplication();
}
