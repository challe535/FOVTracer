#pragma once

#include "Core.h"
#include "Tracer.h"
#include "Input.h"

class Application
{

public:
	Application() {};
	Application(Application const&) = delete;
	void operator=(Application const&) = delete;

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

	static Application& GetApplication() { static Application Instance; return Instance; };

	Input InputHandler;
private:
	HWND Window;
	Tracer RayTracer;
	Scene RayScene;

	
};

