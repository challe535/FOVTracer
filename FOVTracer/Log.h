#pragma once

#include <memory>

#include "spdlog/spdlog.h"
#include "spdlog/sinks/stdout_color_sinks.h"

class Log
{
public:
	static void Init();

	inline static std::shared_ptr<spdlog::logger> s_CoreLogger;
};

//Temporary debug toggling. In future set different log levels instead.  
#ifdef _DEBUG
#define CORE_TRACE(...)  ::Log::s_CoreLogger->trace(__VA_ARGS__)
#define CORE_INFO(...)   ::Log::s_CoreLogger->info(__VA_ARGS__)
#define CORE_WARN(...)   ::Log::s_CoreLogger->warn(__VA_ARGS__)
#define CORE_ERROR(...)  ::Log::s_CoreLogger->error(__VA_ARGS__)
#define CORE_CRITICAL(...)  ::Log::s_CoreLogger->critical(__VA_ARGS__)
#else
#define CORE_TRACE(...)
#define CORE_INFO(...)
#define CORE_WARN(...)
#define CORE_ERROR(...)
#define CORE_CRITICAL(...)
#endif

