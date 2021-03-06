#include "Viewport.h"

#include "Render.h"
#include "imgui_impl_sdl.h"

EEngineStatus CViewport::Initialize()
{
	m_Window = SDL_CreateWindow(kViewportWindowTitle, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, kViewportInitialWidth, kViewportInitialHeight, SDL_WINDOW_VULKAN | SDL_WINDOW_ALLOW_HIGHDPI);

	if (m_Window == nullptr)
	{
		return EEngineStatus::Failed;
	}

	ImGui_ImplSDL2_InitForVulkan(m_Window);

	return EEngineStatus::Ok;
}

EEngineStatus CViewport::Update()
{
	SDL_Event event;

	while (SDL_PollEvent(&event) != 0)
	{
		if (ImGui_ImplSDL2_ProcessEvent(&event))continue;
		switch (event.type)
		{
		case SDL_QUIT:
		{
			gEngine->Quit();
			break;
		}
		case SDL_MOUSEWHEEL:
		{
			gEngine->GetRender()->m_RotationSpeed += (static_cast<float>(event.wheel.y) * 1.f);
			SDL_Log("New speed: %f", gEngine->GetRender()->m_RotationSpeed);
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
	ImGui_ImplSDL2_Shutdown();
	return EEngineStatus::Ok;
}
