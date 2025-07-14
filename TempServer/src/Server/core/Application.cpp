#include "Spch.h"
#include "Application.h"
#include "network/NetworkEngine.h"

void Tso::Application::Run()
{
	NetWorkEngine::Init();
	NetWorkEngine::StartServer(6000);
	while (true) {
		NetWorkEngine::OnUpdate(0.f);// for information exchange
		AppOnUpdate(0.f);
	}
	NetWorkEngine::Shutdown();
}

