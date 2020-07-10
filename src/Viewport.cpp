#include "Viewport.h"

EEngineStatus CViewport::Initialize()
{
	m_Window = SDL_CreateWindow(kViewportWindowTitle, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, kViewportInitialWidth, kViewportInitialHeight, SDL_WINDOW_VULKAN | SDL_WINDOW_ALLOW_HIGHDPI);

	if (m_Window == nullptr)
	{
		return EEngineStatus::Failed;
	}

	return EEngineStatus::Ok;
}

EEngineStatus CViewport::Update()
{
	SDL_Event event;

	while (SDL_PollEvent(&event) != 0)
	{
		switch (event.type)
		{
		case SDL_QUIT:
		{
			gEngine->Quit();
			break;
		}
		default:
			break;
		}
	}

	return EEngineStatus::Ok;
}

EEngineStatus CViewport::Shutdown()
{

	return EEngineStatus::Ok;
}
