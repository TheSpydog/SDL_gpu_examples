#ifndef SDL_GPU_EXAMPLES_H
#define SDL_GPU_EXAMPLES_H

#include <SDL3/SDL.h>

typedef struct Context
{
	SDL_Window* Window;
	SDL_GpuDevice* Device;
} Context;

typedef struct Example
{
	const char* Name;
	int (*Init)(Context* context);
	int (*Update)(Context* context);
	int (*Draw)(Context* context);
	void (*Quit)(Context* context);
} Example;

extern Example ClearScreen_Example;
extern Example ClearScreenMultiWindow_Example;

#endif