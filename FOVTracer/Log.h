#pragma once

#include <memory>

#include "spdlog/spdlog.h"
#include "spdlog/sinks/stdout_color_sinks.h"

class Log
{
public:
	static void Init();

	inline static std::shared_ptr<spdlog::logger> s_CoreLogger;
	inline static std::shared_ptr<spdlog::logger> s_ClientLogger;
};

//Temporary debug toggling. In future set different log levels instead.  
#ifdef _DEBUG
#define LUM_CORE_TRACE(...)  ::Log::s_CoreLogger->trace(__VA_ARGS__)
#define LUM_CORE_INFO(...)   ::Log::s_CoreLogger->info(__VA_ARGS__)
#define LUM_CORE_WARN(...)   ::Log::s_CoreLogger->warn(__VA_ARGS__)
#define LUM_CORE_ERROR(...)  ::Log::s_CoreLogger->error(__VA_ARGS__)
#define LUM_CORE_CRITICAL(...)  ::Log::s_CoreLogger->critical(__VA_ARGS__)

#define LUM_TRACE(...)  ::Log::s_ClientLogger->trace(__VA_ARGS__)
#define LUM_INFO(...)   ::Log::s_ClientLogger->info(__VA_ARGS__)
#define LUM_WARN(...)   ::Log::s_ClientLogger->warn(__VA_ARGS__)
#define LUM_ERROR(...)  ::Log::s_ClientLogger->error(__VA_ARGS__)
#define LUM_CRITICAL(...)  ::Log::s_ClientLogger->critical(__VA_ARGS__)
#else
#define LUM_CORE_TRACE(...)
#define LUM_CORE_INFO(...)
#define LUM_CORE_WARN(...)
#define LUM_CORE_ERROR(...)
#define LUM_CORE_CRITICAL(...)

#define LUM_TRACE(...)
#define LUM_INFO(...)
#define LUM_WARN(...)
#define LUM_ERROR(...)
#define LUM_CRITICAL(...) 
#endif

