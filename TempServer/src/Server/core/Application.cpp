#include "Spch.h"
#include "Application.h"
#include "network/NetworkEngine.h"
namespace Tso{
Application* Application::s_Instance = nullptr;

Application::Application(){
    s_Instance = this;
}


void Application::Run()
{
    NetWorkEngine::Init();
    NetWorkEngine::StartServer(6000);
    RegistryProtocol();
    while (true) {
        NetWorkEngine::OnUpdate(0.f);// for information exchange
        AppOnUpdate(0.f);
    }
    NetWorkEngine::Shutdown();
}

//Application* Application::GetApp(){
//    return s_Instance;
//}


}
