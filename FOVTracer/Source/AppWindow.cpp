/* Copyright (c) 2018-2019, NVIDIA CORPORATION. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *  * Neither the name of NVIDIA CORPORATION nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "pch.h"
#include "AppWindow.h"
#include "Application.h"

#include "synchapi.h"

//#include <cstdio>
#include <iostream>
//#include <memory>
//#include <stdexcept>
//#include <string>
//#include <array>

 // Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

 /**
  * Windows message loop.
  */
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	if (ImGui_ImplWin32_WndProcHandler(hWnd, message, wParam, lParam))
		return true;

	PAINTSTRUCT ps;
	switch (message)
	{
	case WM_PAINT:
		BeginPaint(hWnd, &ps);
		EndPaint(hWnd, &ps);
		break;
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}

namespace AppWindow
{

	/**
	 * Create a new window.
	*/
	HRESULT Create(LONG width, LONG height, HINSTANCE& instance, HWND& window, LPCWSTR title)
	{
		// Register the window class
		WNDCLASSEX wcex;
		wcex.cbSize = sizeof(WNDCLASSEX);
		wcex.style = CS_HREDRAW | CS_VREDRAW;
		wcex.lpfnWndProc = WndProc;
		wcex.cbClsExtra = 0;
		wcex.cbWndExtra = 0;
		wcex.hInstance = instance;
		wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
		wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
		wcex.lpszMenuName = NULL;
		wcex.lpszClassName = L"WindowClass";
		wcex.hIcon = nullptr;
		wcex.hIconSm = nullptr;

		if (!RegisterClassEx(&wcex))
		{
			throw std::runtime_error("Failed to register window!");
		}

		// Get the desktop resolution
		RECT desktop;
		const HWND hDesktop = GetDesktopWindow();
		GetWindowRect(hDesktop, &desktop);

		int x = (desktop.right - width) / 2;

		// Create the window
		RECT rc = { 0, 0, width, height };
		AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
		window = CreateWindow(wcex.lpszClassName, title, WS_OVERLAPPEDWINDOW, x, 0, (rc.right - rc.left), (rc.bottom - rc.top), NULL, NULL, instance, NULL);
		if (!window) return E_FAIL;

		//// Set the window icon
		//HANDLE hIcon = LoadImageA(GetModuleHandle(NULL), "nvidia.ico", IMAGE_ICON, 0, 0, LR_DEFAULTSIZE | LR_LOADFROMFILE);
		//SendMessage(window, WM_SETICON, ICON_BIG, (LPARAM)hIcon);

		// Show the window
		ShowWindow(window, SW_SHOWDEFAULT);
		UpdateWindow(window);

		return S_OK;
	}

	VOID Startup(LPCTSTR lpApplicationName, LPWSTR cmdLine)
	{
		// additional information
		STARTUPINFO si;
		PROCESS_INFORMATION pi;

		// set the size of the structures
		ZeroMemory(&si, sizeof(si));
		si.cb = sizeof(si);
		ZeroMemory(&pi, sizeof(pi));

		// start the program up
		CreateProcess(lpApplicationName,   // the path
			cmdLine,        // Command line
			NULL,           // Process handle not inheritable
			NULL,           // Thread handle not inheritable
			FALSE,          // Set handle inheritance to FALSE
			0,              // No creation flags
			NULL,           // Use parent's environment block
			NULL,           // Use parent's starting directory 
			&si,            // Pointer to STARTUPINFO structure
			&pi             // Pointer to PROCESS_INFORMATION structure (removed extra parentheses)
		);

		WaitForSingleObject(pi.hProcess, INFINITE);

		// Close process and thread handles. 
		CloseHandle(pi.hProcess);
		CloseHandle(pi.hThread);
	}

	//std::string ExecCmd(const wchar_t* cmd)
	//{
	//	std::string strResult;
	//	HANDLE hPipeRead, hPipeWrite;

	//	SECURITY_ATTRIBUTES saAttr = { sizeof(SECURITY_ATTRIBUTES) };
	//	saAttr.bInheritHandle = TRUE; // Pipe handles are inherited by child process.
	//	saAttr.lpSecurityDescriptor = NULL;

	//	// Create a pipe to get results from child's stdout.
	//	if (!CreatePipe(&hPipeRead, &hPipeWrite, &saAttr, 0))
	//		return strResult;

	//	STARTUPINFOW si = { sizeof(STARTUPINFOW) };
	//	si.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
	//	si.hStdOutput = hPipeWrite;
	//	si.hStdError = hPipeWrite;
	//	si.wShowWindow = SW_HIDE; // Prevents cmd window from flashing.
	//							  // Requires STARTF_USESHOWWINDOW in dwFlags.

	//	PROCESS_INFORMATION pi = { 0 };

	//	BOOL fSuccess = CreateProcessW(NULL, (LPWSTR)cmd, NULL, NULL, TRUE, CREATE_NEW_CONSOLE, NULL, NULL, &si, &pi);
	//	if (!fSuccess)
	//	{
	//		CloseHandle(hPipeWrite);
	//		CloseHandle(hPipeRead);
	//		return strResult;
	//	}

	//	bool bProcessEnded = false;
	//	for (; !bProcessEnded;)
	//	{
	//		// Give some timeslice (50 ms), so we won't waste 100% CPU.
	//		bProcessEnded = WaitForSingleObject(pi.hProcess, 50) == WAIT_OBJECT_0;

	//		// Even if process exited - we continue reading, if
	//		// there is some data available over pipe.
	//		for (;;)
	//		{
	//			char buf[1024];
	//			DWORD dwRead = 0;
	//			DWORD dwAvail = 0;

	//			if (!::PeekNamedPipe(hPipeRead, NULL, 0, NULL, &dwAvail, NULL))
	//				break;

	//			if (!dwAvail) // No data available, return
	//				break;

	//			if (!::ReadFile(hPipeRead, buf, Math::min(sizeof(buf) - 1, static_cast<unsigned long long>(dwAvail)), &dwRead, NULL) || !dwRead)
	//				// Error, the child process might ended
	//				break;

	//			buf[dwRead] = 0;
	//			strResult += buf;
	//		}
	//	} //for

	//	CloseHandle(hPipeWrite);
	//	CloseHandle(hPipeRead);
	//	CloseHandle(pi.hProcess);
	//	CloseHandle(pi.hThread);
	//	return strResult;
	//}
}