#pragma once


namespace Tso {
	class Application {
	public:
		Application() = default;
		~Application() = default;
		void Run();
		virtual void AppOnUpdate(float ts){}
		inline static Application* GetApp() { return s_Instance; }
	private:
		static Application* s_Instance;
	};

	Application* CreateApplication();
}