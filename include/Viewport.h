#pragma once

#include "Engine.h"
#include "SDL.h"

const int kViewportInitialWidth = 1920;
const int kViewportInitialHeight = 1080;
const char* const kViewportWindowTitle = "Vulkan Sample";

class CViewport
{
public:
	EEngineStatus Initialize();
	EEngineStatus Update();
	EEngineStatus Shutdown();
	
	inline SDL_Window* GetWindow() const
	{
		return m_Window;
	}
private:
	SDL_Window* m_Window = nullptr;
};