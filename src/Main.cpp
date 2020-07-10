#include <SDL.h>
#include <cstdlib>
#include <Engine.h>

void VkAtExit()
{
	SDL_Log("VkAtExit!");
}

int main(int argc, char** argv)
{
	if(SDL_Init(SDL_INIT_TIMER | SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) != 0)
	{
		return 1;
	}

	std::atexit(VkAtExit);

	return CEngine::Run(argc, argv);
}