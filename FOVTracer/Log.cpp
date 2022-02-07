#include "Log.h"


void Log::Init()
{
	spdlog::set_pattern("%^[%T] %n: %v%$");

	s_CoreLogger = spdlog::stderr_color_mt("CORE");
	s_CoreLogger->set_level(spdlog::level::trace);
}