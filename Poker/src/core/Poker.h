#include "Server.h"
namespace Tso {
	class Poker : public Application {
	public: 
		virtual void AppOnUpdate(float ts)override;
	};

	Application* CreateApplication() {
		return new Poker();
	}

}