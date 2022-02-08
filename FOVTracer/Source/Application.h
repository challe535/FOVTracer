#pragma once

#include "Core.h"
#include "Tracer.h"

class Application
{

public:
	/**
	* Initialize application resources.
	*/
	void Init(LONG width, LONG height, HINSTANCE& instance, LPCWSTR title);

	/**
	* Run application loop. Returns exit code.
	*/
	void Run();

	/**
	* Cleanup resources.
	*/
	void Cleanup();

private:
	HWND Window;
	Tracer RayTracer;
	Scene RayScene;
};

