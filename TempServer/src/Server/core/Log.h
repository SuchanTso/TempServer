#pragma once
#define SPDLOG_DISABLE_TID_CACHING
#include "spdlog/spdlog.h"
#include "Core.h"
#include "spdlog/sinks/stdout_color_sinks.h"

namespace Tso {
	class Log
	{
	public:
		Log();
		~Log();


		static void Init();

		inline static std::shared_ptr<spdlog::logger>& GetCoreLogger() { return s_CoreLogger; }
		inline static std::shared_ptr<spdlog::logger>& GetClientLogger() { return s_ClientLogger; }

	private:
		static std::shared_ptr<spdlog::logger> s_CoreLogger;
		static std::shared_ptr<spdlog::logger> s_ClientLogger;

	};
}



// core log macros
#define SERVER_TRACE(...)  ::Tso::Log::GetCoreLogger()->trace(__VA_ARGS__)
#define SERVER_INFO(...)   ::Tso::Log::GetCoreLogger()->info(__VA_ARGS__)
#define SERVER_WARN(...)   ::Tso::Log::GetCoreLogger()->warn(__VA_ARGS__)
#define SERVER_ERROR(...)  ::Tso::Log::GetCoreLogger()->error(__VA_ARGS__)
#define SERVER_FATAL(...)  ::Tso::Log::GetCoreLogger()->fatal(__VA_ARGS__)


#pragma once
