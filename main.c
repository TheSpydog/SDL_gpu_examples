#include "SDL_gpu_examples.h"

static Example* Examples[] =
{
	&ClearScreen_Example,
	&ClearScreenMultiWindow_Example,
};

int main(int argc, char **argv)
{
	Context context = { 0 };
	int exampleIndex = -1;
	int gotoExampleIndex = 0;
	int quit = 0;

	if (SDL_Init(SDL_INIT_VIDEO) < 0)
	{
		SDL_Log("Failed to initialize SDL: %s", SDL_GetError());
		return 1;
	}

	SDL_Log("Welcome to the SDL_Gpu example suite!");
	SDL_Log("Press LEFT and RIGHT to move between examples!");

	while (!quit)
	{
		SDL_Event evt;
		while (SDL_PollEvent(&evt))
		{
			if (evt.type == SDL_EVENT_QUIT)
			{
				if (exampleIndex != -1)
				{
					Examples[exampleIndex]->Quit(&context);
				}
				quit = 1;
			}
			else if (evt.type == SDL_EVENT_KEY_DOWN)
			{
				if (evt.key.keysym.sym == SDLK_RIGHT)
				{
					gotoExampleIndex = (exampleIndex + 1) % SDL_arraysize(Examples);
				}
				else if (evt.key.keysym.sym == SDLK_LEFT)
				{
					gotoExampleIndex = (exampleIndex - 1) % SDL_arraysize(Examples);
				}
			}
		}
		if (quit)
		{
			break;
		}

		if (gotoExampleIndex != -1)
		{
			if (exampleIndex != -1)
			{
				Examples[exampleIndex]->Quit(&context);
				SDL_zero(context);
			}

			exampleIndex = gotoExampleIndex;
			SDL_Log("STARTING EXAMPLE: %s", Examples[exampleIndex]->Name);
			if (Examples[exampleIndex]->Init(&context) < 0)
			{
				SDL_Log("Init failed!");
				return 1;
			}

			gotoExampleIndex = -1;
		}

		if (Examples[exampleIndex]->Update(&context) < 0)
		{
			SDL_Log("Update failed!");
			return 1;
		}

		if (Examples[exampleIndex]->Draw(&context) < 0)
		{
			SDL_Log("Draw failed!");
			return 1;
		}
	}

	return 0;
}