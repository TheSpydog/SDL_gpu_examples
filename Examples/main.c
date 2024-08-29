#include "Common.h"
#include <SDL3/SDL_main.h>

static Example* Examples[] =
{
	&ClearScreen_Example,
#if !(defined(SDL_PLATFORM_XBOXONE) || defined(SDL_PLATFORM_XBOXSERIES))
	&ClearScreenMultiWindow_Example,
#endif
	&BasicTriangle_Example,
	&BasicVertexBuffer_Example,
	&CullMode_Example,
	&BasicStencil_Example,
	&InstancedIndexed_Example,
	&TexturedQuad_Example,
	&TexturedAnimatedQuad_Example,
	&Clear3DSlice_Example,
	&BasicCompute_Example,
	&ComputeUniforms_Example,
	&ToneMapping_Example,
	&CustomSampling_Example,
	&DrawIndirect_Example,
	&ComputeSpriteBatch_Example,
	&CopyAndReadback_Example,
	&CopyConsistency_Example,
	&Texture2DArray_Example,
	&TriangleMSAA_Example,
	&Cubemap_Example,
	&WindowResize_Example,
	&Blit2DArray_Example,
	&BlitCube_Example,
	&BlitMirror_Example,
	&GenerateMipmaps_Example,
};

SDL_bool AppLifecycleWatcher(void *userdata, SDL_Event *event)
{
	/* This callback may be on a different thread, so let's
	 * push these events as USER events so they appear
	 * in the main thread's event loop.
	 *
	 * That allows us to cancel drawing before/after we finish
	 * drawing a frame, rather than mid-draw (which can crash!).
	 */
	if (event->type == SDL_EVENT_DID_ENTER_BACKGROUND)
	{
		SDL_Event evt;
		evt.type = SDL_EVENT_USER;
		evt.user.code = 0;
		SDL_PushEvent(&evt);
	}
	else if (event->type == SDL_EVENT_WILL_ENTER_FOREGROUND)
	{
		SDL_Event evt;
		evt.type = SDL_EVENT_USER;
		evt.user.code = 1;
		SDL_PushEvent(&evt);
	}
	return SDL_FALSE;
}

int main(int argc, char **argv)
{
	Context context = { 0 };
	int exampleIndex = -1;
	int gotoExampleIndex = 0;
	int quit = 0;
	float lastTime = 0;

	for (int i = 1; i < argc; i += 1)
	{
		if (SDL_strcmp(argv[i], "-name") == 0 && argc > i + 1)
		{
			const char* exampleName = argv[i + 1];
			int foundExample = 0;

			for (int j = 0; j < SDL_arraysize(Examples); j += 1)
			{
				if (SDL_strcmp(Examples[j]->Name, exampleName) == 0)
				{
					gotoExampleIndex = j;
					foundExample = 1;
					break;
				}
			}

			if (!foundExample)
			{
				SDL_Log("No example named '%s' exists.", exampleName);
				return 1;
			}
		}
	}

	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD) < 0)
	{
		SDL_Log("Failed to initialize SDL: %s", SDL_GetError());
		return 1;
	}

	InitializeAssetLoader();
	SDL_AddEventWatch(AppLifecycleWatcher, NULL);

	SDL_Log("Welcome to the SDL_Gpu example suite!");
	SDL_Log("Press A/D (or LB/RB) to move between examples!");

	SDL_Gamepad* gamepad = NULL;
	SDL_bool canDraw = SDL_TRUE;

	while (!quit)
	{
		context.LeftPressed = 0;
		context.RightPressed = 0;
		context.DownPressed = 0;
		context.UpPressed = 0;

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
			else if (evt.type == SDL_EVENT_GAMEPAD_ADDED)
			{
				if (gamepad == NULL)
				{
					gamepad = SDL_OpenGamepad(evt.gdevice.which);
				}
			}
			else if (evt.type == SDL_EVENT_GAMEPAD_REMOVED)
			{
				if (evt.gdevice.which == SDL_GetGamepadID(gamepad))
				{
					SDL_CloseGamepad(gamepad);
				}
			}
			else if (evt.type == SDL_EVENT_USER)
			{
				if (evt.user.code == 0)
				{
#ifdef SDL_PLATFORM_GDK
					SDL_GDKSuspendGpu(context.Device);
					canDraw = SDL_FALSE;
					SDL_GDKSuspendComplete();
#endif
				}
				else if (evt.user.code == 1)
				{
#ifdef SDL_PLATFORM_GDK
					SDL_GDKResumeGpu(context.Device);
					canDraw = SDL_TRUE;
#endif
				}
			}
			else if (evt.type == SDL_EVENT_KEY_DOWN)
			{
				if (evt.key.key == SDLK_D)
				{
					gotoExampleIndex = exampleIndex + 1;
					if (gotoExampleIndex >= SDL_arraysize(Examples)) {
						gotoExampleIndex = 0;
					}
				}
				else if (evt.key.key == SDLK_A)
				{
					gotoExampleIndex = exampleIndex - 1;
					if (gotoExampleIndex < 0) {
						gotoExampleIndex = SDL_arraysize(Examples) - 1;
					}
				}
				else if (evt.key.key == SDLK_LEFT)
				{
					context.LeftPressed = SDL_TRUE;
				}
				else if (evt.key.key == SDLK_RIGHT)
				{
					context.RightPressed = SDL_TRUE;
				}
				else if (evt.key.key == SDLK_DOWN)
				{
					context.DownPressed = SDL_TRUE;
				}
				else if (evt.key.key == SDLK_UP)
				{
					context.UpPressed = SDL_TRUE;
				}
			}
			else if (evt.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN)
			{
				if (evt.gbutton.button == SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER)
				{
					gotoExampleIndex = exampleIndex + 1;
					if (gotoExampleIndex >= SDL_arraysize(Examples)) {
						gotoExampleIndex = 0;
					}
				}
				else if (evt.gbutton.button == SDL_GAMEPAD_BUTTON_LEFT_SHOULDER)
				{
					gotoExampleIndex = exampleIndex - 1;
					if (gotoExampleIndex < 0) {
						gotoExampleIndex = SDL_arraysize(Examples) - 1;
					}
				}
				else if (evt.gbutton.button == SDL_GAMEPAD_BUTTON_DPAD_LEFT)
				{
					context.LeftPressed = SDL_TRUE;
				}
				else if (evt.gbutton.button == SDL_GAMEPAD_BUTTON_DPAD_RIGHT)
				{
					context.RightPressed = SDL_TRUE;
				}
				else if (evt.gbutton.button == SDL_GAMEPAD_BUTTON_DPAD_DOWN)
				{
					context.DownPressed = SDL_TRUE;
				}
				else if (evt.gbutton.button == SDL_GAMEPAD_BUTTON_DPAD_UP)
				{
					context.UpPressed = SDL_TRUE;
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
			context.ExampleName = Examples[exampleIndex]->Name;
			SDL_Log("STARTING EXAMPLE: %s", context.ExampleName);
			if (Examples[exampleIndex]->Init(&context) < 0)
			{
				SDL_Log("Init failed!");
				return 1;
			}

			gotoExampleIndex = -1;
		}

		float newTime = SDL_GetTicks() / 1000.0f;
		context.DeltaTime = newTime - lastTime;
		lastTime = newTime;

		if (Examples[exampleIndex]->Update(&context) < 0)
		{
			SDL_Log("Update failed!");
			return 1;
		}

		if (canDraw)
		{
			if (Examples[exampleIndex]->Draw(&context) < 0)
			{
				SDL_Log("Draw failed!");
				return 1;
			}
		}
	}

	return 0;
}
